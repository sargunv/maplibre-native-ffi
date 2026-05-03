#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

#include "maplibre_native_c.h"

namespace mbgl {
class Map;
class RendererObserver;
class UpdateParameters;
}  // namespace mbgl

namespace mln::core {

auto map_options_default() noexcept -> mln_map_options;
auto camera_options_default() noexcept -> mln_camera_options;
auto animation_options_default() noexcept -> mln_animation_options;
auto camera_fit_options_default() noexcept -> mln_camera_fit_options;
auto bound_options_default() noexcept -> mln_bound_options;
auto free_camera_options_default() noexcept -> mln_free_camera_options;
auto projection_mode_default() noexcept -> mln_projection_mode;
auto map_viewport_options_default() noexcept -> mln_map_viewport_options;
auto map_tile_options_default() noexcept -> mln_map_tile_options;
auto style_tile_source_options_default() noexcept
  -> mln_style_tile_source_options;
auto custom_geometry_source_options_default() noexcept
  -> mln_custom_geometry_source_options;
auto premultiplied_rgba8_image_default() noexcept
  -> mln_premultiplied_rgba8_image;
auto style_image_options_default() noexcept -> mln_style_image_options;
auto style_image_info_default() noexcept -> mln_style_image_info;
auto create_map(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) -> mln_status;
auto destroy_map(mln_map* map) -> mln_status;
auto map_request_repaint(mln_map* map) -> mln_status;
auto map_request_still_image(mln_map* map) -> mln_status;
auto map_set_style_url(mln_map* map, const char* url) -> mln_status;
auto map_set_style_json(mln_map* map, const char* json) -> mln_status;
auto style_id_list_count(const mln_style_id_list* list, size_t* out_count)
  -> mln_status;
auto style_id_list_get(
  const mln_style_id_list* list, size_t index, mln_string_view* out_id
) -> mln_status;
auto style_id_list_destroy(mln_style_id_list* list) -> void;
auto map_add_style_source_json(
  mln_map* map, mln_string_view source_id, const mln_json_value* source_json
) -> mln_status;
auto map_remove_style_source(
  mln_map* map, mln_string_view source_id, bool* out_removed
) -> mln_status;
auto map_style_source_exists(
  mln_map* map, mln_string_view source_id, bool* out_exists
) -> mln_status;
auto map_get_style_source_type(
  mln_map* map, mln_string_view source_id, uint32_t* out_source_type,
  bool* out_found
) -> mln_status;
auto map_get_style_source_info(
  mln_map* map, mln_string_view source_id, mln_style_source_info* out_info,
  bool* out_found
) -> mln_status;
auto map_copy_style_source_attribution(
  mln_map* map, mln_string_view source_id, char* out_attribution,
  size_t attribution_capacity, size_t* out_attribution_size, bool* out_found
) -> mln_status;
auto map_list_style_source_ids(mln_map* map, mln_style_id_list** out_source_ids)
  -> mln_status;
auto map_add_geojson_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) -> mln_status;
auto map_add_geojson_source_data(
  mln_map* map, mln_string_view source_id, const mln_geojson* data
) -> mln_status;
auto map_set_geojson_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) -> mln_status;
auto map_set_geojson_source_data(
  mln_map* map, mln_string_view source_id, const mln_geojson* data
) -> mln_status;
auto map_add_vector_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) -> mln_status;
auto map_add_vector_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) -> mln_status;
auto map_add_raster_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) -> mln_status;
auto map_add_raster_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) -> mln_status;
auto map_add_raster_dem_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) -> mln_status;
auto map_add_raster_dem_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) -> mln_status;
auto map_add_custom_geometry_source(
  mln_map* map, mln_string_view source_id,
  const mln_custom_geometry_source_options* options
) -> mln_status;
auto map_set_custom_geometry_source_tile_data(
  mln_map* map, mln_string_view source_id, mln_canonical_tile_id tile_id,
  const mln_geojson* data
) -> mln_status;
auto map_invalidate_custom_geometry_source_tile(
  mln_map* map, mln_string_view source_id, mln_canonical_tile_id tile_id
) -> mln_status;
auto map_invalidate_custom_geometry_source_region(
  mln_map* map, mln_string_view source_id, mln_lat_lng_bounds bounds
) -> mln_status;
auto map_set_style_image(
  mln_map* map, mln_string_view image_id,
  const mln_premultiplied_rgba8_image* image,
  const mln_style_image_options* options
) -> mln_status;
auto map_remove_style_image(
  mln_map* map, mln_string_view image_id, bool* out_removed
) -> mln_status;
auto map_style_image_exists(
  mln_map* map, mln_string_view image_id, bool* out_exists
) -> mln_status;
auto map_get_style_image_info(
  mln_map* map, mln_string_view image_id, mln_style_image_info* out_info,
  bool* out_found
) -> mln_status;
auto map_copy_style_image_premultiplied_rgba8(
  mln_map* map, mln_string_view image_id, uint8_t* out_pixels,
  size_t pixel_capacity, size_t* out_byte_length, bool* out_found
) -> mln_status;
auto map_add_image_source_url(
  mln_map* map, mln_string_view source_id, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_string_view url
) -> mln_status;
auto map_add_image_source_image(
  mln_map* map, mln_string_view source_id, const mln_lat_lng* coordinates,
  size_t coordinate_count, const mln_premultiplied_rgba8_image* image
) -> mln_status;
auto map_set_image_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) -> mln_status;
auto map_set_image_source_image(
  mln_map* map, mln_string_view source_id,
  const mln_premultiplied_rgba8_image* image
) -> mln_status;
auto map_set_image_source_coordinates(
  mln_map* map, mln_string_view source_id, const mln_lat_lng* coordinates,
  size_t coordinate_count
) -> mln_status;
auto map_get_image_source_coordinates(
  mln_map* map, mln_string_view source_id, mln_lat_lng* out_coordinates,
  size_t coordinate_capacity, size_t* out_coordinate_count, bool* out_found
) -> mln_status;
auto map_add_hillshade_layer(
  mln_map* map, mln_string_view layer_id, mln_string_view source_id,
  mln_string_view before_layer_id
) -> mln_status;
auto map_add_color_relief_layer(
  mln_map* map, mln_string_view layer_id, mln_string_view source_id,
  mln_string_view before_layer_id
) -> mln_status;
auto map_add_location_indicator_layer(
  mln_map* map, mln_string_view layer_id, mln_string_view before_layer_id
) -> mln_status;
auto map_set_location_indicator_location(
  mln_map* map, mln_string_view layer_id, mln_lat_lng coordinate,
  double altitude
) -> mln_status;
auto map_set_location_indicator_bearing(
  mln_map* map, mln_string_view layer_id, double bearing
) -> mln_status;
auto map_set_location_indicator_accuracy_radius(
  mln_map* map, mln_string_view layer_id, double radius
) -> mln_status;
auto map_set_location_indicator_image_name(
  mln_map* map, mln_string_view layer_id, uint32_t image_kind,
  mln_string_view image_id
) -> mln_status;
auto map_add_style_layer_json(
  mln_map* map, const mln_json_value* layer_json,
  mln_string_view before_layer_id
) -> mln_status;
auto map_remove_style_layer(
  mln_map* map, mln_string_view layer_id, bool* out_removed
) -> mln_status;
auto map_style_layer_exists(
  mln_map* map, mln_string_view layer_id, bool* out_exists
) -> mln_status;
auto map_get_style_layer_type(
  mln_map* map, mln_string_view layer_id, mln_string_view* out_layer_type,
  bool* out_found
) -> mln_status;
auto map_list_style_layer_ids(mln_map* map, mln_style_id_list** out_layer_ids)
  -> mln_status;
auto map_move_style_layer(
  mln_map* map, mln_string_view layer_id, mln_string_view before_layer_id
) -> mln_status;
auto map_get_style_layer_json(
  mln_map* map, mln_string_view layer_id, mln_json_snapshot** out_layer,
  bool* out_found
) -> mln_status;
auto map_set_style_light_json(mln_map* map, const mln_json_value* light_json)
  -> mln_status;
auto map_set_style_light_property(
  mln_map* map, mln_string_view property_name, const mln_json_value* value
) -> mln_status;
auto map_get_style_light_property(
  mln_map* map, mln_string_view property_name, mln_json_snapshot** out_value
) -> mln_status;
auto map_set_layer_property(
  mln_map* map, mln_string_view layer_id, mln_string_view property_name,
  const mln_json_value* value
) -> mln_status;
auto map_get_layer_property(
  mln_map* map, mln_string_view layer_id, mln_string_view property_name,
  mln_json_snapshot** out_value
) -> mln_status;
auto map_set_layer_filter(
  mln_map* map, mln_string_view layer_id, const mln_json_value* filter
) -> mln_status;
auto map_get_layer_filter(
  mln_map* map, mln_string_view layer_id, mln_json_snapshot** out_filter
) -> mln_status;
auto map_get_camera(mln_map* map, mln_camera_options* out_camera) -> mln_status;
auto map_jump_to(mln_map* map, const mln_camera_options* camera) -> mln_status;
auto map_ease_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) -> mln_status;
auto map_fly_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) -> mln_status;
auto map_get_projection_mode(mln_map* map, mln_projection_mode* out_mode)
  -> mln_status;
auto map_set_projection_mode(mln_map* map, const mln_projection_mode* mode)
  -> mln_status;
auto map_set_debug_options(mln_map* map, uint32_t options) -> mln_status;
auto map_get_debug_options(mln_map* map, uint32_t* out_options) -> mln_status;
auto map_set_rendering_stats_view_enabled(mln_map* map, bool enabled)
  -> mln_status;
auto map_get_rendering_stats_view_enabled(mln_map* map, bool* out_enabled)
  -> mln_status;
auto map_is_fully_loaded(mln_map* map, bool* out_loaded) -> mln_status;
auto map_dump_debug_logs(mln_map* map) -> mln_status;
auto map_get_viewport_options(
  mln_map* map, mln_map_viewport_options* out_options
) -> mln_status;
auto map_set_viewport_options(
  mln_map* map, const mln_map_viewport_options* options
) -> mln_status;
auto map_get_tile_options(mln_map* map, mln_map_tile_options* out_options)
  -> mln_status;
auto map_set_tile_options(mln_map* map, const mln_map_tile_options* options)
  -> mln_status;
auto map_pixel_for_lat_lng(
  mln_map* map, mln_lat_lng coordinate, mln_screen_point* out_point
) -> mln_status;
auto map_lat_lng_for_pixel(
  mln_map* map, mln_screen_point point, mln_lat_lng* out_coordinate
) -> mln_status;
auto map_pixels_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  mln_screen_point* out_points
) -> mln_status;
auto map_lat_lngs_for_pixels(
  mln_map* map, const mln_screen_point* points, size_t point_count,
  mln_lat_lng* out_coordinates
) -> mln_status;
auto map_projection_create(mln_map* map, mln_map_projection** out_projection)
  -> mln_status;
auto map_projection_destroy(mln_map_projection* projection) -> mln_status;
auto map_projection_get_camera(
  mln_map_projection* projection, mln_camera_options* out_camera
) -> mln_status;
auto map_projection_set_camera(
  mln_map_projection* projection, const mln_camera_options* camera
) -> mln_status;
auto map_projection_set_visible_coordinates(
  mln_map_projection* projection, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_edge_insets padding
) -> mln_status;
auto map_projection_set_visible_geometry(
  mln_map_projection* projection, const mln_geometry* geometry,
  mln_edge_insets padding
) -> mln_status;
auto map_projection_pixel_for_lat_lng(
  mln_map_projection* projection, mln_lat_lng coordinate,
  mln_screen_point* out_point
) -> mln_status;
auto map_projection_lat_lng_for_pixel(
  mln_map_projection* projection, mln_screen_point point,
  mln_lat_lng* out_coordinate
) -> mln_status;
auto projected_meters_for_lat_lng(
  mln_lat_lng coordinate, mln_projected_meters* out_meters
) -> mln_status;
auto lat_lng_for_projected_meters(
  mln_projected_meters meters, mln_lat_lng* out_coordinate
) -> mln_status;
auto map_move_by(mln_map* map, double delta_x, double delta_y) -> mln_status;
auto map_move_by_animated(
  mln_map* map, double delta_x, double delta_y,
  const mln_animation_options* animation
) -> mln_status;
auto map_scale_by(mln_map* map, double scale, const mln_screen_point* anchor)
  -> mln_status;
auto map_scale_by_animated(
  mln_map* map, double scale, const mln_screen_point* anchor,
  const mln_animation_options* animation
) -> mln_status;
auto map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) -> mln_status;
auto map_rotate_by_animated(
  mln_map* map, mln_screen_point first, mln_screen_point second,
  const mln_animation_options* animation
) -> mln_status;
auto map_pitch_by(mln_map* map, double pitch) -> mln_status;
auto map_pitch_by_animated(
  mln_map* map, double pitch, const mln_animation_options* animation
) -> mln_status;
auto map_cancel_transitions(mln_map* map) -> mln_status;
auto map_camera_for_lat_lng_bounds(
  mln_map* map, mln_lat_lng_bounds bounds,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) -> mln_status;
auto map_camera_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) -> mln_status;
auto map_camera_for_geometry(
  mln_map* map, const mln_geometry* geometry,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) -> mln_status;
auto map_lat_lng_bounds_for_camera(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) -> mln_status;
auto map_lat_lng_bounds_for_camera_unwrapped(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) -> mln_status;
auto map_get_bounds(mln_map* map, mln_bound_options* out_options) -> mln_status;
auto map_set_bounds(mln_map* map, const mln_bound_options* options)
  -> mln_status;
auto map_get_free_camera_options(
  mln_map* map, mln_free_camera_options* out_options
) -> mln_status;
auto map_set_free_camera_options(
  mln_map* map, const mln_free_camera_options* options
) -> mln_status;
auto validate_map(mln_map* map) -> mln_status;
auto map_owner_thread(const mln_map* map) -> std::thread::id;
auto map_native(mln_map* map) -> mbgl::Map*;
auto map_latest_update(mln_map* map) -> std::shared_ptr<mbgl::UpdateParameters>;
auto map_renderer_observer(mln_map* map) -> mbgl::RendererObserver*;
auto map_run_render_jobs(mln_map* map) -> void;
auto map_attach_render_target_session(mln_map* map, void* session)
  -> mln_status;
auto map_detach_render_target_session(mln_map* map, void* session)
  -> mln_status;

}  // namespace mln::core
