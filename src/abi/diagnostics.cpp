#define MLN_BUILDING_ABI

#include "core/diagnostics.hpp"

#include "maplibre_native_abi.h"

auto mln_thread_last_error_message(void) -> const char* {
  return mln::core::thread_last_error().c_str();
}
