#include <mutex>
#include <unordered_map>

#include "render/render_session_common.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"

namespace {

auto render_session_mutex() -> std::mutex& {
  static auto value = std::mutex{};
  return value;
}

auto render_sessions()
  -> std::unordered_map<mln_render_session*, mln::core::RenderSessionKind>& {
  static auto value =
    std::unordered_map<mln_render_session*, mln::core::RenderSessionKind>{};
  return value;
}

}  // namespace

namespace mln::core {

auto register_render_session(
  mln_render_session* session, RenderSessionKind kind
) -> void {
  const auto lock = std::scoped_lock{render_session_mutex()};
  render_sessions().emplace(session, kind);
}

auto unregister_render_session(mln_render_session* session) -> void {
  const auto lock = std::scoped_lock{render_session_mutex()};
  render_sessions().erase(session);
}

auto render_session_kind(
  mln_render_session* session, RenderSessionKind* out_kind
) -> mln_status {
  if (session == nullptr) {
    set_thread_error("render session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto lock = std::scoped_lock{render_session_mutex()};
  const auto found = render_sessions().find(session);
  if (found == render_sessions().end()) {
    set_thread_error("render session is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_kind = found->second;
  return MLN_STATUS_OK;
}

}  // namespace mln::core
