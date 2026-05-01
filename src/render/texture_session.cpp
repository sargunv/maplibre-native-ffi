#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/util/size.hpp>

#include "render/texture_session.hpp"

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c.h"

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

}  // namespace

namespace mln::core {

auto validate_attach_output(mln_texture_session** out_texture) -> mln_status {
  if (out_texture == nullptr) {
    set_thread_error("out_texture must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_texture != nullptr) {
    set_thread_error("out_texture must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_texture(mln_texture_session* texture) -> mln_status {
  if (texture == nullptr) {
    set_thread_error("texture session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(texture_registry_mutex());
  if (!texture_registry().contains(texture)) {
    set_thread_error("texture session is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->owner_thread != std::this_thread::get_id()) {
    set_thread_error("texture session call must be made on its owner thread");
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
    set_thread_error("texture session is detached");
    return MLN_STATUS_INVALID_STATE;
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
  if (
    std::ceil(width * scale_factor) > max_dimension ||
    std::ceil(height * scale_factor) > max_dimension
  ) {
    set_thread_error("scaled texture dimensions are too large");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto texture_attach_session(
  std::unique_ptr<mln_texture_session> session,
  mln_texture_session** out_texture
) -> mln_status {
  if (session == nullptr) {
    set_thread_error("texture session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  auto* map = session->map;
  auto* handle = session.get();
  const auto attach_status = map_attach_texture_session(map, handle);
  if (attach_status != MLN_STATUS_OK) {
    return attach_status;
  }
  try {
    if (auto* native_map = map_native(map); native_map != nullptr) {
      native_map->setSize(mbgl::Size{session->width, session->height});
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
  texture->rendered_native_texture = nullptr;
  texture->acquired_native_texture = nullptr;
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

}  // namespace mln::core
