#define MLN_BUILDING_C

#include <cstdint>

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"
#include "render/surface_session.hpp"

auto mln_metal_surface_descriptor_default(void) noexcept
  -> mln_metal_surface_descriptor {
  return mln::core::metal_surface_descriptor_default();
}

auto mln_vulkan_surface_descriptor_default(void) noexcept
  -> mln_vulkan_surface_descriptor {
  return mln::core::vulkan_surface_descriptor_default();
}

auto mln_metal_surface_attach(
  mln_map* map, const mln_metal_surface_descriptor* descriptor,
  mln_surface_session** out_surface
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_surface_attach(map, descriptor, out_surface);
  });
}

auto mln_vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_surface_session** out_surface
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_surface_attach(map, descriptor, out_surface);
  });
}

auto mln_surface_resize(
  mln_surface_session* surface, uint32_t width, uint32_t height,
  double scale_factor
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::surface_resize(surface, width, height, scale_factor);
  });
}

auto mln_surface_render_update(mln_surface_session* surface) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::surface_render_update(surface);
  });
}

auto mln_surface_detach(mln_surface_session* surface) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::surface_detach(surface);
  });
}

auto mln_surface_destroy(mln_surface_session* surface) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::surface_destroy(surface);
  });
}
