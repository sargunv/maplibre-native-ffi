#define MLN_BUILDING_ABI
#include "maplibre_native_abi.h"

uint32_t mln_abi_version(void) { return 1; }

mln_status mln_hello_world(const char **out_message) {
  if (out_message == nullptr) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_message = "hello from maplibre_native_abi";
  return MLN_STATUS_OK;
}
