#define MLN_BUILDING_ABI

#include <cstdint>
#include <memory>
#include <string>

#include <mbgl/util/event.hpp>
#include <mbgl/util/logging.hpp>

#include "abi/boundary.hpp"
#include "core/diagnostics.hpp"
#include "maplibre_native_abi.h"

namespace {

class CallbackLogObserver final : public mbgl::Log::Observer {
 public:
  CallbackLogObserver(mln_log_callback callback, void* user_data)
      : callback_(callback), user_data_(user_data) {}

  auto onRecord(
    mbgl::EventSeverity severity, mbgl::Event event, std::int64_t code,
    const std::string& message
  ) -> bool override {
    if (callback_ == nullptr) {
      return false;
    }

    return callback_(
             user_data_, static_cast<std::uint32_t>(severity),
             static_cast<std::uint32_t>(event), code, message.c_str()
           ) != 0U;
  }

 private:
  mln_log_callback callback_ = nullptr;
  void* user_data_ = nullptr;
};

auto set_severity_async(
  std::uint32_t mask, mln_log_severity_mask bit, mbgl::EventSeverity severity
) -> void {
  mbgl::Log::useLogThread((mask & bit) != 0U, severity);
}

}  // namespace

auto mln_log_set_callback(mln_log_callback callback, void* user_data) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    if (callback == nullptr) {
      mbgl::Log::removeObserver();
      return MLN_STATUS_OK;
    }

    mbgl::Log::setObserver(
      std::make_unique<CallbackLogObserver>(callback, user_data)
    );
    return MLN_STATUS_OK;
  });
}

auto mln_log_clear_callback(void) noexcept -> mln_status {
  return mln::abi::status_boundary([]() -> mln_status {
    mbgl::Log::removeObserver();
    return MLN_STATUS_OK;
  });
}

auto mln_log_set_async_severity_mask(std::uint32_t mask) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    if ((mask & ~MLN_LOG_SEVERITY_MASK_ALL) != 0U) {
      mln::core::set_thread_error(
        "log async severity mask contains unknown bits"
      );
      return MLN_STATUS_INVALID_ARGUMENT;
    }

    set_severity_async(
      mask, MLN_LOG_SEVERITY_MASK_DEBUG, mbgl::EventSeverity::Debug
    );
    set_severity_async(
      mask, MLN_LOG_SEVERITY_MASK_INFO, mbgl::EventSeverity::Info
    );
    set_severity_async(
      mask, MLN_LOG_SEVERITY_MASK_WARNING, mbgl::EventSeverity::Warning
    );
    set_severity_async(
      mask, MLN_LOG_SEVERITY_MASK_ERROR, mbgl::EventSeverity::Error
    );
    return MLN_STATUS_OK;
  });
}
