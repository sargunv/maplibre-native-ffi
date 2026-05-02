#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/storage/database_file_source.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_manager.hpp>
#include <mbgl/storage/offline.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/util/client_options.hpp>
#include <mbgl/util/expected.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/run_loop.hpp>

#include "runtime/runtime.hpp"

#include "diagnostics/diagnostics.hpp"
#include "maplibre_native_c.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

struct OfflineRegionData {
  mln_offline_region_id id = 0;
  uint32_t definition_type = 0;
  std::string style_url;
  mln_lat_lng_bounds bounds{};
  double min_zoom = 0.0;
  double max_zoom = 0.0;
  float pixel_ratio = 0.0F;
  bool include_ideographs = false;
  std::vector<uint8_t> metadata;
};

struct mln_offline_region_snapshot {
  OfflineRegionData data;
};

struct mln_offline_region_list {
  std::vector<OfflineRegionData> regions;
};

namespace {
using RuntimeRegistry =
  std::unordered_map<mln_runtime*, std::unique_ptr<mln_runtime>>;

auto runtime_registry_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto runtime_registry() -> RuntimeRegistry& {
  static RuntimeRegistry value;
  return value;
}

using OfflineRegionSnapshotRegistry = std::unordered_map<
  const mln_offline_region_snapshot*,
  std::unique_ptr<mln_offline_region_snapshot>>;
using OfflineRegionListRegistry = std::unordered_map<
  const mln_offline_region_list*, std::unique_ptr<mln_offline_region_list>>;

auto offline_region_handle_mutex() -> std::mutex& {
  static std::mutex value;
  return value;
}

auto offline_region_snapshots() -> OfflineRegionSnapshotRegistry& {
  static OfflineRegionSnapshotRegistry value;
  return value;
}

auto offline_region_lists() -> OfflineRegionListRegistry& {
  static OfflineRegionListRegistry value;
  return value;
}

auto set_status_from_exception(
  std::exception_ptr exception, const char* fallback_message
) -> mln_status {
  if (exception) {
    try {
      std::rethrow_exception(exception);
    } catch (const std::exception& caught) {
      mln::core::set_thread_error(caught.what());
    } catch (...) {
      mln::core::set_thread_error(fallback_message);
    }
  } else {
    mln::core::set_thread_error(fallback_message);
  }
  return MLN_STATUS_NATIVE_ERROR;
}

template <typename Result, typename Start>
auto wait_for_database_result(Start start) -> Result {
  auto mutex = std::mutex{};
  auto condition = std::condition_variable{};
  auto result = std::optional<Result>{};

  start([&](Result value) -> void {
    {
      const std::scoped_lock lock(mutex);
      result.emplace(std::move(value));
    }
    condition.notify_one();
  });

  auto lock = std::unique_lock{mutex};
  condition.wait(lock, [&]() -> bool { return result.has_value(); });
  if (!result.has_value()) {
    std::terminate();
  }
  return std::move(result.value());
}

template <typename Payload>
auto payload_bytes(const Payload& payload) -> std::vector<std::byte> {
  auto result = std::vector<std::byte>(sizeof(Payload));
  std::memcpy(result.data(), &payload, sizeof(Payload));
  return result;
}

auto valid_coordinate(const mln_lat_lng& coordinate) -> bool {
  return std::isfinite(coordinate.latitude) && coordinate.latitude >= -90.0 &&
         coordinate.latitude <= 90.0 && std::isfinite(coordinate.longitude);
}

auto validate_tile_pyramid_definition(
  const mln_offline_tile_pyramid_region_definition& definition
) -> mln_status {
  if (definition.size < sizeof(mln_offline_tile_pyramid_region_definition)) {
    mln::core::set_thread_error(
      "mln_offline_tile_pyramid_region_definition.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (definition.style_url == nullptr) {
    mln::core::set_thread_error("offline region style_url must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    !valid_coordinate(definition.bounds.southwest) ||
    !valid_coordinate(definition.bounds.northeast) ||
    definition.bounds.southwest.latitude >
      definition.bounds.northeast.latitude ||
    definition.bounds.southwest.longitude >
      definition.bounds.northeast.longitude
  ) {
    mln::core::set_thread_error("offline region bounds are invalid");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    !std::isfinite(definition.min_zoom) || definition.min_zoom < 0.0 ||
    std::isnan(definition.max_zoom) || definition.max_zoom < definition.min_zoom
  ) {
    mln::core::set_thread_error("offline region zoom range is invalid");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!std::isfinite(definition.pixel_ratio) || definition.pixel_ratio < 0.0F) {
    mln::core::set_thread_error("offline region pixel_ratio is invalid");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_offline_region_definition(
  const mln_offline_region_definition* definition
) -> mln_status {
  if (definition == nullptr) {
    mln::core::set_thread_error("offline region definition must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (definition->size < sizeof(mln_offline_region_definition)) {
    mln::core::set_thread_error(
      "mln_offline_region_definition.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  switch (definition->type) {
    case MLN_OFFLINE_REGION_DEFINITION_TILE_PYRAMID:
      return validate_tile_pyramid_definition(definition->data.tile_pyramid);
    case MLN_OFFLINE_REGION_DEFINITION_GEOMETRY:
      // TODO: Support geometry regions after the shared geometry ABI lands:
      // https://github.com/sargunv/maplibre-native-ffi/issues/19
      mln::core::set_thread_error(
        "offline geometry region definitions are not supported"
      );
      return MLN_STATUS_UNSUPPORTED;
    default:
      mln::core::set_thread_error("offline region definition type is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }
}

auto to_native_offline_region_definition(
  const mln_offline_region_definition& definition
) -> mbgl::OfflineRegionDefinition {
  const auto& tile = definition.data.tile_pyramid;
  auto bounds = mbgl::LatLngBounds::hull(
    {tile.bounds.southwest.latitude, tile.bounds.southwest.longitude},
    {tile.bounds.northeast.latitude, tile.bounds.northeast.longitude}
  );
  return mbgl::OfflineTilePyramidRegionDefinition{
    std::string{tile.style_url},
    bounds,
    tile.min_zoom,
    tile.max_zoom,
    tile.pixel_ratio,
    tile.include_ideographs
  };
}

auto to_c_download_state(mbgl::OfflineRegionDownloadState state) -> uint32_t {
  switch (state) {
    case mbgl::OfflineRegionDownloadState::Inactive:
      return MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE;
    case mbgl::OfflineRegionDownloadState::Active:
      return MLN_OFFLINE_REGION_DOWNLOAD_ACTIVE;
  }
  return MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE;
}

auto to_c_status(const mbgl::OfflineRegionStatus& status)
  -> mln_offline_region_status {
  return mln_offline_region_status{
    .size = sizeof(mln_offline_region_status),
    .download_state = to_c_download_state(status.downloadState),
    .completed_resource_count = status.completedResourceCount,
    .completed_resource_size = status.completedResourceSize,
    .completed_tile_count = status.completedTileCount,
    .required_tile_count = status.requiredTileCount,
    .completed_tile_size = status.completedTileSize,
    .required_resource_count = status.requiredResourceCount,
    .required_resource_count_is_precise = status.requiredResourceCountIsPrecise,
    .complete = status.complete()
  };
}

auto to_native_download_state(uint32_t state)
  -> std::optional<mbgl::OfflineRegionDownloadState> {
  switch (state) {
    case MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE:
      return mbgl::OfflineRegionDownloadState::Inactive;
    case MLN_OFFLINE_REGION_DOWNLOAD_ACTIVE:
      return mbgl::OfflineRegionDownloadState::Active;
    default:
      return std::nullopt;
  }
}

auto to_c_resource_error_reason(mbgl::Response::Error::Reason reason)
  -> uint32_t {
  switch (reason) {
    case mbgl::Response::Error::Reason::Success:
      return MLN_RESOURCE_ERROR_REASON_NONE;
    case mbgl::Response::Error::Reason::NotFound:
      return MLN_RESOURCE_ERROR_REASON_NOT_FOUND;
    case mbgl::Response::Error::Reason::Server:
      return MLN_RESOURCE_ERROR_REASON_SERVER;
    case mbgl::Response::Error::Reason::Connection:
      return MLN_RESOURCE_ERROR_REASON_CONNECTION;
    case mbgl::Response::Error::Reason::RateLimit:
      return MLN_RESOURCE_ERROR_REASON_RATE_LIMIT;
    case mbgl::Response::Error::Reason::Other:
      return MLN_RESOURCE_ERROR_REASON_OTHER;
  }
  return MLN_RESOURCE_ERROR_REASON_OTHER;
}

auto to_c_region_data(const mbgl::OfflineRegion& region)
  -> std::optional<OfflineRegionData> {
  auto data = OfflineRegionData{
    .id = region.getID(),
    .definition_type = MLN_OFFLINE_REGION_DEFINITION_TILE_PYRAMID,
    .style_url = {},
    .bounds = {},
    .min_zoom = 0.0,
    .max_zoom = 0.0,
    .pixel_ratio = 0.0F,
    .include_ideographs = false,
    .metadata = region.getMetadata()
  };

  if (
    const auto* tile = std::get_if<mbgl::OfflineTilePyramidRegionDefinition>(
      &region.getDefinition()
    )
  ) {
    data.style_url = tile->styleURL;
    data.bounds = mln_lat_lng_bounds{
      .southwest =
        mln_lat_lng{
          .latitude = tile->bounds.south(), .longitude = tile->bounds.west()
        },
      .northeast = mln_lat_lng{
        .latitude = tile->bounds.north(), .longitude = tile->bounds.east()
      }
    };
    data.min_zoom = tile->minZoom;
    data.max_zoom = tile->maxZoom;
    data.pixel_ratio = tile->pixelRatio;
    data.include_ideographs = tile->includeIdeographs;
    return data;
  }

  mln::core::set_thread_error(
    "offline geometry region definitions are not supported"
  );
  return std::nullopt;
}

auto fill_region_info(
  const OfflineRegionData& data, mln_offline_region_info* out_info
) -> mln_status {
  if (out_info == nullptr || out_info->size < sizeof(mln_offline_region_info)) {
    mln::core::set_thread_error(
      "out_info must not be null and must have a valid size"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  *out_info = mln_offline_region_info{
    .size = sizeof(mln_offline_region_info),
    .id = data.id,
    .definition =
      mln_offline_region_definition{
        .size = sizeof(mln_offline_region_definition),
        .type = data.definition_type,
        .data =
          {.tile_pyramid =
             mln_offline_tile_pyramid_region_definition{
               .size = sizeof(mln_offline_tile_pyramid_region_definition),
               .style_url = data.style_url.c_str(),
               .bounds = data.bounds,
               .min_zoom = data.min_zoom,
               .max_zoom = data.max_zoom,
               .pixel_ratio = data.pixel_ratio,
               .include_ideographs = data.include_ideographs
             }}
      },
    .metadata = data.metadata.empty() ? nullptr : data.metadata.data(),
    .metadata_size = data.metadata.size()
  };
  return MLN_STATUS_OK;
}

auto push_offline_region_event(
  const std::shared_ptr<mln::core::OfflineRegionEventState>& state,
  mln_offline_region_id region_id, uint32_t type, uint32_t payload_type,
  std::vector<std::byte> payload, std::string message = {}
) -> void {
  const std::scoped_lock state_lock(state->mutex);
  auto* runtime = state->runtime;
  if (!state->alive || runtime == nullptr) {
    return;
  }

  auto event = mln::core::QueuedRuntimeEvent{
    .type = type,
    .source_type = MLN_RUNTIME_EVENT_SOURCE_RUNTIME,
    .source = runtime,
    .map = nullptr,
    .code = 0,
    .payload_type = payload_type,
    .payload = std::move(payload),
    .message = std::move(message),
    .has_offline_region = true,
    .offline_region_id = region_id
  };

  const std::scoped_lock event_lock(runtime->event_mutex);
  if (!runtime->observed_offline_regions.contains(region_id)) {
    return;
  }
  runtime->events.push_back(std::move(event));
}

class OfflineRegionRuntimeObserver final : public mbgl::OfflineRegionObserver {
 public:
  OfflineRegionRuntimeObserver(
    std::shared_ptr<mln::core::OfflineRegionEventState> state,
    mln_offline_region_id region_id
  )
      : state_(std::move(state)), region_id_(region_id) {}

  void statusChanged(mbgl::OfflineRegionStatus status) override {
    auto payload = mln_runtime_event_offline_region_status{
      .size = sizeof(mln_runtime_event_offline_region_status),
      .region_id = region_id_,
      .status = to_c_status(status)
    };
    push_offline_region_event(
      state_, region_id_, MLN_RUNTIME_EVENT_OFFLINE_REGION_STATUS_CHANGED,
      MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_STATUS, payload_bytes(payload)
    );
  }

  void responseError(mbgl::Response::Error error) override {
    auto payload = mln_runtime_event_offline_region_response_error{
      .size = sizeof(mln_runtime_event_offline_region_response_error),
      .region_id = region_id_,
      .reason = to_c_resource_error_reason(error.reason)
    };
    push_offline_region_event(
      state_, region_id_, MLN_RUNTIME_EVENT_OFFLINE_REGION_RESPONSE_ERROR,
      MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_RESPONSE_ERROR,
      payload_bytes(payload), error.message
    );
  }

  void mapboxTileCountLimitExceeded(uint64_t limit) override {
    auto payload = mln_runtime_event_offline_region_tile_count_limit{
      .size = sizeof(mln_runtime_event_offline_region_tile_count_limit),
      .region_id = region_id_,
      .limit = limit
    };
    push_offline_region_event(
      state_, region_id_,
      MLN_RUNTIME_EVENT_OFFLINE_REGION_TILE_COUNT_LIMIT_EXCEEDED,
      MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_TILE_COUNT_LIMIT,
      payload_bytes(payload)
    );
  }

 private:
  std::shared_ptr<mln::core::OfflineRegionEventState> state_;
  mln_offline_region_id region_id_;
};

auto set_offline_region_observed_flag(
  mln_runtime* runtime, mln_offline_region_id region_id, bool observed
) -> void {
  const std::scoped_lock lock(runtime->event_mutex);
  if (observed) {
    runtime->observed_offline_regions.insert(region_id);
  } else {
    runtime->observed_offline_regions.erase(region_id);
    std::erase_if(runtime->events, [region_id](const auto& event) -> bool {
      return event.has_offline_region && event.offline_region_id == region_id;
    });
  }
}

auto register_offline_region_snapshot(OfflineRegionData data)
  -> mln_offline_region_snapshot* {
  auto owned = std::make_unique<mln_offline_region_snapshot>();
  owned->data = std::move(data);
  auto* handle = owned.get();
  const std::scoped_lock lock(offline_region_handle_mutex());
  offline_region_snapshots().emplace(handle, std::move(owned));
  return handle;
}

auto register_offline_region_list(std::vector<OfflineRegionData> regions)
  -> mln_offline_region_list* {
  auto owned = std::make_unique<mln_offline_region_list>();
  owned->regions = std::move(regions);
  auto* handle = owned.get();
  const std::scoped_lock lock(offline_region_handle_mutex());
  offline_region_lists().emplace(handle, std::move(owned));
  return handle;
}

auto register_offline_region_list(const mbgl::OfflineRegions& native_regions)
  -> std::optional<mln_offline_region_list*> {
  auto regions = std::vector<OfflineRegionData>{};
  regions.reserve(native_regions.size());
  for (const auto& region : native_regions) {
    auto data = to_c_region_data(region);
    if (!data) {
      return std::nullopt;
    }
    regions.push_back(std::move(*data));
  }
  return register_offline_region_list(std::move(regions));
}

auto find_offline_region_snapshot_locked(
  const mln_offline_region_snapshot* snapshot
) -> const mln_offline_region_snapshot* {
  if (snapshot == nullptr) {
    mln::core::set_thread_error("offline region snapshot must not be null");
    return nullptr;
  }
  const auto found = offline_region_snapshots().find(snapshot);
  if (found == offline_region_snapshots().end()) {
    mln::core::set_thread_error("offline region snapshot is not a live handle");
    return nullptr;
  }
  return found->second.get();
}

auto find_offline_region_list_locked(const mln_offline_region_list* list)
  -> const mln_offline_region_list* {
  if (list == nullptr) {
    mln::core::set_thread_error("offline region list must not be null");
    return nullptr;
  }
  const auto found = offline_region_lists().find(list);
  if (found == offline_region_lists().end()) {
    mln::core::set_thread_error("offline region list is not a live handle");
    return nullptr;
  }
  return found->second.get();
}

auto validate_runtime_options(const mln_runtime_options* options)
  -> mln_status {
  if (options == nullptr) {
    return MLN_STATUS_OK;
  }

  if (options->size < sizeof(mln_runtime_options)) {
    mln::core::set_thread_error("mln_runtime_options.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  constexpr auto known_flags =
    static_cast<uint32_t>(MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE);
  if ((options->flags & ~known_flags) != 0) {
    mln::core::set_thread_error(
      "mln_runtime_options.flags contains unknown bits"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

}  // namespace

namespace mln::core {

namespace {

auto database_source_for_runtime(mln_runtime* runtime)
  -> std::shared_ptr<mbgl::DatabaseFileSource> {
  if (runtime == nullptr) {
    return nullptr;
  }
  if (runtime->database_source != nullptr) {
    return runtime->database_source;
  }

  auto source = mbgl::FileSourceManager::get()->getFileSource(
    mbgl::FileSourceType::Database, resource_options_for_runtime(runtime),
    mbgl::ClientOptions()
  );
  // The Database FileSourceManager factory is registered by the C API layer and
  // always returns DatabaseFileSource for FileSourceType::Database. MapLibre is
  // built without RTTI, so keep this path non-RTTI as well.
  auto database = std::static_pointer_cast<mbgl::DatabaseFileSource>(source);
  if (database != nullptr && runtime->has_maximum_cache_size) {
    database->setMaximumAmbientCacheSize(
      runtime->maximum_cache_size, [](std::exception_ptr) -> void {}
    );
  }
  runtime->database_source = database;
  return runtime->database_source;
}

auto wait_for_database_operation(
  const std::function<void(std::function<void(std::exception_ptr)>)>& start
) -> mln_status {
  auto mutex = std::mutex{};
  auto condition = std::condition_variable{};
  auto complete = false;
  auto failure = std::exception_ptr{};

  start([&](std::exception_ptr exception) -> void {
    {
      const std::scoped_lock lock(mutex);
      failure = exception;
      complete = true;
    }
    condition.notify_one();
  });

  auto lock = std::unique_lock{mutex};
  condition.wait(lock, [&]() -> bool { return complete; });
  if (failure) {
    try {
      std::rethrow_exception(failure);
    } catch (const std::exception& exception) {
      set_thread_error(exception.what());
    } catch (...) {
      set_thread_error("ambient cache operation failed");
    }
    return MLN_STATUS_NATIVE_ERROR;
  }
  return MLN_STATUS_OK;
}

auto patch_polled_payload_strings(mln_runtime* runtime, uint32_t payload_type)
  -> void {
  const auto* const text = runtime->last_polled_event_message.empty()
                             ? nullptr
                             : runtime->last_polled_event_message.c_str();
  const auto text_size = runtime->last_polled_event_message.size();

  switch (payload_type) {
    case MLN_RUNTIME_EVENT_PAYLOAD_STYLE_IMAGE_MISSING:
      if (
        runtime->last_polled_event_payload.size() >=
        sizeof(mln_runtime_event_style_image_missing)
      ) {
        auto payload = mln_runtime_event_style_image_missing{};
        std::memcpy(
          &payload, runtime->last_polled_event_payload.data(), sizeof(payload)
        );
        payload.image_id = text;
        payload.image_id_size = text_size;
        std::memcpy(
          runtime->last_polled_event_payload.data(), &payload, sizeof(payload)
        );
      }
      break;
    case MLN_RUNTIME_EVENT_PAYLOAD_TILE_ACTION:
      if (
        runtime->last_polled_event_payload.size() >=
        sizeof(mln_runtime_event_tile_action)
      ) {
        auto payload = mln_runtime_event_tile_action{};
        std::memcpy(
          &payload, runtime->last_polled_event_payload.data(), sizeof(payload)
        );
        payload.source_id = text;
        payload.source_id_size = text_size;
        std::memcpy(
          runtime->last_polled_event_payload.data(), &payload, sizeof(payload)
        );
      }
      break;
    default:
      break;
  }
}

}  // namespace

auto validate_runtime(mln_runtime* runtime) -> mln_status {
  if (runtime == nullptr) {
    set_thread_error("runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (!runtime_registry().contains(runtime)) {
    set_thread_error("runtime is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (runtime->owner_thread != std::this_thread::get_id()) {
    set_thread_error("runtime call must be made on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  return MLN_STATUS_OK;
}

auto create_runtime(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status {
  const auto options_status = validate_runtime_options(options);
  if (options_status != MLN_STATUS_OK) {
    return options_status;
  }

  if (out_runtime == nullptr) {
    set_thread_error("out_runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (*out_runtime != nullptr) {
    set_thread_error("out_runtime must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto owner_thread = std::this_thread::get_id();
  {
    const std::scoped_lock lock(runtime_registry_mutex());
    if (
      std::any_of(
        runtime_registry().begin(), runtime_registry().end(),
        [&](const auto& entry) -> bool {
          return entry.second->owner_thread == owner_thread;
        }
      )
    ) {
      set_thread_error("owner thread already has a live runtime");
      return MLN_STATUS_INVALID_STATE;
    }
  }
  if (mbgl::Scheduler::GetCurrent(false) != nullptr) {
    set_thread_error("owner thread already has an active MapLibre scheduler");
    return MLN_STATUS_INVALID_STATE;
  }

  auto owned_runtime = std::make_unique<mln_runtime>();
  owned_runtime->owner_thread = owner_thread;
  owned_runtime->run_loop =
    std::make_unique<mbgl::util::RunLoop>(mbgl::util::RunLoop::Type::New);
  owned_runtime->asset_path =
    options == nullptr || options->asset_path == nullptr
      ? std::string{}
      : std::string{options->asset_path};
  owned_runtime->cache_path =
    options == nullptr || options->cache_path == nullptr
      ? std::string{}
      : std::string{options->cache_path};
  owned_runtime->has_maximum_cache_size =
    options != nullptr &&
    (options->flags & MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE) != 0;
  owned_runtime->maximum_cache_size =
    options == nullptr ? 0 : options->maximum_cache_size;
  owned_runtime->offline_event_state =
    std::make_shared<OfflineRegionEventState>();
  owned_runtime->offline_event_state->runtime = owned_runtime.get();
  owned_runtime->offline_event_state->alive = true;
  auto* runtime = owned_runtime.get();
  {
    const std::scoped_lock lock(runtime_registry_mutex());
    runtime_registry().emplace(runtime, std::move(owned_runtime));
  }

  *out_runtime = runtime;
  return MLN_STATUS_OK;
}

auto set_resource_transform(
  mln_runtime* runtime, const mln_resource_transform* transform
) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (transform == nullptr) {
    set_thread_error("resource transform must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (transform->size < sizeof(mln_resource_transform)) {
    set_thread_error("mln_resource_transform.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (transform->callback == nullptr) {
    set_thread_error("resource transform callback must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime->live_maps != 0) {
    set_thread_error(
      "resource transform must be registered before map creation"
    );
    return MLN_STATUS_INVALID_STATE;
  }
  runtime->resource_transform_callback = transform->callback;
  runtime->resource_transform_user_data = transform->user_data;
  return MLN_STATUS_OK;
}

auto run_ambient_cache_operation(mln_runtime* runtime, uint32_t operation)
  -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  switch (operation) {
    case MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE:
    case MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE:
    case MLN_AMBIENT_CACHE_OPERATION_INVALIDATE:
    case MLN_AMBIENT_CACHE_OPERATION_CLEAR:
      break;
    default:
      set_thread_error("ambient cache operation is invalid");
      return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  switch (operation) {
    case MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE:
      return wait_for_database_operation([&](auto callback) -> void {
        database->resetDatabase(std::move(callback));
      });
    case MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE:
      return wait_for_database_operation([&](auto callback) -> void {
        database->packDatabase(std::move(callback));
      });
    case MLN_AMBIENT_CACHE_OPERATION_INVALIDATE:
      return wait_for_database_operation([&](auto callback) -> void {
        database->invalidateAmbientCache(std::move(callback));
      });
    case MLN_AMBIENT_CACHE_OPERATION_CLEAR:
      return wait_for_database_operation([&](auto callback) -> void {
        database->clearAmbientCache(std::move(callback));
      });
    default:
      return MLN_STATUS_INVALID_ARGUMENT;
  }
}

auto offline_region_create(
  mln_runtime* runtime, const mln_offline_region_definition* definition,
  const uint8_t* metadata, size_t metadata_size,
  mln_offline_region_snapshot** out_region
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  const auto definition_status = validate_offline_region_definition(definition);
  if (definition_status != MLN_STATUS_OK) {
    return definition_status;
  }
  if (metadata == nullptr && metadata_size != 0) {
    set_thread_error("offline region metadata must not be null when non-empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_region == nullptr || *out_region != nullptr) {
    set_thread_error("out_region must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  const auto native_definition =
    to_native_offline_region_definition(*definition);
  auto native_metadata = mbgl::OfflineRegionMetadata{};
  if (metadata_size != 0) {
    native_metadata.resize(metadata_size);
    std::memcpy(native_metadata.data(), metadata, metadata_size);
  }
  auto result = wait_for_database_result<
    mbgl::expected<mbgl::OfflineRegion, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->createOfflineRegion(
        native_definition, native_metadata, std::move(callback)
      );
    }
  );
  if (!result) {
    return set_status_from_exception(
      result.error(), "offline region creation failed"
    );
  }

  auto data = to_c_region_data(result.value());
  if (!data) {
    return MLN_STATUS_UNSUPPORTED;
  }
  *out_region = register_offline_region_snapshot(std::move(*data));
  return MLN_STATUS_OK;
}

auto offline_region_get(
  mln_runtime* runtime, mln_offline_region_id region_id,
  mln_offline_region_snapshot** out_region, bool* out_found
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  if (out_region == nullptr || *out_region != nullptr || out_found == nullptr) {
    set_thread_error(
      "out_region must point to null and out_found must not be null"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!result) {
    return set_status_from_exception(
      result.error(), "offline region get failed"
    );
  }
  if (!result.value()) {
    *out_found = false;
    return MLN_STATUS_OK;
  }

  auto data = to_c_region_data(*result.value());
  if (!data) {
    return MLN_STATUS_UNSUPPORTED;
  }
  *out_region = register_offline_region_snapshot(std::move(*data));
  *out_found = true;
  return MLN_STATUS_OK;
}

auto offline_regions_list(
  mln_runtime* runtime, mln_offline_region_list** out_regions
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  if (out_regions == nullptr || *out_regions != nullptr) {
    set_thread_error("out_regions must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto result = wait_for_database_result<
    mbgl::expected<mbgl::OfflineRegions, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->listOfflineRegions(std::move(callback));
    }
  );
  if (!result) {
    return set_status_from_exception(
      result.error(), "offline region list failed"
    );
  }

  auto list = register_offline_region_list(result.value());
  if (!list) {
    return MLN_STATUS_UNSUPPORTED;
  }
  *out_regions = *list;
  return MLN_STATUS_OK;
}

auto offline_regions_merge_database(
  mln_runtime* runtime, const char* side_database_path,
  mln_offline_region_list** out_regions
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  if (side_database_path == nullptr) {
    set_thread_error("side_database_path must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_regions == nullptr || *out_regions != nullptr) {
    set_thread_error("out_regions must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto result = wait_for_database_result<
    mbgl::expected<mbgl::OfflineRegions, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->mergeOfflineRegions(side_database_path, std::move(callback));
    }
  );
  if (!result) {
    return set_status_from_exception(
      result.error(), "offline region database merge failed"
    );
  }

  auto list = register_offline_region_list(result.value());
  if (!list) {
    return MLN_STATUS_UNSUPPORTED;
  }
  *out_regions = *list;
  return MLN_STATUS_OK;
}

auto offline_region_update_metadata(
  mln_runtime* runtime, mln_offline_region_id region_id,
  const uint8_t* metadata, size_t metadata_size,
  mln_offline_region_snapshot** out_region
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  if (metadata == nullptr && metadata_size != 0) {
    set_thread_error("offline region metadata must not be null when non-empty");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_region == nullptr || *out_region != nullptr) {
    set_thread_error("out_region must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto native_metadata = mbgl::OfflineRegionMetadata{};
  if (metadata_size != 0) {
    native_metadata.resize(metadata_size);
    std::memcpy(native_metadata.data(), metadata, metadata_size);
  }
  auto update_result = wait_for_database_result<
    mbgl::expected<mbgl::OfflineRegionMetadata, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->updateOfflineMetadata(
        region_id, native_metadata, std::move(callback)
      );
    }
  );
  if (!update_result) {
    return set_status_from_exception(
      update_result.error(), "offline region metadata update failed"
    );
  }

  auto get_result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!get_result) {
    return set_status_from_exception(
      get_result.error(), "offline region get failed"
    );
  }
  if (!get_result.value()) {
    set_thread_error("offline region not found");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto data = to_c_region_data(*get_result.value());
  if (!data) {
    return MLN_STATUS_UNSUPPORTED;
  }
  *out_region = register_offline_region_snapshot(std::move(*data));
  return MLN_STATUS_OK;
}

auto offline_region_get_status(
  mln_runtime* runtime, mln_offline_region_id region_id,
  mln_offline_region_status* out_status
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  if (
    out_status == nullptr ||
    out_status->size < sizeof(mln_offline_region_status)
  ) {
    set_thread_error("out_status must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto get_result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!get_result) {
    return set_status_from_exception(
      get_result.error(), "offline region get failed"
    );
  }
  if (!get_result.value()) {
    set_thread_error("offline region not found");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto status_result = wait_for_database_result<
    mbgl::expected<mbgl::OfflineRegionStatus, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegionStatus(
        *get_result.value(), std::move(callback)
      );
    }
  );
  if (!status_result) {
    return set_status_from_exception(
      status_result.error(), "offline region status query failed"
    );
  }
  *out_status = to_c_status(status_result.value());
  return MLN_STATUS_OK;
}

auto offline_region_set_observed(
  mln_runtime* runtime, mln_offline_region_id region_id, bool observed
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto get_result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!get_result) {
    return set_status_from_exception(
      get_result.error(), "offline region get failed"
    );
  }
  if (!get_result.value()) {
    set_thread_error("offline region not found");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  set_offline_region_observed_flag(runtime, region_id, observed);
  auto observer = observed ? std::make_unique<OfflineRegionRuntimeObserver>(
                               runtime->offline_event_state, region_id
                             )
                           : nullptr;
  database->setOfflineRegionObserver(*get_result.value(), std::move(observer));
  return MLN_STATUS_OK;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
auto offline_region_set_download_state(
  mln_runtime* runtime, mln_offline_region_id region_id, uint32_t state
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }
  const auto native_state = to_native_download_state(state);
  if (!native_state) {
    set_thread_error("offline region download state is invalid");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto get_result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!get_result) {
    return set_status_from_exception(
      get_result.error(), "offline region get failed"
    );
  }
  if (!get_result.value()) {
    set_thread_error("offline region not found");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  database->setOfflineRegionDownloadState(*get_result.value(), *native_state);
  return MLN_STATUS_OK;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

auto offline_region_invalidate(
  mln_runtime* runtime, mln_offline_region_id region_id
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto get_result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!get_result) {
    return set_status_from_exception(
      get_result.error(), "offline region get failed"
    );
  }
  if (!get_result.value()) {
    set_thread_error("offline region not found");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return wait_for_database_operation([&](auto callback) -> void {
    database->invalidateOfflineRegion(*get_result.value(), std::move(callback));
  });
}

auto offline_region_delete(
  mln_runtime* runtime, mln_offline_region_id region_id
) -> mln_status {
  const auto runtime_status = validate_runtime(runtime);
  if (runtime_status != MLN_STATUS_OK) {
    return runtime_status;
  }

  auto database = database_source_for_runtime(runtime);
  if (database == nullptr) {
    set_thread_error("database file source is unavailable");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto get_result = wait_for_database_result<
    mbgl::expected<std::optional<mbgl::OfflineRegion>, std::exception_ptr>>(
    [&](auto callback) -> void {
      database->getOfflineRegion(region_id, std::move(callback));
    }
  );
  if (!get_result) {
    return set_status_from_exception(
      get_result.error(), "offline region get failed"
    );
  }
  if (!get_result.value()) {
    set_thread_error("offline region not found");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  set_offline_region_observed_flag(runtime, region_id, false);
  database->setOfflineRegionObserver(*get_result.value(), nullptr);
  database->setOfflineRegionDownloadState(
    *get_result.value(), mbgl::OfflineRegionDownloadState::Inactive
  );

  return wait_for_database_operation([&](auto callback) -> void {
    database->deleteOfflineRegion(*get_result.value(), std::move(callback));
  });
}

auto offline_region_snapshot_get(
  const mln_offline_region_snapshot* snapshot, mln_offline_region_info* out_info
) -> mln_status {
  const std::scoped_lock lock(offline_region_handle_mutex());
  const auto* live_snapshot = find_offline_region_snapshot_locked(snapshot);
  if (live_snapshot == nullptr) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return fill_region_info(live_snapshot->data, out_info);
}

auto offline_region_snapshot_destroy(
  mln_offline_region_snapshot* snapshot
) noexcept -> void {
  if (snapshot == nullptr) {
    return;
  }
  const std::scoped_lock lock(offline_region_handle_mutex());
  offline_region_snapshots().erase(snapshot);
}

auto offline_region_list_count(
  const mln_offline_region_list* list, size_t* out_count
) -> mln_status {
  const std::scoped_lock lock(offline_region_handle_mutex());
  const auto* live_list = find_offline_region_list_locked(list);
  if (live_list == nullptr) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (out_count == nullptr) {
    set_thread_error("out_count must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  *out_count = live_list->regions.size();
  return MLN_STATUS_OK;
}

auto offline_region_list_get(
  const mln_offline_region_list* list, size_t index,
  mln_offline_region_info* out_info
) -> mln_status {
  const std::scoped_lock lock(offline_region_handle_mutex());
  const auto* live_list = find_offline_region_list_locked(list);
  if (live_list == nullptr) {
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (index >= live_list->regions.size()) {
    set_thread_error("offline region list index is out of range");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return fill_region_info(live_list->regions.at(index), out_info);
}

auto offline_region_list_destroy(mln_offline_region_list* list) noexcept
  -> void {
  if (list == nullptr) {
    return;
  }
  const std::scoped_lock lock(offline_region_handle_mutex());
  offline_region_lists().erase(list);
}

auto set_resource_provider(
  mln_runtime* runtime, const mln_resource_provider* provider
) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (provider == nullptr) {
    set_thread_error("provider must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (provider->size < sizeof(mln_resource_provider)) {
    set_thread_error("mln_resource_provider.size is too small");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (provider->callback == nullptr) {
    set_thread_error("provider callback must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime->live_maps != 0) {
    set_thread_error("resource provider must be set before map creation");
    return MLN_STATUS_INVALID_STATE;
  }
  runtime->has_resource_provider = true;
  runtime->resource_provider = ResourceProvider{
    .callback = provider->callback,
    .user_data = provider->user_data,
  };
  return MLN_STATUS_OK;
}

auto destroy_runtime(mln_runtime* runtime) -> mln_status {
  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime == nullptr) {
    set_thread_error("runtime must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const auto found = runtime_registry().find(runtime);
  if (found == runtime_registry().end()) {
    set_thread_error("runtime is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  if (found->second->owner_thread != std::this_thread::get_id()) {
    set_thread_error("runtime must be destroyed on its owner thread");
    return MLN_STATUS_WRONG_THREAD;
  }

  if (found->second->live_maps != 0) {
    set_thread_error("runtime still owns live maps");
    return MLN_STATUS_INVALID_STATE;
  }

  {
    const std::scoped_lock state_lock(
      found->second->offline_event_state->mutex
    );
    found->second->offline_event_state->alive = false;
    found->second->offline_event_state->runtime = nullptr;
    const std::scoped_lock event_lock(found->second->event_mutex);
    found->second->observed_offline_regions.clear();
    std::erase_if(found->second->events, [](const auto& event) -> bool {
      return event.has_offline_region;
    });
  }

  runtime_registry().erase(runtime);
  return MLN_STATUS_OK;
}

auto run_runtime_once(mln_runtime* runtime) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  runtime->run_loop->runOnce();
  return MLN_STATUS_OK;
}

auto poll_runtime_event(
  mln_runtime* runtime, mln_runtime_event* out_event, bool* out_has_event
) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_event == nullptr || out_has_event == nullptr ||
    out_event->size < sizeof(mln_runtime_event)
  ) {
    set_thread_error(
      "out_event and out_has_event must not be null, and out_event must have a "
      "valid size"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  const std::scoped_lock lock(runtime->event_mutex);
  runtime->last_polled_event_payload.clear();
  runtime->last_polled_event_message.clear();
  *out_event = mln_runtime_event{
    .size = sizeof(mln_runtime_event),
    .type = 0,
    .source_type = MLN_RUNTIME_EVENT_SOURCE_RUNTIME,
    .source = runtime,
    .code = 0,
    .payload_type = MLN_RUNTIME_EVENT_PAYLOAD_NONE,
    .payload = nullptr,
    .payload_size = 0,
    .message = nullptr,
    .message_size = 0
  };

  if (runtime->events.empty()) {
    *out_has_event = false;
    return MLN_STATUS_OK;
  }

  auto event = std::move(runtime->events.front());
  runtime->events.pop_front();
  runtime->last_polled_event_payload = std::move(event.payload);
  runtime->last_polled_event_message = std::move(event.message);
  patch_polled_payload_strings(runtime, event.payload_type);

  out_event->type = event.type;
  out_event->source_type = event.source_type;
  out_event->source = event.source;
  out_event->code = event.code;
  out_event->payload_type = event.payload_type;
  out_event->payload = runtime->last_polled_event_payload.empty()
                         ? nullptr
                         : runtime->last_polled_event_payload.data();
  out_event->payload_size = runtime->last_polled_event_payload.size();
  out_event->message = runtime->last_polled_event_message.empty()
                         ? nullptr
                         : runtime->last_polled_event_message.c_str();
  out_event->message_size = runtime->last_polled_event_message.size();
  *out_has_event = true;
  return MLN_STATUS_OK;
}

auto retain_runtime_map(mln_runtime* runtime) -> mln_status {
  const auto status = validate_runtime(runtime);
  if (status != MLN_STATUS_OK) {
    return status;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  ++runtime->live_maps;
  return MLN_STATUS_OK;
}

auto release_runtime_map(mln_runtime* runtime) noexcept -> void {
  if (runtime == nullptr) {
    return;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  if (runtime_registry().contains(runtime) && runtime->live_maps != 0) {
    --runtime->live_maps;
  }
}

auto resource_options_for_runtime(mln_runtime* runtime)
  -> mbgl::ResourceOptions {
  auto options = mbgl::ResourceOptions::Default();
  if (runtime != nullptr) {
    options.withPlatformContext(runtime);
    if (!runtime->asset_path.empty()) {
      options.withAssetPath(runtime->asset_path);
    }
    if (!runtime->cache_path.empty()) {
      options.withCachePath(runtime->cache_path);
    }
    if (runtime->has_maximum_cache_size) {
      options.withMaximumCacheSize(runtime->maximum_cache_size);
    }
  }
  return options;
}

auto find_runtime_for_platform_context(void* platform_context) noexcept
  -> mln_runtime* {
  if (platform_context == nullptr) {
    return nullptr;
  }

  const std::scoped_lock lock(runtime_registry_mutex());
  auto* runtime = static_cast<mln_runtime*>(platform_context);
  return runtime_registry().contains(runtime) ? runtime : nullptr;
}

auto push_runtime_map_event(
  mln_runtime* runtime, mln_map* map, uint32_t type, int32_t code,
  const char* message
) -> void {
  push_runtime_map_event_payload(
    runtime, map, type, MLN_RUNTIME_EVENT_PAYLOAD_NONE, {}, code,
    message == nullptr ? std::string{} : std::string{message}
  );
}

auto push_runtime_map_event_payload(
  mln_runtime* runtime, mln_map* map, uint32_t type, uint32_t payload_type,
  std::vector<std::byte> payload, int32_t code, std::string message
) -> void {
  if (runtime == nullptr) {
    return;
  }

  auto event = QueuedRuntimeEvent{
    .type = type,
    .source_type = MLN_RUNTIME_EVENT_SOURCE_MAP,
    .source = map,
    .map = map,
    .code = code,
    .payload_type = payload_type,
    .payload = std::move(payload),
    .message = std::move(message)
  };

  const std::scoped_lock lock(runtime->event_mutex);
  if (map != nullptr && !runtime->event_maps.contains(map)) {
    return;
  }
  if (type == MLN_RUNTIME_EVENT_MAP_LOADING_FAILED && map != nullptr) {
    runtime->map_loading_failures[map] = event.message;
  }
  runtime->events.push_back(std::move(event));
}

auto register_runtime_map_events(mln_runtime* runtime, const mln_map* map)
  -> void {
  if (runtime == nullptr || map == nullptr) {
    return;
  }

  const std::scoped_lock lock(runtime->event_mutex);
  runtime->event_maps.insert(map);
}

auto clear_runtime_map_loading_failure(mln_runtime* runtime, const mln_map* map)
  -> void {
  if (runtime == nullptr || map == nullptr) {
    return;
  }

  const std::scoped_lock lock(runtime->event_mutex);
  runtime->map_loading_failures.erase(map);
}

auto runtime_map_loading_failed(mln_runtime* runtime, const mln_map* map)
  -> bool {
  if (runtime == nullptr || map == nullptr) {
    return false;
  }

  const std::scoped_lock lock(runtime->event_mutex);
  return runtime->map_loading_failures.contains(map);
}

auto runtime_map_loading_failure_message(
  mln_runtime* runtime, const mln_map* map
) -> std::string {
  if (runtime == nullptr || map == nullptr) {
    return {};
  }

  const std::scoped_lock lock(runtime->event_mutex);
  const auto found = runtime->map_loading_failures.find(map);
  return found == runtime->map_loading_failures.end() ? std::string{}
                                                      : found->second;
}

auto discard_runtime_map_events(mln_runtime* runtime, const mln_map* map)
  -> void {
  if (runtime == nullptr || map == nullptr) {
    return;
  }

  const std::scoped_lock lock(runtime->event_mutex);
  runtime->event_maps.erase(map);
  std::erase_if(runtime->events, [map](const auto& event) -> bool {
    return event.map == map;
  });
  runtime->map_loading_failures.erase(map);
}

}  // namespace mln::core

// NOLINTEND(cppcoreguidelines-pro-type-union-access)
