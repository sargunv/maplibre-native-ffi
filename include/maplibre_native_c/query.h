/**
 * @file maplibre_native_c/query.h
 * Public C API declarations for rendered and source feature queries.
 */

#ifndef MAPLIBRE_NATIVE_C_QUERY_H
#define MAPLIBRE_NATIVE_C_QUERY_H

#include <stddef.h>
#include <stdint.h>

#include "base.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Rendered and source feature queries

typedef struct mln_feature_query_result mln_feature_query_result;

/** Rendered feature query geometry variants. */
typedef enum mln_rendered_query_geometry_type : uint32_t {
  MLN_RENDERED_QUERY_GEOMETRY_TYPE_POINT = 1,
  MLN_RENDERED_QUERY_GEOMETRY_TYPE_BOX = 2,
  MLN_RENDERED_QUERY_GEOMETRY_TYPE_LINE_STRING = 3,
} mln_rendered_query_geometry_type;

/** Screen-space box in logical map pixels. */
typedef struct mln_screen_box {
  mln_screen_point min;
  mln_screen_point max;
} mln_screen_box;

/** Screen-space line string in logical map pixels. */
typedef struct mln_screen_line_string {
  /** Points. Null only when point_count is 0. */
  const mln_screen_point* points;
  size_t point_count;
} mln_screen_line_string;

/** Rendered feature query geometry descriptor. */
typedef struct mln_rendered_query_geometry {
  uint32_t size;
  /** One of mln_rendered_query_geometry_type. */
  uint32_t type;
  union {
    mln_screen_point point;
    mln_screen_box box;
    mln_screen_line_string line_string;
  } data;
} mln_rendered_query_geometry;

/** Optional fields for mln_rendered_feature_query_options. */
typedef enum mln_rendered_feature_query_option_field : uint32_t {
  MLN_RENDERED_FEATURE_QUERY_OPTION_LAYER_IDS = 1U << 0U,
} mln_rendered_feature_query_option_field;

/** Options for rendered feature queries. */
typedef struct mln_rendered_feature_query_options {
  uint32_t size;
  uint32_t fields;
  /** Optional style layer IDs. When absent, all rendered layers are queried. */
  const mln_string_view* layer_ids;
  size_t layer_id_count;
  /** Optional MapLibre style-spec filter JSON. Null means no filter. */
  const mln_json_value* filter;
} mln_rendered_feature_query_options;

/** Optional fields for mln_source_feature_query_options. */
typedef enum mln_source_feature_query_option_field : uint32_t {
  MLN_SOURCE_FEATURE_QUERY_OPTION_SOURCE_LAYER_IDS = 1U << 0U,
} mln_source_feature_query_option_field;

/** Options for source feature queries. */
typedef struct mln_source_feature_query_options {
  uint32_t size;
  uint32_t fields;
  /** Optional source-layer IDs. Required by vector sources; ignored by GeoJSON.
   */
  const mln_string_view* source_layer_ids;
  size_t source_layer_id_count;
  /** Optional MapLibre style-spec filter JSON. Null means no filter. */
  const mln_json_value* filter;
} mln_source_feature_query_options;

/** Optional fields returned by mln_queried_feature. */
typedef enum mln_queried_feature_field : uint32_t {
  MLN_QUERIED_FEATURE_SOURCE_ID = 1U << 0U,
  MLN_QUERIED_FEATURE_SOURCE_LAYER_ID = 1U << 1U,
  MLN_QUERIED_FEATURE_STATE = 1U << 2U,
} mln_queried_feature_field;

/** One copied query result feature. */
typedef struct mln_queried_feature {
  uint32_t size;
  uint32_t fields;
  /** GeoJSON feature descriptor. Nested pointers are result-owned. */
  mln_feature feature;
  /** Native render source ID when available. */
  mln_string_view source_id;
  /** Native source layer ID when available. */
  mln_string_view source_layer_id;
  /** Rendered feature state when available. */
  const mln_json_value* state;
} mln_queried_feature;

/** Returns default rendered feature query options. */
MLN_API mln_rendered_feature_query_options
mln_rendered_feature_query_options_default(void) MLN_NOEXCEPT;

/** Returns default source feature query options. */
MLN_API mln_source_feature_query_options
mln_source_feature_query_options_default(void) MLN_NOEXCEPT;

/** Returns a rendered point query geometry descriptor. */
MLN_API mln_rendered_query_geometry
mln_rendered_query_geometry_point(mln_screen_point point) MLN_NOEXCEPT;

/** Returns a rendered box query geometry descriptor. */
MLN_API mln_rendered_query_geometry
mln_rendered_query_geometry_box(mln_screen_box box) MLN_NOEXCEPT;

/** Returns a rendered line-string query geometry descriptor. */
MLN_API mln_rendered_query_geometry mln_rendered_query_geometry_line_string(
  const mln_screen_point* points, size_t point_count
) MLN_NOEXCEPT;

/**
 * Queries rendered features from the latest render session state.
 *
 * The session renderer must already exist. geometry and options are borrowed
 * for the duration of the call. Passing null for options uses default options.
 * On success, *out_result receives an owned result handle. Destroy it with
 * mln_feature_query_result_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live, geometry is
 *   null or invalid, options are invalid, out_result is null, or *out_result is
 *   not null.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_render_session_query_rendered_features(
  mln_render_session* session, const mln_rendered_query_geometry* geometry,
  const mln_rendered_feature_query_options* options,
  mln_feature_query_result** out_result
) MLN_NOEXCEPT;

/**
 * Queries source features from the latest render session state.
 *
 * The session renderer must already exist. source_id and options are borrowed
 * for the duration of the call. Passing null for options uses default options.
 * On success, *out_result receives an owned result handle. Destroy it with
 * mln_feature_query_result_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when session is null or not live, source_id is
 *   invalid or empty, options are invalid, out_result is null, or *out_result
 * is not null.
 * - MLN_STATUS_INVALID_STATE when the session is detached or no renderer has
 *   been created for the session yet.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_render_session_query_source_features(
  mln_render_session* session, mln_string_view source_id,
  const mln_source_feature_query_options* options,
  mln_feature_query_result** out_result
) MLN_NOEXCEPT;

/**
 * Gets the number of features in a query result handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when result is null or not live, or out_count
 *   is null.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_feature_query_result_count(
  const mln_feature_query_result* result, size_t* out_count
) MLN_NOEXCEPT;

/**
 * Borrows one feature from a query result handle.
 *
 * On success, out_feature receives views into result-owned storage. Those views
 * remain valid until result is destroyed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when result is null or not live, index is out
 *   of range, out_feature is null, or out_feature->size is too small.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_feature_query_result_get(
  const mln_feature_query_result* result, size_t index,
  mln_queried_feature* out_feature
) MLN_NOEXCEPT;

/** Destroys a feature query result handle. Null is accepted as a no-op. */
MLN_API void mln_feature_query_result_destroy(
  mln_feature_query_result* result
) MLN_NOEXCEPT;

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_QUERY_H
