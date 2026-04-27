# Roadmap

## Current Objective

Prove a small, safe C ABI over MapLibre Native that can create interactive maps,
render them into GPU textures, and be consumed from non-C++ languages without
leaking C++ ownership, exceptions, or threading assumptions.

The design reference is `DESIGN.md`.

## M0: Repository and Core Source Completed

Goal: Establish a reproducible native build against a known MapLibre Native
source revision.

Deliverables:

- Add `third_party/maplibre-native` as a pinned git submodule.
- Support `MLN_SOURCE_DIR` for working against a sibling checkout.
- Build a minimal C++ wrapper library against MapLibre Native on the first
  target platform.
- Export at least one C symbol from the wrapper library.

Acceptance:

- Fresh checkout can initialize the submodule and configure the wrapper build.
- Local `~/Code/maplibre-native` can be used via `MLN_SOURCE_DIR`.
- Build does not implicitly download native dependencies.

Acceptance evidence:

- `third_party/maplibre-native` is pinned as a git submodule at
  `2489914ec28a196a7979bf4fafa10f87c9c99cb8`.
- `cmake -S . -B build` configures MapLibre Native through `add_subdirectory`,
  using `MLN_SOURCE_DIR` when provided and the submodule by default.
- `cmake --build build` builds `maplibre_native_abi` linked against MapLibre
  Native `mbgl-core` with the Metal backend on macOS.
- `zig build run` exercises exported ABI symbols through the shared library.

Out of scope:

- Published artifacts.
- Android AARs or iOS XCFrameworks.
- Language bindings beyond the C header.

## M1: Minimal C ABI Skeleton Completed

Goal: Create the public C boundary and prove it is consumable outside C++.

Deliverables:

- `include/maplibre_native_abi.h` with opaque handles, `mln_status`, exported
  function decoration, and versioned option structs.
- `mln_runtime_create` and `mln_runtime_destroy`.
- Thread-local diagnostics for failures before a valid handle exists.
- Basic handle validation and no-exception boundary in `src/abi`.
- Zig CLI smoke that imports the public header with `@cImport`.

Acceptance:

- Zig can compile against the installed or build-tree C header.
- Runtime create/destroy works from Zig.
- Invalid arguments return documented `mln_status` values.
- C++ exceptions do not cross the C ABI.

Acceptance evidence:

- `include/maplibre_native_abi.h` defines the public C boundary with exported
  function decoration, opaque `mln_runtime`, `mln_status`, and versioned
  `mln_runtime_options` with an ABI-provided default initializer.
- `mln_runtime_create` and `mln_runtime_destroy` validate arguments, convert C++
  exceptions to `MLN_STATUS_NATIVE_ERROR`, preserve thread-local diagnostics,
  and reject non-live runtime handles.
- `mln_runtime_destroy` enforces owner-thread destruction for the runtime
  handle.
- `examples/zig-headless/main.zig` imports the public header with `@cImport`,
  calls `mln_runtime_create`/`mln_runtime_destroy`, and verifies a documented
  invalid argument result.
- `cmake --build build` builds `maplibre_native_abi` on macOS.
- `zig build run` prints the ABI version and completes the runtime lifecycle
  smoke.

Out of scope:

- `mln_map`.
- Rendering.
- Map events.

## M2: Headless Map Lifecycle Smoke Completed

Goal: Prove map lifecycle, style loading, camera control, and map-owned event
plumbing before introducing GPU texture ownership.

Deliverables:

- `mln_map_create` and `mln_map_destroy`.
- Internal ownership for `mbgl::Map`, `RunLoop`, `MapObserver`, and
  `RendererFrontend`.
- Style URL/JSON load functions.
- Camera snapshot and basic camera commands.
- Map-owned event queue with style, camera, render-invalidated, and error events
  where available.
- Thread-local diagnostics for synchronous runtime and map failures.
- Zig CLI lifecycle smoke.

Acceptance:

- Zig creates a runtime and map, loads a local inline style JSON, issues camera
  commands, drains events, and destroys everything cleanly.
- The smoke style contains visible content suitable for later rendering checks,
  such as a background plus a small inline GeoJSON source/layer rather than a
  blank black background.
- Style parse/load failures produce status codes, map events, and diagnostics.
- Wrong-thread and invalid-lifecycle calls return documented statuses.

Acceptance evidence:

- `include/maplibre_native_abi.h` now exposes opaque `mln_map`, map options,
  camera options, event structs, style load, camera command/snapshot, event
  polling, and run-loop pumping. Synchronous failures use thread-local
  diagnostics; async/observer failures use map events.
- `src/core/map.cpp` constructs a real `mbgl::Map` with an owner-thread
  `mbgl::util::RunLoop`, `MapObserver` event bridge, and headless
  `RendererFrontend` that queues render-invalidated events without owning GPU
  resources.
- `examples/zig-headless/main.zig` creates a runtime and map, loads a visible
  inline style JSON with a background and inline GeoJSON circle layer, issues
  camera commands, drains map events, and destroys handles cleanly.
- `tests/abi/main.zig` covers intentional failure cases and lifecycle contracts:
  invalid runtime arguments, runtime destruction while maps are live, stale map
  handles, inline style success, camera snapshot fields, and malformed style
  status/diagnostic/event behavior.
- `cmake --build build` builds `maplibre_native_abi` on macOS.
- `zig build test --summary all` reports 4/4 ABI tests passed.
- `zig build run` prints ABI/map events and completes the headless map lifecycle
  demo without deliberate failure calls.

Follow-up:

- The M2.1 resource slice replaced the temporary null `FileSourceManager` and
  resolved local style URL loading for file, asset, and custom schemes.

Confidence: This milestone proves ABI shape and map/event plumbing. It does not
prove GPU rendering, texture synchronization, or UI integration.

Out of scope:

- Texture sessions.
- Native surfaces.
- Interactive windowed UI.

## M2.x: Resource Loading, Cache, and Offline

Goal: Build the resource subsystem in the final ABI shape: a runtime-owned,
wrapper-composite resource loader that supports local files, bundled resources,
custom URL schemes, network loading, ambient cache, and offline data without
requiring callers to reimplement MapLibre's normal resource stack.

Design reference: `RESOURCE_API_DESIGN.md`.

Shared architecture:

- Replace the M2 null `FileSourceManager` with an ABI-owned manager and
  composite `ResourceLoader`.
- Keep resource configuration runtime-level and pass runtime-derived
  `mbgl::ResourceOptions` into map creation.
- Treat M2.x boundaries as provider enablement slices, not as throwaway loader
  designs.
- Keep custom providers additive for host-specific schemes such as `apk://`,
  `bundle://`, or app-specific package URLs.
- Add each built-in provider behind explicit ABI options and deterministic
  tests.

### M2.1: Composite Loader, Local Files, and Custom Schemes Completed

Deliverables:

- ABI-owned composite `ResourceLoader` skeleton with cancellation-safe requests.
- Runtime `asset_path` and optional `cache_path` options.
- `file://` and filesystem `asset://` loading.
- Custom scheme provider registration and dispatch.
- Tests for successful `file://`, successful `asset://`, successful custom
  scheme, missing-resource failure, invalid provider registration, and existing
  inline-style behavior.
- Zig headless smoke loads a visible local style by URL.

Acceptance evidence:

- `FileSourceManager::get()` now returns an ABI-owned manager with a single
  `FileSourceType::ResourceLoader` factory that constructs the wrapper composite
  loader.
- Map creation passes runtime-derived `ResourceOptions` with runtime-unique
  `platformContext`, `asset_path`, and reserved `cache_path` into MapLibre.
- The composite loader dispatches `file://` to MapLibre's default local file
  source, `asset://` to MapLibre's default asset file source, registered custom
  schemes to runtime-owned ABI callbacks, and unsupported URLs to resource
  errors.
- `include/maplibre_native_abi.h` exposes runtime resource options and
  `mln_runtime_register_resource_provider`; registration rejects invalid,
  reserved, duplicate, null-callback, and post-map providers.
- `tests/abi/resources.zig` covers successful `file://`, successful `asset://`,
  successful custom scheme, missing file failure, invalid provider registration,
  and post-map registration rejection. Existing inline style tests continue to
  pass.
- `examples/zig-headless/main.zig` writes a visible local style and loads it by
  `file://` URL through the C ABI.
- `cmake --build build --target maplibre_native_abi --parallel 4` builds the
  wrapper library.
- `zig build test --summary all` reports 34/34 ABI tests passed.
- `zig build run` completes the headless local-style URL smoke.

Out of scope:

- Built-in network, ambient cache, offline regions, MBTiles/PMTiles, and
  resource transforms.

### M2.2: Built-In Network Provider Completed

Deliverables:

- MapLibre-backed HTTP/HTTPS provider wiring for the first target platform.
- Process-global network status APIs wrapping `mbgl::NetworkStatus`.
- Tile-server/API-key options only as needed for deterministic tests.
- Tests against a controlled local HTTP fixture or equivalent non-flaky
  endpoint.

Research required:

- Default libcurl vs Darwin `NSURLSession` source selection and CMake inputs.
- `ClientOptions`, process-global network status, resource transform, and
  platform lifecycle requirements.

Acceptance evidence:

- The ABI composite loader now includes MapLibre's `OnlineFileSource` and
  dispatches `http://` and `https://` resources through it.
- The Apple target wires Darwin `HTTPFileSource` (`NSURLSession`) plus
  `native_apple_interface.m`, and avoids the default curl HTTP source.
- `include/maplibre_native_abi.h` exposes process-global
  `mln_network_status_get` and `mln_network_status_set` wrappers over
  `mbgl::NetworkStatus`.
- `tests/abi/resources.zig` covers invalid network status arguments,
  online/offline round-tripping, and loading a style from a deterministic local
  `http://127.0.0.1` fixture server.
- `zig build test --summary all` reports 36/36 ABI tests passed.

Remaining risk:

- HTTPS dispatch uses the same MapLibre `OnlineFileSource` path as HTTP, but
  M2.2 does not add a local HTTPS fixture. Deterministic HTTPS coverage would
  require certificate/trust setup that is not worth adding for this slice.

### M2.3: Ambient Cache Provider Completed

Deliverables:

- Runtime cache path, cache enablement, and maximum cache size options.
- MapLibre-backed database/cache provider wiring.
- Tests proving cached data can satisfy a later request without network.

Acceptance evidence:

- `mln_runtime_options` now exposes `maximum_cache_size` behind
  `MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE`, and runtime resource options forward
  both `cache_path` and maximum cache size.
- The ABI composite loader includes MapLibre's `DatabaseFileSource` when
  `cache_path` is configured, routes cache-capable requests through the database
  before network, and forwards successful network responses into the database.
- `mln_runtime_run_ambient_cache_operation` wraps reset database, pack database,
  invalidate ambient cache, and clear ambient cache with status-returning ABI
  semantics. Runtimes without `cache_path` use MapLibre's default in-memory
  database behavior rather than wrapper-invented rejection.
- `tests/abi/resources.zig` covers default in-memory cache operation behavior,
  invalid ambient cache operation arguments, maximum-cache-size runtime
  creation, successful database maintenance operations, and cached HTTP style
  loading after the first online load.
- `zig build test --summary all` reports 39/39 ABI tests passed.

Remaining risk:

- Headless style-load tests prove style resources are cached; tile/sprite/glyph
  cache behavior will get stronger coverage once M4 adds a render target that
  forces full source/tile loading.

### M2.4: Offline Regions and Offline Database APIs Deferred

Deliverables:

- ABI operations for supported MapLibre offline region lifecycle and status.
- Offline observer/event plumbing.
- Deterministic offline tests using controlled resources.

Deferred scope:

- The shared `DatabaseFileSource` prerequisite is now present for cache-backed
  runtimes, but public offline region APIs are not exposed yet.
- Region handles, metadata buffer ownership, observer/event delivery, download
  state changes, status polling, merge/delete/invalidate semantics, and
  deterministic offline download tests need a separate ABI design pass.
- Do not mark M2.4 complete until those ownership and event semantics are
  implemented and tested.

### M2.5: MBTiles/PMTiles Provider Wiring Completed; Render Validation Deferred

Deliverables:

- Built-in local package provider support for MBTiles and PMTiles.
- URL tests for `mbtiles://` and/or `pmtiles://` fixtures.

Acceptance evidence:

- `FileSourceManager::get()` registers MBTiles and PMTiles file-source factories
  in addition to the ABI composite resource loader.
- The composite loader routes `mbtiles://` and `pmtiles://` through MapLibre's
  built-in providers before local file, database, network, and custom-provider
  fallback.
- The wrapper build now includes MapLibre's default MBTiles provider and the
  full PMTiles provider source by forcing `MLN_WITH_PMTILES=ON`, with SQLite,
  zlib, and PMTiles vendor include/link wiring.
- `cmake --build build --target maplibre_native_abi --parallel 4` builds the
  full PMTiles provider on macOS.

Deferred validation:

- Current headless ABI tests cannot force tile package requests without a render
  target. M4 texture rendering should add fixture styles that render MBTiles and
  PMTiles tiles visibly before treating package URL rendering as fully proven.

### M2.6: Resource Transform and Request Interception Completed

Deliverables:

- Request/header/auth/URL transform ABI only if required by a concrete product
  use case.
- Clear callback threading and memory ownership contract.

Acceptance evidence:

- `include/maplibre_native_abi.h` exposes `mln_resource_transform` and
  `mln_runtime_set_resource_transform` as URL-only network rewrite APIs matching
  MapLibre's `ResourceTransform` semantics.
- Transform registration is runtime-scoped, rejected after map creation, and
  forwarded to `OnlineFileSource` by the ABI composite.
- `tests/abi/resources.zig` verifies a fake HTTP style URL is rewritten to a
  deterministic local HTTP fixture and that post-map transform registration is
  rejected.
- `zig build test --summary all` reports 39/39 ABI tests passed.

Remaining risk:

- The ABI intentionally does not expose header/body/auth mutation because
  MapLibre's native `ResourceTransform` only rewrites URLs.

### M2.7: Async Custom Resource Providers

Goal: Revisit resource-provider and database-maintenance async semantics so the
ABI can represent MapLibre Native's asynchronous `FileSource` and
`DatabaseFileSource` contracts as directly as a safe C ABI allows.

Motivation:

- The current custom provider path is adequate for bundled styles, sprites,
  glyph chunks, and small local resources, but the host callback must return
  bytes before returning.
- MapLibre's outer `FileSource::request` model is asynchronous and cancellable;
  an async custom-provider ABI would be the general shape, with synchronous
  providers becoming the inline-completion case.
- Native `FileSource` providers can complete later, retain request state,
  observe cancellation, and handle byte ranges. The C ABI custom provider should
  avoid inventing narrower restrictions beyond what is needed to make those
  semantics safe across FFI.
- Larger resources, host async file APIs, IPC, authentication refresh, ranged
  package reads, and slow non-HTTP sources need explicit request handles,
  completion, cancellation, range metadata, and lifetime rules.
- MapLibre ambient cache and offline database maintenance APIs are also
  callback-based asynchronous operations; the current status-returning ABI
  blocks until completion and should be reviewed before offline region APIs are
  added.

Deliverables:

- Design the async provider ABI shape and decide whether it replaces or
  complements the synchronous provider API.
- Define request handle ownership, completion threading, cancellation, runtime
  teardown behavior, and reentrancy rules.
- Explore whether the ABI composite should become a thin custom-scheme wrapper
  around MapLibre's native `MainResourceLoader` instead of mirroring its
  waterfall, so built-in provider dispatch stays closer to upstream behavior.
- If `MainResourceLoader` delegation is not viable, document the concrete reason
  and keep the wrapper composite's mirrored waterfall as an intentional
  divergence.
- Decide whether ambient cache maintenance should remain blocking or move to the
  same request/event completion model planned for offline database APIs.
- Include delayed completion and whole-resource responses in the first async
  provider design.
- Decide whether byte ranges are implemented in the first async provider slice
  or explicitly split into the immediately following slice; do not leave the
  final ABI shape incompatible with MapLibre range requests.
- Add deterministic tests for inline completion, delayed completion,
  cancellation, range metadata behavior, and provider error completion.

Out of scope:

- Designing the full API in this roadmap entry.
- HTTP header/body/auth mutation unless it is deliberately selected as a
  separate request-interception feature.

## M3: ABI Contracts and Test Harness

Goal: Make the C boundary reliable enough to extend without accumulating
undefined behavior.

Deliverables:

- Per-function documentation for possible `mln_status` returns.
- Per-function async category and valid calling-thread documentation.
- Diagnostic API contract for thread-local synchronous errors.
- Runtime-returned string/buffer release APIs.
- ABI tests for struct sizes, null handling, lifecycle state, ownership, and
  event draining.

Acceptance:

- Tests cover invalid arguments, stale handles where feasible, wrong lifecycle
  state, and thread-local diagnostics behavior.
- Public APIs document whether completion is represented by return status, later
  map events, or both.
- No returned pointer has undocumented lifetime.

Out of scope:

- Broad style/data mutation.
- Backend-specific texture contracts.

## M4: Metal Texture Session

Goal: Prove the primary rendering model on Apple platforms by rendering MapLibre
into an offscreen Metal texture and sampling it from a host renderer.

Deliverables:

- `mln_texture_attach`, `mln_texture_resize`, `mln_texture_render`,
  `mln_texture_acquire_frame`, `mln_texture_release_frame`,
  `mln_texture_detach`, and `mln_texture_destroy` for Metal.
- Metal texture descriptor and frame structs.
- Documented Metal ownership and synchronization contract.
- Render target generation handling across resize/detach.
- Minimal host renderer that samples the acquired `MTLTexture`.

Acceptance:

- A visible local style renders into an offscreen `MTLTexture`.
- Host Metal code samples the texture into a window.
- Resize produces a new generation without use-after-free.
- Acquire/release prevents stale texture use according to documented rules.
- Detach and destroy release backend-bound resources on the correct thread.

Out of scope:

- Vulkan.
- Compose, DVUI, or other full UI toolkit integration.
- CPU readback as a product path.

## M5: Interactive Zig Map Example

Goal: Use Zig as a real non-C++ ABI consumer for an interactive texture-rendered
map.

Deliverables:

- Zig example that imports the C header with `@cImport`.
- Window/input shell using SDL3 or a tiny native host.
- Pointer drag, scroll, and keyboard controls translated into ABI camera
  commands.
- Event drain loop.
- Metal texture-session rendering and sampling on macOS.

Acceptance:

- User can pan, zoom, rotate, and pitch the map interactively.
- The example uses C ABI calls only; it does not call C++ directly.
- Rendering remains texture-session based, not direct native-surface rendering.
- Lifecycle, events, and diagnostics remain visible enough to debug failures.

Out of scope:

- Product-quality Zig SDK.
- DVUI integration.
- Compose integration.

## M5.5: Offline Region ABI

Goal: Expose MapLibre offline region management after the interactive/rendered
map path can validate real tile/source downloads and package-backed rendering.

Deliverables:

- Public ABI handles and ownership rules for offline regions.
- Metadata buffer ownership and release APIs.
- APIs to list, get, create, update metadata, set download state, get status,
  merge, delete, and invalidate offline regions through the shared
  `DatabaseFileSource`.
- Offline observer/event plumbing using the existing map/runtime event model
  rather than direct arbitrary-thread host callbacks.
- Deterministic tests using controlled local HTTP resources and rendered map
  requests, including download progress and offline reload behavior.

Acceptance:

- A controlled style/source can be downloaded into an offline region and later
  loaded without network when backed by persistent storage; in-memory database
  behavior is allowed when MapLibre supports it but documented as non-durable.
- Region status and observer events are delivered through documented ABI-owned
  storage and lifetime rules.
- Deleting or invalidating a region has observable, tested behavior without
  corrupting ambient cache resources.

Out of scope:

- Product-level online/offline mode policy.
- Custom eviction hooks or wrapper-invented cache policies.
- Header/body/auth request interception beyond MapLibre's URL-only resource
  transform.

## M6: Style and Data Mutation Slice

Goal: Expose enough style/data APIs to support representative dynamic map UI
without building a generated style SDK in C.

Deliverables:

- Style URL/JSON load and readback.
- Runtime image add/get/remove.
- Source/layer existence queries and removal by ID.
- JSON-first source/layer insertion.
- Generic layer property get/set by style-spec property name using typed
  `mln_value` payloads.
- GeoJSON source helper for URL and inline GeoJSON.

Acceptance:

- Example can add/update/remove visible GeoJSON data without full style reload.
- Example can add a layer and update paint/layout properties through generic
  property APIs.
- Invalid style JSON, source JSON, layer JSON, or property values produce
  documented statuses and diagnostics.

Out of scope:

- Exhaustive generated C setters for every layer property.
- Custom expression function registration.
- Native annotation API exposure unless deliberately selected later.

## M7: Vulkan Texture Session

Goal: Validate that the texture-session model generalizes beyond Metal.

Deliverables:

- Vulkan texture descriptor and frame structs.
- Vulkan attach/resize/render/acquire/release/detach/destroy implementation.
- Documented Vulkan ownership and synchronization contract: device/queue, image
  ownership, layouts, semaphores/fences, and in-flight frames.
- Minimal host renderer that samples the produced Vulkan image.

Acceptance:

- A visible local style renders through the Vulkan texture path.
- Host Vulkan code samples the produced image correctly.
- Metal-specific assumptions are either removed from the common ABI or
  documented as backend-specific.

Out of scope:

- OpenGL-first rendering.
- WebGPU ABI.

## M8: Non-Compose UI Validation

Goal: Test the architecture against a real non-Compose UI toolkit before
designing Compose-specific bindings.

Deliverables:

- DVUI-based Zig experiment that embeds the map texture inside a real UI.
- Adapter code that owns gestures, layout, toolkit lifecycle, and declarative UI
  state outside the C ABI.
- Notes on ABI gaps versus adapter-only conveniences.

Acceptance:

- DVUI can display and interact with the map through the same texture-session
  ABI.
- Any required ABI changes are general-purpose, not DVUI-specific.
- The experiment confirms whether the ABI is usable by a toolkit that is not
  Compose-shaped.

Out of scope:

- Product DVUI SDK.
- Compose API design.

## Later

- JNI/Kotlin and MapLibre Compose adapter.
- Swift/ObjC shim.
- Kotlin/Native binding.
- Rust binding over the C ABI.
- Native-surface fallback path.
- Offline-region API surface.
- Android and iOS packaging.
- Published binary artifacts.
- WebGPU research after Metal and Vulkan are proven.
- OpenGL compatibility fallback only for a concrete host or platform need.

## Not Yet

- Do not add JNI, Compose, Swift, Rust, Flutter, React Native, or DVUI adapters
  to the core before the C ABI and texture-session contract are proven.
- Do not generate exhaustive per-layer C setters unless the core ABI
  deliberately becomes a generated style SDK.
- Do not make CPU readback a product rendering path.
- Do not build around WebGPU or OpenGL first.
- Do not introduce a wrapper-owned futures/coroutines/promises model at the C
  boundary.
