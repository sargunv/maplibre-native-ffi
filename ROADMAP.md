# Roadmap

## Current Objective

Prove a small, safe C ABI over MapLibre Native that can create interactive maps,
render them into GPU textures, and be consumed from non-C++ languages without
leaking C++ ownership, exceptions, or threading assumptions.

The design reference is `DESIGN.md`.

## Completed Foundation: M0-M3

The repository now builds a small C ABI wrapper against pinned MapLibre Native
sources, with a Zig consumer proving the public header is usable outside C++.
The implemented foundation includes:

- Runtime and map lifecycle with opaque handles, owner-thread validation,
  no-exception ABI boundaries, and thread-local diagnostics.
- Style URL/JSON loading, camera commands/snapshots, owner-thread run-loop
  pumping, map-owned event queues, and a headless Zig smoke example.
- Runtime-scoped resource configuration using MapLibre's native
  `MainResourceLoader`, built-in local/network/cache/package providers, URL-only
  resource transforms, process-global network status wrappers, and an async
  custom resource provider ABI.
- ABI contract tests for invalid arguments, stale handles, lifecycle state,
  wrong-thread calls, diagnostics, event draining/copying, resource request
  cancellation, double completion, and cross-thread provider completion.

Useful deferred items from the foundation:

- Public offline-region APIs are deferred until after rendered map requests can
  validate real tile/source downloads.
- MBTiles/PMTiles registration is present, but visible package rendering remains
  deferred to M4 because headless tests cannot force tile package requests.
- Custom provider range metadata tests are deferred until a direct resource
  harness or render-driven PMTiles path can force range requests through the
  public ABI.
- ABI-owned string/buffer release APIs are deferred until the first public API
  returns owned ABI storage.
- Golden struct layout tests are deferred while `mln_abi_version() == 0`.

Current validation: `mise run fix` and `mise run test` pass with `48/48` ABI
tests.

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
