#define MLN_BUILDING_C

#include <cstdint>

#include "c_api/boundary.hpp"
#include "maplibre_native_c.h"
#include "render/render_session_common.hpp"
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

auto mln_render_session_resize(
  mln_render_session* session, uint32_t width, uint32_t height,
  double scale_factor
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_resize(
      session, width, height, scale_factor
    );
  });
}

auto mln_render_session_render_update(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_render_update(session);
  });
}

auto mln_render_session_detach(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_detach(session);
  });
}

auto mln_render_session_destroy(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_destroy(session);
  });
}

auto mln_render_session_reduce_memory_use(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_reduce_memory_use(session);
  });
}

auto mln_render_session_clear_data(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_clear_data(session);
  });
}

auto mln_render_session_dump_debug_logs(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_dump_debug_logs(session);
  });
}
