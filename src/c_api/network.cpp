#define MLN_BUILDING_C

#include <cstdint>

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"
#include "resources/network_status.hpp"

auto mln_network_status_get(std::uint32_t* out_status) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::network_status_get(out_status);
  });
}

auto mln_network_status_set(std::uint32_t status) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::network_status_set(status);
  });
}
