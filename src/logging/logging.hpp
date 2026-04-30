#pragma once

#include <cstdint>

#include "maplibre_native_c.h"

namespace mln::core {

auto set_log_callback(mln_log_callback callback, void* user_data) -> mln_status;
auto clear_log_callback() -> mln_status;
auto set_log_async_severity_mask(std::uint32_t mask) -> mln_status;

}  // namespace mln::core
