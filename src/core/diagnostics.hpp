#pragma once

#include <exception>
#include <string>

namespace mln::core {

auto thread_last_error() -> std::string&;
auto set_thread_error(const char* message) -> void;
auto set_thread_error(const std::exception& exception) -> void;

}  // namespace mln::core
