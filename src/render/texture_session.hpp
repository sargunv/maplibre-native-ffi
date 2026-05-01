#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/renderer/renderer.hpp>

#include "maplibre_native_c.h"

struct mln_texture_session;

namespace mln::core {

enum class TextureSessionBackend : uint8_t { Owned, Metal, Vulkan };
enum class TextureSessionFrameKind : uint8_t { None, Metal, Vulkan, Shared };

using TextureSessionPrepareCallback = void (*)(mln_texture_session*);
using TextureSessionAfterRenderCallback = mln_status (*)(mln_texture_session*);

}  // namespace mln::core

struct mln_texture_session {
  mln_map* map = nullptr;
  std::thread::id owner_thread;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t physical_width = 0;
  uint32_t physical_height = 0;
  double scale_factor = 1.0;
  uint64_t generation = 1;
  uint64_t next_frame_id = 1;
  uint64_t acquired_frame_id = 0;
  uint64_t rendered_generation = 0;
  bool attached = true;
  bool acquired = false;
  mln::core::TextureSessionFrameKind acquired_frame_kind =
    mln::core::TextureSessionFrameKind::None;
  uint32_t shared_required_handle_type = MLN_SHARED_TEXTURE_HANDLE_NONE;
  mln::core::TextureSessionBackend backend_kind =
    mln::core::TextureSessionBackend::Owned;
  std::unique_ptr<mbgl::gfx::HeadlessBackend> backend = nullptr;
  std::unique_ptr<mbgl::Renderer> renderer = nullptr;
  void* rendered_native_texture = nullptr;
  void* acquired_native_texture = nullptr;
  mln::core::TextureSessionPrepareCallback prepare_render_resources = nullptr;
  mln::core::TextureSessionAfterRenderCallback after_render = nullptr;
};

namespace mln::core {

auto owned_texture_descriptor_default() noexcept
  -> mln_owned_texture_descriptor;
auto shared_texture_descriptor_default() noexcept
  -> mln_shared_texture_descriptor;
auto metal_texture_descriptor_default() noexcept
  -> mln_metal_texture_descriptor;
auto vulkan_texture_descriptor_default() noexcept
  -> mln_vulkan_texture_descriptor;
auto texture_image_info_default() noexcept -> mln_texture_image_info;
auto validate_attach_output(mln_texture_session** out_texture) -> mln_status;
auto validate_shared_texture_descriptor(
  const mln_shared_texture_descriptor* descriptor
) -> mln_status;
auto validate_texture(mln_texture_session* texture) -> mln_status;
auto validate_live_attached_texture(mln_texture_session* texture) -> mln_status;
auto validate_shared_frame_output(mln_shared_texture_frame* out_frame)
  -> mln_status;
auto physical_dimension(uint32_t logical, double scale_factor) -> uint32_t;
auto validate_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status;
auto texture_attach_session(
  std::unique_ptr<mln_texture_session> session,
  mln_texture_session** out_texture
) -> mln_status;
auto owned_texture_attach(
  mln_map* map, const mln_owned_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status;
auto metal_texture_attach(
  mln_map* map, const mln_metal_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status;
auto vulkan_texture_attach(
  mln_map* map, const mln_vulkan_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status;
auto shared_texture_attach(
  mln_map* map, const mln_shared_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status;
auto texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status;
auto texture_render_update(mln_texture_session* texture) -> mln_status;
auto texture_read_premultiplied_rgba8(
  mln_texture_session* texture, uint8_t* out_data, size_t out_data_capacity,
  mln_texture_image_info* out_info
) -> mln_status;
auto metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) -> mln_status;
auto metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) -> mln_status;
auto vulkan_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_texture_frame* out_frame
) -> mln_status;
auto texture_acquire_shared_frame(
  mln_texture_session* texture, mln_shared_texture_frame* out_frame
) -> mln_status;
auto vulkan_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_texture_frame* frame
) -> mln_status;
auto texture_release_shared_frame(
  mln_texture_session* texture, const mln_shared_texture_frame* frame
) -> mln_status;
auto texture_detach(mln_texture_session* texture) -> mln_status;
auto texture_destroy(mln_texture_session* texture) -> mln_status;

}  // namespace mln::core
