#include "render/surface_session.hpp"

#include "maplibre_native_c.h"

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

}  // namespace mln::core
