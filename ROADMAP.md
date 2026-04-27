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
- `/Users/sargunv/.local/share/mise/installs/cmake/3.31.6/cmake-3.31.6-macos-universal/CMake.app/Contents/bin/cmake --build build`
  builds `maplibre_native_abi` on macOS.
- `zig build run` prints the ABI version and completes the runtime lifecycle
  smoke.

Out of scope:

- `mln_map`.
- Rendering.
- Map events.

## M2: Headless Map Lifecycle Smoke

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
- Handle-scoped diagnostics for runtime and map failures.
- Zig CLI lifecycle smoke.

Acceptance:

- Zig creates a runtime and map, loads a local inline style JSON, issues camera
  commands, drains events, and destroys everything cleanly.
- The smoke style contains visible content suitable for later rendering checks,
  such as a background plus a small inline GeoJSON source/layer rather than a
  blank black background.
- Style parse/load failures produce status codes, map events, and diagnostics.
- Wrong-thread and invalid-lifecycle calls return documented statuses.

Confidence: This milestone proves ABI shape and map/event plumbing. It does not
prove GPU rendering, texture synchronization, or UI integration.

Out of scope:

- Texture sessions.
- Native surfaces.
- Interactive windowed UI.

## M3: ABI Contracts and Test Harness

Goal: Make the C boundary reliable enough to extend without accumulating
undefined behavior.

Deliverables:

- Per-function documentation for possible `mln_status` returns.
- Per-function async category and valid calling-thread documentation.
- Diagnostic APIs for thread-local and handle-scoped errors.
- Runtime-returned string/buffer release APIs.
- ABI tests for struct sizes, null handling, lifecycle state, ownership, and
  event draining.

Acceptance:

- Tests cover invalid arguments, stale handles where feasible, wrong lifecycle
  state, and diagnostics ownership.
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
