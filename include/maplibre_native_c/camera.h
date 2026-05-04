/**
 * @file maplibre_native_c/camera.h
 * Public C API declarations for map camera and coordinate conversion.
 */

#ifndef MAPLIBRE_NATIVE_C_CAMERA_H
#define MAPLIBRE_NATIVE_C_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#include "base.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns empty camera options initialized for this C API version.
 */
MLN_API mln_camera_options mln_camera_options_default(void) MLN_NOEXCEPT;

/** Returns empty animation options initialized for this C API version. */
MLN_API mln_animation_options mln_animation_options_default(void) MLN_NOEXCEPT;

/** Returns empty camera fitting options initialized for this C API version. */
MLN_API mln_camera_fit_options
mln_camera_fit_options_default(void) MLN_NOEXCEPT;

/** Returns empty map bound options initialized for this C API version. */
MLN_API mln_bound_options mln_bound_options_default(void) MLN_NOEXCEPT;

/** Returns empty free camera options initialized for this C API version. */
MLN_API mln_free_camera_options
mln_free_camera_options_default(void) MLN_NOEXCEPT;

/**
 * Returns empty axonometric rendering options initialized for this C API
 * version.
 */
MLN_API mln_projection_mode mln_projection_mode_default(void) MLN_NOEXCEPT;

/** Returns empty viewport options initialized for this C API version. */
MLN_API mln_map_viewport_options
mln_map_viewport_options_default(void) MLN_NOEXCEPT;

/** Returns empty tile tuning options initialized for this C API version. */
MLN_API mln_map_tile_options mln_map_tile_options_default(void) MLN_NOEXCEPT;

/**
 * Applies MapLibre debug overlay mask bits to a map.
 *
 * Pass 0 to disable all debug overlays.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or options
 *   contains unknown bits.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_set_debug_options(mln_map* map, uint32_t options) MLN_NOEXCEPT;

/**
 * Copies the current MapLibre debug overlay mask bits.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or out_options is
 *   null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_get_debug_options(mln_map* map, uint32_t* out_options) MLN_NOEXCEPT;

/**
 * Enables or disables MapLibre's rendering stats overlay view.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_rendering_stats_view_enabled(
  mln_map* map, bool enabled
) MLN_NOEXCEPT;

/**
 * Copies whether MapLibre's rendering stats overlay view is enabled.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or out_enabled is
 *   null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_rendering_stats_view_enabled(
  mln_map* map, bool* out_enabled
) MLN_NOEXCEPT;

/**
 * Copies whether MapLibre currently considers the map fully loaded.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or out_loaded is
 *   null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_is_fully_loaded(mln_map* map, bool* out_loaded) MLN_NOEXCEPT;

/**
 * Dumps map debug logs through MapLibre Native logging.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_dump_debug_logs(mln_map* map) MLN_NOEXCEPT;

/**
 * Copies live map viewport and render-transform controls.
 *
 * On success, *out_options is overwritten and all known fields are marked.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_options is
 *   null, or out_options->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_viewport_options(
  mln_map* map, mln_map_viewport_options* out_options
) MLN_NOEXCEPT;

/**
 * Applies selected live map viewport and render-transform controls.
 *
 * Only fields indicated by options->fields affect the map.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, options is null,
 *   options->size is too small, options->fields contains unknown bits, an enum
 *   value is unknown, or an enabled frustum offset value is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_viewport_options(
  mln_map* map, const mln_map_viewport_options* options
) MLN_NOEXCEPT;

/**
 * Copies tile prefetch and LOD tuning controls.
 *
 * On success, *out_options is overwritten and all known fields are marked.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_options is
 *   null, or out_options->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_tile_options(
  mln_map* map, mln_map_tile_options* out_options
) MLN_NOEXCEPT;

/**
 * Applies selected tile prefetch and LOD tuning controls.
 *
 * Only fields indicated by options->fields affect the map.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, options is null,
 *   options->size is too small, options->fields contains unknown bits,
 *   prefetch_zoom_delta is greater than 255, a double field is non-finite, or
 *   lod_mode is unknown.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_tile_options(
  mln_map* map, const mln_map_tile_options* options
) MLN_NOEXCEPT;

/**
 * Copies the current camera snapshot.
 *
 * On success, *out_camera is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_camera is
 *   null, or out_camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_get_camera(mln_map* map, mln_camera_options* out_camera) MLN_NOEXCEPT;

/**
 * Applies a camera jump command.
 *
 * Only fields indicated by camera->fields affect the map.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null,
 *   camera->size is too small, camera->fields contains unknown bits, or an
 *   enabled camera field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_jump_to(mln_map* map, const mln_camera_options* camera) MLN_NOEXCEPT;

/**
 * Applies a camera ease transition command.
 *
 * Only fields indicated by camera->fields affect the map. Passing a null
 * animation uses MapLibre Native's default animation options.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null,
 *   camera->size is too small, camera->fields contains unknown bits, an enabled
 *   camera field is invalid, animation->size is too small, animation->fields
 *   contains unknown bits, or an enabled animation field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_ease_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) MLN_NOEXCEPT;

/**
 * Applies a camera fly transition command.
 *
 * Only fields indicated by camera->fields affect the map. Passing a null
 * animation uses MapLibre Native's default animation options.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null,
 *   camera->size is too small, camera->fields contains unknown bits, an enabled
 *   camera field is invalid, animation->size is too small, animation->fields
 *   contains unknown bits, or an enabled animation field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_fly_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) MLN_NOEXCEPT;

/**
 * Applies a screen-space pan command.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or a delta value
 *   is non-finite.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_move_by(mln_map* map, double delta_x, double delta_y) MLN_NOEXCEPT;

/**
 * Applies an animated screen-space pan command.
 *
 * Passing a null animation uses MapLibre Native's default animation options.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, a delta value is
 *   non-finite, animation->size is too small, animation->fields contains
 *   unknown bits, or an enabled animation field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_move_by_animated(
  mln_map* map, double delta_x, double delta_y,
  const mln_animation_options* animation
) MLN_NOEXCEPT;

/**
 * Applies a screen-space zoom command.
 *
 * Passing a null anchor uses MapLibre Native's default zoom anchor.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, scale is
 *   non-positive or non-finite, or anchor contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_scale_by(
  mln_map* map, double scale, const mln_screen_point* anchor
) MLN_NOEXCEPT;

/**
 * Applies an animated screen-space zoom command.
 *
 * Passing a null anchor uses MapLibre Native's default zoom anchor. Passing a
 * null animation uses MapLibre Native's default animation options.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, scale is
 *   non-positive or non-finite, anchor contains non-finite values,
 *   animation->size is too small, animation->fields contains unknown bits, or
 *   an enabled animation field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_scale_by_animated(
  mln_map* map, double scale, const mln_screen_point* anchor,
  const mln_animation_options* animation
) MLN_NOEXCEPT;

/**
 * Applies a screen-space rotate command.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or a point
 *   contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) MLN_NOEXCEPT;

/**
 * Applies an animated screen-space rotate command.
 *
 * Passing a null animation uses MapLibre Native's default animation options.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, a point contains
 *   non-finite values, animation->size is too small, animation->fields contains
 *   unknown bits, or an enabled animation field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_rotate_by_animated(
  mln_map* map, mln_screen_point first, mln_screen_point second,
  const mln_animation_options* animation
) MLN_NOEXCEPT;

/**
 * Applies a pitch delta command.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, or pitch is
 *   non-finite.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pitch_by(mln_map* map, double pitch) MLN_NOEXCEPT;

/**
 * Applies an animated pitch delta command.
 *
 * Passing a null animation uses MapLibre Native's default animation options.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, pitch is
 *   non-finite, animation->size is too small, animation->fields contains
 *   unknown bits, or an enabled animation field is invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pitch_by_animated(
  mln_map* map, double pitch, const mln_animation_options* animation
) MLN_NOEXCEPT;

/**
 * Cancels active camera transitions.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_cancel_transitions(mln_map* map) MLN_NOEXCEPT;

/**
 * Computes a camera that fits geographic bounds in the current viewport.
 *
 * Passing null fit_options uses zero padding with no bearing or pitch override.
 * On success, *out_camera is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, bounds are
 *   invalid, fit_options is invalid, out_camera is null, or out_camera->size is
 *   too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_camera_for_lat_lng_bounds(
  mln_map* map, mln_lat_lng_bounds bounds,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) MLN_NOEXCEPT;

/**
 * Computes a camera that fits geographic coordinates in the current viewport.
 *
 * The coordinates array is borrowed for the duration of this call and is not
 * retained. Passing null fit_options uses zero padding with no bearing or pitch
 * override. On success, *out_camera is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, coordinates is
 *   null, coordinate_count is 0, any coordinate is invalid, fit_options is
 *   invalid, out_camera is null, or out_camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_camera_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) MLN_NOEXCEPT;

/**
 * Computes a camera that fits a geometry in the current viewport.
 *
 * The geometry descriptor graph is borrowed for the duration of this call and
 * is not retained. Empty geometry objects and geometry collections with no
 * coordinates are invalid for camera fitting. Passing null fit_options uses
 * zero padding with no bearing or pitch override. On success, *out_camera is
 * overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, geometry is null
 *   or invalid, geometry contains no coordinates, fit_options is invalid,
 *   out_camera is null, or out_camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_camera_for_geometry(
  mln_map* map, const mln_geometry* geometry,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) MLN_NOEXCEPT;

/**
 * Computes wrapped geographic bounds for a camera in the current viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null or
 *   invalid, or out_bounds is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_lat_lng_bounds_for_camera(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) MLN_NOEXCEPT;

/**
 * Computes unwrapped geographic bounds for a camera in the current viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null or
 *   invalid, or out_bounds is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_lat_lng_bounds_for_camera_unwrapped(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) MLN_NOEXCEPT;

/**
 * Copies map camera constraint options.
 *
 * On success, *out_options is overwritten and all known fields are marked.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_options is
 *   null, or out_options->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_get_bounds(mln_map* map, mln_bound_options* out_options) MLN_NOEXCEPT;

/**
 * Applies selected map camera constraint options.
 *
 * Only fields indicated by options->fields affect the map.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, options is null,
 *   options->size is too small, options->fields contains unknown bits, bounds
 *   are invalid, a numeric field is non-finite, or paired min/max fields are
 *   inconsistent.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_set_bounds(mln_map* map, const mln_bound_options* options) MLN_NOEXCEPT;

/**
 * Copies the current free camera position and orientation.
 *
 * On success, *out_options is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_options is
 *   null, or out_options->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_free_camera_options(
  mln_map* map, mln_free_camera_options* out_options
) MLN_NOEXCEPT;

/**
 * Applies selected free camera position and orientation fields.
 *
 * Position uses MapLibre Native's modified Web Mercator camera space.
 * Orientation is a quaternion stored as x, y, z, w. Only fields indicated by
 * options->fields affect the map.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, options is null,
 *   options->size is too small, options->fields contains unknown bits, position
 *   contains non-finite values, or orientation contains non-finite values or is
 *   zero length.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_free_camera_options(
  mln_map* map, const mln_free_camera_options* options
) MLN_NOEXCEPT;

/**
 * Copies the current axonometric rendering options.
 *
 * On success, *out_mode is overwritten. MapLibre currently reports all fields.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_mode is null,
 *   or out_mode->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_projection_mode(
  mln_map* map, mln_projection_mode* out_mode
) MLN_NOEXCEPT;

/**
 * Applies axonometric rendering option fields to a map.
 *
 * Only fields indicated by mode->fields affect the map. Unspecified fields keep
 * their current native values. These options mutate the live map render
 * transform and do not change coordinate conversion units or formulas.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, mode is null,
 *   mode->size is too small, mode->fields contains unknown bits, or an enabled
 *   skew value is non-finite.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_projection_mode(
  mln_map* map, const mln_projection_mode* mode
) MLN_NOEXCEPT;

/**
 * Converts a geographic world coordinate to a screen point for the current map.
 *
 * The output point uses logical map pixels with an origin at the top-left of
 * the map viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_point is
 *   null, or coordinate contains invalid latitude or longitude values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pixel_for_lat_lng(
  mln_map* map, mln_lat_lng coordinate, mln_screen_point* out_point
) MLN_NOEXCEPT;

/**
 * Converts a screen point to a geographic world coordinate for the current map.
 *
 * The input point uses logical map pixels with an origin at the top-left of the
 * map viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_coordinate is
 *   null, or point contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_lat_lng_for_pixel(
  mln_map* map, mln_screen_point point, mln_lat_lng* out_coordinate
) MLN_NOEXCEPT;

/**
 * Converts geographic world coordinates to screen points for the current map.
 *
 * The caller owns both arrays. On success, out_points receives coordinate_count
 * entries. coordinates and out_points may be null only when coordinate_count is
 * 0.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, a required array
 *   is null, or any coordinate contains invalid latitude or longitude values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pixels_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  mln_screen_point* out_points
) MLN_NOEXCEPT;

/**
 * Converts screen points to geographic world coordinates for the current map.
 *
 * The caller owns both arrays. On success, out_coordinates receives point_count
 * entries. points and out_coordinates may be null only when point_count is 0.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, a required array
 *   is null, or any point contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_lat_lngs_for_pixels(
  mln_map* map, const mln_screen_point* points, size_t point_count,
  mln_lat_lng* out_coordinates
) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_CAMERA_H
