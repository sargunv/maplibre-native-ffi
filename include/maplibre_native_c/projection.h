/**
 * @file maplibre_native_c/projection.h
 * Public C API declarations for projection helpers.
 */

#ifndef MAPLIBRE_NATIVE_C_PROJECTION_H
#define MAPLIBRE_NATIVE_C_PROJECTION_H

#include <stddef.h>

#include "base.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a standalone projection helper from the current map transform.
 *
 * The helper owns projection and camera transform state only. It does not own
 * style, resources, render targets, or runtime events. Use it to convert
 * coordinates or compute camera fitting without changing the source map.
 *
 * Creation snapshots the map's transform. Later map camera or projection
 * changes do not update the helper. The creating thread owns the helper and
 * must call projection functions on that thread.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_projection is
 *   null, or *out_projection is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_create(
  mln_map* map, mln_map_projection** out_projection
) MLN_NOEXCEPT;

/**
 * Destroys a standalone projection helper.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_projection_destroy(mln_map_projection* projection) MLN_NOEXCEPT;

/**
 * Copies the current camera snapshot from a standalone projection helper.
 *
 * On success, *out_camera is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, out_camera
 *   is null, or out_camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_get_camera(
  mln_map_projection* projection, mln_camera_options* out_camera
) MLN_NOEXCEPT;

/**
 * Applies camera fields to a standalone projection helper.
 *
 * Only fields indicated by camera->fields affect the helper.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, camera is
 *   null, camera->size is too small, or camera->fields contains unknown bits.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_set_camera(
  mln_map_projection* projection, const mln_camera_options* camera
) MLN_NOEXCEPT;

/**
 * Updates a projection helper camera so coordinates are visible within padding.
 *
 * The coordinates array is borrowed for the duration of this call and is not
 * retained. Use mln_map_projection_get_camera() after this call to read the
 * computed camera.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live,
 *   coordinates is null, coordinate_count is 0, padding contains negative or
 *   non-finite values, or any coordinate contains invalid latitude or longitude
 *   values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_set_visible_coordinates(
  mln_map_projection* projection, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_edge_insets padding
) MLN_NOEXCEPT;

/**
 * Updates a projection helper camera so geometry coordinates are visible.
 *
 * The geometry descriptor graph, including all nested pointers, is borrowed for
 * the duration of this call and is not retained. Use
 * mln_map_projection_get_camera() after this call to read the computed camera.
 * Empty geometry objects and geometry collections with no coordinates are
 * invalid for camera fitting.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, geometry
 *   is null or invalid, padding contains negative or non-finite values, or the
 *   geometry contains no coordinates.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_set_visible_geometry(
  mln_map_projection* projection, const mln_geometry* geometry,
  mln_edge_insets padding
) MLN_NOEXCEPT;

/**
 * Converts a geographic world coordinate using a standalone projection helper.
 *
 * The output point uses logical map pixels with an origin at the top-left of
 * the helper viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, out_point
 *   is null, or coordinate contains invalid latitude or longitude values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_pixel_for_lat_lng(
  mln_map_projection* projection, mln_lat_lng coordinate,
  mln_screen_point* out_point
) MLN_NOEXCEPT;

/**
 * Converts a screen point using a standalone projection helper.
 *
 * The input point uses logical map pixels with an origin at the top-left of the
 * helper viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live,
 *   out_coordinate is null, or point contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_lat_lng_for_pixel(
  mln_map_projection* projection, mln_screen_point point,
  mln_lat_lng* out_coordinate
) MLN_NOEXCEPT;

/**
 * Converts a geographic coordinate to spherical Mercator projected meters.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_meters is null or coordinate contains
 *   invalid latitude or longitude values.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_projected_meters_for_lat_lng(
  mln_lat_lng coordinate, mln_projected_meters* out_meters
) MLN_NOEXCEPT;

/**
 * Converts spherical Mercator projected meters to a geographic coordinate.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_coordinate is null or meters contains
 *   non-finite values.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_lat_lng_for_projected_meters(
  mln_projected_meters meters, mln_lat_lng* out_coordinate
) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_PROJECTION_H
