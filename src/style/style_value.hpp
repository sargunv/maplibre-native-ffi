#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <mbgl/style/conversion.hpp>
#include <mbgl/style/conversion_impl.hpp>
#include <mbgl/style/filter.hpp>
#include <mbgl/util/feature.hpp>
#include <mbgl/util/geojson.hpp>

#include "maplibre_native_c/map.h"

namespace mbgl::style::conversion {

template <>
class ConversionTraits<const mln_json_value*> {
 public:
  static auto isUndefined(const mln_json_value* value) -> bool;
  static auto isArray(const mln_json_value* value) -> bool;
  static auto arrayLength(const mln_json_value* value) -> std::size_t;
  static auto arrayMember(const mln_json_value* value, std::size_t index)
    -> const mln_json_value*;
  static auto isObject(const mln_json_value* value) -> bool;
  static auto objectMember(const mln_json_value* value, const char* name)
    -> std::optional<const mln_json_value*>;

  template <class Callback>
  static auto eachMember(const mln_json_value* value, Callback callback)
    -> std::optional<Error> {
    const auto object = value->data.object_value;
    auto members =
      std::span<const mln_json_member>{object.members, object.member_count};
    for (const auto& member : members) {
      const auto* child = member.value;
      auto result = callback(string_from_view(member.key), std::move(child));
      if (result) {
        return result;
      }
    }
    return std::nullopt;
  }

  static auto toBool(const mln_json_value* value) -> std::optional<bool>;
  static auto toNumber(const mln_json_value* value) -> std::optional<float>;
  static auto toDouble(const mln_json_value* value) -> std::optional<double>;
  static auto toString(const mln_json_value* value)
    -> std::optional<std::string>;
  static auto toValue(const mln_json_value* value)
    -> std::optional<mbgl::Value>;
  static auto toGeoJSON(const mln_json_value* value, Error& error)
    -> std::optional<mbgl::GeoJSON>;

 private:
  static auto string_from_view(mln_string_view string) -> std::string;
};

}  // namespace mbgl::style::conversion

namespace mln::core {

auto validate_style_json_value(const mln_json_value* value) -> bool;

auto to_native_style_filter(const mln_json_value* filter)
  -> std::optional<mbgl::style::Filter>;

auto set_style_conversion_error(
  const char* context, const mbgl::style::conversion::Error& error
) -> void;

}  // namespace mln::core
