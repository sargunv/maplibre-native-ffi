#pragma once

#include <cstdint>
#include <memory>

#include "maplibre_native_c.h"

struct mln_render_session;

namespace mln::core {

auto metal_surface_descriptor_default() noexcept
  -> mln_metal_surface_descriptor;
auto vulkan_surface_descriptor_default() noexcept
  -> mln_vulkan_surface_descriptor;
auto validate_surface_attach_output(mln_render_session** out_surface)
  -> mln_status;
auto validate_surface(mln_render_session* surface) -> mln_status;
auto validate_live_attached_surface(mln_render_session* surface) -> mln_status;
auto surface_physical_dimension(uint32_t logical, double scale_factor)
  -> uint32_t;
auto validate_surface_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status;
auto surface_attach_session(
  std::unique_ptr<mln_render_session> session, mln_render_session** out_surface
) -> mln_status;
auto metal_surface_attach(
  mln_map* map, const mln_metal_surface_descriptor* descriptor,
  mln_render_session** out_surface
) -> mln_status;
auto vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_render_session** out_surface
) -> mln_status;

}  // namespace mln::core
