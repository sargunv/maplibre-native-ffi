#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/util/size.hpp>

#include <Metal/MTLDevice.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLTexture.hpp>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "render/metal/metal_texture_backend.inc"
#include "render/texture_session.hpp"

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
  std::unique_ptr<mln::core::MetalTextureBackend> backend;
  std::unique_ptr<mbgl::Renderer> renderer;
  MTL::Texture* rendered_texture = nullptr;
  MTL::Texture* acquired_texture = nullptr;
};

namespace {
using TextureRegistry = std::unordered_map<
  mln_texture_session*, std::unique_ptr<mln_texture_session>>;

auto texture_registry_mutex() -> std::mutex& {
  static auto mutex = std::mutex{};
  return mutex;
}

auto texture_registry() -> TextureRegistry& {
  static auto registry = TextureRegistry{};
  return registry;
}

auto validate_descriptor(const mln_metal_texture_descriptor* descriptor)
  -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_texture_descriptor)) {
    mln::core::set_thread_error("mln_metal_texture_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->width == 0 || descriptor->height == 0 ||
      !std::isfinite(descriptor->scale_factor) ||
      descriptor->scale_factor <= 0.0) {
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

auto physical_dimension(uint32_t logical, double scale_factor) -> uint32_t {
  return static_cast<uint32_t>(std::ceil(logical * scale_factor));
}

auto validate_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status {
  constexpr auto max_dimension =
    static_cast<double>(std::numeric_limits<uint32_t>::max());
  if (std::ceil(width * scale_factor) > max_dimension ||
      std::ceil(height * scale_factor) > max_dimension) {
    mln::core::set_thread_error("scaled texture dimensions are too large");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_texture(mln_texture_session* texture) -> mln_status {
  if (texture == nullptr) {
    mln::core::set_thread_error("texture session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(texture_registry_mutex());
  if (!texture_registry().contains(texture)) {
    mln::core::set_thread_error("texture session is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->owner_thread != std::this_thread::get_id()) {
    mln::core::set_thread_error(
      "texture session call must be made on its owner thread"
    );
    return MLN_STATUS_WRONG_THREAD;
  }
  return MLN_STATUS_OK;
}

auto validate_live_attached_texture(mln_texture_session* texture)
  -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!texture->attached || texture->backend == nullptr) {
    mln::core::set_thread_error("texture session is detached");
    return MLN_STATUS_INVALID_STATE;
  }
  return MLN_STATUS_OK;
}

auto fill_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) -> mln_status {
  auto* metal_texture = texture->rendered_texture;
  if (metal_texture == nullptr) {
    mln::core::set_thread_error("rendered Metal texture is not available");
    return MLN_STATUS_INVALID_STATE;
  }

  *out_frame = mln_metal_texture_frame{
    .size = sizeof(mln_metal_texture_frame),
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

auto metal_texture_descriptor_default() noexcept
  -> mln_metal_texture_descriptor {
  return mln_metal_texture_descriptor{
    .size = sizeof(mln_metal_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .device = nullptr
  };
}

auto metal_texture_attach(
  mln_map* map, const mln_metal_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  if (out_texture == nullptr) {
    set_thread_error("out_texture must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_texture != nullptr) {
    set_thread_error("out_texture must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
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
  session->backend = std::make_unique<MetalTextureBackend>(
    static_cast<MTL::Device*>(descriptor->device),
    mbgl::Size{session->physical_width, session->physical_height}
  );
  auto* handle = session.get();

  const auto attach_status = map_attach_texture_session(map, handle);
  if (attach_status != MLN_STATUS_OK) {
    return attach_status;
  }
  try {
    if (auto* native_map = map_native(map); native_map != nullptr) {
      native_map->setSize(mbgl::Size{descriptor->width, descriptor->height});
    }
    const std::scoped_lock lock(texture_registry_mutex());
    texture_registry().emplace(handle, std::move(session));
  } catch (...) {
    static_cast<void>(map_detach_texture_session(map, handle));
    throw;
  }

  *out_texture = handle;
  return MLN_STATUS_OK;
}

auto texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (width == 0 || height == 0 || !std::isfinite(scale_factor) ||
      scale_factor <= 0.0) {
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

  // Map::setSize receives the logical UI viewport, while the render backend
  // owns the physical pixel target. MapLibre Native's map pixelRatio is fixed
  // at map construction, so runtime scale changes are represented here by
  // resizing the backend texture and recreating the renderer with the new scale
  // factor.
  texture->backend->setSize(mbgl::Size{physical_width, physical_height});
  if (auto* map = map_native(texture->map); map != nullptr) {
    map->setSize(mbgl::Size{width, height});
  }
  texture->renderer.reset();
  texture->rendered_texture = nullptr;
  texture->rendered_generation = 0;
  texture->width = width;
  texture->height = height;
  texture->physical_width = physical_width;
  texture->physical_height = physical_height;
  texture->scale_factor = scale_factor;
  ++texture->generation;
  return MLN_STATUS_OK;
}

auto texture_render(mln_texture_session* texture) -> mln_status {
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

  auto guard = mbgl::gfx::BackendScope{
    *texture->backend, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  map_run_render_jobs(texture->map);
  if (texture->renderer == nullptr) {
    texture->renderer = std::make_unique<mbgl::Renderer>(
      *texture->backend, static_cast<float>(texture->scale_factor)
    );
    texture->renderer->setObserver(map_renderer_observer(texture->map));
  }

  texture->renderer->render(update);
  texture->rendered_texture = texture->backend->metal_texture();
  if (texture->rendered_texture == nullptr) {
    set_thread_error("render did not produce a Metal texture");
    return MLN_STATUS_NATIVE_ERROR;
  }
  texture->rendered_generation = texture->generation;
  return MLN_STATUS_OK;
}

auto metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_frame == nullptr ||
      out_frame->size < sizeof(mln_metal_texture_frame)) {
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

  const auto frame_status = fill_frame(texture, out_frame);
  if (frame_status != MLN_STATUS_OK) {
    return frame_status;
  }
  texture->acquired_texture = texture->rendered_texture;
  texture->acquired = true;
  texture->acquired_frame_id = out_frame->frame_id;
  ++texture->next_frame_id;
  return MLN_STATUS_OK;
}

auto metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (frame == nullptr || frame->size < sizeof(mln_metal_texture_frame)) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!texture->acquired) {
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
  texture->acquired_texture = nullptr;
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

  const auto detach_status = map_detach_texture_session(texture->map, texture);
  if (detach_status != MLN_STATUS_OK) {
    return detach_status;
  }
  texture->renderer.reset();
  texture->rendered_texture = nullptr;
  texture->backend.reset();
  texture->attached = false;
  texture->rendered_generation = 0;
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

  auto owned_texture = std::unique_ptr<mln_texture_session>{};
  {
    const std::scoped_lock lock(texture_registry_mutex());
    auto found = texture_registry().find(texture);
    owned_texture = std::move(found->second);
    texture_registry().erase(found);
  }
  owned_texture.reset();
  return MLN_STATUS_OK;
}

auto vulkan_texture_descriptor_default() noexcept
  -> mln_vulkan_texture_descriptor {
  return mln_vulkan_texture_descriptor{
    .size = sizeof(mln_vulkan_texture_descriptor),
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

auto vulkan_texture_attach(
  mln_map* map, const mln_vulkan_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  (void)map;
  (void)descriptor;
  if (out_texture != nullptr) {
    *out_texture = nullptr;
  }
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto vulkan_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_texture_frame* out_frame
) -> mln_status {
  (void)texture;
  (void)out_frame;
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto vulkan_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_texture_frame* frame
) -> mln_status {
  (void)texture;
  (void)frame;
  set_thread_error("Vulkan texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core
