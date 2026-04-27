#define MLN_BUILDING_ABI

#include "diagnostics/diagnostics.hpp"

#include "maplibre_native_abi.h"

auto mln_thread_last_error_message(void) noexcept -> const char* {
  return mln::core::thread_last_error_message();
}
