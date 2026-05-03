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
auto create_map(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) -> mln_status;
auto destroy_map(mln_map* map) -> mln_status;
auto map_request_repaint(mln_map* map) -> mln_status;
auto map_request_still_image(mln_map* map) -> mln_status;
auto map_set_style_url(mln_map* map, const char* url) -> mln_status;
auto map_set_style_json(mln_map* map, const char* json) -> mln_status;
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
