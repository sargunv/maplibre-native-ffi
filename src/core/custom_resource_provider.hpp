#pragma once

#include <memory>

#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/util/async_request.hpp>

#include "maplibre_native_abi.h"

namespace mln::core {

auto request_custom_resource(
  const mbgl::Resource& resource,
  mln_resource_provider_callback provider_callback, void* user_data,
  mbgl::FileSource::Callback file_source_callback
) -> std::unique_ptr<mbgl::AsyncRequest>;

auto complete_resource_request(
  mln_resource_request_handle* handle, const mln_resource_response* response
) -> mln_status;

auto resource_request_cancelled(
  const mln_resource_request_handle* handle, bool* out_cancelled
) -> mln_status;
void release_resource_request(mln_resource_request_handle* handle) noexcept;

}  // namespace mln::core
