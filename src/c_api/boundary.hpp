#pragma once

#include <exception>

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"

namespace mln::c_api {

template <typename Function>
auto status_boundary(Function function) noexcept -> mln_status {
  mln::core::clear_thread_error();
  try {
    return function();
  } catch (const std::exception& exception) {
    mln::core::set_thread_error(exception);
    return MLN_STATUS_NATIVE_ERROR;
  } catch (...) {
    mln::core::set_thread_error("unknown native exception");
    return MLN_STATUS_NATIVE_ERROR;
  }
}

}  // namespace mln::c_api
