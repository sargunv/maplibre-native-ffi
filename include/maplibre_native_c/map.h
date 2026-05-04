/**
 * @file maplibre_native_c/map.h
 * Public C API declarations for map lifecycle, shared types, and offline
 * regions.
 */

#ifndef MAPLIBRE_NATIVE_C_MAP_H
#define MAPLIBRE_NATIVE_C_MAP_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"
#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Field mask values for mln_camera_options. */
typedef enum mln_camera_option_field : uint32_t {
  MLN_CAMERA_OPTION_CENTER = 1U << 0U,
  MLN_CAMERA_OPTION_ZOOM = 1U << 1U,
  MLN_CAMERA_OPTION_BEARING = 1U << 2U,
  MLN_CAMERA_OPTION_PITCH = 1U << 3U,
  MLN_CAMERA_OPTION_CENTER_ALTITUDE = 1U << 4U,
  MLN_CAMERA_OPTION_PADDING = 1U << 5U,
  MLN_CAMERA_OPTION_ANCHOR = 1U << 6U,
  MLN_CAMERA_OPTION_ROLL = 1U << 7U,
  MLN_CAMERA_OPTION_FOV = 1U << 8U,
} mln_camera_option_field;

/** Field mask values for mln_animation_options. */
typedef enum mln_animation_option_field : uint32_t {
  MLN_ANIMATION_OPTION_DURATION = 1U << 0U,
  MLN_ANIMATION_OPTION_VELOCITY = 1U << 1U,
  MLN_ANIMATION_OPTION_MIN_ZOOM = 1U << 2U,
  MLN_ANIMATION_OPTION_EASING = 1U << 3U,
} mln_animation_option_field;

/** Field mask values for mln_camera_fit_options. */
typedef enum mln_camera_fit_option_field : uint32_t {
  MLN_CAMERA_FIT_OPTION_PADDING = 1U << 0U,
  MLN_CAMERA_FIT_OPTION_BEARING = 1U << 1U,
  MLN_CAMERA_FIT_OPTION_PITCH = 1U << 2U,
} mln_camera_fit_option_field;

/** Field mask values for mln_bound_options. */
typedef enum mln_bound_option_field : uint32_t {
  MLN_BOUND_OPTION_BOUNDS = 1U << 0U,
  MLN_BOUND_OPTION_MIN_ZOOM = 1U << 1U,
  MLN_BOUND_OPTION_MAX_ZOOM = 1U << 2U,
  MLN_BOUND_OPTION_MIN_PITCH = 1U << 3U,
  MLN_BOUND_OPTION_MAX_PITCH = 1U << 4U,
} mln_bound_option_field;

/** Field mask values for mln_free_camera_options. */
typedef enum mln_free_camera_option_field : uint32_t {
  MLN_FREE_CAMERA_OPTION_POSITION = 1U << 0U,
  MLN_FREE_CAMERA_OPTION_ORIENTATION = 1U << 1U,
} mln_free_camera_option_field;

/** Field mask values for MapLibre axonometric rendering options. */
typedef enum mln_projection_mode_field : uint32_t {
  MLN_PROJECTION_MODE_AXONOMETRIC = 1U << 0U,
  MLN_PROJECTION_MODE_X_SKEW = 1U << 1U,
  MLN_PROJECTION_MODE_Y_SKEW = 1U << 2U,
} mln_projection_mode_field;

/** Debug overlay mask values for mln_map_set_debug_options(). */
typedef enum mln_map_debug_option : uint32_t {
  MLN_MAP_DEBUG_TILE_BORDERS = 1U << 1U,
  MLN_MAP_DEBUG_PARSE_STATUS = 1U << 2U,
  MLN_MAP_DEBUG_TIMESTAMPS = 1U << 3U,
  MLN_MAP_DEBUG_COLLISION = 1U << 4U,
  MLN_MAP_DEBUG_OVERDRAW = 1U << 5U,
  MLN_MAP_DEBUG_STENCIL_CLIP = 1U << 6U,
  MLN_MAP_DEBUG_DEPTH_BUFFER = 1U << 7U,
} mln_map_debug_option;

/** Map north orientation values used by mln_map_viewport_options. */
typedef enum mln_north_orientation : uint32_t {
  MLN_NORTH_ORIENTATION_UP = 0,
  MLN_NORTH_ORIENTATION_RIGHT = 1,
  MLN_NORTH_ORIENTATION_DOWN = 2,
  MLN_NORTH_ORIENTATION_LEFT = 3,
} mln_north_orientation;

/** Map constraint modes used by mln_map_viewport_options. */
typedef enum mln_constrain_mode : uint32_t {
  MLN_CONSTRAIN_MODE_NONE = 0,
  MLN_CONSTRAIN_MODE_HEIGHT_ONLY = 1,
  MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT = 2,
  MLN_CONSTRAIN_MODE_SCREEN = 3,
} mln_constrain_mode;

/** Viewport orientation modes used by mln_map_viewport_options. */
typedef enum mln_viewport_mode : uint32_t {
  MLN_VIEWPORT_MODE_DEFAULT = 0,
  MLN_VIEWPORT_MODE_FLIPPED_Y = 1,
} mln_viewport_mode;

/** Field mask values for mln_map_viewport_options. */
typedef enum mln_map_viewport_option_field : uint32_t {
  MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION = 1U << 0U,
  MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE = 1U << 1U,
  MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE = 1U << 2U,
  MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET = 1U << 3U,
} mln_map_viewport_option_field;

/** Tile LOD algorithms used by mln_map_tile_options. */
typedef enum mln_tile_lod_mode : uint32_t {
  MLN_TILE_LOD_MODE_DEFAULT = 0,
  MLN_TILE_LOD_MODE_DISTANCE = 1,
} mln_tile_lod_mode;

/** Field mask values for mln_map_tile_options. */
typedef enum mln_map_tile_option_field : uint32_t {
  MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA = 1U << 0U,
  MLN_MAP_TILE_OPTION_LOD_MIN_RADIUS = 1U << 1U,
  MLN_MAP_TILE_OPTION_LOD_SCALE = 1U << 2U,
  MLN_MAP_TILE_OPTION_LOD_PITCH_THRESHOLD = 1U << 3U,
  MLN_MAP_TILE_OPTION_LOD_ZOOM_SHIFT = 1U << 4U,
  MLN_MAP_TILE_OPTION_LOD_MODE = 1U << 5U,
} mln_map_tile_option_field;

/** Map rendering modes used when creating a map. */
typedef enum mln_map_mode : uint32_t {
  /** Continuously updates as data arrives and map state changes. */
  MLN_MAP_MODE_CONTINUOUS = 0,
  /** Produces one-off still images of an arbitrary viewport. */
  MLN_MAP_MODE_STATIC = 1,
  /** Produces one-off still images for a single tile. */
  MLN_MAP_MODE_TILE = 2,
} mln_map_mode;

/** Options used when creating a map. */
typedef struct mln_map_options {
  uint32_t size;
  uint32_t width;
  uint32_t height;
  double scale_factor;
  /** One of mln_map_mode. Defaults to MLN_MAP_MODE_CONTINUOUS. */
  uint32_t map_mode;
} mln_map_options;

/** Screen-space point in logical map pixels. */
typedef struct mln_screen_point {
  double x;
  double y;
} mln_screen_point;

/** Screen-space inset in logical map pixels. */
typedef struct mln_edge_insets {
  double top;
  double left;
  double bottom;
  double right;
} mln_edge_insets;

/** Camera fields used for snapshots and camera commands. */
typedef struct mln_camera_options {
  uint32_t size;
  uint32_t fields;
  double latitude;
  double longitude;
  double center_altitude;
  mln_edge_insets padding;
  mln_screen_point anchor;
  double zoom;
  double bearing;
  double pitch;
  double roll;
  double field_of_view;
} mln_camera_options;

/** Cubic easing curve for animated camera transitions. */
typedef struct mln_unit_bezier {
  double x1;
  double y1;
  double x2;
  double y2;
} mln_unit_bezier;

/** Optional animation controls for camera transitions. */
typedef struct mln_animation_options {
  uint32_t size;
  uint32_t fields;
  /**
   * Duration in milliseconds. Must be finite and non-negative. Values that
   * would overflow MapLibre Native's internal duration are invalid.
   */
  double duration_ms;
  /** Average flyTo velocity in screenfuls per second. Must be positive. */
  double velocity;
  /** Peak zoom for flyTo transitions. */
  double min_zoom;
  mln_unit_bezier easing;
} mln_animation_options;

/** Optional fitting controls for camera-for-viewport queries. */
typedef struct mln_camera_fit_options {
  uint32_t size;
  uint32_t fields;
  mln_edge_insets padding;
  double bearing;
  double pitch;
} mln_camera_fit_options;

/** Three-component vector used by free camera options. */
typedef struct mln_vec3 {
  double x;
  double y;
  double z;
} mln_vec3;

/** Quaternion stored as x, y, z, w components. */
typedef struct mln_quaternion {
  double x;
  double y;
  double z;
  double w;
} mln_quaternion;

/** Free camera position and orientation in MapLibre Native camera space. */
typedef struct mln_free_camera_options {
  uint32_t size;
  uint32_t fields;
  mln_vec3 position;
  mln_quaternion orientation;
} mln_free_camera_options;

/** Geographic coordinate in degrees used by map and projection APIs. */
typedef struct mln_lat_lng {
  /** Latitude in degrees. Input latitude must be finite and within [-90, 90].
   */
  double latitude;
  /** Longitude in degrees. Input longitude must be finite. */
  double longitude;
} mln_lat_lng;

/** UTF-8 text view. The pointer may be null only when size is 0. */
typedef struct mln_string_view {
  /** UTF-8 bytes. Null only when size is 0. */
  const char* data;
  size_t size;
} mln_string_view;

typedef struct mln_json_value mln_json_value;
typedef struct mln_geometry mln_geometry;

/** Geometry variant tags used by mln_geometry. */
typedef enum mln_geometry_type : uint32_t {
  MLN_GEOMETRY_TYPE_EMPTY = 0,
  MLN_GEOMETRY_TYPE_POINT = 1,
  MLN_GEOMETRY_TYPE_LINE_STRING = 2,
  MLN_GEOMETRY_TYPE_POLYGON = 3,
  MLN_GEOMETRY_TYPE_MULTI_POINT = 4,
  MLN_GEOMETRY_TYPE_MULTI_LINE_STRING = 5,
  MLN_GEOMETRY_TYPE_MULTI_POLYGON = 6,
  MLN_GEOMETRY_TYPE_GEOMETRY_COLLECTION = 7,
} mln_geometry_type;

/** Coordinate array view. Coordinates are latitude/longitude pairs. */
typedef struct mln_coordinate_span {
  /** Coordinates. Null only when coordinate_count is 0. */
  const mln_lat_lng* coordinates;
  size_t coordinate_count;
} mln_coordinate_span;

/** Polygon ring array view. Each ring is a coordinate span. */
typedef struct mln_polygon_geometry {
  /** Rings. Null only when ring_count is 0. */
  const mln_coordinate_span* rings;
  size_t ring_count;
} mln_polygon_geometry;

/** Multi-line geometry view. Each line is a coordinate span. */
typedef struct mln_multi_line_geometry {
  /** Lines. Null only when line_count is 0. */
  const mln_coordinate_span* lines;
  size_t line_count;
} mln_multi_line_geometry;

/** Multi-polygon geometry view. Each polygon contains ring views. */
typedef struct mln_multi_polygon_geometry {
  /** Polygons. Null only when polygon_count is 0. */
  const mln_polygon_geometry* polygons;
  size_t polygon_count;
} mln_multi_polygon_geometry;

/** Geometry collection view. */
typedef struct mln_geometry_collection {
  /** Child geometries. Null only when geometry_count is 0. */
  const mln_geometry* geometries;
  size_t geometry_count;
} mln_geometry_collection;

/**
 * MapLibre geometry input descriptor graph.
 *
 * Geometry coordinates use mln_lat_lng for consistency with the rest of the C
 * API. They are converted to native geometry points as longitude/latitude.
 * A root geometry descriptor starts at nesting depth 0. Status-returning
 * functions reject geometry collection children past depth 64 with
 * MLN_STATUS_INVALID_ARGUMENT.
 */
typedef struct mln_geometry {
  uint32_t size;
  /** One of mln_geometry_type. */
  uint32_t type;
  union {
    mln_lat_lng point;
    mln_coordinate_span line_string;
    mln_polygon_geometry polygon;
    mln_coordinate_span multi_point;
    mln_multi_line_geometry multi_line_string;
    mln_multi_polygon_geometry multi_polygon;
    mln_geometry_collection geometry_collection;
  } data;
} mln_geometry;

/** JSON-like value variant tags used by mln_json_value. */
typedef enum mln_json_value_type : uint32_t {
  MLN_JSON_VALUE_TYPE_NULL = 0,
  MLN_JSON_VALUE_TYPE_BOOL = 1,
  MLN_JSON_VALUE_TYPE_UINT = 2,
  MLN_JSON_VALUE_TYPE_INT = 3,
  MLN_JSON_VALUE_TYPE_DOUBLE = 4,
  MLN_JSON_VALUE_TYPE_STRING = 5,
  MLN_JSON_VALUE_TYPE_ARRAY = 6,
  MLN_JSON_VALUE_TYPE_OBJECT = 7,
} mln_json_value_type;

/** JSON value array view. */
typedef struct mln_json_array {
  /** Values. Null only when value_count is 0. */
  const mln_json_value* values;
  size_t value_count;
} mln_json_array;

/** JSON object member view. */
typedef struct mln_json_member {
  mln_string_view key;
  /** Value descriptor. Must not be null. */
  const mln_json_value* value;
} mln_json_member;

/** JSON object member array view. */
typedef struct mln_json_object {
  /** Members. Null only when member_count is 0. */
  const mln_json_member* members;
  size_t member_count;
} mln_json_object;

/**
 * JSON-like value input descriptor graph used by feature properties/states.
 *
 * Input functions reject NaN and infinities for double values because JSON and
 * GeoJSON numbers are finite.
 * A root JSON value descriptor starts at nesting depth 0. Status-returning
 * functions reject array/object children past depth 64 with
 * MLN_STATUS_INVALID_ARGUMENT.
 */
typedef struct mln_json_value {
  uint32_t size;
  /** One of mln_json_value_type. */
  uint32_t type;
  union {
    bool bool_value;
    uint64_t uint_value;
    int64_t int_value;
    double double_value;
    mln_string_view string_value;
    mln_json_array array_value;
    mln_json_object object_value;
  } data;
} mln_json_value;

/** Optional fields for mln_feature_state_selector. */
typedef enum mln_feature_state_selector_field : uint32_t {
  MLN_FEATURE_STATE_SELECTOR_SOURCE_LAYER_ID = 1U << 0U,
  MLN_FEATURE_STATE_SELECTOR_FEATURE_ID = 1U << 1U,
  MLN_FEATURE_STATE_SELECTOR_STATE_KEY = 1U << 2U,
} mln_feature_state_selector_field;

/** Feature-state source, feature, and key selector. */
typedef struct mln_feature_state_selector {
  uint32_t size;
  uint32_t fields;
  /** Source ID. Required and borrowed for the duration of the call. */
  mln_string_view source_id;
  /** Optional source layer ID. Required for vector-source disambiguation. */
  mln_string_view source_layer_id;
  /** Optional feature ID string. Required by set/get and optional for remove.
   */
  mln_string_view feature_id;
  /** Optional state key. Used only by remove and requires feature_id. */
  mln_string_view state_key;
} mln_feature_state_selector;

/** Feature identifier variant tags used by mln_feature. */
typedef enum mln_feature_identifier_type : uint32_t {
  MLN_FEATURE_IDENTIFIER_TYPE_NULL = 0,
  MLN_FEATURE_IDENTIFIER_TYPE_UINT = 1,
  MLN_FEATURE_IDENTIFIER_TYPE_INT = 2,
  MLN_FEATURE_IDENTIFIER_TYPE_DOUBLE = 3,
  MLN_FEATURE_IDENTIFIER_TYPE_STRING = 4,
} mln_feature_identifier_type;

/** GeoJSON feature input descriptor graph. */
typedef struct mln_feature {
  uint32_t size;
  /**
   * Geometry descriptor. Must not be null. Use MLN_GEOMETRY_TYPE_EMPTY for an
   * empty geometry.
   */
  const mln_geometry* geometry;
  /** Property member views. May be null only when property_count is 0. */
  const mln_json_member* properties;
  size_t property_count;
  /** One of mln_feature_identifier_type. */
  uint32_t identifier_type;
  union {
    uint64_t uint_value;
    int64_t int_value;
    double double_value;
    mln_string_view string_value;
  } identifier;
} mln_feature;

/** GeoJSON variant tags used by mln_geojson. */
typedef enum mln_geojson_type : uint32_t {
  MLN_GEOJSON_TYPE_GEOMETRY = 1,
  MLN_GEOJSON_TYPE_FEATURE = 2,
  MLN_GEOJSON_TYPE_FEATURE_COLLECTION = 3,
} mln_geojson_type;

/** Feature collection view. */
typedef struct mln_feature_collection {
  /** Features. Null only when feature_count is 0. */
  const mln_feature* features;
  size_t feature_count;
} mln_feature_collection;

/**
 * GeoJSON geometry, feature, or feature collection input descriptor graph.
 * Nested geometry and property descriptors share the 64-depth descriptor limit
 * documented on mln_geometry and mln_json_value.
 */
typedef struct mln_geojson {
  uint32_t size;
  /** One of mln_geojson_type. */
  uint32_t type;
  union {
    /**
     * Geometry descriptor selected by MLN_GEOJSON_TYPE_GEOMETRY. Must not be
     * null.
     */
    const mln_geometry* geometry;
    /**
     * Feature descriptor selected by MLN_GEOJSON_TYPE_FEATURE. Must not be
     * null.
     */
    const mln_feature* feature;
    mln_feature_collection feature_collection;
  } data;
} mln_geojson;

/** Geographic bounds in degrees. */
typedef struct mln_lat_lng_bounds {
  mln_lat_lng southwest;
  mln_lat_lng northeast;
} mln_lat_lng_bounds;

/** Optional map camera constraint fields. */
typedef struct mln_bound_options {
  uint32_t size;
  uint32_t fields;
  mln_lat_lng_bounds bounds;
  double min_zoom;
  double max_zoom;
  double min_pitch;
  double max_pitch;
} mln_bound_options;

/** Tile-pyramid offline region definition. */
typedef struct mln_offline_tile_pyramid_region_definition {
  uint32_t size;
  /** Style URL. Copied during region creation. */
  const char* style_url;
  mln_lat_lng_bounds bounds;
  double min_zoom;
  /**
   * Maximum zoom. Positive infinity follows MapLibre Native behavior and lets
   * each tile source use its own maximum zoom.
   */
  double max_zoom;
  float pixel_ratio;
  bool include_ideographs;
} mln_offline_tile_pyramid_region_definition;

/** Geometry offline region definition. */
typedef struct mln_offline_geometry_region_definition {
  uint32_t size;
  /** Style URL. Copied during region creation. */
  const char* style_url;
  /** Geometry descriptor. Borrowed for the duration of region creation. */
  const mln_geometry* geometry;
  double min_zoom;
  /**
   * Maximum zoom. Positive infinity follows MapLibre Native behavior and lets
   * each tile source use its own maximum zoom.
   */
  double max_zoom;
  float pixel_ratio;
  bool include_ideographs;
} mln_offline_geometry_region_definition;

/** Tagged offline region definition. */
typedef struct mln_offline_region_definition {
  uint32_t size;
  /** One of mln_offline_region_definition_type. */
  uint32_t type;
  union {
    mln_offline_tile_pyramid_region_definition tile_pyramid;
    mln_offline_geometry_region_definition geometry;
  } data;
} mln_offline_region_definition;

/** Region data view returned from a snapshot or list handle. */
typedef struct mln_offline_region_info {
  uint32_t size;
  mln_offline_region_id id;
  mln_offline_region_definition definition;
  /** Metadata bytes. Valid until the owner snapshot/list is destroyed.
   */
  const uint8_t* metadata;
  size_t metadata_size;
} mln_offline_region_info;

/**
 * Creates a tile-pyramid offline region.
 *
 * The returned snapshot owns copied region data. Destroy it with
 * mln_offline_region_snapshot_destroy(). Input strings, geometry descriptors,
 * and metadata are borrowed for the duration of this call and are not retained.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, definition is
 *   null or invalid, metadata is null with a non-zero size, out_region is null,
 *   or *out_region is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_create(
  mln_runtime* runtime, const mln_offline_region_definition* definition,
  const uint8_t* metadata, size_t metadata_size,
  mln_offline_region_snapshot** out_region
) MLN_NOEXCEPT;

/**
 * Gets an offline region snapshot by ID.
 *
 * On success, out_found indicates whether the region exists. When out_found is
 * false, *out_region remains null.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, out_region is
 *   null, *out_region is not null, or out_found is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_get(
  mln_runtime* runtime, mln_offline_region_id region_id,
  mln_offline_region_snapshot** out_region, bool* out_found
) MLN_NOEXCEPT;

/**
 * Lists offline region snapshots in the runtime database.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live,
 *   out_regions is null, or *out_regions is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_regions_list(
  mln_runtime* runtime, mln_offline_region_list** out_regions
) MLN_NOEXCEPT;

/**
 * Merges offline regions from another database path.
 *
 * The returned list owns the exact region list reported by MapLibre Native's
 * merge callback. The side database may be upgraded in place by native code and
 * must be writable when native merge requires it.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live,
 *   side_database_path is null, out_regions is null, or *out_regions is not
 *   null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_regions_merge_database(
  mln_runtime* runtime, const char* side_database_path,
  mln_offline_region_list** out_regions
) MLN_NOEXCEPT;

/**
 * Updates opaque binary metadata for an offline region.
 *
 * The returned snapshot contains the updated metadata.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, metadata is
 *   null with a non-zero size, out_region is null, *out_region is not null, or
 *   no region exists for id.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_update_metadata(
  mln_runtime* runtime, mln_offline_region_id region_id,
  const uint8_t* metadata, size_t metadata_size,
  mln_offline_region_snapshot** out_region
) MLN_NOEXCEPT;

/**
 * Gets the current completed/download status for an offline region.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, out_status is
 *   null, out_status->size is too small, or no region exists for id.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_get_status(
  mln_runtime* runtime, mln_offline_region_id region_id,
  mln_offline_region_status* out_status
) MLN_NOEXCEPT;

/**
 * Enables or disables runtime events for an offline region.
 *
 * Observer callbacks are copied into runtime events. Disabling observation also
 * discards queued events for this region.
 *
 * Returns:
 * - MLN_STATUS_OK when the observer command was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, or no region
 *   exists for region_id.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_set_observed(
  mln_runtime* runtime, mln_offline_region_id region_id, bool observed
) MLN_NOEXCEPT;

/**
 * Sets an offline region's native download state.
 *
 * Register observation separately with
 * mln_runtime_offline_region_set_observed() to receive progress and error
 * events.
 *
 * Returns:
 * - MLN_STATUS_OK when the state command was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, state is not
 *   a mln_offline_region_download_state value, or no region exists for
 *   region_id.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_set_download_state(
  mln_runtime* runtime, mln_offline_region_id region_id, uint32_t state
) MLN_NOEXCEPT;

/**
 * Invalidates cached resources for an offline region.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, or no region
 *   exists for id.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_invalidate(
  mln_runtime* runtime, mln_offline_region_id region_id
) MLN_NOEXCEPT;

/**
 * Deletes an offline region.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, or no region
 *   exists for id.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when a native database error or internal exception
 *   is converted to status.
 */
MLN_API mln_status mln_runtime_offline_region_delete(
  mln_runtime* runtime, mln_offline_region_id region_id
) MLN_NOEXCEPT;

/**
 * Copies a region data view out of a snapshot handle.
 *
 * On success, out_info receives pointers into snapshot-owned storage. Those
 * pointers remain valid until the snapshot is destroyed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when snapshot is null or not live, out_info is
 *   null, or out_info->size is too small.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_offline_region_snapshot_get(
  const mln_offline_region_snapshot* snapshot, mln_offline_region_info* out_info
) MLN_NOEXCEPT;

/** Destroys an offline region snapshot handle. Null is accepted as a no-op. */
MLN_API void mln_offline_region_snapshot_destroy(
  mln_offline_region_snapshot* snapshot
) MLN_NOEXCEPT;

/**
 * Gets the number of regions in a list handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when list is null or not live, or out_count is
 *   null.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_offline_region_list_count(
  const mln_offline_region_list* list, size_t* out_count
) MLN_NOEXCEPT;

/**
 * Copies a region data view for one list entry.
 *
 * On success, out_info receives pointers into list-owned storage. Those
 * pointers remain valid until the list is destroyed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when list is null or not live, index is out of
 *   range, out_info is null, or out_info->size is too small.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_offline_region_list_get(
  const mln_offline_region_list* list, size_t index,
  mln_offline_region_info* out_info
) MLN_NOEXCEPT;

/** Destroys an offline region list handle. Null is accepted as a no-op. */
MLN_API void mln_offline_region_list_destroy(
  mln_offline_region_list* list
) MLN_NOEXCEPT;

/**
 * Lower-level Spherical Mercator projected-meter coordinate.
 *
 * Map coordinate conversion APIs use mln_lat_lng. This type is only for
 * Mercator helper functions.
 */
typedef struct mln_projected_meters {
  /** Distance measured northward from the equator, in meters. */
  double northing;
  /** Distance measured eastward from the prime meridian, in meters. */
  double easting;
} mln_projected_meters;

/**
 * MapLibre axonometric rendering options used for snapshots and commands.
 *
 * MapLibre Native names this native type ProjectionMode. It controls the live
 * map render transform, not the geographic coordinate model.
 */
typedef struct mln_projection_mode {
  uint32_t size;
  uint32_t fields;
  /** Enables a non-perspective axonometric render transform. */
  bool axonometric;
  /** Native x-skew factor used by the axonometric transform. */
  double x_skew;
  /** Native y-skew factor used by the axonometric transform. */
  double y_skew;
} mln_projection_mode;

/** Live map viewport and render-transform controls. */
typedef struct mln_map_viewport_options {
  uint32_t size;
  uint32_t fields;
  /** One of mln_north_orientation. */
  uint32_t north_orientation;
  /** One of mln_constrain_mode. */
  uint32_t constrain_mode;
  /** One of mln_viewport_mode. */
  uint32_t viewport_mode;
  mln_edge_insets frustum_offset;
} mln_map_viewport_options;

/** Tile prefetch and LOD tuning controls. */
typedef struct mln_map_tile_options {
  uint32_t size;
  uint32_t fields;
  /** Native uint8_t prefetch zoom delta. */
  uint32_t prefetch_zoom_delta;
  double lod_min_radius;
  double lod_scale;
  double lod_pitch_threshold;
  double lod_zoom_shift;
  /** One of mln_tile_lod_mode. */
  uint32_t lod_mode;
} mln_map_tile_options;

/**
 * Returns map options initialized for this C API version.
 */
MLN_API mln_map_options mln_map_options_default(void) MLN_NOEXCEPT;

/**
 * Creates a map handle on the runtime owner thread.
 *
 * On success, the runtime owner thread becomes the map owner thread.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, out_map is
 *   null, *out_map is not null, or options are invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_create(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) MLN_NOEXCEPT;

/**
 * Requests a repaint for a continuous map.
 *
 * Continuous maps also invalidate automatically when style data, resources,
 * camera, or transitions change. Ask attached render targets to process the
 * latest update when MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE is reported.
 * Repaint requests do not produce
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED or
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED events.
 *
 * Returns:
 * - MLN_STATUS_OK when the request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_INVALID_STATE when map is not in MLN_MAP_MODE_CONTINUOUS.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_request_repaint(mln_map* map) MLN_NOEXCEPT;

/**
 * Requests one still image for a static or tile map.
 *
 * Pump the runtime and poll runtime events for this map until
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED or
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED is reported. While the request is
 * pending, process each MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE event
 * from this map. Render targets use mln_render_session_render_update(). Surface
 * targets present directly. A render-update
 * call can return MLN_STATUS_INVALID_STATE before the next update is available;
 * keep pumping and polling in that case. After
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED, use the latest successful texture
 * update when the host needs image bytes or a backend texture.
 *
 * Returns:
 * - MLN_STATUS_OK when the request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_INVALID_STATE when map is not in MLN_MAP_MODE_STATIC or
 *   MLN_MAP_MODE_TILE, or when a still-image request is already pending.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_request_still_image(mln_map* map) MLN_NOEXCEPT;

/**
 * Destroys a map handle on its owner thread.
 *
 * The map must not have an attached render target session.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not a live map handle.
 * - MLN_STATUS_INVALID_STATE when map still has an attached render target
 *   session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_destroy(mln_map* map) MLN_NOEXCEPT;

/**
 * Loads a style URL through MapLibre Native style APIs.
 *
 * This is a map command. The return status reports synchronous acceptance or
 * failure. Later native success and failure are reported through runtime
 * events.
 *
 * Returns:
 * - MLN_STATUS_OK when the load request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null, not live, or url is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when a synchronous native error is reported or an
 *   internal exception is converted to status.
 */
MLN_API mln_status
mln_map_set_style_url(mln_map* map, const char* url) MLN_NOEXCEPT;

/**
 * Loads inline style JSON through MapLibre Native style APIs.
 *
 * This is a map command. The return status reports synchronous acceptance or
 * failure. Later native success and failure are reported through runtime
 * events. Malformed JSON can fail synchronously and still enqueue a
 * loading-failed event.
 *
 * Returns:
 * - MLN_STATUS_OK when the load request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null, not live, or json is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when a synchronous native error is reported or an
 *   internal exception is converted to status.
 */
MLN_API mln_status
mln_map_set_style_json(mln_map* map, const char* json) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_MAP_H
