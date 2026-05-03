#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <mbgl/util/feature.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/geojson.hpp>
#include <mbgl/util/geometry.hpp>

#include "maplibre_native_c.h"

namespace mln::core {

struct OwnedGeometryDescriptor {
  mln_geometry root{};
  std::vector<std::vector<mln_lat_lng>> coordinate_lists;
  std::vector<mln_coordinate_span> coordinate_spans;
  std::vector<mln_polygon_geometry> polygons;
  std::vector<std::unique_ptr<OwnedGeometryDescriptor>> children;
  std::vector<mln_geometry> child_geometries;
};

struct OwnedJsonDescriptor {
  mln_json_value root{};
  std::vector<std::string> strings;
  std::vector<std::unique_ptr<OwnedJsonDescriptor>> children;
  std::vector<mln_json_value> child_values;
  std::vector<mln_json_member> members;
};

auto to_native_geometry(const mln_geometry* geometry)
  -> std::optional<mbgl::Geometry<double>>;
auto to_c_geometry(const mbgl::Geometry<double>& geometry)
  -> std::unique_ptr<OwnedGeometryDescriptor>;
auto to_native_json_value(const mln_json_value* value)
  -> std::optional<mbgl::Value>;
auto to_c_json_value(const mbgl::Value& value)
  -> std::unique_ptr<OwnedJsonDescriptor>;
auto json_snapshot_create(mbgl::Value value, mln_json_snapshot** out_snapshot)
  -> mln_status;
auto json_snapshot_get(
  const mln_json_snapshot* snapshot, const mln_json_value** out_value
) -> mln_status;
auto json_snapshot_destroy(mln_json_snapshot* snapshot) -> void;
auto to_native_feature(const mln_feature* feature)
  -> std::optional<mbgl::GeoJSONFeature>;
auto to_native_geojson(const mln_geojson* geojson)
  -> std::optional<mbgl::GeoJSON>;
auto geometry_lat_lngs(const mbgl::Geometry<double>& geometry)
  -> std::vector<mbgl::LatLng>;

}  // namespace mln::core
