#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>

#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/renderer/renderer.hpp>

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"

namespace mln::core {

enum class RenderSessionKind : uint8_t { Surface, Texture };
enum class TextureSessionApi : uint8_t { Generic, Metal, Vulkan };
enum class TextureSessionFrameKind : uint8_t { None, MetalOwned, VulkanOwned };
enum class TextureSessionMode : uint8_t { Owned, Borrowed };

using SurfaceSessionResizeCallback =
  void (*)(mln_render_session*, uint32_t, uint32_t);
using TextureSessionPrepareCallback = void (*)(mln_render_session*);
using TextureSessionAfterRenderCallback = mln_status (*)(mln_render_session*);

struct RenderSurfaceState {
  std::unique_ptr<mbgl::gfx::RendererBackend> backend = nullptr;
  SurfaceSessionResizeCallback resize_backend = nullptr;
};

struct RenderTextureState {
  std::unique_ptr<mbgl::gfx::HeadlessBackend> backend = nullptr;
  uint64_t next_frame_id = 1;
  uint64_t acquired_frame_id = 0;
  bool acquired = false;
  TextureSessionFrameKind acquired_frame_kind = TextureSessionFrameKind::None;
  TextureSessionApi api_kind = TextureSessionApi::Generic;
  TextureSessionMode mode = TextureSessionMode::Owned;
  void* rendered_native_texture = nullptr;
  void* acquired_native_texture = nullptr;
  TextureSessionPrepareCallback prepare_render_resources = nullptr;
  TextureSessionAfterRenderCallback after_render = nullptr;
};

}  // namespace mln::core

struct mln_render_session {
  mln::core::RenderSessionKind kind = mln::core::RenderSessionKind::Surface;
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

  std::unique_ptr<mbgl::Renderer> renderer = nullptr;
  mln::core::RenderSurfaceState surface;
  mln::core::RenderTextureState texture;
};

namespace mln::core {

struct RenderSessionAttachMessages {
  const char* null_session;
  const char* null_output;
  const char* non_null_output;
};

auto register_render_session(
  mln_render_session* handle, std::unique_ptr<mln_render_session> session
) -> void;
auto validate_render_session(mln_render_session* session) -> mln_status;
auto validate_live_attached_render_session(mln_render_session* session)
  -> mln_status;
auto erase_render_session(mln_render_session* session)
  -> std::unique_ptr<mln_render_session>;

inline auto validate_attach_output(
  mln_render_session** out_session, const char* null_message,
  const char* not_null_message
) -> mln_status {
  if (out_session == nullptr) {
    set_thread_error(null_message);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_session != nullptr) {
    set_thread_error(not_null_message);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

inline auto physical_dimension(uint32_t logical, double scale_factor)
  -> uint32_t {
  return static_cast<uint32_t>(std::ceil(logical * scale_factor));
}

inline auto validate_physical_size(
  uint32_t width, uint32_t height, double scale_factor,
  const char* too_large_message
) -> mln_status {
  constexpr auto max_dimension =
    static_cast<double>(std::numeric_limits<uint32_t>::max());
  if (
    std::ceil(width * scale_factor) > max_dimension ||
    std::ceil(height * scale_factor) > max_dimension
  ) {
    set_thread_error(too_large_message);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto attach_render_session(
  std::unique_ptr<mln_render_session> session, mln_render_session** out_session,
  RenderSessionKind kind, RenderSessionAttachMessages messages
) -> mln_status;

auto render_session_resize(
  mln_render_session* session, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status;
auto render_session_render_update(mln_render_session* session) -> mln_status;
auto render_session_detach(mln_render_session* session) -> mln_status;
auto render_session_destroy(mln_render_session* session) -> mln_status;
auto render_session_reduce_memory_use(mln_render_session* session)
  -> mln_status;
auto render_session_clear_data(mln_render_session* session) -> mln_status;
auto render_session_dump_debug_logs(mln_render_session* session) -> mln_status;
auto render_session_set_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector,
  const mln_json_value* state
) -> mln_status;
auto render_session_get_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector,
  mln_json_snapshot** out_state
) -> mln_status;
auto render_session_remove_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector
) -> mln_status;

}  // namespace mln::core
