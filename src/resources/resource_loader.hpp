#pragma once

#include <memory>

#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/client_options.hpp>

namespace mln::core {

auto make_network_file_source(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource>;

auto make_database_file_source(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource>;

auto make_main_resource_loader(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource>;

}  // namespace mln::core
