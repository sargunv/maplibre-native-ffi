#define MLN_BUILDING_ABI

#include <cstdint>

#include "core/runtime.hpp"

#include "abi/boundary.hpp"
#include "core/custom_resource_provider.hpp"
#include "maplibre_native_abi.h"

auto mln_runtime_options_default(void) noexcept -> mln_runtime_options {
  return mln_runtime_options{
    .size = sizeof(mln_runtime_options),
    .flags = 0,
    .asset_path = nullptr,
    .cache_path = nullptr,
    .maximum_cache_size = 0,
  };
}

auto mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::create_runtime(options, out_runtime);
  });
}

auto mln_runtime_set_resource_provider(
  mln_runtime* runtime, const mln_resource_provider* provider
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::set_resource_provider(runtime, provider);
  });
}

auto mln_resource_request_complete(
  mln_resource_request_handle* handle, const mln_resource_response* response
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::complete_resource_request(handle, response);
  });
}

auto mln_resource_request_cancelled(
  const mln_resource_request_handle* handle, bool* out_cancelled
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::resource_request_cancelled(handle, out_cancelled);
  });
}

auto mln_resource_request_release(mln_resource_request_handle* handle) noexcept
  -> void {
  mln::core::release_resource_request(handle);
}

auto mln_runtime_set_resource_transform(
  mln_runtime* runtime, const mln_resource_transform* transform
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::set_resource_transform(runtime, transform);
  });
}

auto mln_runtime_run_ambient_cache_operation(
  mln_runtime* runtime, uint32_t operation
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::run_ambient_cache_operation(runtime, operation);
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
