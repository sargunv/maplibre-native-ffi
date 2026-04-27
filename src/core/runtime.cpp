#include "core/runtime.hpp"

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "core/diagnostics.hpp"

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

auto validate_runtime_options(const mln_runtime_options* options) -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }

  if (options->size < sizeof(mln_runtime_options)) {
    mln::core::set_thread_error("mln_runtime_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}
}  // namespace

namespace mln::core {

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

  auto owned_runtime =
    std::make_unique<mln_runtime>(mln_runtime{std::this_thread::get_id()});
  auto* runtime = owned_runtime.get();
  {
    const std::scoped_lock lock(runtime_registry_mutex());
    runtime_registry().emplace(runtime, std::move(owned_runtime));
  }

  *out_runtime = runtime;
  return MLN_STATUS_OK;
}

auto destroy_runtime(mln_runtime* runtime) -> mln_status {
  if (runtime == nullptr) {
    set_thread_error("runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  const auto found = runtime_registry().find(runtime);
  if (found == runtime_registry().end()) {
    set_thread_error("runtime is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (found->second->owner_thread != std::this_thread::get_id()) {
    set_thread_error("runtime must be destroyed on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  runtime_registry().erase(runtime);
  return MLN_STATUS_OK;
}

}  // namespace mln::core
