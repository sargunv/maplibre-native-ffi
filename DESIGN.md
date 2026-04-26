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
- MapLibre Native architecture: <https://github.com/maplibre/maplibre-native/blob/main/ARCHITECTURE.md>
- MapLibre Compose: <https://github.com/maplibre/maplibre-compose>
- MapLibre Native Rust bindings: <https://github.com/maplibre/maplibre-native-rs>
- MapLibre Native Slint experiment: <https://github.com/maplibre/maplibre-native-slint>
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
- Native surface attach, resize, render, detach, and loss semantics as a fallback
  or comparison path.

## Why C First

The primary product is a universal ABI for many ecosystems. C is the best common
denominator for that goal.

- Kotlin/Native can consume C headers through `cinterop`.
- Swift and ObjC can import C directly or through a small ObjC++ shim.
- Zig can validate the ABI with `@cImport`.
- JNI can call C functions directly.
- Flutter, React Native, Python, C#/.NET, Rust, GTK, and Qt all have mature C FFI
  paths.

Rust may still be useful later as an idiomatic binding over the C ABI. Zig is a
good smoke-test consumer and may help with build orchestration. But neither Rust
nor Zig should be mandatory transit layers between consumers and MapLibre Native
if the product goal is a universal native ABI.

## ABI Shape

Use opaque handles:

```c
typedef struct mln_runtime mln_runtime;
typedef struct mln_map mln_map;
typedef struct mln_texture_session mln_texture_session;
typedef struct mln_surface_session mln_surface_session;
typedef struct mln_event_queue mln_event_queue;
```

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

Use status returns:

```c
typedef enum mln_status {
    MLN_STATUS_OK = 0,
    MLN_STATUS_ACCEPTED = 1,
    MLN_STATUS_INVALID_ARGUMENT = -1,
    MLN_STATUS_INVALID_STATE = -2,
    MLN_STATUS_WRONG_THREAD = -3,
    MLN_STATUS_UNSUPPORTED = -4,
    MLN_STATUS_NATIVE_ERROR = -5,
} mln_status;
```

Representative API:

```c
mln_status mln_runtime_create(const mln_runtime_options* options,
                              mln_runtime** out_runtime);
mln_status mln_runtime_destroy(mln_runtime* runtime);
mln_status mln_runtime_poll_event(mln_runtime* runtime, mln_event* out_event);

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

Do not expose C++ types, STL types, exceptions, callbacks, references, templates,
or C++ object ownership through the ABI.

## Map and Render Targets Are Separate

Offscreen GPU textures are the primary render target for high-quality UI
integration. Native surfaces are still useful as a fallback or comparison path,
but they should not be the center of the design.

A single “map view” handle is too vague. The ABI should separate map/control
state from render attachment.

```text
mln_map
  owns map/control state: style, camera, resource options, observer events,
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
```

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

- Backend type: Metal, Vulkan, Direct3D, or future WebGPU.
- Whether the host or wrapper owns the GPU device/queue.
- Whether the host or wrapper owns the texture/image.
- Texture format, dimensions, scale factor, color space, and alpha behavior.
- Synchronization: command buffer completion, fences, semaphores, image layouts,
  acquire/release rules, and number of in-flight frames.
- Import/sampling requirements for the host UI toolkit.
- Resize and invalidation behavior.
- Teardown requirements.

Start with one concrete texture descriptor. Generalize only after a second
backend proves which fields are shared.

`mln_map` may live without an attached `mln_texture_session` or
`mln_surface_session`. Detaching a render target does not destroy map state. This
is important for declarative UI frameworks where map state can outlive native
view/surface/texture lifetime.

Detached maps should support:

- style URL/JSON commands;
- camera commands and camera snapshots;
- bounds/options/debug settings;
- event queue ownership;
- pending map state that applies to the next attached render target.

Operations that require render state may return `MLN_STATUS_NO_RENDER_TARGET` or
`MLN_STATUS_NOT_READY` while detached, including frame rendering, snapshots,
rendered-feature queries, and projection APIs that require current render target
size or placement state.

## Thread Ownership

GPU texture and native surface rendering require split ownership.

The wrapper should not assume one internal thread can own everything.

```text
Map/control owner
  owns mbgl::Map, RunLoop, style/camera commands, observer queue

Render target owner
  owns graphics context, renderable, texture or drawable acquisition,
  render/present or render/acquire/release

Host/application owner
  owns UI state, gestures, toolkit view lifecycle, event draining
```

Some functions are command functions and may be thread-safe by enqueueing work.
Other functions are thread-bound and must return `MLN_STATUS_WRONG_THREAD` when
called from the wrong thread. The ABI should not silently switch behavior based
on caller thread.

Thread-affine handles should store their owner thread or executor identity. Every
public function should validate:

- handle is non-null;
- handle is alive;
- call is valid in current lifecycle state;
- call is on an allowed thread, or the function is explicitly documented as an
  enqueueing command.

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
store or coalesce the latest update and mark rendering pending without touching
backend or renderable resources. When a new texture or surface target attaches,
the render target consumes the latest update and requests a render.

`RendererBackend`, `Renderable`, and renderer resources are owned by the active
render target session, not by `mln_map`. For texture targets they are coupled to
the offscreen texture/render target and synchronization objects. For native
surface targets they are coupled to the surface/window/layer that makes those
resources valid.

Representative lifecycle:

```text
mln_map_create
  create RunLoop
  create MapObserver shim
  create long-lived RendererFrontend shim
  create mbgl::Map(frontend, observer, options, resources)

mln_texture_attach or mln_surface_attach
  create platform RendererBackend
  create Renderable / texture or surface resources
  create renderer resources
  connect frontend to render target session
  consume latest UpdateParameters
  request render

mln_texture_detach or mln_surface_detach
  disconnect frontend from render target session
  synchronously reset/destroy renderer resources
  destroy Renderable / RendererBackend / context-bound resources
  increment render target generation
  keep mbgl::Map alive

mln_map_destroy
  detach active render target if needed
  destroy mbgl::Map on owner thread
  destroy frontend, observer, RunLoop resources
```

Initial detach policy should be simple: detach must be called on the render
target owner thread and returns `MLN_STATUS_WRONG_THREAD` otherwise. Async detach
can be added later if a real adapter requires it.

For texture and native surface targets, the host or framework often controls when
rendering is allowed. The wrapper should support framework-driven rendering:

```text
Map state changes -> RendererFrontend update -> render invalidated event
Host schedules frame -> mln_texture_render(texture) or mln_surface_render(surface)
```

## Events and Observers

MapLibre Native observer callbacks should become queued C events. Host languages
should poll or drain events; the C++ wrapper should not call directly into JVM,
Swift, Kotlin/Native, JS, or other host runtimes from arbitrary native threads.

Initial event mapping:

| C event | Native grounding |
| --- | --- |
| Camera will/is/did change | `MapObserver::onCameraWillChange`, `onCameraIsChanging`, `onCameraDidChange` |
| Style loaded | `MapObserver::onDidFinishLoadingStyle` |
| Map loading started/finished/failed | `onWillStartLoadingMap`, `onDidFinishLoadingMap`, `onDidFailLoadingMap` |
| Map idle | `onDidBecomeIdle` |
| Frame render started/finished | `onWillStartRenderingFrame`, `onDidFinishRenderingFrame` |
| Map render started/finished | `onWillStartRenderingMap`, `onDidFinishRenderingMap` |
| Style image missing | `onStyleImageMissing` |
| Glyph/sprite/tile events | glyph callbacks, sprite callbacks, `onTileAction` |
| Render error | `onRenderError` |
| Render target invalidated/lost/destroyed | wrapper/frontend/session derived |

Event payloads should be plain data. Events should include map identity and, when
useful, generation counters such as style generation or render target generation.
Generations are for stale-event filtering; they are not exact request IDs unless
the underlying native API provides such a request identity.

## Async Semantics Without Futures

The ABI should preserve MapLibre Native's imperative and observer-driven model.
Do not expose Rust futures, C++ futures, Kotlin coroutines, Swift async, or JS
promises at the C layer.

Classify each operation:

| Category | Meaning |
| --- | --- |
| Immediate | Completes synchronously and the result is final. |
| Command | Applies or enqueues a native command; later effects may produce events. |
| Snapshot | Returns last-known state and may be stale relative to pending work. |
| Blocking query | Explicit synchronous query that may cross to the render/orchestration thread and block. |
| Request | Wrapper- or adapter-modeled async operation with completion/failure/cancellation events. Use only where intentionally modeled. |
| Event stream | Produces many events over time, such as camera animation or resource loading. |

Language adapters may convert the event model into coroutines, flows, promises,
callbacks, bindings, or futures appropriate to their ecosystem.

## Camera and Gestures

The C ABI exposes MapLibre Native camera operations. Application SDKs own gesture
recognition and declarative state.

The ABI should expose movement primitives that gestures produce:

- `jump_to`, `ease_to`, `fly_to`;
- `move_by`;
- `scale_by` around a screen anchor;
- `rotate_by` around screen points;
- `pitch_by` where supported;
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
the map inside their own scene graph. This is the path that can avoid native view
interop problems in Compose, Flutter, React Native, DVUI, and other GPU-rendered
or declarative UI systems.

The ABI should make texture rendering explicit instead of treating it as a
special case of native surfaces.

## Metal Texture Rendering Decision

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

Expose native-backed resource options first:

- API key and tile server options.
- Cache path and maximum cache size.
- Asset path.
- Platform context where required.
- Resource transform hook through the native `FileSource` model.

Do not promise wrapper-owned online/offline mode, retry policy, eviction hooks,
or offline-region management until they are tied to specific MapLibre Native
APIs.

Any thread that calls `FileSource::request` must own an active
`mbgl::util::RunLoop`; callbacks return on that same thread, and cancellation is
by dropping the returned `AsyncRequest`.

## Style and Data APIs

Start small and faithful.

Initial API:

- Load style by URL and JSON.
- Get current style JSON where supported.
- Add/remove/update runtime style images where needed.
- Query rendered features.
- Projection helpers.

Do not promise arbitrary style-spec JSON source/layer insertion until the
implementation deliberately uses and tests the relevant MapLibre Native style
conversion/parser internals or public APIs.

Compose parity candidates, not initial ABI commitments:

- GeoJSON source mutation without full style reload.
- Ordered layer insertion and movement.
- Layer paint/layout/filter mutation.
- Expression and filter JSON conversion.
- Feature state.
- Style serialization tests for runtime mutations.
- Light, terrain, sky, fog, projection, and DEM/raster-dem dependencies.
- Custom layers.
- Native annotation subsystem.

Annotations should initially be adapter conveniences over sources, layers,
images, queries, and feature state unless a native annotation API is deliberately
exposed.

## Error Handling

All ABI calls return `mln_status`. Detailed diagnostics should be retrievable
through an explicit last-error or diagnostic API with owned strings/buffers.

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
- Runtime-returned strings and buffers must be released through explicit ABI
  functions.
- No pointer returned from the ABI remains valid after a mutating call unless
  documented.
- Image buffers must document pixel format, stride, premultiplication, color
  space, scale, and ownership.

## Safety Invariants

- No C++ exceptions cross the C ABI.
- No host-language callback is invoked directly from arbitrary native threads.
- Opaque handles have explicit owners and destruction rules.
- Thread-affine calls validate their owner thread.
- Rendering and teardown happen only with valid backend/context state where
  required.
- Events are copied into queues as owned data.
- Public APIs preserve MapLibre Native behavior instead of inventing replacement
  semantics.

## Smoke Tests

Use Zig as the first non-C++ ABI consumer.

The initial Zig smoke should use `@cImport` against the public C header and:

- create and destroy a runtime;
- create and destroy a map;
- attach a texture target when available;
- set size, style, and camera;
- poll native events;
- render and acquire/release a GPU texture frame when available.

The Zig smoke is not a product adapter. It validates that the ABI is actually C,
not accidentally C++, Rust, or JNI shaped.

For the first texture smoke, prefer a minimal host renderer over a full UI
toolkit. On macOS, render MapLibre into an offscreen `MTLTexture`, then sample it
from a tiny Metal host. On Linux/Windows, do the equivalent with Vulkan once the
Metal spike proves the texture-session shape.

SDL3 may still be useful as a small cross-platform window/event host for the
sampling app, but the important test is offscreen texture rendering and sampling,
not rendering directly to an SDL window surface.

Keep the graphics backend constraint explicit: Metal on Apple platforms and
Vulkan elsewhere. Do not choose an OpenGL-first path.

For a real small Zig application UI, DVUI is the most relevant Zig-native toolkit
to track. It should be considered only after the ABI and texture-session contract
are proven with backend-appropriate Metal/Vulkan smoke tests, and only if it
teaches us something about sampling the map texture inside a real Zig application
UI.

MapLibre Native has an experimental WebGPU backend, but the initial ABI and
texture contract should not be based on WebGPU or `wgpu`. Revisit WebGPU after
Metal and Vulkan texture sessions are proven.

## Build Policy

Start with one platform/backend. MapLibre Native validates exactly one graphics
backend per build.

Initial candidates:

- macOS arm64 Metal.
- Linux amd64/arm64 Vulkan.
- Windows amd64/arm64 Vulkan.
- iOS arm64 Metal.
- Android arm64 Vulkan.

Build rules:

- Pin the MapLibre Native revision or core release.
- Keep backend selection explicit.
- Do not download native dependencies implicitly from normal builds.
- Support local prebuilt headers/libraries for debugging.
- Keep the wrapper C++ build small and focused.
- Defer full artifact publishing, support tiers, symbols, license bundles,
  Android AARs, iOS XCFrameworks, and Gradle capabilities until the native
  texture design is proven.

## Open Questions

- Which exact Metal texture target API can MapLibre Native support with the least
  invasive backend work?
- Should map/control ownership be wrapper-thread-owned, host-pumped, or support
  both from the beginning?
