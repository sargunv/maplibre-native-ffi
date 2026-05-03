#define MLN_BUILDING_C

#include <cstdint>

#include "c_api/boundary.hpp"
#include "geojson/geojson.hpp"
#include "maplibre_native_c.h"
#include "render/render_session_common.hpp"

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

auto mln_render_session_set_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector,
  const mln_json_value* state
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_set_feature_state(
      session, selector, state
    );
  });
}

auto mln_render_session_get_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector,
  mln_json_snapshot** out_state
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_get_feature_state(
      session, selector, out_state
    );
  });
}

auto mln_render_session_remove_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::render_session_remove_feature_state(session, selector);
  });
}

auto mln_json_snapshot_get(
  const mln_json_snapshot* snapshot, const mln_json_value** out_value
) noexcept -> mln_status {
  return mln::c_api::status_boundary([&]() -> mln_status {
    return mln::core::json_snapshot_get(snapshot, out_value);
  });
}

auto mln_json_snapshot_destroy(mln_json_snapshot* snapshot) noexcept -> void {
  mln::core::json_snapshot_destroy(snapshot);
}
