#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/run_loop.hpp>

#include "maplibre_native_abi.h"

namespace mln::core {

struct ResourceProvider {
  mln_resource_provider_callback callback = nullptr;
  void* user_data = nullptr;
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
};

namespace mln::core {

auto create_runtime(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status;
auto destroy_runtime(mln_runtime* runtime) -> mln_status;
auto run_runtime_once(mln_runtime* runtime) -> mln_status;
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

}  // namespace mln::core
