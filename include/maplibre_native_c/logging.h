/**
 * @file maplibre_native_c/logging.h
 * Public C API declarations for logging.
 */

#ifndef MAPLIBRE_NATIVE_C_LOGGING_H
#define MAPLIBRE_NATIVE_C_LOGGING_H

#include <stdint.h>

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Logging
/** Log severity values emitted by MapLibre Native. */
typedef enum mln_log_severity : uint32_t {
  MLN_LOG_SEVERITY_INFO = 1,
  MLN_LOG_SEVERITY_WARNING = 2,
  MLN_LOG_SEVERITY_ERROR = 3,
} mln_log_severity;

/** Bitmask values for log severities dispatched asynchronously. */
typedef enum mln_log_severity_mask : uint32_t {
  MLN_LOG_SEVERITY_MASK_INFO = 1U << MLN_LOG_SEVERITY_INFO,
  MLN_LOG_SEVERITY_MASK_WARNING = 1U << MLN_LOG_SEVERITY_WARNING,
  MLN_LOG_SEVERITY_MASK_ERROR = 1U << MLN_LOG_SEVERITY_ERROR,
  MLN_LOG_SEVERITY_MASK_DEFAULT =
    MLN_LOG_SEVERITY_MASK_INFO | MLN_LOG_SEVERITY_MASK_WARNING,
  MLN_LOG_SEVERITY_MASK_ALL = MLN_LOG_SEVERITY_MASK_INFO |
                              MLN_LOG_SEVERITY_MASK_WARNING |
                              MLN_LOG_SEVERITY_MASK_ERROR,
} mln_log_severity_mask;

/** Log event categories emitted by MapLibre Native. */
typedef enum mln_log_event : uint32_t {
  MLN_LOG_EVENT_GENERAL = 0,
  MLN_LOG_EVENT_SETUP = 1,
  MLN_LOG_EVENT_SHADER = 2,
  MLN_LOG_EVENT_PARSE_STYLE = 3,
  MLN_LOG_EVENT_PARSE_TILE = 4,
  MLN_LOG_EVENT_RENDER = 5,
  MLN_LOG_EVENT_STYLE = 6,
  MLN_LOG_EVENT_DATABASE = 7,
  MLN_LOG_EVENT_HTTP_REQUEST = 8,
  MLN_LOG_EVENT_SPRITE = 9,
  MLN_LOG_EVENT_IMAGE = 10,
  MLN_LOG_EVENT_OPENGL = 11,
  MLN_LOG_EVENT_JNI = 12,
  MLN_LOG_EVENT_ANDROID = 13,
  MLN_LOG_EVENT_CRASH = 14,
  MLN_LOG_EVENT_GLYPH = 15,
  MLN_LOG_EVENT_TIMING = 16,
} mln_log_event;

/**
 * Receives a MapLibre Native log record.
 *
 * The message pointer is borrowed for the callback duration. Returning non-zero
 * consumes the record. Returning zero lets MapLibre Native's platform logger
 * handle it.
 */
typedef uint32_t (*mln_log_callback)(
  void* user_data, uint32_t severity, uint32_t event, int64_t code,
  const char* message
);

/**
 * Installs a process-global MapLibre Native log callback.
 *
 * Passing null clears the current callback. The callback and user_data are
 * stored by reference and must remain valid until the callback is replaced or
 * cleared.
 *
 * The callback is a low-level native callback:
 *
 * - MapLibre may invoke it from logging or worker threads selected by the async
 *   severity mask.
 * - MapLibre may invoke it while holding internal logging locks.
 * - The callback must be thread-safe, return quickly, and must not call this C
 *   API or MapLibre Native APIs.
 * - Language adapters for runtimes that restrict native-thread callbacks can
 *   marshal records into host-managed logging facilities before invoking user
 *   code.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_log_set_callback(mln_log_callback callback, void* user_data) MLN_NOEXCEPT;

/**
 * Clears the process-global log callback.
 *
 * After this call succeeds, future log dispatches no longer use the callback
 * that was previously registered through mln_log_set_callback().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_log_clear_callback(void) MLN_NOEXCEPT;

/**
 * Controls which log severities MapLibre Native may dispatch asynchronously.
 *
 * MLN_LOG_SEVERITY_MASK_DEFAULT restores MapLibre Native's default behavior:
 * info and warning records may be asynchronous, while error records remain
 * synchronous.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when mask contains unknown bits.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_log_set_async_severity_mask(uint32_t mask) MLN_NOEXCEPT;

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_LOGGING_H
