# Design: Thin Rust Wrapper for MapLibre Native UI Rendering

## Purpose

This repository explores a new Rust crate that provides a thin, safer, faithful
wrapper over MapLibre Native for interactive UI rendering.

The crate is not intended to be a MapLibre Compose implementation detail. It
should be usable by MapLibre Compose first and should avoid design choices that
would unnecessarily block future adapters for other UI frameworks.

Future adapter context matters even though it is not part of the initial phase.
Potential later consumers include GTK, Qt, WinUI, SwiftUI, Flutter, React Native,
Slint, egui, iced, C#/.NET, Python, and other Rust or native UI toolkits. The
initial wrapper should not build abstractions for all of them up front, but it
should avoid Compose-specific assumptions in the core Rust API.

The near-term user is MapLibre Compose's JVM desktop target. Later Android, iOS,
or other framework adapters should be treated as separate experiments once the
desktop path proves the wrapper useful.

## Goals

- Provide a Rust-owned wrapper over MapLibre Native Core for interactive maps.
- Hide most C++ ownership and lifetime hazards behind Rust types.
- Preserve MapLibre Native rendering behavior and compatibility.
- Keep UI framework and language binding layers thin.
- Support multiple rendering backends as package/build variants.
- Make MapLibre Compose desktop more capable without baking Compose concepts
  into the Rust runtime.
- Do not knowingly block later Android or iOS native-core experiments.

## Non-Goals

- Reimplement MapLibre Native in Rust.
- Expose the complete MapLibre Native C++ API directly.
- Replace MapLibre Compose's public Kotlin API.
- Provide a full declarative map SDK in Rust.
- Abstract every GUI toolkit as a lowest-common-denominator widget API.
- Make WebAssembly support part of the initial native-core path.
- Promise Android or iOS SDK replacement before their lifecycle, gestures,
  offline/cache, permissions, location, annotations, and platform UI behavior are
  inventoried.

Web should continue to use MapLibre GL JS for the foreseeable future. A future
Wasm experiment can be designed separately once the native runtime is proven.

## Prior Art

- MapLibre Native: <https://github.com/maplibre/maplibre-native>
- MapLibre Native architecture: <https://github.com/maplibre/maplibre-native/blob/main/ARCHITECTURE.md>
- MapLibre Native Rust bindings: <https://github.com/maplibre/maplibre-native-rs>
- `maplibre_native` crate docs: <https://docs.rs/maplibre_native>
- MapLibre Compose: <https://github.com/maplibre/maplibre-compose>
- MapLibre Compose roadmap: <https://maplibre.org/maplibre-compose/roadmap>
- MapLibre Native Slint experiment: <https://github.com/maplibre/maplibre-native-slint>
- CXX Rust/C++ bridge: <https://cxx.rs/>
- Rust JNI bindings: <https://github.com/jni-rs/jni-rs>
- Gobley/UniFFI KMP bindings: <https://github.com/gobley/gobley>

## Key Existing Lessons

`maplibre-native-rs` already proves that Rust can wrap MapLibre Native Core with
`cxx`. It also contains reusable ideas for:

- Pinning a MapLibre Native Core release.
- Downloading or building MapLibre Native Core.
- Selecting Metal, OpenGL, or Vulkan backends with Cargo features.
- Linking native system libraries and frameworks.
- Bridging C++ objects through `cxx`.
- Providing typed Rust wrappers for camera, size, coordinates, style loading,
  observers, images, sources, and layers.

Its current public API is primarily image/headless/server oriented. It has a
continuous renderer and a Slint example, but that example performs GPU render to
CPU readback to UI pixel buffer. This is useful as an MVP pattern, but it should
not define the final UI runtime architecture.

MapLibre Compose already has a desktop native path based on JVM, AWT `Canvas`,
SimpleJNI, CMake, and C++ glue. The public Kotlin API is separated from platform
implementation internals by `MapAdapter` and platform `ComposableMapView`
actuals. That boundary should be preserved.

MapLibre Native platform SDKs show that interactive rendering is not only a map
API problem. It also requires careful handling of `RendererFrontend`,
`RendererBackend`, `Renderable`, `RunLoop`, surface creation/destruction,
render-thread scheduling, app lifecycle, and context loss.

## High-Level Architecture

```text
MapLibre Native C++ Core
        |
small C++ shim compatible with cxx
        |
maplibre-native-core
        |
maplibre-native-runtime Rust API
        |
optional adapters: JNI, Compose, Rust UI examples
        |
MapLibre Compose first
```

The Rust crate should expose the smallest safe model that can drive MapLibre
Native correctly. It should preserve native concepts where possible and only
introduce Rust abstractions to make ownership, threading, error handling, and
lifecycle constraints explicit.

## Why Rust

Rust is an implementation choice for the wrapper's internal safety boundary, not
because MapLibre Native requires a Rust-shaped public API. The wrapper still aims
to be thin and faithful to MapLibre Native.

Rust is useful here because it can make these wrapper-level concerns explicit:

- Ownership of native map, observer, frontend, surface, and event objects.
- Intentional `Send`/`Sync` behavior for thread-affine handles.
- Concentration of unsafe code at the C++ bridge.
- Explicit shutdown instead of accidental destructor-thread behavior.
- Command/event APIs that avoid pretending native loading and rendering are
  synchronously complete.
- Safe adapters for Rust callers and a strong implementation layer for JNI or
  future FFI shims.

Rust does not remove the hardest MapLibre Native integration problems: renderer
lifecycle, platform surfaces, backend context validity, resource loading,
callbacks, and native build/distribution. Those remain native integration
problems and must be solved faithfully against the C++ API.

Alternatives are viable depending on the primary product:

- A modern C++ wrapper is the closest fit to MapLibre Native internals and may be
  the simplest path if the only goal is JNI/C++ integration.
- A plain C API is the strongest long-term language-neutral ABI if the product is
  a universal native substrate for many ecosystems.
- Zig is attractive for C ABI smoke tests, cross-compilation, and build
  orchestration around a C shim.

This design chooses Rust because the desired first-class artifact is a safe
source-level wrapper that can later expose JNI and experimental C ABI surfaces.
If the project goal changes to “stable universal ABI first,” the architecture
should be revisited and a C-first or Zig/C-shim design may be more appropriate.

## Proposed Workspace Structure

```text
crates/
  maplibre-native-core/
  maplibre-native-runtime/
  maplibre-native-jni/
examples/
  desktop-minimal/
  headless-smoke/
  jni-smoke/
xtask/
```

`maplibre-native-core` is the low-level C++ bridge and build/link layer. It may
start by porting selected build and `cxx` bridge ideas from `maplibre-native-rs`.

`maplibre-native-runtime` is the thin safe Rust wrapper over MapLibre Native UI
rendering concepts. This is the main product API for Rust users and downstream
adapters.

`maplibre-native-jni` is an optional adapter/demo crate. It should prove that the
runtime can be consumed from JVM and Android, but MapLibre Compose may still own
its final Kotlin-specific JNI bridge in its own repository.

## API Layers

The project should keep these API layers separate:

- Rust API: thin, safe, faithful wrapper over MapLibre Native concepts.
- Experimental C ABI smoke: minimal non-Rust validation surface, preferably
  exercised by a tiny Zig program using `@cImport`.
- JNI adapter: narrow JVM bridge needed by MapLibre Compose desktop, implemented
  as a peer adapter over the Rust runtime rather than through the C ABI unless a
  later stable ABI justifies that layering.
- Toolkit adapters: Compose first; other adapters only when there is a concrete
  consumer or a proven need to generalize.

A stable C ABI is not part of the initial design. A minimal experimental C ABI
smoke is useful early to verify that the Rust runtime can be exposed without
Rust-specific assumptions. JNI and the C ABI should initially be peer adapters
over the Rust runtime, not stacked layers.

## API Boundaries

### C++ Boundary

The C++ boundary should be narrow and explicit. It should expose only the pieces
needed to drive an interactive map runtime:

- Construct and destroy native map/runtime objects.
- Create or attach renderer frontend/backend objects.
- Load styles.
- Mutate camera.
- Resize render targets.
- Process input events when the runtime owns gesture translation.
- Query rendered features and projection data.
- Add/remove images, sources, and layers.
- Receive observer callbacks.
- Render, present, pause, resume, and handle surface loss.

Use `cxx` for Rust/C++ interop where possible. Avoid broad generated bindings
over arbitrary MapLibre Native headers. The Rust crate should own the public API
shape rather than mirroring C++ classes wholesale.

C++ shim policy:

- No C++ exception may cross the FFI boundary.
- C++ subclasses for MapLibre Native virtual interfaces should live in the shim.
- Rust callbacks must not be invoked directly from arbitrary native threads.
- Observer/frontend events should be queued as plain-data events and delivered
  through the runtime dispatcher.
- Each opaque C++ object must have an explicit owner and destruction thread.
- Objects requiring an active backend/context for teardown must expose explicit
  shutdown rather than relying on Rust `Drop` alone.

### Rust Runtime Boundary

The runtime should expose framework-neutral concepts:

```rust
NativeRuntime
NativeMap
RenderSurface
SurfaceDescriptor
RenderBackend
MapCamera
RuntimeEvent
MapEvent
ResourceEvent
RenderEvent
```

Broad style object types such as `Style`, `Source`, and `Layer` should not be
stabilized early. Prefer narrow commands that map to concrete MapLibre Native
APIs until a typed style model proves its value.

Representative API shape:

```rust
let runtime = Runtime::new(RuntimeOptions::default())?;
let map = runtime.create_map(MapOptions::default())?;
let session = map.attach_surface(surface_descriptor)?;

session.resize(Size::new(width, height), scale_factor)?;
map.set_style_url("https://demotiles.maplibre.org/style.json")?;
map.set_camera(Camera::new(lat, lon, zoom))?;

while let Some(event) = runtime.poll_event()? {
    match event {
        RuntimeEvent::Map { map_id, event: MapEvent::StyleLoaded { generation } }
            if map_id == map.id() && generation == map.style_generation() => {}
        RuntimeEvent::Map { map_id, event: MapEvent::Idle } if map_id == map.id() => break,
        RuntimeEvent::Error(error) => return Err(error.into()),
        _ => {}
    }
}

session.shutdown()?;
```

This sketch is not a final API. It illustrates the intended direction:
frameworks provide surfaces and events; the runtime owns map/render lifecycle;
style and resource loading are asynchronous runtime activity rather than
synchronous success from `set_style_url`.

Initial handle contracts:

| Handle | Thread model | `Send` / `Sync` intent | Notes |
| --- | --- | --- | --- |
| `RuntimeOwner` | Owns or is bound to the runtime thread | intentionally `!Send` and `!Sync` unless actor-owned | Pumps `RunLoop` and dispatches events. |
| `RuntimeHandle` | Command sender to runtime | `Send + Clone` if actor-backed | For host threads that cannot touch runtime-owned state directly. |
| `NativeMap` | Bound to one runtime owner or represented by a handle | initially `!Send` and `!Sync` unless actor-backed | State-changing calls must run on the owner or be marshalled. |
| `RenderSurface` | Bound to platform creation/render constraints | platform-specific | Must model resize, loss, recreation, and destruction. |
| `EventReceiver` | Adapter-owned dispatch target | adapter-specific | Receives plain-data events, never raw C++ references. |

Initial lifecycle states:

```text
Uninitialized -> SurfaceAttached -> Running -> SurfaceLost -> Running
       |                |              |             |
       +----------------+--------------+-------------+-> Destroyed
```

Only a subset of methods is valid in each state. Invalid calls should return an
immediate API error. Asynchronous fatal failures should transition the map to a
degraded or destroyed state and emit an event.

Before any public API stabilization, lifecycle states must be represented either
by split/typestate handles such as `NativeMap` and `SurfaceSession`, or by a
documented `state()` query plus non-exhaustive lifecycle errors.

### Future FFI Boundary

A stable shared C ABI is not part of the initial wrapper. However, a tiny
experimental C ABI smoke should be added early to validate opaque handles,
plain-data structs, explicit destroy functions, status codes, and queued events.
The preferred smoke consumer is a small Zig program using `@cImport`, because it
exercises the C header directly without a binding generator or large UI
framework.

The C ABI smoke is not a product adapter. It should stay narrow: create/destroy
runtime, create/destroy map, set size/style/camera, poll events, and optionally
render/read back a frame. If multiple non-Rust adapters later need the same
boundary, this smoke can evolve into a stable shared ABI.

Even before a shared C ABI exists, FFI shims must obey the basic invariants: no
Rust panics or C++ exceptions unwind across FFI, no borrowed Rust references are
held by host languages, and host callbacks are queued as owned data rather than
called directly from arbitrary native threads.

### Public API Stabilization Rules

- Do not publish stable crates before the native surface contract is accepted.
- If crates are published during exploration, document that `0.x` minor releases
  may break APIs and keep unstable modules clearly marked.
- Public Rust event and error enums should be `#[non_exhaustive]`.
- Public handles must deliberately opt into or out of `Send` and `Sync`.
- CPU-readback-only APIs must stay experimental or example-local until the
  native surface path proves whether they belong in the core API.
- Backend feature names and platform descriptors are public API and should be
  treated as semver-sensitive once published.
- The core runtime API should avoid `async fn` as a semantic foundation.
  Optional convenience crates may wrap queued events into futures later, but the
  core runtime and FFI shims should remain command/event based.

### Adapter Boundary

Adapters translate toolkit and language concepts into the Rust runtime.

Examples:

- JVM desktop adapter translates AWT/Swing/Compose lifecycle and native canvas
  handles into a runtime surface descriptor.
- Android adapter translates `Surface`, `SurfaceView`, `TextureView`, or
  `ANativeWindow` lifecycle into runtime surface callbacks.
- iOS adapter translates `UIView`, `CAMetalLayer`, `MTKView`, display link, app
  lifecycle, and memory warnings into runtime hooks.
- Rust UI adapters translate toolkit windows, frame clocks, resize events, and
  pointer events into runtime calls.

Adapters may be handwritten. The initial interactive rendering path should not
depend on UniFFI. UniFFI or Gobley may be useful later for simple data/control
surfaces, but rendering, native handles, thread affinity, and callbacks are
likely better served by explicit JNI or platform-specific shims.

Application SDK layers own gesture recognition. For MapLibre Compose, that means
Compose Multiplatform should translate pointer, touch, wheel, keyboard, and
platform gesture input into map movement commands. The runtime exposes the common
movements that gestures produce, plus direct camera mutation, projection, hit
testing, and feature query primitives.

Movement primitives should include:

- Pan by screen delta.
- Zoom or scale around a screen focus point.
- Rotate around a screen focus point.
- Pitch around a focus point or anchor.
- Move to, ease to, fly to, and jump to camera options where supported.
- Cancel in-progress transitions or animations.
- Convert between screen coordinates and geographic coordinates.

Camera ownership is split deliberately:

- The runtime owns the authoritative native camera used for rendering, tile
  selection, placement, projection, and feature queries.
- Application SDK adapters may own declarative camera state separately from
  presentation and reconcile that state into runtime camera commands.
- The Rust runtime should not impose a Compose, React, SwiftUI, or Qt-style state
  model.
- The runtime should expose faithful MapLibre Native camera primitives and camera
  observer events so adapters can build idiomatic controlled, uncontrolled,
  binding, or imperative APIs on top.

To make adapter reconciliation practical, camera events should include source or
reason metadata where MapLibre Native can provide it. The core runtime should not
invent precise request correlation for camera transitions unless MapLibre Native
provides it. Adapters that need operation IDs for coroutines, promises, or state
machines can layer them above the native event stream.

Camera observers are notification, not veto. The runtime should not call into an
adapter to ask whether each native camera movement is allowed. MapLibre Native
may mutate the camera internally during gestures, inertia, easing, or `fly_to`
animations. Adapters that want to reject or override movement should issue a new
command such as `cancel_transitions`, `set_camera`, `ease_to`, or `fly_to`.

Declarative adapters should model at least two camera concepts:

- Desired camera: what the application requested.
- Observed camera: what the native map is currently rendering.

During an animation these intentionally differ: the desired target may be fixed
while the observed camera changes every frame. The runtime should expose events
such as camera changed, transition started, transition finished, and transition
cancelled where those events can be faithfully derived from MapLibre Native
observer callbacks.

Adapters may build either controlled or uncontrolled APIs:

- Uncontrolled: the app provides initial camera state, the native map mutates it,
  and the adapter updates observed state from runtime events.
- Controlled: the app owns desired camera state, the adapter applies it to the
  runtime, and native camera changes are reported back so the app can accept,
  ignore, or override them with a later command.

Input should support two adapter modes over time, with the first mode preferred
for SDKs such as Compose Multiplatform:

- Adapter-recognized gestures: the adapter translates host gestures into camera
  mutations and query calls.
- Runtime-recognized pointer input: optional future helper mode where the adapter
  forwards normalized pointer, wheel, keyboard, touch, modifier, timestamp, and
  device-kind events.

The MVP should use adapter-recognized gestures. Runtime-recognized input should
not be part of the initial stable API and must not assume Compose, Android, iOS,
or browser gesture semantics.

### Kotlin Boundary

MapLibre Compose should keep its public `commonMain` API stable and continue to
hide platform details behind internal abstractions.

The Rust runtime should sit below Compose's platform `actual` implementation:

```text
MaplibreMap composable
        |
internal MapAdapter / ComposableMapView actual
        |
Kotlin JVM or Native bridge
        |
Rust runtime
        |
MapLibre Native Core
```

Compose should not expose Rust handles or Rust-specific lifecycle concepts in
its public API.

## Rendering Model

The runtime should support two rendering models over time.

### MVP: CPU Frame Readback

The first implementation may use the existing `maplibre-native-rs` style of
continuous/headless rendering, read pixels back to CPU memory, and upload or copy
them into the UI toolkit.

Benefits:

- Fastest path to proving Rust, build, style loading, camera updates, and
  adapter ergonomics.
- Useful for tests, screenshots, and some UI frameworks.
- Based on existing prior art in `maplibre-native-rs` and Slint examples.

Costs:

- Extra GPU to CPU to UI copy.
- Higher latency and CPU overhead.
- Not acceptable as the final high-performance path for all UI frameworks.

### Target: Native Surface Rendering

The target architecture should render directly into a native surface or
framework-provided render target where MapLibre Native has a valid backend
context.

This requires explicit modeling of:

- Surface creation and destruction.
- Surface resize and scale factor.
- Backend context activation/deactivation.
- Present/swap behavior.
- Render loop wakeups.
- Context loss and recreation.
- Teardown on the correct thread.

The design should follow MapLibre Native platform patterns: GLFW for neutral
desktop rendering, Android for surface loss and render-thread scheduling, and
iOS Metal for display-link and native layer integration.

The native surface contract is a gating design artifact, not a late
optimization. JNI and Compose APIs should not stabilize until one concrete
native surface path is proven.

The first native surface descriptor should be platform-specific and concrete,
probably matching the current Compose desktop/AWT path or a small GLFW-style
desktop spike. Generalize only after a second backend proves which fields are
shared.

Each surface integration must document ownership, valid threads, resize behavior,
present ownership, context/surface loss behavior, and teardown requirements.

Custom style layers are out of scope until the runtime has a safe
render-callback contract for backend context ownership, thread affinity, resource
lifetime, and adapter language callbacks.

## Native Lifecycle Requirements

MapLibre Native has strict scheduling expectations around `RunLoop`, renderer
frontends, backend activation, and object destruction. The Rust API should expose
those constraints without inventing a broad framework runtime.

Required invariants:

- A runtime-owned `mbgl::Map`/`NativeMap` handle is bound to one documented owner
  thread or executor.
- Calls crossing that boundary are marshalled.
- Rendering happens only while the backend context and surface are valid.
- Teardown happens on the required native thread/context.
- Host callbacks are queued as owned data, not invoked directly from arbitrary
  native threads.
- Adapters can forward memory-pressure signals such as low-memory warnings,
  backgrounding, or host cache pressure to the wrapper.

Style/resource operations are asynchronous. Methods such as `set_style_url`
validate input and enqueue or apply work; completion or failure is reported
through native observer-derived events. Initial events should mirror
`MapObserver` where possible, including `onDidFinishLoadingStyle`,
`onDidFinishLoadingMap`, `onDidFailLoadingMap`, `onDidBecomeIdle`, and
`onDidFinishRenderingFrame`.

The runtime may maintain generation counters such as `style_generation`,
`camera_generation`, and `surface_generation`. Generations are for stale-event
filtering and lifecycle reasoning. They are not exact native request IDs and
should not imply precise causality unless the underlying native API provides it.

Memory pressure should be represented as a wrapper signal, not an iOS-only
concern. The wrapper should support cache trimming, maximum in-flight frame
limits, CPU readback buffer reuse limits, and resource-cache size configuration
where native APIs support them.

### Renderer Lifecycle Contract

The runtime must map its public lifecycle to MapLibre Native internals rather
than treating render as a generic callback.

The contract should define:

- Which layer owns `RendererFrontend`, `RendererBackend`, `Renderable`, `Map`,
  and surface/context objects.
- Creation order for surface, backend, renderer, frontend, map, and observers.
- How the chosen frontend implementation coalesces `update(UpdateParameters)`,
  wakes the frame loop, preserves the reply-on-original-thread contract, and
  implements synchronous `reset()`.
- When backend/context activation begins and ends.
- Resize order across surface, backend renderable, map size, and frame invalidation.
- Pause/resume and memory-pressure behavior.
- Destruction order on the correct runtime/render thread.

### Run Loop and Scheduler

The runtime should support both owned and embedded scheduling:

- Owned mode: the runtime creates its own control/render threads and exposes a
  command/event API to adapters.
- Embedded mode: an adapter pumps the runtime from a host event loop or frame
  callback.

The design must specify how MapLibre `RunLoop` work is pumped, how timers and
native callbacks wake the loop, whether public calls are blocking or async, and
how shutdown drains queued commands, resource callbacks, and observer events.

Any thread that calls `FileSource::request` must own an active
`mbgl::util::RunLoop`; callbacks return on that same thread, and cancellation is
by dropping the returned `AsyncRequest`.

### Frame Scheduling Contract

Supported frame scheduling modes should be explicit:

- Runtime-driven continuous render loop.
- Framework-driven render callback.
- On-demand invalidation with adapter-provided frame clock.
- Headless/manual render step.

For each mode, the contract must define who requests the next frame, who owns
present, which thread renders, and how frame completion is reported.

## Resource Loading, Storage, and Cache Model

MapLibre Native resource behavior should be a first-class part of runtime
configuration, not only a mobile parity concern.

The wrapper should expose the native-backed resource options first:

- API key and tile server options.
- Cache path and maximum cache size.
- Asset path.
- Platform context where required by MapLibre Native.
- Resource transform hook through the native `FileSource` model.

Later wrapper or adapter policy may add online/offline mode, timeout/retry
configuration, explicit cache eviction hooks, richer request metadata, and
offline-region management, but those should be tied to specific MapLibre Native
APIs before being promised as core wrapper features.

Request transformation and resource callbacks must be queued through the runtime
dispatcher. They must not call directly into host languages from native worker
threads.

Offline packs and full offline-region management are not required for the first
desktop prototype, but constructor options should not preclude cache/database and
offline behavior later.

## Glyph, Sprite, and Image Loading

The runtime should distinguish:

- Glyph range resources.
- Sprite JSON and sprite image resources.
- Runtime style images.
- Missing style-image callbacks.

Style-image-missing fulfillment should have a clear protocol: immediate response,
deferred response tied to the missing image ID and style generation, or
next-frame update. Image metadata must include pixel ratio, SDF flag, optional
stretch/content metadata where supported, pixel format, stride, and ownership.

## Initial Build Policy

MapLibre Native validates exactly one graphics backend per build. The initial
build should focus on one host/backend first and avoid designing the whole
distribution matrix before the wrapper works.

Initial backend priority:

- macOS arm64: Metal.
- Linux amd64 OpenGL is the likely second target if macOS is not the best first
  development host.

The build system should support:

- Pinning a MapLibre Native Core release.
- Using local prebuilt headers/libraries for debugging.
- Keeping backend selection explicit.
- Not downloading native dependencies implicitly from `build.rs`.
- Building full MapLibre Native from source through explicit `xtask` or CI
  workflows unless direct `cargo build` source builds are intentionally supported.

Native dependency fetching must be explicit and reproducible. If downloads are
supported, they should be opt-in and pinned by version and checksum. Local
headers/libraries should be validated against the expected MapLibre Native
revision and backend where possible.

The `cxx` shim build must pin C++ standard, exception policy, RTTI policy, symbol
visibility, and include path layout. Generated `cxx` headers are internal
implementation details.

## Future Distribution

Release artifact layout, support tiers, debug symbols, license bundles, Android
AARs, iOS XCFrameworks, Windows DLL policy, Linux system dependencies, and Gradle
capabilities are deferred until the desktop prototype proves useful.

## Error Handling

The Rust API should distinguish immediate API errors from asynchronous runtime
failures.

Immediate `ApiError` categories should include:

- Build/backend unsupported.
- Native library loading or linking failure.
- Surface creation or invalid surface state.
- Renderer/backend/context failure.
- Invalid API call for current lifecycle state.
- Native exception or unknown C++ failure.
- Unsupported target, missing artifact, architecture mismatch, backend variant
  mismatch, missing native dependency, duplicate native library load, wrong
  thread, and surface owner violation.
- Invalid style-spec JSON, expression, filter, source, layer, or image
  definition.
- Duplicate or missing source, layer, or image IDs.
- Unsupported style source, layer, or property for the pinned native-core
  version.
- Style mutation invalid for the current style lifecycle state.

Asynchronous `RuntimeEvent::Error` categories should include:

- Style load failure.
- Resource/network failure.
- Tile parse or tile resource failure.
- Storage/cache failure.
- Render failure.
- Context loss.
- Callback dispatch failure.

Fatal runtime errors should move the affected map into a degraded or destroyed
state. Style events should carry style generation information where useful so
adapters can ignore stale completions and failures after style replacement.

Language adapters should translate these errors into idiomatic errors for their
host language without losing diagnostic detail.

## Event and Observer Model

The runtime should expose observer events that mirror native callbacks first:

| Runtime event | Native grounding |
| --- | --- |
| Camera will/is/did change | `MapObserver::onCameraWillChange`, `onCameraIsChanging`, `onCameraDidChange` with `CameraChangeMode::Immediate` or `Animated` |
| Style loaded | `MapObserver::onDidFinishLoadingStyle` |
| Map loading started/finished/failed | `onWillStartLoadingMap`, `onDidFinishLoadingMap`, `onDidFailLoadingMap` |
| Map idle | `onDidBecomeIdle` |
| Frame render started/finished | `onWillStartRenderingFrame`, `onDidFinishRenderingFrame` |
| Map render started/finished | `onWillStartRenderingMap`, `onDidFinishRenderingMap` |
| Style image missing | `onStyleImageMissing` |
| Glyph/sprite/tile events | glyph callbacks, sprite callbacks, `onTileAction` |
| Render error | `onRenderError` |

Aggregated errors such as `ResourceError` are wrapper-derived categories, not a
single core `MapObserver` callback.

Callbacks must be marshalled to adapter-owned dispatchers where necessary. The
runtime should avoid calling directly into JVM, Swift, Kotlin/Native, or JS from
arbitrary MapLibre Native threads.

The runtime should expose events through an explicit queue as the primary
semantic model. The event contract must define:

- Which thread may enqueue events and which thread may drain events.
- Ordering per map, per generation, and per runtime.
- Which events may be coalesced, such as camera changes and frame rendered.
- Which events must not be coalesced, such as errors, style-image-missing,
  surface lost, and destruction acknowledgements.
- Whether adapter calls made while draining events are allowed or rejected.
- What happens to pending events after style replacement, cancellation,
  surface loss, map destruction, or runtime shutdown.
- How backpressure is handled if a host language stops draining events.

Event payloads should include `MapId` and generation counters where relevant.
Request IDs should appear only for operations the wrapper explicitly models as
requests, such as snapshot/render callbacks or adapter-level async APIs.

## Async Semantics Without Async Rust

The core Rust runtime should faithfully expose MapLibre Native's imperative and
observer-driven pattern. It should not convert the model into `async fn` or a
specific executor-based abstraction.

Rules:

- Synchronous method return means the command was validated, applied, or enqueued
  according to that method's contract. It does not imply that rendering,
  loading, animation, or resource work has completed unless explicitly stated.
- Animation-starting commands such as `ease_to`, `fly_to`, animated movement,
  and transition cancellation are observed through camera/render events.
- Style, resource, glyph, sprite, tile, and render lifecycle progress is observed
  through queued runtime events.
- Blocking query operations may exist where they faithfully match native APIs,
  but names should make the blocking behavior explicit and cross-language
  adapters should avoid blocking UI threads.
- Language adapters may convert the event model into coroutines, flows, promises,
  callbacks, bindings, or futures appropriate to their ecosystem.

Each public operation should be classified before stabilization:

| Category | Meaning |
| --- | --- |
| Immediate | Completes synchronously and the result is final. |
| Command | Applies or enqueues a native command; later effects may produce events. |
| Snapshot | Returns last-known runtime state and may be stale relative to pending work. |
| Blocking query | Explicit synchronous query that may cross to the render/orchestration thread and block. |
| Request | Wrapper- or adapter-modeled async operation with explicit completion, failure, or cancellation events. Use only where this is intentionally modeled, not as a universal core concept. |
| Event stream | Produces many events over time, such as camera animation or resource loading. |

## Query API Contract

Query and projection APIs must define lifecycle and coordinate semantics:

- Query rendered features by point, box, or geometry with optional layer IDs and
  style-spec filter JSON.
- Query source features by source ID and source-layer with optional filter JSON.
- Define behavior before style load, during style reload, while tiles are
  pending, and after surface loss.
- Define screen coordinate space, physical pixels vs logical points, and scale
  factor.
- Return stable, serializable feature data suitable for JNI/Kotlin boundaries.
- Decide whether queries are explicit blocking queries, adapter-modeled async
  requests, or both.

## Cancellation and Shutdown

Cancellation is part of the API contract. Style loads, resource requests,
queries, snapshots, and surface sessions should have clear generation, callback,
or session semantics where needed.

Rules:

- Destroying a map cancels outstanding map-owned work.
- Replacing a style cancels or supersedes old style-scoped requests.
- Events after cancellation or destruction are either dropped or converted into
  documented terminal events.
- Shutdown waits for native teardown on the owning thread and drains or discards
  queued callbacks according to documented policy.

## Logging and Diagnostics

Native logging should bridge into Rust without calling host runtimes directly
from native threads. Diagnostics should include structured log level, subsystem,
map ID, generation, resource kind, backend, and native error detail where safe.

Resource tracing must redact URLs, headers, access tokens, and credentials by
default. Host adapters may opt into forwarding logs to platform logging systems.

## Style and Data APIs

The first API should cover a small set of features needed to prove MapLibre
Compose desktop integration:

- Load style by URL and JSON.
- Set camera and bounds.
- Query projection and visible region.
- Query rendered features.
- Add/remove runtime style images if needed by the prototype.
- Pass style-spec JSON through where MapLibre Native already supports it.

Do not attempt to bind all MapLibre style types at once. Add focused coverage as
adapter requirements demand it.

### Runtime Style API Contract

The compatibility target is the MapLibre style-spec JSON model, but MapLibre
Native does not expose one public C++ API for arbitrary JSON source/layer
mutation. The wrapper should stay faithful to the native APIs it actually uses.

Initial style/data API:

- Load style by URL and JSON.
- Get current style JSON where supported by `mbgl::style::Style`.
- Add/remove/update runtime style images where needed.
- Query rendered features.
- Use focused typed construction or explicit native conversion paths for sources,
  layers, filters, expressions, and properties as each feature is added.

Do not promise arbitrary style-spec JSON source/layer insertion until an
implementation deliberately uses and tests the relevant MapLibre Native style
conversion/parser internals or public APIs.

Compose parity candidates, not initial wrapper commitments:

- GeoJSON source mutation without full style reload.
- Ordered layer insertion and movement.
- Layer paint/layout/filter mutation.
- Expression and filter JSON conversion.
- Feature state.
- Style serialization tests for runtime mutations.
- Global and per-property transitions.
- Light, terrain, sky, fog, projection, and DEM/raster-dem dependencies.
- Custom layers.
- Native annotation subsystem.

Annotation APIs should not be part of the initial Rust runtime. Compose or
platform adapters can implement annotations as higher-level conveniences over
GeoJSON sources, images, layers, feature queries, and feature state.

Prefer the narrowest API that faithfully maps to MapLibre Native. Add typed Rust
APIs when they improve safety for high-frequency operations or hide native
lifetime/threading hazards. Add JSON pass-through only where the native API path
is explicit and tested.

Initial Compose parity inventory:

| Compose need | Runtime primitive | Priority |
| --- | --- | --- |
| Style URL/JSON | set style, style generation, style loaded/error events | High |
| Camera state | set/get camera, camera changed events | High |
| Resize/layout | surface resize, scale factor, render invalidation | High |
| Visible region | projection/query visible bounds | Medium |
| Click handling | adapter gesture, projection, feature query | Medium |
| Query rendered features | feature query on runtime/render owner | Medium |
| Style images | add/remove image, image missing event | Medium |
| GeoJSON overlays | add/remove source and common layers | Medium |
| Declarative style reconciliation | exists/get/add/update/remove/move primitives with stable IDs | High |
| Layer ordering | add before, move before/above/below | High |
| Runtime property mutation | set paint/layout/filter/source data without style reload | High |
| Feature state | set/get/remove feature state for source/source-layer/id | Medium |
| Snapshots | CPU readback or explicit snapshot API | Low |

## Safety Principles

- Prefer opaque Rust-owned handles over exposed raw pointers.
- Keep unsafe code concentrated in bridge modules.
- Make lifecycle states explicit where possible.
- Do not rely on Rust `Drop` alone for operations that must run on a specific
  thread or inside an active graphics context.
- Avoid long-lived borrowed references across FFI boundaries.
- Use copyable plain data types at language boundaries.
- Validate size, scale, and surface state before rendering.
- Treat callbacks as asynchronous messages.

## Open Questions

- Phase 1 must decide whether the first crate directly depends on
  `maplibre_native`, forks selected code from it, or ports only its build/bridge
  ideas.
- Phase 4 must prove the minimal native surface abstraction before Compose uses
  the JNI adapter.
- Phase 7 and Phase 8 must turn the Compose parity inventory into tracked issues
  or tests.
- Future Android and iOS work must inventory SDK replacement risks before
  proposing production migration.
- Future distribution work must define artifact versioning relative to MapLibre
  Native Core and MapLibre Compose releases.
