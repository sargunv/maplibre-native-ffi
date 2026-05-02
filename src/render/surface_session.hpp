#pragma once

#include "maplibre_native_c.h"

struct mln_render_session;

namespace mln::core {

auto metal_surface_descriptor_default() noexcept
  -> mln_metal_surface_descriptor;
auto vulkan_surface_descriptor_default() noexcept
  -> mln_vulkan_surface_descriptor;
auto metal_surface_attach(
  mln_map* map, const mln_metal_surface_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status;
auto vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status;

}  // namespace mln::core
