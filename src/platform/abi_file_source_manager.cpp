#include <exception>

#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_manager.hpp>

#include "core/resource_loader.hpp"

namespace mbgl {

auto FileSourceManager::get() noexcept -> FileSourceManager* {
  static auto manager = FileSourceManager{};
  static const auto registered = []() noexcept -> bool {
    try {
      manager.registerFileSourceFactory(
        FileSourceType::ResourceLoader, mln::core::make_resource_loader
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
