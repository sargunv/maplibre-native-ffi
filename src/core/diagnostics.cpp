#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>

#include "core/diagnostics.hpp"

namespace {
constexpr auto diagnostic_capacity = std::size_t{4096};

auto thread_last_error_buffer() noexcept
  -> std::array<char, diagnostic_capacity>& {
  thread_local std::array<char, diagnostic_capacity> value{};
  return value;
}
}  // namespace

namespace mln::core {

auto thread_last_error_message() noexcept -> const char* {
  return thread_last_error_buffer().data();
}

auto clear_thread_error() noexcept -> void {
  thread_last_error_buffer().front() = '\0';
}

auto set_thread_error(const char* message) noexcept -> void {
  auto& buffer = thread_last_error_buffer();
  if (message == nullptr) {
    buffer.front() = '\0';
    return;
  }

  const auto length =
    std::min(std::char_traits<char>::length(message), buffer.size() - 1);
  auto* const output =
    std::ranges::copy(std::string_view{message, length}, buffer.begin()).out;
  *output = '\0';
}

auto set_thread_error(const std::exception& exception) noexcept -> void {
  set_thread_error(exception.what());
}

}  // namespace mln::core
