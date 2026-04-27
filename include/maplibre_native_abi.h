#ifndef MAPLIBRE_NATIVE_ABI_H
#define MAPLIBRE_NATIVE_ABI_H

// NOLINTBEGIN(cppcoreguidelines-use-enum-class)
// NOLINTBEGIN(modernize-deprecated-headers)
// NOLINTBEGIN(modernize-use-trailing-return-type)
// NOLINTBEGIN(modernize-use-using)
// NOLINTBEGIN(readability-uppercase-literal-suffix)

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
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
#define MLN_NOEXCEPT noexcept
#else
#define MLN_NOEXCEPT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ABI contract notes:
 *
 * All functions are memory-safe to call from any thread. Functions that operate
 * on thread-affine handles validate the caller thread and return
 * MLN_STATUS_WRONG_THREAD rather than causing undefined behavior. Functions
 * without an explicit owner-thread requirement may be called from any thread.
 *
 * Status-returning functions clear thread-local diagnostics on entry. When a
 * synchronous failure status is returned, callers should read
 * mln_thread_last_error_message() on the same thread before making another ABI
 * call. Asynchronous native failures are reported through map events.
 */

typedef enum mln_status {
  MLN_STATUS_OK = 0,
  /** A pointer, size field, mask, or handle argument was invalid. */
  MLN_STATUS_INVALID_ARGUMENT = -1,
  /** The object is valid but not currently in a state that permits the call. */
  MLN_STATUS_INVALID_STATE = -2,
  /** The handle is thread-affine and the call was made from the wrong thread.
   */
  MLN_STATUS_WRONG_THREAD = -3,
  /** The ABI entry point or requested behavior is not supported on this build.
   */
  MLN_STATUS_UNSUPPORTED = -4,
  /** A native MapLibre error or C++ exception was converted to status. */
  MLN_STATUS_NATIVE_ERROR = -5,
} mln_status;

typedef struct mln_runtime mln_runtime;
typedef struct mln_map mln_map;

/**
 * Returns the C ABI contract version.
 *
 * Returns 0 while the ABI is unstable. Stable ABI contract editions use YYYYMM
 * and only change when the ABI contract changes.
 */
MLN_API uint32_t mln_abi_version(void) MLN_NOEXCEPT;

/**
 * Returns the last thread-local diagnostic message, or an empty string.
 *
 * The returned pointer is owned by the ABI and remains valid until the next ABI
 * call on the same thread that writes a thread-local diagnostic.
 */
MLN_API const char* mln_thread_last_error_message(void) MLN_NOEXCEPT;

typedef enum mln_log_severity {
  MLN_LOG_SEVERITY_DEBUG = 0,
  MLN_LOG_SEVERITY_INFO = 1,
  MLN_LOG_SEVERITY_WARNING = 2,
  MLN_LOG_SEVERITY_ERROR = 3,
} mln_log_severity;

/** Bitmask values for log severities dispatched asynchronously. */
typedef enum mln_log_severity_mask {
  MLN_LOG_SEVERITY_MASK_DEBUG = 1u << MLN_LOG_SEVERITY_DEBUG,
  MLN_LOG_SEVERITY_MASK_INFO = 1u << MLN_LOG_SEVERITY_INFO,
  MLN_LOG_SEVERITY_MASK_WARNING = 1u << MLN_LOG_SEVERITY_WARNING,
  MLN_LOG_SEVERITY_MASK_ERROR = 1u << MLN_LOG_SEVERITY_ERROR,
  MLN_LOG_SEVERITY_MASK_DEFAULT = MLN_LOG_SEVERITY_MASK_DEBUG |
                                  MLN_LOG_SEVERITY_MASK_INFO |
                                  MLN_LOG_SEVERITY_MASK_WARNING,
  MLN_LOG_SEVERITY_MASK_ALL =
    MLN_LOG_SEVERITY_MASK_DEBUG | MLN_LOG_SEVERITY_MASK_INFO |
    MLN_LOG_SEVERITY_MASK_WARNING | MLN_LOG_SEVERITY_MASK_ERROR,
} mln_log_severity_mask;

/** MapLibre Native log event categories exposed as ABI-stable integer values.
 */
typedef enum mln_log_event {
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

typedef uint32_t (*mln_log_callback)(
  void* user_data, uint32_t severity, uint32_t event, int64_t code,
  const char* message
);

/**
 * Installs a process-global MapLibre Native log callback.
 *
 * Returning a non-zero value from the callback consumes the message. Returning
 * zero lets it fall through to MapLibre Native's platform logger. The callback
 * and user_data must remain valid until the callback is replaced or cleared.
 *
 * The callback may be invoked from MapLibre logging or worker threads depending
 * on the async severity mask. The callback should not call logging
 * configuration APIs.
 *
 * Passing a null callback clears the current callback.
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
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_log_clear_callback(void) MLN_NOEXCEPT;

/**
 * Sets which severities are dispatched asynchronously by MapLibre Native.
 *
 * Pass MLN_LOG_SEVERITY_MASK_DEFAULT to restore MapLibre Native's default of
 * asynchronous debug/info/warning logs and synchronous error logs.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when mask contains unknown bits.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_log_set_async_severity_mask(uint32_t mask) MLN_NOEXCEPT;

typedef struct mln_runtime_options {
  uint32_t size;
  uint32_t flags;
} mln_runtime_options;

/**
 * Returns default runtime options with the ABI size field populated.
 */
MLN_API mln_runtime_options mln_runtime_options_default(void) MLN_NOEXCEPT;

/**
 * Creates a runtime handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_runtime is null, *out_runtime is not
 *   null, or options has an unsupported size.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
) MLN_NOEXCEPT;

/**
 * Destroys a runtime handle.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not a live runtime
 *   handle.
 * - MLN_STATUS_INVALID_STATE when runtime still owns live maps.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the creating
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_destroy(mln_runtime* runtime) MLN_NOEXCEPT;

/**
 * Runs one pending owner-thread task for this runtime/map thread, if any.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not a live runtime
 *   handle.
 * - MLN_STATUS_INVALID_STATE when no run loop is active for this runtime.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_run_once(mln_runtime* runtime) MLN_NOEXCEPT;

typedef enum mln_camera_option_field {
  MLN_CAMERA_OPTION_CENTER = 1u << 0u,
  MLN_CAMERA_OPTION_ZOOM = 1u << 1u,
  MLN_CAMERA_OPTION_BEARING = 1u << 2u,
  MLN_CAMERA_OPTION_PITCH = 1u << 3u,
} mln_camera_option_field;

typedef enum mln_map_event_type {
  MLN_MAP_EVENT_NONE = 0,
  MLN_MAP_EVENT_CAMERA_WILL_CHANGE = 1,
  MLN_MAP_EVENT_CAMERA_IS_CHANGING = 2,
  MLN_MAP_EVENT_CAMERA_DID_CHANGE = 3,
  MLN_MAP_EVENT_STYLE_LOADED = 4,
  MLN_MAP_EVENT_MAP_LOADING_STARTED = 5,
  MLN_MAP_EVENT_MAP_LOADING_FINISHED = 6,
  MLN_MAP_EVENT_MAP_LOADING_FAILED = 7,
  MLN_MAP_EVENT_MAP_IDLE = 8,
  MLN_MAP_EVENT_RENDER_INVALIDATED = 9,
  MLN_MAP_EVENT_RENDER_ERROR = 10,
} mln_map_event_type;

typedef struct mln_map_options {
  uint32_t size;
  uint32_t width;
  uint32_t height;
  double scale_factor;
} mln_map_options;

typedef struct mln_camera_options {
  uint32_t size;
  uint32_t fields;
  double latitude;
  double longitude;
  double zoom;
  double bearing;
  double pitch;
} mln_camera_options;

typedef struct mln_screen_point {
  double x;
  double y;
} mln_screen_point;

typedef struct mln_map_event {
  uint32_t size;
  uint32_t type;
  int32_t code;
  char message[512];
} mln_map_event;

/**
 * Returns default map options with the ABI size field populated.
 */
MLN_API mln_map_options mln_map_options_default(void) MLN_NOEXCEPT;

/**
 * Returns default empty camera options with the ABI size field populated.
 */
MLN_API mln_camera_options mln_camera_options_default(void) MLN_NOEXCEPT;

/**
 * Creates a map handle on the runtime owner thread.
 *
 * On success, the runtime owner thread becomes the map owner thread.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, out_map is
 *   null, *out_map is not null, or options are invalid.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_create(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) MLN_NOEXCEPT;

/**
 * Destroys a map handle on its owner thread.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not a live map handle.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_destroy(mln_map* map) MLN_NOEXCEPT;

/**
 * Loads a style URL through MapLibre Native style APIs.
 *
 * Completion may be represented by the return status and later map events.
 * Synchronous failures are reported through status and thread-local
 * diagnostics; asynchronous native failures are reported through map events.
 *
 * Returns:
 * - MLN_STATUS_OK when the load request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null, not live, or url is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when a synchronous native error is reported or an
 *   internal exception is converted to status.
 */
MLN_API mln_status
mln_map_set_style_url(mln_map* map, const char* url) MLN_NOEXCEPT;

/**
 * Loads inline style JSON through MapLibre Native style APIs.
 *
 * Completion may be represented by the return status and later map events.
 * Malformed JSON can fail synchronously with diagnostics and a loading-failed
 * event.
 *
 * Returns:
 * - MLN_STATUS_OK when the load request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null, not live, or json is null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when a synchronous native error is reported or an
 *   internal exception is converted to status.
 */
MLN_API mln_status
mln_map_set_style_json(mln_map* map, const char* json) MLN_NOEXCEPT;

/**
 * Returns the current camera snapshot.
 *
 * On success, *out_camera is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_camera is
 *   null, or out_camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_get_camera(mln_map* map, mln_camera_options* out_camera) MLN_NOEXCEPT;

/**
 * Applies a camera jump command.
 *
 * Only fields indicated by camera->fields are used.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null,
 *   or camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_jump_to(mln_map* map, const mln_camera_options* camera) MLN_NOEXCEPT;

/**
 * Applies a screen-space pan command.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_move_by(mln_map* map, double delta_x, double delta_y) MLN_NOEXCEPT;

/**
 * Applies a screen-space zoom command.
 *
 * The anchor pointer may be null.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_scale_by(
  mln_map* map, double scale, const mln_screen_point* anchor
) MLN_NOEXCEPT;

/**
 * Applies a screen-space rotate command.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) MLN_NOEXCEPT;

/**
 * Applies a pitch delta command.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pitch_by(mln_map* map, double pitch) MLN_NOEXCEPT;

/**
 * Cancels active camera transitions.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_cancel_transitions(mln_map* map) MLN_NOEXCEPT;

/**
 * Pops the next queued map event.
 *
 * On success, *out_has_event is set to whether an event was available. When an
 * event is available, *out_event is overwritten. Event message storage is
 * copied into out_event and remains valid after later ABI calls.
 *
 * Returns:
 * - MLN_STATUS_OK when the poll completed; out_has_event indicates whether an
 *   event was written to out_event.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_event is
 *   null, out_has_event is null, or out_event->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_poll_event(
  mln_map* map, mln_map_event* out_event, bool* out_has_event
) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

// NOLINTEND(readability-uppercase-literal-suffix)
// NOLINTEND(modernize-use-using)
// NOLINTEND(modernize-use-trailing-return-type)
// NOLINTEND(modernize-deprecated-headers)
// NOLINTEND(cppcoreguidelines-use-enum-class)

#endif
