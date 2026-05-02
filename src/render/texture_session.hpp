#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "maplibre_native_c.h"

struct mln_render_session;

namespace mln::core {

auto owned_texture_descriptor_default() noexcept
  -> mln_owned_texture_descriptor;
auto metal_owned_texture_descriptor_default() noexcept
  -> mln_metal_owned_texture_descriptor;
auto metal_borrowed_texture_descriptor_default() noexcept
  -> mln_metal_borrowed_texture_descriptor;
auto vulkan_owned_texture_descriptor_default() noexcept
  -> mln_vulkan_owned_texture_descriptor;
auto vulkan_borrowed_texture_descriptor_default() noexcept
  -> mln_vulkan_borrowed_texture_descriptor;
auto texture_image_info_default() noexcept -> mln_texture_image_info;
auto validate_attach_output(mln_render_session** out_texture) -> mln_status;
auto validate_texture(mln_render_session* texture) -> mln_status;
auto validate_live_attached_texture(mln_render_session* texture) -> mln_status;
auto physical_dimension(uint32_t logical, double scale_factor) -> uint32_t;
auto validate_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status;
auto texture_attach_session(
  std::unique_ptr<mln_render_session> session, mln_render_session** out_texture
) -> mln_status;
auto owned_texture_attach(
  mln_map* map, const mln_owned_texture_descriptor* descriptor,
  mln_render_session** out_texture
) -> mln_status;
auto metal_owned_texture_attach(
  mln_map* map, const mln_metal_owned_texture_descriptor* descriptor,
  mln_render_session** out_texture
) -> mln_status;
auto metal_borrowed_texture_attach(
  mln_map* map, const mln_metal_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_texture
) -> mln_status;
auto vulkan_owned_texture_attach(
  mln_map* map, const mln_vulkan_owned_texture_descriptor* descriptor,
  mln_render_session** out_texture
) -> mln_status;
auto vulkan_borrowed_texture_attach(
  mln_map* map, const mln_vulkan_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_texture
) -> mln_status;
auto texture_read_premultiplied_rgba8(
  mln_render_session* texture, uint8_t* out_data, size_t out_data_capacity,
  mln_texture_image_info* out_info
) -> mln_status;
auto metal_owned_texture_acquire_frame(
  mln_render_session* texture, mln_metal_owned_texture_frame* out_frame
) -> mln_status;
auto metal_owned_texture_release_frame(
  mln_render_session* texture, const mln_metal_owned_texture_frame* frame
) -> mln_status;
auto vulkan_owned_texture_acquire_frame(
  mln_render_session* texture, mln_vulkan_owned_texture_frame* out_frame
) -> mln_status;
auto vulkan_owned_texture_release_frame(
  mln_render_session* texture, const mln_vulkan_owned_texture_frame* frame
) -> mln_status;

}  // namespace mln::core
