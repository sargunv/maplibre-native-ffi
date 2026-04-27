#define MLN_BUILDING_ABI
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "maplibre_native_abi.h"

struct mln_runtime {
  std::thread::id owner_thread;
};

namespace {
using RuntimeRegistry =
  std::unordered_map<mln_runtime*, std::unique_ptr<mln_runtime>>;

auto thread_last_error() -> std::string& {
  thread_local std::string value;
  return value;
}

auto runtime_registry_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto runtime_registry() -> RuntimeRegistry& {
  static RuntimeRegistry value;
  return value;
}

auto set_thread_error(const char* message) -> void {
  thread_last_error() = message;
}

auto set_thread_error(const std::exception& exception) -> void {
  thread_last_error() = exception.what();
}

auto validate_runtime_options(const mln_runtime_options* options)
  -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }

  if (options->size < sizeof(mln_runtime_options)) {
    set_thread_error("mln_runtime_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}
}  // namespace

auto mln_abi_version(void) -> std::uint32_t { return 0; }

auto mln_runtime_options_default(void) -> mln_runtime_options {
  return mln_runtime_options{.size = sizeof(mln_runtime_options), .flags = 0};
}

auto mln_thread_last_error_message(void) -> const char* {
  return thread_last_error().c_str();
}

auto mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status {
  try {
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
  } catch (const std::exception& exception) {
    set_thread_error(exception);
    return MLN_STATUS_NATIVE_ERROR;
  } catch (...) {
    set_thread_error("unknown native exception");
    return MLN_STATUS_NATIVE_ERROR;
  }
}

auto mln_runtime_destroy(mln_runtime* runtime) -> mln_status {
  try {
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
  } catch (const std::exception& exception) {
    set_thread_error(exception);
    return MLN_STATUS_NATIVE_ERROR;
  } catch (...) {
    set_thread_error("unknown native exception");
    return MLN_STATUS_NATIVE_ERROR;
  }
}
