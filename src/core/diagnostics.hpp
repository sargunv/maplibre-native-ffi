#pragma once

#include <exception>

namespace mln::core {

auto thread_last_error_message() noexcept -> const char*;
auto set_thread_error(const char* message) noexcept -> void;
auto set_thread_error(const std::exception& exception) noexcept -> void;

}  // namespace mln::core
