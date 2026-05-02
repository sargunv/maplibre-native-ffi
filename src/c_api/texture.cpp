#define MLN_BUILDING_C

#include <cstddef>
#include <cstdint>

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"
#include "render/texture_session.hpp"

auto mln_owned_texture_descriptor_default(void) noexcept
  -> mln_owned_texture_descriptor {
  return mln::core::owned_texture_descriptor_default();
}

auto mln_metal_owned_texture_descriptor_default(void) noexcept
  -> mln_metal_owned_texture_descriptor {
  return mln::core::metal_owned_texture_descriptor_default();
}

auto mln_metal_borrowed_texture_descriptor_default(void) noexcept
  -> mln_metal_borrowed_texture_descriptor {
  return mln::core::metal_borrowed_texture_descriptor_default();
}

auto mln_vulkan_owned_texture_descriptor_default(void) noexcept
  -> mln_vulkan_owned_texture_descriptor {
  return mln::core::vulkan_owned_texture_descriptor_default();
}

auto mln_vulkan_borrowed_texture_descriptor_default(void) noexcept
  -> mln_vulkan_borrowed_texture_descriptor {
  return mln::core::vulkan_borrowed_texture_descriptor_default();
}

auto mln_texture_image_info_default(void) noexcept -> mln_texture_image_info {
  return mln::core::texture_image_info_default();
}

auto mln_owned_texture_attach(
  mln_map* map, const mln_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::owned_texture_attach(map, descriptor, out_session);
  });
}

auto mln_metal_owned_texture_attach(
  mln_map* map, const mln_metal_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_owned_texture_attach(map, descriptor, out_session);
  });
}

auto mln_metal_borrowed_texture_attach(
  mln_map* map, const mln_metal_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_borrowed_texture_attach(
      map, descriptor, out_session
    );
  });
}

auto mln_vulkan_owned_texture_attach(
  mln_map* map, const mln_vulkan_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_owned_texture_attach(map, descriptor, out_session);
  });
}

auto mln_vulkan_borrowed_texture_attach(
  mln_map* map, const mln_vulkan_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_borrowed_texture_attach(
      map, descriptor, out_session
    );
  });
}

auto mln_texture_read_premultiplied_rgba8(
  mln_render_session* session, uint8_t* out_data, size_t out_data_capacity,
  mln_texture_image_info* out_info
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::texture_read_premultiplied_rgba8(
      session, out_data, out_data_capacity, out_info
    );
  });
}

auto mln_metal_owned_texture_acquire_frame(
  mln_render_session* session, mln_metal_owned_texture_frame* out_frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_owned_texture_acquire_frame(session, out_frame);
  });
}

auto mln_metal_owned_texture_release_frame(
  mln_render_session* session, const mln_metal_owned_texture_frame* frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::metal_owned_texture_release_frame(session, frame);
  });
}

auto mln_vulkan_owned_texture_acquire_frame(
  mln_render_session* session, mln_vulkan_owned_texture_frame* out_frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_owned_texture_acquire_frame(session, out_frame);
  });
}

auto mln_vulkan_owned_texture_release_frame(
  mln_render_session* session, const mln_vulkan_owned_texture_frame* frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::vulkan_owned_texture_release_frame(session, frame);
  });
}
