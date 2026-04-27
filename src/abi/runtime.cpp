#define MLN_BUILDING_ABI

#include <exception>

#include "core/runtime.hpp"

#include "core/diagnostics.hpp"
#include "maplibre_native_abi.h"

auto mln_runtime_options_default(void) -> mln_runtime_options {
  return mln_runtime_options{.size = sizeof(mln_runtime_options), .flags = 0};
}

auto mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status {
  try {
    return mln::core::create_runtime(options, out_runtime);
  } catch (const std::exception& exception) {
    mln::core::set_thread_error(exception);
    return MLN_STATUS_NATIVE_ERROR;
  } catch (...) {
    mln::core::set_thread_error("unknown native exception");
    return MLN_STATUS_NATIVE_ERROR;
  }
}

auto mln_runtime_destroy(mln_runtime* runtime) -> mln_status {
  try {
    return mln::core::destroy_runtime(runtime);
  } catch (const std::exception& exception) {
    mln::core::set_thread_error(exception);
    return MLN_STATUS_NATIVE_ERROR;
  } catch (...) {
    mln::core::set_thread_error("unknown native exception");
    return MLN_STATUS_NATIVE_ERROR;
  }
}
