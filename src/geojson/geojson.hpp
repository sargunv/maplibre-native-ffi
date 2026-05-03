#pragma once

#include <optional>
#include <vector>

#include <mbgl/util/feature.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/geojson.hpp>
#include <mbgl/util/geometry.hpp>

#include "maplibre_native_c.h"

namespace mln::core {

auto to_native_geometry(const mln_geometry* geometry)
  -> std::optional<mbgl::Geometry<double>>;
auto to_native_json_value(const mln_json_value* value)
  -> std::optional<mbgl::Value>;
auto to_native_feature(const mln_feature* feature)
  -> std::optional<mbgl::GeoJSONFeature>;
auto to_native_geojson(const mln_geojson* geojson)
  -> std::optional<mbgl::GeoJSON>;
auto geometry_lat_lngs(const mbgl::Geometry<double>& geometry)
  -> std::vector<mbgl::LatLng>;

}  // namespace mln::core
