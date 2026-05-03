#pragma once

#include <memory>
#include <optional>
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

auto to_native_geometry(const mln_geometry* geometry)
  -> std::optional<mbgl::Geometry<double>>;
auto to_c_geometry(const mbgl::Geometry<double>& geometry)
  -> std::unique_ptr<OwnedGeometryDescriptor>;
auto to_native_json_value(const mln_json_value* value)
  -> std::optional<mbgl::Value>;
auto to_native_feature(const mln_feature* feature)
  -> std::optional<mbgl::GeoJSONFeature>;
auto to_native_geojson(const mln_geojson* geojson)
  -> std::optional<mbgl::GeoJSON>;
auto geometry_lat_lngs(const mbgl::Geometry<double>& geometry)
  -> std::vector<mbgl::LatLng>;

}  // namespace mln::core
