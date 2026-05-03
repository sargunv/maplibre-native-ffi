#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <mbgl/util/feature.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/geojson.hpp>
#include <mbgl/util/geometry.hpp>

#include <mapbox/geometry/geometry.hpp>

#include "geojson/geojson.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"

// Public C ABI uses tagged unions.
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

namespace {

constexpr std::size_t max_recursive_depth = 64;

auto validate_depth(std::size_t depth) -> bool {
  if (depth > max_recursive_depth) {
    mln::core::set_thread_error("GeoJSON value nesting is too deep");
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

auto to_string(mln_string_view string) -> std::string {
  if (string.size == 0) {
    return {};
  }
  return std::string{string.data, string.size};
}

auto validate_coordinate(mln_lat_lng coordinate) -> bool {
  if (
    !std::isfinite(coordinate.latitude) || coordinate.latitude < -90.0 ||
    coordinate.latitude > 90.0 || !std::isfinite(coordinate.longitude)
  ) {
    mln::core::set_thread_error(
      "latitude must be finite and within [-90, 90], and longitude must be "
      "finite"
    );
    return false;
  }
  return true;
}

auto to_native_point(mln_lat_lng coordinate) -> mbgl::Point<double> {
  return mbgl::Point<double>{coordinate.longitude, coordinate.latitude};
}

auto convert_coordinate_span(mln_coordinate_span span)
  -> std::optional<mbgl::LineString<double>> {
  if (span.coordinate_count > 0 && span.coordinates == nullptr) {
    mln::core::set_thread_error("coordinates must not be null");
    return std::nullopt;
  }

  auto result = mbgl::LineString<double>{};
  result.reserve(span.coordinate_count);
  for (const auto coordinate :
       std::span<const mln_lat_lng>{span.coordinates, span.coordinate_count}) {
    if (!validate_coordinate(coordinate)) {
      return std::nullopt;
    }
    result.emplace_back(to_native_point(coordinate));
  }
  return result;
}

auto convert_polygon(mln_polygon_geometry polygon)
  -> std::optional<mbgl::Polygon<double>> {
  if (polygon.ring_count > 0 && polygon.rings == nullptr) {
    mln::core::set_thread_error("polygon rings must not be null");
    return std::nullopt;
  }

  auto result = mbgl::Polygon<double>{};
  result.reserve(polygon.ring_count);
  for (const auto ring : std::span<const mln_coordinate_span>{
         polygon.rings, polygon.ring_count
       }) {
    auto converted_ring = convert_coordinate_span(ring);
    if (!converted_ring) {
      return std::nullopt;
    }
    result.emplace_back(std::move(*converted_ring));
  }
  return result;
}

auto convert_multi_line(mln_multi_line_geometry multi_line)
  -> std::optional<mbgl::MultiLineString<double>> {
  if (multi_line.line_count > 0 && multi_line.lines == nullptr) {
    mln::core::set_thread_error("multi-line strings must not be null");
    return std::nullopt;
  }

  auto result = mbgl::MultiLineString<double>{};
  result.reserve(multi_line.line_count);
  for (const auto line : std::span<const mln_coordinate_span>{
         multi_line.lines, multi_line.line_count
       }) {
    auto converted_line = convert_coordinate_span(line);
    if (!converted_line) {
      return std::nullopt;
    }
    result.emplace_back(std::move(*converted_line));
  }
  return result;
}

auto convert_multi_polygon(mln_multi_polygon_geometry multi_polygon)
  -> std::optional<mbgl::MultiPolygon<double>> {
  if (multi_polygon.polygon_count > 0 && multi_polygon.polygons == nullptr) {
    mln::core::set_thread_error("multi-polygons must not be null");
    return std::nullopt;
  }

  auto result = mbgl::MultiPolygon<double>{};
  result.reserve(multi_polygon.polygon_count);
  for (const auto polygon : std::span<const mln_polygon_geometry>{
         multi_polygon.polygons, multi_polygon.polygon_count
       }) {
    auto converted_polygon = convert_polygon(polygon);
    if (!converted_polygon) {
      return std::nullopt;
    }
    result.emplace_back(std::move(*converted_polygon));
  }
  return result;
}

auto convert_geometry(const mln_geometry* geometry, std::size_t depth)
  -> std::optional<mbgl::Geometry<double>> {
  if (!validate_depth(depth)) {
    return std::nullopt;
  }
  if (geometry == nullptr) {
    mln::core::set_thread_error("geometry must not be null");
    return std::nullopt;
  }
  if (geometry->size < sizeof(mln_geometry)) {
    mln::core::set_thread_error("mln_geometry.size is too small");
    return std::nullopt;
  }

  switch (geometry->type) {
    case MLN_GEOMETRY_TYPE_EMPTY:
      return mbgl::Geometry<double>{mbgl::EmptyGeometry{}};
    case MLN_GEOMETRY_TYPE_POINT:
      if (!validate_coordinate(geometry->data.point)) {
        return std::nullopt;
      }
      return mbgl::Geometry<double>{to_native_point(geometry->data.point)};
    case MLN_GEOMETRY_TYPE_LINE_STRING: {
      auto line = convert_coordinate_span(geometry->data.line_string);
      if (!line) {
        return std::nullopt;
      }
      return mbgl::Geometry<double>{std::move(*line)};
    }
    case MLN_GEOMETRY_TYPE_POLYGON: {
      auto polygon = convert_polygon(geometry->data.polygon);
      if (!polygon) {
        return std::nullopt;
      }
      return mbgl::Geometry<double>{std::move(*polygon)};
    }
    case MLN_GEOMETRY_TYPE_MULTI_POINT: {
      auto points = convert_coordinate_span(geometry->data.multi_point);
      if (!points) {
        return std::nullopt;
      }
      return mbgl::Geometry<double>{mbgl::MultiPoint<double>{
        std::make_move_iterator(points->begin()),
        std::make_move_iterator(points->end())
      }};
    }
    case MLN_GEOMETRY_TYPE_MULTI_LINE_STRING: {
      auto multi_line = convert_multi_line(geometry->data.multi_line_string);
      if (!multi_line) {
        return std::nullopt;
      }
      return mbgl::Geometry<double>{std::move(*multi_line)};
    }
    case MLN_GEOMETRY_TYPE_MULTI_POLYGON: {
      auto multi_polygon = convert_multi_polygon(geometry->data.multi_polygon);
      if (!multi_polygon) {
        return std::nullopt;
      }
      return mbgl::Geometry<double>{std::move(*multi_polygon)};
    }
    case MLN_GEOMETRY_TYPE_GEOMETRY_COLLECTION: {
      const auto collection = geometry->data.geometry_collection;
      if (collection.geometry_count > 0 && collection.geometries == nullptr) {
        mln::core::set_thread_error("geometry collection must not be null");
        return std::nullopt;
      }

      auto result = mapbox::geometry::geometry_collection<double>{};
      result.reserve(collection.geometry_count);
      for (const auto& child : std::span<const mln_geometry>{
             collection.geometries, collection.geometry_count
           }) {
        auto converted_child = convert_geometry(&child, depth + 1);
        if (!converted_child) {
          return std::nullopt;
        }
        result.emplace_back(std::move(*converted_child));
      }
      return mbgl::Geometry<double>{std::move(result)};
    }
    default:
      mln::core::set_thread_error("geometry type is invalid");
      return std::nullopt;
  }
}

auto convert_value(const mln_json_value* value, std::size_t depth)
  -> std::optional<mbgl::Value>;

auto convert_array_value(mln_json_array array, std::size_t depth)
  -> std::optional<mbgl::Value> {
  if (array.value_count > 0 && array.values == nullptr) {
    mln::core::set_thread_error("array values must not be null");
    return std::nullopt;
  }

  auto result = mbgl::Value::array_type{};
  result.reserve(array.value_count);
  for (const auto& child :
       std::span<const mln_json_value>{array.values, array.value_count}) {
    auto converted_child = convert_value(&child, depth + 1);
    if (!converted_child) {
      return std::nullopt;
    }
    result.emplace_back(std::move(*converted_child));
  }
  return mbgl::Value{std::move(result)};
}

auto convert_object_value(mln_json_object object, std::size_t depth)
  -> std::optional<mbgl::Value> {
  if (object.member_count > 0 && object.members == nullptr) {
    mln::core::set_thread_error("object members must not be null");
    return std::nullopt;
  }

  auto result = mbgl::Value::object_type{};
  for (const auto& member :
       std::span<const mln_json_member>{object.members, object.member_count}) {
    if (!validate_string(member.key)) {
      return std::nullopt;
    }
    auto converted_value = convert_value(member.value, depth + 1);
    if (!converted_value) {
      return std::nullopt;
    }
    result[to_string(member.key)] = std::move(*converted_value);
  }
  return mbgl::Value{std::move(result)};
}

auto convert_value(const mln_json_value* value, std::size_t depth)
  -> std::optional<mbgl::Value> {
  if (!validate_depth(depth)) {
    return std::nullopt;
  }
  if (value == nullptr) {
    mln::core::set_thread_error("value must not be null");
    return std::nullopt;
  }
  if (value->size < sizeof(mln_json_value)) {
    mln::core::set_thread_error("mln_json_value.size is too small");
    return std::nullopt;
  }

  switch (value->type) {
    case MLN_JSON_VALUE_TYPE_NULL:
      return mbgl::Value{mbgl::NullValue{}};
    case MLN_JSON_VALUE_TYPE_BOOL:
      return mbgl::Value{value->data.bool_value};
    case MLN_JSON_VALUE_TYPE_UINT:
      return mbgl::Value{value->data.uint_value};
    case MLN_JSON_VALUE_TYPE_INT:
      return mbgl::Value{value->data.int_value};
    case MLN_JSON_VALUE_TYPE_DOUBLE:
      if (!std::isfinite(value->data.double_value)) {
        mln::core::set_thread_error("double value must be finite");
        return std::nullopt;
      }
      return mbgl::Value{value->data.double_value};
    case MLN_JSON_VALUE_TYPE_STRING:
      if (!validate_string(value->data.string_value)) {
        return std::nullopt;
      }
      return mbgl::Value{to_string(value->data.string_value)};
    case MLN_JSON_VALUE_TYPE_ARRAY:
      return convert_array_value(value->data.array_value, depth);
    case MLN_JSON_VALUE_TYPE_OBJECT:
      return convert_object_value(value->data.object_value, depth);
    default:
      mln::core::set_thread_error("JSON value type is invalid");
      return std::nullopt;
  }
}

auto convert_identifier(const mln_feature& feature)
  -> std::optional<mbgl::FeatureIdentifier> {
  switch (feature.identifier_type) {
    case MLN_FEATURE_IDENTIFIER_TYPE_NULL:
      return mbgl::FeatureIdentifier{mbgl::NullValue{}};
    case MLN_FEATURE_IDENTIFIER_TYPE_UINT:
      return mbgl::FeatureIdentifier{feature.identifier.uint_value};
    case MLN_FEATURE_IDENTIFIER_TYPE_INT:
      return mbgl::FeatureIdentifier{feature.identifier.int_value};
    case MLN_FEATURE_IDENTIFIER_TYPE_DOUBLE:
      if (!std::isfinite(feature.identifier.double_value)) {
        mln::core::set_thread_error("feature identifier double must be finite");
        return std::nullopt;
      }
      return mbgl::FeatureIdentifier{feature.identifier.double_value};
    case MLN_FEATURE_IDENTIFIER_TYPE_STRING:
      if (!validate_string(feature.identifier.string_value)) {
        return std::nullopt;
      }
      return mbgl::FeatureIdentifier{
        to_string(feature.identifier.string_value)
      };
    default:
      mln::core::set_thread_error("feature identifier type is invalid");
      return std::nullopt;
  }
}

auto convert_feature(const mln_feature* feature, std::size_t depth)
  -> std::optional<mbgl::GeoJSONFeature> {
  if (!validate_depth(depth)) {
    return std::nullopt;
  }
  if (feature == nullptr) {
    mln::core::set_thread_error("feature must not be null");
    return std::nullopt;
  }
  if (feature->size < sizeof(mln_feature)) {
    mln::core::set_thread_error("mln_feature.size is too small");
    return std::nullopt;
  }
  if (feature->property_count > 0 && feature->properties == nullptr) {
    mln::core::set_thread_error("feature properties must not be null");
    return std::nullopt;
  }

  auto geometry = convert_geometry(feature->geometry, depth + 1);
  if (!geometry) {
    return std::nullopt;
  }
  auto identifier = convert_identifier(*feature);
  if (!identifier) {
    return std::nullopt;
  }

  auto properties = mbgl::PropertyMap{};
  for (const auto& property : std::span<const mln_json_member>{
         feature->properties, feature->property_count
       }) {
    if (!validate_string(property.key)) {
      return std::nullopt;
    }
    auto value = convert_value(property.value, depth + 1);
    if (!value) {
      return std::nullopt;
    }
    properties[to_string(property.key)] = std::move(*value);
  }

  return mbgl::GeoJSONFeature{
    std::move(*geometry), std::move(properties), std::move(*identifier)
  };
}

}  // namespace

namespace mln::core {

auto to_native_geometry(const mln_geometry* geometry)
  -> std::optional<mbgl::Geometry<double>> {
  return convert_geometry(geometry, 0);
}

auto to_native_json_value(const mln_json_value* value)
  -> std::optional<mbgl::Value> {
  return convert_value(value, 0);
}

auto to_native_feature(const mln_feature* feature)
  -> std::optional<mbgl::GeoJSONFeature> {
  return convert_feature(feature, 0);
}

auto to_native_geojson(const mln_geojson* geojson)
  -> std::optional<mbgl::GeoJSON> {
  if (geojson == nullptr) {
    set_thread_error("GeoJSON must not be null");
    return std::nullopt;
  }
  if (geojson->size < sizeof(mln_geojson)) {
    set_thread_error("mln_geojson.size is too small");
    return std::nullopt;
  }

  switch (geojson->type) {
    case MLN_GEOJSON_TYPE_GEOMETRY: {
      auto geometry = to_native_geometry(geojson->data.geometry);
      if (!geometry) {
        return std::nullopt;
      }
      return mbgl::GeoJSON{std::move(*geometry)};
    }
    case MLN_GEOJSON_TYPE_FEATURE: {
      auto feature = to_native_feature(geojson->data.feature);
      if (!feature) {
        return std::nullopt;
      }
      return mbgl::GeoJSON{std::move(*feature)};
    }
    case MLN_GEOJSON_TYPE_FEATURE_COLLECTION: {
      const auto collection = geojson->data.feature_collection;
      if (collection.feature_count > 0 && collection.features == nullptr) {
        set_thread_error("feature collection must not be null");
        return std::nullopt;
      }

      auto result = mbgl::FeatureCollection{};
      result.reserve(collection.feature_count);
      for (const auto& feature : std::span<const mln_feature>{
             collection.features, collection.feature_count
           }) {
        auto converted_feature = convert_feature(&feature, 1);
        if (!converted_feature) {
          return std::nullopt;
        }
        result.emplace_back(std::move(*converted_feature));
      }
      return mbgl::GeoJSON{std::move(result)};
    }
    default:
      set_thread_error("GeoJSON type is invalid");
      return std::nullopt;
  }
}

auto geometry_lat_lngs(const mbgl::Geometry<double>& geometry)
  -> std::vector<mbgl::LatLng> {
  auto result = std::vector<mbgl::LatLng>{};
  mbgl::forEachPoint(geometry, [&](const mbgl::Point<double>& point) -> void {
    result.emplace_back(point.y, point.x);
  });
  return result;
}

}  // namespace mln::core

// NOLINTEND(cppcoreguidelines-pro-type-union-access)
