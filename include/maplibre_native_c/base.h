/**
 * @file maplibre_native_c/base.h
 * Public C API declarations for base ABI types and status values.
 */

#ifndef MAPLIBRE_NATIVE_C_BASE_H
#define MAPLIBRE_NATIVE_C_BASE_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#if defined(MLN_BUILDING_C)
#define MLN_API __declspec(dllexport)
#else
#define MLN_API __declspec(dllimport)
#endif
#else
#define MLN_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define MLN_NOEXCEPT noexcept
#else
#define MLN_NOEXCEPT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup base Base ABI contract
 * @{
 */
/** Status values returned by status-returning functions. */
typedef enum mln_status : int32_t {
  MLN_STATUS_OK = 0,
  /** A pointer, size field, mask, or handle argument was invalid. */
  MLN_STATUS_INVALID_ARGUMENT = -1,
  /** The object is valid but not currently in a state that permits the call. */
  MLN_STATUS_INVALID_STATE = -2,
  /** The handle is thread-affine and the call was made from the wrong thread.
   */
  MLN_STATUS_WRONG_THREAD = -3,
  /** The entry point or requested behavior is unavailable in this build. */
  MLN_STATUS_UNSUPPORTED = -4,
  /** A native MapLibre error or C++ exception was converted to status. */
  MLN_STATUS_NATIVE_ERROR = -5,
} mln_status;

typedef struct mln_runtime mln_runtime;
typedef struct mln_map mln_map;
typedef struct mln_map_projection mln_map_projection;
typedef struct mln_offline_region_snapshot mln_offline_region_snapshot;
typedef struct mln_offline_region_list mln_offline_region_list;
typedef struct mln_json_snapshot mln_json_snapshot;
typedef struct mln_resource_request_handle mln_resource_request_handle;
typedef struct mln_render_session mln_render_session;

/**
 * Reports the C ABI contract version.
 *
 * The value is 0 while the ABI is unstable. Stable ABI contract editions use
 * YYYYMMDD and change only when the ABI contract changes.
 */
MLN_API uint32_t mln_c_version(void) MLN_NOEXCEPT;

/** @} */

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_BASE_H
