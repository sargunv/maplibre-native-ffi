#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/storage/database_file_source.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_manager.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/client_options.hpp>
#include <mbgl/util/run_loop.hpp>

#include "runtime/runtime.hpp"

#include "diagnostics/diagnostics.hpp"
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
  constexpr auto known_flags =
    static_cast<uint32_t>(MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE);
  if ((options->flags & ~known_flags) != 0) {
    mln::core::set_thread_error(
      "mln_runtime_options.flags contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

}  // namespace

namespace mln::core {

namespace {

auto database_source_for_runtime(mln_runtime* runtime)
  -> std::shared_ptr<mbgl::DatabaseFileSource> {
  if (runtime == nullptr) {
    return nullptr;
  }

  auto source = mbgl::FileSourceManager::get()->getFileSource(
    mbgl::FileSourceType::Database, resource_options_for_runtime(runtime),
    mbgl::ClientOptions()
  );
  // The Database FileSourceManager factory is registered by this wrapper and
  // always returns DatabaseFileSource for FileSourceType::Database. MapLibre is
  // built without RTTI, so keep this path non-RTTI as well.
  auto database = std::static_pointer_cast<mbgl::DatabaseFileSource>(source);
  if (database != nullptr && runtime->has_maximum_cache_size) {
    database->setMaximumAmbientCacheSize(
      runtime->maximum_cache_size, [](std::exception_ptr) -> void {}
    );
  }
  return database;
}

auto wait_for_database_operation(
  const std::function<void(std::function<void(std::exception_ptr)>)>& start
) -> mln_status {
  auto mutex = std::mutex{};
  auto condition = std::condition_variable{};
  auto complete = false;
  auto failure = std::exception_ptr{};

  start([&](std::exception_ptr exception) -> void {
    {
      const std::scoped_lock lock(mutex);
      failure = exception;
      complete = true;
    }
    condition.notify_one();
  });

  auto lock = std::unique_lock{mutex};
  condition.wait(lock, [&]() -> bool { return complete; });
  if (failure) {
    try {
      std::rethrow_exception(failure);
    } catch (const std::exception& exception) {
      set_thread_error(exception.what());
    } catch (...) {
      set_thread_error("ambient cache operation failed");
    }
    return MLN_STATUS_NATIVE_ERROR;
  }
  return MLN_STATUS_OK;
}

}  // namespace

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
    if (std::any_of(
          runtime_registry().begin(), runtime_registry().end(),
          [&](const auto& entry) -> bool {
            return entry.second->owner_thread == owner_thread;
          }
        )) {
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
                    : std::string{options->cache_path},
    .has_maximum_cache_size =
      options != nullptr &&
      (options->flags & MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE) != 0,
    .maximum_cache_size = options == nullptr ? 0 : options->maximum_cache_size,
  });
  auto* runtime = owned_runtime.get();
  {
    const std::scoped_lock lock(runtime_registry_mutex());
    runtime_registry().emplace(runtime, std::move(owned_runtime));
  }

  *out_runtime = runtime;
  return MLN_STATUS_OK;
}

auto set_resource_transform(
  mln_runtime* runtime, const mln_resource_transform* transform
) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (transform == nullptr) {
    set_thread_error("resource transform must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (transform->size < sizeof(mln_resource_transform)) {
    set_thread_error("mln_resource_transform.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (transform->callback == nullptr) {
    set_thread_error("resource transform callback must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime->live_maps != 0) {
    set_thread_error("resource transform must be registered before map creation"
    );
    return MLN_STATUS_INVALID_STATE;
  }
  runtime->resource_transform_callback = transform->callback;
  runtime->resource_transform_user_data = transform->user_data;
  return MLN_STATUS_OK;
}

auto run_ambient_cache_operation(mln_runtime* runtime, uint32_t operation)
  -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  switch (operation) {
    case MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE:
    case MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE:
    case MLN_AMBIENT_CACHE_OPERATION_INVALIDATE:
    case MLN_AMBIENT_CACHE_OPERATION_CLEAR:
      break;
    default:
      set_thread_error("ambient cache operation is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  switch (operation) {
    case MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE:
      return wait_for_database_operation([&](auto callback) -> void {
        database->resetDatabase(std::move(callback));
      });
    case MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE:
      return wait_for_database_operation([&](auto callback) -> void {
        database->packDatabase(std::move(callback));
      });
    case MLN_AMBIENT_CACHE_OPERATION_INVALIDATE:
      return wait_for_database_operation([&](auto callback) -> void {
        database->invalidateAmbientCache(std::move(callback));
      });
    case MLN_AMBIENT_CACHE_OPERATION_CLEAR:
      return wait_for_database_operation([&](auto callback) -> void {
        database->clearAmbientCache(std::move(callback));
      });
    default:
      return MLN_STATUS_INVALID_ARGUMENT;
  }
}

auto set_resource_provider(
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

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime->live_maps != 0) {
    set_thread_error("resource provider must be set before map creation");
    return MLN_STATUS_INVALID_STATE;
  }
  runtime->has_resource_provider = true;
  runtime->resource_provider = ResourceProvider{
    .callback = provider->callback,
    .user_data = provider->user_data,
  };
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
    if (runtime->has_maximum_cache_size) {
      options.withMaximumCacheSize(runtime->maximum_cache_size);
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
