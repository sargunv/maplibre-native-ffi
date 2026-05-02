#pragma once

#include <cstdint>
#include <memory>
#include <thread>

#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/renderer/renderer.hpp>

#include "maplibre_native_c.h"

struct mln_surface_session;

namespace mln::core {

using SurfaceSessionResizeCallback =
  void (*)(mln_surface_session*, uint32_t, uint32_t);

}  // namespace mln::core

struct mln_surface_session {
  mln_map* map = nullptr;
  std::thread::id owner_thread;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t physical_width = 0;
  uint32_t physical_height = 0;
  double scale_factor = 1.0;
  uint64_t generation = 1;
  uint64_t rendered_generation = 0;
  bool attached = true;
  std::unique_ptr<mbgl::gfx::RendererBackend> backend = nullptr;
  std::unique_ptr<mbgl::Renderer> renderer = nullptr;
  mln::core::SurfaceSessionResizeCallback resize_backend = nullptr;
};

namespace mln::core {

auto metal_surface_descriptor_default() noexcept
  -> mln_metal_surface_descriptor;
auto vulkan_surface_descriptor_default() noexcept
  -> mln_vulkan_surface_descriptor;
auto validate_surface_attach_output(mln_surface_session** out_surface)
  -> mln_status;
auto validate_surface(mln_surface_session* surface) -> mln_status;
auto validate_live_attached_surface(mln_surface_session* surface) -> mln_status;
auto surface_physical_dimension(uint32_t logical, double scale_factor)
  -> uint32_t;
auto validate_surface_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status;
auto surface_attach_session(
  std::unique_ptr<mln_surface_session> session,
  mln_surface_session** out_surface
) -> mln_status;
auto metal_surface_attach(
  mln_map* map, const mln_metal_surface_descriptor* descriptor,
  mln_surface_session** out_surface
) -> mln_status;
auto vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_surface_session** out_surface
) -> mln_status;
auto surface_resize(
  mln_surface_session* surface, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status;
auto surface_render_update(mln_surface_session* surface) -> mln_status;
auto surface_detach(mln_surface_session* surface) -> mln_status;
auto surface_destroy(mln_surface_session* surface) -> mln_status;

}  // namespace mln::core
