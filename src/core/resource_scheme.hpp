#pragma once

#include <optional>
#include <string>

namespace mln::core {

auto scheme_for_url(const std::string& url) -> std::optional<std::string>;
auto normalize_scheme(const char* scheme) -> std::string;
auto is_valid_scheme(const std::string& scheme) -> bool;
auto is_reserved_scheme(const std::string& scheme) -> bool;

}  // namespace mln::core
