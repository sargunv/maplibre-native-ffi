#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/gfx/renderable.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/util/image.hpp>
#include <mbgl/util/size.hpp>

#include "render/texture_session.hpp"

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c.h"
#include "render/render_session_common.hpp"

namespace {

using TextureRegistry = mln::core::RenderSessionRegistry<mln_texture_session>;

auto texture_registry()
  -> mln::core::RenderSessionRegistry<mln_texture_session>& {
  static auto registry = TextureRegistry{};
  return registry;
}

auto validate_owned_descriptor(const mln_owned_texture_descriptor* descriptor)
  -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_owned_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_owned_texture_descriptor.size is too small"
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
  return MLN_STATUS_OK;
}

auto is_known_shared_export_type(uint32_t export_type) -> bool {
  return export_type == MLN_SHARED_TEXTURE_EXPORT_NONE ||
         export_type == MLN_SHARED_TEXTURE_EXPORT_DMA_BUF ||
         export_type == MLN_SHARED_TEXTURE_EXPORT_IOSURFACE ||
         export_type == MLN_SHARED_TEXTURE_EXPORT_D3D_SHARED_HANDLE;
}

}  // namespace

namespace mln::core {

auto owned_texture_descriptor_default() noexcept
  -> mln_owned_texture_descriptor {
  return mln_owned_texture_descriptor{
    .size = sizeof(mln_owned_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0
  };
}

auto shared_texture_descriptor_default() noexcept
  -> mln_shared_texture_descriptor {
  return mln_shared_texture_descriptor{
    .size = sizeof(mln_shared_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .required_export_type = MLN_SHARED_TEXTURE_EXPORT_NONE,
    .device = nullptr,
    .instance = nullptr,
    .physical_device = nullptr,
    .graphics_queue = nullptr,
    .graphics_queue_family_index = 0
  };
}

auto texture_image_info_default() noexcept -> mln_texture_image_info {
  return mln_texture_image_info{
    .size = sizeof(mln_texture_image_info),
    .width = 0,
    .height = 0,
    .stride = 0,
    .byte_length = 0
  };
}

auto validate_attach_output(mln_texture_session** out_texture) -> mln_status {
  return validate_render_session_attach_output(
    out_texture, "out_texture must not be null",
    "out_texture must point to a null handle"
  );
}

auto validate_shared_texture_descriptor(
  const mln_shared_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_shared_texture_descriptor)) {
    set_thread_error("mln_shared_texture_descriptor.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    set_thread_error("texture dimensions and scale_factor must be positive");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!is_known_shared_export_type(descriptor->required_export_type)) {
    set_thread_error("shared texture export type is invalid");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_texture(mln_texture_session* texture) -> mln_status {
  return texture_registry().validate(
    texture, "texture session must not be null",
    "texture session is not a live handle",
    "texture session call must be made on its owner thread"
  );
}

auto validate_live_attached_texture(mln_texture_session* texture)
  -> mln_status {
  return validate_live_attached_render_session(
    texture, validate_texture, "texture session is detached"
  );
}

auto validate_shared_frame_output(mln_shared_texture_frame* out_frame)
  -> mln_status {
  if (
    out_frame == nullptr || out_frame->size < sizeof(mln_shared_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto physical_dimension(uint32_t logical, double scale_factor) -> uint32_t {
  return render_session_physical_dimension(logical, scale_factor);
}

auto validate_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status {
  return validate_render_session_physical_size(
    width, height, scale_factor, "scaled texture dimensions are too large"
  );
}

auto texture_attach_session(
  std::unique_ptr<mln_texture_session> session,
  mln_texture_session** out_texture
) -> mln_status {
  return attach_render_session(
    std::move(session), out_texture, texture_registry(),
    RenderSessionAttachMessages{
      .null_session = "texture session must not be null",
      .null_output = "out_texture must not be null",
      .non_null_output = "out_texture must point to a null handle"
    }
  );
}

auto owned_texture_attach(
  mln_map* map, const mln_owned_texture_descriptor* descriptor,
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
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
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
  session->backend_kind = TextureSessionBackend::Owned;
  session->mode = TextureSessionMode::Owned;
  session->backend = mbgl::gfx::HeadlessBackend::Create(
    mbgl::Size{session->physical_width, session->physical_height},
    mbgl::gfx::Renderable::SwapBehaviour::Flush, mbgl::gfx::ContextMode::Unique
  );
  return texture_attach_session(std::move(session), out_texture);
}

auto texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    width == 0 || height == 0 || !std::isfinite(scale_factor) ||
    scale_factor <= 0.0
  ) {
    set_thread_error("texture dimensions and scale_factor must be positive");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->acquired) {
    set_thread_error("cannot resize while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  const auto physical_status =
    validate_physical_size(width, height, scale_factor);
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  const auto physical_width = physical_dimension(width, scale_factor);
  const auto physical_height = physical_dimension(height, scale_factor);

  texture->backend->setSize(mbgl::Size{physical_width, physical_height});
  if (auto* map = map_native(texture->map); map != nullptr) {
    map->setSize(mbgl::Size{width, height});
  }
  texture->renderer.reset();
  texture->rendered_native_texture = nullptr;
  texture->acquired_native_texture = nullptr;
  texture->rendered_generation = 0;
  texture->acquired_frame_kind = TextureSessionFrameKind::None;
  texture->width = width;
  texture->height = height;
  texture->physical_width = physical_width;
  texture->physical_height = physical_height;
  texture->scale_factor = scale_factor;
  ++texture->generation;
  return MLN_STATUS_OK;
}

auto texture_render_update(mln_texture_session* texture) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->acquired) {
    set_thread_error("cannot render while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }

  auto update = map_latest_update(texture->map);
  if (!update) {
    set_thread_error("no map render update is available");
    return MLN_STATUS_INVALID_STATE;
  }

  if (texture->prepare_render_resources != nullptr) {
    texture->prepare_render_resources(texture);
  }
  auto* renderer_backend = texture->backend->getRendererBackend();
  if (renderer_backend == nullptr) {
    set_thread_error("texture session renderer backend is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  auto guard = mbgl::gfx::BackendScope{
    *renderer_backend, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  map_run_render_jobs(texture->map);
  if (texture->renderer == nullptr) {
    texture->renderer = std::make_unique<mbgl::Renderer>(
      *renderer_backend, static_cast<float>(texture->scale_factor)
    );
    texture->renderer->setObserver(map_renderer_observer(texture->map));
  }

  texture->renderer->render(update);
  if (texture->after_render != nullptr) {
    const auto after_status = texture->after_render(texture);
    if (after_status != MLN_STATUS_OK) {
      return after_status;
    }
  }
  texture->rendered_generation = texture->generation;
  return MLN_STATUS_OK;
}

auto texture_read_premultiplied_rgba8(
  mln_texture_session* texture, uint8_t* out_data, size_t out_data_capacity,
  mln_texture_image_info* out_info
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_info == nullptr || out_info->size < sizeof(mln_texture_image_info)) {
    set_thread_error("out_info must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->acquired) {
    set_thread_error("cannot read while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->mode != TextureSessionMode::Owned) {
    set_thread_error("texture session does not support CPU readback");
    return MLN_STATUS_UNSUPPORTED;
  }
  if (texture->rendered_generation != texture->generation) {
    set_thread_error("no rendered frame is available for this generation");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->physical_width > std::numeric_limits<uint32_t>::max() / 4) {
    set_thread_error("texture readback stride is too large");
    return MLN_STATUS_INVALID_STATE;
  }

  const auto stride = texture->physical_width * 4;
  if (
    texture->physical_height != 0 &&
    stride > std::numeric_limits<size_t>::max() / texture->physical_height
  ) {
    set_thread_error("texture readback byte length is too large");
    return MLN_STATUS_INVALID_STATE;
  }
  const auto byte_length =
    static_cast<size_t>(stride) * texture->physical_height;

  *out_info = mln_texture_image_info{
    .size = sizeof(mln_texture_image_info),
    .width = texture->physical_width,
    .height = texture->physical_height,
    .stride = stride,
    .byte_length = byte_length
  };

  if (out_data == nullptr || out_data_capacity < byte_length) {
    set_thread_error("out_data capacity is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* renderer_backend = texture->backend->getRendererBackend();
  if (renderer_backend == nullptr) {
    set_thread_error("texture session renderer backend is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  auto guard = mbgl::gfx::BackendScope{
    *renderer_backend, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  auto image = texture->backend->readStillImage();
  if (!image.valid()) {
    set_thread_error("texture readback did not produce an image");
    return MLN_STATUS_INVALID_STATE;
  }
  if (
    image.size.width != texture->physical_width ||
    image.size.height != texture->physical_height || image.stride() != stride ||
    image.bytes() != byte_length
  ) {
    set_thread_error("texture readback image layout did not match the session");
    return MLN_STATUS_INVALID_STATE;
  }

  std::memcpy(out_data, image.data.get(), image.bytes());
  return MLN_STATUS_OK;
}

auto texture_detach(mln_texture_session* texture) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->acquired) {
    set_thread_error("cannot detach while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }

  const auto detach_status =
    map_detach_render_target_session(texture->map, texture);
  if (detach_status != MLN_STATUS_OK) {
    return detach_status;
  }
  texture->renderer.reset();
  texture->rendered_native_texture = nullptr;
  texture->acquired_native_texture = nullptr;
  texture->backend.reset();
  texture->attached = false;
  texture->rendered_generation = 0;
  texture->acquired_frame_kind = TextureSessionFrameKind::None;
  ++texture->generation;
  return MLN_STATUS_OK;
}

auto texture_destroy(mln_texture_session* texture) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->acquired) {
    set_thread_error("cannot destroy while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->attached) {
    const auto detach_status = texture_detach(texture);
    if (detach_status != MLN_STATUS_OK) {
      return detach_status;
    }
  }

  auto owned_texture = texture_registry().erase(texture);
  owned_texture.reset();
  return MLN_STATUS_OK;
}

}  // namespace mln::core
