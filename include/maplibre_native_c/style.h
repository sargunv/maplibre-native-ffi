/**
 * @file maplibre_native_c/style.h
 * Public C API declarations for style source and layer mutation.
 */

#ifndef MAPLIBRE_NATIVE_C_STYLE_H
#define MAPLIBRE_NATIVE_C_STYLE_H

#include "base.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Style mutation

/**
 * Sets one layer property using its MapLibre style-spec property name.
 *
 * layer_id, property_name, and value are borrowed for the duration of the call.
 * The value is a style-spec JSON value descriptor. Expressions use the standard
 * style-spec expression JSON array representation. The accepted property value
 * is parsed and copied into MapLibre Native's typed style property storage
 * before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id or
 *   property_name is invalid or empty, value is null or invalid, the layer does
 *   not exist, the property name is unknown for that layer, or the property
 *   value cannot be converted for that property.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_layer_property(
  mln_map* map, mln_string_view layer_id, mln_string_view property_name,
  const mln_json_value* value
) MLN_NOEXCEPT;

/**
 * Copies one layer property as a style-spec JSON value snapshot.
 *
 * On success, *out_value receives an owned snapshot handle. Use
 * mln_json_snapshot_get() to borrow its root JSON value, and destroy it with
 * mln_json_snapshot_destroy(). Undefined native style properties are reported
 * as null snapshots.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id or
 *   property_name is invalid or empty, out_value is null, *out_value is not
 *   null, or the layer does not exist.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_layer_property(
  mln_map* map, mln_string_view layer_id, mln_string_view property_name,
  mln_json_snapshot** out_value
) MLN_NOEXCEPT;

/**
 * Sets or clears one layer filter.
 *
 * layer_id and filter are borrowed for the duration of the call. Passing null
 * for filter clears the layer filter. Non-null filters use the MapLibre
 * style-spec filter JSON representation and are parsed into MapLibre Native's
 * typed filter expression storage before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, filter is invalid, the layer does not exist, or the
 * filter cannot be converted.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_layer_filter(
  mln_map* map, mln_string_view layer_id, const mln_json_value* filter
) MLN_NOEXCEPT;

/**
 * Copies one layer filter as a style-spec JSON value snapshot.
 *
 * On success, *out_filter receives an owned snapshot handle. Use
 * mln_json_snapshot_get() to borrow its root JSON value, and destroy it with
 * mln_json_snapshot_destroy(). Missing filters are reported as null snapshots.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, out_filter is null, *out_filter is not null, or the layer
 *   does not exist.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_layer_filter(
  mln_map* map, mln_string_view layer_id, mln_json_snapshot** out_filter
) MLN_NOEXCEPT;

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_STYLE_H
