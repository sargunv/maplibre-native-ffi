#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/run_loop.hpp>

#include "core/runtime.hpp"

#include "core/diagnostics.hpp"
#include "core/resource_scheme.hpp"
#include "maplibre_native_abi.h"

namespace {
using RuntimeRegistry =
  std::unordered_map<mln_runtime*, std::unique_ptr<mln_runtime>>;

auto runtime_registry_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto runtime_registry() -> RuntimeRegistry& {
  static RuntimeRegistry value;
  return value;
}

auto validate_runtime_options(const mln_runtime_options* options)
  -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }

  if (options->size < sizeof(mln_runtime_options)) {
    mln::core::set_thread_error("mln_runtime_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (options->flags != 0) {
    mln::core::set_thread_error(
      "mln_runtime_options.flags contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

}  // namespace

namespace mln::core {

auto validate_runtime(mln_runtime* runtime) -> mln_status {
  if (runtime == nullptr) {
    set_thread_error("runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (!runtime_registry().contains(runtime)) {
    set_thread_error("runtime is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (runtime->owner_thread != std::this_thread::get_id()) {
    set_thread_error("runtime call must be made on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  return MLN_STATUS_OK;
}

auto create_runtime(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status {
  const auto options_status = validate_runtime_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  if (out_runtime == nullptr) {
    set_thread_error("out_runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (*out_runtime != nullptr) {
    set_thread_error("out_runtime must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto owner_thread = std::this_thread::get_id();
  {
    const std::scoped_lock lock(runtime_registry_mutex());
    if (
      std::any_of(
        runtime_registry().begin(), runtime_registry().end(),
        [&](const auto& entry) -> bool {
          return entry.second->owner_thread == owner_thread;
        }
      )
    ) {
      set_thread_error("owner thread already has a live runtime");
      return MLN_STATUS_INVALID_STATE;
    }
  }
  if (mbgl::Scheduler::GetCurrent(false) != nullptr) {
    set_thread_error("owner thread already has an active MapLibre scheduler");
    return MLN_STATUS_INVALID_STATE;
  }

  auto owned_runtime = std::make_unique<mln_runtime>(mln_runtime{
    .owner_thread = owner_thread,
    .run_loop =
      std::make_unique<mbgl::util::RunLoop>(mbgl::util::RunLoop::Type::New),
    .asset_path = options == nullptr || options->asset_path == nullptr
                    ? std::string{}
                    : std::string{options->asset_path},
    .cache_path = options == nullptr || options->cache_path == nullptr
                    ? std::string{}
                    : std::string{options->cache_path}
  });
  auto* runtime = owned_runtime.get();
  {
    const std::scoped_lock lock(runtime_registry_mutex());
    runtime_registry().emplace(runtime, std::move(owned_runtime));
  }

  *out_runtime = runtime;
  return MLN_STATUS_OK;
}

auto register_resource_provider(
  mln_runtime* runtime, const mln_resource_provider* provider
) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (provider == nullptr) {
    set_thread_error("provider must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (provider->size < sizeof(mln_resource_provider)) {
    set_thread_error("mln_resource_provider.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (provider->callback == nullptr) {
    set_thread_error("provider callback must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto scheme = normalize_scheme(provider->scheme);
  if (!is_valid_scheme(scheme)) {
    set_thread_error("provider scheme is invalid");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (is_reserved_scheme(scheme)) {
    set_thread_error("provider scheme is reserved by the ABI");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime->live_maps != 0) {
    set_thread_error(
      "resource providers must be registered before map creation"
    );
    return MLN_STATUS_INVALID_STATE;
  }
  const auto existing = std::ranges::find_if(
    runtime->resource_providers,
    [&](const auto& entry) -> bool { return entry.scheme == scheme; }
  );
  if (existing != runtime->resource_providers.end()) {
    set_thread_error("resource provider scheme is already registered");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  runtime->resource_providers.push_back(
    ResourceProvider{
      .scheme = scheme,
      .callback = provider->callback,
      .user_data = provider->user_data,
    }
  );
  return MLN_STATUS_OK;
}

auto destroy_runtime(mln_runtime* runtime) -> mln_status {
  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime == nullptr) {
    set_thread_error("runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto found = runtime_registry().find(runtime);
  if (found == runtime_registry().end()) {
    set_thread_error("runtime is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (found->second->owner_thread != std::this_thread::get_id()) {
    set_thread_error("runtime must be destroyed on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  if (found->second->live_maps != 0) {
    set_thread_error("runtime still owns live maps");
    return MLN_STATUS_INVALID_STATE;
  }

  runtime_registry().erase(runtime);
  return MLN_STATUS_OK;
}

auto run_runtime_once(mln_runtime* runtime) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  runtime->run_loop->runOnce();
  return MLN_STATUS_OK;
}

auto retain_runtime_map(mln_runtime* runtime) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  ++runtime->live_maps;
  return MLN_STATUS_OK;
}

auto release_runtime_map(mln_runtime* runtime) noexcept -> void {
  if (runtime == nullptr) {
    return;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime_registry().contains(runtime) && runtime->live_maps != 0) {
    --runtime->live_maps;
  }
}

auto resource_options_for_runtime(mln_runtime* runtime)
  -> mbgl::ResourceOptions {
  auto options = mbgl::ResourceOptions::Default();
  if (runtime != nullptr) {
    options.withPlatformContext(runtime);
    if (!runtime->asset_path.empty()) {
      options.withAssetPath(runtime->asset_path);
    }
    if (!runtime->cache_path.empty()) {
      options.withCachePath(runtime->cache_path);
    }
  }
  return options;
}

auto find_runtime_for_platform_context(void* platform_context) noexcept
  -> mln_runtime* {
  if (platform_context == nullptr) {
    return nullptr;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  auto* runtime = static_cast<mln_runtime*>(platform_context);
  return runtime_registry().contains(runtime) ? runtime : nullptr;
}

}  // namespace mln::core
