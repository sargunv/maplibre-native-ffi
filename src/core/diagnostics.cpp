#include <exception>
#include <string>

#include "core/diagnostics.hpp"

namespace mln::core {

auto thread_last_error() -> std::string& {
  thread_local std::string value;
  return value;
}

auto set_thread_error(const char* message) -> void {
  thread_last_error() = message;
}

auto set_thread_error(const std::exception& exception) -> void {
  thread_last_error() = exception.what();
}

}  // namespace mln::core
