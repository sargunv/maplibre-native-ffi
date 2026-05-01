#define MLN_BUILDING_C

#include <cstdint>

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"
#include "render/texture_session.hpp"

auto mln_owned_texture_descriptor_default(void) noexcept
  -> mln_owned_texture_descriptor {
  return mln::core::owned_texture_descriptor_default();
}

auto mln_metal_texture_descriptor_default(void) noexcept
  -> mln_metal_texture_descriptor {
  return mln::core::metal_texture_descriptor_default();
}

auto mln_vulkan_texture_descriptor_default(void) noexcept
  -> mln_vulkan_texture_descriptor {
  return mln::core::vulkan_texture_descriptor_default();
}

auto mln_owned_texture_attach(
  mln_map* map, const mln_owned_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::owned_texture_attach(map, descriptor, out_texture);
  });
}

auto mln_metal_texture_attach(
  mln_map* map, const mln_metal_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_texture_attach(map, descriptor, out_texture);
  });
}

auto mln_vulkan_texture_attach(
  mln_map* map, const mln_vulkan_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_texture_attach(map, descriptor, out_texture);
  });
}

auto mln_texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::texture_resize(texture, width, height, scale_factor);
  });
}

auto mln_texture_render_update(mln_texture_session* texture) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::texture_render_update(texture);
  });
}

auto mln_metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_texture_acquire_frame(texture, out_frame);
  });
}

auto mln_metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_texture_release_frame(texture, frame);
  });
}

auto mln_vulkan_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_texture_frame* out_frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_texture_acquire_frame(texture, out_frame);
  });
}

auto mln_vulkan_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_texture_frame* frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_texture_release_frame(texture, frame);
  });
}

auto mln_texture_detach(mln_texture_session* texture) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::texture_detach(texture);
  });
}

auto mln_texture_destroy(mln_texture_session* texture) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::texture_destroy(texture);
  });
}
