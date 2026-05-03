#define MLN_BUILDING_C

#include <cstddef>
#include <cstdint>

#include "map/map.hpp"

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"

auto mln_map_options_default(void) noexcept -> mln_map_options {
  return mln::core::map_options_default();
}

auto mln_camera_options_default(void) noexcept -> mln_camera_options {
  return mln::core::camera_options_default();
}

auto mln_animation_options_default(void) noexcept -> mln_animation_options {
  return mln::core::animation_options_default();
}

auto mln_camera_fit_options_default(void) noexcept -> mln_camera_fit_options {
  return mln::core::camera_fit_options_default();
}

auto mln_bound_options_default(void) noexcept -> mln_bound_options {
  return mln::core::bound_options_default();
}

auto mln_free_camera_options_default(void) noexcept -> mln_free_camera_options {
  return mln::core::free_camera_options_default();
}

auto mln_projection_mode_default(void) noexcept -> mln_projection_mode {
  return mln::core::projection_mode_default();
}

auto mln_map_viewport_options_default(void) noexcept
  -> mln_map_viewport_options {
  return mln::core::map_viewport_options_default();
}

auto mln_map_tile_options_default(void) noexcept -> mln_map_tile_options {
  return mln::core::map_tile_options_default();
}

auto mln_map_create(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::create_map(runtime, options, out_map);
  });
}

auto mln_map_destroy(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::destroy_map(map);
  });
}

auto mln_map_request_repaint(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_request_repaint(map);
  });
}

auto mln_map_request_still_image(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_request_still_image(map);
  });
}

auto mln_map_set_style_url(mln_map* map, const char* url) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_style_url(map, url);
  });
}

auto mln_map_set_style_json(mln_map* map, const char* json) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_style_json(map, json);
  });
}

auto mln_map_get_camera(mln_map* map, mln_camera_options* out_camera) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_camera(map, out_camera);
  });
}

auto mln_map_jump_to(mln_map* map, const mln_camera_options* camera) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_jump_to(map, camera);
  });
}

auto mln_map_ease_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_ease_to(map, camera, animation);
  });
}

auto mln_map_fly_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_fly_to(map, camera, animation);
  });
}

auto mln_map_get_projection_mode(
  mln_map* map, mln_projection_mode* out_mode
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_projection_mode(map, out_mode);
  });
}

auto mln_map_set_projection_mode(
  mln_map* map, const mln_projection_mode* mode
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_projection_mode(map, mode);
  });
}

auto mln_map_set_debug_options(mln_map* map, uint32_t options) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_debug_options(map, options);
  });
}

auto mln_map_get_debug_options(mln_map* map, uint32_t* out_options) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_debug_options(map, out_options);
  });
}

auto mln_map_set_rendering_stats_view_enabled(
  mln_map* map, bool enabled
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_rendering_stats_view_enabled(map, enabled);
  });
}

auto mln_map_get_rendering_stats_view_enabled(
  mln_map* map, bool* out_enabled
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_rendering_stats_view_enabled(map, out_enabled);
  });
}

auto mln_map_is_fully_loaded(mln_map* map, bool* out_loaded) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_is_fully_loaded(map, out_loaded);
  });
}

auto mln_map_dump_debug_logs(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_dump_debug_logs(map);
  });
}

auto mln_map_get_viewport_options(
  mln_map* map, mln_map_viewport_options* out_options
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_viewport_options(map, out_options);
  });
}

auto mln_map_set_viewport_options(
  mln_map* map, const mln_map_viewport_options* options
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_viewport_options(map, options);
  });
}

auto mln_map_get_tile_options(
  mln_map* map, mln_map_tile_options* out_options
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_tile_options(map, out_options);
  });
}

auto mln_map_set_tile_options(
  mln_map* map, const mln_map_tile_options* options
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_tile_options(map, options);
  });
}

auto mln_map_pixel_for_lat_lng(
  mln_map* map, mln_lat_lng coordinate, mln_screen_point* out_point
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_pixel_for_lat_lng(map, coordinate, out_point);
  });
}

auto mln_map_lat_lng_for_pixel(
  mln_map* map, mln_screen_point point, mln_lat_lng* out_coordinate
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_lat_lng_for_pixel(map, point, out_coordinate);
  });
}

auto mln_map_pixels_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  mln_screen_point* out_points
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_pixels_for_lat_lngs(
      map, coordinates, coordinate_count, out_points
    );
  });
}

auto mln_map_lat_lngs_for_pixels(
  mln_map* map, const mln_screen_point* points, size_t point_count,
  mln_lat_lng* out_coordinates
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_lat_lngs_for_pixels(
      map, points, point_count, out_coordinates
    );
  });
}

auto mln_map_projection_create(
  mln_map* map, mln_map_projection** out_projection
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_create(map, out_projection);
  });
}

auto mln_map_projection_destroy(mln_map_projection* projection) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_destroy(projection);
  });
}

auto mln_map_projection_get_camera(
  mln_map_projection* projection, mln_camera_options* out_camera
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_get_camera(projection, out_camera);
  });
}

auto mln_map_projection_set_camera(
  mln_map_projection* projection, const mln_camera_options* camera
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_set_camera(projection, camera);
  });
}

auto mln_map_projection_set_visible_coordinates(
  mln_map_projection* projection, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_edge_insets padding
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_set_visible_coordinates(
      projection, coordinates, coordinate_count, padding
    );
  });
}

auto mln_map_projection_set_visible_geometry(
  mln_map_projection* projection, const mln_geometry* geometry,
  mln_edge_insets padding
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_set_visible_geometry(
      projection, geometry, padding
    );
  });
}

auto mln_map_projection_pixel_for_lat_lng(
  mln_map_projection* projection, mln_lat_lng coordinate,
  mln_screen_point* out_point
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_pixel_for_lat_lng(
      projection, coordinate, out_point
    );
  });
}

auto mln_map_projection_lat_lng_for_pixel(
  mln_map_projection* projection, mln_screen_point point,
  mln_lat_lng* out_coordinate
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_projection_lat_lng_for_pixel(
      projection, point, out_coordinate
    );
  });
}

auto mln_projected_meters_for_lat_lng(
  mln_lat_lng coordinate, mln_projected_meters* out_meters
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::projected_meters_for_lat_lng(coordinate, out_meters);
  });
}

auto mln_lat_lng_for_projected_meters(
  mln_projected_meters meters, mln_lat_lng* out_coordinate
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::lat_lng_for_projected_meters(meters, out_coordinate);
  });
}

auto mln_map_move_by(mln_map* map, double delta_x, double delta_y) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_move_by(map, delta_x, delta_y);
  });
}

auto mln_map_move_by_animated(
  mln_map* map, double delta_x, double delta_y,
  const mln_animation_options* animation
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_move_by_animated(map, delta_x, delta_y, animation);
  });
}

auto mln_map_scale_by(
  mln_map* map, double scale, const mln_screen_point* anchor
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_scale_by(map, scale, anchor);
  });
}

auto mln_map_scale_by_animated(
  mln_map* map, double scale, const mln_screen_point* anchor,
  const mln_animation_options* animation
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_scale_by_animated(map, scale, anchor, animation);
  });
}

auto mln_map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_rotate_by(map, first, second);
  });
}

auto mln_map_rotate_by_animated(
  mln_map* map, mln_screen_point first, mln_screen_point second,
  const mln_animation_options* animation
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_rotate_by_animated(map, first, second, animation);
  });
}

auto mln_map_pitch_by(mln_map* map, double pitch) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_pitch_by(map, pitch);
  });
}

auto mln_map_pitch_by_animated(
  mln_map* map, double pitch, const mln_animation_options* animation
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_pitch_by_animated(map, pitch, animation);
  });
}

auto mln_map_cancel_transitions(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_cancel_transitions(map);
  });
}

auto mln_map_camera_for_lat_lng_bounds(
  mln_map* map, mln_lat_lng_bounds bounds,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_camera_for_lat_lng_bounds(
      map, bounds, fit_options, out_camera
    );
  });
}

auto mln_map_camera_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_camera_for_lat_lngs(
      map, coordinates, coordinate_count, fit_options, out_camera
    );
  });
}

auto mln_map_camera_for_geometry(
  mln_map* map, const mln_geometry* geometry,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_camera_for_geometry(
      map, geometry, fit_options, out_camera
    );
  });
}

auto mln_map_lat_lng_bounds_for_camera(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_lat_lng_bounds_for_camera(map, camera, out_bounds);
  });
}

auto mln_map_lat_lng_bounds_for_camera_unwrapped(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_lat_lng_bounds_for_camera_unwrapped(
      map, camera, out_bounds
    );
  });
}

auto mln_map_get_bounds(mln_map* map, mln_bound_options* out_options) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_bounds(map, out_options);
  });
}

auto mln_map_set_bounds(mln_map* map, const mln_bound_options* options) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_bounds(map, options);
  });
}

auto mln_map_get_free_camera_options(
  mln_map* map, mln_free_camera_options* out_options
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_get_free_camera_options(map, out_options);
  });
}

auto mln_map_set_free_camera_options(
  mln_map* map, const mln_free_camera_options* options
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_set_free_camera_options(map, options);
  });
}
