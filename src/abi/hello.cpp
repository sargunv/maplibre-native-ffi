#define MLN_BUILDING_ABI
#include <cstdint>

#include "maplibre_native_abi.h"

auto mln_abi_version(void) -> std::uint32_t { return 0; }

auto mln_hello_world(const char** out_message) -> mln_status {
  if (out_message == nullptr) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_message = "hello from maplibre_native_abi";
  return MLN_STATUS_OK;
}
