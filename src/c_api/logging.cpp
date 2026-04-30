#define MLN_BUILDING_C

#include <cstdint>

#include "logging/logging.hpp"

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"

auto mln_log_set_callback(mln_log_callback callback, void* user_data) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::set_log_callback(callback, user_data);
  });
}

auto mln_log_clear_callback(void) noexcept -> mln_status {
  return mln::c_api::status_boundary([]() -> mln_status {
    return mln::core::clear_log_callback();
  });
}

auto mln_log_set_async_severity_mask(std::uint32_t mask) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::set_log_async_severity_mask(mask);
  });
}
