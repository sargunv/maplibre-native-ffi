#ifndef MAPLIBRE_NATIVE_ABI_H
#define MAPLIBRE_NATIVE_ABI_H

// NOLINTBEGIN(cppcoreguidelines-use-enum-class)
// NOLINTBEGIN(modernize-deprecated-headers)
// NOLINTBEGIN(modernize-use-trailing-return-type)
// NOLINTBEGIN(modernize-use-using)
// NOLINTBEGIN(readability-uppercase-literal-suffix)

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
typedef struct mln_map mln_map;

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

typedef struct mln_runtime_options {
  uint32_t size;
  uint32_t flags;
} mln_runtime_options;

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
 * Returns the C ABI contract version.
 *
 * Returns 0 while the ABI is unstable. Stable ABI contract editions use YYYYMM
 * and only change when the ABI contract changes.
 */
MLN_API uint32_t mln_abi_version(void) MLN_NOEXCEPT;

/**
 * Returns default runtime options with the ABI size field populated.
 */
MLN_API mln_runtime_options mln_runtime_options_default(void) MLN_NOEXCEPT;

/**
 * Returns the last thread-local diagnostic message, or an empty string.
 *
 * The returned pointer is owned by the ABI and remains valid until the next ABI
 * call on the same thread that writes a thread-local diagnostic.
 */
MLN_API const char* mln_thread_last_error_message(void) MLN_NOEXCEPT;

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
) MLN_NOEXCEPT;

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
MLN_API mln_status mln_runtime_destroy(mln_runtime* runtime) MLN_NOEXCEPT;

/** Runs one pending owner-thread task for this runtime/map thread, if any. */
MLN_API mln_status mln_runtime_run_once(mln_runtime* runtime) MLN_NOEXCEPT;

/** Returns default map options with the ABI size field populated. */
MLN_API mln_map_options mln_map_options_default(void) MLN_NOEXCEPT;

/** Returns default empty camera options with the ABI size field populated. */
MLN_API mln_camera_options mln_camera_options_default(void) MLN_NOEXCEPT;

/** Creates a map handle on the runtime owner thread. */
MLN_API mln_status mln_map_create(
  mln_runtime* runtime, const mln_map_options* options, mln_map** out_map
) MLN_NOEXCEPT;

/** Destroys a map handle on its owner thread. */
MLN_API mln_status mln_map_destroy(mln_map* map) MLN_NOEXCEPT;

/** Loads a style URL through MapLibre Native style APIs. */
MLN_API mln_status
mln_map_set_style_url(mln_map* map, const char* url) MLN_NOEXCEPT;

/** Loads inline style JSON through MapLibre Native style APIs. */
MLN_API mln_status
mln_map_set_style_json(mln_map* map, const char* json) MLN_NOEXCEPT;

/** Returns the current camera snapshot. */
MLN_API mln_status
mln_map_get_camera(mln_map* map, mln_camera_options* out_camera) MLN_NOEXCEPT;

/** Applies a camera jump command. */
MLN_API mln_status
mln_map_jump_to(mln_map* map, const mln_camera_options* camera) MLN_NOEXCEPT;

/** Applies a screen-space pan command. */
MLN_API mln_status
mln_map_move_by(mln_map* map, double delta_x, double delta_y) MLN_NOEXCEPT;

/** Applies a screen-space zoom command. */
MLN_API mln_status mln_map_scale_by(
  mln_map* map, double scale, const mln_screen_point* anchor
) MLN_NOEXCEPT;

/** Applies a screen-space rotate command. */
MLN_API mln_status mln_map_rotate_by(
  mln_map* map, mln_screen_point first, mln_screen_point second
) MLN_NOEXCEPT;

/** Applies a pitch delta command. */
MLN_API mln_status mln_map_pitch_by(mln_map* map, double pitch) MLN_NOEXCEPT;

/** Cancels active camera transitions. */
MLN_API mln_status mln_map_cancel_transitions(mln_map* map) MLN_NOEXCEPT;

/** Pops the next queued map event, returning MLN_STATUS_ACCEPTED if empty. */
MLN_API mln_status
mln_map_poll_event(mln_map* map, mln_map_event* out_event) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

// NOLINTEND(readability-uppercase-literal-suffix)
// NOLINTEND(modernize-use-using)
// NOLINTEND(modernize-use-trailing-return-type)
// NOLINTEND(modernize-deprecated-headers)
// NOLINTEND(cppcoreguidelines-use-enum-class)

#endif
