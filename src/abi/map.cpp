#define MLN_BUILDING_ABI

#include "map/map.hpp"

#include "abi/boundary.hpp"
#include "maplibre_native_abi.h"

auto mln_map_options_default(void) noexcept -> mln_map_options {
  return mln::core::map_options_default();
}

auto mln_camera_options_default(void) noexcept -> mln_camera_options {
  return mln::core::camera_options_default();
}

auto mln_map_create(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::create_map(runtime, options, out_map);
  });
}

auto mln_map_destroy(mln_map* map) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::destroy_map(map);
  });
}

auto mln_map_set_style_url(mln_map* map, const char* url) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_set_style_url(map, url);
  });
}

auto mln_map_set_style_json(mln_map* map, const char* json) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_set_style_json(map, json);
  });
}

auto mln_map_get_camera(mln_map* map, mln_camera_options* out_camera) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_get_camera(map, out_camera);
  });
}

auto mln_map_jump_to(mln_map* map, const mln_camera_options* camera) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_jump_to(map, camera);
  });
}

auto mln_map_move_by(mln_map* map, double delta_x, double delta_y) noexcept
  -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_move_by(map, delta_x, delta_y);
  });
}

auto mln_map_scale_by(
  mln_map* map, double scale, const mln_screen_point* anchor
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_scale_by(map, scale, anchor);
  });
}

auto mln_map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_rotate_by(map, first, second);
  });
}

auto mln_map_pitch_by(mln_map* map, double pitch) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_pitch_by(map, pitch);
  });
}

auto mln_map_cancel_transitions(mln_map* map) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_cancel_transitions(map);
  });
}

auto mln_map_poll_event(
  mln_map* map, mln_map_event* out_event, bool* out_has_event
) noexcept -> mln_status {
  return mln::abi::status_boundary([&]() -> mln_status {
    return mln::core::map_poll_event(map, out_event, out_has_event);
  });
}
