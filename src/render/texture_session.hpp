#pragma once

#include <cstdint>

#include "maplibre_native_c.h"

namespace mln::core {

auto metal_texture_descriptor_default() noexcept
  -> mln_metal_texture_descriptor;
auto vulkan_texture_descriptor_default() noexcept
  -> mln_vulkan_texture_descriptor;
auto metal_texture_attach(
  mln_map* map, const mln_metal_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status;
auto vulkan_texture_attach(
  mln_map* map, const mln_vulkan_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status;
auto texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status;
auto texture_render_update(mln_texture_session* texture) -> mln_status;
auto metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) -> mln_status;
auto metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) -> mln_status;
auto vulkan_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_texture_frame* out_frame
) -> mln_status;
auto vulkan_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_texture_frame* frame
) -> mln_status;
auto texture_detach(mln_texture_session* texture) -> mln_status;
auto texture_destroy(mln_texture_session* texture) -> mln_status;

}  // namespace mln::core
