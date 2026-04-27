#pragma once

#include "maplibre_native_abi.h"

namespace mln::core {

auto map_options_default() noexcept -> mln_map_options;
auto camera_options_default() noexcept -> mln_camera_options;
auto create_map(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) -> mln_status;
auto destroy_map(mln_map* map) -> mln_status;
auto map_set_style_url(mln_map* map, const char* url) -> mln_status;
auto map_set_style_json(mln_map* map, const char* json) -> mln_status;
auto map_get_camera(mln_map* map, mln_camera_options* out_camera) -> mln_status;
auto map_jump_to(mln_map* map, const mln_camera_options* camera) -> mln_status;
auto map_move_by(mln_map* map, double delta_x, double delta_y) -> mln_status;
auto map_scale_by(mln_map* map, double scale, const mln_screen_point* anchor)
  -> mln_status;
auto map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) -> mln_status;
auto map_pitch_by(mln_map* map, double pitch) -> mln_status;
auto map_cancel_transitions(mln_map* map) -> mln_status;
auto map_poll_event(mln_map* map, mln_map_event* out_event) -> mln_status;

}  // namespace mln::core
