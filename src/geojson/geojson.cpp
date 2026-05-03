#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

struct mln_json_snapshot {
  std::unique_ptr<mln::core::OwnedJsonDescriptor> value;
};

namespace {

constexpr std::size_t max_recursive_depth = 64;

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

using GeometryDescriptorPtr =
  std::unique_ptr<mln::core::OwnedGeometryDescriptor>;
using JsonDescriptorPtr = std::unique_ptr<mln::core::OwnedJsonDescriptor>;

auto json_snapshot_mutex() -> std::mutex& {
  static auto value = std::mutex{};
  return value;
}

auto json_snapshots() -> std::unordered_map<
  const mln_json_snapshot*, std::unique_ptr<mln_json_snapshot>>& {
  static auto value = std::unordered_map<
    const mln_json_snapshot*, std::unique_ptr<mln_json_snapshot>>{};
  return value;
}

auto validate_depth(std::size_t depth) -> bool {
  if (depth > max_recursive_depth) {
    mln::core::set_thread_error("GeoJSON value nesting is too deep");
    return false;
  }
  return true;
}

auto validate_export_depth(std::size_t depth) -> void {
  if (depth > max_recursive_depth) {
    throw std::runtime_error("GeoJSON value nesting is too deep");
  }
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

auto from_native_point(const mbgl::Point<double>& point) -> mln_lat_lng {
  return mln_lat_lng{.latitude = point.y, .longitude = point.x};
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

template <typename PointList>
auto to_coordinate_list(const PointList& points) -> std::vector<mln_lat_lng> {
  auto result = std::vector<mln_lat_lng>{};
  result.reserve(points.size());
  for (const auto& point : points) {
    result.emplace_back(from_native_point(point));
  }
  return result;
}

auto coordinate_span(const std::vector<mln_lat_lng>& coordinates)
  -> mln_coordinate_span {
  return mln_coordinate_span{
    .coordinates = coordinates.empty() ? nullptr : coordinates.data(),
    .coordinate_count = coordinates.size()
  };
}

auto make_geometry_descriptor(
  const mbgl::Geometry<double>& geometry, std::size_t depth
) -> GeometryDescriptorPtr;

auto make_geometry_descriptor(
  const mapbox::geometry::geometry_collection<double>& collection,
  std::size_t depth
) -> GeometryDescriptorPtr {
  auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
  result->root = mln_geometry{
    .size = sizeof(mln_geometry),
    .type = MLN_GEOMETRY_TYPE_GEOMETRY_COLLECTION,
    .data = {.geometry_collection = {}}
  };
  result->children.reserve(collection.size());
  result->child_geometries.reserve(collection.size());
  for (const auto& child : collection) {
    result->children.emplace_back(make_geometry_descriptor(child, depth + 1));
  }
  for (const auto& child : result->children) {
    result->child_geometries.emplace_back(child->root);
  }
  result->root.data.geometry_collection = mln_geometry_collection{
    .geometries = result->child_geometries.empty()
                    ? nullptr
                    : result->child_geometries.data(),
    .geometry_count = result->child_geometries.size()
  };
  return result;
}

auto make_polygon_descriptor(const mbgl::Polygon<double>& polygon)
  -> GeometryDescriptorPtr {
  auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
  result->coordinate_lists.reserve(polygon.size());
  result->coordinate_spans.reserve(polygon.size());
  for (const auto& ring : polygon) {
    result->coordinate_lists.emplace_back(to_coordinate_list(ring));
  }
  for (const auto& coordinates : result->coordinate_lists) {
    result->coordinate_spans.emplace_back(coordinate_span(coordinates));
  }
  result->root = mln_geometry{
    .size = sizeof(mln_geometry),
    .type = MLN_GEOMETRY_TYPE_POLYGON,
    .data = {
      .polygon = {
        .rings = result->coordinate_spans.empty()
                   ? nullptr
                   : result->coordinate_spans.data(),
        .ring_count = result->coordinate_spans.size()
      }
    }
  };
  return result;
}

auto make_multi_line_descriptor(const mbgl::MultiLineString<double>& multi_line)
  -> GeometryDescriptorPtr {
  auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
  result->coordinate_lists.reserve(multi_line.size());
  result->coordinate_spans.reserve(multi_line.size());
  for (const auto& line : multi_line) {
    result->coordinate_lists.emplace_back(to_coordinate_list(line));
  }
  for (const auto& coordinates : result->coordinate_lists) {
    result->coordinate_spans.emplace_back(coordinate_span(coordinates));
  }
  result->root = mln_geometry{
    .size = sizeof(mln_geometry),
    .type = MLN_GEOMETRY_TYPE_MULTI_LINE_STRING,
    .data = {
      .multi_line_string = {
        .lines = result->coordinate_spans.empty()
                   ? nullptr
                   : result->coordinate_spans.data(),
        .line_count = result->coordinate_spans.size()
      }
    }
  };
  return result;
}

auto make_multi_polygon_descriptor(
  const mbgl::MultiPolygon<double>& multi_polygon
) -> GeometryDescriptorPtr {
  auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
  auto ring_count = std::size_t{0};
  for (const auto& polygon : multi_polygon) {
    ring_count += polygon.size();
  }
  result->coordinate_lists.reserve(ring_count);
  result->coordinate_spans.reserve(ring_count);
  result->polygons.reserve(multi_polygon.size());

  for (const auto& polygon : multi_polygon) {
    const auto first_ring = result->coordinate_spans.size();
    for (const auto& ring : polygon) {
      result->coordinate_lists.emplace_back(to_coordinate_list(ring));
      result->coordinate_spans.emplace_back(
        coordinate_span(result->coordinate_lists.back())
      );
    }
    result->polygons.emplace_back(
      mln_polygon_geometry{
        .rings =
          polygon.empty() ? nullptr : &result->coordinate_spans.at(first_ring),
        .ring_count = polygon.size()
      }
    );
  }

  result->root = mln_geometry{
    .size = sizeof(mln_geometry),
    .type = MLN_GEOMETRY_TYPE_MULTI_POLYGON,
    .data = {
      .multi_polygon = {
        .polygons =
          result->polygons.empty() ? nullptr : result->polygons.data(),
        .polygon_count = result->polygons.size()
      }
    }
  };
  return result;
}

auto make_geometry_descriptor(
  const mbgl::Geometry<double>& geometry, std::size_t depth
) -> GeometryDescriptorPtr {
  validate_export_depth(depth);
  return geometry.match(
    Overloaded{
      [](const mbgl::EmptyGeometry&) -> GeometryDescriptorPtr {
        auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
        result->root = mln_geometry{
          .size = sizeof(mln_geometry),
          .type = MLN_GEOMETRY_TYPE_EMPTY,
          .data = {.point = {}}
        };
        return result;
      },
      [](const mbgl::Point<double>& point) -> GeometryDescriptorPtr {
        auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
        result->root = mln_geometry{
          .size = sizeof(mln_geometry),
          .type = MLN_GEOMETRY_TYPE_POINT,
          .data = {.point = from_native_point(point)}
        };
        return result;
      },
      [](const mbgl::LineString<double>& line) -> GeometryDescriptorPtr {
        auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
        result->coordinate_lists.emplace_back(to_coordinate_list(line));
        result->root = mln_geometry{
          .size = sizeof(mln_geometry),
          .type = MLN_GEOMETRY_TYPE_LINE_STRING,
          .data = {
            .line_string = coordinate_span(result->coordinate_lists.front())
          }
        };
        return result;
      },
      [](const mbgl::Polygon<double>& polygon) -> GeometryDescriptorPtr {
        return make_polygon_descriptor(polygon);
      },
      [](const mbgl::MultiPoint<double>& points) -> GeometryDescriptorPtr {
        auto result = std::make_unique<mln::core::OwnedGeometryDescriptor>();
        result->coordinate_lists.emplace_back(to_coordinate_list(points));
        result->root = mln_geometry{
          .size = sizeof(mln_geometry),
          .type = MLN_GEOMETRY_TYPE_MULTI_POINT,
          .data = {
            .multi_point = coordinate_span(result->coordinate_lists.front())
          }
        };
        return result;
      },
      [](const mbgl::MultiLineString<double>& multi_line)
        -> GeometryDescriptorPtr {
        return make_multi_line_descriptor(multi_line);
      },
      [](const mbgl::MultiPolygon<double>& multi_polygon)
        -> GeometryDescriptorPtr {
        return make_multi_polygon_descriptor(multi_polygon);
      },
      [depth](const mapbox::geometry::geometry_collection<double>& collection)
        -> GeometryDescriptorPtr {
        return make_geometry_descriptor(collection, depth);
      }
    }
  );
}

auto string_view(const std::string& string) -> mln_string_view {
  return mln_string_view{
    .data = string.empty() ? nullptr : string.data(), .size = string.size()
  };
}

auto make_json_descriptor(const mbgl::Value& value, std::size_t depth)
  -> JsonDescriptorPtr;

auto make_array_json_descriptor(
  const mbgl::Value::array_type& array, std::size_t depth
) -> JsonDescriptorPtr {
  auto result = std::make_unique<mln::core::OwnedJsonDescriptor>();
  result->children.reserve(array.size());
  result->child_values.reserve(array.size());
  for (const auto& child : array) {
    result->children.emplace_back(make_json_descriptor(child, depth + 1));
  }
  for (const auto& child : result->children) {
    result->child_values.emplace_back(child->root);
  }

  result->root = mln_json_value{
    .size = sizeof(mln_json_value),
    .type = MLN_JSON_VALUE_TYPE_ARRAY,
    .data = {
      .array_value = {
        .values =
          result->child_values.empty() ? nullptr : result->child_values.data(),
        .value_count = result->child_values.size()
      }
    }
  };
  return result;
}

auto make_object_json_descriptor(
  const mbgl::Value::object_type& object, std::size_t depth
) -> JsonDescriptorPtr {
  auto result = std::make_unique<mln::core::OwnedJsonDescriptor>();
  result->strings.reserve(object.size());
  result->children.reserve(object.size());
  result->child_values.reserve(object.size());
  result->members.reserve(object.size());

  for (const auto& [key, child] : object) {
    result->strings.emplace_back(key);
    result->children.emplace_back(make_json_descriptor(child, depth + 1));
  }
  for (std::size_t index = 0; index < result->children.size(); ++index) {
    result->child_values.emplace_back(result->children.at(index)->root);
    result->members.emplace_back(
      mln_json_member{
        .key = string_view(result->strings.at(index)),
        .value = &result->child_values.back()
      }
    );
  }

  result->root = mln_json_value{
    .size = sizeof(mln_json_value),
    .type = MLN_JSON_VALUE_TYPE_OBJECT,
    .data = {
      .object_value = {
        .members = result->members.empty() ? nullptr : result->members.data(),
        .member_count = result->members.size()
      }
    }
  };
  return result;
}

auto make_json_descriptor(const mbgl::Value& value, std::size_t depth)
  -> JsonDescriptorPtr {
  validate_export_depth(depth);

  auto result = std::make_unique<mln::core::OwnedJsonDescriptor>();
  if (value.is<mbgl::NullValue>()) {
    result->root = mln_json_value{
      .size = sizeof(mln_json_value),
      .type = MLN_JSON_VALUE_TYPE_NULL,
      .data = {.bool_value = false}
    };
    return result;
  }
  if (const auto* bool_value = value.getBool(); bool_value != nullptr) {
    result->root = mln_json_value{
      .size = sizeof(mln_json_value),
      .type = MLN_JSON_VALUE_TYPE_BOOL,
      .data = {.bool_value = *bool_value}
    };
    return result;
  }
  if (const auto* uint_value = value.getUint(); uint_value != nullptr) {
    result->root = mln_json_value{
      .size = sizeof(mln_json_value),
      .type = MLN_JSON_VALUE_TYPE_UINT,
      .data = {.uint_value = *uint_value}
    };
    return result;
  }
  if (const auto* int_value = value.getInt(); int_value != nullptr) {
    result->root = mln_json_value{
      .size = sizeof(mln_json_value),
      .type = MLN_JSON_VALUE_TYPE_INT,
      .data = {.int_value = *int_value}
    };
    return result;
  }
  if (const auto* double_value = value.getDouble(); double_value != nullptr) {
    result->root = mln_json_value{
      .size = sizeof(mln_json_value),
      .type = MLN_JSON_VALUE_TYPE_DOUBLE,
      .data = {.double_value = *double_value}
    };
    return result;
  }
  if (const auto* string_value = value.getString(); string_value != nullptr) {
    result->strings.emplace_back(*string_value);
    result->root = mln_json_value{
      .size = sizeof(mln_json_value),
      .type = MLN_JSON_VALUE_TYPE_STRING,
      .data = {.string_value = string_view(result->strings.front())}
    };
    return result;
  }
  if (const auto* array_value = value.getArray(); array_value != nullptr) {
    return make_array_json_descriptor(*array_value, depth);
  }
  if (const auto* object_value = value.getObject(); object_value != nullptr) {
    return make_object_json_descriptor(*object_value, depth);
  }

  throw std::runtime_error("unsupported JSON value type");
}

}  // namespace

namespace mln::core {

auto to_native_geometry(const mln_geometry* geometry)
  -> std::optional<mbgl::Geometry<double>> {
  return convert_geometry(geometry, 0);
}

auto to_c_geometry(const mbgl::Geometry<double>& geometry)
  -> std::unique_ptr<OwnedGeometryDescriptor> {
  return make_geometry_descriptor(geometry, 0);
}

auto to_native_json_value(const mln_json_value* value)
  -> std::optional<mbgl::Value> {
  return convert_value(value, 0);
}

auto to_c_json_value(const mbgl::Value& value)
  -> std::unique_ptr<OwnedJsonDescriptor> {
  return make_json_descriptor(value, 0);
}

auto json_snapshot_create(mbgl::Value value, mln_json_snapshot** out_snapshot)
  -> mln_status {
  if (out_snapshot == nullptr) {
    set_thread_error("out_state must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_snapshot != nullptr) {
    set_thread_error("*out_state must be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto snapshot = std::make_unique<mln_json_snapshot>();
  snapshot->value = to_c_json_value(value);
  auto* handle = snapshot.get();
  const auto lock = std::scoped_lock{json_snapshot_mutex()};
  json_snapshots().emplace(handle, std::move(snapshot));
  *out_snapshot = handle;
  return MLN_STATUS_OK;
}

auto json_snapshot_get(
  const mln_json_snapshot* snapshot, const mln_json_value** out_value
) -> mln_status {
  if (snapshot == nullptr) {
    set_thread_error("JSON snapshot must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_value == nullptr) {
    set_thread_error("out_value must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto lock = std::scoped_lock{json_snapshot_mutex()};
  const auto found = json_snapshots().find(snapshot);
  if (found == json_snapshots().end()) {
    set_thread_error("JSON snapshot is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_value = &found->second->value->root;
  return MLN_STATUS_OK;
}

auto json_snapshot_destroy(mln_json_snapshot* snapshot) -> void {
  if (snapshot == nullptr) {
    return;
  }

  const auto lock = std::scoped_lock{json_snapshot_mutex()};
  json_snapshots().erase(snapshot);
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
