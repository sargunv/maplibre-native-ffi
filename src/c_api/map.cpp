#define MLN_BUILDING_C

#include <cstddef>

#include "map/map.hpp"

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"

auto mln_map_options_default(void) noexcept -> mln_map_options {
  return mln::core::map_options_default();
}

auto mln_camera_options_default(void) noexcept -> mln_camera_options {
  return mln::core::camera_options_default();
}

auto mln_projection_mode_default(void) noexcept -> mln_projection_mode {
  return mln::core::projection_mode_default();
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

auto mln_map_request_render(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_request_render(map);
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

auto mln_map_scale_by(
  mln_map* map, double scale, const mln_screen_point* anchor
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_scale_by(map, scale, anchor);
  });
}

auto mln_map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_rotate_by(map, first, second);
  });
}

auto mln_map_pitch_by(mln_map* map, double pitch) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_pitch_by(map, pitch);
  });
}

auto mln_map_cancel_transitions(mln_map* map) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_cancel_transitions(map);
  });
}

auto mln_map_poll_event(
  mln_map* map, mln_map_event* out_event, bool* out_has_event
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::map_poll_event(map, out_event, out_has_event);
  });
}
