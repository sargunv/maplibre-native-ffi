#define MLN_BUILDING_C

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
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_surface_attach(map, descriptor, out_session);
  });
}

auto mln_vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_surface_attach(map, descriptor, out_session);
  });
}
