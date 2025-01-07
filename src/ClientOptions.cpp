#include "ClientOptions.h"

#include <mbgl/util/client_options.hpp>

extern "C" {
auto MLN_ClientOptions_new(const char *name, const char *version)
    -> MLN_ClientOptions * {
  auto *options = new mbgl::ClientOptions();
  options->withName(name).withVersion(version);
  return options;
}

void MLN_ClientOptions_delete(MLN_ClientOptions *nativePtr) {
  delete static_cast<mbgl::ClientOptions *>(nativePtr);
}

auto MLN_ClientOptions_name(MLN_ClientOptions *nativePtr) -> const char * {
  return static_cast<mbgl::ClientOptions *>(nativePtr)->name().c_str();
}

auto MLN_ClientOptions_version(MLN_ClientOptions *nativePtr) -> const char * {
  return static_cast<mbgl::ClientOptions *>(nativePtr)->version().c_str();
}
}
