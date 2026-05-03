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

/** Field mask values for mln_style_tile_source_options. */
typedef enum mln_style_tile_source_option_field : uint32_t {
  MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM = 1U << 0U,
  MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM = 1U << 1U,
  MLN_STYLE_TILE_SOURCE_OPTION_ATTRIBUTION = 1U << 2U,
  MLN_STYLE_TILE_SOURCE_OPTION_SCHEME = 1U << 3U,
  MLN_STYLE_TILE_SOURCE_OPTION_BOUNDS = 1U << 4U,
  MLN_STYLE_TILE_SOURCE_OPTION_TILE_SIZE = 1U << 5U,
  MLN_STYLE_TILE_SOURCE_OPTION_VECTOR_ENCODING = 1U << 6U,
} mln_style_tile_source_option_field;

/** Tile URL coordinate scheme values used by mln_style_tile_source_options. */
typedef enum mln_style_tile_scheme : uint32_t {
  MLN_STYLE_TILE_SCHEME_XYZ = 0,
  MLN_STYLE_TILE_SCHEME_TMS = 1,
} mln_style_tile_scheme;

/** Vector tile encoding values used by mln_style_tile_source_options. */
typedef enum mln_style_vector_tile_encoding : uint32_t {
  MLN_STYLE_VECTOR_TILE_ENCODING_MVT = 0,
  MLN_STYLE_VECTOR_TILE_ENCODING_MLT = 1,
} mln_style_vector_tile_encoding;

/** Field mask values for mln_style_image_options. */
typedef enum mln_style_image_option_field : uint32_t {
  MLN_STYLE_IMAGE_OPTION_PIXEL_RATIO = 1U << 0U,
  MLN_STYLE_IMAGE_OPTION_SDF = 1U << 1U,
} mln_style_image_option_field;

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

/** Options for vector and raster tile sources. */
typedef struct mln_style_tile_source_options {
  uint32_t size;
  uint32_t fields;
  double min_zoom;
  double max_zoom;
  mln_string_view attribution;
  /** One of mln_style_tile_scheme. Defaults to MLN_STYLE_TILE_SCHEME_XYZ. */
  uint32_t scheme;
  mln_lat_lng_bounds bounds;
  /** Raster tile size in pixels. Defaults to 512. */
  uint32_t tile_size;
  /** One of mln_style_vector_tile_encoding. Defaults to MVT. */
  uint32_t vector_encoding;
} mln_style_tile_source_options;

/** Caller-owned premultiplied RGBA8 image pixels. */
typedef struct mln_premultiplied_rgba8_image {
  uint32_t size;
  uint32_t width;
  uint32_t height;
  /** Bytes per image row. Must be at least width * 4. */
  uint32_t stride;
  /** Premultiplied RGBA8 pixels. Must not be null for a non-empty image. */
  const uint8_t* pixels;
  /** Available bytes at pixels. */
  size_t byte_length;
} mln_premultiplied_rgba8_image;

/** Options for runtime style images. */
typedef struct mln_style_image_options {
  uint32_t size;
  uint32_t fields;
  /** Sprite pixel ratio. Defaults to 1. */
  float pixel_ratio;
  /** Whether the image is a signed distance field icon. Defaults to false. */
  bool sdf;
} mln_style_image_options;

/** Fixed metadata for one runtime style image. */
typedef struct mln_style_image_info {
  uint32_t size;
  uint32_t width;
  uint32_t height;
  /** Native copied images are exposed as tightly packed premultiplied RGBA8. */
  uint32_t stride;
  size_t byte_length;
  float pixel_ratio;
  bool sdf;
} mln_style_image_info;

/** Returns default tile source options. */
MLN_API mln_style_tile_source_options
mln_style_tile_source_options_default(void) MLN_NOEXCEPT;

/** Returns a default premultiplied RGBA8 image descriptor. */
MLN_API mln_premultiplied_rgba8_image
mln_premultiplied_rgba8_image_default(void) MLN_NOEXCEPT;

/** Returns default runtime style image options. */
MLN_API mln_style_image_options
mln_style_image_options_default(void) MLN_NOEXCEPT;

/** Returns default runtime style image metadata. */
MLN_API mln_style_image_info mln_style_image_info_default(void) MLN_NOEXCEPT;

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
 * Adds a GeoJSON source with URL data.
 *
 * source_id and url are borrowed for the call. The source loads GeoJSON from
 * url through MapLibre Native's resource system.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id or url
 *   is invalid or empty, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_geojson_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) MLN_NOEXCEPT;

/**
 * Adds a GeoJSON source with inline data.
 *
 * source_id and data are borrowed for the call. The accepted GeoJSON descriptor
 * is copied into MapLibre Native before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, data is null or invalid, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_geojson_source_data(
  mln_map* map, mln_string_view source_id, const mln_geojson* data
) MLN_NOEXCEPT;

/**
 * Updates one GeoJSON source to load data from a URL.
 *
 * source_id and url are borrowed for the call.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id or url
 *   is invalid or empty, the source does not exist, or the source is not a
 *   GeoJSON source.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_geojson_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) MLN_NOEXCEPT;

/**
 * Updates one GeoJSON source with inline data.
 *
 * source_id and data are borrowed for the call. The accepted GeoJSON descriptor
 * is copied into MapLibre Native before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, data is null or invalid, the source does not exist, or
 *   the source is not a GeoJSON source.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_geojson_source_data(
  mln_map* map, mln_string_view source_id, const mln_geojson* data
) MLN_NOEXCEPT;

/**
 * Adds a vector source with a TileJSON URL.
 *
 * source_id and url are borrowed for the call. options may be null for
 * defaults. For URL sources, min_zoom, max_zoom, and vector_encoding override
 * values from the loaded TileJSON when their field bits are set.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id or url
 *   is invalid or empty, options is invalid, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_vector_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) MLN_NOEXCEPT;

/**
 * Adds a vector source with inline tile URLs.
 *
 * source_id and tile URL views are borrowed for the call. The function copies
 * accepted strings into MapLibre Native before return. options may be null for
 * defaults.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, tile URLs are null, empty, or invalid, options is
 *   invalid, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_vector_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) MLN_NOEXCEPT;

/**
 * Adds a raster source with a TileJSON URL.
 *
 * source_id and url are borrowed for the call. options may be null for
 * defaults. For URL sources, only tile_size is used when its field bit is set.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id or url
 *   is invalid or empty, options is invalid, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_raster_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) MLN_NOEXCEPT;

/**
 * Adds a raster source with inline tile URLs.
 *
 * source_id and tile URL views are borrowed for the call. The function copies
 * accepted strings into MapLibre Native before return. options may be null for
 * defaults.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, tile URLs are null, empty, or invalid, options is
 *   invalid, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_raster_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) MLN_NOEXCEPT;

/**
 * Sets one runtime style image.
 *
 * image_id, image, and image pixels are borrowed for the call. The function
 * copies accepted pixel bytes into the current style before return. If image_id
 * already exists, the native image is replaced.
 *
 * Runtime style images belong to the current style. Loading another style URL
 * or JSON document drops images that were added to the previous style.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, image_id is
 *   invalid or empty, image or options is invalid, image pixels are null, image
 *   dimensions or stride are invalid, or image byte_length is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_style_image(
  mln_map* map, mln_string_view image_id,
  const mln_premultiplied_rgba8_image* image,
  const mln_style_image_options* options
) MLN_NOEXCEPT;

/**
 * Removes one runtime style image by ID.
 *
 * image_id is borrowed for the call. On success, out_removed reports whether an
 * image existed and was removed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, image_id is
 *   invalid or empty, or out_removed is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_remove_style_image(
  mln_map* map, mln_string_view image_id, bool* out_removed
) MLN_NOEXCEPT;

/**
 * Reports whether a runtime style image ID exists.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, image_id is
 *   invalid or empty, or out_exists is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_style_image_exists(
  mln_map* map, mln_string_view image_id, bool* out_exists
) MLN_NOEXCEPT;

/**
 * Copies fixed metadata for one runtime style image.
 *
 * On success, out_found reports whether image_id exists. When not found,
 * out_info receives default image metadata.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, image_id is
 *   invalid or empty, out_info is null, out_info->size is too small, or
 *   out_found is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_style_image_info(
  mln_map* map, mln_string_view image_id, mln_style_image_info* out_info,
  bool* out_found
) MLN_NOEXCEPT;

/**
 * Copies one runtime style image as tightly packed premultiplied RGBA8 pixels.
 *
 * image_id is borrowed for the call. out_pixels may be null only when
 * pixel_capacity is 0. On success, out_byte_length receives the required byte
 * length. When out_found is false, out_byte_length receives 0. If
 * pixel_capacity is too small for a present image, out_byte_length still
 * receives the required byte length and the function returns
 * MLN_STATUS_INVALID_ARGUMENT.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, image_id is
 *   invalid or empty, out_pixels is null with non-zero capacity, pixel_capacity
 *   is too small for a present image, out_byte_length is null, or out_found is
 *   null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_copy_style_image_premultiplied_rgba8(
  mln_map* map, mln_string_view image_id, uint8_t* out_pixels,
  size_t pixel_capacity, size_t* out_byte_length, bool* out_found
) MLN_NOEXCEPT;

/**
 * Adds an image source that loads its image from a URL.
 *
 * source_id, coordinates, and url are borrowed for the call. coordinates must
 * contain exactly four coordinates in top-left, top-right, bottom-right,
 * bottom-left order. The function copies accepted strings and coordinates into
 * the current style before return. Later URL load or decode failures are
 * reported through runtime events.
 *
 * Image sources belong to the current style. Loading another style URL or JSON
 * document drops sources that were added to the previous style.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id or url
 *   is invalid or empty, coordinates is null or invalid, coordinate_count is
 * not 4, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_image_source_url(
  mln_map* map, mln_string_view source_id, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_string_view url
) MLN_NOEXCEPT;

/**
 * Adds an image source with inline image pixels.
 *
 * source_id, coordinates, image, and image pixels are borrowed for the call.
 * coordinates must contain exactly four coordinates in top-left, top-right,
 * bottom-right, bottom-left order. The function copies accepted coordinates and
 * pixels into the current style before return.
 *
 * Image sources belong to the current style. Loading another style URL or JSON
 * document drops sources that were added to the previous style.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, coordinates is null or invalid, coordinate_count is not
 * 4, image is invalid, image pixels are null, image dimensions or stride are
 *   invalid, image byte_length is too small, or the source ID already exists.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_add_image_source_image(
  mln_map* map, mln_string_view source_id, const mln_lat_lng* coordinates,
  size_t coordinate_count, const mln_premultiplied_rgba8_image* image
) MLN_NOEXCEPT;

/**
 * Updates an image source to load its image from a URL.
 *
 * source_id and url are borrowed for the call. Later URL load or decode
 * failures are reported through runtime events.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id or url
 *   is invalid or empty, the source does not exist, or the source is not an
 *   image source.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_image_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) MLN_NOEXCEPT;

/**
 * Updates an image source with inline image pixels.
 *
 * source_id, image, and image pixels are borrowed for the call. The function
 * copies accepted pixels into MapLibre Native before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, image is invalid, image pixels are null, image dimensions
 *   or stride are invalid, image byte_length is too small, the source does not
 *   exist, or the source is not an image source.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_image_source_image(
  mln_map* map, mln_string_view source_id,
  const mln_premultiplied_rgba8_image* image
) MLN_NOEXCEPT;

/**
 * Updates image source coordinates.
 *
 * coordinates is borrowed for the call and must contain exactly four
 * coordinates in top-left, top-right, bottom-right, bottom-left order. The
 * function copies accepted coordinates into MapLibre Native before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, coordinates is null or invalid, coordinate_count is not
 * 4, the source does not exist, or the source is not an image source.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_image_source_coordinates(
  mln_map* map, mln_string_view source_id, const mln_lat_lng* coordinates,
  size_t coordinate_count
) MLN_NOEXCEPT;

/**
 * Copies image source coordinates.
 *
 * On success, out_found reports whether source_id exists. When found,
 * out_coordinate_count receives 4. If coordinate_capacity is less than 4,
 * out_coordinate_count still receives 4 and the function returns
 * MLN_STATUS_INVALID_ARGUMENT.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, source_id is
 *   invalid or empty, out_coordinates is null with non-zero capacity,
 *   coordinate_capacity is too small for a found source, out_coordinate_count
 * is null, out_found is null, or the source exists and is not an image source.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_image_source_coordinates(
  mln_map* map, mln_string_view source_id, mln_lat_lng* out_coordinates,
  size_t coordinate_capacity, size_t* out_coordinate_count, bool* out_found
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
 * Sets the style light from a style-spec light JSON object.
 *
 * light_json is borrowed for the call. The function parses and copies the
 * accepted light into MapLibre Native before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, light_json is
 *   null or invalid, or the light JSON cannot be converted.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_style_light_json(
  mln_map* map, const mln_json_value* light_json
) MLN_NOEXCEPT;

/**
 * Sets one style light property using its MapLibre style-spec property name.
 *
 * property_name and value are borrowed for the call. value is a style-spec JSON
 * value descriptor. The function parses and copies the accepted value into
 * MapLibre Native's typed light property storage before return.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, property_name is
 *   invalid or empty, value is null or invalid, the property name is unknown,
 *   or the property value cannot be converted for that property.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_style_light_property(
  mln_map* map, mln_string_view property_name, const mln_json_value* value
) MLN_NOEXCEPT;

/**
 * Copies one style light property as a style-spec JSON value snapshot.
 *
 * On success, *out_value receives an owned snapshot handle. Use
 * mln_json_snapshot_get() to borrow its root JSON value. Destroy the snapshot
 * with mln_json_snapshot_destroy(). Undefined native style light properties
 * return null snapshots.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, property_name is
 *   invalid or empty, out_value is null, or *out_value is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_style_light_property(
  mln_map* map, mln_string_view property_name, mln_json_snapshot** out_value
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
