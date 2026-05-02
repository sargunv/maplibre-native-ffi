#include <cstdint>
#include <memory>
#include <utility>

#include "render/surface_session.hpp"

#include "maplibre_native_c.h"
#include "render/render_session_common.hpp"

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

auto validate_surface_attach_output(mln_render_session** out_surface)
  -> mln_status {
  return validate_render_session_attach_output(
    out_surface, "out_surface must not be null",
    "out_surface must point to a null handle"
  );
}

auto validate_surface(mln_render_session* surface) -> mln_status {
  return validate_render_session(surface);
}

auto validate_live_attached_surface(mln_render_session* surface) -> mln_status {
  return validate_live_attached_render_session(surface);
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
  std::unique_ptr<mln_render_session> session, mln_render_session** out_surface
) -> mln_status {
  return attach_render_session(
    std::move(session), out_surface, RenderSessionKind::Surface,
    RenderSessionAttachMessages{
      .null_session = "surface session must not be null",
      .null_output = "out_surface must not be null",
      .non_null_output = "out_surface must point to a null handle"
    }
  );
}

}  // namespace mln::core
