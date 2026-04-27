#include <exception>
#include <memory>

#include <mbgl/storage/asset_file_source.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_manager.hpp>
#include <mbgl/storage/local_file_source.hpp>
#include <mbgl/storage/mbtiles_file_source.hpp>
#include <mbgl/storage/pmtiles_file_source.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/client_options.hpp>

#include "core/resource_loader.hpp"

namespace mbgl {

auto FileSourceManager::get() noexcept -> FileSourceManager* {
  static auto manager = FileSourceManager{};
  static const auto registered = []() noexcept -> bool {
    try {
      manager.registerFileSourceFactory(
        FileSourceType::Asset,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return std::make_unique<AssetFileSource>(
            resource_options, client_options
          );
        }
      );
      manager.registerFileSourceFactory(
        FileSourceType::Database,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return mln::core::make_database_file_source(
            resource_options, client_options
          );
        }
      );
      manager.registerFileSourceFactory(
        FileSourceType::FileSystem,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return std::make_unique<LocalFileSource>(
            resource_options, client_options
          );
        }
      );
      manager.registerFileSourceFactory(
        FileSourceType::Network,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return mln::core::make_network_file_source(
            resource_options, client_options
          );
        }
      );
      manager.registerFileSourceFactory(
        FileSourceType::Mbtiles,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return std::make_unique<MBTilesFileSource>(
            resource_options, client_options
          );
        }
      );
      manager.registerFileSourceFactory(
        FileSourceType::Pmtiles,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return std::make_unique<PMTilesFileSource>(
            resource_options, client_options
          );
        }
      );
      manager.registerFileSourceFactory(
        FileSourceType::ResourceLoader,
        [](
          const ResourceOptions& resource_options,
          const ClientOptions& client_options
        ) -> std::unique_ptr<FileSource> {
          return mln::core::make_main_resource_loader(
            resource_options, client_options
          );
        }
      );
      return true;
    } catch (...) {
      std::terminate();
    }
  }();
  static_cast<void>(registered);
  return &manager;
}

}  // namespace mbgl
