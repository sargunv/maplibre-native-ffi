#pragma once

extern "C" {
using MLN_ClientOptions = void;
auto MLN_ClientOptions_new() -> MLN_ClientOptions *;
void MLN_ClientOptions_delete(MLN_ClientOptions *nativePtr);
void MLN_ClientOptions_setName(MLN_ClientOptions *nativePtr, const char *name);
auto MLN_ClientOptions_name(MLN_ClientOptions *nativePtr) -> const char *;
void MLN_ClientOptions_setVersion(
    MLN_ClientOptions *nativePtr, const char *version
);
auto MLN_ClientOptions_version(MLN_ClientOptions *nativePtr) -> const char *;
}
