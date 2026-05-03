#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <mbgl/style/conversion.hpp>
#include <mbgl/style/conversion/filter.hpp>
#include <mbgl/style/conversion/geojson.hpp>
#include <mbgl/style/conversion_impl.hpp>
#include <mbgl/style/filter.hpp>
#include <mbgl/util/feature.hpp>
#include <mbgl/util/geojson.hpp>
#include <mbgl/util/rapidjson.hpp>

#include <mapbox/geojson/rapidjson.hpp>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include "style/style_value.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c/map.h"

namespace {

constexpr std::size_t max_recursive_depth = 64;

auto validate_depth(std::size_t depth) -> bool {
  if (depth > max_recursive_depth) {
    mln::core::set_thread_error("style JSON value nesting is too deep");
    return false;
  }
  return true;
}

auto validate_string(mln_string_view string) -> bool {
  if (string.size > 0 && string.data == nullptr) {
    mln::core::set_thread_error("string data must not be null");
    return false;
  }
  return true;
}

auto string_from_view(mln_string_view string) -> std::string {
  if (string.size == 0) {
    return {};
  }
  return std::string{string.data, string.size};
}

auto string_view_equals(mln_string_view string, const char* value) -> bool {
  return std::string_view{
           string.data == nullptr ? "" : string.data, string.size
         } == std::string_view{value};
}

auto validate_value(const mln_json_value* value, std::size_t depth) -> bool;

auto validate_array(mln_json_array array, std::size_t depth) -> bool {
  if (array.value_count > 0 && array.values == nullptr) {
    mln::core::set_thread_error("array values must not be null");
    return false;
  }
  for (const auto& child :
       std::span<const mln_json_value>{array.values, array.value_count}) {
    if (!validate_value(&child, depth + 1)) {
      return false;
    }
  }
  return true;
}

auto validate_object(mln_json_object object, std::size_t depth) -> bool {
  if (object.member_count > 0 && object.members == nullptr) {
    mln::core::set_thread_error("object members must not be null");
    return false;
  }
  auto members =
    std::span<const mln_json_member>{object.members, object.member_count};
  return std::ranges::all_of(members, [depth](const auto& member) -> bool {
    return validate_string(member.key) &&
           validate_value(member.value, depth + 1);
  });
}

auto validate_value(const mln_json_value* value, std::size_t depth) -> bool {
  if (!validate_depth(depth)) {
    return false;
  }
  if (value == nullptr) {
    mln::core::set_thread_error("value must not be null");
    return false;
  }
  if (value->size < sizeof(mln_json_value)) {
    mln::core::set_thread_error("mln_json_value.size is too small");
    return false;
  }

  switch (value->type) {
    case MLN_JSON_VALUE_TYPE_NULL:
    case MLN_JSON_VALUE_TYPE_BOOL:
    case MLN_JSON_VALUE_TYPE_UINT:
    case MLN_JSON_VALUE_TYPE_INT:
      return true;
    case MLN_JSON_VALUE_TYPE_DOUBLE:
      if (!std::isfinite(value->data.double_value)) {
        mln::core::set_thread_error("double value must be finite");
        return false;
      }
      return true;
    case MLN_JSON_VALUE_TYPE_STRING:
      return validate_string(value->data.string_value);
    case MLN_JSON_VALUE_TYPE_ARRAY:
      return validate_array(value->data.array_value, depth);
    case MLN_JSON_VALUE_TYPE_OBJECT:
      return validate_object(value->data.object_value, depth);
    default:
      mln::core::set_thread_error("JSON value type is invalid");
      return false;
  }
}

auto make_rapidjson_string(
  mln_string_view string, mbgl::JSDocument::AllocatorType& allocator,
  mbgl::style::conversion::Error& error
) -> std::optional<mbgl::JSValue> {
  if (string.size > std::numeric_limits<rapidjson::SizeType>::max()) {
    error = {"string is too large for GeoJSON conversion"};
    return std::nullopt;
  }

  auto result = mbgl::JSValue{};
  result.SetString(
    string.data == nullptr ? "" : string.data,
    static_cast<rapidjson::SizeType>(string.size), allocator
  );
  return result;
}

auto to_rapidjson_value(
  const mln_json_value* value, mbgl::JSDocument::AllocatorType& allocator,
  mbgl::style::conversion::Error& error
) -> std::optional<mbgl::JSValue>;

auto to_rapidjson_array(
  mln_json_array array, mbgl::JSDocument::AllocatorType& allocator,
  mbgl::style::conversion::Error& error
) -> std::optional<mbgl::JSValue> {
  if (array.value_count > std::numeric_limits<rapidjson::SizeType>::max()) {
    error = {"array is too large for GeoJSON conversion"};
    return std::nullopt;
  }

  auto result = mbgl::JSValue{};
  result.SetArray();
  result.Reserve(
    static_cast<rapidjson::SizeType>(array.value_count), allocator
  );
  for (const auto& child :
       std::span<const mln_json_value>{array.values, array.value_count}) {
    auto converted = to_rapidjson_value(&child, allocator, error);
    if (!converted) {
      return std::nullopt;
    }
    result.PushBack(converted->Move(), allocator);
  }
  return result;
}

auto to_rapidjson_object(
  mln_json_object object, mbgl::JSDocument::AllocatorType& allocator,
  mbgl::style::conversion::Error& error
) -> std::optional<mbgl::JSValue> {
  if (object.member_count > std::numeric_limits<rapidjson::SizeType>::max()) {
    error = {"object is too large for GeoJSON conversion"};
    return std::nullopt;
  }

  auto result = mbgl::JSValue{};
  result.SetObject();
  result.MemberReserve(
    static_cast<rapidjson::SizeType>(object.member_count), allocator
  );
  for (const auto& member :
       std::span<const mln_json_member>{object.members, object.member_count}) {
    auto key = make_rapidjson_string(member.key, allocator, error);
    auto converted = to_rapidjson_value(member.value, allocator, error);
    if (!key || !converted) {
      return std::nullopt;
    }
    result.AddMember(key->Move(), converted->Move(), allocator);
  }
  return result;
}

auto to_rapidjson_value(
  const mln_json_value* value, mbgl::JSDocument::AllocatorType& allocator,
  mbgl::style::conversion::Error& error
) -> std::optional<mbgl::JSValue> {
  auto result = mbgl::JSValue{};
  switch (value->type) {
    case MLN_JSON_VALUE_TYPE_NULL:
      result.SetNull();
      return result;
    case MLN_JSON_VALUE_TYPE_BOOL:
      result.SetBool(value->data.bool_value);
      return result;
    case MLN_JSON_VALUE_TYPE_UINT:
      result.SetUint64(value->data.uint_value);
      return result;
    case MLN_JSON_VALUE_TYPE_INT:
      result.SetInt64(value->data.int_value);
      return result;
    case MLN_JSON_VALUE_TYPE_DOUBLE:
      result.SetDouble(value->data.double_value);
      return result;
    case MLN_JSON_VALUE_TYPE_STRING:
      return make_rapidjson_string(value->data.string_value, allocator, error);
    case MLN_JSON_VALUE_TYPE_ARRAY:
      return to_rapidjson_array(value->data.array_value, allocator, error);
    case MLN_JSON_VALUE_TYPE_OBJECT:
      return to_rapidjson_object(value->data.object_value, allocator, error);
    default:
      error = {"JSON value type is invalid"};
      return std::nullopt;
  }
}

}  // namespace

namespace mbgl::style::conversion {

auto ConversionTraits<const mln_json_value*>::isUndefined(
  const mln_json_value* value
) -> bool {
  return value->type == MLN_JSON_VALUE_TYPE_NULL;
}

auto ConversionTraits<const mln_json_value*>::isArray(
  const mln_json_value* value
) -> bool {
  return value->type == MLN_JSON_VALUE_TYPE_ARRAY;
}

auto ConversionTraits<const mln_json_value*>::arrayLength(
  const mln_json_value* value
) -> std::size_t {
  return value->data.array_value.value_count;
}

auto ConversionTraits<const mln_json_value*>::arrayMember(
  const mln_json_value* value, std::size_t index
) -> const mln_json_value* {
  auto values = std::span<const mln_json_value>{
    value->data.array_value.values, value->data.array_value.value_count
  };
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  return &values[index];
}

auto ConversionTraits<const mln_json_value*>::isObject(
  const mln_json_value* value
) -> bool {
  return value->type == MLN_JSON_VALUE_TYPE_OBJECT;
}

auto ConversionTraits<const mln_json_value*>::objectMember(
  const mln_json_value* value, const char* name
) -> std::optional<const mln_json_value*> {
  const auto object = value->data.object_value;
  for (const auto& member :
       std::span<const mln_json_member>{object.members, object.member_count}) {
    if (string_view_equals(member.key, name)) {
      return member.value;
    }
  }
  return std::nullopt;
}

auto ConversionTraits<const mln_json_value*>::toBool(
  const mln_json_value* value
) -> std::optional<bool> {
  if (value->type != MLN_JSON_VALUE_TYPE_BOOL) {
    return std::nullopt;
  }
  return value->data.bool_value;
}

auto ConversionTraits<const mln_json_value*>::toNumber(
  const mln_json_value* value
) -> std::optional<float> {
  switch (value->type) {
    case MLN_JSON_VALUE_TYPE_UINT:
      return static_cast<float>(value->data.uint_value);
    case MLN_JSON_VALUE_TYPE_INT:
      return static_cast<float>(value->data.int_value);
    case MLN_JSON_VALUE_TYPE_DOUBLE:
      return static_cast<float>(value->data.double_value);
    default:
      return std::nullopt;
  }
}

auto ConversionTraits<const mln_json_value*>::toDouble(
  const mln_json_value* value
) -> std::optional<double> {
  switch (value->type) {
    case MLN_JSON_VALUE_TYPE_UINT:
      return static_cast<double>(value->data.uint_value);
    case MLN_JSON_VALUE_TYPE_INT:
      return static_cast<double>(value->data.int_value);
    case MLN_JSON_VALUE_TYPE_DOUBLE:
      return value->data.double_value;
    default:
      return std::nullopt;
  }
}

auto ConversionTraits<const mln_json_value*>::toString(
  const mln_json_value* value
) -> std::optional<std::string> {
  if (value->type != MLN_JSON_VALUE_TYPE_STRING) {
    return std::nullopt;
  }
  return string_from_view(value->data.string_value);
}

auto ConversionTraits<const mln_json_value*>::toValue(
  const mln_json_value* value
) -> std::optional<mbgl::Value> {
  switch (value->type) {
    case MLN_JSON_VALUE_TYPE_BOOL:
      return mbgl::Value{value->data.bool_value};
    case MLN_JSON_VALUE_TYPE_UINT:
      return mbgl::Value{value->data.uint_value};
    case MLN_JSON_VALUE_TYPE_INT:
      return mbgl::Value{value->data.int_value};
    case MLN_JSON_VALUE_TYPE_DOUBLE:
      return mbgl::Value{value->data.double_value};
    case MLN_JSON_VALUE_TYPE_STRING:
      return mbgl::Value{string_from_view(value->data.string_value)};
    default:
      return std::nullopt;
  }
}

auto ConversionTraits<const mln_json_value*>::toGeoJSON(
  const mln_json_value* value, Error& error
) -> std::optional<mbgl::GeoJSON> {
  if (value->type == MLN_JSON_VALUE_TYPE_STRING) {
    return parseGeoJSON(string_from_view(value->data.string_value), error);
  }

  auto document = mbgl::JSDocument{};
  auto converted = to_rapidjson_value(value, document.GetAllocator(), error);
  if (!converted) {
    return std::nullopt;
  }
  document.Swap(*converted);
  try {
    return mapbox::geojson::convert(document);
  } catch (const std::exception& exception) {
    error = {exception.what()};
    return std::nullopt;
  }
}

auto ConversionTraits<const mln_json_value*>::string_from_view(
  mln_string_view string
) -> std::string {
  return ::string_from_view(string);
}

}  // namespace mbgl::style::conversion

namespace mln::core {

auto validate_style_json_value(const mln_json_value* value) -> bool {
  return validate_value(value, 0);
}

auto to_native_style_filter(const mln_json_value* filter)
  -> std::optional<mbgl::style::Filter> {
  if (!validate_style_json_value(filter)) {
    return std::nullopt;
  }

  auto error = mbgl::style::conversion::Error{};
  auto converted = mbgl::style::conversion::Converter<mbgl::style::Filter>{}(
    mbgl::style::conversion::Convertible{filter}, error
  );
  if (!converted) {
    set_style_conversion_error("style filter", error);
    return std::nullopt;
  }
  return converted;
}

auto set_style_conversion_error(
  const char* context, const mbgl::style::conversion::Error& error
) -> void {
  auto message = std::string{context} + " is invalid";
  if (!error.message.empty()) {
    message += ": ";
    message += error.message;
  }
  set_thread_error(message.c_str());
}

}  // namespace mln::core
