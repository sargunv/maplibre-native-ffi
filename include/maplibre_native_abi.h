#ifndef MAPLIBRE_NATIVE_ABI_H
#define MAPLIBRE_NATIVE_ABI_H

// NOLINTBEGIN(
//   cppcoreguidelines-use-enum-class,
//   modernize-use-trailing-return-type,
//   modernize-use-using
// )

#include <stdint.h>

#ifdef _WIN32
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
  MLN_STATUS_ACCEPTED = 1,
  MLN_STATUS_INVALID_ARGUMENT = -1,
  MLN_STATUS_INVALID_STATE = -2,
  MLN_STATUS_WRONG_THREAD = -3,
  MLN_STATUS_UNSUPPORTED = -4,
  MLN_STATUS_NATIVE_ERROR = -5,
} mln_status;

typedef struct mln_runtime mln_runtime;

typedef struct mln_runtime_options {
  uint32_t size;
  uint32_t flags;
} mln_runtime_options;

/**
 * Returns the C ABI contract version.
 *
 * Returns 0 while the ABI is unstable. Stable ABI contract editions use YYYYMM
 * and only change when the ABI contract changes.
 */
MLN_API uint32_t mln_abi_version(void);

/**
 * Returns default runtime options with the ABI size field populated.
 */
MLN_API mln_runtime_options mln_runtime_options_default(void);

/**
 * Returns the last thread-local diagnostic message, or an empty string.
 *
 * The returned pointer is owned by the ABI and remains valid until the next ABI
 * call on the same thread that writes a thread-local diagnostic.
 */
MLN_API const char* mln_thread_last_error_message(void);

/**
 * Creates a runtime handle on the calling thread.
 *
 * The options pointer may be null. When non-null, options->size must be at
 * least sizeof(mln_runtime_options). The created runtime must be destroyed on
 * the same thread.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_runtime is null, *out_runtime is not
 *   null, or options has an unsupported size.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
);

/**
 * Destroys a runtime handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not a live runtime
 *   handle.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the creating
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_destroy(mln_runtime* runtime);

#ifdef __cplusplus
}
#endif

// NOLINTEND(
//   cppcoreguidelines-use-enum-class,
//   modernize-use-trailing-return-type,
//   modernize-use-using
// )

#endif
