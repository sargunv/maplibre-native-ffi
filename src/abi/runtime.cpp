#define MLN_BUILDING_ABI

#include "core/runtime.hpp"

#include "abi/boundary.hpp"
#include "maplibre_native_abi.h"

auto mln_runtime_options_default(void) noexcept -> mln_runtime_options {
  return mln_runtime_options{.size = sizeof(mln_runtime_options), .flags = 0};
}

auto mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::create_runtime(options, out_runtime);
  });
}

auto mln_runtime_destroy(mln_runtime* runtime) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::destroy_runtime(runtime);
  });
}

auto mln_runtime_run_once(mln_runtime* runtime) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::run_runtime_once(runtime);
  });
}
