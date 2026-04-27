#pragma once

#include <thread>

#include "maplibre_native_abi.h"

struct mln_runtime {
  std::thread::id owner_thread;
};

namespace mln::core {

auto create_runtime(
  const mln_runtime_options* options, mln_runtime** out_runtime
) -> mln_status;
auto destroy_runtime(mln_runtime* runtime) -> mln_status;

}  // namespace mln::core
