#define MLN_BUILDING_C

#include <cstddef>
#include <cstdint>

#include "c_api/boundary.hpp"
#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"
#include "render/render_session_common.hpp"
#include "render/texture_session.hpp"

namespace {

auto as_render_session(mln_texture_session* texture) -> mln_render_session* {
  return static_cast<mln_render_session*>(texture);
}

auto as_texture_session(mln_render_session* session) -> mln_texture_session* {
  return static_cast<mln_texture_session*>(session->concrete);
}

auto validate_texture_render_session(mln_render_session* session)
  -> mln_status {
  auto kind = mln::core::RenderSessionKind::Surface;
  auto status = mln::core::render_session_kind(session, &kind);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (kind != mln::core::RenderSessionKind::Texture) {
    mln::core::set_thread_error("render session is not a texture session");
    return MLN_STATUS_UNSUPPORTED;
  }
  return MLN_STATUS_OK;
}

auto attach_texture_session(
  auto attach, mln_map* map, const auto* descriptor,
  mln_render_session** out_session
) -> mln_status {
  auto output_status = mln::core::validate_render_session_attach_output(
    out_session, "out_session must not be null",
    "out_session must point to a null handle"
  );
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  auto* texture = static_cast<mln_texture_session*>(nullptr);
  auto status = attach(map, descriptor, &texture);
  if (status == MLN_STATUS_OK) {
    *out_session = as_render_session(texture);
  }
  return status;
}

}  // namespace

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
    return attach_texture_session(
      mln::core::owned_texture_attach, map, descriptor, out_session
    );
  });
}

auto mln_metal_owned_texture_attach(
  mln_map* map, const mln_metal_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return attach_texture_session(
      mln::core::metal_owned_texture_attach, map, descriptor, out_session
    );
  });
}

auto mln_metal_borrowed_texture_attach(
  mln_map* map, const mln_metal_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return attach_texture_session(
      mln::core::metal_borrowed_texture_attach, map, descriptor, out_session
    );
  });
}

auto mln_vulkan_owned_texture_attach(
  mln_map* map, const mln_vulkan_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return attach_texture_session(
      mln::core::vulkan_owned_texture_attach, map, descriptor, out_session
    );
  });
}

auto mln_vulkan_borrowed_texture_attach(
  mln_map* map, const mln_vulkan_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_session
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return attach_texture_session(
      mln::core::vulkan_borrowed_texture_attach, map, descriptor, out_session
    );
  });
}

auto mln_texture_read_premultiplied_rgba8(
  mln_render_session* session, uint8_t* out_data, size_t out_data_capacity,
  mln_texture_image_info* out_info
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto status = validate_texture_render_session(session);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    return mln::core::texture_read_premultiplied_rgba8(
      as_texture_session(session), out_data, out_data_capacity, out_info
    );
  });
}

auto mln_metal_owned_texture_acquire_frame(
  mln_render_session* session, mln_metal_owned_texture_frame* out_frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto status = validate_texture_render_session(session);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    return mln::core::metal_owned_texture_acquire_frame(
      as_texture_session(session), out_frame
    );
  });
}

auto mln_metal_owned_texture_release_frame(
  mln_render_session* session, const mln_metal_owned_texture_frame* frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto status = validate_texture_render_session(session);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    return mln::core::metal_owned_texture_release_frame(
      as_texture_session(session), frame
    );
  });
}

auto mln_vulkan_owned_texture_acquire_frame(
  mln_render_session* session, mln_vulkan_owned_texture_frame* out_frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto status = validate_texture_render_session(session);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    return mln::core::vulkan_owned_texture_acquire_frame(
      as_texture_session(session), out_frame
    );
  });
}

auto mln_vulkan_owned_texture_release_frame(
  mln_render_session* session, const mln_vulkan_owned_texture_frame* frame
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    auto status = validate_texture_render_session(session);
    if (status != MLN_STATUS_OK) {
      return status;
    }
    return mln::core::vulkan_owned_texture_release_frame(
      as_texture_session(session), frame
    );
  });
}
