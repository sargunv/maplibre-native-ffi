/**
 * @file maplibre_native_c/surface.h
 * Public C API declarations for surface render targets.
 */

#ifndef MAPLIBRE_NATIVE_C_SURFACE_H
#define MAPLIBRE_NATIVE_C_SURFACE_H

#include <stdint.h>

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Metal native surface session attachment options. */
typedef struct mln_metal_surface_descriptor {
  uint32_t size;
  /** Logical map width in UI pixels. */
  uint32_t width;
  /** Logical map height in UI pixels. */
  uint32_t height;
  /** UI-to-device pixel scale. Must be positive and finite. */
  double scale_factor;
  /** CAMetalLayer* / CA::MetalLayer* retained by the session. Required. */
  void* layer;
  /** Optional id<MTLDevice> / MTL::Device* retained by the session. */
  void* device;
} mln_metal_surface_descriptor;

/** Vulkan native surface session attachment options. */
typedef struct mln_vulkan_surface_descriptor {
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
  /** Borrowed VkDevice with VK_KHR_swapchain enabled. Required. */
  void* device;
  /** Borrowed graphics VkQueue. Required. */
  void* graphics_queue;
  /**
   * Queue family index for graphics_queue. Must support graphics commands and
   * presentation to surface.
   */
  uint32_t graphics_queue_family_index;
  /** Borrowed VkSurfaceKHR. Required. */
  void* surface;
} mln_vulkan_surface_descriptor;

/**
 * Returns Metal surface descriptor defaults for this C API version.
 */
MLN_API mln_metal_surface_descriptor
mln_metal_surface_descriptor_default(void) MLN_NOEXCEPT;

/**
 * Returns Vulkan surface descriptor defaults for this C API version.
 */
MLN_API mln_vulkan_surface_descriptor
mln_vulkan_surface_descriptor_default(void) MLN_NOEXCEPT;

/**
 * Attaches a Metal native surface render target to a map.
 *
 * The map may have at most one live render session. The session and
 * every surface-session call are owner-thread affine to the map owner thread.
 * The session retains descriptor->layer and optional descriptor->device. It
 * renders into the layer and presents through it. On success, *out_session
 * receives a handle the caller destroys with mln_render_session_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, descriptor is
 *   null or invalid, out_session is null, or *out_session is not null.
 * - MLN_STATUS_INVALID_STATE when the map already has a render session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_UNSUPPORTED when Metal surface sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_metal_surface_attach(
  mln_map* map, const mln_metal_surface_descriptor* descriptor,
  mln_render_session** out_session
) MLN_NOEXCEPT;

/**
 * Attaches a Vulkan native surface render target to a map.
 *
 * The map may have at most one live render session. The session and
 * every surface-session call are owner-thread affine to the map owner thread.
 * The session renders to descriptor->surface and presents through it. Vulkan
 * handles are borrowed and must remain valid until the session is detached or
 * destroyed. On success, *out_session receives a handle the caller destroys
 * with mln_render_session_destroy().
 *
 * Returns:
 * - MLN_STATUS_OK on success.
 * - MLN_STATUS_INVALID_ARGUMENT when map is null or not live, descriptor is
 *   null or invalid, out_session is null, or *out_session is not null.
 * - MLN_STATUS_INVALID_STATE when the map already has a render session.
 * - MLN_STATUS_WRONG_THREAD when called from a thread other than the map owner
 *   thread.
 * - MLN_STATUS_UNSUPPORTED when Vulkan surface sessions are not supported by
 *   this build.
 * - MLN_STATUS_NATIVE_ERROR when an internal exception is converted to status.
 */
MLN_API mln_status mln_vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_render_session** out_session
) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_SURFACE_H
