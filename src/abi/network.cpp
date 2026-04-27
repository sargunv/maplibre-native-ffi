#define MLN_BUILDING_ABI

#include <cstdint>

#include "abi/boundary.hpp"
#include "core/network_status.hpp"
#include "maplibre_native_abi.h"

auto mln_network_status_get(std::uint32_t* out_status) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::network_status_get(out_status);
  });
}

auto mln_network_status_set(std::uint32_t status) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::network_status_set(status);
  });
}
