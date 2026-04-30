#include <cstdint>

#include <mbgl/storage/network_status.hpp>

#include "resources/network_status.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"

namespace mln::core {

auto network_status_get(std::uint32_t* out_status) -> mln_status {
  if (out_status == nullptr) {
    set_thread_error("out_status must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_status =
    mbgl::NetworkStatus::Get() == mbgl::NetworkStatus::Status::Online
      ? MLN_NETWORK_STATUS_ONLINE
      : MLN_NETWORK_STATUS_OFFLINE;
  return MLN_STATUS_OK;
}

auto network_status_set(std::uint32_t status) -> mln_status {
  switch (status) {
    case MLN_NETWORK_STATUS_ONLINE:
      mbgl::NetworkStatus::Set(mbgl::NetworkStatus::Status::Online);
      return MLN_STATUS_OK;
    case MLN_NETWORK_STATUS_OFFLINE:
      mbgl::NetworkStatus::Set(mbgl::NetworkStatus::Status::Offline);
      return MLN_STATUS_OK;
    default:
      set_thread_error("network status is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }
}

}  // namespace mln::core
