#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/util/size.hpp>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c.h"

namespace mln::core {

enum class RenderSessionKind : uint8_t { Surface, Texture };

auto register_render_session(
  mln_render_session* session, RenderSessionKind kind
) -> void;
auto unregister_render_session(mln_render_session* session) -> void;
auto render_session_kind(
  mln_render_session* session, RenderSessionKind* out_kind
) -> mln_status;

}  // namespace mln::core

struct mln_render_session {
  mln::core::RenderSessionKind kind = mln::core::RenderSessionKind::Surface;
  void* concrete = nullptr;
};

namespace mln::core {

struct RenderSessionAttachMessages {
  const char* null_session;
  const char* null_output;
  const char* non_null_output;
};

template <typename Session>
class RenderSessionRegistry final {
 public:
  auto validate(
    Session* session, const char* null_message, const char* not_live_message,
    const char* wrong_thread_message
  ) -> mln_status {
    if (session == nullptr) {
      set_thread_error(null_message);
      return MLN_STATUS_INVALID_ARGUMENT;
    }
    const std::scoped_lock lock(mutex_);
    if (!sessions_.contains(session)) {
      set_thread_error(not_live_message);
      return MLN_STATUS_INVALID_ARGUMENT;
    }
    if (session->owner_thread != std::this_thread::get_id()) {
      set_thread_error(wrong_thread_message);
      return MLN_STATUS_WRONG_THREAD;
    }
    return MLN_STATUS_OK;
  }

  auto emplace(Session* handle, std::unique_ptr<Session> session) -> void {
    const std::scoped_lock lock(mutex_);
    sessions_.emplace(handle, std::move(session));
  }

  auto erase(Session* handle) -> std::unique_ptr<Session> {
    const std::scoped_lock lock(mutex_);
    auto found = sessions_.find(handle);
    if (found == sessions_.end()) {
      return nullptr;
    }
    auto session = std::move(found->second);
    sessions_.erase(found);
    return session;
  }

 private:
  std::mutex mutex_;
  std::unordered_map<Session*, std::unique_ptr<Session>> sessions_;
};

template <typename Session>
auto validate_render_session_attach_output(
  Session** out_session, const char* null_message, const char* not_null_message
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

template <typename Session, typename Validate>
auto validate_live_attached_render_session(
  Session* session, Validate validate, const char* detached_message
) -> mln_status {
  const auto status = validate(session);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!session->attached || session->backend == nullptr) {
    set_thread_error(detached_message);
    return MLN_STATUS_INVALID_STATE;
  }
  return MLN_STATUS_OK;
}

inline auto render_session_physical_dimension(
  uint32_t logical, double scale_factor
) -> uint32_t {
  return static_cast<uint32_t>(std::ceil(logical * scale_factor));
}

inline auto validate_render_session_physical_size(
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

template <typename Session>
auto attach_render_session(
  std::unique_ptr<Session> session, Session** out_session,
  RenderSessionRegistry<Session>& registry, RenderSessionKind kind,
  RenderSessionAttachMessages messages
) -> mln_status {
  if (session == nullptr) {
    set_thread_error(messages.null_session);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto output_status = validate_render_session_attach_output(
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
    session->concrete = handle;
    registry.emplace(handle, std::move(session));
    register_render_session(static_cast<mln_render_session*>(handle), kind);
  } catch (...) {
    unregister_render_session(static_cast<mln_render_session*>(handle));
    static_cast<void>(map_detach_render_target_session(map, handle));
    static_cast<void>(registry.erase(handle));
    throw;
  }

  *out_session = handle;
  return MLN_STATUS_OK;
}

}  // namespace mln::core
