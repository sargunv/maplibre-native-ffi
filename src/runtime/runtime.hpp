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

namespace mln::core {

struct ResourceProvider {
  mln_resource_provider_callback callback = nullptr;
  void* user_data = nullptr;
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
};

}  // namespace mln::core

struct mln_runtime {
  std::thread::id owner_thread;
  std::unique_ptr<mbgl::util::RunLoop> run_loop;
  std::string asset_path;
  std::string cache_path;
  bool has_maximum_cache_size = false;
  std::uint64_t maximum_cache_size = 0;
  bool has_resource_provider = false;
  mln::core::ResourceProvider resource_provider;
  mln_resource_transform_callback resource_transform_callback = nullptr;
  void* resource_transform_user_data = nullptr;
  std::size_t live_maps = 0;
  mutable std::mutex event_mutex;
  std::unordered_set<const mln_map*> event_maps;
  std::deque<mln::core::QueuedRuntimeEvent> events;
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
