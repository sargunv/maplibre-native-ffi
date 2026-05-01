#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/util/size.hpp>

#include "render/surface_session.hpp"

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c.h"

namespace {

using SurfaceRegistry = std::unordered_map<
  mln_surface_session*, std::unique_ptr<mln_surface_session>>;

auto surface_registry_mutex() -> std::mutex& {
  static auto mutex = std::mutex{};
  return mutex;
}

auto surface_registry() -> SurfaceRegistry& {
  static auto registry = SurfaceRegistry{};
  return registry;
}

}  // namespace

namespace mln::core {

auto metal_surface_descriptor_default() noexcept
  -> mln_metal_surface_descriptor {
  return mln_metal_surface_descriptor{
    .size = sizeof(mln_metal_surface_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .layer = nullptr,
    .device = nullptr
  };
}

auto vulkan_surface_descriptor_default() noexcept
  -> mln_vulkan_surface_descriptor {
  return mln_vulkan_surface_descriptor{
    .size = sizeof(mln_vulkan_surface_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .instance = nullptr,
    .physical_device = nullptr,
    .device = nullptr,
    .graphics_queue = nullptr,
    .graphics_queue_family_index = 0,
    .surface = nullptr
  };
}

auto validate_surface_attach_output(mln_surface_session** out_surface)
  -> mln_status {
  if (out_surface == nullptr) {
    set_thread_error("out_surface must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_surface != nullptr) {
    set_thread_error("out_surface must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_surface(mln_surface_session* surface) -> mln_status {
  if (surface == nullptr) {
    set_thread_error("surface session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(surface_registry_mutex());
  if (!surface_registry().contains(surface)) {
    set_thread_error("surface session is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (surface->owner_thread != std::this_thread::get_id()) {
    set_thread_error("surface session call must be made on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }
  return MLN_STATUS_OK;
}

auto validate_live_attached_surface(mln_surface_session* surface)
  -> mln_status {
  const auto status = validate_surface(surface);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!surface->attached || surface->backend == nullptr) {
    set_thread_error("surface session is detached");
    return MLN_STATUS_INVALID_STATE;
  }
  return MLN_STATUS_OK;
}

auto surface_physical_dimension(uint32_t logical, double scale_factor)
  -> uint32_t {
  return static_cast<uint32_t>(std::ceil(logical * scale_factor));
}

auto validate_surface_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status {
  constexpr auto max_dimension =
    static_cast<double>(std::numeric_limits<uint32_t>::max());
  if (
    std::ceil(width * scale_factor) > max_dimension ||
    std::ceil(height * scale_factor) > max_dimension
  ) {
    set_thread_error("scaled surface dimensions are too large");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto surface_attach_session(
  std::unique_ptr<mln_surface_session> session,
  mln_surface_session** out_surface
) -> mln_status {
  if (session == nullptr) {
    set_thread_error("surface session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto output_status = validate_surface_attach_output(out_surface);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  auto* map = session->map;
  auto* handle = session.get();
  const auto attach_status = map_attach_render_target_session(map, handle);
  if (attach_status != MLN_STATUS_OK) {
    return attach_status;
  }
  try {
    if (auto* native_map = map_native(map); native_map != nullptr) {
      native_map->setSize(mbgl::Size{session->width, session->height});
    }
    const std::scoped_lock lock(surface_registry_mutex());
    surface_registry().emplace(handle, std::move(session));
  } catch (...) {
    static_cast<void>(map_detach_render_target_session(map, handle));
    throw;
  }

  *out_surface = handle;
  return MLN_STATUS_OK;
}

auto surface_resize(
  mln_surface_session* surface, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status {
  const auto status = validate_live_attached_surface(surface);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    width == 0 || height == 0 || !std::isfinite(scale_factor) ||
    scale_factor <= 0.0
  ) {
    set_thread_error("surface dimensions and scale_factor must be positive");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto physical_status =
    validate_surface_physical_size(width, height, scale_factor);
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }

  surface->width = width;
  surface->height = height;
  surface->physical_width = surface_physical_dimension(width, scale_factor);
  surface->physical_height = surface_physical_dimension(height, scale_factor);
  surface->scale_factor = scale_factor;
  if (surface->resize_backend != nullptr) {
    surface->resize_backend(surface);
  }
  if (auto* map = map_native(surface->map); map != nullptr) {
    map->setSize(mbgl::Size{width, height});
  }
  surface->renderer.reset();
  surface->rendered_generation = 0;
  ++surface->generation;
  return MLN_STATUS_OK;
}

auto surface_render_update(mln_surface_session* surface) -> mln_status {
  const auto status = validate_live_attached_surface(surface);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  auto update = map_latest_update(surface->map);
  if (!update) {
    set_thread_error("no map render update is available");
    return MLN_STATUS_INVALID_STATE;
  }

  auto& renderer_backend = *surface->backend;
  auto guard = mbgl::gfx::BackendScope{
    renderer_backend, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  map_run_render_jobs(surface->map);
  if (surface->renderer == nullptr) {
    surface->renderer = std::make_unique<mbgl::Renderer>(
      renderer_backend, static_cast<float>(surface->scale_factor)
    );
    surface->renderer->setObserver(map_renderer_observer(surface->map));
  }

  surface->renderer->render(update);
  surface->rendered_generation = surface->generation;
  return MLN_STATUS_OK;
}

auto surface_detach(mln_surface_session* surface) -> mln_status {
  const auto status = validate_live_attached_surface(surface);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  const auto detach_status =
    map_detach_render_target_session(surface->map, surface);
  if (detach_status != MLN_STATUS_OK) {
    return detach_status;
  }
  surface->renderer.reset();
  surface->backend.reset();
  surface->attached = false;
  surface->rendered_generation = 0;
  ++surface->generation;
  return MLN_STATUS_OK;
}

auto surface_destroy(mln_surface_session* surface) -> mln_status {
  const auto status = validate_surface(surface);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (surface->attached) {
    const auto detach_status = surface_detach(surface);
    if (detach_status != MLN_STATUS_OK) {
      return detach_status;
    }
  }

  auto owned_surface = std::unique_ptr<mln_surface_session>{};
  {
    const std::scoped_lock lock(surface_registry_mutex());
    auto found = surface_registry().find(surface);
    owned_surface = std::move(found->second);
    surface_registry().erase(found);
  }
  owned_surface.reset();
  return MLN_STATUS_OK;
}

}  // namespace mln::core
