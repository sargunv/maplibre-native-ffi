#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

#include "core/resource_scheme.hpp"

namespace mln::core {

auto scheme_for_url(const std::string& url) -> std::optional<std::string> {
  const auto separator = url.find("://");
  if (separator == std::string::npos || separator == 0) {
    return std::nullopt;
  }
  auto scheme = url.substr(0, separator);
  for (auto& character : scheme) {
    character =
      static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return scheme;
}

auto normalize_scheme(const char* scheme) -> std::string {
  if (scheme == nullptr || *scheme == '\0') {
    return {};
  }

  auto normalized = std::string{scheme};
  for (auto& character : normalized) {
    character =
      static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return normalized;
}

auto is_valid_scheme(const std::string& scheme) -> bool {
  if (
    scheme.empty() ||
    std::isalpha(static_cast<unsigned char>(scheme.front())) == 0
  ) {
    return false;
  }
  return std::all_of(
    scheme.begin() + 1, scheme.end(), [](const char character) -> bool {
      const auto value = static_cast<unsigned char>(character);
      return std::isalnum(value) || character == '+' || character == '-' ||
             character == '.';
    }
  );
}

auto is_reserved_scheme(const std::string& scheme) -> bool {
  return scheme == "file" || scheme == "asset" || scheme == "http" ||
         scheme == "https" || scheme == "mbtiles" || scheme == "pmtiles";
}

}  // namespace mln::core
