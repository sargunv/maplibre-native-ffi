#pragma once

#include <cstdint>

#include "maplibre_native_abi.h"

namespace mln::core {

auto network_status_get(std::uint32_t* out_status) -> mln_status;
auto network_status_set(std::uint32_t status) -> mln_status;

}  // namespace mln::core
