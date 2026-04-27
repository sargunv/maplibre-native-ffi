# Design: Universal C ABI for MapLibre Native UI Rendering

## Purpose

This project explores a safe, simple, universal native API for embedding
MapLibre Native interactive maps in many UI ecosystems.

The primary product is a C ABI over MapLibre Native's C++ core. The C ABI should
be usable from Kotlin/JVM, Kotlin/Native, Swift/ObjC, Zig, Rust, Flutter, React
Native, C#/.NET, Python, GTK, Qt, WinUI, SwiftUI, Slint, egui, iced, and other
native or Rust UI toolkits.

MapLibre Compose is the motivating first application, but the ABI should not be
Compose-shaped. Compose, React Native, SwiftUI, Flutter, and other application
SDKs should build idiomatic state, gesture, lifecycle, and view APIs on top of
the lower-level native API.

## Goals

- Provide a safe and simple C ABI over MapLibre Native for interactive maps.
- Keep the API faithful to MapLibre Native's real C++ model.
- Make offscreen GPU texture rendering the primary path for UI integration.
- Keep language and UI framework adapters thin.
- Make ownership, thread affinity, lifecycle, and async observer behavior
  explicit at the ABI boundary.
- Support future bindings for JVM/JNI, Kotlin/Native, Swift/ObjC, Zig, Rust,
  Flutter, React Native, and other ecosystems from the same low-level API.

## Non-Goals

- Reimplement MapLibre Native.
- Expose the complete MapLibre Native C++ API directly.
- Provide a declarative map SDK in the C ABI.
- Own gesture recognition in the C ABI.
- Abstract every GUI toolkit as a generic widget API.
- Promise Android, iOS, or Web parity before their platform SDK behavior is
  inventoried.

## Prior Art

- MapLibre Native: <https://github.com/maplibre/maplibre-native>
- MapLibre Native architecture:
  <https://github.com/maplibre/maplibre-native/blob/main/ARCHITECTURE.md>
- MapLibre Compose: <https://github.com/maplibre/maplibre-compose>
- MapLibre Native Rust bindings:
  <https://github.com/maplibre/maplibre-native-rs>
- MapLibre Native Slint experiment:
  <https://github.com/maplibre/maplibre-native-slint>
- CXX Rust/C++ bridge: <https://cxx.rs/>
- Rust JNI bindings: <https://github.com/jni-rs/jni-rs>

## Architecture

```text
MapLibre Native C++ Core
        |
small C++ wrapper/shim
        |
universal C ABI
        |
language bindings and toolkit adapters
        |
Compose, JNI, Kotlin/Native, Swift, Zig, Rust, Flutter, RN, GTK, Qt, etc.
```

The implementation can use modern C++ internally. The exported product boundary
is C.

The C++ wrapper owns C++-specific concerns:

- `mbgl::Map` construction and destruction.
- `mbgl::util::RunLoop` ownership and pumping.
- `mbgl::MapObserver` subclasses.
- `mbgl::RendererFrontend` implementation.
- Renderer/backend/renderable ownership.
- Exception containment.
- Thread-affine teardown.
- Conversion between MapLibre Native types and ABI structs.

The C ABI owns cross-language concerns:

- Opaque handles.
- Explicit create/destroy/shutdown.
- Plain-data option, camera, size, coordinate, and event structs.
- Status codes and diagnostics.
- Event polling or draining.
- Thread ownership rules.
- Texture target attach, resize, render, frame acquire/release, and loss
  semantics.
- Native surface attach, resize, render, detach, and loss semantics as a
  fallback or comparison path.

## Project Structure

This structure is representative.

```text
include/
  maplibre_native_abi.h

src/
  abi/
    version.cpp
    diagnostics.cpp
    runtime.cpp
    map.cpp
    style.cpp
    camera.cpp
    events.cpp
    texture.cpp
    surface.cpp
  core/
    diagnostics.hpp/.cpp
    runtime.hpp/.cpp
    map.hpp/.cpp
    handle_registry.hpp/.cpp
    event_queue.hpp/.cpp
    map_observer.hpp/.cpp
    resource_loader.hpp/.cpp
    custom_resource_provider.hpp/.cpp
    renderer_frontend.hpp/.cpp
  render/
    texture_session.hpp/.cpp
    surface_session.hpp/.cpp
    metal/
      metal_texture_session.hpp/.mm
    vulkan/
      vulkan_texture_session.hpp/.cpp
  platform/
    darwin/

examples/
  zig-headless/
  zig-metal-texture/

tests/
  abi/
  lifecycle/
  texture/

cmake/
  toolchains/

third_party/
  maplibre-native/  # git submodule pinned to a known MapLibre Native commit
```

`include/maplibre_native_abi.h` is the product boundary. It should contain only
C types, opaque handles, status codes, versioned structs, and exported
functions.

`src/abi` implements exported C functions and performs ABI validation: null
checks, struct-size checks, state checks, thread checks, error conversion, and
calls into implementation objects. Files in this directory should stay thin:
they are the no-exception C boundary, not the owner of MapLibre Native concepts.

`src/core` owns C++ state that is independent of a concrete render backend:
runtime and map handles, handle registries, diagnostics, event queues, the
runtime-owned `RunLoop`, `mbgl::Map`, observer subclasses, the long-lived
renderer frontend, resource-loader dispatch and custom provider invocation, and
conversions between ABI structs and C++ types.

`src/render` owns render target sessions and backend-bound renderer resources.
Common texture/surface lifecycle code lives directly under `src/render`;
concrete graphics integrations live in backend subdirectories such as
`src/render/metal` and `src/render/vulkan`.

`src/platform` is reserved for wrapper-owned platform glue when needed, such as
MapLibre Native platform entry points and host-specific adapters. Runtime-owned
resource policy stays in `src/core`; platform files should only bridge
MapLibre's platform seams to core implementations. Directly linked MapLibre
Native platform support sources may remain in the build files until there is
local wrapper code worth organizing here.

`examples/zig-headless` validates the C header with `@cImport` and exercises
runtime/map/event lifecycle without depending on a UI toolkit.

`examples/zig-metal-texture` validates the primary rendering path: render into
an offscreen Metal texture and sample it with a tiny host Metal renderer.

`third_party/maplibre-native` is the initial development source for MapLibre
Native. Keeping it as a pinned submodule makes it possible to inspect, debug,
and patch backend internals such as Metal `HeadlessBackend` and
`OffscreenTexture` while the texture-session API is being designed.

Do not add JNI, Kotlin/Native, Swift, Rust, Flutter, React Native, or DVUI
adapters to the core layout until the C ABI and texture-session model are
proven.

## Why C First

The primary product is a universal ABI for many ecosystems. C is the best common
denominator for that goal.

- Kotlin/Native can consume C headers through `cinterop`.
- Swift and ObjC can import C directly or through a small ObjC++ shim.
- Zig can validate the ABI with `@cImport`.
- JNI can call C functions directly.
- Flutter, React Native, Python, C#/.NET, Rust, GTK, and Qt all have mature C
  FFI paths.

Rust may still be useful later as an idiomatic binding over the C ABI. Zig is a
good smoke-test consumer and may help with build orchestration.

## ABI Shape

Use opaque handles:

```c
typedef struct mln_runtime mln_runtime;
typedef struct mln_map mln_map;
typedef struct mln_texture_session mln_texture_session;
typedef struct mln_surface_session mln_surface_session;
```

`mln_runtime` is the shared native environment for one or more maps. It owns
runtime-level infrastructure such as resource loading, cache configuration,
backend capability checks, and the owner-thread `RunLoop` used by every map in
the runtime. `mln_map` owns one map instance inside a runtime: style, camera,
observer events, render invalidation state, and the currently attached texture
or surface session. Multiple maps may share one runtime while using different
styles, cameras, and render targets.

Use versioned plain-data structs:

```c
typedef struct mln_camera_options {
    uint32_t size;
    uint32_t fields;
    double latitude;
    double longitude;
    double zoom;
    double bearing;
    double pitch;
} mln_camera_options;
```

`size` is the caller-visible struct size for ABI evolution. `fields` is a
bitmask of intentionally supplied values so zero remains a valid value instead
of meaning "unset".

Use status returns:

```c
typedef enum mln_status {
    MLN_STATUS_OK = 0,
    MLN_STATUS_INVALID_ARGUMENT = -1,
    MLN_STATUS_INVALID_STATE = -2,
    MLN_STATUS_WRONG_THREAD = -3,
    MLN_STATUS_UNSUPPORTED = -4,
    MLN_STATUS_NATIVE_ERROR = -5,
} mln_status;
```

Each ABI function must document the exact `mln_status` values it can return and
the meaning of each value for that function.

Representative control API shape, not yet a frozen ABI contract:

```c
mln_status mln_runtime_create(const mln_runtime_options* options,
                              mln_runtime** out_runtime);
mln_status mln_runtime_destroy(mln_runtime* runtime);

mln_status mln_map_create(mln_runtime* runtime,
                          const mln_map_options* options,
                          mln_map** out_map);
mln_status mln_map_destroy(mln_map* map);

mln_status mln_map_set_style_url(mln_map* map, const char* url);
mln_status mln_map_set_style_json(mln_map* map, const char* json);
mln_status mln_map_get_camera(mln_map* map, mln_camera_options* out_camera);
mln_status mln_map_jump_to(mln_map* map, const mln_camera_options* camera);
mln_status mln_map_ease_to(mln_map* map,
                           const mln_camera_options* camera,
                           const mln_animation_options* animation);
mln_status mln_map_fly_to(mln_map* map,
                          const mln_camera_options* camera,
                          const mln_animation_options* animation);
mln_status mln_map_move_by(mln_map* map,
                           double dx,
                           double dy,
                           const mln_animation_options* animation);
mln_status mln_map_scale_by(mln_map* map,
                            double scale,
                            const mln_screen_point* anchor,
                            const mln_animation_options* animation);
mln_status mln_map_rotate_by(mln_map* map,
                             mln_screen_point first,
                             mln_screen_point second,
                             const mln_animation_options* animation);
mln_status mln_map_cancel_transitions(mln_map* map);
```

Do not expose C++ types, STL types, exceptions, references, templates, or C++
object ownership through the ABI. Avoid callbacks in the universal ABI unless a
callback is explicitly documented as a low-level native callback with strict
threading, reentrancy, and lifetime rules.

## Map and Render Targets Are Separate

Offscreen GPU textures are the primary render target for high-quality UI
integration. Native surfaces are still useful as a fallback or comparison path,
but they should not be the center of the design.

A single “map view” handle is too vague. The ABI should separate map/control
state from render attachment.

```text
mln_map
  owns map/control state: style, camera, observer events,
  long-lived RendererFrontend, mbgl::Map

mln_texture_session
  owns offscreen render attachment: backend texture/render target, renderer
  resources, resize, render, frame acquire/release, synchronization, teardown

mln_surface_session
  owns native-surface render attachment: native surface handle, RendererBackend,
  Renderable/surface resources, renderer resources, resize, render/present,
  surface loss, teardown
```

Representative texture API:

```c
mln_status mln_texture_attach(mln_map* map,
                              const mln_texture_descriptor* descriptor,
                              mln_texture_session** out_texture);
mln_status mln_texture_resize(mln_texture_session* texture,
                              uint32_t width,
                              uint32_t height,
                              double scale_factor);
mln_status mln_texture_render(mln_texture_session* texture);
mln_status mln_texture_acquire_frame(mln_texture_session* texture,
                                     mln_texture_frame* out_frame);
mln_status mln_texture_release_frame(mln_texture_session* texture,
                                     const mln_texture_frame* frame);
mln_status mln_texture_detach(mln_texture_session* texture);
mln_status mln_texture_destroy(mln_texture_session* texture);
```

Representative native-surface API:

```c
mln_status mln_surface_attach(mln_map* map,
                              const mln_surface_descriptor* descriptor,
                              mln_surface_session** out_surface);
mln_status mln_surface_resize(mln_surface_session* surface,
                              uint32_t width,
                              uint32_t height,
                              double scale_factor);
mln_status mln_surface_render(mln_surface_session* surface);
mln_status mln_surface_detach(mln_surface_session* surface);
mln_status mln_surface_destroy(mln_surface_session* surface);
```

All render target integrations should follow the same lifecycle shape: attach,
resize, render, detach, destroy. Backend-specific native handles and GPU
synchronization rules must still be documented per integration because Metal,
Vulkan, and native surfaces do not share identical ownership rules.

Each surface integration must document:

- Native handle type.
- Handle ownership.
- Valid creation thread.
- Valid render thread.
- Resize semantics.
- Present/swap ownership.
- Context or surface loss behavior.
- Teardown requirements.

Each texture integration must document:

- Backend type. Initial texture targets are Metal and Vulkan.
- Whether the host or wrapper owns the GPU device/queue.
- Whether the host or wrapper owns the texture/image.
- Texture format, dimensions, scale factor, color space, and alpha behavior.
- Synchronization: command buffer completion, fences, semaphores, image layouts,
  acquire/release rules, and number of in-flight frames.
- Import/sampling requirements for the host UI toolkit.
- Resize and invalidation behavior.
- Teardown requirements.

OpenGL is not a primary target and may be added only as a compatibility fallback
for a concrete host or platform requirement. WebGPU is a future research target,
not an initial ABI target.

`mln_map` may live without an attached `mln_texture_session` or
`mln_surface_session`. Detaching a render target does not destroy map state.
This is important for declarative UI frameworks where map state can outlive
native view/surface/texture lifetime.

Detached maps should support:

- style URL/JSON commands;
- camera commands and camera snapshots;
- bounds/options/debug settings;
- map event delivery;
- pending map state that applies to the next attached render target.

Operations that require render state may return `MLN_STATUS_NO_RENDER_TARGET` or
`MLN_STATUS_NOT_READY` while detached.

## Thread Ownership

Thread ownership is described in terms of logical roles, not mandatory physical
threads. Some hosts may run all roles on one thread. Others may split map
control, rendering, and UI lifecycle across different threads because their
graphics API or UI toolkit requires it.

```text
Map/control owner
  owns runtime RunLoop, mbgl::Map instances, style/camera commands,
  observer delivery

Render target owner
  owns graphics context, renderable, texture or drawable acquisition,
  render/present or render/acquire/release

Host/application owner
  owns UI state, gestures, toolkit view lifecycle, event draining
```

The wrapper should record which thread owns each created object and return
`MLN_STATUS_WRONG_THREAD` when an API is called from an invalid thread. For
simple hosts, the same thread may own all roles.

Some functions are command functions and may be thread-safe by enqueueing work.
Other functions are thread-bound and must return `MLN_STATUS_WRONG_THREAD` when
called from the wrong thread. The ABI should not silently switch behavior based
on caller thread.

Thread-affine handles should store their owner thread or executor identity.
Every public function should validate:

- handle is non-null;
- handle is alive;
- call is valid in current lifecycle state;
- call is on an allowed thread, or the function is explicitly documented as an
  enqueueing command.

Map/control ownership is host-pumped initially. The host creates `mln_runtime`
on an owner thread and must call map/control APIs and runtime pumping APIs from
that thread. The ABI should not create a hidden map/control thread in the
initial design. Adapters that want a threaded model can build one above the C
ABI.

MapLibre's `RunLoop` is the current scheduler for its owner thread, so the
initial ABI permits only one live runtime per owner thread. Multiple maps share
that runtime-owned scheduler. `mln_runtime_run_once` pumps the runtime's own
`RunLoop`; it does not discover or borrow an ambient scheduler from the thread.

Process-global native callbacks, such as the low-level log callback, are an
explicit exception to the host-pumped model. They may run on MapLibre logging or
worker threads and must not call back into the ABI or MapLibre Native. Language
adapters should translate these callbacks into host-appropriate logging models,
for example by queueing records onto a runtime-owned executor before invoking
user code.

## RendererFrontend Model

`mbgl::Map` requires a `RendererFrontend`. The wrapper's frontend is the bridge
between map/control state and the current render target.

The frontend should:

- receive `UpdateParameters` from `mbgl::Map`;
- coalesce the latest update where appropriate;
- signal render invalidation to the texture session, surface session, or host
  adapter;
- preserve MapLibre Native's callback-thread expectations;
- implement synchronous renderer cleanup in `reset()`;
- avoid direct host-language callbacks from native threads.

The frontend should be long-lived with `mln_map`. It should not own platform GPU
resources directly. Instead, it connects to the currently attached
`mln_texture_session` or `mln_surface_session`.

When no render target is attached, frontend `update(UpdateParameters)` should
store the latest render update needed to initialize the next attached target and
mark rendering pending without touching backend or renderable resources. When a
new texture or surface target attaches, the render target consumes the latest
update and requests a render.

`RendererBackend`, `Renderable`, and backend-bound renderer/renderable resources
are owned by the active render target session, not by `mln_map`. For texture
targets they are coupled to the offscreen texture/render target and
synchronization objects. For native surface targets they are coupled to the
surface/window/layer that makes those resources valid.

Representative ownership lifecycle, not exact exported function names:

```text
mln_runtime_create
  create RunLoop

mln_map_create
  create MapObserver shim
  create long-lived RendererFrontend shim
  create mbgl::Map(frontend, observer, options, resources)

mln_texture_attach or mln_surface_attach
  create platform RendererBackend
  create Renderable / texture or surface resources
  create backend-bound renderer/renderable resources
  connect frontend to render target session
  consume latest UpdateParameters
  request render

mln_texture_detach or mln_surface_detach
  disconnect frontend from render target session
  synchronously reset/destroy backend-bound renderer/renderable resources
  destroy Renderable / RendererBackend / context-bound resources
  increment render target generation
  keep mbgl::Map alive

mln_map_destroy
  detach active render target if needed
  destroy mbgl::Map on owner thread
  destroy frontend and observer resources

mln_runtime_destroy
  destroy RunLoop resources
```

Initial detach policy should be simple: `detach` releases backend-bound render
resources and must be called on the render target owner thread. `destroy`
releases the opaque session handle after detach, or performs detach first if the
session is still attached. Both functions return `MLN_STATUS_WRONG_THREAD` when
called from an invalid thread.

For texture and native surface targets, the host or framework often controls
when rendering is allowed. The wrapper should support framework-driven
rendering:

```text
Map state changes -> RendererFrontend update -> map render-invalidated event
Host schedules frame -> mln_texture_render(texture) or mln_surface_render(surface)
```

## Events and Observers

MapLibre Native observer callbacks should become queued C events owned by
`mln_map`. Host languages should poll or drain events through documented map
event APIs; the C++ wrapper should not call directly into JVM, Swift,
Kotlin/Native, JS, or other host runtimes from arbitrary native threads.

Initial event candidates:

| C event                                  | Native grounding                                                             |
| ---------------------------------------- | ---------------------------------------------------------------------------- |
| Camera will/is/did change                | `MapObserver::onCameraWillChange`, `onCameraIsChanging`, `onCameraDidChange` |
| Style loaded                             | `MapObserver::onDidFinishLoadingStyle`                                       |
| Map loading started/finished/failed      | `onWillStartLoadingMap`, `onDidFinishLoadingMap`, `onDidFailLoadingMap`      |
| Map idle                                 | `onDidBecomeIdle`                                                            |
| Frame render started/finished            | `onWillStartRenderingFrame`, `onDidFinishRenderingFrame`                     |
| Map render started/finished              | `onWillStartRenderingMap`, `onDidFinishRenderingMap`                         |
| Style image missing                      | `onStyleImageMissing`                                                        |
| Glyph/sprite/tile events                 | glyph callbacks, sprite callbacks, `onTileAction`                            |
| Render error                             | `onRenderError`                                                              |
| Render target invalidated/lost/destroyed | wrapper/frontend/session derived                                             |

The final ABI does not need to expose every native observer callback initially.
Start with events needed for camera state, style/load lifecycle, render
invalidation, render completion, and errors.

Event payloads should be plain data. Events should include map identity and,
when useful, generation counters such as style generation or render target
generation. Generations are for stale-event filtering; they are not exact
request IDs unless the underlying native API provides such a request identity.

## Async Semantics Without Futures

The ABI should preserve MapLibre Native's imperative and observer-driven model.
Do not expose Rust futures, C++ futures, Kotlin coroutines, Swift async, or JS
promises at the C layer.

Classify each operation:

| Category       | Meaning                                                                                                                            |
| -------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| Immediate      | Completes synchronously and the result is final.                                                                                   |
| Command        | Applies or enqueues a native command; later effects may produce events.                                                            |
| Snapshot       | Returns last-known state and may be stale relative to pending work.                                                                |
| Blocking query | Explicit synchronous query that may cross to the render/orchestration thread and block. Use sparingly and document deadlock risks. |
| Request        | Wrapper- or adapter-modeled async operation with completion/failure/cancellation events. Use only where intentionally modeled.     |
| Event stream   | Produces many events over time, such as camera animation or resource loading.                                                      |

Each ABI function should document its async category, valid calling thread, and
whether completion is represented by the return status, later map events, or
both.

Language adapters may convert the event model into coroutines, flows, promises,
callbacks, bindings, or futures appropriate to their ecosystem.

## Camera and Gestures

The C ABI exposes MapLibre Native camera operations. Application SDKs own
gesture recognition and declarative state.

These operations should be grounded in `mbgl::Map` camera and projection APIs,
not invented as adapter-specific gesture concepts. The ABI should expose camera
movement primitives that application SDK gesture recognizers can translate into:

- `jump_to`, `ease_to`, `fly_to`;
- `move_by`;
- `scale_by` around a screen anchor;
- `rotate_by` around screen points;
- `pitch_by`;
- `cancel_transitions`;
- screen/geographic projection helpers.

Camera observers are notification, not veto. If an adapter wants to reject or
override native movement, it should issue a later command such as
`cancel_transitions` or `jump_to`.

Declarative SDKs should model desired camera separately from observed native
camera. During `fly_to` or `ease_to`, the desired target and observed camera
intentionally differ.

## Texture Rendering

Texture rendering is the strategic target because it lets UI toolkits composite
the map inside their own scene graph. This is the path that can avoid native
view interop problems in Compose, Flutter, React Native, DVUI, and other
GPU-rendered or declarative UI systems.

The ABI should make texture rendering explicit instead of treating it as a
special case of native surfaces.

## Metal Texture Rendering

MapLibre Native already has a Metal offscreen rendering path through
`mbgl::mtl::HeadlessBackend` and `mbgl::gfx::OffscreenTexture`. The Metal
offscreen texture is backed by an internal `MTL::Texture` and is configured for
render-target and shader-read usage. The initial Metal texture-session design
should build on this existing headless/offscreen path rather than modifying the
view-oriented `MLNMapView+Metal` path.

The ABI should expose acquired texture frames from this offscreen render target,
with explicit lifetime and synchronization rules. CPU readback remains a debug
and test tool only.

Design assumptions:

- The offscreen Metal texture can be exposed as a sampleable `MTLTexture` or
  equivalent Objective-C Metal object.
- Texture lifetime can be modeled with acquire/release semantics.
- Resize creates a new texture generation.
- Initial synchronization can be conservative; optimized synchronization can
  evolve without changing the high-level texture-session model.
- The same texture-session concept can later be evaluated for Vulkan, even if
  Vulkan requires different ownership or synchronization details.

Risks:

- MapLibre Native may need a small public or wrapper-private accessor for the
  internal offscreen Metal texture.
- Host UI frameworks may not accept externally produced GPU textures directly.
- Synchronization may require backend-specific frame metadata.
- Vulkan may require a different ownership model than Metal.

Vulkan texture support should be designed after the Metal texture path clarifies
the common ABI shape and which parts must remain backend-specific.

## Resource Loading and Cache

Resource configuration is runtime-level so multiple maps can share file sources,
cache policy, and tile-server URL behavior. The ABI should keep MapLibre's
native resource stack in control instead of asking host languages to reimplement
loading.

The resource subsystem design is:

- Use MapLibre Native's `MainResourceLoader` behind the `FileSourceManager`
  hook.
- Register native built-ins for file, asset, database/cache, MBTiles, PMTiles,
  and platform HTTP/HTTPS loading.
- Wrap `FileSourceType::Network` only as an extension point: ABI providers may
  handle host-specific resources, otherwise requests pass through to native
  `OnlineFileSource`.
- Keep `ResourceOptions` runtime-derived, including asset path, cache path,
  maximum ambient cache size, and runtime-unique `platformContext` identity.
- Expose process-level network status as wrappers over `mbgl::NetworkStatus`,
  matching MapLibre Native rather than inventing runtime-scoped online/offline
  policy.
- Keep resource transforms URL-only, matching `mbgl::ResourceTransform`; do not
  expose header/body/auth mutation unless a concrete product requirement selects
  a separate request-interception feature.

Custom providers use an async request-handle ABI. The provider callback decides
pass-through versus handle, may complete inline or later, can observe
cancellation, and releases the handle when finished. Completion copies response
bytes/strings before returning to the host. Provider failures become MapLibre
resource errors and ABI events/diagnostics without allowing exceptions through
the C boundary.

Cache and offline operations should stay direct wrappers over MapLibre concepts:

- Ambient cache maintenance covers reset database, invalidate ambient cache,
  clear ambient cache, pack database, and maximum ambient cache size.
- Offline region APIs remain deferred until region handle ownership, metadata
  buffer release, observer delivery, and download status semantics are designed.
- Retry policy, custom eviction hooks, and higher-level online/offline product
  modes stay out of the core ABI unless they wrap a specific MapLibre API.

## Style and Data APIs

MapLibre Native exposes style mutation through `mbgl::style::Style`, reached
from `mbgl::Map::getStyle()`. The C ABI should wrap that model instead of
inventing a separate declarative style system.

Initial style API:

- load style by URL or JSON through `Style::loadURL` and `Style::loadJSON`;
- read current style URL/JSON through `Style::getURL` and `Style::getJSON`;
- read style name and default camera;
- configure transition options;
- add, get, and remove runtime style images;
- query style/source/layer existence by ID;
- remove sources and layers by ID.

Source and layer mutation should be added in stages:

1. JSON-first source/layer insertion: accept style-spec JSON fragments,
   construct the corresponding native `Source` or `Layer`, and add them through
   `Style::addSource` and `Style::addLayer`.
2. Common typed source helpers: GeoJSON source with URL or inline GeoJSON first,
   then vector, raster, raster-dem, image, and so on.
3. Generic layer property get/set using style-spec property names and typed
   `mln_value` payloads. JSON property values may be useful for smoke tests and
   tooling, but typed values should be the primary non-JSON C ABI path.

Layer mutation should use `Layer::setProperty(name, value)` and
`Layer::getProperty(name)` as the generic C ABI path for paint/layout/filter and
common layer fields such as source, source layer, visibility, and zoom range.
Typed per-property setters are better generated in higher-level SDKs such as
Compose or Swift. Add typed C helpers only for common cross-layer fields or
proven performance-sensitive paths; they should not be required for style-spec
coverage.

Rendered-feature queries and screen/geographic projection helpers are map/render
queries, not style construction APIs. They belong near the map or render target
query surface and should document whether they require an attached render
target.

## Error Handling

All ABI calls return `mln_status`. Every ABI call clears thread-local
diagnostics on entry, and every non-OK synchronous return stores its diagnostic
message in thread-local diagnostics. Callers should read
`mln_thread_last_error_message()` immediately after a non-OK status.
Async/native observer failures are delivered through map events with copied
error payloads rather than handle-local last-error state.

Important error categories:

- Invalid argument.
- Invalid lifecycle state.
- Wrong thread.
- Unsupported backend/platform/feature.
- Native exception converted to error.
- Texture/surface/context failure.
- Style load/parse failure.
- Resource/glyph/sprite/tile failure.
- Render failure.

Error strings are diagnostic only and must not be parsed by adapters.

## Memory Ownership

- Host-provided strings and buffers are borrowed for the duration of the call
  unless a function explicitly retains/copies them.
- ABI-owned returned strings and buffers must be released through explicit ABI
  functions. Do not add release APIs until a public function returns owned ABI
  storage.
- No pointer returned from the ABI remains valid after a mutating call unless
  documented.
- Image buffers must document pixel format, stride, premultiplication, color
  space, scale, and ownership.

## Safety Invariants

- No C++ exceptions cross the C ABI.
- Host-language callbacks are not invoked directly from arbitrary native
  threads; any low-level native callback exception must be explicitly documented
  and adapted by higher-level language bindings before reaching user code.
- Opaque handles have explicit owners and destruction rules.
- Thread-affine calls validate their owner thread.
- Rendering and teardown happen only with valid backend/context state where
  required.
- Events are copied into map-owned queues as owned data.
- Public APIs preserve MapLibre Native behavior instead of inventing replacement
  semantics.

## Smoke Tests

Use Zig as the first non-C++ ABI consumer because `@cImport` quickly exposes
whether the public header is actually C-shaped.

The representative Zig example should be a small interactive map, not a product
adapter. It should exercise the core ABI surface:

- create and destroy a runtime;
- create and destroy a map;
- attach a texture target;
- set size, style, and camera;
- translate basic pointer/scroll/keyboard input into camera commands;
- drain map events;
- render and acquire/release GPU texture frames;
- sample the map texture in a minimal host renderer.

The example should validate ABI shape, lifecycle, events, texture ownership, and
interactive camera control without introducing a full UI toolkit.

Backend choice remains explicit: Metal on Apple platforms and Vulkan elsewhere.
OpenGL is not a primary target. WebGPU is a future research target after Metal
and Vulkan texture sessions are proven.

SDL3 or a tiny native shell may be used to provide a window and input, but the
example should render through the texture-session path rather than treating a
window surface as the primary integration model.

DVUI is the relevant Zig-native UI toolkit for a later architecture check
against a non-Compose toolkit. Use it after the minimal Zig texture example
proves the C ABI and texture-session contract.

## Build Policy

The wrapper should build against a known MapLibre Native source revision. During
development, that source is `third_party/maplibre-native`, a pinned git
submodule. A local `MLN_SOURCE_DIR` override may point at a sibling checkout for
MapLibre Native development.

Backend selection is explicit per build. Initial targets follow the rendering
design: Metal on Apple platforms and Vulkan elsewhere. OpenGL is not a primary
target.

Normal builds should not implicitly download native dependencies. Packaging
formats such as Android AARs, iOS XCFrameworks, and published binary artifacts
are distribution concerns, not part of the core ABI design.

When building against MapLibre Native with `MLN_WITH_CORE_ONLY`, the wrapper
must still provide or link the platform support symbols that `mbgl-core`
expects, such as logging, run-loop, thread-local storage, timers, and platform
thread helpers. On macOS the initial wrapper links the Darwin run-loop/thread
helpers plus the default portable support files directly into
`maplibre_native_abi`.
