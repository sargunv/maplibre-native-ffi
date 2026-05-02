#include <cmath>
#include <memory>

#include <mbgl/util/size.hpp>

#include <Metal/MTLDevice.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLTexture.hpp>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "render/metal/metal_texture_backend.inc"
#include "render/texture_session.hpp"

namespace {
auto validate_owned_descriptor(
  const mln_metal_owned_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_owned_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_metal_owned_texture_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "texture dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->device == nullptr) {
    mln::core::set_thread_error("Metal device must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_borrowed_descriptor(
  const mln_metal_borrowed_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_borrowed_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_metal_borrowed_texture_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "texture dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto* metal_texture = static_cast<MTL::Texture*>(descriptor->texture);
  if (metal_texture == nullptr) {
    mln::core::set_thread_error("Metal texture must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto physical_status = mln::core::validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  const auto physical_width =
    mln::core::physical_dimension(descriptor->width, descriptor->scale_factor);
  const auto physical_height =
    mln::core::physical_dimension(descriptor->height, descriptor->scale_factor);
  if (
    metal_texture->width() != physical_width ||
    metal_texture->height() != physical_height
  ) {
    mln::core::set_thread_error(
      "Metal texture dimensions must match descriptor physical size"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((metal_texture->usage() & MTL::TextureUsageRenderTarget) == 0) {
    mln::core::set_thread_error("Metal texture must allow render target usage");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_vulkan_owned_descriptor(
  const mln_vulkan_owned_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_vulkan_owned_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_vulkan_owned_texture_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "texture dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->instance == nullptr || descriptor->physical_device == nullptr ||
    descriptor->device == nullptr || descriptor->graphics_queue == nullptr
  ) {
    mln::core::set_thread_error("Vulkan handles must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_vulkan_borrowed_descriptor(
  const mln_vulkan_borrowed_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_vulkan_borrowed_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_vulkan_borrowed_texture_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "texture dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->instance == nullptr || descriptor->physical_device == nullptr ||
    descriptor->device == nullptr || descriptor->graphics_queue == nullptr ||
    descriptor->image == nullptr || descriptor->image_view == nullptr
  ) {
    mln::core::set_thread_error("Vulkan handles must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->format == 0 || descriptor->final_layout == 0) {
    mln::core::set_thread_error(
      "Vulkan format and final_layout must be specified"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto finish_metal_render(mln_texture_session* texture) -> mln_status {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  auto& backend =
    static_cast<mln::core::MetalTextureBackend&>(*texture->backend);
  auto* rendered_texture = backend.metal_texture();
  if (rendered_texture == nullptr) {
    mln::core::set_thread_error(
      "render update did not produce a Metal texture"
    );
    return MLN_STATUS_INVALID_STATE;
  }
  texture->rendered_native_texture = rendered_texture;
  return MLN_STATUS_OK;
}

auto fill_frame(
  mln_texture_session* texture, mln_metal_owned_texture_frame* out_frame
) -> mln_status {
  auto* metal_texture =
    static_cast<MTL::Texture*>(texture->rendered_native_texture);
  if (metal_texture == nullptr) {
    mln::core::set_thread_error("rendered Metal texture is not available");
    return MLN_STATUS_INVALID_STATE;
  }

  *out_frame = mln_metal_owned_texture_frame{
    .size = sizeof(mln_metal_owned_texture_frame),
    .generation = texture->generation,
    .width = texture->physical_width,
    .height = texture->physical_height,
    .scale_factor = texture->scale_factor,
    .frame_id = texture->next_frame_id,
    .texture = metal_texture,
    .device = metal_texture->device(),
    .pixel_format = static_cast<uint64_t>(metal_texture->pixelFormat())
  };
  return MLN_STATUS_OK;
}
}  // namespace

namespace mln::core {

auto metal_owned_texture_descriptor_default() noexcept
  -> mln_metal_owned_texture_descriptor {
  return mln_metal_owned_texture_descriptor{
    .size = sizeof(mln_metal_owned_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .device = nullptr
  };
}

auto metal_borrowed_texture_descriptor_default() noexcept
  -> mln_metal_borrowed_texture_descriptor {
  return mln_metal_borrowed_texture_descriptor{
    .size = sizeof(mln_metal_borrowed_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .texture = nullptr
  };
}

auto metal_owned_texture_attach(
  mln_map* map, const mln_metal_owned_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_owned_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  auto session = std::make_unique<mln_texture_session>();
  session->map = map;
  session->owner_thread = map_owner_thread(map);
  session->width = descriptor->width;
  session->height = descriptor->height;
  session->scale_factor = descriptor->scale_factor;
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  session->physical_width =
    physical_dimension(descriptor->width, descriptor->scale_factor);
  session->physical_height =
    physical_dimension(descriptor->height, descriptor->scale_factor);
  session->backend_kind = TextureSessionBackend::Metal;
  session->mode = TextureSessionMode::Owned;
  session->backend = std::make_unique<MetalTextureBackend>(
    static_cast<MTL::Device*>(descriptor->device),
    mbgl::Size{session->physical_width, session->physical_height}
  );
  session->after_render = finish_metal_render;
  return texture_attach_session(std::move(session), out_texture);
}

auto metal_borrowed_texture_attach(
  mln_map* map, const mln_metal_borrowed_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_borrowed_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  auto session = std::make_unique<mln_texture_session>();
  session->map = map;
  session->owner_thread = map_owner_thread(map);
  session->width = descriptor->width;
  session->height = descriptor->height;
  session->scale_factor = descriptor->scale_factor;
  session->physical_width =
    physical_dimension(descriptor->width, descriptor->scale_factor);
  session->physical_height =
    physical_dimension(descriptor->height, descriptor->scale_factor);
  session->backend_kind = TextureSessionBackend::Metal;
  session->mode = TextureSessionMode::Borrowed;
  session->backend = std::make_unique<MetalTextureBackend>(
    static_cast<MTL::Texture*>(descriptor->texture),
    mbgl::Size{session->physical_width, session->physical_height}
  );
  session->after_render = finish_metal_render;
  return texture_attach_session(std::move(session), out_texture);
}

auto metal_owned_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_owned_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_frame == nullptr ||
    out_frame->size < sizeof(mln_metal_owned_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->acquired) {
    set_thread_error("a texture frame is already acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->rendered_generation != texture->generation) {
    set_thread_error("no rendered frame is available for this generation");
    return MLN_STATUS_INVALID_STATE;
  }
  if (
    texture->mode != TextureSessionMode::Owned ||
    texture->backend_kind != TextureSessionBackend::Metal
  ) {
    set_thread_error("texture session cannot expose a Metal texture frame");
    return MLN_STATUS_UNSUPPORTED;
  }

  const auto frame_status = fill_frame(texture, out_frame);
  if (frame_status != MLN_STATUS_OK) {
    return frame_status;
  }
  texture->acquired_native_texture = texture->rendered_native_texture;
  texture->acquired = true;
  texture->acquired_frame_id = out_frame->frame_id;
  texture->acquired_frame_kind = TextureSessionFrameKind::MetalOwned;
  ++texture->next_frame_id;
  return MLN_STATUS_OK;
}

auto metal_owned_texture_release_frame(
  mln_texture_session* texture, const mln_metal_owned_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (frame == nullptr || frame->size < sizeof(mln_metal_owned_texture_frame)) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    !texture->acquired ||
    texture->acquired_frame_kind != TextureSessionFrameKind::MetalOwned
  ) {
    set_thread_error("no texture frame is currently acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (frame->generation != texture->generation) {
    set_thread_error("frame generation does not match acquired frame");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (frame->frame_id != texture->acquired_frame_id) {
    set_thread_error("frame identity does not match acquired frame");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  texture->acquired = false;
  texture->acquired_frame_id = 0;
  texture->acquired_frame_kind = TextureSessionFrameKind::None;
  texture->acquired_native_texture = nullptr;
  return MLN_STATUS_OK;
}

auto vulkan_owned_texture_descriptor_default() noexcept
  -> mln_vulkan_owned_texture_descriptor {
  return mln_vulkan_owned_texture_descriptor{
    .size = sizeof(mln_vulkan_owned_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .instance = nullptr,
    .physical_device = nullptr,
    .device = nullptr,
    .graphics_queue = nullptr,
    .graphics_queue_family_index = 0,
  };
}

auto vulkan_borrowed_texture_descriptor_default() noexcept
  -> mln_vulkan_borrowed_texture_descriptor {
  return mln_vulkan_borrowed_texture_descriptor{
    .size = sizeof(mln_vulkan_borrowed_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .instance = nullptr,
    .physical_device = nullptr,
    .device = nullptr,
    .graphics_queue = nullptr,
    .graphics_queue_family_index = 0,
    .image = nullptr,
    .image_view = nullptr,
    .format = 0,
    .initial_layout = 0,
    .final_layout = 5,
  };
}

auto vulkan_owned_texture_attach(
  mln_map* map, const mln_vulkan_owned_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_vulkan_owned_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto vulkan_borrowed_texture_attach(
  mln_map* map, const mln_vulkan_borrowed_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status =
    validate_vulkan_borrowed_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto vulkan_owned_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_owned_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_frame == nullptr ||
    out_frame->size < sizeof(mln_vulkan_owned_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto vulkan_owned_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_owned_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    frame == nullptr || frame->size < sizeof(mln_vulkan_owned_texture_frame)
  ) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core
