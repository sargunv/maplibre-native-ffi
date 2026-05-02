#include <cmath>
#include <cstdint>
#include <memory>
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
#include "render/render_session_common.hpp"

namespace {

using SurfaceRegistry = mln::core::RenderSessionRegistry<mln_surface_session>;

auto surface_registry()
  -> mln::core::RenderSessionRegistry<mln_surface_session>& {
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
  return validate_render_session_attach_output(
    out_surface, "out_surface must not be null",
    "out_surface must point to a null handle"
  );
}

auto validate_surface(mln_surface_session* surface) -> mln_status {
  return surface_registry().validate(
    surface, "surface session must not be null",
    "surface session is not a live handle",
    "surface session call must be made on its owner thread"
  );
}

auto validate_live_attached_surface(mln_surface_session* surface)
  -> mln_status {
  return validate_live_attached_render_session(
    surface, validate_surface, "surface session is detached"
  );
}

auto surface_physical_dimension(uint32_t logical, double scale_factor)
  -> uint32_t {
  return render_session_physical_dimension(logical, scale_factor);
}

auto validate_surface_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status {
  return validate_render_session_physical_size(
    width, height, scale_factor, "scaled surface dimensions are too large"
  );
}

auto surface_attach_session(
  std::unique_ptr<mln_surface_session> session,
  mln_surface_session** out_surface
) -> mln_status {
  return attach_render_session(
    std::move(session), out_surface, surface_registry(),
    RenderSessionKind::Surface,
    RenderSessionAttachMessages{
      .null_session = "surface session must not be null",
      .null_output = "out_surface must not be null",
      .non_null_output = "out_surface must point to a null handle"
    }
  );
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
  const auto physical_width = surface_physical_dimension(width, scale_factor);
  const auto physical_height = surface_physical_dimension(height, scale_factor);

  if (surface->resize_backend != nullptr) {
    surface->resize_backend(surface, physical_width, physical_height);
  }
  if (auto* map = map_native(surface->map); map != nullptr) {
    map->setSize(mbgl::Size{width, height});
  }
  surface->renderer.reset();
  surface->rendered_generation = 0;
  surface->width = width;
  surface->height = height;
  surface->physical_width = physical_width;
  surface->physical_height = physical_height;
  surface->scale_factor = scale_factor;
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

  auto owned_surface = surface_registry().erase(surface);
  owned_surface.reset();
  return MLN_STATUS_OK;
}

}  // namespace mln::core
