#define MLN_BUILDING_C

#include <cstdint>

#include "c_api/boundary.hpp"
#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"
#include "render/render_session_common.hpp"
#include "render/surface_session.hpp"
#include "render/texture_session.hpp"

namespace {

auto as_render_session(mln_surface_session* surface) -> mln_render_session* {
  return static_cast<mln_render_session*>(surface);
}

auto as_surface_session(mln_render_session* session) -> mln_surface_session* {
  return static_cast<mln_surface_session*>(session->concrete);
}

auto as_texture_session(mln_render_session* session) -> mln_texture_session* {
  return static_cast<mln_texture_session*>(session->concrete);
}

template <typename Operation>
auto with_renderer(mln_render_session* session, Operation operation)
  -> mln_status {
  auto kind = mln::core::RenderSessionKind::Surface;
  auto status = mln::core::render_session_kind(session, &kind);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  if (kind == mln::core::RenderSessionKind::Surface) {
    auto* surface = as_surface_session(session);
    status = mln::core::validate_live_attached_surface(surface);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    if (surface->renderer == nullptr) {
      mln::core::set_thread_error("render session renderer is not available");
      return MLN_STATUS_INVALID_STATE;
    }
    operation(*surface->renderer);
    return MLN_STATUS_OK;
  }

  auto* texture = as_texture_session(session);
  status = mln::core::validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->renderer == nullptr) {
    mln::core::set_thread_error("render session renderer is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  operation(*texture->renderer);
  return MLN_STATUS_OK;
}

}  // namespace

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
    auto output_status = mln::core::validate_render_session_attach_output(
      out_session, "out_session must not be null",
      "out_session must point to a null handle"
    );
    if (output_status != MLN_STATUS_OK) {
      return output_status;
    }
    auto* surface = static_cast<mln_surface_session*>(nullptr);
    auto status = mln::core::metal_surface_attach(map, descriptor, &surface);
    if (status == MLN_STATUS_OK) {
      *out_session = as_render_session(surface);
    }
    return status;
  });
}

auto mln_vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto output_status = mln::core::validate_render_session_attach_output(
      out_session, "out_session must not be null",
      "out_session must point to a null handle"
    );
    if (output_status != MLN_STATUS_OK) {
      return output_status;
    }
    auto* surface = static_cast<mln_surface_session*>(nullptr);
    auto status = mln::core::vulkan_surface_attach(map, descriptor, &surface);
    if (status == MLN_STATUS_OK) {
      *out_session = as_render_session(surface);
    }
    return status;
  });
}

auto mln_render_session_resize(
  mln_render_session* session, uint32_t width, uint32_t height,
  double scale_factor
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto kind = mln::core::RenderSessionKind::Surface;
    auto status = mln::core::render_session_kind(session, &kind);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    if (kind == mln::core::RenderSessionKind::Surface) {
      return mln::core::surface_resize(
        as_surface_session(session), width, height, scale_factor
      );
    }
    return mln::core::texture_resize(
      as_texture_session(session), width, height, scale_factor
    );
  });
}

auto mln_render_session_render_update(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto kind = mln::core::RenderSessionKind::Surface;
    auto status = mln::core::render_session_kind(session, &kind);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    if (kind == mln::core::RenderSessionKind::Surface) {
      return mln::core::surface_render_update(as_surface_session(session));
    }
    return mln::core::texture_render_update(as_texture_session(session));
  });
}

auto mln_render_session_detach(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto kind = mln::core::RenderSessionKind::Surface;
    auto status = mln::core::render_session_kind(session, &kind);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    if (kind == mln::core::RenderSessionKind::Surface) {
      return mln::core::surface_detach(as_surface_session(session));
    }
    return mln::core::texture_detach(as_texture_session(session));
  });
}

auto mln_render_session_destroy(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto kind = mln::core::RenderSessionKind::Surface;
    auto status = mln::core::render_session_kind(session, &kind);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    if (kind == mln::core::RenderSessionKind::Surface) {
      status = mln::core::surface_destroy(as_surface_session(session));
    } else {
      status = mln::core::texture_destroy(as_texture_session(session));
    }
    if (status == MLN_STATUS_OK) {
      mln::core::unregister_render_session(session);
    }
    return status;
  });
}

auto mln_render_session_reduce_memory_use(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return with_renderer(session, [](auto& renderer) -> void {
      renderer.reduceMemoryUse();
    });
  });
}

auto mln_render_session_clear_data(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return with_renderer(session, [](auto& renderer) -> void {
      renderer.clearData();
    });
  });
}

auto mln_render_session_dump_debug_logs(mln_render_session* session) noexcept
  -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return with_renderer(session, [](auto& renderer) -> void {
      renderer.dumpDebugLogs();
    });
  });
}
