/**
 * @file maplibre_native_c/render_session.h
 * Public C API declarations for render sessions.
 */

#ifndef MAPLIBRE_NATIVE_C_RENDER_SESSION_H
#define MAPLIBRE_NATIVE_C_RENDER_SESSION_H

#include <stdint.h>

#include "base.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resizes an attached render session.
 *
 * Width and height are logical map dimensions. The scale_factor value maps
 * them to physical backend pixels.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live, dimensions
 *   are zero, scale_factor is non-positive or non-finite, or scaled dimensions
 *   are too large.
 * - MLN_STATUS_INVALID_STATE when the session is detached or a texture frame is
 *   currently acquired.
 * - MLN_STATUS_UNSUPPORTED when resizing is not supported by the session kind
 *   or mode, such as a caller-owned borrowed texture target.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_render_session_resize(
  mln_render_session* session, uint32_t width, uint32_t height,
  double scale_factor
) MLN_NOEXCEPT;

/**
 * Processes the latest map render update for an attached render session.
 *
 * Surface sessions render and present through their native surface. Texture
 * sessions render into their texture target.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live.
 * - MLN_STATUS_INVALID_STATE when no render update is available or the session
 *   is detached, or a texture frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_render_session_render_update(mln_render_session* session) MLN_NOEXCEPT;

/**
 * Detaches backend-bound render resources from the map while keeping the
 * session handle live for destruction.
 *
 * After detach, resize, render, readback, acquire, and renderer maintenance
 * operations return MLN_STATUS_INVALID_STATE.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live.
 * - MLN_STATUS_INVALID_STATE when already detached or a texture frame is
 *   acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_render_session_detach(mln_render_session* session) MLN_NOEXCEPT;

/**
 * Destroys a render session handle.
 *
 * If the session is still attached, this function detaches it first.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live.
 * - MLN_STATUS_INVALID_STATE when a texture frame is acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_render_session_destroy(mln_render_session* session) MLN_NOEXCEPT;

/**
 * Asks the session renderer to release cached resources where possible.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_render_session_reduce_memory_use(mln_render_session* session) MLN_NOEXCEPT;

/**
 * Clears renderer data for the session.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_render_session_clear_data(mln_render_session* session) MLN_NOEXCEPT;

/**
 * Dumps renderer debug logs for the session through MapLibre Native logging.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_render_session_dump_debug_logs(mln_render_session* session) MLN_NOEXCEPT;

/**
 * Sets per-feature state on a render source for this render session.
 *
 * The session renderer must already exist; call
 * mln_render_session_render_update() once after loading style data before using
 * feature state. selector->source_id and selector->feature_id are borrowed for
 * the duration of the call. state must be a JSON object descriptor and is
 * copied before return. The accepted command requests a map repaint.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live, selector is
 *   null or invalid, selector lacks MLN_FEATURE_STATE_SELECTOR_FEATURE_ID,
 *   state is null or not an object, state contains invalid descriptor data, or
 *   state contains non-finite numbers.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_render_session_set_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector,
  const mln_json_value* state
) MLN_NOEXCEPT;

/**
 * Copies per-feature state from a render source in this render session.
 *
 * The session renderer must already exist. selector->source_id and
 * selector->feature_id are borrowed for the duration of the call. On success,
 * *out_state receives an owned snapshot handle. Use
 * mln_json_snapshot_get() to borrow its root JSON object value, and destroy it
 * with mln_json_snapshot_destroy(). Missing native source or feature state is
 * reported as an empty object snapshot.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live, selector is
 *   null or invalid, selector lacks MLN_FEATURE_STATE_SELECTOR_FEATURE_ID,
 *   out_state is null, or *out_state is not null.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_render_session_get_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector,
  mln_json_snapshot** out_state
) MLN_NOEXCEPT;

/**
 * Removes per-feature state from a render source in this render session.
 *
 * The session renderer must already exist. selector->source_id is required.
 * selector->feature_id and selector->state_key are optional. Passing both
 * removes one state key from one feature. Passing only feature_id removes all
 * state for that feature. Passing neither removes all feature state for the
 * source/source-layer. The accepted command requests a map repaint.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live, selector is
 *   null or invalid, or selector has MLN_FEATURE_STATE_SELECTOR_STATE_KEY
 *   without MLN_FEATURE_STATE_SELECTOR_FEATURE_ID.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_render_session_remove_feature_state(
  mln_render_session* session, const mln_feature_state_selector* selector
) MLN_NOEXCEPT;

/**
 * Borrows the root JSON value from a snapshot handle.
 *
 * The returned pointer and all nested pointers remain valid until the snapshot
 * is destroyed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when snapshot is null or not live, or out_value
 *   is null.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_json_snapshot_get(
  const mln_json_snapshot* snapshot, const mln_json_value** out_value
) MLN_NOEXCEPT;

/** Destroys a JSON snapshot handle. Null is accepted as a no-op. */
MLN_API void mln_json_snapshot_destroy(
  mln_json_snapshot* snapshot
) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_RENDER_SESSION_H
