#pragma once

#include <cstddef>
#include <thread>

#include "maplibre_native_abi.h"

struct mln_runtime {
  std::thread::id owner_thread;
  std::size_t live_maps = 0;
};

namespace mln::core {

auto create_runtime(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status;
auto destroy_runtime(mln_runtime* runtime) -> mln_status;
auto run_runtime_once(mln_runtime* runtime) -> mln_status;
auto retain_runtime_map(mln_runtime* runtime) -> mln_status;
auto release_runtime_map(mln_runtime* runtime) noexcept -> void;
auto validate_runtime(mln_runtime* runtime) -> mln_status;

}  // namespace mln::core
