# Render Session Refactor Notes

This note records the current render backend model and the API direction for the
render session refactor. It is written for contributors changing the C ABI,
render target ownership, or backend integration.

## Development Constraints

The project keeps map state separate from render targets. `mln_map` owns style,
camera, observer events, and invalidation state, while render target sessions
own backend-bound resources (`docs/development.md:157-168`). Runtime, map, and
texture-session calls that touch thread-affine state validate the owner thread
(`docs/development.md:101-113`).

The C API should expose low-level render target primitives, not full platform
view integrations or convenience snapshot APIs (`docs/development.md:3-18`).
Ownership, synchronization, borrowed pointer lifetimes, generation or
stale-frame behavior, and teardown rules belong in the render target API
contract (`docs/development.md:82-100`, `docs/development.md:157-168`).

## Upstream Backend Model

MapLibre Native separates the graphics backend from the render target.

| Concept                 | C++ type                              | Responsibility                                                                                      |
| ----------------------- | ------------------------------------- | --------------------------------------------------------------------------------------------------- |
| Graphics backend        | `mbgl::gfx::RendererBackend`          | Owns or borrows API state such as Metal, Vulkan, OpenGL, or WebGPU context resources.               |
| Render target           | `mbgl::gfx::Renderable`               | Represents the target size and the default place the renderer draws.                                |
| Backend target resource | backend-specific `RenderableResource` | Holds the framebuffer, swapchain image, Metal drawable, offscreen texture, or similar API resource. |

`RendererBackend` exposes `getDefaultRenderable()` and creates the graphics
context
(`third_party/maplibre-native/include/mbgl/gfx/renderer_backend.hpp:27-58`).
`Renderable` stores size and a `RenderableResource`
(`third_party/maplibre-native/include/mbgl/gfx/renderable.hpp:23-56`). Vulkan
specializes this with `SurfaceRenderableResource`, which can either create a
platform surface or use optional color textures when no surface exists
(`third_party/maplibre-native/include/mbgl/vulkan/renderable_resource.hpp:35-99`).

The renderer itself renders to whatever `backend.getDefaultRenderable()`
returns. During a frame it waits for the renderable, creates render passes
against the default renderable, then presents it
(`third_party/maplibre-native/src/mbgl/renderer/renderer_impl.cpp:167-169`,
`third_party/maplibre-native/src/mbgl/renderer/renderer_impl.cpp:365-370`,
`third_party/maplibre-native/src/mbgl/renderer/renderer_impl.cpp:449-455`).
Backend command encoders implement present by calling the renderable resource's
`swap()` method for Vulkan, Metal, and OpenGL
(`third_party/maplibre-native/src/mbgl/vulkan/command_encoder.cpp:27-29`,
`third_party/maplibre-native/src/mbgl/mtl/command_encoder.cpp:29-31`,
`third_party/maplibre-native/src/mbgl/gl/command_encoder.cpp:28-30`).

This model is the right mental model for the C API: the map is not the renderer
target, and a render session is the C API's ownership and lifetime wrapper
around one concrete render target.

## MapLibre-Owned Headless Backends

MapLibre Native can set up graphics API state when it owns a headless target.

Vulkan `mbgl::vulkan::HeadlessBackend` subclasses both `vulkan::RendererBackend`
and `gfx::HeadlessBackend`, then calls `init()` in its constructor
(`third_party/maplibre-native/platform/default/include/mbgl/vulkan/headless_backend.hpp:13-22`,
`third_party/maplibre-native/platform/default/src/mbgl/vulkan/headless_backend.cpp:34-40`).
The shared Vulkan `RendererBackend::init()` path initializes the dynamic loader,
instance, debug hooks, surface hook, device, allocator, swapchain resources, and
command pool
(`third_party/maplibre-native/src/mbgl/vulkan/renderer_backend.cpp:380-403`).
The default Vulkan device path chooses a physical device, creates a logical
device and queues, then initializes swapchain/offscreen resources and a command
pool
(`third_party/maplibre-native/src/mbgl/vulkan/renderer_backend.cpp:508-631`).

Vulkan headless readback is implemented by waiting for the frame, copying the
rendered image into a read texture, and reading image bytes
(`third_party/maplibre-native/platform/default/src/mbgl/vulkan/headless_backend.cpp:77-97`).
That path is useful for server/static rendering because callers do not need to
write Vulkan setup code.

Metal `mbgl::mtl::RendererBackend` creates a system default `MTLDevice` and a
command queue in its constructor
(`third_party/maplibre-native/src/mbgl/mtl/renderer_backend.cpp:40-44`). Metal
`HeadlessBackend` owns an offscreen texture resource and exposes both
`readStillImage()` and `getMetalTexture()`
(`third_party/maplibre-native/platform/default/src/mbgl/mtl/headless_backend.cpp:56-61`,
`third_party/maplibre-native/platform/default/src/mbgl/mtl/headless_backend.cpp:84-100`).
The `getMetalTexture()` exposure was added upstream for this project, but this
repository no longer uses that upstream class for current Metal texture
sessions.

OpenGL headless creates a backend-owned context/renderable resource and reads
from the framebuffer
(`third_party/maplibre-native/platform/default/src/mbgl/gl/headless_backend.cpp:44-49`,
`third_party/maplibre-native/platform/default/src/mbgl/gl/headless_backend.cpp:96-119`).

## Current Wrapper Texture Sessions

The current public texture descriptors are host-GPU oriented. Metal texture
sessions require a borrowed `id<MTLDevice>` / `MTL::Device*`
(`include/maplibre_native_c.h:1389-1400`). Vulkan texture sessions require a
borrowed `VkInstance`, `VkPhysicalDevice`, `VkDevice`, graphics `VkQueue`, and
queue family index (`include/maplibre_native_c.h:1423-1442`). The attach
comments state that the wrapper renders into resources created on the
caller-provided backend device (`include/maplibre_native_c.h:1481-1529`).

Metal sessions create this repository's custom `MetalTextureBackend`, not
upstream `mbgl::mtl::HeadlessBackend`
(`src/render/metal/metal_texture_session.mm:254-257`). The custom backend stores
the host device and creates a command queue from it
(`src/render/metal/metal_texture_backend.mm:77-84`). It exposes the rendered
offscreen texture through `MetalTextureBackend::metal_texture()`
(`src/render/metal/metal_texture_backend.mm:117-120`).

Vulkan sessions create this repository's custom `VulkanTextureBackend` from the
host descriptor (`src/render/vulkan/vulkan_texture_session.cpp:307-309`). That
backend wraps the caller's instance and device as a shared context rather than
creating its own (`src/render/vulkan/vulkan_texture_backend.cpp:225-233`,
`src/render/vulkan/vulkan_texture_backend.cpp:244-258`,
`src/render/vulkan/vulkan_texture_backend.cpp:309-355`). It returns the current
rendered `VkImage`, `VkImageView`, `VkDevice`, and format for same-backend
sampling (`src/render/vulkan/vulkan_texture_backend.cpp:299-306`). Its
`readStillImage()` is currently a stub returning an empty image
(`src/render/vulkan/vulkan_texture_backend.cpp:269-270`).

This means the current texture sessions are not MapLibre-owned headless
sessions. They are host-provided GPU sessions designed for UI composition where
the host UI framework already owns the graphics device or context.

## Texture Usability Without CPU Readback

MapLibre-owned headless output is reliable as a CPU readback source. It is not a
general zero-copy UI sharing contract.

Metal is the narrow exception. A MapLibre-owned Metal headless texture can be
usable in-process when the consumer uses the same `MTLDevice` and accepts
MapLibre's lifetime and synchronization contract. That is not the same as a
general UI integration API because the host did not choose the device.

Vulkan headless does not expose a public exportable image contract, and the
default offscreen images are not allocated as external-memory resources. General
same-process, cross-process, or cross-API sharing needs explicit allocation and
API contracts for external memory handles, adapter/device compatibility, image
format, image layout/state, and GPU synchronization. The default headless
backend does not provide those exported handles or synchronization primitives.

For UI apps, host-provided GPU sessions are the right default. A UI framework or
application usually already owns a GPU context, and MapLibre should render into
resources compatible with that context. For server or CLI still-image rendering,
MapLibre-owned headless sessions plus CPU readback are the right default because
the caller should not need Vulkan, Metal, or OpenGL boilerplate.

## API Direction From First Principles

The ideal API should treat backend ownership and render target kind as separate
axes.

| Host need                      | GPU owner                                  | Render target                     | Output                             |
| ------------------------------ | ------------------------------------------ | --------------------------------- | ---------------------------------- |
| Server still image             | MapLibre                                   | owned offscreen texture           | premultiplied RGBA8 readback       |
| UI, same graphics API          | Host/UI framework                          | host-compatible offscreen texture | backend-native texture/image frame |
| UI, different graphics API     | Host/UI framework plus export contract     | exportable offscreen texture      | shared GPU handle plus GPU sync    |
| Embedded native surface/window | MapLibre or host-provided platform surface | surface session                   | direct present                     |

One internal render target attachment model should enforce the map invariant.
`mln_map` currently stores a single `mln_texture_session*`
(`src/map/map.cpp:681-690`). Its attach/detach helpers enforce one attached
texture session (`src/map/map.cpp:963-997`). The refactor should generalize this
to one attached render target session so texture sessions and surface sessions
cannot be attached at the same time.

The public ABI can keep target-specific attach and access functions while
sharing one lifecycle model. A working shape is:

```c
typedef struct mln_render_session mln_render_session;

MLN_API mln_status mln_render_session_resize(
  mln_render_session* session,
  uint32_t width,
  uint32_t height,
  double scale_factor
) MLN_NOEXCEPT;

MLN_API mln_status
mln_render_session_render_update(mln_render_session* session) MLN_NOEXCEPT;

MLN_API mln_status
mln_render_session_detach(mln_render_session* session) MLN_NOEXCEPT;

MLN_API mln_status
mln_render_session_destroy(mln_render_session* session) MLN_NOEXCEPT;
```

Attach families should describe both target kind and GPU ownership:

```c
MLN_API mln_status mln_owned_texture_attach(
  mln_map* map,
  const mln_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) MLN_NOEXCEPT;

MLN_API mln_status mln_metal_texture_attach(
  mln_map* map,
  const mln_metal_texture_descriptor* descriptor,
  mln_render_session** out_session
) MLN_NOEXCEPT;

MLN_API mln_status mln_vulkan_texture_attach(
  mln_map* map,
  const mln_vulkan_texture_descriptor* descriptor,
  mln_render_session** out_session
) MLN_NOEXCEPT;

MLN_API mln_status mln_metal_surface_attach(...);
MLN_API mln_status mln_vulkan_surface_attach(...);
MLN_API mln_status mln_opengl_surface_attach(...);
```

Texture access should remain target-specific:

```c
MLN_API mln_status mln_metal_texture_acquire_frame(
  mln_render_session* session,
  mln_metal_texture_frame* out_frame
) MLN_NOEXCEPT;

MLN_API mln_status mln_vulkan_texture_acquire_frame(
  mln_render_session* session,
  mln_vulkan_texture_frame* out_frame
) MLN_NOEXCEPT;

MLN_API mln_status mln_texture_acquire_shared_frame(
  mln_render_session* session,
  mln_shared_texture_frame* out_frame
) MLN_NOEXCEPT;

MLN_API mln_status mln_texture_read_premultiplied_rgba8(
  mln_render_session* session,
  uint8_t* out_data,
  size_t out_data_capacity,
  mln_texture_image_info* out_info
) MLN_NOEXCEPT;
```

`mln_owned_texture_attach` should be readback-first. It should let servers
render without writing graphics API setup code. It may expose backend-native
handles only when there is a concrete same-process use case and a documented
device and synchronization contract.

Host-provided texture attaches should be UI-first. They should keep requiring
the host's Metal or Vulkan handles because that is what makes the rendered
texture compatible with the UI framework.

Shared texture frames should be export-first. They should be requested through
an explicit descriptor because exportability usually affects allocation. The
shared frame contract must include producer backend, native handle kind,
ownership, format, dimensions, layout/state, adapter/device compatibility, and
acquire/release synchronization.

Surface sessions should be direct-present targets. They should share map/style,
camera, and event ownership with texture sessions, but they should not expose
texture-frame acquire/release APIs.

## Practical Order

The lowest-risk order is:

1. Generalize map attachment from one `texture_session` to one render target
   session.
2. Move common session lifecycle into shared render-session code.
3. Add MapLibre-owned/offscreen texture sessions for readback-first server use.
4. Add `mln_texture_read_premultiplied_rgba8` on rendered texture sessions.
5. Add shared/exportable texture descriptors and frame metadata for cross-API UI
   sharing.
6. Add native surface sessions as a sibling render target mode.

This order keeps the server path simple, preserves the existing host-provided UI
texture path, and prevents surface sessions from becoming a parallel renderer
model.
