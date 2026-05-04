/**
 * @file maplibre_native_c/runtime.h
 * Public C API declarations for runtime, resources, and events.
 */

#ifndef MAPLIBRE_NATIVE_C_RUNTIME_H
#define MAPLIBRE_NATIVE_C_RUNTIME_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mln_network_status : uint32_t {
  MLN_NETWORK_STATUS_ONLINE = 1,
  MLN_NETWORK_STATUS_OFFLINE = 2,
} mln_network_status;

typedef enum mln_runtime_option_flag : uint32_t {
  MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE = 1U << 0U,
} mln_runtime_option_flag;

typedef enum mln_ambient_cache_operation : uint32_t {
  MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE = 1,
  MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE = 2,
  MLN_AMBIENT_CACHE_OPERATION_INVALIDATE = 3,
  MLN_AMBIENT_CACHE_OPERATION_CLEAR = 4,
} mln_ambient_cache_operation;

typedef int64_t mln_offline_region_id;

typedef enum mln_offline_region_definition_type : uint32_t {
  MLN_OFFLINE_REGION_DEFINITION_TILE_PYRAMID = 1,
  MLN_OFFLINE_REGION_DEFINITION_GEOMETRY = 2,
} mln_offline_region_definition_type;

typedef enum mln_offline_region_download_state : uint32_t {
  MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE = 0,
  MLN_OFFLINE_REGION_DOWNLOAD_ACTIVE = 1,
} mln_offline_region_download_state;

/** Offline region status snapshot. */
typedef struct mln_offline_region_status {
  uint32_t size;
  /** One of mln_offline_region_download_state. */
  uint32_t download_state;
  uint64_t completed_resource_count;
  uint64_t completed_resource_size;
  uint64_t completed_tile_count;
  uint64_t required_tile_count;
  uint64_t completed_tile_size;
  uint64_t required_resource_count;
  bool required_resource_count_is_precise;
  bool complete;
} mln_offline_region_status;

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
  MLN_RUNTIME_EVENT_OFFLINE_REGION_STATUS_CHANGED = 19,
  MLN_RUNTIME_EVENT_OFFLINE_REGION_RESPONSE_ERROR = 20,
  MLN_RUNTIME_EVENT_OFFLINE_REGION_TILE_COUNT_LIMIT_EXCEEDED = 21,
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
  MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_STATUS = 5,
  MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_RESPONSE_ERROR = 6,
  MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_TILE_COUNT_LIMIT = 7,
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

/** Payload for MLN_RUNTIME_EVENT_OFFLINE_REGION_STATUS_CHANGED. */
typedef struct mln_runtime_event_offline_region_status {
  uint32_t size;
  mln_offline_region_id region_id;
  mln_offline_region_status status;
} mln_runtime_event_offline_region_status;

/** Payload for MLN_RUNTIME_EVENT_OFFLINE_REGION_RESPONSE_ERROR. */
typedef struct mln_runtime_event_offline_region_response_error {
  uint32_t size;
  mln_offline_region_id region_id;
  /** One of mln_resource_error_reason. */
  uint32_t reason;
} mln_runtime_event_offline_region_response_error;

/** Payload for MLN_RUNTIME_EVENT_OFFLINE_REGION_TILE_COUNT_LIMIT_EXCEEDED. */
typedef struct mln_runtime_event_offline_region_tile_count_limit {
  uint32_t size;
  mln_offline_region_id region_id;
  uint64_t limit;
} mln_runtime_event_offline_region_tile_count_limit;

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

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_RUNTIME_H
