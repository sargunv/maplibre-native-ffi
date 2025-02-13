#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

  using MLN_ResourceOptions = void;
  auto MLN_ResourceOptions_new() -> MLN_ResourceOptions *;
  void MLN_ResourceOptions_delete(MLN_ResourceOptions *nativePtr);
  void MLN_ResourceOptions_setCachePath(
    MLN_ResourceOptions *nativePtr, const char *path
  );
  auto MLN_ResourceOptions_cachePath(MLN_ResourceOptions *nativePtr) -> const
    char *;
  void MLN_ResourceOptions_setAssetPath(
    MLN_ResourceOptions *nativePtr, const char *path
  );
  auto MLN_ResourceOptions_assetPath(MLN_ResourceOptions *nativePtr) -> const
    char *;
  void MLN_ResourceOptions_setMaximumCacheSize(
    MLN_ResourceOptions *nativePtr, uint64_t size
  );
  auto MLN_ResourceOptions_maximumCacheSize(MLN_ResourceOptions *nativePtr)
    -> uint64_t;

#ifdef __cplusplus
}
#endif
