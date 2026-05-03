#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/gfx/rendering_stats.hpp>
#include <mbgl/map/bound_options.hpp>
#include <mbgl/map/camera.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/map/map_observer.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/map/map_projection.hpp>
#include <mbgl/map/mode.hpp>
#include <mbgl/map/projection_mode.hpp>
#include <mbgl/renderer/renderer_frontend.hpp>
#include <mbgl/renderer/renderer_observer.hpp>
#include <mbgl/renderer/update_parameters.hpp>
#include <mbgl/style/conversion.hpp>
#include <mbgl/style/conversion/layer.hpp>   // IWYU pragma: keep
#include <mbgl/style/conversion/light.hpp>   // IWYU pragma: keep
#include <mbgl/style/conversion/source.hpp>  // IWYU pragma: keep
#include <mbgl/style/conversion_impl.hpp>
#include <mbgl/style/image.hpp>
#include <mbgl/style/layer.hpp>
#include <mbgl/style/light.hpp>
#include <mbgl/style/source.hpp>
#include <mbgl/style/sources/geojson_source.hpp>
#include <mbgl/style/sources/raster_source.hpp>
#include <mbgl/style/sources/vector_source.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/style/style_property.hpp>
#include <mbgl/style/types.hpp>
#include <mbgl/tile/tile_id.hpp>
#include <mbgl/tile/tile_operation.hpp>
#include <mbgl/util/chrono.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/feature.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/image.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/util/range.hpp>
#include <mbgl/util/size.hpp>
#include <mbgl/util/tileset.hpp>
#include <mbgl/util/vectors.hpp>

#include "map/map.hpp"

#include "diagnostics/diagnostics.hpp"
#include "geojson/geojson.hpp"
#include "maplibre_native_c.h"
#include "runtime/runtime.hpp"
#include "style/style_value.hpp"

struct mln_style_id_list {
  std::vector<std::string> ids;
};

namespace {
using MapRegistry = std::unordered_map<mln_map*, std::unique_ptr<mln_map>>;
using ProjectionRegistry =
  std::unordered_map<mln_map_projection*, std::unique_ptr<mln_map_projection>>;
using StyleIdListRegistry = std::unordered_map<
  const mln_style_id_list*, std::unique_ptr<mln_style_id_list>>;

constexpr auto default_map_width = uint32_t{256};
constexpr auto default_map_height = uint32_t{256};
constexpr double default_scale_factor = 1.0;

auto map_registry_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto map_registry() -> MapRegistry& {
  static MapRegistry value;
  return value;
}

auto map_projection_registry_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto map_projection_registry() -> ProjectionRegistry& {
  static ProjectionRegistry value;
  return value;
}

auto style_id_list_registry_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto style_id_list_registry() -> StyleIdListRegistry& {
  static StyleIdListRegistry value;
  return value;
}

auto validate_string_view(mln_string_view string, const char* name) -> bool {
  if (string.size > 0 && string.data == nullptr) {
    auto message = std::string{name} + " data must not be null";
    mln::core::set_thread_error(message.c_str());
    return false;
  }
  return true;
}

auto string_from_view(mln_string_view string) -> std::string {
  if (string.size == 0) {
    return {};
  }
  return std::string{string.data, string.size};
}

auto string_view_from_string(const std::string& string) -> mln_string_view {
  return mln_string_view{.data = string.data(), .size = string.size()};
}

auto string_view_from_literal(const char* string) -> mln_string_view {
  return mln_string_view{.data = string, .size = std::strlen(string)};
}

auto validate_lat_lng_bounds(mln_lat_lng_bounds bounds) -> mln_status;
auto to_native_lat_lng_bounds(mln_lat_lng_bounds bounds) -> mbgl::LatLngBounds;

auto to_c_source_type(mbgl::style::SourceType type) -> uint32_t {
  switch (type) {
    case mbgl::style::SourceType::Vector:
      return MLN_STYLE_SOURCE_TYPE_VECTOR;
    case mbgl::style::SourceType::Raster:
      return MLN_STYLE_SOURCE_TYPE_RASTER;
    case mbgl::style::SourceType::RasterDEM:
      return MLN_STYLE_SOURCE_TYPE_RASTER_DEM;
    case mbgl::style::SourceType::GeoJSON:
      return MLN_STYLE_SOURCE_TYPE_GEOJSON;
    case mbgl::style::SourceType::Video:
      return MLN_STYLE_SOURCE_TYPE_VIDEO;
    case mbgl::style::SourceType::Annotations:
      return MLN_STYLE_SOURCE_TYPE_ANNOTATIONS;
    case mbgl::style::SourceType::Image:
      return MLN_STYLE_SOURCE_TYPE_IMAGE;
    case mbgl::style::SourceType::CustomVector:
      return MLN_STYLE_SOURCE_TYPE_CUSTOM_VECTOR;
  }
  return MLN_STYLE_SOURCE_TYPE_UNKNOWN;
}

auto has_tile_source_option(
  const mln_style_tile_source_options& options, uint32_t field
) -> bool {
  return (options.fields & field) != 0U;
}

auto validate_zoom_option(double zoom, const char* name) -> mln_status {
  if (!std::isfinite(zoom) || zoom < 0.0 || zoom > 255.0) {
    auto message = std::string{name} + " must be finite and within [0, 255]";
    mln::core::set_thread_error(message.c_str());
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_tile_source_option_header(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (options.size < sizeof(mln_style_tile_source_options)) {
    mln::core::set_thread_error(
      "mln_style_tile_source_options.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM) |
    MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM |
    MLN_STYLE_TILE_SOURCE_OPTION_ATTRIBUTION |
    MLN_STYLE_TILE_SOURCE_OPTION_SCHEME | MLN_STYLE_TILE_SOURCE_OPTION_BOUNDS |
    MLN_STYLE_TILE_SOURCE_OPTION_TILE_SIZE |
    MLN_STYLE_TILE_SOURCE_OPTION_VECTOR_ENCODING;
  if ((options.fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_style_tile_source_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_tile_source_zoom_options(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM)) {
    const auto status = validate_zoom_option(options.min_zoom, "min_zoom");
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if (has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM)) {
    const auto status = validate_zoom_option(options.max_zoom, "max_zoom");
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if (
    has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM) &&
    has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM) &&
    options.min_zoom > options.max_zoom
  ) {
    mln::core::set_thread_error(
      "min_zoom must be less than or equal to max_zoom"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_tile_source_attribution_option(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (!has_tile_source_option(
        options, MLN_STYLE_TILE_SOURCE_OPTION_ATTRIBUTION
      )) {
    return MLN_STATUS_OK;
  }
  return validate_string_view(options.attribution, "attribution")
           ? MLN_STATUS_OK
           : MLN_STATUS_INVALID_ARGUMENT;
}

auto validate_tile_source_scheme_option(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (!has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_SCHEME)) {
    return MLN_STATUS_OK;
  }
  switch (options.scheme) {
    case MLN_STYLE_TILE_SCHEME_XYZ:
    case MLN_STYLE_TILE_SCHEME_TMS:
      return MLN_STATUS_OK;
    default:
      mln::core::set_thread_error("scheme is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }
}

auto validate_tile_source_bounds_option(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (!has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_BOUNDS)) {
    return MLN_STATUS_OK;
  }
  return validate_lat_lng_bounds(options.bounds);
}

auto validate_tile_source_tile_size_option(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (!has_tile_source_option(
        options, MLN_STYLE_TILE_SOURCE_OPTION_TILE_SIZE
      )) {
    return MLN_STATUS_OK;
  }
  if (options.tile_size == 0 || options.tile_size > 65535U) {
    mln::core::set_thread_error("tile_size must be within [1, 65535]");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_tile_source_vector_encoding_option(
  const mln_style_tile_source_options& options
) -> mln_status {
  if (!has_tile_source_option(
        options, MLN_STYLE_TILE_SOURCE_OPTION_VECTOR_ENCODING
      )) {
    return MLN_STATUS_OK;
  }
  switch (options.vector_encoding) {
    case MLN_STYLE_VECTOR_TILE_ENCODING_MVT:
    case MLN_STYLE_VECTOR_TILE_ENCODING_MLT:
      return MLN_STATUS_OK;
    default:
      mln::core::set_thread_error("vector_encoding is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }
}

auto validate_tile_source_options(const mln_style_tile_source_options* options)
  -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }
  for (const auto validator : {
         validate_tile_source_option_header,
         validate_tile_source_zoom_options,
         validate_tile_source_attribution_option,
         validate_tile_source_scheme_option,
         validate_tile_source_bounds_option,
         validate_tile_source_tile_size_option,
         validate_tile_source_vector_encoding_option,
       }) {
    const auto status = validator(*options);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  return MLN_STATUS_OK;
}

auto effective_tile_source_options(const mln_style_tile_source_options* options)
  -> mln_style_tile_source_options {
  auto result = mln::core::style_tile_source_options_default();
  if (options == nullptr) {
    return result;
  }

  result.fields = options->fields;
  if (has_tile_source_option(*options, MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM)) {
    result.min_zoom = options->min_zoom;
  }
  if (has_tile_source_option(*options, MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM)) {
    result.max_zoom = options->max_zoom;
  }
  if (
    has_tile_source_option(*options, MLN_STYLE_TILE_SOURCE_OPTION_ATTRIBUTION)
  ) {
    result.attribution = options->attribution;
  }
  if (has_tile_source_option(*options, MLN_STYLE_TILE_SOURCE_OPTION_SCHEME)) {
    result.scheme = options->scheme;
  }
  if (has_tile_source_option(*options, MLN_STYLE_TILE_SOURCE_OPTION_BOUNDS)) {
    result.bounds = options->bounds;
  }
  if (
    has_tile_source_option(*options, MLN_STYLE_TILE_SOURCE_OPTION_TILE_SIZE)
  ) {
    result.tile_size = options->tile_size;
  }
  if (
    has_tile_source_option(
      *options, MLN_STYLE_TILE_SOURCE_OPTION_VECTOR_ENCODING
    )
  ) {
    result.vector_encoding = options->vector_encoding;
  }
  return result;
}

auto to_native_tile_scheme(uint32_t scheme) -> mbgl::Tileset::Scheme {
  return scheme == MLN_STYLE_TILE_SCHEME_TMS ? mbgl::Tileset::Scheme::TMS
                                             : mbgl::Tileset::Scheme::XYZ;
}

auto to_native_vector_encoding(uint32_t encoding)
  -> mbgl::Tileset::VectorEncoding {
  return encoding == MLN_STYLE_VECTOR_TILE_ENCODING_MLT
           ? mbgl::Tileset::VectorEncoding::MLT
           : mbgl::Tileset::VectorEncoding::Mapbox;
}

auto validate_tile_urls(const mln_string_view* tiles, size_t tile_count)
  -> mln_status {
  if (tile_count == 0) {
    mln::core::set_thread_error("tile_count must be greater than 0");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (tiles == nullptr) {
    mln::core::set_thread_error("tiles must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  for (const auto tile : std::span<const mln_string_view>{tiles, tile_count}) {
    if (!validate_string_view(tile, "tile URL")) {
      return MLN_STATUS_INVALID_ARGUMENT;
    }
    if (tile.size == 0) {
      mln::core::set_thread_error("tile URLs must not be empty");
      return MLN_STATUS_INVALID_ARGUMENT;
    }
  }
  return MLN_STATUS_OK;
}

auto to_native_tile_urls(const mln_string_view* tiles, size_t tile_count)
  -> std::vector<std::string> {
  auto result = std::vector<std::string>{};
  result.reserve(tile_count);
  for (const auto tile : std::span<const mln_string_view>{tiles, tile_count}) {
    result.push_back(string_from_view(tile));
  }
  return result;
}

auto to_native_tileset(
  const mln_string_view* tiles, size_t tile_count,
  const mln_style_tile_source_options& options, bool vector_source
) -> std::optional<mbgl::Tileset> {
  if (options.min_zoom > options.max_zoom) {
    mln::core::set_thread_error(
      "effective min_zoom must be less than or equal to max_zoom"
    );
    return std::nullopt;
  }

  auto tileset = mbgl::Tileset{
    to_native_tile_urls(tiles, tile_count),
    mbgl::Range<uint8_t>{
      static_cast<uint8_t>(options.min_zoom),
      static_cast<uint8_t>(options.max_zoom)
    },
    string_from_view(options.attribution),
    to_native_tile_scheme(options.scheme),
    std::nullopt,
    vector_source
      ? std::optional<mbgl::Tileset::VectorEncoding>{to_native_vector_encoding(
          options.vector_encoding
        )}
      : std::nullopt
  };
  if (has_tile_source_option(options, MLN_STYLE_TILE_SOURCE_OPTION_BOUNDS)) {
    tileset.bounds = to_native_lat_lng_bounds(options.bounds);
  }
  return tileset;
}

auto validate_source_id(mln_string_view source_id) -> mln_status {
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    mln::core::set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_source_can_be_added(
  mbgl::style::Style& style, const std::string& source_id
) -> mln_status {
  if (style.getSource(source_id) != nullptr) {
    mln::core::set_thread_error("source already exists");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto has_style_image_option(
  const mln_style_image_options& options, uint32_t field
) -> bool {
  return (options.fields & field) != 0U;
}

auto validate_style_image_options(const mln_style_image_options* options)
  -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }
  if (options->size < sizeof(mln_style_image_options)) {
    mln::core::set_thread_error("mln_style_image_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_STYLE_IMAGE_OPTION_PIXEL_RATIO) |
    MLN_STYLE_IMAGE_OPTION_SDF;
  if ((options->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_style_image_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    has_style_image_option(*options, MLN_STYLE_IMAGE_OPTION_PIXEL_RATIO) &&
    (!std::isfinite(options->pixel_ratio) || options->pixel_ratio <= 0.0F)
  ) {
    mln::core::set_thread_error("pixel_ratio must be finite and positive");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto effective_style_image_options(const mln_style_image_options* options)
  -> mln_style_image_options {
  auto result = mln::core::style_image_options_default();
  if (options == nullptr) {
    return result;
  }
  result.fields = options->fields;
  if (has_style_image_option(*options, MLN_STYLE_IMAGE_OPTION_PIXEL_RATIO)) {
    result.pixel_ratio = options->pixel_ratio;
  }
  if (has_style_image_option(*options, MLN_STYLE_IMAGE_OPTION_SDF)) {
    result.sdf = options->sdf;
  }
  return result;
}

auto required_premultiplied_rgba8_bytes(
  uint32_t width, uint32_t height, uint32_t stride
) -> std::optional<size_t> {
  constexpr auto channels = size_t{4};
  const auto row_bytes = static_cast<size_t>(width) * channels;
  if (height == 0) {
    return std::nullopt;
  }
  if (height == 1) {
    return row_bytes;
  }
  const auto trailing_rows = static_cast<size_t>(height - 1U);
  const auto row_stride = static_cast<size_t>(stride);
  if (
    trailing_rows >
    (std::numeric_limits<size_t>::max() - row_bytes) / row_stride
  ) {
    return std::nullopt;
  }
  return (trailing_rows * row_stride) + row_bytes;
}

auto validate_premultiplied_rgba8_image(
  const mln_premultiplied_rgba8_image* image
) -> mln_status {
  if (image == nullptr) {
    mln::core::set_thread_error("image must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (image->size < sizeof(mln_premultiplied_rgba8_image)) {
    mln::core::set_thread_error(
      "mln_premultiplied_rgba8_image.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (image->width == 0 || image->height == 0) {
    mln::core::set_thread_error("image dimensions must be positive");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  constexpr auto channels = uint32_t{4};
  if (image->width > std::numeric_limits<uint32_t>::max() / channels) {
    mln::core::set_thread_error("image row byte length overflows");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto row_bytes = image->width * channels;
  if (image->stride < row_bytes) {
    mln::core::set_thread_error("image stride must be at least width * 4");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (image->pixels == nullptr) {
    mln::core::set_thread_error("image pixels must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto required = required_premultiplied_rgba8_bytes(
    image->width, image->height, image->stride
  );
  if (!required || image->byte_length < *required) {
    mln::core::set_thread_error("image byte_length is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto to_native_premultiplied_rgba8_image(
  const mln_premultiplied_rgba8_image& image
) -> mbgl::PremultipliedImage {
  auto result = mbgl::PremultipliedImage{mbgl::Size{image.width, image.height}};
  const auto output_stride = result.stride();
  const auto row_bytes = static_cast<size_t>(image.width) * 4U;
  const auto input = std::span<const uint8_t>{image.pixels, image.byte_length};
  const auto output = std::span<uint8_t>{result.data.get(), result.bytes()};
  for (auto row = uint32_t{0}; row < image.height; ++row) {
    const auto input_offset = static_cast<size_t>(row) * image.stride;
    const auto output_offset = static_cast<size_t>(row) * output_stride;
    std::copy_n(
      input.subspan(input_offset, row_bytes).begin(), row_bytes,
      output.subspan(output_offset, row_bytes).begin()
    );
  }
  return result;
}

auto validate_image_id(mln_string_view image_id) -> mln_status {
  if (!validate_string_view(image_id, "image_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (image_id.size == 0) {
    mln::core::set_thread_error("image_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto style_image_info_from_native(const mbgl::style::Image& image)
  -> mln_style_image_info {
  const auto& pixels = image.getImage();
  return mln_style_image_info{
    .size = sizeof(mln_style_image_info),
    .width = pixels.size.width,
    .height = pixels.size.height,
    .stride = static_cast<uint32_t>(pixels.stride()),
    .byte_length = pixels.bytes(),
    .pixel_ratio = image.getPixelRatio(),
    .sdf = image.isSdf()
  };
}

auto create_style_id_list(
  std::vector<std::string> ids, mln_style_id_list** out_list
) -> mln_status {
  if (out_list == nullptr || *out_list != nullptr) {
    mln::core::set_thread_error(
      "out_list must not be null and *out_list must be null"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto list = std::make_unique<mln_style_id_list>();
  list->ids = std::move(ids);
  auto* handle = list.get();
  const auto lock = std::scoped_lock{style_id_list_registry_mutex()};
  style_id_list_registry().emplace(handle, std::move(list));
  *out_list = handle;
  return MLN_STATUS_OK;
}

auto find_style_id_list_locked(const mln_style_id_list* list)
  -> const mln_style_id_list* {
  const auto found = style_id_list_registry().find(list);
  if (found == style_id_list_registry().end()) {
    return nullptr;
  }
  return found->second.get();
}

template <typename Payload>
auto payload_bytes(const Payload& payload) -> std::vector<std::byte> {
  auto result = std::vector<std::byte>(sizeof(Payload));
  std::memcpy(result.data(), &payload, sizeof(Payload));
  return result;
}

auto to_c_render_mode(mbgl::MapObserver::RenderMode mode) -> uint32_t {
  switch (mode) {
    case mbgl::MapObserver::RenderMode::Partial:
      return MLN_RENDER_MODE_PARTIAL;
    case mbgl::MapObserver::RenderMode::Full:
      return MLN_RENDER_MODE_FULL;
  }
  return MLN_RENDER_MODE_PARTIAL;
}

auto to_c_rendering_stats(const mbgl::gfx::RenderingStats& stats)
  -> mln_rendering_stats {
  return mln_rendering_stats{
    .size = sizeof(mln_rendering_stats),
    .encoding_time = stats.encodingTime,
    .rendering_time = stats.renderingTime,
    .frame_count = stats.numFrames,
    .draw_call_count = stats.numDrawCalls,
    .total_draw_call_count = stats.totalDrawCalls
  };
}

auto render_frame_payload(const mbgl::MapObserver::RenderFrameStatus& status)
  -> mln_runtime_event_render_frame {
  return mln_runtime_event_render_frame{
    .size = sizeof(mln_runtime_event_render_frame),
    .mode = to_c_render_mode(status.mode),
    .needs_repaint = status.needsRepaint,
    .placement_changed = status.placementChanged,
    .stats = to_c_rendering_stats(status.renderingStats)
  };
}

auto render_map_payload(mbgl::MapObserver::RenderMode mode)
  -> mln_runtime_event_render_map {
  return mln_runtime_event_render_map{
    .size = sizeof(mln_runtime_event_render_map), .mode = to_c_render_mode(mode)
  };
}

auto style_image_missing_payload() -> mln_runtime_event_style_image_missing {
  return mln_runtime_event_style_image_missing{
    .size = sizeof(mln_runtime_event_style_image_missing),
    .image_id = nullptr,
    .image_id_size = 0
  };
}

auto to_c_tile_operation(mbgl::TileOperation operation) -> uint32_t {
  switch (operation) {
    case mbgl::TileOperation::RequestedFromCache:
      return MLN_TILE_OPERATION_REQUESTED_FROM_CACHE;
    case mbgl::TileOperation::RequestedFromNetwork:
      return MLN_TILE_OPERATION_REQUESTED_FROM_NETWORK;
    case mbgl::TileOperation::LoadFromNetwork:
      return MLN_TILE_OPERATION_LOAD_FROM_NETWORK;
    case mbgl::TileOperation::LoadFromCache:
      return MLN_TILE_OPERATION_LOAD_FROM_CACHE;
    case mbgl::TileOperation::StartParse:
      return MLN_TILE_OPERATION_START_PARSE;
    case mbgl::TileOperation::EndParse:
      return MLN_TILE_OPERATION_END_PARSE;
    case mbgl::TileOperation::Error:
      return MLN_TILE_OPERATION_ERROR;
    case mbgl::TileOperation::Cancelled:
      return MLN_TILE_OPERATION_CANCELLED;
    case mbgl::TileOperation::NullOp:
      return MLN_TILE_OPERATION_NULL;
  }
  return MLN_TILE_OPERATION_NULL;
}

auto to_c_tile_id(const mbgl::OverscaledTileID& tile_id) -> mln_tile_id {
  return mln_tile_id{
    .overscaled_z = tile_id.overscaledZ,
    .wrap = tile_id.wrap,
    .canonical_z = tile_id.canonical.z,
    .canonical_x = tile_id.canonical.x,
    .canonical_y = tile_id.canonical.y
  };
}

auto tile_action_payload(
  mbgl::TileOperation operation, const mbgl::OverscaledTileID& tile_id
) -> mln_runtime_event_tile_action {
  return mln_runtime_event_tile_action{
    .size = sizeof(mln_runtime_event_tile_action),
    .operation = to_c_tile_operation(operation),
    .tile_id = to_c_tile_id(tile_id),
    .source_id = nullptr,
    .source_id_size = 0
  };
}

class HeadlessObserver final : public mbgl::MapObserver {
 public:
  HeadlessObserver(mln_runtime* runtime, mln_map* map)
      : runtime_(runtime), map_(map) {}

  void onCameraWillChange(CameraChangeMode mode) override {
    push(MLN_RUNTIME_EVENT_MAP_CAMERA_WILL_CHANGE, static_cast<int32_t>(mode));
  }

  void onCameraIsChanging() override {
    push(MLN_RUNTIME_EVENT_MAP_CAMERA_IS_CHANGING);
  }

  void onCameraDidChange(CameraChangeMode mode) override {
    push(MLN_RUNTIME_EVENT_MAP_CAMERA_DID_CHANGE, static_cast<int32_t>(mode));
  }

  void onWillStartLoadingMap() override {
    push(MLN_RUNTIME_EVENT_MAP_LOADING_STARTED);
  }

  void onDidFinishLoadingMap() override {
    push(MLN_RUNTIME_EVENT_MAP_LOADING_FINISHED);
  }

  void onDidFailLoadingMap(
    mbgl::MapLoadError error, const std::string& message
  ) override {
    push(
      MLN_RUNTIME_EVENT_MAP_LOADING_FAILED, static_cast<int32_t>(error),
      message.c_str()
    );
  }

  void onDidFinishLoadingStyle() override {
    push(MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);
  }

  void onWillStartRenderingFrame() override {
    push(MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_STARTED);
  }

  void onDidFinishRenderingFrame(const RenderFrameStatus& status) override {
    push_payload(
      MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_FINISHED,
      MLN_RUNTIME_EVENT_PAYLOAD_RENDER_FRAME,
      payload_bytes(render_frame_payload(status))
    );
  }

  void onWillStartRenderingMap() override {
    push(MLN_RUNTIME_EVENT_MAP_RENDER_MAP_STARTED);
  }

  void onDidFinishRenderingMap(RenderMode mode) override {
    push_payload(
      MLN_RUNTIME_EVENT_MAP_RENDER_MAP_FINISHED,
      MLN_RUNTIME_EVENT_PAYLOAD_RENDER_MAP,
      payload_bytes(render_map_payload(mode))
    );
  }

  void onDidBecomeIdle() override { push(MLN_RUNTIME_EVENT_MAP_IDLE); }

  void onStyleImageMissing(const std::string& image_id) override {
    push_payload(
      MLN_RUNTIME_EVENT_MAP_STYLE_IMAGE_MISSING,
      MLN_RUNTIME_EVENT_PAYLOAD_STYLE_IMAGE_MISSING,
      payload_bytes(style_image_missing_payload()), 0, image_id
    );
  }

  void onTileAction(
    mbgl::TileOperation operation, const mbgl::OverscaledTileID& tile_id,
    const std::string& source_id
  ) override {
    push_payload(
      MLN_RUNTIME_EVENT_MAP_TILE_ACTION, MLN_RUNTIME_EVENT_PAYLOAD_TILE_ACTION,
      payload_bytes(tile_action_payload(operation, tile_id)), 0, source_id
    );
  }

  void onRenderError(std::exception_ptr error) override {
    try {
      if (error) {
        std::rethrow_exception(error);
      }
      push(MLN_RUNTIME_EVENT_MAP_RENDER_ERROR);
    } catch (const std::exception& exception) {
      push(MLN_RUNTIME_EVENT_MAP_RENDER_ERROR, 0, exception.what());
    } catch (...) {
      push(MLN_RUNTIME_EVENT_MAP_RENDER_ERROR, 0, "unknown render error");
    }
  }

 private:
  auto push(uint32_t type, int32_t code = 0, const char* message = nullptr)
    -> void {
    mln::core::push_runtime_map_event(runtime_, map_, type, code, message);
  }

  auto push_payload(
    uint32_t type, uint32_t payload_type, std::vector<std::byte> payload,
    int32_t code = 0, std::string message = {}
  ) -> void {
    mln::core::push_runtime_map_event_payload(
      runtime_, map_, type, payload_type, std::move(payload), code,
      std::move(message)
    );
  }

  mln_runtime* runtime_;
  mln_map* map_;
};

class HeadlessFrontend final : public mbgl::RendererFrontend {
 public:
  HeadlessFrontend(mln_runtime* runtime, mln_map* map)
      : runtime_(runtime),
        map_(map),
        thread_pool_(
          mbgl::Scheduler::GetBackground(), mbgl::util::SimpleIdentity::Empty
        ) {}

  void reset() override {
    const std::scoped_lock lock(latest_update_mutex_);
    latest_update_.reset();
  }

  void setObserver(mbgl::RendererObserver& observer) override {
    observer_ = &observer;
  }

  void update(std::shared_ptr<mbgl::UpdateParameters> update) override {
    const std::scoped_lock lock(latest_update_mutex_);
    latest_update_ = std::move(update);
    mln::core::push_runtime_map_event(
      runtime_, map_, MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE
    );
  }

  [[nodiscard]] auto latest_update() const
    -> std::shared_ptr<mbgl::UpdateParameters> {
    const std::scoped_lock lock(latest_update_mutex_);
    return latest_update_;
  }

  auto run_render_jobs() -> void { thread_pool_.runRenderJobs(); }

  [[nodiscard]] auto renderer_observer() const -> mbgl::RendererObserver* {
    return observer_;
  }

  [[nodiscard]] auto getThreadPool() const
    -> const mbgl::TaggedScheduler& override {
    return thread_pool_;
  }

 private:
  mln_runtime* runtime_;
  mln_map* map_;
  mbgl::RendererObserver* observer_ = nullptr;
  mbgl::TaggedScheduler thread_pool_;
  mutable std::mutex latest_update_mutex_;
  std::shared_ptr<mbgl::UpdateParameters> latest_update_;
};

auto validate_map_options(const mln_map_options* options) -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }

  if (options->size < sizeof(mln_map_options)) {
    mln::core::set_thread_error("mln_map_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (
    options->width == 0 || options->height == 0 ||
    !std::isfinite(options->scale_factor) || options->scale_factor <= 0
  ) {
    mln::core::set_thread_error(
      "map dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  switch (options->map_mode) {
    case MLN_MAP_MODE_CONTINUOUS:
    case MLN_MAP_MODE_STATIC:
    case MLN_MAP_MODE_TILE:
      break;
    default:
      mln::core::set_thread_error("map_mode is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

auto to_native_map_mode(uint32_t mode) -> mbgl::MapMode {
  switch (mode) {
    case MLN_MAP_MODE_STATIC:
      return mbgl::MapMode::Static;
    case MLN_MAP_MODE_TILE:
      return mbgl::MapMode::Tile;
    case MLN_MAP_MODE_CONTINUOUS:
    default:
      return mbgl::MapMode::Continuous;
  }
}

auto is_still_map_mode(uint32_t mode) -> bool {
  return mode == MLN_MAP_MODE_STATIC || mode == MLN_MAP_MODE_TILE;
}

auto exception_message(std::exception_ptr error) -> std::string {
  if (!error) {
    return {};
  }
  try {
    std::rethrow_exception(error);
  } catch (const std::exception& exception) {
    return exception.what();
  } catch (...) {
    return "unknown still-image request error";
  }
}

auto validate_lat_lng(mln_lat_lng coordinate) -> mln_status;
auto validate_edge_insets(mln_edge_insets padding) -> mln_status;
auto validate_screen_point(mln_screen_point point) -> mln_status;

auto validate_camera_options(const mln_camera_options* camera) -> mln_status {
  if (camera == nullptr) {
    mln::core::set_thread_error("camera must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (camera->size < sizeof(mln_camera_options)) {
    mln::core::set_thread_error("mln_camera_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_CAMERA_OPTION_CENTER) | MLN_CAMERA_OPTION_ZOOM |
    MLN_CAMERA_OPTION_BEARING | MLN_CAMERA_OPTION_PITCH |
    MLN_CAMERA_OPTION_CENTER_ALTITUDE | MLN_CAMERA_OPTION_PADDING |
    MLN_CAMERA_OPTION_ANCHOR | MLN_CAMERA_OPTION_ROLL | MLN_CAMERA_OPTION_FOV;
  if ((camera->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_camera_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if ((camera->fields & MLN_CAMERA_OPTION_CENTER) != 0U) {
    const auto status = validate_lat_lng(
      mln_lat_lng{.latitude = camera->latitude, .longitude = camera->longitude}
    );
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if (
    ((camera->fields & MLN_CAMERA_OPTION_CENTER_ALTITUDE) != 0U &&
     !std::isfinite(camera->center_altitude)) ||
    ((camera->fields & MLN_CAMERA_OPTION_ZOOM) != 0U &&
     !std::isfinite(camera->zoom)) ||
    ((camera->fields & MLN_CAMERA_OPTION_BEARING) != 0U &&
     !std::isfinite(camera->bearing)) ||
    ((camera->fields & MLN_CAMERA_OPTION_PITCH) != 0U &&
     !std::isfinite(camera->pitch)) ||
    ((camera->fields & MLN_CAMERA_OPTION_ROLL) != 0U &&
     !std::isfinite(camera->roll)) ||
    ((camera->fields & MLN_CAMERA_OPTION_FOV) != 0U &&
     !std::isfinite(camera->field_of_view))
  ) {
    mln::core::set_thread_error("enabled camera numeric fields must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((camera->fields & MLN_CAMERA_OPTION_PADDING) != 0U) {
    const auto status = validate_edge_insets(camera->padding);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if ((camera->fields & MLN_CAMERA_OPTION_ANCHOR) != 0U) {
    const auto status = validate_screen_point(camera->anchor);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }

  return MLN_STATUS_OK;
}

auto max_animation_duration_ms() -> double {
  using DoubleMilliseconds = std::chrono::duration<double, std::milli>;
  return std::chrono::duration_cast<DoubleMilliseconds>(mbgl::Duration::max())
    .count();
}

auto validate_animation_options(const mln_animation_options* animation)
  -> mln_status {
  if (animation == nullptr) {
    return MLN_STATUS_OK;
  }
  if (animation->size < sizeof(mln_animation_options)) {
    mln::core::set_thread_error("mln_animation_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_ANIMATION_OPTION_DURATION) |
    MLN_ANIMATION_OPTION_VELOCITY | MLN_ANIMATION_OPTION_MIN_ZOOM |
    MLN_ANIMATION_OPTION_EASING;
  if ((animation->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_animation_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (animation->fields & MLN_ANIMATION_OPTION_DURATION) != 0U &&
    (!std::isfinite(animation->duration_ms) || animation->duration_ms < 0.0 ||
     animation->duration_ms > max_animation_duration_ms())
  ) {
    mln::core::set_thread_error(
      "animation duration_ms must fit the native duration range"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (animation->fields & MLN_ANIMATION_OPTION_VELOCITY) != 0U &&
    (!std::isfinite(animation->velocity) || animation->velocity <= 0.0)
  ) {
    mln::core::set_thread_error(
      "animation velocity must be positive and finite"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (animation->fields & MLN_ANIMATION_OPTION_MIN_ZOOM) != 0U &&
    !std::isfinite(animation->min_zoom)
  ) {
    mln::core::set_thread_error("animation min_zoom must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((animation->fields & MLN_ANIMATION_OPTION_EASING) != 0U) {
    const auto easing = animation->easing;
    if (
      !std::isfinite(easing.x1) || !std::isfinite(easing.y1) ||
      !std::isfinite(easing.x2) || !std::isfinite(easing.y2) ||
      easing.x1 < 0.0 || easing.x1 > 1.0 || easing.x2 < 0.0 || easing.x2 > 1.0
    ) {
      mln::core::set_thread_error(
        "animation easing x values must be within [0, 1] and all easing values "
        "must be finite"
      );
      return MLN_STATUS_INVALID_ARGUMENT;
    }
  }
  return MLN_STATUS_OK;
}

auto validate_camera_fit_options(const mln_camera_fit_options* options)
  -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }
  if (options->size < sizeof(mln_camera_fit_options)) {
    mln::core::set_thread_error("mln_camera_fit_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_CAMERA_FIT_OPTION_PADDING) |
    MLN_CAMERA_FIT_OPTION_BEARING | MLN_CAMERA_FIT_OPTION_PITCH;
  if ((options->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_camera_fit_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((options->fields & MLN_CAMERA_FIT_OPTION_PADDING) != 0U) {
    const auto status = validate_edge_insets(options->padding);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if (
    ((options->fields & MLN_CAMERA_FIT_OPTION_BEARING) != 0U &&
     !std::isfinite(options->bearing)) ||
    ((options->fields & MLN_CAMERA_FIT_OPTION_PITCH) != 0U &&
     !std::isfinite(options->pitch))
  ) {
    mln::core::set_thread_error("camera fit bearing and pitch must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_bound_options(const mln_bound_options* options) -> mln_status {
  if (options == nullptr) {
    mln::core::set_thread_error("bound options must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (options->size < sizeof(mln_bound_options)) {
    mln::core::set_thread_error("mln_bound_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_BOUND_OPTION_BOUNDS) | MLN_BOUND_OPTION_MIN_ZOOM |
    MLN_BOUND_OPTION_MAX_ZOOM | MLN_BOUND_OPTION_MIN_PITCH |
    MLN_BOUND_OPTION_MAX_PITCH;
  if ((options->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_bound_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((options->fields & MLN_BOUND_OPTION_BOUNDS) != 0U) {
    const auto status = validate_lat_lng_bounds(options->bounds);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if (
    ((options->fields & MLN_BOUND_OPTION_MIN_ZOOM) != 0U &&
     !std::isfinite(options->min_zoom)) ||
    ((options->fields & MLN_BOUND_OPTION_MAX_ZOOM) != 0U &&
     !std::isfinite(options->max_zoom)) ||
    ((options->fields & MLN_BOUND_OPTION_MIN_PITCH) != 0U &&
     !std::isfinite(options->min_pitch)) ||
    ((options->fields & MLN_BOUND_OPTION_MAX_PITCH) != 0U &&
     !std::isfinite(options->max_pitch))
  ) {
    mln::core::set_thread_error("bound numeric fields must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (options->fields & MLN_BOUND_OPTION_MIN_ZOOM) != 0U &&
    (options->fields & MLN_BOUND_OPTION_MAX_ZOOM) != 0U &&
    options->min_zoom > options->max_zoom
  ) {
    mln::core::set_thread_error(
      "min_zoom must be less than or equal to max_zoom"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (options->fields & MLN_BOUND_OPTION_MIN_PITCH) != 0U &&
    (options->fields & MLN_BOUND_OPTION_MAX_PITCH) != 0U &&
    options->min_pitch > options->max_pitch
  ) {
    mln::core::set_thread_error(
      "min_pitch must be less than or equal to max_pitch"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_vec3(mln_vec3 value, const char* name) -> mln_status {
  if (
    !std::isfinite(value.x) || !std::isfinite(value.y) ||
    !std::isfinite(value.z)
  ) {
    mln::core::set_thread_error(name);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_quaternion(mln_quaternion value) -> mln_status {
  if (
    !std::isfinite(value.x) || !std::isfinite(value.y) ||
    !std::isfinite(value.z) || !std::isfinite(value.w)
  ) {
    mln::core::set_thread_error(
      "free camera orientation values must be finite"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (value.x == 0.0 && value.y == 0.0 && value.z == 0.0 && value.w == 0.0) {
    mln::core::set_thread_error(
      "free camera orientation must not be zero length"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_free_camera_options(const mln_free_camera_options* options)
  -> mln_status {
  if (options == nullptr) {
    mln::core::set_thread_error("free camera options must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (options->size < sizeof(mln_free_camera_options)) {
    mln::core::set_thread_error("mln_free_camera_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_FREE_CAMERA_OPTION_POSITION) |
    MLN_FREE_CAMERA_OPTION_ORIENTATION;
  if ((options->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_free_camera_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((options->fields & MLN_FREE_CAMERA_OPTION_POSITION) != 0U) {
    const auto status = validate_vec3(
      options->position, "free camera position values must be finite"
    );
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  if ((options->fields & MLN_FREE_CAMERA_OPTION_ORIENTATION) != 0U) {
    const auto status = validate_quaternion(options->orientation);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  return MLN_STATUS_OK;
}

auto validate_projection_mode_options(const mln_projection_mode* mode)
  -> mln_status {
  if (mode == nullptr) {
    mln::core::set_thread_error("projection mode must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (mode->size < sizeof(mln_projection_mode)) {
    mln::core::set_thread_error("mln_projection_mode.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_PROJECTION_MODE_AXONOMETRIC) |
    MLN_PROJECTION_MODE_X_SKEW | MLN_PROJECTION_MODE_Y_SKEW;
  if ((mode->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_projection_mode.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (
    ((mode->fields & MLN_PROJECTION_MODE_X_SKEW) != 0U &&
     !std::isfinite(mode->x_skew)) ||
    ((mode->fields & MLN_PROJECTION_MODE_Y_SKEW) != 0U &&
     !std::isfinite(mode->y_skew))
  ) {
    mln::core::set_thread_error("projection skew values must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

auto validate_debug_options(uint32_t options) -> mln_status {
  constexpr auto known_options =
    static_cast<uint32_t>(MLN_MAP_DEBUG_TILE_BORDERS) |
    MLN_MAP_DEBUG_PARSE_STATUS | MLN_MAP_DEBUG_TIMESTAMPS |
    MLN_MAP_DEBUG_COLLISION | MLN_MAP_DEBUG_OVERDRAW |
    MLN_MAP_DEBUG_STENCIL_CLIP | MLN_MAP_DEBUG_DEPTH_BUFFER;
  if ((options & ~known_options) != 0U) {
    mln::core::set_thread_error("debug options contain unknown bits");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_frustum_offset(mln_edge_insets offset) -> mln_status {
  if (
    !std::isfinite(offset.top) || !std::isfinite(offset.left) ||
    !std::isfinite(offset.bottom) || !std::isfinite(offset.right)
  ) {
    mln::core::set_thread_error("frustum offset values must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    offset.top < 0.0 || offset.left < 0.0 || offset.bottom < 0.0 ||
    offset.right < 0.0
  ) {
    mln::core::set_thread_error(
      "frustum offset values must be greater than or equal to 0"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_viewport_options(const mln_map_viewport_options* options)
  -> mln_status {
  if (options == nullptr) {
    mln::core::set_thread_error("viewport options must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (options->size < sizeof(mln_map_viewport_options)) {
    mln::core::set_thread_error("mln_map_viewport_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION) |
    MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE |
    MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE |
    MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET;
  if ((options->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_map_viewport_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION) != 0U) {
    switch (options->north_orientation) {
      case MLN_NORTH_ORIENTATION_UP:
      case MLN_NORTH_ORIENTATION_RIGHT:
      case MLN_NORTH_ORIENTATION_DOWN:
      case MLN_NORTH_ORIENTATION_LEFT:
        break;
      default:
        mln::core::set_thread_error("north_orientation is invalid");
        return MLN_STATUS_INVALID_ARGUMENT;
    }
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE) != 0U) {
    switch (options->constrain_mode) {
      case MLN_CONSTRAIN_MODE_NONE:
      case MLN_CONSTRAIN_MODE_HEIGHT_ONLY:
      case MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT:
      case MLN_CONSTRAIN_MODE_SCREEN:
        break;
      default:
        mln::core::set_thread_error("constrain_mode is invalid");
        return MLN_STATUS_INVALID_ARGUMENT;
    }
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE) != 0U) {
    switch (options->viewport_mode) {
      case MLN_VIEWPORT_MODE_DEFAULT:
      case MLN_VIEWPORT_MODE_FLIPPED_Y:
        break;
      default:
        mln::core::set_thread_error("viewport_mode is invalid");
        return MLN_STATUS_INVALID_ARGUMENT;
    }
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET) != 0U) {
    return validate_frustum_offset(options->frustum_offset);
  }
  return MLN_STATUS_OK;
}

auto validate_tile_options(const mln_map_tile_options* options) -> mln_status {
  if (options == nullptr) {
    mln::core::set_thread_error("tile options must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (options->size < sizeof(mln_map_tile_options)) {
    mln::core::set_thread_error("mln_map_tile_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  constexpr auto known_fields =
    static_cast<uint32_t>(MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA) |
    MLN_MAP_TILE_OPTION_LOD_MIN_RADIUS | MLN_MAP_TILE_OPTION_LOD_SCALE |
    MLN_MAP_TILE_OPTION_LOD_PITCH_THRESHOLD |
    MLN_MAP_TILE_OPTION_LOD_ZOOM_SHIFT | MLN_MAP_TILE_OPTION_LOD_MODE;
  if ((options->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_map_tile_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (options->fields & MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA) != 0U &&
    options->prefetch_zoom_delta > std::numeric_limits<uint8_t>::max()
  ) {
    mln::core::set_thread_error("prefetch_zoom_delta must be at most 255");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    ((options->fields & MLN_MAP_TILE_OPTION_LOD_MIN_RADIUS) != 0U &&
     !std::isfinite(options->lod_min_radius)) ||
    ((options->fields & MLN_MAP_TILE_OPTION_LOD_SCALE) != 0U &&
     !std::isfinite(options->lod_scale)) ||
    ((options->fields & MLN_MAP_TILE_OPTION_LOD_PITCH_THRESHOLD) != 0U &&
     !std::isfinite(options->lod_pitch_threshold)) ||
    ((options->fields & MLN_MAP_TILE_OPTION_LOD_ZOOM_SHIFT) != 0U &&
     !std::isfinite(options->lod_zoom_shift))
  ) {
    mln::core::set_thread_error("tile LOD values must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if ((options->fields & MLN_MAP_TILE_OPTION_LOD_MODE) != 0U) {
    switch (options->lod_mode) {
      case MLN_TILE_LOD_MODE_DEFAULT:
      case MLN_TILE_LOD_MODE_DISTANCE:
        break;
      default:
        mln::core::set_thread_error("lod_mode is invalid");
        return MLN_STATUS_INVALID_ARGUMENT;
    }
  }
  return MLN_STATUS_OK;
}

auto validate_lat_lng(mln_lat_lng coordinate) -> mln_status {
  if (
    !std::isfinite(coordinate.latitude) || coordinate.latitude < -90.0 ||
    coordinate.latitude > 90.0 || !std::isfinite(coordinate.longitude)
  ) {
    mln::core::set_thread_error(
      "latitude must be finite and within [-90, 90], and longitude must be "
      "finite"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_lat_lng_bounds(mln_lat_lng_bounds bounds) -> mln_status {
  const auto southwest_status = validate_lat_lng(bounds.southwest);
  if (southwest_status != MLN_STATUS_OK) {
    return southwest_status;
  }
  const auto northeast_status = validate_lat_lng(bounds.northeast);
  if (northeast_status != MLN_STATUS_OK) {
    return northeast_status;
  }
  if (
    bounds.southwest.latitude > bounds.northeast.latitude ||
    bounds.southwest.longitude > bounds.northeast.longitude
  ) {
    mln::core::set_thread_error(
      "bounds southwest must be less than or equal to northeast"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_lat_lng_array(
  const mln_lat_lng* coordinates, size_t coordinate_count, bool allow_empty
) -> mln_status {
  if (coordinate_count == 0) {
    if (allow_empty) {
      return MLN_STATUS_OK;
    }
    mln::core::set_thread_error("coordinate_count must be greater than 0");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (coordinates == nullptr) {
    mln::core::set_thread_error("coordinates must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto coordinate_span =
    std::span<const mln_lat_lng>{coordinates, coordinate_count};
  for (const auto coordinate : coordinate_span) {
    const auto status = validate_lat_lng(coordinate);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  return MLN_STATUS_OK;
}

auto validate_screen_point(mln_screen_point point) -> mln_status {
  if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
    mln::core::set_thread_error("screen point values must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_screen_point_array(
  const mln_screen_point* points, size_t point_count
) -> mln_status {
  if (point_count == 0) {
    return MLN_STATUS_OK;
  }

  if (points == nullptr) {
    mln::core::set_thread_error("points must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto point_span =
    std::span<const mln_screen_point>{points, point_count};
  for (const auto point : point_span) {
    const auto status = validate_screen_point(point);
    if (status != MLN_STATUS_OK) {
      return status;
    }
  }
  return MLN_STATUS_OK;
}

auto validate_edge_insets(mln_edge_insets padding) -> mln_status {
  if (
    !std::isfinite(padding.top) || !std::isfinite(padding.left) ||
    !std::isfinite(padding.bottom) || !std::isfinite(padding.right) ||
    padding.top < 0.0 || padding.left < 0.0 || padding.bottom < 0.0 ||
    padding.right < 0.0
  ) {
    mln::core::set_thread_error(
      "padding values must be finite and greater than or equal to 0"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_projected_meters(mln_projected_meters meters) -> mln_status {
  if (!std::isfinite(meters.northing) || !std::isfinite(meters.easting)) {
    mln::core::set_thread_error("projected meter values must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto to_native_screen_point(mln_screen_point point) -> mbgl::ScreenCoordinate;
auto from_native_screen_point(const mbgl::ScreenCoordinate& point)
  -> mln_screen_point;
auto to_native_edge_insets(mln_edge_insets padding) -> mbgl::EdgeInsets;
auto from_native_edge_insets(const mbgl::EdgeInsets& insets) -> mln_edge_insets;

auto to_native_camera(const mln_camera_options& camera) -> mbgl::CameraOptions {
  auto result = mbgl::CameraOptions{};
  if ((camera.fields & MLN_CAMERA_OPTION_CENTER) != 0U) {
    result.withCenter(mbgl::LatLng{camera.latitude, camera.longitude});
  }
  if ((camera.fields & MLN_CAMERA_OPTION_CENTER_ALTITUDE) != 0U) {
    result.withCenterAltitude(camera.center_altitude);
  }
  if ((camera.fields & MLN_CAMERA_OPTION_PADDING) != 0U) {
    result.withPadding(to_native_edge_insets(camera.padding));
  }
  if ((camera.fields & MLN_CAMERA_OPTION_ANCHOR) != 0U) {
    result.withAnchor(to_native_screen_point(camera.anchor));
  }
  if ((camera.fields & MLN_CAMERA_OPTION_ZOOM) != 0U) {
    result.withZoom(camera.zoom);
  }
  if ((camera.fields & MLN_CAMERA_OPTION_BEARING) != 0U) {
    result.withBearing(camera.bearing);
  }
  if ((camera.fields & MLN_CAMERA_OPTION_PITCH) != 0U) {
    result.withPitch(camera.pitch);
  }
  if ((camera.fields & MLN_CAMERA_OPTION_ROLL) != 0U) {
    result.withRoll(camera.roll);
  }
  if ((camera.fields & MLN_CAMERA_OPTION_FOV) != 0U) {
    result.withFov(camera.field_of_view);
  }
  return result;
}

auto from_native_camera(const mbgl::CameraOptions& camera)
  -> mln_camera_options {
  auto result = mln::core::camera_options_default();
  if (camera.center) {
    result.fields |= MLN_CAMERA_OPTION_CENTER;
    result.latitude = camera.center->latitude();
    result.longitude = camera.center->longitude();
  }
  if (camera.centerAltitude) {
    result.fields |= MLN_CAMERA_OPTION_CENTER_ALTITUDE;
    result.center_altitude = *camera.centerAltitude;
  }
  if (camera.padding) {
    result.fields |= MLN_CAMERA_OPTION_PADDING;
    result.padding = from_native_edge_insets(*camera.padding);
  }
  if (camera.anchor) {
    result.fields |= MLN_CAMERA_OPTION_ANCHOR;
    result.anchor = from_native_screen_point(*camera.anchor);
  }
  if (camera.zoom) {
    result.fields |= MLN_CAMERA_OPTION_ZOOM;
    result.zoom = *camera.zoom;
  }
  if (camera.bearing) {
    result.fields |= MLN_CAMERA_OPTION_BEARING;
    result.bearing = *camera.bearing;
  }
  if (camera.pitch) {
    result.fields |= MLN_CAMERA_OPTION_PITCH;
    result.pitch = *camera.pitch;
  }
  if (camera.roll) {
    result.fields |= MLN_CAMERA_OPTION_ROLL;
    result.roll = *camera.roll;
  }
  if (camera.fov) {
    result.fields |= MLN_CAMERA_OPTION_FOV;
    result.field_of_view = *camera.fov;
  }
  return result;
}

auto to_native_animation(const mln_animation_options* animation)
  -> mbgl::AnimationOptions {
  auto result = mbgl::AnimationOptions{};
  if (animation == nullptr) {
    return result;
  }
  if ((animation->fields & MLN_ANIMATION_OPTION_DURATION) != 0U) {
    result.duration = std::chrono::duration_cast<mbgl::Duration>(
      std::chrono::duration<double, std::milli>{animation->duration_ms}
    );
  }
  if ((animation->fields & MLN_ANIMATION_OPTION_VELOCITY) != 0U) {
    result.velocity = animation->velocity;
  }
  if ((animation->fields & MLN_ANIMATION_OPTION_MIN_ZOOM) != 0U) {
    result.minZoom = animation->min_zoom;
  }
  if ((animation->fields & MLN_ANIMATION_OPTION_EASING) != 0U) {
    const auto easing = animation->easing;
    result.easing.emplace(easing.x1, easing.y1, easing.x2, easing.y2);
  }
  return result;
}

auto camera_fit_padding(const mln_camera_fit_options* options)
  -> mbgl::EdgeInsets {
  if (
    options == nullptr ||
    (options->fields & MLN_CAMERA_FIT_OPTION_PADDING) == 0U
  ) {
    return mbgl::EdgeInsets{};
  }
  return to_native_edge_insets(options->padding);
}

auto camera_fit_bearing(const mln_camera_fit_options* options)
  -> std::optional<double> {
  if (
    options == nullptr ||
    (options->fields & MLN_CAMERA_FIT_OPTION_BEARING) == 0U
  ) {
    return std::nullopt;
  }
  return options->bearing;
}

auto camera_fit_pitch(const mln_camera_fit_options* options)
  -> std::optional<double> {
  if (
    options == nullptr || (options->fields & MLN_CAMERA_FIT_OPTION_PITCH) == 0U
  ) {
    return std::nullopt;
  }
  return options->pitch;
}

auto to_native_projection_mode(const mln_projection_mode& mode)
  -> mbgl::ProjectionMode {
  auto result = mbgl::ProjectionMode{};
  if ((mode.fields & MLN_PROJECTION_MODE_AXONOMETRIC) != 0U) {
    result.withAxonometric(mode.axonometric);
  }
  if ((mode.fields & MLN_PROJECTION_MODE_X_SKEW) != 0U) {
    result.withXSkew(mode.x_skew);
  }
  if ((mode.fields & MLN_PROJECTION_MODE_Y_SKEW) != 0U) {
    result.withYSkew(mode.y_skew);
  }
  return result;
}

auto to_native_debug_options(uint32_t options) -> mbgl::MapDebugOptions {
  return static_cast<mbgl::MapDebugOptions>(options);
}

auto from_native_debug_options(mbgl::MapDebugOptions options) -> uint32_t {
  return static_cast<uint32_t>(options);
}

auto to_native_north_orientation(uint32_t orientation)
  -> mbgl::NorthOrientation {
  switch (orientation) {
    case MLN_NORTH_ORIENTATION_RIGHT:
      return mbgl::NorthOrientation::Rightwards;
    case MLN_NORTH_ORIENTATION_DOWN:
      return mbgl::NorthOrientation::Downwards;
    case MLN_NORTH_ORIENTATION_LEFT:
      return mbgl::NorthOrientation::Leftwards;
    case MLN_NORTH_ORIENTATION_UP:
    default:
      return mbgl::NorthOrientation::Upwards;
  }
}

auto from_native_north_orientation(mbgl::NorthOrientation orientation)
  -> uint32_t {
  switch (orientation) {
    case mbgl::NorthOrientation::Rightwards:
      return MLN_NORTH_ORIENTATION_RIGHT;
    case mbgl::NorthOrientation::Downwards:
      return MLN_NORTH_ORIENTATION_DOWN;
    case mbgl::NorthOrientation::Leftwards:
      return MLN_NORTH_ORIENTATION_LEFT;
    case mbgl::NorthOrientation::Upwards:
    default:
      return MLN_NORTH_ORIENTATION_UP;
  }
}

auto to_native_constrain_mode(uint32_t mode) -> mbgl::ConstrainMode {
  switch (mode) {
    case MLN_CONSTRAIN_MODE_NONE:
      return mbgl::ConstrainMode::None;
    case MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT:
      return mbgl::ConstrainMode::WidthAndHeight;
    case MLN_CONSTRAIN_MODE_SCREEN:
      return mbgl::ConstrainMode::Screen;
    case MLN_CONSTRAIN_MODE_HEIGHT_ONLY:
    default:
      return mbgl::ConstrainMode::HeightOnly;
  }
}

auto from_native_constrain_mode(mbgl::ConstrainMode mode) -> uint32_t {
  switch (mode) {
    case mbgl::ConstrainMode::None:
      return MLN_CONSTRAIN_MODE_NONE;
    case mbgl::ConstrainMode::WidthAndHeight:
      return MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT;
    case mbgl::ConstrainMode::Screen:
      return MLN_CONSTRAIN_MODE_SCREEN;
    case mbgl::ConstrainMode::HeightOnly:
    default:
      return MLN_CONSTRAIN_MODE_HEIGHT_ONLY;
  }
}

auto to_native_viewport_mode(uint32_t mode) -> mbgl::ViewportMode {
  switch (mode) {
    case MLN_VIEWPORT_MODE_FLIPPED_Y:
      return mbgl::ViewportMode::FlippedY;
    case MLN_VIEWPORT_MODE_DEFAULT:
    default:
      return mbgl::ViewportMode::Default;
  }
}

auto from_native_viewport_mode(mbgl::ViewportMode mode) -> uint32_t {
  switch (mode) {
    case mbgl::ViewportMode::FlippedY:
      return MLN_VIEWPORT_MODE_FLIPPED_Y;
    case mbgl::ViewportMode::Default:
    default:
      return MLN_VIEWPORT_MODE_DEFAULT;
  }
}

auto from_native_edge_insets(const mbgl::EdgeInsets& insets)
  -> mln_edge_insets {
  return mln_edge_insets{
    .top = insets.top(),
    .left = insets.left(),
    .bottom = insets.bottom(),
    .right = insets.right()
  };
}

auto to_native_tile_lod_mode(uint32_t mode) -> mbgl::TileLodMode {
  switch (mode) {
    case MLN_TILE_LOD_MODE_DISTANCE:
      return mbgl::TileLodMode::Distance;
    case MLN_TILE_LOD_MODE_DEFAULT:
    default:
      return mbgl::TileLodMode::Default;
  }
}

auto from_native_tile_lod_mode(mbgl::TileLodMode mode) -> uint32_t {
  switch (mode) {
    case mbgl::TileLodMode::Distance:
      return MLN_TILE_LOD_MODE_DISTANCE;
    case mbgl::TileLodMode::Default:
    default:
      return MLN_TILE_LOD_MODE_DEFAULT;
  }
}

auto from_native_projection_mode(const mbgl::ProjectionMode& mode)
  -> mln_projection_mode {
  auto result = mln::core::projection_mode_default();
  if (mode.axonometric) {
    result.fields |= MLN_PROJECTION_MODE_AXONOMETRIC;
    result.axonometric = *mode.axonometric;
  }
  if (mode.xSkew) {
    result.fields |= MLN_PROJECTION_MODE_X_SKEW;
    result.x_skew = *mode.xSkew;
  }
  if (mode.ySkew) {
    result.fields |= MLN_PROJECTION_MODE_Y_SKEW;
    result.y_skew = *mode.ySkew;
  }
  return result;
}

auto to_native_lat_lng(mln_lat_lng coordinate) -> mbgl::LatLng {
  return mbgl::LatLng{coordinate.latitude, coordinate.longitude};
}

auto from_native_lat_lng(const mbgl::LatLng& coordinate) -> mln_lat_lng {
  return mln_lat_lng{
    .latitude = coordinate.latitude(), .longitude = coordinate.longitude()
  };
}

auto to_native_lat_lng_bounds(mln_lat_lng_bounds bounds) -> mbgl::LatLngBounds {
  return mbgl::LatLngBounds::hull(
    to_native_lat_lng(bounds.southwest), to_native_lat_lng(bounds.northeast)
  );
}

auto from_native_lat_lng_bounds(const mbgl::LatLngBounds& bounds)
  -> mln_lat_lng_bounds {
  return mln_lat_lng_bounds{
    .southwest =
      mln_lat_lng{.latitude = bounds.south(), .longitude = bounds.west()},
    .northeast =
      mln_lat_lng{.latitude = bounds.north(), .longitude = bounds.east()}
  };
}

auto to_native_lat_lngs(const mln_lat_lng* coordinates, size_t coordinate_count)
  -> std::vector<mbgl::LatLng> {
  auto result = std::vector<mbgl::LatLng>{};
  result.reserve(coordinate_count);
  const auto coordinate_span =
    std::span<const mln_lat_lng>{coordinates, coordinate_count};
  for (const auto coordinate : coordinate_span) {
    result.emplace_back(to_native_lat_lng(coordinate));
  }
  return result;
}

auto to_native_screen_point(mln_screen_point point) -> mbgl::ScreenCoordinate {
  return mbgl::ScreenCoordinate{point.x, point.y};
}

auto from_native_screen_point(const mbgl::ScreenCoordinate& point)
  -> mln_screen_point {
  return mln_screen_point{.x = point.x, .y = point.y};
}

auto to_native_screen_points(const mln_screen_point* points, size_t point_count)
  -> std::vector<mbgl::ScreenCoordinate> {
  auto result = std::vector<mbgl::ScreenCoordinate>{};
  result.reserve(point_count);
  const auto point_span =
    std::span<const mln_screen_point>{points, point_count};
  for (const auto point : point_span) {
    result.emplace_back(to_native_screen_point(point));
  }
  return result;
}

auto to_native_edge_insets(mln_edge_insets padding) -> mbgl::EdgeInsets {
  return mbgl::EdgeInsets{
    padding.top, padding.left, padding.bottom, padding.right
  };
}

auto to_native_bound_options(const mln_bound_options& options)
  -> mbgl::BoundOptions {
  auto result = mbgl::BoundOptions{};
  if ((options.fields & MLN_BOUND_OPTION_BOUNDS) != 0U) {
    result.withLatLngBounds(to_native_lat_lng_bounds(options.bounds));
  }
  if ((options.fields & MLN_BOUND_OPTION_MIN_ZOOM) != 0U) {
    result.withMinZoom(options.min_zoom);
  }
  if ((options.fields & MLN_BOUND_OPTION_MAX_ZOOM) != 0U) {
    result.withMaxZoom(options.max_zoom);
  }
  if ((options.fields & MLN_BOUND_OPTION_MIN_PITCH) != 0U) {
    result.withMinPitch(options.min_pitch);
  }
  if ((options.fields & MLN_BOUND_OPTION_MAX_PITCH) != 0U) {
    result.withMaxPitch(options.max_pitch);
  }
  return result;
}

auto from_native_bound_options(const mbgl::BoundOptions& options)
  -> mln_bound_options {
  auto result = mln::core::bound_options_default();
  if (options.bounds) {
    result.fields |= MLN_BOUND_OPTION_BOUNDS;
    result.bounds = from_native_lat_lng_bounds(*options.bounds);
  }
  if (options.minZoom) {
    result.fields |= MLN_BOUND_OPTION_MIN_ZOOM;
    result.min_zoom = *options.minZoom;
  }
  if (options.maxZoom) {
    result.fields |= MLN_BOUND_OPTION_MAX_ZOOM;
    result.max_zoom = *options.maxZoom;
  }
  if (options.minPitch) {
    result.fields |= MLN_BOUND_OPTION_MIN_PITCH;
    result.min_pitch = *options.minPitch;
  }
  if (options.maxPitch) {
    result.fields |= MLN_BOUND_OPTION_MAX_PITCH;
    result.max_pitch = *options.maxPitch;
  }
  return result;
}

auto to_native_vec3(mln_vec3 value) -> mbgl::vec3 {
  return mbgl::vec3{{value.x, value.y, value.z}};
}

auto from_native_vec3(const mbgl::vec3& value) -> mln_vec3 {
  const auto [x_component, y_component, z_component] = value;
  return mln_vec3{.x = x_component, .y = y_component, .z = z_component};
}

auto to_native_vec4(mln_quaternion value) -> mbgl::vec4 {
  return mbgl::vec4{{value.x, value.y, value.z, value.w}};
}

auto from_native_vec4(const mbgl::vec4& value) -> mln_quaternion {
  const auto [x_component, y_component, z_component, w_component] = value;
  return mln_quaternion{
    .x = x_component, .y = y_component, .z = z_component, .w = w_component
  };
}

auto to_native_free_camera(const mln_free_camera_options& options)
  -> mbgl::FreeCameraOptions {
  auto result = mbgl::FreeCameraOptions{};
  if ((options.fields & MLN_FREE_CAMERA_OPTION_POSITION) != 0U) {
    result.position = to_native_vec3(options.position);
  }
  if ((options.fields & MLN_FREE_CAMERA_OPTION_ORIENTATION) != 0U) {
    result.orientation = to_native_vec4(options.orientation);
  }
  return result;
}

auto from_native_free_camera(const mbgl::FreeCameraOptions& options)
  -> mln_free_camera_options {
  auto result = mln::core::free_camera_options_default();
  if (options.position) {
    result.fields |= MLN_FREE_CAMERA_OPTION_POSITION;
    result.position = from_native_vec3(*options.position);
  }
  if (options.orientation) {
    result.fields |= MLN_FREE_CAMERA_OPTION_ORIENTATION;
    result.orientation = from_native_vec4(*options.orientation);
  }
  return result;
}

auto screen_point(mln_screen_point point) -> mbgl::ScreenCoordinate {
  return mbgl::ScreenCoordinate{point.x, point.y};
}
}  // namespace

struct mln_map {
  mln_runtime* runtime = nullptr;
  std::thread::id owner_thread;
  uint32_t map_mode = MLN_MAP_MODE_CONTINUOUS;
  bool still_image_request_pending = false;
  std::unique_ptr<HeadlessObserver> observer;
  std::unique_ptr<HeadlessFrontend> frontend;
  std::unique_ptr<mbgl::Map> map;
  void* render_target_session = nullptr;
};

struct mln_map_projection {
  std::thread::id owner_thread;
  std::unique_ptr<mbgl::MapProjection> projection;
};

namespace mln::core {

namespace {

class RuntimeMapRetainGuard final {
 public:
  explicit RuntimeMapRetainGuard(mln_runtime* runtime) noexcept
      : runtime_(runtime) {}

  ~RuntimeMapRetainGuard() { release_runtime_map(runtime_); }

  RuntimeMapRetainGuard(const RuntimeMapRetainGuard&) = delete;
  RuntimeMapRetainGuard(RuntimeMapRetainGuard&&) = delete;
  auto operator=(const RuntimeMapRetainGuard&)
    -> RuntimeMapRetainGuard& = delete;
  auto operator=(RuntimeMapRetainGuard&&) -> RuntimeMapRetainGuard& = delete;

  auto dismiss() noexcept -> void { runtime_ = nullptr; }

 private:
  mln_runtime* runtime_ = nullptr;
};

auto finish_still_image_request(mln_map* map, std::exception_ptr error)
  -> void {
  map->still_image_request_pending = false;
  if (error) {
    const auto message = exception_message(error);
    push_runtime_map_event(
      map->runtime, map, MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED, 0,
      message.c_str()
    );
    return;
  }

  push_runtime_map_event(
    map->runtime, map, MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED
  );
}

}  // namespace

auto map_options_default() noexcept -> mln_map_options {
  return mln_map_options{
    .size = sizeof(mln_map_options),
    .width = default_map_width,
    .height = default_map_height,
    .scale_factor = default_scale_factor,
    .map_mode = MLN_MAP_MODE_CONTINUOUS
  };
}

auto camera_options_default() noexcept -> mln_camera_options {
  return mln_camera_options{
    .size = sizeof(mln_camera_options),
    .fields = 0,
    .latitude = 0,
    .longitude = 0,
    .center_altitude = 0,
    .padding = {.top = 0, .left = 0, .bottom = 0, .right = 0},
    .anchor = {.x = 0, .y = 0},
    .zoom = 0,
    .bearing = 0,
    .pitch = 0,
    .roll = 0,
    .field_of_view = 0
  };
}

auto animation_options_default() noexcept -> mln_animation_options {
  return mln_animation_options{
    .size = sizeof(mln_animation_options),
    .fields = 0,
    .duration_ms = 0,
    .velocity = 0,
    .min_zoom = 0,
    .easing = {.x1 = 0, .y1 = 0, .x2 = 0.25, .y2 = 1}
  };
}

auto camera_fit_options_default() noexcept -> mln_camera_fit_options {
  return mln_camera_fit_options{
    .size = sizeof(mln_camera_fit_options),
    .fields = 0,
    .padding = {.top = 0, .left = 0, .bottom = 0, .right = 0},
    .bearing = 0,
    .pitch = 0
  };
}

auto bound_options_default() noexcept -> mln_bound_options {
  return mln_bound_options{
    .size = sizeof(mln_bound_options),
    .fields = 0,
    .bounds =
      {.southwest = {.latitude = 0, .longitude = 0},
       .northeast = {.latitude = 0, .longitude = 0}},
    .min_zoom = 0,
    .max_zoom = 0,
    .min_pitch = 0,
    .max_pitch = 0
  };
}

auto free_camera_options_default() noexcept -> mln_free_camera_options {
  return mln_free_camera_options{
    .size = sizeof(mln_free_camera_options),
    .fields = 0,
    .position = {.x = 0, .y = 0, .z = 0},
    .orientation = {.x = 0, .y = 0, .z = 0, .w = 1}
  };
}

auto projection_mode_default() noexcept -> mln_projection_mode {
  return mln_projection_mode{
    .size = sizeof(mln_projection_mode),
    .fields = 0,
    .axonometric = false,
    .x_skew = 0,
    .y_skew = 0
  };
}

auto map_viewport_options_default() noexcept -> mln_map_viewport_options {
  return mln_map_viewport_options{
    .size = sizeof(mln_map_viewport_options),
    .fields = 0,
    .north_orientation = MLN_NORTH_ORIENTATION_UP,
    .constrain_mode = MLN_CONSTRAIN_MODE_HEIGHT_ONLY,
    .viewport_mode = MLN_VIEWPORT_MODE_DEFAULT,
    .frustum_offset = {.top = 0, .left = 0, .bottom = 0, .right = 0}
  };
}

auto map_tile_options_default() noexcept -> mln_map_tile_options {
  return mln_map_tile_options{
    .size = sizeof(mln_map_tile_options),
    .fields = 0,
    .prefetch_zoom_delta = 0,
    .lod_min_radius = 0,
    .lod_scale = 0,
    .lod_pitch_threshold = 0,
    .lod_zoom_shift = 0,
    .lod_mode = MLN_TILE_LOD_MODE_DEFAULT
  };
}

auto style_tile_source_options_default() noexcept
  -> mln_style_tile_source_options {
  return mln_style_tile_source_options{
    .size = sizeof(mln_style_tile_source_options),
    .fields = 0,
    .min_zoom = 0,
    .max_zoom = mbgl::util::DEFAULT_MAX_ZOOM,
    .attribution = {.data = nullptr, .size = 0},
    .scheme = MLN_STYLE_TILE_SCHEME_XYZ,
    .bounds =
      {.southwest = {.latitude = 0, .longitude = 0},
       .northeast = {.latitude = 0, .longitude = 0}},
    .tile_size = mbgl::util::tileSize_I,
    .vector_encoding = MLN_STYLE_VECTOR_TILE_ENCODING_MVT
  };
}

auto premultiplied_rgba8_image_default() noexcept
  -> mln_premultiplied_rgba8_image {
  return mln_premultiplied_rgba8_image{
    .size = sizeof(mln_premultiplied_rgba8_image),
    .width = 0,
    .height = 0,
    .stride = 0,
    .pixels = nullptr,
    .byte_length = 0
  };
}

auto style_image_options_default() noexcept -> mln_style_image_options {
  return mln_style_image_options{
    .size = sizeof(mln_style_image_options),
    .fields = 0,
    .pixel_ratio = 1.0F,
    .sdf = false
  };
}

auto style_image_info_default() noexcept -> mln_style_image_info {
  return mln_style_image_info{
    .size = sizeof(mln_style_image_info),
    .width = 0,
    .height = 0,
    .stride = 0,
    .byte_length = 0,
    .pixel_ratio = 1.0F,
    .sdf = false
  };
}

auto validate_map(mln_map* map) -> mln_status {
  if (map == nullptr) {
    set_thread_error("map must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(map_registry_mutex());
  if (!map_registry().contains(map)) {
    set_thread_error("map is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (map->owner_thread != std::this_thread::get_id()) {
    set_thread_error("map call must be made on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  return MLN_STATUS_OK;
}

auto validate_map_projection(mln_map_projection* projection) -> mln_status {
  if (projection == nullptr) {
    set_thread_error("projection must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(map_projection_registry_mutex());
  if (!map_projection_registry().contains(projection)) {
    set_thread_error("projection is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (projection->owner_thread != std::this_thread::get_id()) {
    set_thread_error("projection call must be made on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  return MLN_STATUS_OK;
}

auto create_map(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) -> mln_status {
  const auto options_status = validate_map_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }
  if (out_map == nullptr) {
    set_thread_error("out_map must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_map != nullptr) {
    set_thread_error("out_map must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto retain_status = retain_runtime_map(runtime);
  if (retain_status != MLN_STATUS_OK) {
    return retain_status;
  }
  auto retain_guard = RuntimeMapRetainGuard{runtime};

  const auto effective = options == nullptr ? map_options_default() : *options;
  auto owned_map = std::make_unique<mln_map>();
  auto* handle = owned_map.get();
  register_runtime_map_events(runtime, handle);
  owned_map->runtime = runtime;
  owned_map->owner_thread = std::this_thread::get_id();
  owned_map->map_mode = effective.map_mode;
  try {
    owned_map->observer = std::make_unique<HeadlessObserver>(runtime, handle);
    owned_map->frontend = std::make_unique<HeadlessFrontend>(runtime, handle);

    auto map_options = mbgl::MapOptions{};
    map_options.withMapMode(to_native_map_mode(effective.map_mode))
      .withSize(mbgl::Size{effective.width, effective.height})
      .withPixelRatio(static_cast<float>(effective.scale_factor));
    owned_map->map = std::make_unique<mbgl::Map>(
      *owned_map->frontend, *owned_map->observer, map_options,
      resource_options_for_runtime(runtime)
    );

    const std::scoped_lock lock(map_registry_mutex());
    map_registry().emplace(handle, std::move(owned_map));
  } catch (...) {
    discard_runtime_map_events(runtime, handle);
    throw;
  }
  *out_map = handle;
  retain_guard.dismiss();
  return MLN_STATUS_OK;
}

auto destroy_map(mln_map* map) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  if (map->render_target_session != nullptr) {
    set_thread_error("map still has an attached render target session");
    return MLN_STATUS_INVALID_STATE;
  }

  auto* runtime = map->runtime;
  auto owned_map = std::unique_ptr<mln_map>{};
  {
    const std::scoped_lock lock(map_registry_mutex());
    const auto found = map_registry().find(map);
    owned_map = std::move(found->second);
    map_registry().erase(found);
  }
  discard_runtime_map_events(runtime, map);
  owned_map.reset();
  release_runtime_map(runtime);
  return MLN_STATUS_OK;
}

auto map_request_repaint(mln_map* map) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  if (map->map_mode != MLN_MAP_MODE_CONTINUOUS) {
    set_thread_error("map is not in continuous mode");
    return MLN_STATUS_INVALID_STATE;
  }

  map->map->triggerRepaint();
  return MLN_STATUS_OK;
}

auto map_request_still_image(mln_map* map) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  if (!is_still_map_mode(map->map_mode)) {
    set_thread_error("map is not in static or tile mode");
    return MLN_STATUS_INVALID_STATE;
  }

  if (map->still_image_request_pending) {
    set_thread_error("map already has a pending still-image request");
    return MLN_STATUS_INVALID_STATE;
  }

  map->still_image_request_pending = true;
  map->map->renderStill([map](std::exception_ptr error) -> void {
    finish_still_image_request(map, error);
  });
  return MLN_STATUS_OK;
}

auto map_owner_thread(const mln_map* map) -> std::thread::id {
  if (map == nullptr) {
    return {};
  }
  return map->owner_thread;
}

auto map_native(mln_map* map) -> mbgl::Map* {
  if (map == nullptr) {
    return nullptr;
  }
  return map->map.get();
}

auto map_latest_update(mln_map* map)
  -> std::shared_ptr<mbgl::UpdateParameters> {
  if (map == nullptr || map->frontend == nullptr) {
    return nullptr;
  }
  return map->frontend->latest_update();
}

auto map_renderer_observer(mln_map* map) -> mbgl::RendererObserver* {
  if (map == nullptr || map->frontend == nullptr) {
    return nullptr;
  }
  return map->frontend->renderer_observer();
}

auto map_run_render_jobs(mln_map* map) -> void {
  if (map == nullptr || map->frontend == nullptr) {
    return;
  }
  map->frontend->run_render_jobs();
}

auto map_attach_render_target_session(mln_map* map, void* session)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (session == nullptr) {
    set_thread_error("render target session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (map->render_target_session != nullptr) {
    set_thread_error("map already has an attached render target session");
    return MLN_STATUS_INVALID_STATE;
  }
  map->render_target_session = session;
  return MLN_STATUS_OK;
}

auto map_detach_render_target_session(mln_map* map, void* session)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (session == nullptr) {
    set_thread_error("render target session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (map->render_target_session != session) {
    set_thread_error("render target session is not attached to this map");
    return MLN_STATUS_INVALID_STATE;
  }
  map->render_target_session = nullptr;
  return MLN_STATUS_OK;
}

auto map_set_style_url(mln_map* map, const char* url) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (url == nullptr) {
    set_thread_error("url must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  clear_runtime_map_loading_failure(map->runtime, map);
  map->map->getStyle().loadURL(url);
  if (runtime_map_loading_failed(map->runtime, map)) {
    set_thread_error(
      runtime_map_loading_failure_message(map->runtime, map).c_str()
    );
    return MLN_STATUS_NATIVE_ERROR;
  }
  return MLN_STATUS_OK;
}

auto map_set_style_json(mln_map* map, const char* json) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (json == nullptr) {
    set_thread_error("json must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  try {
    clear_runtime_map_loading_failure(map->runtime, map);
    map->map->getStyle().loadJSON(json);
  } catch (const std::exception& exception) {
    set_thread_error(exception.what());
    push_runtime_map_event(
      map->runtime, map, MLN_RUNTIME_EVENT_MAP_LOADING_FAILED, 0,
      exception.what()
    );
    return MLN_STATUS_NATIVE_ERROR;
  }
  if (runtime_map_loading_failed(map->runtime, map)) {
    set_thread_error(
      runtime_map_loading_failure_message(map->runtime, map).c_str()
    );
    return MLN_STATUS_NATIVE_ERROR;
  }
  return MLN_STATUS_OK;
}

auto style_id_list_count(const mln_style_id_list* list, size_t* out_count)
  -> mln_status {
  if (list == nullptr) {
    set_thread_error("style ID list must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_count == nullptr) {
    set_thread_error("out_count must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto lock = std::scoped_lock{style_id_list_registry_mutex()};
  const auto* live_list = find_style_id_list_locked(list);
  if (live_list == nullptr) {
    set_thread_error("style ID list is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_count = live_list->ids.size();
  return MLN_STATUS_OK;
}

auto style_id_list_get(
  const mln_style_id_list* list, size_t index, mln_string_view* out_id
) -> mln_status {
  if (list == nullptr) {
    set_thread_error("style ID list must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_id == nullptr) {
    set_thread_error("out_id must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto lock = std::scoped_lock{style_id_list_registry_mutex()};
  const auto* live_list = find_style_id_list_locked(list);
  if (live_list == nullptr) {
    set_thread_error("style ID list is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (index >= live_list->ids.size()) {
    set_thread_error("index is out of range");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_id = string_view_from_string(live_list->ids.at(index));
  return MLN_STATUS_OK;
}

auto style_id_list_destroy(mln_style_id_list* list) -> void {
  if (list == nullptr) {
    return;
  }
  const auto lock = std::scoped_lock{style_id_list_registry_mutex()};
  style_id_list_registry().erase(list);
}

auto map_add_style_source_json(
  mln_map* map, mln_string_view source_id, const mln_json_value* source_json
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!validate_style_json_value(source_json)) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  if (style.getSource(id) != nullptr) {
    set_thread_error("source already exists");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto error = mbgl::style::conversion::Error{};
  auto source =
    mbgl::style::conversion::convert<std::unique_ptr<mbgl::style::Source>>(
      mbgl::style::conversion::Convertible{source_json}, error, id
    );
  if (!source) {
    set_style_conversion_error("style source", error);
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  style.addSource(std::move(*source));
  return MLN_STATUS_OK;
}

auto map_remove_style_source(
  mln_map* map, mln_string_view source_id, bool* out_removed
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_removed == nullptr) {
    set_thread_error("out_removed must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  if (style.getSource(id) == nullptr) {
    *out_removed = false;
    return MLN_STATUS_OK;
  }

  auto removed = style.removeSource(id);
  if (!removed) {
    set_thread_error("source is used by a layer");
    return MLN_STATUS_INVALID_STATE;
  }
  *out_removed = true;
  return MLN_STATUS_OK;
}

auto map_style_source_exists(
  mln_map* map, mln_string_view source_id, bool* out_exists
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_exists == nullptr) {
    set_thread_error("out_exists must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_exists =
    map->map->getStyle().getSource(string_from_view(source_id)) != nullptr;
  return MLN_STATUS_OK;
}

auto map_get_style_source_type(
  mln_map* map, mln_string_view source_id, uint32_t* out_source_type,
  bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_source_type == nullptr || out_found == nullptr) {
    set_thread_error("out_source_type and out_found must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto* source =
    map->map->getStyle().getSource(string_from_view(source_id));
  *out_found = source != nullptr;
  *out_source_type = MLN_STYLE_SOURCE_TYPE_UNKNOWN;
  if (source != nullptr) {
    *out_source_type = to_c_source_type(source->getType());
  }
  return MLN_STATUS_OK;
}

auto map_get_style_source_info(
  mln_map* map, mln_string_view source_id, mln_style_source_info* out_info,
  bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_info == nullptr || out_info->size < sizeof(mln_style_source_info)) {
    set_thread_error("out_info must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_found == nullptr) {
    set_thread_error("out_found must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto* source =
    map->map->getStyle().getSource(string_from_view(source_id));
  *out_found = source != nullptr;
  *out_info = mln_style_source_info{
    .size = sizeof(mln_style_source_info),
    .type = MLN_STYLE_SOURCE_TYPE_UNKNOWN,
    .id_size = 0,
    .is_volatile = false,
    .has_attribution = false,
    .attribution_size = 0
  };
  if (source == nullptr) {
    return MLN_STATUS_OK;
  }

  const auto attribution = source->getAttribution();
  *out_info = mln_style_source_info{
    .size = sizeof(mln_style_source_info),
    .type = to_c_source_type(source->getType()),
    .id_size = source->getID().size(),
    .is_volatile = source->isVolatile(),
    .has_attribution = attribution.has_value(),
    .attribution_size = attribution ? attribution->size() : 0
  };
  return MLN_STATUS_OK;
}

auto map_copy_style_source_attribution(
  mln_map* map, mln_string_view source_id, char* out_attribution,
  size_t attribution_capacity, size_t* out_attribution_size, bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(source_id, "source_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (source_id.size == 0) {
    set_thread_error("source_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_attribution == nullptr && attribution_capacity > 0) {
    set_thread_error(
      "out_attribution must not be null when capacity is non-zero"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_attribution_size == nullptr || out_found == nullptr) {
    set_thread_error("out_attribution_size and out_found must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto* source =
    map->map->getStyle().getSource(string_from_view(source_id));
  *out_found = source != nullptr;
  *out_attribution_size = 0;
  if (source == nullptr) {
    return MLN_STATUS_OK;
  }

  const auto attribution = source->getAttribution();
  if (!attribution) {
    return MLN_STATUS_OK;
  }
  *out_attribution_size = attribution->size();
  if (attribution_capacity < attribution->size()) {
    set_thread_error("attribution_capacity is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!attribution->empty()) {
    std::copy(attribution->begin(), attribution->end(), out_attribution);
  }
  return MLN_STATUS_OK;
}

auto map_list_style_source_ids(mln_map* map, mln_style_id_list** out_source_ids)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  auto ids = std::vector<std::string>{};
  for (const auto* source : map->map->getStyle().getSources()) {
    ids.push_back(source->getID());
  }
  return create_style_id_list(std::move(ids), out_source_ids);
}

auto map_add_geojson_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }
  if (!validate_string_view(url, "url")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (url.size == 0) {
    set_thread_error("url must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  const auto add_status = validate_source_can_be_added(style, id);
  if (add_status != MLN_STATUS_OK) {
    return add_status;
  }

  auto source = std::make_unique<mbgl::style::GeoJSONSource>(id);
  source->setURL(string_from_view(url));
  style.addSource(std::move(source));
  return MLN_STATUS_OK;
}

auto map_add_geojson_source_data(
  mln_map* map, mln_string_view source_id, const mln_geojson* data
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }

  auto geojson = to_native_geojson(data);
  if (!geojson) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  const auto add_status = validate_source_can_be_added(style, id);
  if (add_status != MLN_STATUS_OK) {
    return add_status;
  }

  auto source = std::make_unique<mbgl::style::GeoJSONSource>(id);
  source->setGeoJSON(*geojson);
  style.addSource(std::move(source));
  return MLN_STATUS_OK;
}

auto map_set_geojson_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }
  if (!validate_string_view(url, "url")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (url.size == 0) {
    set_thread_error("url must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* source = map->map->getStyle().getSource(string_from_view(source_id));
  if (source == nullptr) {
    set_thread_error("source does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto* geojson_source = source->as<mbgl::style::GeoJSONSource>();
  if (geojson_source == nullptr) {
    set_thread_error("source is not a GeoJSON source");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  geojson_source->setURL(string_from_view(url));
  return MLN_STATUS_OK;
}

auto map_set_geojson_source_data(
  mln_map* map, mln_string_view source_id, const mln_geojson* data
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }

  auto geojson = to_native_geojson(data);
  if (!geojson) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* source = map->map->getStyle().getSource(string_from_view(source_id));
  if (source == nullptr) {
    set_thread_error("source does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto* geojson_source = source->as<mbgl::style::GeoJSONSource>();
  if (geojson_source == nullptr) {
    set_thread_error("source is not a GeoJSON source");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  geojson_source->setGeoJSON(*geojson);
  return MLN_STATUS_OK;
}

auto map_add_vector_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }
  if (!validate_string_view(url, "url")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (url.size == 0) {
    set_thread_error("url must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto options_status = validate_tile_source_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  const auto add_status = validate_source_can_be_added(style, id);
  if (add_status != MLN_STATUS_OK) {
    return add_status;
  }

  const auto effective = effective_tile_source_options(options);
  auto min_zoom = std::optional<float>{};
  if (
    has_tile_source_option(effective, MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM)
  ) {
    min_zoom = static_cast<float>(effective.min_zoom);
  }
  auto max_zoom = std::optional<float>{};
  if (
    has_tile_source_option(effective, MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM)
  ) {
    max_zoom = static_cast<float>(effective.max_zoom);
  }

  if (
    has_tile_source_option(
      effective, MLN_STYLE_TILE_SOURCE_OPTION_VECTOR_ENCODING
    )
  ) {
    style.addSource(
      std::make_unique<mbgl::style::VectorSource>(
        id, string_from_view(url), max_zoom, min_zoom,
        to_native_vector_encoding(effective.vector_encoding)
      )
    );
  } else {
    style.addSource(
      std::make_unique<mbgl::style::VectorSource>(
        id, string_from_view(url), max_zoom, min_zoom
      )
    );
  }
  return MLN_STATUS_OK;
}

auto map_add_vector_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }
  const auto tiles_status = validate_tile_urls(tiles, tile_count);
  if (tiles_status != MLN_STATUS_OK) {
    return tiles_status;
  }
  const auto options_status = validate_tile_source_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  const auto add_status = validate_source_can_be_added(style, id);
  if (add_status != MLN_STATUS_OK) {
    return add_status;
  }

  const auto effective = effective_tile_source_options(options);
  auto tileset = to_native_tileset(tiles, tile_count, effective, true);
  if (!tileset) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  style.addSource(
    std::make_unique<mbgl::style::VectorSource>(
      id, *tileset, std::nullopt, std::nullopt,
      to_native_vector_encoding(effective.vector_encoding)
    )
  );
  return MLN_STATUS_OK;
}

auto map_add_raster_source_url(
  mln_map* map, mln_string_view source_id, mln_string_view url,
  const mln_style_tile_source_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }
  if (!validate_string_view(url, "url")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (url.size == 0) {
    set_thread_error("url must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto options_status = validate_tile_source_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  const auto add_status = validate_source_can_be_added(style, id);
  if (add_status != MLN_STATUS_OK) {
    return add_status;
  }

  const auto effective = effective_tile_source_options(options);
  style.addSource(
    std::make_unique<mbgl::style::RasterSource>(
      id, string_from_view(url), static_cast<uint16_t>(effective.tile_size)
    )
  );
  return MLN_STATUS_OK;
}

auto map_add_raster_source_tiles(
  mln_map* map, mln_string_view source_id, const mln_string_view* tiles,
  size_t tile_count, const mln_style_tile_source_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto source_id_status = validate_source_id(source_id);
  if (source_id_status != MLN_STATUS_OK) {
    return source_id_status;
  }
  const auto tiles_status = validate_tile_urls(tiles, tile_count);
  if (tiles_status != MLN_STATUS_OK) {
    return tiles_status;
  }
  const auto options_status = validate_tile_source_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(source_id);
  const auto add_status = validate_source_can_be_added(style, id);
  if (add_status != MLN_STATUS_OK) {
    return add_status;
  }

  const auto effective = effective_tile_source_options(options);
  auto tileset = to_native_tileset(tiles, tile_count, effective, false);
  if (!tileset) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  style.addSource(
    std::make_unique<mbgl::style::RasterSource>(
      id, *tileset, static_cast<uint16_t>(effective.tile_size)
    )
  );
  return MLN_STATUS_OK;
}

auto map_set_style_image(
  mln_map* map, mln_string_view image_id,
  const mln_premultiplied_rgba8_image* image,
  const mln_style_image_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto image_id_status = validate_image_id(image_id);
  if (image_id_status != MLN_STATUS_OK) {
    return image_id_status;
  }
  const auto image_status = validate_premultiplied_rgba8_image(image);
  if (image_status != MLN_STATUS_OK) {
    return image_status;
  }
  const auto options_status = validate_style_image_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  const auto effective = effective_style_image_options(options);
  auto style_image = std::make_unique<mbgl::style::Image>(
    string_from_view(image_id), to_native_premultiplied_rgba8_image(*image),
    effective.pixel_ratio, effective.sdf
  );
  map->map->getStyle().addImage(std::move(style_image));
  return MLN_STATUS_OK;
}

auto map_remove_style_image(
  mln_map* map, mln_string_view image_id, bool* out_removed
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto image_id_status = validate_image_id(image_id);
  if (image_id_status != MLN_STATUS_OK) {
    return image_id_status;
  }
  if (out_removed == nullptr) {
    set_thread_error("out_removed must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(image_id);
  *out_removed = style.getImage(id).has_value();
  if (*out_removed) {
    style.removeImage(id);
  }
  return MLN_STATUS_OK;
}

auto map_style_image_exists(
  mln_map* map, mln_string_view image_id, bool* out_exists
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto image_id_status = validate_image_id(image_id);
  if (image_id_status != MLN_STATUS_OK) {
    return image_id_status;
  }
  if (out_exists == nullptr) {
    set_thread_error("out_exists must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_exists =
    map->map->getStyle().getImage(string_from_view(image_id)).has_value();
  return MLN_STATUS_OK;
}

auto map_get_style_image_info(
  mln_map* map, mln_string_view image_id, mln_style_image_info* out_info,
  bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto image_id_status = validate_image_id(image_id);
  if (image_id_status != MLN_STATUS_OK) {
    return image_id_status;
  }
  if (out_info == nullptr || out_info->size < sizeof(mln_style_image_info)) {
    set_thread_error("out_info must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_found == nullptr) {
    set_thread_error("out_found must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto image = map->map->getStyle().getImage(string_from_view(image_id));
  *out_found = image.has_value();
  *out_info =
    image ? style_image_info_from_native(*image) : style_image_info_default();
  return MLN_STATUS_OK;
}

auto map_copy_style_image_premultiplied_rgba8(
  mln_map* map, mln_string_view image_id, uint8_t* out_pixels,
  size_t pixel_capacity, size_t* out_byte_length, bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto image_id_status = validate_image_id(image_id);
  if (image_id_status != MLN_STATUS_OK) {
    return image_id_status;
  }
  if (out_pixels == nullptr && pixel_capacity > 0) {
    set_thread_error("out_pixels must not be null when capacity is non-zero");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_byte_length == nullptr || out_found == nullptr) {
    set_thread_error("out_byte_length and out_found must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto image = map->map->getStyle().getImage(string_from_view(image_id));
  *out_found = image.has_value();
  *out_byte_length = 0;
  if (!image) {
    return MLN_STATUS_OK;
  }

  const auto& pixels = image->getImage();
  *out_byte_length = pixels.bytes();
  if (pixel_capacity < pixels.bytes()) {
    set_thread_error("pixel_capacity is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (pixels.bytes() > 0) {
    std::copy_n(pixels.data.get(), pixels.bytes(), out_pixels);
  }
  return MLN_STATUS_OK;
}

auto map_add_style_layer_json(
  mln_map* map, const mln_json_value* layer_json,
  mln_string_view before_layer_id
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(before_layer_id, "before_layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!validate_style_json_value(layer_json)) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  auto before = std::optional<std::string>{};
  if (before_layer_id.size > 0) {
    before = string_from_view(before_layer_id);
    if (style.getLayer(*before) == nullptr) {
      set_thread_error("before_layer_id does not exist");
      return MLN_STATUS_INVALID_ARGUMENT;
    }
  }

  auto error = mbgl::style::conversion::Error{};
  auto layer =
    mbgl::style::conversion::convert<std::unique_ptr<mbgl::style::Layer>>(
      mbgl::style::conversion::Convertible{layer_json}, error
    );
  if (!layer) {
    set_style_conversion_error("style layer", error);
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto id = (*layer)->getID();
  if (style.getLayer(id) != nullptr) {
    set_thread_error("layer already exists");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    (*layer)->getTypeInfo()->source ==
      mbgl::style::LayerTypeInfo::Source::Required &&
    style.getSource((*layer)->getSourceID()) == nullptr
  ) {
    set_thread_error("layer source does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  style.addLayer(std::move(*layer), before);
  return MLN_STATUS_OK;
}

auto map_remove_style_layer(
  mln_map* map, mln_string_view layer_id, bool* out_removed
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(layer_id, "layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_removed == nullptr) {
    set_thread_error("out_removed must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto removed = map->map->getStyle().removeLayer(string_from_view(layer_id));
  *out_removed = removed != nullptr;
  return MLN_STATUS_OK;
}

auto map_style_layer_exists(
  mln_map* map, mln_string_view layer_id, bool* out_exists
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(layer_id, "layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_exists == nullptr) {
    set_thread_error("out_exists must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_exists =
    map->map->getStyle().getLayer(string_from_view(layer_id)) != nullptr;
  return MLN_STATUS_OK;
}

auto map_get_style_layer_type(
  mln_map* map, mln_string_view layer_id, mln_string_view* out_layer_type,
  bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(layer_id, "layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_layer_type == nullptr || out_found == nullptr) {
    set_thread_error("out_layer_type and out_found must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto* layer = map->map->getStyle().getLayer(string_from_view(layer_id));
  *out_found = layer != nullptr;
  *out_layer_type = {};
  if (layer != nullptr) {
    *out_layer_type = string_view_from_literal(layer->getTypeInfo()->type);
  }
  return MLN_STATUS_OK;
}

auto map_list_style_layer_ids(mln_map* map, mln_style_id_list** out_layer_ids)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  auto ids = std::vector<std::string>{};
  for (const auto* layer : map->map->getStyle().getLayers()) {
    ids.push_back(layer->getID());
  }
  return create_style_id_list(std::move(ids), out_layer_ids);
}

auto map_move_style_layer(
  mln_map* map, mln_string_view layer_id, mln_string_view before_layer_id
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    !validate_string_view(layer_id, "layer_id") ||
    !validate_string_view(before_layer_id, "before_layer_id")
  ) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto& style = map->map->getStyle();
  const auto id = string_from_view(layer_id);
  if (style.getLayer(id) == nullptr) {
    set_thread_error("layer does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto before = std::optional<std::string>{};
  if (before_layer_id.size > 0) {
    before = string_from_view(before_layer_id);
    if (*before == id) {
      return MLN_STATUS_OK;
    }
    if (style.getLayer(*before) == nullptr) {
      set_thread_error("before_layer_id does not exist");
      return MLN_STATUS_INVALID_ARGUMENT;
    }
  }

  auto layer = style.removeLayer(id);
  if (!layer) {
    set_thread_error("layer does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  style.addLayer(std::move(layer), before);
  return MLN_STATUS_OK;
}

auto map_get_style_layer_json(
  mln_map* map, mln_string_view layer_id, mln_json_snapshot** out_layer,
  bool* out_found
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(layer_id, "layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_layer == nullptr || *out_layer != nullptr || out_found == nullptr) {
    set_thread_error(
      "out_layer must not be null, *out_layer must be null, and out_found must "
      "not be null"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto* layer = map->map->getStyle().getLayer(string_from_view(layer_id));
  *out_found = layer != nullptr;
  if (layer == nullptr) {
    return MLN_STATUS_OK;
  }
  return json_snapshot_create(layer->serialize(), out_layer);
}

auto map_set_style_light_json(mln_map* map, const mln_json_value* light_json)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_style_json_value(light_json)) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto error = mbgl::style::conversion::Error{};
  auto light = mbgl::style::conversion::convert<mbgl::style::Light>(
    mbgl::style::conversion::Convertible{light_json}, error
  );
  if (!light) {
    set_style_conversion_error("style light", error);
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  map->map->getStyle().setLight(std::make_unique<mbgl::style::Light>(*light));
  return MLN_STATUS_OK;
}

auto map_set_style_light_property(
  mln_map* map, mln_string_view property_name, const mln_json_value* value
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(property_name, "property_name")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (property_name.size == 0) {
    set_thread_error("property_name must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!validate_style_json_value(value)) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* light = map->map->getStyle().getLight();
  if (light == nullptr) {
    set_thread_error("style light does not exist");
    return MLN_STATUS_INVALID_STATE;
  }

  auto error = light->setProperty(
    string_from_view(property_name), mbgl::style::conversion::Convertible{value}
  );
  if (error) {
    set_style_conversion_error("style light property", *error);
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto map_get_style_light_property(
  mln_map* map, mln_string_view property_name, mln_json_snapshot** out_value
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(property_name, "property_name")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (property_name.size == 0) {
    set_thread_error("property_name must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_value == nullptr || *out_value != nullptr) {
    set_thread_error("out_value must not be null and *out_value must be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* light = map->map->getStyle().getLight();
  if (light == nullptr) {
    set_thread_error("style light does not exist");
    return MLN_STATUS_INVALID_STATE;
  }

  const auto property = light->getProperty(string_from_view(property_name));
  if (property.getKind() == mbgl::style::StyleProperty::Kind::Undefined) {
    return MLN_STATUS_OK;
  }
  return json_snapshot_create(property.getValue(), out_value);
}

auto map_set_layer_property(
  mln_map* map, mln_string_view layer_id, mln_string_view property_name,
  const mln_json_value* value
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    !validate_string_view(layer_id, "layer_id") ||
    !validate_string_view(property_name, "property_name")
  ) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0 || property_name.size == 0) {
    set_thread_error("layer_id and property_name must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (!validate_style_json_value(value)) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* layer = map->map->getStyle().getLayer(string_from_view(layer_id));
  if (layer == nullptr) {
    set_thread_error("layer does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto error = layer->setProperty(
    string_from_view(property_name), mbgl::style::conversion::Convertible{value}
  );
  if (error) {
    set_style_conversion_error("layer property", *error);
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

auto map_get_layer_property(
  mln_map* map, mln_string_view layer_id, mln_string_view property_name,
  mln_json_snapshot** out_value
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    !validate_string_view(layer_id, "layer_id") ||
    !validate_string_view(property_name, "property_name")
  ) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0 || property_name.size == 0) {
    set_thread_error("layer_id and property_name must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_value == nullptr || *out_value != nullptr) {
    set_thread_error("out_value must not be null and *out_value must be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* layer = map->map->getStyle().getLayer(string_from_view(layer_id));
  if (layer == nullptr) {
    set_thread_error("layer does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto property = layer->getProperty(string_from_view(property_name));
  const auto value =
    property.getKind() == mbgl::style::StyleProperty::Kind::Undefined
      ? mbgl::Value{mbgl::NullValue{}}
      : property.getValue();
  return json_snapshot_create(value, out_value);
}

auto map_set_layer_filter(
  mln_map* map, mln_string_view layer_id, const mln_json_value* filter
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(layer_id, "layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* layer = map->map->getStyle().getLayer(string_from_view(layer_id));
  if (layer == nullptr) {
    set_thread_error("layer does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (filter == nullptr) {
    layer->setFilter(mbgl::style::Filter{});
    return MLN_STATUS_OK;
  }

  auto native_filter = to_native_style_filter(filter);
  if (!native_filter) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  layer->setFilter(*native_filter);
  return MLN_STATUS_OK;
}

auto map_get_layer_filter(
  mln_map* map, mln_string_view layer_id, mln_json_snapshot** out_filter
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!validate_string_view(layer_id, "layer_id")) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (layer_id.size == 0) {
    set_thread_error("layer_id must not be empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_filter == nullptr || *out_filter != nullptr) {
    set_thread_error(
      "out_filter must not be null and *out_filter must be null"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto* layer = map->map->getStyle().getLayer(string_from_view(layer_id));
  if (layer == nullptr) {
    set_thread_error("layer does not exist");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return json_snapshot_create(layer->getFilter().serialize(), out_filter);
}

auto map_get_camera(mln_map* map, mln_camera_options* out_camera)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_camera == nullptr || out_camera->size < sizeof(mln_camera_options)) {
    set_thread_error("out_camera must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_camera = from_native_camera(map->map->getCameraOptions());
  return MLN_STATUS_OK;
}

auto map_jump_to(mln_map* map, const mln_camera_options* camera) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto camera_status = validate_camera_options(camera);
  if (camera_status != MLN_STATUS_OK) {
    return camera_status;
  }
  map->map->jumpTo(to_native_camera(*camera));
  return MLN_STATUS_OK;
}

auto map_ease_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto camera_status = validate_camera_options(camera);
  if (camera_status != MLN_STATUS_OK) {
    return camera_status;
  }
  const auto animation_status = validate_animation_options(animation);
  if (animation_status != MLN_STATUS_OK) {
    return animation_status;
  }

  map->map->easeTo(to_native_camera(*camera), to_native_animation(animation));
  return MLN_STATUS_OK;
}

auto map_fly_to(
  mln_map* map, const mln_camera_options* camera,
  const mln_animation_options* animation
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto camera_status = validate_camera_options(camera);
  if (camera_status != MLN_STATUS_OK) {
    return camera_status;
  }
  const auto animation_status = validate_animation_options(animation);
  if (animation_status != MLN_STATUS_OK) {
    return animation_status;
  }

  map->map->flyTo(to_native_camera(*camera), to_native_animation(animation));
  return MLN_STATUS_OK;
}

auto map_get_projection_mode(mln_map* map, mln_projection_mode* out_mode)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_mode == nullptr || out_mode->size < sizeof(mln_projection_mode)) {
    set_thread_error("out_mode must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_mode = from_native_projection_mode(map->map->getProjectionMode());
  return MLN_STATUS_OK;
}

auto map_set_projection_mode(mln_map* map, const mln_projection_mode* mode)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto mode_status = validate_projection_mode_options(mode);
  if (mode_status != MLN_STATUS_OK) {
    return mode_status;
  }

  map->map->setProjectionMode(to_native_projection_mode(*mode));
  return MLN_STATUS_OK;
}

auto map_set_debug_options(mln_map* map, uint32_t options) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto options_status = validate_debug_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }
  map->map->setDebug(to_native_debug_options(options));
  return MLN_STATUS_OK;
}

auto map_get_debug_options(mln_map* map, uint32_t* out_options) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_options == nullptr) {
    set_thread_error("out_options must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_options = from_native_debug_options(map->map->getDebug());
  return MLN_STATUS_OK;
}

auto map_set_rendering_stats_view_enabled(mln_map* map, bool enabled)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  map->map->enableRenderingStatsView(enabled);
  return MLN_STATUS_OK;
}

auto map_get_rendering_stats_view_enabled(mln_map* map, bool* out_enabled)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_enabled == nullptr) {
    set_thread_error("out_enabled must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_enabled = map->map->isRenderingStatsViewEnabled();
  return MLN_STATUS_OK;
}

auto map_is_fully_loaded(mln_map* map, bool* out_loaded) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_loaded == nullptr) {
    set_thread_error("out_loaded must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_loaded = map->map->isFullyLoaded();
  return MLN_STATUS_OK;
}

auto map_dump_debug_logs(mln_map* map) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  map->map->dumpDebugLogs();
  return MLN_STATUS_OK;
}

auto map_get_viewport_options(
  mln_map* map, mln_map_viewport_options* out_options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_options == nullptr ||
    out_options->size < sizeof(mln_map_viewport_options)
  ) {
    set_thread_error("out_options must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto options = map->map->getMapOptions();
  *out_options = mln_map_viewport_options{
    .size = sizeof(mln_map_viewport_options),
    .fields = static_cast<uint32_t>(MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION) |
              MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE |
              MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE |
              MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET,
    .north_orientation =
      from_native_north_orientation(options.northOrientation()),
    .constrain_mode = from_native_constrain_mode(options.constrainMode()),
    .viewport_mode = from_native_viewport_mode(options.viewportMode()),
    .frustum_offset = from_native_edge_insets(map->map->getFrustumOffset())
  };
  return MLN_STATUS_OK;
}

auto map_set_viewport_options(
  mln_map* map, const mln_map_viewport_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto options_status = validate_viewport_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION) != 0U) {
    map->map->setNorthOrientation(
      to_native_north_orientation(options->north_orientation)
    );
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE) != 0U) {
    map->map->setConstrainMode(
      to_native_constrain_mode(options->constrain_mode)
    );
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE) != 0U) {
    map->map->setViewportMode(to_native_viewport_mode(options->viewport_mode));
  }
  if ((options->fields & MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET) != 0U) {
    map->map->setFrustumOffset(to_native_edge_insets(options->frustum_offset));
  }
  return MLN_STATUS_OK;
}

auto map_get_tile_options(mln_map* map, mln_map_tile_options* out_options)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_options == nullptr || out_options->size < sizeof(mln_map_tile_options)
  ) {
    set_thread_error("out_options must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_options = mln_map_tile_options{
    .size = sizeof(mln_map_tile_options),
    .fields = static_cast<uint32_t>(MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA) |
              MLN_MAP_TILE_OPTION_LOD_MIN_RADIUS |
              MLN_MAP_TILE_OPTION_LOD_SCALE |
              MLN_MAP_TILE_OPTION_LOD_PITCH_THRESHOLD |
              MLN_MAP_TILE_OPTION_LOD_ZOOM_SHIFT | MLN_MAP_TILE_OPTION_LOD_MODE,
    .prefetch_zoom_delta = map->map->getPrefetchZoomDelta(),
    .lod_min_radius = map->map->getTileLodMinRadius(),
    .lod_scale = map->map->getTileLodScale(),
    .lod_pitch_threshold = map->map->getTileLodPitchThreshold(),
    .lod_zoom_shift = map->map->getTileLodZoomShift(),
    .lod_mode = from_native_tile_lod_mode(map->map->getTileLodMode())
  };
  return MLN_STATUS_OK;
}

auto map_set_tile_options(mln_map* map, const mln_map_tile_options* options)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto options_status = validate_tile_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  if ((options->fields & MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA) != 0U) {
    map->map->setPrefetchZoomDelta(
      static_cast<uint8_t>(options->prefetch_zoom_delta)
    );
  }
  if ((options->fields & MLN_MAP_TILE_OPTION_LOD_MIN_RADIUS) != 0U) {
    map->map->setTileLodMinRadius(options->lod_min_radius);
  }
  if ((options->fields & MLN_MAP_TILE_OPTION_LOD_SCALE) != 0U) {
    map->map->setTileLodScale(options->lod_scale);
  }
  if ((options->fields & MLN_MAP_TILE_OPTION_LOD_PITCH_THRESHOLD) != 0U) {
    map->map->setTileLodPitchThreshold(options->lod_pitch_threshold);
  }
  if ((options->fields & MLN_MAP_TILE_OPTION_LOD_ZOOM_SHIFT) != 0U) {
    map->map->setTileLodZoomShift(options->lod_zoom_shift);
  }
  if ((options->fields & MLN_MAP_TILE_OPTION_LOD_MODE) != 0U) {
    map->map->setTileLodMode(to_native_tile_lod_mode(options->lod_mode));
  }
  return MLN_STATUS_OK;
}

auto map_pixel_for_lat_lng(
  mln_map* map, mln_lat_lng coordinate, mln_screen_point* out_point
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_point == nullptr) {
    set_thread_error("out_point must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto coordinate_status = validate_lat_lng(coordinate);
  if (coordinate_status != MLN_STATUS_OK) {
    return coordinate_status;
  }

  *out_point = from_native_screen_point(
    map->map->pixelForLatLng(to_native_lat_lng(coordinate))
  );
  return MLN_STATUS_OK;
}

auto map_lat_lng_for_pixel(
  mln_map* map, mln_screen_point point, mln_lat_lng* out_coordinate
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_coordinate == nullptr) {
    set_thread_error("out_coordinate must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto point_status = validate_screen_point(point);
  if (point_status != MLN_STATUS_OK) {
    return point_status;
  }

  *out_coordinate = from_native_lat_lng(
    map->map->latLngForPixel(to_native_screen_point(point))
  );
  return MLN_STATUS_OK;
}

auto map_pixels_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  mln_screen_point* out_points
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (coordinate_count != 0 && out_points == nullptr) {
    set_thread_error(
      "out_points must not be null when coordinate_count is nonzero"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto coordinates_status =
    validate_lat_lng_array(coordinates, coordinate_count, true);
  if (coordinates_status != MLN_STATUS_OK) {
    return coordinates_status;
  }
  if (coordinate_count == 0) {
    return MLN_STATUS_OK;
  }

  const auto native_coordinates =
    to_native_lat_lngs(coordinates, coordinate_count);
  const auto pixels = map->map->pixelsForLatLngs(native_coordinates);
  auto output = std::span<mln_screen_point>{out_points, pixels.size()};
  auto output_position = output.begin();
  for (const auto& pixel : pixels) {
    *output_position = from_native_screen_point(pixel);
    ++output_position;
  }
  return MLN_STATUS_OK;
}

auto map_lat_lngs_for_pixels(
  mln_map* map, const mln_screen_point* points, size_t point_count,
  mln_lat_lng* out_coordinates
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (point_count != 0 && out_coordinates == nullptr) {
    set_thread_error(
      "out_coordinates must not be null when point_count is nonzero"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto points_status = validate_screen_point_array(points, point_count);
  if (points_status != MLN_STATUS_OK) {
    return points_status;
  }
  if (point_count == 0) {
    return MLN_STATUS_OK;
  }

  const auto native_points = to_native_screen_points(points, point_count);
  const auto coordinates = map->map->latLngsForPixels(native_points);
  auto output = std::span<mln_lat_lng>{out_coordinates, coordinates.size()};
  auto output_position = output.begin();
  for (const auto& coordinate : coordinates) {
    *output_position = from_native_lat_lng(coordinate);
    ++output_position;
  }
  return MLN_STATUS_OK;
}

auto map_projection_create(mln_map* map, mln_map_projection** out_projection)
  -> mln_status {
  if (out_projection == nullptr) {
    set_thread_error("out_projection must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_projection != nullptr) {
    set_thread_error("out_projection must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  auto owned_projection = std::make_unique<mln_map_projection>();
  owned_projection->owner_thread = std::this_thread::get_id();
  owned_projection->projection =
    std::make_unique<mbgl::MapProjection>(*map->map);

  auto* handle = owned_projection.get();
  {
    const std::scoped_lock lock(map_projection_registry_mutex());
    map_projection_registry().emplace(handle, std::move(owned_projection));
  }
  *out_projection = handle;
  return MLN_STATUS_OK;
}

auto map_projection_destroy(mln_map_projection* projection) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  auto owned_projection = std::unique_ptr<mln_map_projection>{};
  {
    const std::scoped_lock lock(map_projection_registry_mutex());
    const auto found = map_projection_registry().find(projection);
    owned_projection = std::move(found->second);
    map_projection_registry().erase(found);
  }
  owned_projection.reset();
  return MLN_STATUS_OK;
}

auto map_projection_get_camera(
  mln_map_projection* projection, mln_camera_options* out_camera
) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_camera == nullptr || out_camera->size < sizeof(mln_camera_options)) {
    set_thread_error("out_camera must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_camera = from_native_camera(projection->projection->getCamera());
  return MLN_STATUS_OK;
}

auto map_projection_set_camera(
  mln_map_projection* projection, const mln_camera_options* camera
) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto camera_status = validate_camera_options(camera);
  if (camera_status != MLN_STATUS_OK) {
    return camera_status;
  }

  projection->projection->setCamera(to_native_camera(*camera));
  return MLN_STATUS_OK;
}

auto map_projection_set_visible_coordinates(
  mln_map_projection* projection, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_edge_insets padding
) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto coordinates_status =
    validate_lat_lng_array(coordinates, coordinate_count, false);
  if (coordinates_status != MLN_STATUS_OK) {
    return coordinates_status;
  }
  const auto padding_status = validate_edge_insets(padding);
  if (padding_status != MLN_STATUS_OK) {
    return padding_status;
  }

  projection->projection->setVisibleCoordinates(
    to_native_lat_lngs(coordinates, coordinate_count),
    to_native_edge_insets(padding)
  );
  return MLN_STATUS_OK;
}

auto map_projection_set_visible_geometry(
  mln_map_projection* projection, const mln_geometry* geometry,
  mln_edge_insets padding
) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto padding_status = validate_edge_insets(padding);
  if (padding_status != MLN_STATUS_OK) {
    return padding_status;
  }
  auto native_geometry = to_native_geometry(geometry);
  if (!native_geometry) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto coordinates = geometry_lat_lngs(*native_geometry);
  if (coordinates.empty()) {
    set_thread_error("geometry must contain at least one coordinate");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  projection->projection->setVisibleCoordinates(
    coordinates, to_native_edge_insets(padding)
  );
  return MLN_STATUS_OK;
}

auto map_projection_pixel_for_lat_lng(
  mln_map_projection* projection, mln_lat_lng coordinate,
  mln_screen_point* out_point
) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_point == nullptr) {
    set_thread_error("out_point must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto coordinate_status = validate_lat_lng(coordinate);
  if (coordinate_status != MLN_STATUS_OK) {
    return coordinate_status;
  }

  *out_point = from_native_screen_point(
    projection->projection->pixelForLatLng(to_native_lat_lng(coordinate))
  );
  return MLN_STATUS_OK;
}

auto map_projection_lat_lng_for_pixel(
  mln_map_projection* projection, mln_screen_point point,
  mln_lat_lng* out_coordinate
) -> mln_status {
  const auto status = validate_map_projection(projection);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_coordinate == nullptr) {
    set_thread_error("out_coordinate must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto point_status = validate_screen_point(point);
  if (point_status != MLN_STATUS_OK) {
    return point_status;
  }

  *out_coordinate = from_native_lat_lng(
    projection->projection->latLngForPixel(to_native_screen_point(point))
  );
  return MLN_STATUS_OK;
}

auto projected_meters_for_lat_lng(
  mln_lat_lng coordinate, mln_projected_meters* out_meters
) -> mln_status {
  if (out_meters == nullptr) {
    set_thread_error("out_meters must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto coordinate_status = validate_lat_lng(coordinate);
  if (coordinate_status != MLN_STATUS_OK) {
    return coordinate_status;
  }

  const auto meters =
    mbgl::Projection::projectedMetersForLatLng(to_native_lat_lng(coordinate));
  *out_meters = mln_projected_meters{
    .northing = meters.northing(), .easting = meters.easting()
  };
  return MLN_STATUS_OK;
}

auto lat_lng_for_projected_meters(
  mln_projected_meters meters, mln_lat_lng* out_coordinate
) -> mln_status {
  if (out_coordinate == nullptr) {
    set_thread_error("out_coordinate must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto meters_status = validate_projected_meters(meters);
  if (meters_status != MLN_STATUS_OK) {
    return meters_status;
  }

  *out_coordinate = from_native_lat_lng(
    mbgl::Projection::latLngForProjectedMeters(
      mbgl::ProjectedMeters{meters.northing, meters.easting}
    )
  );
  return MLN_STATUS_OK;
}

auto map_move_by(mln_map* map, double delta_x, double delta_y) -> mln_status {
  return map_move_by_animated(map, delta_x, delta_y, nullptr);
}

auto map_move_by_animated(
  mln_map* map, double delta_x, double delta_y,
  const mln_animation_options* animation
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!std::isfinite(delta_x) || !std::isfinite(delta_y)) {
    set_thread_error("move deltas must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto animation_status = validate_animation_options(animation);
  if (animation_status != MLN_STATUS_OK) {
    return animation_status;
  }

  map->map->moveBy(
    mbgl::ScreenCoordinate{delta_x, delta_y}, to_native_animation(animation)
  );
  return MLN_STATUS_OK;
}

auto map_scale_by(mln_map* map, double scale, const mln_screen_point* anchor)
  -> mln_status {
  return map_scale_by_animated(map, scale, anchor, nullptr);
}

auto map_scale_by_animated(
  mln_map* map, double scale, const mln_screen_point* anchor,
  const mln_animation_options* animation
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!std::isfinite(scale) || scale <= 0.0) {
    set_thread_error("scale must be positive and finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto native_anchor = std::optional<mbgl::ScreenCoordinate>{};
  if (anchor != nullptr) {
    const auto anchor_status = validate_screen_point(*anchor);
    if (anchor_status != MLN_STATUS_OK) {
      return anchor_status;
    }
    native_anchor = screen_point(*anchor);
  }
  const auto animation_status = validate_animation_options(animation);
  if (animation_status != MLN_STATUS_OK) {
    return animation_status;
  }

  map->map->scaleBy(scale, native_anchor, to_native_animation(animation));
  return MLN_STATUS_OK;
}

auto map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) -> mln_status {
  return map_rotate_by_animated(map, first, second, nullptr);
}

auto map_rotate_by_animated(
  mln_map* map, mln_screen_point first, mln_screen_point second,
  const mln_animation_options* animation
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto first_status = validate_screen_point(first);
  if (first_status != MLN_STATUS_OK) {
    return first_status;
  }
  const auto second_status = validate_screen_point(second);
  if (second_status != MLN_STATUS_OK) {
    return second_status;
  }
  const auto animation_status = validate_animation_options(animation);
  if (animation_status != MLN_STATUS_OK) {
    return animation_status;
  }

  map->map->rotateBy(
    screen_point(first), screen_point(second), to_native_animation(animation)
  );
  return MLN_STATUS_OK;
}

auto map_pitch_by(mln_map* map, double pitch) -> mln_status {
  return map_pitch_by_animated(map, pitch, nullptr);
}

auto map_pitch_by_animated(
  mln_map* map, double pitch, const mln_animation_options* animation
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!std::isfinite(pitch)) {
    set_thread_error("pitch must be finite");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto animation_status = validate_animation_options(animation);
  if (animation_status != MLN_STATUS_OK) {
    return animation_status;
  }

  map->map->pitchBy(pitch, to_native_animation(animation));
  return MLN_STATUS_OK;
}

auto map_cancel_transitions(mln_map* map) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  map->map->cancelTransitions();
  return MLN_STATUS_OK;
}

auto validate_camera_output(mln_camera_options* out_camera) -> mln_status {
  if (out_camera == nullptr || out_camera->size < sizeof(mln_camera_options)) {
    set_thread_error("out_camera must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto map_camera_for_lat_lng_bounds(
  mln_map* map, mln_lat_lng_bounds bounds,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto bounds_status = validate_lat_lng_bounds(bounds);
  if (bounds_status != MLN_STATUS_OK) {
    return bounds_status;
  }
  const auto fit_status = validate_camera_fit_options(fit_options);
  if (fit_status != MLN_STATUS_OK) {
    return fit_status;
  }
  const auto output_status = validate_camera_output(out_camera);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  *out_camera = from_native_camera(map->map->cameraForLatLngBounds(
    to_native_lat_lng_bounds(bounds), camera_fit_padding(fit_options),
    camera_fit_bearing(fit_options), camera_fit_pitch(fit_options)
  ));
  return MLN_STATUS_OK;
}

auto map_camera_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto coordinates_status =
    validate_lat_lng_array(coordinates, coordinate_count, false);
  if (coordinates_status != MLN_STATUS_OK) {
    return coordinates_status;
  }
  const auto fit_status = validate_camera_fit_options(fit_options);
  if (fit_status != MLN_STATUS_OK) {
    return fit_status;
  }
  const auto output_status = validate_camera_output(out_camera);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  *out_camera = from_native_camera(map->map->cameraForLatLngs(
    to_native_lat_lngs(coordinates, coordinate_count),
    camera_fit_padding(fit_options), camera_fit_bearing(fit_options),
    camera_fit_pitch(fit_options)
  ));
  return MLN_STATUS_OK;
}

auto map_camera_for_geometry(
  mln_map* map, const mln_geometry* geometry,
  const mln_camera_fit_options* fit_options, mln_camera_options* out_camera
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  auto native_geometry = to_native_geometry(geometry);
  if (!native_geometry) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (geometry_lat_lngs(*native_geometry).empty()) {
    set_thread_error("geometry must contain at least one coordinate");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto fit_status = validate_camera_fit_options(fit_options);
  if (fit_status != MLN_STATUS_OK) {
    return fit_status;
  }
  const auto output_status = validate_camera_output(out_camera);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }

  *out_camera = from_native_camera(map->map->cameraForGeometry(
    *native_geometry, camera_fit_padding(fit_options),
    camera_fit_bearing(fit_options), camera_fit_pitch(fit_options)
  ));
  return MLN_STATUS_OK;
}

auto map_lat_lng_bounds_for_camera(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto camera_status = validate_camera_options(camera);
  if (camera_status != MLN_STATUS_OK) {
    return camera_status;
  }
  if (out_bounds == nullptr) {
    set_thread_error("out_bounds must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_bounds = from_native_lat_lng_bounds(
    map->map->latLngBoundsForCamera(to_native_camera(*camera))
  );
  return MLN_STATUS_OK;
}

auto map_lat_lng_bounds_for_camera_unwrapped(
  mln_map* map, const mln_camera_options* camera, mln_lat_lng_bounds* out_bounds
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto camera_status = validate_camera_options(camera);
  if (camera_status != MLN_STATUS_OK) {
    return camera_status;
  }
  if (out_bounds == nullptr) {
    set_thread_error("out_bounds must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_bounds = from_native_lat_lng_bounds(
    map->map->latLngBoundsForCameraUnwrapped(to_native_camera(*camera))
  );
  return MLN_STATUS_OK;
}

auto map_get_bounds(mln_map* map, mln_bound_options* out_options)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (out_options == nullptr || out_options->size < sizeof(mln_bound_options)) {
    set_thread_error("out_options must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_options = from_native_bound_options(map->map->getBounds());
  return MLN_STATUS_OK;
}

auto map_set_bounds(mln_map* map, const mln_bound_options* options)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto options_status = validate_bound_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  // Native setBounds only applies optionals that are set, so this preserves
  // constraints omitted from options->fields.
  map->map->setBounds(to_native_bound_options(*options));
  return MLN_STATUS_OK;
}

auto map_get_free_camera_options(
  mln_map* map, mln_free_camera_options* out_options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_options == nullptr ||
    out_options->size < sizeof(mln_free_camera_options)
  ) {
    set_thread_error("out_options must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_options = from_native_free_camera(map->map->getFreeCameraOptions());
  return MLN_STATUS_OK;
}

auto map_set_free_camera_options(
  mln_map* map, const mln_free_camera_options* options
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto options_status = validate_free_camera_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  map->map->setFreeCameraOptions(to_native_free_camera(*options));
  return MLN_STATUS_OK;
}

}  // namespace mln::core
