/**
 * @file maplibre_native_c.h
 * Public C API for low-level MapLibre Native bindings.
 *
 * Functions that operate on thread-affine handles validate the caller thread
 * and return MLN_STATUS_WRONG_THREAD for owner-thread mismatches. Functions
 * without an explicit owner-thread requirement may be called from any thread.
 *
 * Status-returning functions clear thread-local diagnostics on entry. After a
 * synchronous failure status is returned, read
 * mln_thread_last_error_message() on the same thread before making another C
 * API call. Asynchronous native failures are reported through runtime events.
 *
 * This header targets C23.
 */

#ifndef MAPLIBRE_NATIVE_C_H
#define MAPLIBRE_NATIVE_C_H

// NOLINTBEGIN(cppcoreguidelines-use-enum-class)
// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
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

#pragma region Common C API contract
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
typedef struct mln_resource_request_handle mln_resource_request_handle;
typedef struct mln_texture_session mln_texture_session;

/**
 * Reports the C ABI contract version.
 *
 * The value is 0 while the ABI is unstable. Stable ABI contract editions use
 * YYYYMMDD and change only when the ABI contract changes.
 */
MLN_API uint32_t mln_c_version(void) MLN_NOEXCEPT;

#pragma endregion

#pragma region Diagnostics
/**
 * Returns the last thread-local diagnostic message.
 *
 * The returned string is empty when no diagnostic is available. The pointer is
 * owned by the C API and remains valid until the next C API call on the same
 * thread that writes a thread-local diagnostic.
 */
MLN_API const char* mln_thread_last_error_message(void) MLN_NOEXCEPT;

#pragma endregion

#pragma region Logging
/** Log severity values emitted by MapLibre Native. */
typedef enum mln_log_severity : uint32_t {
  MLN_LOG_SEVERITY_INFO = 1,
  MLN_LOG_SEVERITY_WARNING = 2,
  MLN_LOG_SEVERITY_ERROR = 3,
} mln_log_severity;

/** Bitmask values for log severities dispatched asynchronously. */
typedef enum mln_log_severity_mask : uint32_t {
  MLN_LOG_SEVERITY_MASK_INFO = 1u << MLN_LOG_SEVERITY_INFO,
  MLN_LOG_SEVERITY_MASK_WARNING = 1u << MLN_LOG_SEVERITY_WARNING,
  MLN_LOG_SEVERITY_MASK_ERROR = 1u << MLN_LOG_SEVERITY_ERROR,
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

#pragma region Runtime and resources
typedef enum mln_network_status : uint32_t {
  MLN_NETWORK_STATUS_ONLINE = 1,
  MLN_NETWORK_STATUS_OFFLINE = 2,
} mln_network_status;

typedef enum mln_runtime_option_flag : uint32_t {
  MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE = 1u << 0u,
} mln_runtime_option_flag;

typedef enum mln_ambient_cache_operation : uint32_t {
  MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE = 1,
  MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE = 2,
  MLN_AMBIENT_CACHE_OPERATION_INVALIDATE = 3,
  MLN_AMBIENT_CACHE_OPERATION_CLEAR = 4,
} mln_ambient_cache_operation;

/** Runtime event types returned by mln_runtime_poll_event(). */
typedef enum mln_runtime_event_type : uint32_t {
  MLN_RUNTIME_EVENT_MAP_CAMERA_WILL_CHANGE = 1,
  MLN_RUNTIME_EVENT_MAP_CAMERA_IS_CHANGING = 2,
  MLN_RUNTIME_EVENT_MAP_CAMERA_DID_CHANGE = 3,
  MLN_RUNTIME_EVENT_MAP_STYLE_LOADED = 4,
  MLN_RUNTIME_EVENT_MAP_LOADING_STARTED = 5,
  MLN_RUNTIME_EVENT_MAP_LOADING_FINISHED = 6,
  MLN_RUNTIME_EVENT_MAP_LOADING_FAILED = 7,
  MLN_RUNTIME_EVENT_MAP_IDLE = 8,
  MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE = 9,
  MLN_RUNTIME_EVENT_MAP_RENDER_ERROR = 10,
  MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED = 11,
  MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED = 12,
  MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_STARTED = 13,
  MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_FINISHED = 14,
  MLN_RUNTIME_EVENT_MAP_RENDER_MAP_STARTED = 15,
  MLN_RUNTIME_EVENT_MAP_RENDER_MAP_FINISHED = 16,
  MLN_RUNTIME_EVENT_MAP_STYLE_IMAGE_MISSING = 17,
  MLN_RUNTIME_EVENT_MAP_TILE_ACTION = 18,
} mln_runtime_event_type;

/** Source kinds used by mln_runtime_event.source_type. */
typedef enum mln_runtime_event_source_type : uint32_t {
  MLN_RUNTIME_EVENT_SOURCE_RUNTIME = 0,
  MLN_RUNTIME_EVENT_SOURCE_MAP = 1,
} mln_runtime_event_source_type;

/** Payload kinds used by mln_runtime_event.payload_type. */
typedef enum mln_runtime_event_payload_type : uint32_t {
  MLN_RUNTIME_EVENT_PAYLOAD_NONE = 0,
  MLN_RUNTIME_EVENT_PAYLOAD_RENDER_FRAME = 1,
  MLN_RUNTIME_EVENT_PAYLOAD_RENDER_MAP = 2,
  MLN_RUNTIME_EVENT_PAYLOAD_STYLE_IMAGE_MISSING = 3,
  MLN_RUNTIME_EVENT_PAYLOAD_TILE_ACTION = 4,
} mln_runtime_event_payload_type;

/** Render modes reported by render observer events. */
typedef enum mln_render_mode : uint32_t {
  MLN_RENDER_MODE_PARTIAL = 0,
  MLN_RENDER_MODE_FULL = 1,
} mln_render_mode;

/** Tile operations reported by tile observer events. */
typedef enum mln_tile_operation : uint32_t {
  MLN_TILE_OPERATION_REQUESTED_FROM_CACHE = 0,
  MLN_TILE_OPERATION_REQUESTED_FROM_NETWORK = 1,
  MLN_TILE_OPERATION_LOAD_FROM_NETWORK = 2,
  MLN_TILE_OPERATION_LOAD_FROM_CACHE = 3,
  MLN_TILE_OPERATION_START_PARSE = 4,
  MLN_TILE_OPERATION_END_PARSE = 5,
  MLN_TILE_OPERATION_ERROR = 6,
  MLN_TILE_OPERATION_CANCELLED = 7,
  MLN_TILE_OPERATION_NULL = 8,
} mln_tile_operation;

typedef enum mln_resource_kind : uint32_t {
  MLN_RESOURCE_KIND_UNKNOWN = 0,
  MLN_RESOURCE_KIND_STYLE = 1,
  MLN_RESOURCE_KIND_SOURCE = 2,
  MLN_RESOURCE_KIND_TILE = 3,
  MLN_RESOURCE_KIND_GLYPHS = 4,
  MLN_RESOURCE_KIND_SPRITE_IMAGE = 5,
  MLN_RESOURCE_KIND_SPRITE_JSON = 6,
  MLN_RESOURCE_KIND_IMAGE = 7,
} mln_resource_kind;

typedef enum mln_resource_loading_method : uint32_t {
  MLN_RESOURCE_LOADING_METHOD_ALL = 0,
  MLN_RESOURCE_LOADING_METHOD_CACHE_ONLY = 1,
  MLN_RESOURCE_LOADING_METHOD_NETWORK_ONLY = 2,
} mln_resource_loading_method;

typedef enum mln_resource_priority : uint32_t {
  MLN_RESOURCE_PRIORITY_REGULAR = 0,
  MLN_RESOURCE_PRIORITY_LOW = 1,
} mln_resource_priority;

typedef enum mln_resource_usage : uint32_t {
  MLN_RESOURCE_USAGE_ONLINE = 0,
  MLN_RESOURCE_USAGE_OFFLINE = 1,
} mln_resource_usage;

typedef enum mln_resource_storage_policy : uint32_t {
  MLN_RESOURCE_STORAGE_POLICY_PERMANENT = 0,
  MLN_RESOURCE_STORAGE_POLICY_VOLATILE = 1,
} mln_resource_storage_policy;

typedef enum mln_resource_response_status : uint32_t {
  MLN_RESOURCE_RESPONSE_STATUS_OK = 0,
  MLN_RESOURCE_RESPONSE_STATUS_ERROR = 1,
  MLN_RESOURCE_RESPONSE_STATUS_NO_CONTENT = 2,
  MLN_RESOURCE_RESPONSE_STATUS_NOT_MODIFIED = 3,
} mln_resource_response_status;

typedef enum mln_resource_error_reason : uint32_t {
  MLN_RESOURCE_ERROR_REASON_NONE = 0,
  MLN_RESOURCE_ERROR_REASON_NOT_FOUND = 1,
  MLN_RESOURCE_ERROR_REASON_SERVER = 2,
  MLN_RESOURCE_ERROR_REASON_CONNECTION = 3,
  MLN_RESOURCE_ERROR_REASON_RATE_LIMIT = 4,
  MLN_RESOURCE_ERROR_REASON_OTHER = 5,
} mln_resource_error_reason;

typedef enum mln_resource_provider_decision : uint32_t {
  MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH = 0,
  MLN_RESOURCE_PROVIDER_DECISION_HANDLE = 1,
} mln_resource_provider_decision;

/**
 * Reads MapLibre Native's process-global network status.
 *
 * On success, out_status receives a mln_network_status value.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_status is null.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_network_status_get(uint32_t* out_status) MLN_NOEXCEPT;

/**
 * Sets MapLibre Native's process-global network status.
 *
 * MLN_NETWORK_STATUS_ONLINE allows HTTP and HTTPS requests and wakes native
 * subscribers when transitioning from offline. MLN_NETWORK_STATUS_OFFLINE makes
 * MapLibre's online source stop starting network requests until reachability
 * returns. Runtime-scoped resource configuration is unchanged.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when status is not a mln_network_status value.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_network_status_set(uint32_t status) MLN_NOEXCEPT;

typedef struct mln_runtime_options {
  uint32_t size;
  uint32_t flags;
  /** Filesystem root for asset:// URLs. Copied during runtime creation. */
  const char* asset_path;
  /** Cache database path. Copied during runtime creation. */
  const char* cache_path;
  /** Maximum ambient cache size in bytes when the matching flag is set. */
  uint64_t maximum_cache_size;
} mln_runtime_options;

/** Rendering statistics reported in MLN_RUNTIME_EVENT_PAYLOAD_RENDER_FRAME. */
typedef struct mln_rendering_stats {
  uint32_t size;
  /** Frame CPU encoding time in seconds. */
  double encoding_time;
  /** Frame CPU rendering time in seconds. */
  double rendering_time;
  /** Number of frames rendered by the native renderer. */
  int64_t frame_count;
  /** Draw calls executed during the most recent frame. */
  int64_t draw_call_count;
  /** Total draw calls executed by the native renderer. */
  int64_t total_draw_call_count;
} mln_rendering_stats;

/** Payload for MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_FINISHED. */
typedef struct mln_runtime_event_render_frame {
  uint32_t size;
  /** One of mln_render_mode. */
  uint32_t mode;
  /** Whether MapLibre needs another frame after this one. */
  bool needs_repaint;
  /** Whether symbol placement changed during this frame. */
  bool placement_changed;
  mln_rendering_stats stats;
} mln_runtime_event_render_frame;

/** Payload for MLN_RUNTIME_EVENT_MAP_RENDER_MAP_FINISHED. */
typedef struct mln_runtime_event_render_map {
  uint32_t size;
  /** One of mln_render_mode. */
  uint32_t mode;
} mln_runtime_event_render_map;

/** Payload for MLN_RUNTIME_EVENT_MAP_STYLE_IMAGE_MISSING. */
typedef struct mln_runtime_event_style_image_missing {
  uint32_t size;
  /**
   * Borrowed image ID bytes. Valid until the next poll for this runtime or
   * until the runtime is destroyed.
   */
  const char* image_id;
  /** Number of bytes in image_id, excluding the trailing null terminator. */
  size_t image_id_size;
} mln_runtime_event_style_image_missing;

/** Overscaled tile identity reported in tile observer events. */
typedef struct mln_tile_id {
  uint32_t overscaled_z;
  int32_t wrap;
  uint32_t canonical_z;
  uint32_t canonical_x;
  uint32_t canonical_y;
} mln_tile_id;

/** Payload for MLN_RUNTIME_EVENT_MAP_TILE_ACTION. */
typedef struct mln_runtime_event_tile_action {
  uint32_t size;
  /** One of mln_tile_operation. */
  uint32_t operation;
  mln_tile_id tile_id;
  /**
   * Borrowed source ID bytes. Valid until the next poll for this runtime or
   * until the runtime is destroyed.
   */
  const char* source_id;
  /** Number of bytes in source_id, excluding the trailing null terminator. */
  size_t source_id_size;
} mln_runtime_event_tile_action;

/** Event payload returned by mln_runtime_poll_event(). */
typedef struct mln_runtime_event {
  uint32_t size;
  uint32_t type;
  /** One of mln_runtime_event_source_type. */
  uint32_t source_type;
  /**
   * Source handle for this event. For map-originated events, this is an
   * mln_map*. For runtime-originated events, this is an mln_runtime*. Borrowed;
   * valid while the source handle remains live.
   */
  void* source;
  int32_t code;
  /** One of mln_runtime_event_payload_type. */
  uint32_t payload_type;
  /** Borrowed payload selected by payload_type. Null when payload_size is 0. */
  const void* payload;
  /** Number of bytes in payload. */
  size_t payload_size;
  /** Borrowed event message bytes. Null when message_size is 0. */
  const char* message;
  /** Number of bytes in message, excluding the trailing null terminator. */
  size_t message_size;
} mln_runtime_event;

typedef struct mln_resource_transform_response {
  uint32_t size;
  /** Replacement URL. Null or empty keeps the original URL. Copied on return.
   */
  const char* url;
} mln_resource_transform_response;

/**
 * Rewrites a network resource URL.
 *
 * This callback can only replace the request URL. It cannot mutate headers,
 * bodies, cache policy, or convert a request into an error.
 *
 * Callback invocations follow these rules:
 *
 * - MapLibre may invoke the callback on a worker or network thread instead of
 *   the runtime owner thread.
 * - The callback must be thread-safe, return quickly, and must not call this C
 *   API.
 * - url and out_response are borrowed for the callback duration.
 * - The C API copies out_response->url before the callback returns when a
 *   replacement URL is set.
 * - A non-OK return status is treated as no rewrite and does not fail the
 *   resource request.
 * - The callback and user_data must remain valid until no live maps or
 *   in-flight requests can invoke the transform, normally until runtime
 *   teardown.
 */
typedef mln_status (*mln_resource_transform_callback)(
  void* user_data, uint32_t kind, const char* url,
  mln_resource_transform_response* out_response
);

typedef struct mln_resource_transform {
  uint32_t size;
  mln_resource_transform_callback callback;
  void* user_data;
} mln_resource_transform;

typedef struct mln_resource_request {
  uint32_t size;
  const char* url;
  uint32_t kind;
  uint32_t loading_method;
  uint32_t priority;
  uint32_t usage;
  uint32_t storage_policy;
  bool has_range;
  uint64_t range_start;
  uint64_t range_end;
  bool has_prior_modified;
  int64_t prior_modified_unix_ms;
  bool has_prior_expires;
  int64_t prior_expires_unix_ms;
  const char* prior_etag;
  const uint8_t* prior_data;
  size_t prior_data_size;
} mln_resource_request;

typedef struct mln_resource_response {
  uint32_t size;
  uint32_t status;
  uint32_t error_reason;
  /** Response bytes. May be null only when byte_count is 0. */
  const uint8_t* bytes;
  size_t byte_count;
  const char* error_message;
  bool must_revalidate;
  bool has_modified;
  int64_t modified_unix_ms;
  bool has_expires;
  int64_t expires_unix_ms;
  const char* etag;
  bool has_retry_after;
  int64_t retry_after_unix_ms;
} mln_resource_response;

/**
 * Intercepts a network resource request.
 *
 * The callback runs synchronously on the thread that reaches the C API network
 * file source. That thread may be a MapLibre worker or network thread instead
 * of the runtime owner thread.
 *
 * Request handling follows these rules:
 *
 * - request and its pointed-to fields are borrowed for the callback duration.
 * - MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH lets native OnlineFileSource
 *   handle the request.
 * - After returning PASS_THROUGH, the provider must not retain, complete, or
 *   release the handle.
 * - MLN_RESOURCE_PROVIDER_DECISION_HANDLE lets the provider complete the
 *   request through the handle inline or later.
 * - Unknown decision values produce a provider error response. The C API
 *   releases the provided handle and does not pass the request through.
 * - The C API copies completion data, and mln_resource_request_complete() may
 *   be called from any thread.
 * - Providers must release handled request handles after they no longer need to
 *   complete or observe cancellation.
 * - The callback must be thread-safe, return quickly, and must not call map or
 *   runtime C API functions.
 * - The callback may call resource request handle functions for the provided
 *   handle.
 */
typedef uint32_t (*mln_resource_provider_callback)(
  void* user_data, const mln_resource_request* request,
  mln_resource_request_handle* handle
);

typedef struct mln_resource_provider {
  uint32_t size;
  mln_resource_provider_callback callback;
  void* user_data;
} mln_resource_provider;

/**
 * Returns runtime options initialized for this C API version.
 */
MLN_API mln_runtime_options mln_runtime_options_default(void) MLN_NOEXCEPT;

/**
 * Creates a runtime handle.
 *
 * The creating thread becomes the runtime owner thread. Each owner thread may
 * hold one live runtime.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_runtime is null, *out_runtime is not
 *   null, or options has an unsupported size or flags.
 * - MLN_STATUS_INVALID_STATE when the current thread already owns a live
 *   runtime.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_create(
  const mln_runtime_options* options, mln_runtime** out_runtime
) MLN_NOEXCEPT;

/**
 * Sets a runtime-scoped network resource provider.
 *
 * The provider must be set before any map is created from the runtime. It is
 * invoked for requests that reach the C API network file source. Built-in
 * non-network schemes such as file, asset, mbtiles, and pmtiles are handled by
 * native MainResourceLoader before this extension point. The callback and
 * user_data are stored by reference and must remain valid until the runtime is
 * destroyed.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, provider is
 *   null, provider->size is too small, or callback is null.
 * - MLN_STATUS_INVALID_STATE when runtime already owns live maps.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_set_resource_provider(
  mln_runtime* runtime, const mln_resource_provider* provider
) MLN_NOEXCEPT;

/**
 * Completes a C API resource provider request.
 *
 * This function may be called inline from the provider callback or later from
 * any thread. The C API copies all response bytes and strings before returning.
 *
 * Completion is one-shot. A second completion, completion after cancellation,
 * or completion with null arguments returns a non-OK status and does not invoke
 * MapLibre's resource callback. Malformed response contents are converted to
 * provider error responses and still consume the completion.
 *
 * Returns:
 * - MLN_STATUS_OK when the response was accepted for asynchronous delivery.
 * - MLN_STATUS_INVALID_ARGUMENT when handle or response is null.
 * - MLN_STATUS_INVALID_STATE when the request was cancelled, already completed,
 *   or can no longer accept a response.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_resource_request_complete(
  mln_resource_request_handle* handle, const mln_resource_response* response
) MLN_NOEXCEPT;

/**
 * Reports whether MapLibre has cancelled a C API resource provider request.
 *
 * This function may be called from any thread while the provider still owns the
 * handle. A cancelled request no longer wants a response; later completion is
 * ignored with MLN_STATUS_INVALID_STATE.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when handle or out_cancelled is null.
 */
MLN_API mln_status mln_resource_request_cancelled(
  const mln_resource_request_handle* handle, bool* out_cancelled
) MLN_NOEXCEPT;

/**
 * Releases the provider's reference to a resource request handle.
 *
 * Providers own a releasable handle only after returning
 * MLN_RESOURCE_PROVIDER_DECISION_HANDLE from the callback. Release the handle
 * exactly once after completing the request or deciding not to complete it.
 * Passing null is a no-op. A released handle must not be used again.
 */
MLN_API void mln_resource_request_release(
  mln_resource_request_handle* handle
) MLN_NOEXCEPT;

/**
 * Registers a runtime-scoped URL transform for network resources.
 *
 * The transform must be registered before any map is created from the runtime.
 * It is forwarded to MapLibre's OnlineFileSource, so it applies wherever native
 * OnlineFileSource applies transforms, including nested PMTiles network range
 * requests. It does not apply to file, asset, database, MBTiles, or registered
 * C API provider responses intercepted before OnlineFileSource.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, transform is
 *   null, transform->size is too small, or callback is null.
 * - MLN_STATUS_INVALID_STATE when runtime already owns live maps.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_set_resource_transform(
  mln_runtime* runtime, const mln_resource_transform* transform
) MLN_NOEXCEPT;

/**
 * Runs a MapLibre ambient cache maintenance operation for this runtime.
 *
 * When runtime options omit cache_path, this operates on MapLibre's default
 * in-memory database and its effects are not durable beyond the native database
 * lifetime. Native cache operations are asynchronous internally; this call
 * waits until MapLibre's database callback reports completion and returns the
 * resulting status.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, or operation
 *   is not a mln_ambient_cache_operation value.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_run_ambient_cache_operation(
  mln_runtime* runtime, uint32_t operation
) MLN_NOEXCEPT;

/**
 * Destroys a runtime handle.
 *
 * The runtime must no longer own live maps.
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
 * Runs one pending owner-thread task for this runtime.
 *
 * If no task is pending, the call returns MLN_STATUS_OK without doing work.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not a live runtime
 *   handle.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_run_once(mln_runtime* runtime) MLN_NOEXCEPT;

/**
 * Pops the next queued runtime event.
 *
 * On success, *out_event is reset and *out_has_event indicates whether an event
 * was available. When an event is available, *out_event receives it.
 * Map-originated events set out_event->source_type to
 * MLN_RUNTIME_EVENT_SOURCE_MAP and out_event->source to the source map.
 * Runtime-originated events set out_event->source_type to
 * MLN_RUNTIME_EVENT_SOURCE_RUNTIME.
 *
 * When an event is available, out_event->payload points to runtime-owned
 * storage containing a struct selected by out_event->payload_type, or null when
 * the payload type is MLN_RUNTIME_EVENT_PAYLOAD_NONE. String pointers inside
 * typed payloads and out_event->message remain valid until the next
 * mln_runtime_poll_event() call for the same runtime or until the runtime is
 * destroyed. Copy those bytes before then when they must outlive that window.
 * For style-image-missing and tile-action events, out_event->message contains
 * the same ID string exposed by the typed payload.
 *
 * Returns:
 * - MLN_STATUS_OK when the poll completed; out_has_event indicates whether an
 *   event was written to out_event.
 * - MLN_STATUS_INVALID_ARGUMENT when runtime is null or not live, out_event is
 *   null, out_has_event is null, or out_event->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the runtime
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_runtime_poll_event(
  mln_runtime* runtime, mln_runtime_event* out_event, bool* out_has_event
) MLN_NOEXCEPT;

#pragma endregion

#pragma region Map types, lifecycle, and style
/** Field mask values for mln_camera_options. */
typedef enum mln_camera_option_field : uint32_t {
  MLN_CAMERA_OPTION_CENTER = 1u << 0u,
  MLN_CAMERA_OPTION_ZOOM = 1u << 1u,
  MLN_CAMERA_OPTION_BEARING = 1u << 2u,
  MLN_CAMERA_OPTION_PITCH = 1u << 3u,
} mln_camera_option_field;

/** Field mask values for MapLibre axonometric rendering options. */
typedef enum mln_projection_mode_field : uint32_t {
  MLN_PROJECTION_MODE_AXONOMETRIC = 1u << 0u,
  MLN_PROJECTION_MODE_X_SKEW = 1u << 1u,
  MLN_PROJECTION_MODE_Y_SKEW = 1u << 2u,
} mln_projection_mode_field;

/** Map rendering modes used when creating a map. */
typedef enum mln_map_mode : uint32_t {
  /** Continuously updates as data arrives and map state changes. */
  MLN_MAP_MODE_CONTINUOUS = 0,
  /** Produces one-off still images of an arbitrary viewport. */
  MLN_MAP_MODE_STATIC = 1,
  /** Produces one-off still images for a single tile. */
  MLN_MAP_MODE_TILE = 2,
} mln_map_mode;

/** Options used when creating a map. */
typedef struct mln_map_options {
  uint32_t size;
  uint32_t width;
  uint32_t height;
  double scale_factor;
  /** One of mln_map_mode. Defaults to MLN_MAP_MODE_CONTINUOUS. */
  uint32_t map_mode;
} mln_map_options;

/** Camera fields used for snapshots and camera commands. */
typedef struct mln_camera_options {
  uint32_t size;
  uint32_t fields;
  double latitude;
  double longitude;
  double zoom;
  double bearing;
  double pitch;
} mln_camera_options;

/** Geographic coordinate in degrees used by map and projection APIs. */
typedef struct mln_lat_lng {
  /** Latitude in degrees. Input latitude must be finite and within [-90, 90].
   */
  double latitude;
  /** Longitude in degrees. Input longitude must be finite. */
  double longitude;
} mln_lat_lng;

/** Screen-space point in logical map pixels. */
typedef struct mln_screen_point {
  double x;
  double y;
} mln_screen_point;

/** Screen-space inset in logical map pixels. */
typedef struct mln_edge_insets {
  double top;
  double left;
  double bottom;
  double right;
} mln_edge_insets;

/**
 * Lower-level Spherical Mercator projected-meter coordinate.
 *
 * Map coordinate conversion APIs use mln_lat_lng. This type is only for
 * Mercator helper functions.
 */
typedef struct mln_projected_meters {
  /** Distance measured northward from the equator, in meters. */
  double northing;
  /** Distance measured eastward from the prime meridian, in meters. */
  double easting;
} mln_projected_meters;

/**
 * MapLibre axonometric rendering options used for snapshots and commands.
 *
 * MapLibre Native names this native type ProjectionMode. It controls the live
 * map render transform, not the geographic coordinate model.
 */
typedef struct mln_projection_mode {
  uint32_t size;
  uint32_t fields;
  /** Enables a non-perspective axonometric render transform. */
  bool axonometric;
  /** Native x-skew factor used by the axonometric transform. */
  double x_skew;
  /** Native y-skew factor used by the axonometric transform. */
  double y_skew;
} mln_projection_mode;

/**
 * Returns map options initialized for this C API version.
 */
MLN_API mln_map_options mln_map_options_default(void) MLN_NOEXCEPT;

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
 * Requests a repaint for a continuous map.
 *
 * Continuous maps also invalidate automatically when style data, resources,
 * camera, or transitions change. Ask attached render targets to process the
 * latest update when MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE is reported.
 * Repaint requests do not produce
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED or
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED events.
 *
 * Returns:
 * - MLN_STATUS_OK when the request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_INVALID_STATE when map is not in MLN_MAP_MODE_CONTINUOUS.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_request_repaint(mln_map* map) MLN_NOEXCEPT;

/**
 * Requests one still image for a static or tile map.
 *
 * Pump the runtime and poll runtime events for this map until
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED or
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED is reported. While the request is
 * pending, process render target updates for
 * MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE events from this map. Texture
 * targets do this with mln_texture_render_update(), which can return
 * MLN_STATUS_INVALID_STATE when no frame is produced for that update. Keep
 * pumping and polling in that case. After
 * MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED, acquire the frame produced by the
 * most recent successful target update.
 *
 * Returns:
 * - MLN_STATUS_OK when the request was accepted.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live.
 * - MLN_STATUS_INVALID_STATE when map is not in MLN_MAP_MODE_STATIC or
 *   MLN_MAP_MODE_TILE, or when a still-image request is already pending.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_request_still_image(mln_map* map) MLN_NOEXCEPT;

/**
 * Destroys a map handle on its owner thread.
 *
 * The map must not have an attached texture session.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not a live map handle.
 * - MLN_STATUS_INVALID_STATE when map still has an attached texture session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_destroy(mln_map* map) MLN_NOEXCEPT;

/**
 * Loads a style URL through MapLibre Native style APIs.
 *
 * This is a map command. The return status reports synchronous acceptance or
 * failure. Later native success and failure are reported through runtime
 * events.
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
 * This is a map command. The return status reports synchronous acceptance or
 * failure. Later native success and failure are reported through runtime
 * events. Malformed JSON can fail synchronously and still enqueue a
 * loading-failed event.
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

#pragma endregion

#pragma region Map camera, render transform, and coordinate conversion
/**
 * Returns empty camera options initialized for this C API version.
 */
MLN_API mln_camera_options mln_camera_options_default(void) MLN_NOEXCEPT;

/**
 * Returns empty axonometric rendering options initialized for this C API
 * version.
 */
MLN_API mln_projection_mode mln_projection_mode_default(void) MLN_NOEXCEPT;

/**
 * Copies the current camera snapshot.
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
 * Only fields indicated by camera->fields affect the map.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, camera is null,
 *   camera->size is too small, or camera->fields contains unknown bits.
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
 * Passing a null anchor uses MapLibre Native's default zoom anchor.
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
 * Copies the current axonometric rendering options.
 *
 * On success, *out_mode is overwritten. MapLibre currently reports all fields.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_mode is null,
 *   or out_mode->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_get_projection_mode(
  mln_map* map, mln_projection_mode* out_mode
) MLN_NOEXCEPT;

/**
 * Applies axonometric rendering option fields to a map.
 *
 * Only fields indicated by mode->fields affect the map. Unspecified fields keep
 * their current native values. These options mutate the live map render
 * transform and do not change coordinate conversion units or formulas.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, mode is null,
 *   mode->size is too small, mode->fields contains unknown bits, or an enabled
 *   skew value is non-finite.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_set_projection_mode(
  mln_map* map, const mln_projection_mode* mode
) MLN_NOEXCEPT;

/**
 * Converts a geographic world coordinate to a screen point for the current map.
 *
 * The output point uses logical map pixels with an origin at the top-left of
 * the map viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_point is
 *   null, or coordinate contains invalid latitude or longitude values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pixel_for_lat_lng(
  mln_map* map, mln_lat_lng coordinate, mln_screen_point* out_point
) MLN_NOEXCEPT;

/**
 * Converts a screen point to a geographic world coordinate for the current map.
 *
 * The input point uses logical map pixels with an origin at the top-left of the
 * map viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_coordinate is
 *   null, or point contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_lat_lng_for_pixel(
  mln_map* map, mln_screen_point point, mln_lat_lng* out_coordinate
) MLN_NOEXCEPT;

/**
 * Converts geographic world coordinates to screen points for the current map.
 *
 * The caller owns both arrays. On success, out_points receives coordinate_count
 * entries. coordinates and out_points may be null only when coordinate_count is
 * 0.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, a required array
 *   is null, or any coordinate contains invalid latitude or longitude values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_pixels_for_lat_lngs(
  mln_map* map, const mln_lat_lng* coordinates, size_t coordinate_count,
  mln_screen_point* out_points
) MLN_NOEXCEPT;

/**
 * Converts screen points to geographic world coordinates for the current map.
 *
 * The caller owns both arrays. On success, out_coordinates receives point_count
 * entries. points and out_coordinates may be null only when point_count is 0.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, a required array
 *   is null, or any point contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_lat_lngs_for_pixels(
  mln_map* map, const mln_screen_point* points, size_t point_count,
  mln_lat_lng* out_coordinates
) MLN_NOEXCEPT;

#pragma endregion

#pragma region Projection helpers and utilities

/**
 * Creates a standalone projection helper from the current map transform.
 *
 * The helper owns projection and camera transform state only. It does not own
 * style, resources, render targets, or runtime events. Use it to convert
 * coordinates or compute camera fitting without changing the source map.
 *
 * Creation snapshots the map's transform. Later map camera or projection
 * changes do not update the helper. The creating thread owns the helper and
 * must call projection functions on that thread.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, out_projection is
 *   null, or *out_projection is not null.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_create(
  mln_map* map, mln_map_projection** out_projection
) MLN_NOEXCEPT;

/**
 * Destroys a standalone projection helper.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_map_projection_destroy(mln_map_projection* projection) MLN_NOEXCEPT;

/**
 * Copies the current camera snapshot from a standalone projection helper.
 *
 * On success, *out_camera is overwritten.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, out_camera
 *   is null, or out_camera->size is too small.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_get_camera(
  mln_map_projection* projection, mln_camera_options* out_camera
) MLN_NOEXCEPT;

/**
 * Applies camera fields to a standalone projection helper.
 *
 * Only fields indicated by camera->fields affect the helper.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, camera is
 *   null, camera->size is too small, or camera->fields contains unknown bits.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_set_camera(
  mln_map_projection* projection, const mln_camera_options* camera
) MLN_NOEXCEPT;

/**
 * Updates a projection helper camera so coordinates are visible within padding.
 *
 * The caller owns coordinates. Use mln_map_projection_get_camera() after this
 * call to read the computed camera.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live,
 *   coordinates is null, coordinate_count is 0, padding contains negative or
 *   non-finite values, or any coordinate contains invalid latitude or longitude
 *   values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_set_visible_coordinates(
  mln_map_projection* projection, const mln_lat_lng* coordinates,
  size_t coordinate_count, mln_edge_insets padding
) MLN_NOEXCEPT;

/**
 * Converts a geographic world coordinate using a standalone projection helper.
 *
 * The output point uses logical map pixels with an origin at the top-left of
 * the helper viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live, out_point
 *   is null, or coordinate contains invalid latitude or longitude values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_pixel_for_lat_lng(
  mln_map_projection* projection, mln_lat_lng coordinate,
  mln_screen_point* out_point
) MLN_NOEXCEPT;

/**
 * Converts a screen point using a standalone projection helper.
 *
 * The input point uses logical map pixels with an origin at the top-left of the
 * helper viewport.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when projection is null or not live,
 *   out_coordinate is null, or point contains non-finite values.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the projection
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_map_projection_lat_lng_for_pixel(
  mln_map_projection* projection, mln_screen_point point,
  mln_lat_lng* out_coordinate
) MLN_NOEXCEPT;

/**
 * Converts a geographic coordinate to spherical Mercator projected meters.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_meters is null or coordinate contains
 *   invalid latitude or longitude values.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_projected_meters_for_lat_lng(
  mln_lat_lng coordinate, mln_projected_meters* out_meters
) MLN_NOEXCEPT;

/**
 * Converts spherical Mercator projected meters to a geographic coordinate.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when out_coordinate is null or meters contains
 *   non-finite values.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_lat_lng_for_projected_meters(
  mln_projected_meters meters, mln_lat_lng* out_coordinate
) MLN_NOEXCEPT;

#pragma endregion

#pragma region Texture sessions

/** MapLibre-owned texture session attachment options. */
typedef struct mln_owned_texture_descriptor {
  uint32_t size;
  /** Logical map width in UI pixels. */
  uint32_t width;
  /** Logical map height in UI pixels. */
  uint32_t height;
  /** UI-to-device pixel scale. Must be positive and finite. */
  double scale_factor;
} mln_owned_texture_descriptor;

/** Backend that produced a shared texture frame. */
typedef enum mln_texture_backend : uint32_t {
  MLN_TEXTURE_BACKEND_NONE = 0,
  MLN_TEXTURE_BACKEND_METAL = 1,
  MLN_TEXTURE_BACKEND_VULKAN = 2,
} mln_texture_backend;

/** Native or exported handle kind carried by a shared texture frame. */
typedef enum mln_shared_texture_handle_type : uint32_t {
  MLN_SHARED_TEXTURE_HANDLE_NONE = 0,
  /** id<MTLTexture> / MTL::Texture*. */
  MLN_SHARED_TEXTURE_HANDLE_METAL_TEXTURE = 1,
  /** MTLSharedTextureHandle / MTL::SharedTextureHandle*. */
  MLN_SHARED_TEXTURE_HANDLE_METAL_SHARED_TEXTURE_HANDLE = 2,
  /** VkImage. */
  MLN_SHARED_TEXTURE_HANDLE_VULKAN_IMAGE = 3,
} mln_shared_texture_handle_type;

/** Shared texture session attachment options. */
typedef struct mln_shared_texture_descriptor {
  uint32_t size;
  /** Logical map width in UI pixels. */
  uint32_t width;
  /** Logical map height in UI pixels. */
  uint32_t height;
  /** UI-to-device pixel scale. Must be positive and finite. */
  double scale_factor;
  /**
   * Required exported handle kind. NONE means the build's native texture
   * handle.
   */
  uint32_t required_handle_type;
  /**
   * Optional borrowed producer device. On Metal this is id<MTLDevice> /
   * MTL::Device*. Null lets the wrapper choose the system default device.
   */
  void* device;
} mln_shared_texture_descriptor;

/** Metal texture session attachment options. */
typedef struct mln_metal_texture_descriptor {
  uint32_t size;
  /** Logical map width in UI pixels. */
  uint32_t width;
  /** Logical map height in UI pixels. */
  uint32_t height;
  /** UI-to-device pixel scale. Must be positive and finite. */
  double scale_factor;
  /** Borrowed id<MTLDevice> / MTL::Device*. Required. */
  void* device;
} mln_metal_texture_descriptor;

/** Metal texture frame acquired from a texture session. */
typedef struct mln_metal_texture_frame {
  uint32_t size;
  /** Session generation that produced this frame. */
  uint64_t generation;
  /** Physical Metal texture width in device pixels. */
  uint32_t width;
  /** Physical Metal texture height in device pixels. */
  uint32_t height;
  /** UI-to-device pixel scale used for this frame. */
  double scale_factor;
  /** Opaque frame identity used to reject stale releases. */
  uint64_t frame_id;
  /** Borrowed id<MTLTexture> / MTL::Texture*. Valid until frame release. */
  void* texture;
  /** Borrowed id<MTLDevice> / MTL::Device*. Valid until frame release. */
  void* device;
  /** Backend-native pixel format value. Metal uses MTLPixelFormat. */
  uint64_t pixel_format;
} mln_metal_texture_frame;

/** Vulkan texture session attachment options. */
typedef struct mln_vulkan_texture_descriptor {
  uint32_t size;
  /** Logical map width in UI pixels. */
  uint32_t width;
  /** Logical map height in UI pixels. */
  uint32_t height;
  /** UI-to-device pixel scale. Must be positive and finite. */
  double scale_factor;
  /** Borrowed VkInstance. Required. */
  void* instance;
  /** Borrowed VkPhysicalDevice. Required. */
  void* physical_device;
  /** Borrowed VkDevice. Required. */
  void* device;
  /** Borrowed graphics VkQueue. Required. */
  void* graphics_queue;
  /** Queue family index for graphics_queue. Must support graphics commands. */
  uint32_t graphics_queue_family_index;
} mln_vulkan_texture_descriptor;

/** Vulkan texture frame acquired from a texture session. */
typedef struct mln_vulkan_texture_frame {
  uint32_t size;
  /** Session generation that produced this frame. */
  uint64_t generation;
  /** Physical Vulkan image width in device pixels. */
  uint32_t width;
  /** Physical Vulkan image height in device pixels. */
  uint32_t height;
  /** UI-to-device pixel scale used for this frame. */
  double scale_factor;
  /** Opaque frame identity used to reject stale releases. */
  uint64_t frame_id;
  /** Borrowed VkImage. Valid until frame release. */
  void* image;
  /** Borrowed VkImageView. Valid until frame release. */
  void* image_view;
  /** Borrowed VkDevice. Valid until frame release. */
  void* device;
  /** Backend-native VkFormat value. */
  uint32_t format;
  /** Backend-native VkImageLayout value; Vulkan frames are host-sampleable. */
  uint32_t layout;
} mln_vulkan_texture_frame;

/** Shared texture frame acquired from a texture session. */
typedef struct mln_shared_texture_frame {
  uint32_t size;
  /** Session generation that produced this frame. */
  uint64_t generation;
  /** Physical texture/image width in device pixels. */
  uint32_t width;
  /** Physical texture/image height in device pixels. */
  uint32_t height;
  /** UI-to-device pixel scale used for this frame. */
  double scale_factor;
  /** Opaque frame identity used to reject stale releases. */
  uint64_t frame_id;
  /** mln_texture_backend value for the producer backend. */
  uint32_t producer_backend;
  /** mln_shared_texture_handle_type value for native_handle. */
  uint32_t native_handle_type;
  /** Borrowed native texture/image handle. Valid until frame release. */
  void* native_handle;
  /** Optional borrowed native view handle, such as VkImageView. */
  void* native_view;
  /** Borrowed native device handle. Valid until frame release. */
  void* native_device;
  /** mln_shared_texture_handle_type value for export_handle, or NONE. */
  uint32_t export_handle_type;
  /** Optional borrowed export handle. Valid until frame release. */
  void* export_handle;
  /** Backend-native pixel format value. */
  uint64_t format;
  /** Backend-native layout/state value. Zero when not applicable. */
  uint32_t layout;
  /** Backend-native plane index. Zero for single-plane textures. */
  uint32_t plane;
} mln_shared_texture_frame;

/** CPU image readback metadata for a texture session frame. */
typedef struct mln_texture_image_info {
  size_t size;
  /** Physical image width in device pixels. */
  uint32_t width;
  /** Physical image height in device pixels. */
  uint32_t height;
  /** Bytes per image row. */
  uint32_t stride;
  /** Required or filled byte length. */
  size_t byte_length;
} mln_texture_image_info;

/**
 * Returns MapLibre-owned texture descriptor values initialized for this C API
 * version.
 */
MLN_API mln_owned_texture_descriptor
mln_owned_texture_descriptor_default(void) MLN_NOEXCEPT;

/**
 * Returns shared texture descriptor values initialized for this C API version.
 */
MLN_API mln_shared_texture_descriptor
mln_shared_texture_descriptor_default(void) MLN_NOEXCEPT;

/**
 * Returns Metal texture descriptor values initialized for this C API version.
 */
MLN_API mln_metal_texture_descriptor
mln_metal_texture_descriptor_default(void) MLN_NOEXCEPT;

/**
 * Returns Vulkan texture descriptor values initialized for this C API version.
 */
MLN_API mln_vulkan_texture_descriptor
mln_vulkan_texture_descriptor_default(void) MLN_NOEXCEPT;

/**
 * Returns texture image info values initialized for this C API version.
 */
MLN_API mln_texture_image_info
mln_texture_image_info_default(void) MLN_NOEXCEPT;

/**
 * Attaches a MapLibre-owned offscreen texture render target to a map.
 *
 * The map may have at most one live texture session. The session and every
 * texture-session call are owner-thread affine to the map owner thread. The
 * wrapper creates a backend-native offscreen target using MapLibre Native's
 * default headless backend for this build. On success, *out_texture receives a
 * handle the caller destroys with mln_texture_destroy().
 *
 * This target is intended for still-image and CPU-readback workflows. Use the
 * backend-specific host-provided texture attach functions when a UI framework
 * needs to sample the rendered texture on its own graphics device.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, descriptor is
 *   null or invalid, out_texture is null, or *out_texture is not null.
 * - MLN_STATUS_INVALID_STATE when the map already has a texture session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_owned_texture_attach(
  mln_map* map, const mln_owned_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) MLN_NOEXCEPT;

/**
 * Attaches a Metal offscreen texture render target to a map.
 *
 * The map may have at most one live texture session. The session and every
 * texture-session call are owner-thread affine to the map owner thread. The
 * wrapper renders into a wrapper-owned texture created on the caller-provided
 * Metal device. On success, *out_texture receives a handle the caller destroys
 * with mln_texture_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, descriptor is
 *   null or invalid, out_texture is null, or *out_texture is not null.
 * - MLN_STATUS_INVALID_STATE when the map already has a texture session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_UNSUPPORTED when Metal texture sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_metal_texture_attach(
  mln_map* map, const mln_metal_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) MLN_NOEXCEPT;

/**
 * Attaches a Vulkan offscreen texture render target to a map.
 *
 * The map may have at most one live texture session. The session and every
 * texture-session call are owner-thread affine to the map owner thread. The
 * wrapper renders into a wrapper-owned image created on the caller-provided
 * Vulkan device. On success, *out_texture receives a handle the caller destroys
 * with mln_texture_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, descriptor is
 *   null or invalid, out_texture is null, or *out_texture is not null.
 * - MLN_STATUS_INVALID_STATE when the map already has a texture session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_UNSUPPORTED when Vulkan texture sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_vulkan_texture_attach(
  mln_map* map, const mln_vulkan_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) MLN_NOEXCEPT;

/**
 * Attaches a shared offscreen texture render target to a map.
 *
 * The map may have at most one live texture session. The session and every
 * texture-session call are owner-thread affine to the map owner thread. The
 * wrapper renders into a texture/image that can be acquired through
 * mln_texture_acquire_shared_frame().
 *
 * Exportability is requested through descriptor->required_handle_type because
 * some backends must choose exportable allocation paths up front. A value of
 * MLN_SHARED_TEXTURE_HANDLE_NONE requests the build's native texture handle.
 * Unsupported export handle requests fail during attach.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, descriptor is
 *   null or invalid, out_texture is null, or *out_texture is not null.
 * - MLN_STATUS_INVALID_STATE when the map already has a texture session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_UNSUPPORTED when shared texture sessions or the requested handle
 *   kind are not supported by this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_shared_texture_attach(
  mln_map* map, const mln_shared_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) MLN_NOEXCEPT;

/**
 * Resizes a texture session and advances its generation.
 *
 * Width and height are logical map dimensions. The session renders into a
 * physical backend texture/image sized from the logical dimensions and
 * scale_factor. Resize clears the previously rendered frame.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, dimensions
 *   are zero, or scale_factor is not positive and finite.
 * - MLN_STATUS_INVALID_STATE when the session is detached or a frame is
 *   currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) MLN_NOEXCEPT;

/**
 * Processes the latest map render update for an offscreen texture session.
 *
 * When the update produces a frame, this renders into the texture and makes a
 * frame available to the backend-specific acquire function for the same session
 * generation.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live.
 * - MLN_STATUS_INVALID_STATE when no render update is available, the current
 *   update does not produce a frame, the session is detached, or a frame is
 *   currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_texture_render_update(mln_texture_session* texture) MLN_NOEXCEPT;

/**
 * Reads the most recently rendered texture frame into caller-owned storage.
 *
 * The copied image is premultiplied RGBA8 in physical pixels. The function
 * fills out_info with the required byte length and image layout metadata. When
 * out_data is null or out_data_capacity is too small, out_info is still filled
 * and the function returns MLN_STATUS_INVALID_ARGUMENT.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, out_info is
 *   null, out_info->size is too small, out_data is null, or out_data_capacity
 *   is too small.
 * - MLN_STATUS_INVALID_STATE when no rendered frame is available, the session
 *   is detached, a frame is currently acquired, or readback produces no image.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_texture_read_premultiplied_rgba8(
  mln_texture_session* texture, uint8_t* out_data, size_t out_data_capacity,
  mln_texture_image_info* out_info
) MLN_NOEXCEPT;

/**
 * Acquires the most recently rendered Metal texture frame.
 *
 * The returned texture and device pointers are borrowed and remain valid only
 * until mln_metal_texture_release_frame() is called for the same frame. While
 * acquired, resize, mln_texture_render_update(), detach, destroy, and a second
 * acquire return MLN_STATUS_INVALID_STATE.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, out_frame is
 *   null, or out_frame->size is too small.
 * - MLN_STATUS_INVALID_STATE when no rendered frame is available, the session
 *   is detached, or another frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_UNSUPPORTED when Metal texture sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) MLN_NOEXCEPT;

/**
 * Acquires the most recently rendered Vulkan texture frame.
 *
 * The returned image, image view, and device pointers are borrowed and remain
 * valid only until mln_vulkan_texture_release_frame() is called for the same
 * frame. While acquired, resize, mln_texture_render_update(), detach, destroy,
 * and a second acquire return MLN_STATUS_INVALID_STATE.
 *
 * On success, the image has been rendered and made available in the returned
 * layout for shader sampling through the returned image view until release.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, out_frame is
 *   null, or out_frame->size is too small.
 * - MLN_STATUS_INVALID_STATE when no rendered frame is available, the session
 *   is detached, or another frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_UNSUPPORTED when Vulkan texture sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_vulkan_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_texture_frame* out_frame
) MLN_NOEXCEPT;

/**
 * Acquires the most recently rendered shared texture frame.
 *
 * The returned native and export handles are borrowed and remain valid only
 * until mln_texture_release_shared_frame() is called for the same frame. While
 * acquired, resize, mln_texture_render_update(), detach, destroy, and a second
 * acquire return MLN_STATUS_INVALID_STATE.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, out_frame is
 *   null, or out_frame->size is too small.
 * - MLN_STATUS_INVALID_STATE when no rendered frame is available, the session
 *   is detached, or another frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_UNSUPPORTED when the session cannot expose a shared texture
 *   frame or the requested handle kind is not supported.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_texture_acquire_shared_frame(
  mln_texture_session* texture, mln_shared_texture_frame* out_frame
) MLN_NOEXCEPT;

/**
 * Releases a previously acquired Metal texture frame.
 *
 * The frame must be the active acquired frame for this session. A successful
 * release ends the borrow of frame->texture and frame->device.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, frame is
 *   null, frame->size is too small, or the frame generation or frame_id does
 *   not match the active acquired frame.
 * - MLN_STATUS_INVALID_STATE when no frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_UNSUPPORTED when Metal texture sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) MLN_NOEXCEPT;

/**
 * Releases a previously acquired Vulkan texture frame.
 *
 * The frame must be the active acquired frame for this session. A successful
 * release ends the borrow of frame->image, frame->image_view, and
 * frame->device.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, frame is
 *   null, frame->size is too small, or the frame generation or frame_id does
 *   not match the active acquired frame.
 * - MLN_STATUS_INVALID_STATE when no frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_UNSUPPORTED when Vulkan texture sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_vulkan_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_texture_frame* frame
) MLN_NOEXCEPT;

/**
 * Releases a previously acquired shared texture frame.
 *
 * The frame must be the active acquired shared frame for this session. A
 * successful release ends the borrow of native and export handles.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live, frame is
 *   null, frame->size is too small, or the frame generation or frame_id does
 *   not match the active acquired frame.
 * - MLN_STATUS_INVALID_STATE when no shared frame is currently acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_texture_release_shared_frame(
  mln_texture_session* texture, const mln_shared_texture_frame* frame
) MLN_NOEXCEPT;

/**
 * Detaches backend-bound resources from the map while keeping the session
 * handle live for destruction.
 *
 * Detach advances the session generation. After detach, resize, render, and
 * acquire operations return MLN_STATUS_INVALID_STATE.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live.
 * - MLN_STATUS_INVALID_STATE when already detached or a frame is acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_texture_detach(mln_texture_session* texture) MLN_NOEXCEPT;

/**
 * Destroys a texture session handle.
 *
 * If the session is still attached, this function detaches it first.
 * Destruction is rejected while a frame is acquired so borrowed texture
 * pointers cannot outlive their session.
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when texture is null or not live.
 * - MLN_STATUS_INVALID_STATE when a frame is acquired.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the session
 *   owner thread.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status
mln_texture_destroy(mln_texture_session* texture) MLN_NOEXCEPT;

#pragma endregion

#ifdef __cplusplus
}
#endif

// NOLINTEND(readability-uppercase-literal-suffix)
// NOLINTEND(modernize-use-using)
// NOLINTEND(modernize-use-trailing-return-type)
// NOLINTEND(modernize-deprecated-headers)
// NOLINTEND(cppcoreguidelines-pro-type-member-init)
// NOLINTEND(cppcoreguidelines-use-enum-class)

#endif
