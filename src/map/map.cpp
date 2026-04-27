#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/map/camera.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/map/map_observer.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/map/mode.hpp>
#include <mbgl/renderer/renderer_frontend.hpp>
#include <mbgl/renderer/update_parameters.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/size.hpp>

#include "map/map.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_abi.h"
#include "runtime/runtime.hpp"

namespace {
using MapRegistry = std::unordered_map<mln_map*, std::unique_ptr<mln_map>>;

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

auto copy_message(std::span<char, 512> target, const char* message) noexcept
  -> void {
  target.front() = '\0';
  if (message == nullptr) {
    return;
  }

  const auto length = std::min(std::strlen(message), target.size() - 1);
  std::memcpy(target.data(), message, length);
  target.subspan(length).front() = '\0';
}

class EventQueue final {
 public:
  auto push(
    mln_map_event_type type, int32_t code = 0, const char* message = nullptr
  ) -> void {
    auto event = mln_map_event{
      .size = sizeof(mln_map_event),
      .type = static_cast<uint32_t>(type),
      .code = code,
      .message = {}
    };
    copy_message(std::span<char, 512>{event.message}, message);

    const std::scoped_lock lock(mutex_);
    if (type == MLN_MAP_EVENT_MAP_LOADING_FAILED) {
      failed_ = true;
      failure_message_ =
        message == nullptr ? std::string{} : std::string{message};
    }
    events_.push_back(event);
  }

  auto clear_failure() -> void {
    const std::scoped_lock lock(mutex_);
    failed_ = false;
    failure_message_.clear();
  }

  [[nodiscard]] auto failed() const -> bool {
    const std::scoped_lock lock(mutex_);
    return failed_;
  }

  [[nodiscard]] auto failure_message() const -> std::string {
    const std::scoped_lock lock(mutex_);
    return failure_message_;
  }

  auto poll(mln_map_event* out_event) -> bool {
    const std::scoped_lock lock(mutex_);
    if (events_.empty()) {
      return false;
    }

    *out_event = events_.front();
    events_.pop_front();
    return true;
  }

 private:
  mutable std::mutex mutex_;
  std::deque<mln_map_event> events_;
  bool failed_ = false;
  std::string failure_message_;
};

class HeadlessObserver final : public mbgl::MapObserver {
 public:
  explicit HeadlessObserver(EventQueue& events) : events_(&events) {}

  void onCameraWillChange(CameraChangeMode mode) override {
    events_->push(MLN_MAP_EVENT_CAMERA_WILL_CHANGE, static_cast<int32_t>(mode));
  }

  void onCameraIsChanging() override {
    events_->push(MLN_MAP_EVENT_CAMERA_IS_CHANGING);
  }

  void onCameraDidChange(CameraChangeMode mode) override {
    events_->push(MLN_MAP_EVENT_CAMERA_DID_CHANGE, static_cast<int32_t>(mode));
  }

  void onWillStartLoadingMap() override {
    events_->push(MLN_MAP_EVENT_MAP_LOADING_STARTED);
  }

  void onDidFinishLoadingMap() override {
    events_->push(MLN_MAP_EVENT_MAP_LOADING_FINISHED);
  }

  void onDidFailLoadingMap(
    mbgl::MapLoadError error, const std::string& message
  ) override {
    events_->push(
      MLN_MAP_EVENT_MAP_LOADING_FAILED, static_cast<int32_t>(error),
      message.c_str()
    );
  }

  void onDidFinishLoadingStyle() override {
    events_->push(MLN_MAP_EVENT_STYLE_LOADED);
  }

  void onDidBecomeIdle() override { events_->push(MLN_MAP_EVENT_MAP_IDLE); }

  void onRenderError(std::exception_ptr error) override {
    try {
      if (error) {
        std::rethrow_exception(error);
      }
      events_->push(MLN_MAP_EVENT_RENDER_ERROR);
    } catch (const std::exception& exception) {
      events_->push(MLN_MAP_EVENT_RENDER_ERROR, 0, exception.what());
    } catch (...) {
      events_->push(MLN_MAP_EVENT_RENDER_ERROR, 0, "unknown render error");
    }
  }

 private:
  EventQueue* events_;
};

class HeadlessFrontend final : public mbgl::RendererFrontend {
 public:
  explicit HeadlessFrontend(EventQueue& events)
      : events_(&events),
        thread_pool_(
          mbgl::Scheduler::GetBackground(), mbgl::util::SimpleIdentity::Empty
        ) {}

  void reset() override { latest_update_.reset(); }

  void setObserver(mbgl::RendererObserver& unused_observer) override {
    static_cast<void>(unused_observer);
  }

  void update(std::shared_ptr<mbgl::UpdateParameters> update) override {
    latest_update_ = std::move(update);
    events_->push(MLN_MAP_EVENT_RENDER_INVALIDATED);
  }

  [[nodiscard]] auto getThreadPool() const
    -> const mbgl::TaggedScheduler& override {
    return thread_pool_;
  }

 private:
  EventQueue* events_;
  mbgl::TaggedScheduler thread_pool_;
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

  return MLN_STATUS_OK;
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

auto screen_point(mln_screen_point point) -> mbgl::ScreenCoordinate {
  return mbgl::ScreenCoordinate{point.x, point.y};
}
}  // namespace

struct mln_map {
  mln_runtime* runtime = nullptr;
  std::thread::id owner_thread;
  EventQueue events;
  std::unique_ptr<HeadlessObserver> observer;
  std::unique_ptr<HeadlessFrontend> frontend;
  std::unique_ptr<mbgl::Map> map;
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

}  // namespace

auto map_options_default() noexcept -> mln_map_options {
  return mln_map_options{
    .size = sizeof(mln_map_options),
    .width = default_map_width,
    .height = default_map_height,
    .scale_factor = default_scale_factor
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
  owned_map->runtime = runtime;
  owned_map->owner_thread = std::this_thread::get_id();
  owned_map->observer = std::make_unique<HeadlessObserver>(owned_map->events);
  owned_map->frontend = std::make_unique<HeadlessFrontend>(owned_map->events);

  auto map_options = mbgl::MapOptions{};
  map_options.withMapMode(mbgl::MapMode::Continuous)
    .withSize(mbgl::Size{effective.width, effective.height})
    .withPixelRatio(static_cast<float>(effective.scale_factor));
  owned_map->map = std::make_unique<mbgl::Map>(
    *owned_map->frontend, *owned_map->observer, map_options,
    resource_options_for_runtime(runtime)
  );

  auto* handle = owned_map.get();
  {
    const std::scoped_lock lock(map_registry_mutex());
    map_registry().emplace(handle, std::move(owned_map));
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

  auto* runtime = map->runtime;
  auto owned_map = std::unique_ptr<mln_map>{};
  {
    const std::scoped_lock lock(map_registry_mutex());
    const auto found = map_registry().find(map);
    owned_map = std::move(found->second);
    map_registry().erase(found);
  }
  owned_map.reset();
  release_runtime_map(runtime);
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
  map->events.clear_failure();
  map->map->getStyle().loadURL(url);
  if (map->events.failed()) {
    set_thread_error(map->events.failure_message().c_str());
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
    map->events.clear_failure();
    map->map->getStyle().loadJSON(json);
  } catch (const std::exception& exception) {
    set_thread_error(exception.what());
    map->events.push(MLN_MAP_EVENT_MAP_LOADING_FAILED, 0, exception.what());
    return MLN_STATUS_NATIVE_ERROR;
  }
  if (map->events.failed()) {
    set_thread_error(map->events.failure_message().c_str());
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

auto map_poll_event(mln_map* map, mln_map_event* out_event, bool* out_has_event)
  -> mln_status {
  const auto status = validate_map(map);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_event == nullptr || out_has_event == nullptr ||
    out_event->size < sizeof(mln_map_event)
  ) {
    set_thread_error(
      "out_event and out_has_event must not be null, and out_event must have a "
      "valid size"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_has_event = map->events.poll(out_event);
  return MLN_STATUS_OK;
}

}  // namespace mln::core
