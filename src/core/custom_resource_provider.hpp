#pragma once

#include <memory>

#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/util/async_request.hpp>
#include <mbgl/util/run_loop.hpp>

#include "maplibre_native_abi.h"

namespace mln::core {

auto request_custom_resource(
  const mbgl::Resource& resource,
  mln_resource_provider_callback provider_callback, void* user_data,
  mbgl::util::RunLoop* run_loop, mbgl::FileSource::Callback file_source_callback
) -> std::unique_ptr<mbgl::AsyncRequest>;

}  // namespace mln::core
