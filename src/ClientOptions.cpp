#include "ClientOptions.h"

#include <mbgl/util/client_options.hpp>

extern "C"
{
  auto MLN_ClientOptions_new() -> MLN_ClientOptions *
  {
    auto *options = new mbgl::ClientOptions();
    return options;
  }

  void MLN_ClientOptions_delete(MLN_ClientOptions *nativePtr)
  {
    delete static_cast<mbgl::ClientOptions *>(nativePtr);
  }

  void MLN_ClientOptions_setName(MLN_ClientOptions *nativePtr, const char *name)
  {
    static_cast<mbgl::ClientOptions *>(nativePtr)->withName(name);
  }

  auto MLN_ClientOptions_name(MLN_ClientOptions *nativePtr) -> const char *
  {
    return static_cast<mbgl::ClientOptions *>(nativePtr)->name().c_str();
  }

  void MLN_ClientOptions_setVersion(
    MLN_ClientOptions *nativePtr, const char *version
  )
  {
    static_cast<mbgl::ClientOptions *>(nativePtr)->withVersion(version);
  }

  auto MLN_ClientOptions_version(MLN_ClientOptions *nativePtr) -> const char *
  {
    return static_cast<mbgl::ClientOptions *>(nativePtr)->version().c_str();
  }
}
