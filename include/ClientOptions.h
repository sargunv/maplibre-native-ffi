#pragma once

extern "C" {
using MLN_ClientOptions = void;
auto MLN_ClientOptions_new(const char *name, const char *version) -> void *;
void MLN_ClientOptions_delete(void *nativePtr);
auto MLN_ClientOptions_name(void *nativePtr) -> const char *;
auto MLN_ClientOptions_version(void *nativePtr) -> const char *;
}
