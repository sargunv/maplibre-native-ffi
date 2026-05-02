#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/util/size.hpp>

#include "render/render_session_common.hpp"

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c.h"

namespace {

auto render_session_mutex() -> std::mutex& {
  static auto value = std::mutex{};
  return value;
}

auto render_sessions() -> std::unordered_map<
  mln_render_session*, std::unique_ptr<mln_render_session>>& {
  static auto value = std::unordered_map<
    mln_render_session*, std::unique_ptr<mln_render_session>>{};
  return value;
}

auto has_backend(const mln_render_session* session) -> bool {
  if (session->kind == mln::core::RenderSessionKind::Surface) {
    return session->surface.backend != nullptr;
  }
  return session->texture.backend != nullptr;
}

auto validate_dimensions(
  uint32_t width, uint32_t height, double scale_factor, const char* message
) -> mln_status {
  if (
    width == 0 || height == 0 || !std::isfinite(scale_factor) ||
    scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(message);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto renderer_backend(mln_render_session* session)
  -> mbgl::gfx::RendererBackend* {
  if (session->kind == mln::core::RenderSessionKind::Surface) {
    return session->surface.backend.get();
  }
  return session->texture.backend->getRendererBackend();
}

}  // namespace

namespace mln::core {

auto register_render_session(
  mln_render_session* handle, std::unique_ptr<mln_render_session> session
) -> void {
  const auto lock = std::scoped_lock{render_session_mutex()};
  render_sessions().emplace(handle, std::move(session));
}

auto validate_render_session(mln_render_session* session) -> mln_status {
  if (session == nullptr) {
    set_thread_error("render session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto lock = std::scoped_lock{render_session_mutex()};
  if (!render_sessions().contains(session)) {
    set_thread_error("render session is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (session->owner_thread != std::this_thread::get_id()) {
    set_thread_error("render session call must be made on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }
  return MLN_STATUS_OK;
}

auto validate_live_attached_render_session(mln_render_session* session)
  -> mln_status {
  const auto status = validate_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!session->attached || !has_backend(session)) {
    set_thread_error("render session is detached");
    return MLN_STATUS_INVALID_STATE;
  }
  return MLN_STATUS_OK;
}

auto erase_render_session(mln_render_session* session)
  -> std::unique_ptr<mln_render_session> {
  const auto lock = std::scoped_lock{render_session_mutex()};
  auto found = render_sessions().find(session);
  if (found == render_sessions().end()) {
    return nullptr;
  }
  auto owned = std::move(found->second);
  render_sessions().erase(found);
  return owned;
}

auto attach_render_session(
  std::unique_ptr<mln_render_session> session, mln_render_session** out_session,
  RenderSessionKind kind, RenderSessionAttachMessages messages
) -> mln_status {
  if (session == nullptr) {
    set_thread_error(messages.null_session);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto output_status = validate_attach_output(
    out_session, messages.null_output, messages.non_null_output
  );
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  auto* map = session->map;
  auto* handle = session.get();
  const auto attach_status = map_attach_render_target_session(map, handle);
  if (attach_status != MLN_STATUS_OK) {
    return attach_status;
  }
  try {
    if (auto* native_map = map_native(map); native_map != nullptr) {
      native_map->setSize(mbgl::Size{session->width, session->height});
    }
    session->kind = kind;
    register_render_session(handle, std::move(session));
  } catch (...) {
    static_cast<void>(map_detach_render_target_session(map, handle));
    throw;
  }

  *out_session = handle;
  return MLN_STATUS_OK;
}

auto render_session_resize(
  mln_render_session* session, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status {
  const auto status = validate_live_attached_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto dimensions_status = validate_dimensions(
    width, height, scale_factor,
    session->kind == RenderSessionKind::Surface
      ? "surface dimensions and scale_factor must be positive"
      : "texture dimensions and scale_factor must be positive"
  );
  if (dimensions_status != MLN_STATUS_OK) {
    return dimensions_status;
  }
  if (
    session->kind == RenderSessionKind::Texture && session->texture.acquired
  ) {
    set_thread_error("cannot resize while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (
    session->kind == RenderSessionKind::Texture &&
    session->texture.mode == TextureSessionMode::Borrowed
  ) {
    set_thread_error(
      "borrowed texture sessions cannot be resized; attach a new target"
    );
    return MLN_STATUS_UNSUPPORTED;
  }
  const auto physical_status = validate_physical_size(
    width, height, scale_factor,
    session->kind == RenderSessionKind::Surface
      ? "scaled surface dimensions are too large"
      : "scaled texture dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }

  const auto physical_width = physical_dimension(width, scale_factor);
  const auto physical_height = physical_dimension(height, scale_factor);
  if (session->kind == RenderSessionKind::Surface) {
    if (session->surface.resize_backend != nullptr) {
      session->surface.resize_backend(session, physical_width, physical_height);
    }
  } else {
    session->texture.backend->setSize(
      mbgl::Size{physical_width, physical_height}
    );
    session->texture.rendered_native_texture = nullptr;
    session->texture.acquired_native_texture = nullptr;
    session->texture.acquired_frame_kind = TextureSessionFrameKind::None;
  }
  if (auto* native_map = map_native(session->map); native_map != nullptr) {
    native_map->setSize(mbgl::Size{width, height});
  }
  session->renderer.reset();
  session->rendered_generation = 0;
  session->width = width;
  session->height = height;
  session->physical_width = physical_width;
  session->physical_height = physical_height;
  session->scale_factor = scale_factor;
  ++session->generation;
  return MLN_STATUS_OK;
}

auto render_session_render_update(mln_render_session* session) -> mln_status {
  const auto status = validate_live_attached_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    session->kind == RenderSessionKind::Texture && session->texture.acquired
  ) {
    set_thread_error("cannot render while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }

  auto update = map_latest_update(session->map);
  if (!update) {
    set_thread_error("no map render update is available");
    return MLN_STATUS_INVALID_STATE;
  }

  if (
    session->kind == RenderSessionKind::Texture &&
    session->texture.prepare_render_resources != nullptr
  ) {
    session->texture.prepare_render_resources(session);
  }
  auto* backend = renderer_backend(session);
  if (backend == nullptr) {
    set_thread_error("render session renderer backend is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  auto guard = mbgl::gfx::BackendScope{
    *backend, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  map_run_render_jobs(session->map);
  if (session->renderer == nullptr) {
    session->renderer = std::make_unique<mbgl::Renderer>(
      *backend, static_cast<float>(session->scale_factor)
    );
    session->renderer->setObserver(map_renderer_observer(session->map));
  }

  session->renderer->render(update);
  if (
    session->kind == RenderSessionKind::Texture &&
    session->texture.after_render != nullptr
  ) {
    const auto after_status = session->texture.after_render(session);
    if (after_status != MLN_STATUS_OK) {
      return after_status;
    }
  }
  session->rendered_generation = session->generation;
  return MLN_STATUS_OK;
}

auto render_session_detach(mln_render_session* session) -> mln_status {
  const auto status = validate_live_attached_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    session->kind == RenderSessionKind::Texture && session->texture.acquired
  ) {
    set_thread_error("cannot detach while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }

  const auto detach_status =
    map_detach_render_target_session(session->map, session);
  if (detach_status != MLN_STATUS_OK) {
    return detach_status;
  }
  session->renderer.reset();
  session->surface.backend.reset();
  session->texture.backend.reset();
  session->attached = false;
  session->rendered_generation = 0;
  session->texture.rendered_native_texture = nullptr;
  session->texture.acquired_native_texture = nullptr;
  session->texture.acquired_frame_kind = TextureSessionFrameKind::None;
  ++session->generation;
  return MLN_STATUS_OK;
}

auto render_session_destroy(mln_render_session* session) -> mln_status {
  const auto status = validate_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    session->kind == RenderSessionKind::Texture && session->texture.acquired
  ) {
    set_thread_error("cannot destroy while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (session->attached) {
    const auto detach_status = render_session_detach(session);
    if (detach_status != MLN_STATUS_OK) {
      return detach_status;
    }
  }
  auto owned_session = erase_render_session(session);
  owned_session.reset();
  return MLN_STATUS_OK;
}

auto render_session_reduce_memory_use(mln_render_session* session)
  -> mln_status {
  const auto status = validate_live_attached_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (session->renderer == nullptr) {
    set_thread_error("render session renderer is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  session->renderer->reduceMemoryUse();
  return MLN_STATUS_OK;
}

auto render_session_clear_data(mln_render_session* session) -> mln_status {
  const auto status = validate_live_attached_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (session->renderer == nullptr) {
    set_thread_error("render session renderer is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  session->renderer->clearData();
  return MLN_STATUS_OK;
}

auto render_session_dump_debug_logs(mln_render_session* session) -> mln_status {
  const auto status = validate_live_attached_render_session(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (session->renderer == nullptr) {
    set_thread_error("render session renderer is not available");
    return MLN_STATUS_INVALID_STATE;
  }
  session->renderer->dumpDebugLogs();
  return MLN_STATUS_OK;
}

}  // namespace mln::core
