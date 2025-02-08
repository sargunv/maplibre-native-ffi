#include "ResourceOptions.h"

#include <cstdint>

#include <mbgl/storage/resource_options.hpp>
#include <mbgl/util/tile_server_options.hpp>

extern "C"
{
  auto MLN_ResourceOptions_new() -> void *
  {
    auto *options = new mbgl::ResourceOptions();
    options->withTileServerOptions(
      mbgl::TileServerOptions::MapLibreConfiguration()
    );
    return options;
  }

  void MLN_ResourceOptions_delete(MLN_ResourceOptions *nativePtr)
  {
    delete static_cast<mbgl::ResourceOptions *>(nativePtr);
  }

  void MLN_ResourceOptions_setCachePath(
    MLN_ResourceOptions *nativePtr, const char *path
  )
  {
    static_cast<mbgl::ResourceOptions *>(nativePtr)->withCachePath(path);
  }

  void MLN_ResourceOptions_setAssetPath(
    MLN_ResourceOptions *nativePtr, const char *path
  )
  {
    static_cast<mbgl::ResourceOptions *>(nativePtr)->withAssetPath(path);
  }

  void MLN_ResourceOptions_setMaximumCacheSize(
    MLN_ResourceOptions *nativePtr, const uint64_t size
  )
  {
    static_cast<mbgl::ResourceOptions *>(nativePtr)->withMaximumCacheSize(size);
  }

  auto MLN_ResourceOptions_cachePath(MLN_ResourceOptions *nativePtr) -> const
    char *
  {
    return static_cast<mbgl::ResourceOptions *>(nativePtr)->cachePath().c_str();
  }

  auto MLN_ResourceOptions_assetPath(MLN_ResourceOptions *nativePtr) -> const
    char *
  {
    return static_cast<mbgl::ResourceOptions *>(nativePtr)->assetPath().c_str();
  }

  auto MLN_ResourceOptions_maximumCacheSize(MLN_ResourceOptions *nativePtr)
    -> uint64_t
  {
    return static_cast<mbgl::ResourceOptions *>(nativePtr)->maximumCacheSize();
  }
}
