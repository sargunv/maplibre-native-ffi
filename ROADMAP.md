# Roadmap: From Zero to MapLibre Compose Integration

This roadmap describes execution order for the Rust UI runtime described in
[DESIGN.md](DESIGN.md). It intentionally avoids repeating architectural details;
read the design first.

## Phase 0: Repository Foundation

- Create the Rust workspace layout from [DESIGN.md](DESIGN.md#proposed-workspace-structure).
- Add licenses and attribution for any code or build logic adapted from
  `maplibre-native-rs`.
- Add `rustfmt`, `clippy`, minimal CI, and a local `just` or `xtask` workflow.
- Add placeholder crates for low-level core bridge, runtime, and JNI/demo
  adapters.
- Decide the initial crate names before publishing anything public.
- Document whether early crates remain unpublished/path-only or use experimental
  `0.x` compatibility rules.

Exit criteria:

- Workspace builds with empty or stub crates.
- CI runs formatting and a no-op build.
- `README.md`, `DESIGN.md`, and this roadmap explain the project purpose.
- No public artifact is published without an explicit experimental/stability
  policy.

## Phase 1: Native Core Build and Link

- Port or reimplement the minimum MapLibre Native Core build/link path inspired
  by `maplibre-native-rs`.
- Support one host/backend first, preferably macOS arm64 Metal or Linux amd64
  OpenGL.
- Support local overrides for MapLibre Native headers and libraries.
- Add a pinned MapLibre Native Core version.
- Add a trivial native smoke test that links the library and constructs a
  minimal bridge object.
- Implement the initial build policy from
  [DESIGN.md](DESIGN.md#initial-build-policy): one explicit backend, no implicit
  network downloads, local override validation, and clear build modes.
- Prove native logging capture, `RunLoop` construction/destruction,
  resource-options construction, and backend feature validation.
- Add a negative CI/build check for conflicting backend selections.

Exit criteria:

- `cargo build` links against MapLibre Native Core on one platform.
- CI or local documented workflow proves the selected platform/backend.
- Build failures produce actionable diagnostics.
- Source-build and prebuilt-consumption paths are both documented, even if only
  one is automated initially.

## Phase 2: Minimal Runtime Object Model

- Add the first low-level `cxx` bridge for map/runtime construction, including a
  `RunLoop`, `MapObserver` shim, minimal `RendererFrontend` shim,
  `ResourceOptions`/`ClientOptions`, and then `mbgl::Map` construction.
- Add Rust types for runtime options, map options, size, camera, backend, and
  errors.
- Add explicit lifecycle methods for create, resize, set style, set
  camera, poll events, and destroy.
- Classify every initial operation using
  [DESIGN.md](DESIGN.md#async-semantics-without-async-rust): immediate, command,
  snapshot, blocking query, request, or event stream.
- Add native-backed resource options: API key/tile server options, asset path,
  cache path, maximum cache size, platform context where needed, and resource
  transform hook shape.
- Keep the public Rust API small and unstable.

Exit criteria:

- A Rust example can create a map, set a style, pump events until style load
  or error, set a camera, and shut down without leaks or crashes.
- The example can be run repeatedly in one process.
- Errors are returned as typed Rust errors instead of panics where practical.
- Repeated style URL changes, invalid style URL, local style JSON, resource
  failure, event queue drain on destroy, and no-callbacks-after-destroy behavior
  are covered by tests or scripted checks.
- `RendererFrontend::reset()` destroys renderer state synchronously and observer
  callbacks are marshalled rather than called directly into host code.

## Phase 3: CPU Readback UI Smoke

- Implement the MVP rendering path described in
  [DESIGN.md](DESIGN.md#mvp-cpu-frame-readback).
- Add a simple desktop example that displays frames in a window or UI toolkit.
- Drive a basic render loop and resize handling.
- Add camera movement from explicit adapter-owned controls using runtime movement
  primitives such as pan, zoom around focus, rotate around focus, pitch, and
  direct camera mutation.
- Add observer events for style loaded, loading failure, camera changed, and
  frame rendered.
- Keep any CPU-frame-buffer APIs under an experimental/example-only surface.
- Represent native-surface requirements as pending or failing contract tests
  before Phase 4 begins.

Exit criteria:

- A user can open an interactive map window from a Rust example.
- Style loading, camera changes, resize, and shutdown work reliably on the first
  supported platform/backend.
- The example documents that the CPU readback path is an MVP, not the final
  performance target.
- CPU readback uses the same lifecycle, invalidation, resize, and event contracts
  expected by native surfaces.

## Phase 4: Native Surface Contract Spike

- Implement the target rendering model from
  [DESIGN.md](DESIGN.md#target-native-surface-rendering) on one platform.
- Define and test the first real `SurfaceDescriptor` shape.
- Model surface creation, resize, present, pause/resume, context loss, and
  teardown explicitly in the runtime.
- Compare latency, CPU usage, and memory use against the CPU readback path.
- Keep the API unstable until this phase validates or changes the runtime
  lifecycle model.

Exit criteria:

- At least one desktop backend renders directly to a native surface.
- The runtime API models surface created/resized/lost/destroyed states.
- Resize storm, repeated create/destroy, surface destroy during render, recreate
  after loss, shutdown with pending callbacks, and pending style/resource events
  are covered by automated or scripted tests where feasible.
- Resize while render is scheduled, destroy from observer callback, style
  replacement during tile loading, and surface loss while drawable acquisition or
  present fails are covered by tests where feasible.
- The team explicitly accepts the surface/lifecycle contract before JNI or
  Compose consumes it beyond smoke tests.

## Phase 5: Minimal C ABI and Zig Smoke

- Add a minimal experimental C ABI over the Rust runtime.
- Expose only enough calls to create/destroy runtime and map, set size/style,
  set camera, poll events, and optionally render/read back a frame.
- Add a tiny Zig program using `@cImport` against the C header.
- Keep this ABI explicitly unstable and smoke-test-only.
- Do not make JNI depend on this ABI yet; JNI remains a peer adapter over the
  Rust runtime.

Exit criteria:

- The Zig smoke can create/destroy the runtime and map through the C ABI.
- The Zig smoke can set style/camera and drain at least one native event.
- The C ABI uses opaque handles, explicit destroy, plain-data structs, status
  codes, and no Rust-specific types.
- Any render/readback support remains optional and experimental.

## Phase 6: JVM/JNI Smoke Adapter

- Add or create a separate JNI smoke adapter using `jni-rs`.
- Expose only the minimum calls needed by a JVM test app: create, attach or
  provide a surface or frame target, resize, set style, set camera, poll
  events, destroy.
- Package the native library in a way that resembles MapLibre Compose's current
  desktop native artifact layout.
- Add a tiny Kotlin or Java JVM example outside the main runtime API.
- Treat this adapter as internal and unstable until the runtime surface and
  lifecycle contract is accepted.

Exit criteria:

- A JVM process can load the Rust native library.
- The JVM example can show or obtain rendered frames, change camera, and shut
  down cleanly.
- The JNI layer remains adapter code, not the main runtime API.

## Phase 7: MapLibre Compose Desktop Prototype

- In a MapLibre Compose branch, add an experimental desktop implementation that
  consumes the Rust runtime through the JNI adapter.
- Preserve the public `MaplibreMap` API and internal `MapAdapter` boundary.
- Prefer the native surface path. Use CPU readback only for tests or as an
  explicitly documented temporary fallback.
- Implement style URL loading, camera state synchronization, resize, lifecycle,
  and basic click/camera events, with Compose owning gesture recognition and the
  runtime receiving movement commands.
- Validate one minimal overlay/query slice only if required by the prototype,
  keeping broad style reconciliation out of the initial wrapper API.
- Validate one declarative camera reconciliation slice: adapter-owned camera
  state applies runtime camera commands, observes native camera changes, avoids
  feedback loops, and handles user/gesture, programmatic, and animation-driven
  changes distinctly where metadata allows.
- Keep the current C++/SimpleJNI desktop implementation available behind a flag
  or separate dependency until the Rust path is proven.

Exit criteria:

- The MapLibre Compose demo app can run on desktop using the Rust runtime.
- Basic camera/style interactions work through existing Compose APIs.
- The prototype has documented performance and feature gaps.

## Phase 8: Desktop Feature Parity Push

- Fill MapLibre Compose desktop gaps using focused runtime APIs.
- Prioritize visible region, projection, feature query, style images, GeoJSON
  sources, common layer mutation, snapshots, and robust loading/error events.
- Add tests or demo scenarios for each newly covered API.
- Stabilize native artifact packaging for macOS, Linux, and Windows variants.
- Convert the Compose parity inventory in [DESIGN.md](DESIGN.md#style-and-data-apis)
  into tracked issues, tests, or demo scenarios.
- Add expression/filter/property tests only for APIs that are actually exposed.
- Add query tests for click point query, box query, layer-filtered query, and
  query before style load.

Exit criteria:

- Rust-backed desktop covers the major missing MapLibre Compose desktop
  features that are feasible with the selected rendering path.
- Published artifacts can be consumed by a normal Compose Multiplatform JVM app.
- The old desktop native path can be deprecated or removed in a planned release.

## Future Exploration

These are not roadmap commitments. They become separate design docs only after
the Rust desktop path proves useful:

- Android native-core adapter.
- iOS native-core adapter.
- Stable shared C ABI if multiple non-Rust adapters need the same boundary.
- External framework adapters such as GTK, Qt, WinUI, SwiftUI, Flutter, React
  Native, Slint, egui, iced, C#/.NET, Python, and other Rust or native UI
  toolkits.
- Stabilized publishing, support tiers, symbols, license bundles, and release
  automation.
- Runtime capability metadata for MapLibre Native Core version, style-spec
  support, source/layer types, backend, and optional style-level features.
- Decision whether to upstream pieces back to `maplibre-native-rs`, keep an
  independent crate, or merge efforts under a shared MapLibre Rust workspace.
