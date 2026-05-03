/**
 * @file maplibre_native_c/style.h
 * Public C API declarations for style mutation.
 */

#ifndef MAPLIBRE_NATIVE_C_STYLE_H
#define MAPLIBRE_NATIVE_C_STYLE_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Style mutation

typedef struct mln_style_id_list mln_style_id_list;

/** Style source type values returned by mln_map_get_style_source_type(). */
typedef enum mln_style_source_type : uint32_t {
  MLN_STYLE_SOURCE_TYPE_UNKNOWN = 0,
  MLN_STYLE_SOURCE_TYPE_VECTOR = 1,
  MLN_STYLE_SOURCE_TYPE_RASTER = 2,
  MLN_STYLE_SOURCE_TYPE_RASTER_DEM = 3,
  MLN_STYLE_SOURCE_TYPE_GEOJSON = 4,
  MLN_STYLE_SOURCE_TYPE_IMAGE = 5,
  MLN_STYLE_SOURCE_TYPE_VIDEO = 6,
  MLN_STYLE_SOURCE_TYPE_ANNOTATIONS = 7,
  MLN_STYLE_SOURCE_TYPE_CUSTOM_VECTOR = 8,
} mln_style_source_type;

/** Fixed source metadata returned by mln_map_get_style_source_info(). */
typedef struct mln_style_source_info {
  uint32_t size;
  /** One of mln_style_source_type. */
  uint32_t type;
  /** Source ID byte length, excluding any null terminator. */
  size_t id_size;
  bool is_volatile;
  bool has_attribution;
  /** Attribution byte length, excluding any null terminator. */
  size_t attribution_size;
} mln_style_source_info;

/**
 * Gets the number of IDs in a style ID list handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when list is null or not live, or out_count is
 *   null.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_style_id_list_count(
  const mln_style_id_list* list, size_t* out_count
) MLN_NOEXCEPT;

/**
 * Borrows one ID from a style ID list handle.
 *
 * On success, out_id receives a view into list-owned storage. The view remains
 * valid until the list is destroyed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when list is null or not live, index is out of
 *   range, or out_id is null.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_style_id_list_get(
  const mln_style_id_list* list, size_t index, mln_string_view* out_id
) MLN_NOEXCEPT;

/** Destroys a style ID list handle. Null is accepted as a no-op. */
MLN_API void mln_style_id_list_destroy(mln_style_id_list* list) MLN_NOEXCEPT;

/**
 * Adds one style source from a style-spec source JSON object.
 *
 * source_id and source_json are borrowed for the call. source_json is the
 * object that appears under sources[source_id] in a style document. The
 * function parses and copies the accepted source into MapLibre Native before
 * return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, source_json is null or invalid, the source ID already
 *   exists, or the source JSON cannot be converted.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_style_source_json(
  mln_map* map, mln_string_view source_id, const mln_json_value* source_json
) MLN_NOEXCEPT;

/**
 * Removes one style source by ID.
 *
 * source_id is borrowed for the call. On success, out_removed reports whether a
 * source existed and was removed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, or out_removed is null.
 * - MLN_STATUS_INVALID_STATE when the source exists but a layer still uses it.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_remove_style_source(
  mln_map* map, mln_string_view source_id, bool* out_removed
) MLN_NOEXCEPT;

/**
 * Reports whether a style source ID exists.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, or out_exists is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_style_source_exists(
  mln_map* map, mln_string_view source_id, bool* out_exists
) MLN_NOEXCEPT;

/**
 * Gets one style source type.
 *
 * On success, out_found reports whether source_id exists. When found,
 * out_source_type receives one of mln_style_source_type.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, out_source_type is null, or out_found is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_style_source_type(
  mln_map* map, mln_string_view source_id, uint32_t* out_source_type,
  bool* out_found
) MLN_NOEXCEPT;

/**
 * Copies fixed metadata for one style source.
 *
 * The returned struct contains string lengths, not string contents. Use
 * mln_map_copy_style_source_attribution() to copy attribution bytes when
 * has_attribution is true. The source ID is the lookup key and is also
 * available through style source ID lists.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, out_info is null, out_info->size is too small, or
 *   out_found is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_style_source_info(
  mln_map* map, mln_string_view source_id, mln_style_source_info* out_info,
  bool* out_found
) MLN_NOEXCEPT;

/**
 * Copies one style source attribution string into caller-owned memory.
 *
 * source_id is borrowed for the call. out_attribution may be null only when
 * attribution_capacity is 0. On success, out_attribution_size receives the byte
 * length of the attribution, excluding any null terminator. When out_found is
 * false or the source has no attribution, out_attribution_size receives 0.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, out_attribution is null with non-zero capacity,
 *   attribution_capacity is too small for a present attribution,
 *   out_attribution_size is null, or out_found is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_copy_style_source_attribution(
  mln_map* map, mln_string_view source_id, char* out_attribution,
  size_t attribution_capacity, size_t* out_attribution_size, bool* out_found
) MLN_NOEXCEPT;

/**
 * Copies style source IDs in style order.
 *
 * On success, *out_source_ids receives an owned list handle. Destroy it with
 * mln_style_id_list_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_source_ids is
 *   null, or *out_source_ids is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_list_style_source_ids(
  mln_map* map, mln_style_id_list** out_source_ids
) MLN_NOEXCEPT;

/**
 * Adds one style layer from a full style-spec layer JSON object.
 *
 * layer_json and before_layer_id are borrowed for the call. layer_json must
 * contain id and type members. Passing an empty before_layer_id appends the
 * layer; otherwise the layer is inserted before that existing layer.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_json is
 *   null or invalid, the layer ID already exists, before_layer_id is invalid or
 *   does not exist, or the layer JSON cannot be converted.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_style_layer_json(
  mln_map* map, const mln_json_value* layer_json,
  mln_string_view before_layer_id
) MLN_NOEXCEPT;

/**
 * Removes one style layer by ID.
 *
 * layer_id is borrowed for the call. On success, out_removed reports whether a
 * layer existed and was removed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, or out_removed is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_remove_style_layer(
  mln_map* map, mln_string_view layer_id, bool* out_removed
) MLN_NOEXCEPT;

/**
 * Reports whether a style layer ID exists.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, or out_exists is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_style_layer_exists(
  mln_map* map, mln_string_view layer_id, bool* out_exists
) MLN_NOEXCEPT;

/**
 * Borrows one style layer type string.
 *
 * On success, out_found reports whether layer_id exists. When found,
 * out_layer_type receives a view of a static style-spec layer type string.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, out_layer_type is null, or out_found is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_style_layer_type(
  mln_map* map, mln_string_view layer_id, mln_string_view* out_layer_type,
  bool* out_found
) MLN_NOEXCEPT;

/**
 * Copies style layer IDs in style order.
 *
 * On success, *out_layer_ids receives an owned list handle. Destroy it with
 * mln_style_id_list_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_layer_ids is
 *   null, or *out_layer_ids is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_list_style_layer_ids(
  mln_map* map, mln_style_id_list** out_layer_ids
) MLN_NOEXCEPT;

/**
 * Moves one style layer before another layer or to the top.
 *
 * layer_id and before_layer_id are borrowed for the call. Passing an empty
 * before_layer_id moves the layer to the top of the style order.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, before_layer_id is invalid, layer_id does not exist, or
 *   before_layer_id is non-empty and does not exist.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_move_style_layer(
  mln_map* map, mln_string_view layer_id, mln_string_view before_layer_id
) MLN_NOEXCEPT;

/**
 * Copies one style layer as a full style-spec layer JSON snapshot.
 *
 * On success, out_found reports whether layer_id exists. When found,
 * *out_layer receives an owned snapshot handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, out_layer is null, *out_layer is not null, or out_found
 *   is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_style_layer_json(
  mln_map* map, mln_string_view layer_id, mln_json_snapshot** out_layer,
  bool* out_found
) MLN_NOEXCEPT;

/**
 * Sets one layer property using its MapLibre style-spec property name.
 *
 * layer_id, property_name, and value are borrowed for the call. value is a
 * style-spec JSON value descriptor. Expressions use style-spec expression JSON
 * arrays. The function parses and copies the accepted value into MapLibre
 * Native's typed style property storage before return.
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
 * mln_json_snapshot_get() to borrow its root JSON value. Destroy the snapshot
 * with mln_json_snapshot_destroy(). Undefined native style properties return
 * null snapshots.
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
 * layer_id and filter are borrowed for the call. Passing null for filter clears
 * the layer filter. Non-null filters use the MapLibre style-spec filter JSON
 * representation. The function parses and copies the accepted filter into
 * MapLibre Native's typed filter expression storage before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, layer_id is
 *   invalid or empty, filter is invalid, the layer does not exist, or the
 *   filter cannot be converted.
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
 * mln_json_snapshot_get() to borrow its root JSON value. Destroy the snapshot
 * with mln_json_snapshot_destroy(). Missing filters return null snapshots.
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
