#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <mbgl/actor/scheduler.hpp>
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
#include <mbgl/style/style.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/util/size.hpp>

#include "map/map.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"
#include "runtime/runtime.hpp"

namespace {
using MapRegistry = std::unordered_map<mln_map*, std::unique_ptr<mln_map>>;
using ProjectionRegistry =
  std::unordered_map<mln_map_projection*, std::unique_ptr<mln_map_projection>>;

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

  void onDidBecomeIdle() override { push(MLN_RUNTIME_EVENT_MAP_IDLE); }

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
    MLN_CAMERA_OPTION_BEARING | MLN_CAMERA_OPTION_PITCH;
  if ((camera->fields & ~known_fields) != 0U) {
    mln::core::set_thread_error(
      "mln_camera_options.fields contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
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

auto to_native_camera(const mln_camera_options& camera) -> mbgl::CameraOptions {
  auto result = mbgl::CameraOptions{};
  if ((camera.fields & MLN_CAMERA_OPTION_CENTER) != 0U) {
    result.withCenter(mbgl::LatLng{camera.latitude, camera.longitude});
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
  return result;
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
  mln_texture_session* texture_session = nullptr;
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
    .zoom = 0,
    .bearing = 0,
    .pitch = 0
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

  if (map->texture_session != nullptr) {
    set_thread_error("map still has an attached texture session");
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

auto map_attach_texture_session(mln_map* map, mln_texture_session* texture)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture == nullptr) {
    set_thread_error("texture session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (map->texture_session != nullptr) {
    set_thread_error("map already has an attached texture session");
    return MLN_STATUS_INVALID_STATE;
  }
  map->texture_session = texture;
  return MLN_STATUS_OK;
}

auto map_detach_texture_session(mln_map* map, mln_texture_session* texture)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture == nullptr) {
    set_thread_error("texture session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (map->texture_session != texture) {
    set_thread_error("texture session is not attached to this map");
    return MLN_STATUS_INVALID_STATE;
  }
  map->texture_session = nullptr;
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
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  map->map->moveBy(mbgl::ScreenCoordinate{delta_x, delta_y});
  return MLN_STATUS_OK;
}

auto map_scale_by(mln_map* map, double scale, const mln_screen_point* anchor)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  auto native_anchor = std::optional<mbgl::ScreenCoordinate>{};
  if (anchor != nullptr) {
    native_anchor = screen_point(*anchor);
  }
  map->map->scaleBy(scale, native_anchor);
  return MLN_STATUS_OK;
}

auto map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  map->map->rotateBy(screen_point(first), screen_point(second));
  return MLN_STATUS_OK;
}

auto map_pitch_by(mln_map* map, double pitch) -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  map->map->pitchBy(pitch);
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

}  // namespace mln::core
