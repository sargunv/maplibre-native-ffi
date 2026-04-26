#ifndef MAPLIBRE_NATIVE_ABI_H
#define MAPLIBRE_NATIVE_ABI_H

#include <stdint.h>

#if defined(_WIN32)
#if defined(MLN_BUILDING_ABI)
#define MLN_API __declspec(dllexport)
#else
#define MLN_API __declspec(dllimport)
#endif
#else
#define MLN_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mln_status {
    MLN_STATUS_OK = 0,
    MLN_STATUS_INVALID_ARGUMENT = -1,
} mln_status;

MLN_API uint32_t mln_abi_version(void);
MLN_API mln_status mln_hello_world(const char** out_message);

#ifdef __cplusplus
}
#endif

#endif
