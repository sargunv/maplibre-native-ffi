#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/run_loop.hpp>

#include "maplibre_native_c.h"

namespace mbgl {
class DatabaseFileSource;
}  // namespace mbgl

namespace mln::core {

struct ResourceProvider {
  mln_resource_provider_callback callback = nullptr;
  void* user_data = nullptr;
};

struct OfflineRegionEventState {
  std::mutex mutex;
  mln_runtime* runtime = nullptr;
  bool alive = false;
};

struct QueuedRuntimeEvent {
  uint32_t type;
  uint32_t source_type;
  void* source;
  mln_map* map;
  int32_t code;
  uint32_t payload_type;
  std::vector<std::byte> payload;
  std::string message;
  bool has_offline_region = false;
  mln_offline_region_id offline_region_id = 0;
};

}  // namespace mln::core

struct mln_runtime {
  std::thread::id owner_thread;
  std::unique_ptr<mbgl::util::RunLoop> run_loop;
  std::string asset_path;
  std::string cache_path;
  std::shared_ptr<mbgl::DatabaseFileSource> database_source;
  bool has_maximum_cache_size = false;
  std::uint64_t maximum_cache_size = 0;
  bool has_resource_provider = false;
  mln::core::ResourceProvider resource_provider;
  std::shared_ptr<mln::core::OfflineRegionEventState> offline_event_state;
  mln_resource_transform_callback resource_transform_callback = nullptr;
  void* resource_transform_user_data = nullptr;
  std::size_t live_maps = 0;
  mutable std::mutex event_mutex;
  std::unordered_set<const mln_map*> event_maps;
  std::deque<mln::core::QueuedRuntimeEvent> events;
  std::unordered_set<mln_offline_region_id> observed_offline_regions;
  std::vector<std::byte> last_polled_event_payload;
  std::string last_polled_event_message;
  std::unordered_map<const mln_map*, std::string> map_loading_failures;
};

namespace mln::core {

auto create_runtime(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status;
auto destroy_runtime(mln_runtime* runtime) -> mln_status;
auto run_runtime_once(mln_runtime* runtime) -> mln_status;
auto poll_runtime_event(
  mln_runtime* runtime, mln_runtime_event* out_event, bool* out_has_event
) -> mln_status;
auto set_resource_provider(
  mln_runtime* runtime, const mln_resource_provider* provider
) -> mln_status;
auto set_resource_transform(
  mln_runtime* runtime, const mln_resource_transform* transform
) -> mln_status;
auto run_ambient_cache_operation(mln_runtime* runtime, uint32_t operation)
  -> mln_status;
auto offline_region_create(
  mln_runtime* runtime, const mln_offline_region_definition* definition,
  const uint8_t* metadata, size_t metadata_size,
  mln_offline_region_snapshot** out_region
) -> mln_status;
auto offline_region_get(
  mln_runtime* runtime, mln_offline_region_id region_id,
  mln_offline_region_snapshot** out_region, bool* out_found
) -> mln_status;
auto offline_regions_list(
  mln_runtime* runtime, mln_offline_region_list** out_regions
) -> mln_status;
auto offline_regions_merge_database(
  mln_runtime* runtime, const char* side_database_path,
  mln_offline_region_list** out_regions
) -> mln_status;
auto offline_region_update_metadata(
  mln_runtime* runtime, mln_offline_region_id region_id,
  const uint8_t* metadata, size_t metadata_size,
  mln_offline_region_snapshot** out_region
) -> mln_status;
auto offline_region_get_status(
  mln_runtime* runtime, mln_offline_region_id region_id,
  mln_offline_region_status* out_status
) -> mln_status;
auto offline_region_set_observed(
  mln_runtime* runtime, mln_offline_region_id region_id, bool observed
) -> mln_status;

struct OfflineRegionDownloadStateRequest {
  mln_offline_region_id region_id;
  uint32_t state;
};

auto offline_region_set_download_state(
  mln_runtime* runtime, OfflineRegionDownloadStateRequest request
) -> mln_status;
auto offline_region_invalidate(
  mln_runtime* runtime, mln_offline_region_id region_id
) -> mln_status;
auto offline_region_delete(
  mln_runtime* runtime, mln_offline_region_id region_id
) -> mln_status;
auto offline_region_snapshot_get(
  const mln_offline_region_snapshot* snapshot, mln_offline_region_info* out_info
) -> mln_status;
auto offline_region_snapshot_destroy(
  mln_offline_region_snapshot* snapshot
) noexcept -> void;
auto offline_region_list_count(
  const mln_offline_region_list* list, size_t* out_count
) -> mln_status;
auto offline_region_list_get(
  const mln_offline_region_list* list, size_t index,
  mln_offline_region_info* out_info
) -> mln_status;
auto offline_region_list_destroy(mln_offline_region_list* list) noexcept
  -> void;
auto retain_runtime_map(mln_runtime* runtime) -> mln_status;
auto release_runtime_map(mln_runtime* runtime) noexcept -> void;
auto validate_runtime(mln_runtime* runtime) -> mln_status;
auto resource_options_for_runtime(mln_runtime* runtime)
  -> mbgl::ResourceOptions;
auto find_runtime_for_platform_context(void* platform_context) noexcept
  -> mln_runtime*;
auto push_runtime_map_event(
  mln_runtime* runtime, mln_map* map, uint32_t type, int32_t code = 0,
  const char* message = nullptr
) -> void;
auto push_runtime_map_event_payload(
  mln_runtime* runtime, mln_map* map, uint32_t type, uint32_t payload_type,
  std::vector<std::byte> payload, int32_t code = 0, std::string message = {}
) -> void;
auto register_runtime_map_events(mln_runtime* runtime, const mln_map* map)
  -> void;
auto clear_runtime_map_loading_failure(mln_runtime* runtime, const mln_map* map)
  -> void;
auto runtime_map_loading_failed(mln_runtime* runtime, const mln_map* map)
  -> bool;
auto runtime_map_loading_failure_message(
  mln_runtime* runtime, const mln_map* map
) -> std::string;
auto discard_runtime_map_events(mln_runtime* runtime, const mln_map* map)
  -> void;

}  // namespace mln::core
