# Development Conventions

## Product Boundary

The project wraps MapLibre Native's C++ core with a C ABI.

In scope:

- direct C wrapper for core MapLibre Native functionality;
- safe, low-level language bindings over the C API.

Out of scope:

- convenience wrappers such as a snapshotter;
- batteries-included platform integrations;
- gesture recognition, declarative state, sensor integrations.

Language and UI bindings translate this low-level model into idiomatic host
APIs.

## C API Shape

Keep the C API C-shaped:

- handles are opaque forward-declared structs;
- handles use explicit create/destroy calls;
- option and output structs start with a `size` field;
- structs, enums, bitmasks, and status returns stay plain C;
- optional struct fields use explicit field masks or presence booleans so zero
  remains a valid value;
- strings and byte buffers are borrowed for the documented duration unless a
  function copies or returns owned storage;
- borrowed return pointers use the documented lifetime;
- backend-native handles are `void*` plus documented backend types, ownership,
  and lifetime;
- runtime, resource, map, camera, event, diagnostics, logging, and render target
  primitives document ownership, lifetime, thread-affinity, and async event
  semantics.

The ABI is unstable while `mln_c_version()` returns `0`.

Default constructors such as `mln_runtime_options_default()` return initialized
structs with `size` populated.

## Status And Diagnostics

Status-returning C API functions return `mln_status`. Each function's public
comment lists its status values and their meanings.

Use these categories consistently:

- `MLN_STATUS_INVALID_ARGUMENT` for null pointers, unknown enum values, unknown
  flag bits, undersized structs, invalid dimensions, invalid handles, or output
  handles initialized incorrectly;
- `MLN_STATUS_INVALID_STATE` when an otherwise valid object is in the wrong
  lifecycle state;
- `MLN_STATUS_WRONG_THREAD` when a thread-affine handle is called from the wrong
  owner thread;
- `MLN_STATUS_UNSUPPORTED` when a backend, platform, entry point, or requested
  behavior is unavailable in this build;
- `MLN_STATUS_NATIVE_ERROR` when a native MapLibre error or C++ exception is
  converted to status.

Every exported `MLN_API` C++ definition must be `noexcept`. Status-returning
entry points use the C API boundary helper to clear thread-local diagnostics on
entry and convert exceptions to `MLN_STATUS_NATIVE_ERROR`.

Set thread-local diagnostics for synchronous non-OK returns. Diagnostic strings
serve humans and tests. Report asynchronous native failures through copied map
events.

Keep diagnostics paths non-throwing. Prefer a fallback diagnostic to an error
reporting path that violates the C ABI boundary.

## Ownership And Lifetime

Make ownership explicit at every boundary.

- Host-provided strings, buffers, callbacks, and `user_data` are borrowed unless
  a function documents that it copies them.
- Copy retained inputs before the function or native callback returns.
- ABI-owned handles are released through explicit destroy or release functions.
- Passing null to a void release function may be a no-op if documented.
- Status functions reject null handles.
- Output handle parameters reject non-null `*out_handle` values, preserving live
  host-owned handles on failure.
- Add release APIs only for public functions that return owned ABI storage.

Resource request handles have async ownership. A provider owns a releasable
handle after choosing to handle the request. It may complete the request inline
or later. It releases the handle exactly once after completion or cancellation
observation.

## Threading

The runtime and map model is host-pumped. Runtime creation records the owner
thread. Runtime, map, and texture-session calls that touch thread-affine state
validate that owner thread and return `MLN_STATUS_WRONG_THREAD` for mismatches.

Thread hopping belongs in documented enqueueing commands. Adapters can build
threaded models above the C ABI.

MapLibre's `RunLoop` is owner-thread scheduler state. Each owner thread may hold
one live runtime. `mln_runtime_run_once()` pumps that runtime's run loop.

Low-level native callbacks can run outside the owner thread. Logging, resource
transform, and resource provider callbacks may run on MapLibre worker, network,
logging, or render-related threads. Their documentation states threading,
reentrancy, lifetime, and C API reentry rules.

## Async Model And Events

Preserve MapLibre Native's imperative, observer-driven model. The C ABI returns
status for synchronous acceptance or failure and reports asynchronous native
work through drained events.

Bindings may translate event polling into futures, promises, coroutines, flows,
or callbacks above the ABI.

Copy map events into map-owned storage and drain them with C API calls. Use
plain event payloads with documented lifetimes.

When adding an operation, decide whether it is:

- immediate, where the return status is the final result;
- a command, where return status means accepted and later effects arrive as
  events;
- a snapshot, where the returned data is last-known state;
- a blocking query, used sparingly and documented with deadlock risks;
- an event stream, where many events are expected over time.

## Native Callbacks

Callbacks are low-level escape hatches.

A callback API documents:

- which thread may invoke it;
- how long the callback and `user_data` must remain valid;
- whether input pointers are borrowed or copied;
- whether output pointers are copied before return;
- whether it may call back into any C API function;
- what happens when it returns an error, throws through foreign code, or returns
  an unknown decision value.

Prefer host-managed dispatch in language bindings for user code.

## Resources And Cache

Runtime-scoped resource configuration lets maps share file sources, cache
configuration, and URL behavior.

Follow MapLibre Native's resource model:

- use runtime-derived `ResourceOptions` for asset path, cache path, maximum
  ambient cache size, and the runtime platform context;
- keep process-global network status as a wrapper over MapLibre Native network
  status;
- keep resource transforms URL-only to match MapLibre Native's
  `ResourceTransform`;
- use the custom resource provider as a network request interception point;
- let built-in native resource loaders handle built-in schemes before the C API
  provider sees network requests;
- convert provider failures into native resource errors while containing
  exceptions at the C boundary.

Add higher-level retry policy, custom eviction policy, auth mutation, or offline
product workflows for concrete MapLibre Native APIs or product requirements.

## Maps And Render Targets

Map state is separate from render targets.

`mln_map` owns map/control state: style, camera, observer events, render
invalidation state, and long-lived native map/frontend objects. A map may exist
with no attached render target.

Render target sessions own backend-bound resources. Texture sessions cover Metal
and Vulkan offscreen rendering. Future native surface sessions should preserve
this separation from `mln_map`.

Texture sessions and native surface sessions are alternate render target kinds.
Texture sessions are the primary integration path for UI toolkits that composite
their own scene graph. Native surface sessions provide fallback or comparison
paths for concrete platform needs.

A map may have zero or one attached render target. Attach, resize, render, frame
acquire, frame release, detach, and destroy operations must document owner
thread, backend handle ownership, synchronization, borrowed pointer lifetimes,
generation or stale-frame behavior, and teardown rules.

Texture rendering lets host UI toolkits sample the map inside their own scene
graphs. The wrapper owns the rendered texture or image allocation created on
host-provided backend objects. Hosts own windows, swapchains, widgets, and scene
graph composition.

## Style, Camera, And Gestures

The C ABI wraps MapLibre Native concepts directly.

Style APIs use `mbgl::style::Style` and C representations of MapLibre style
operations. Add source and layer APIs in stages, starting with narrow JSON or
typed wrappers that match native style behavior.

Camera APIs expose native map movement primitives such as jump, pan, scale,
rotate, pitch, and transition cancellation. Gesture recognition and declarative
camera state belong in adapters. Camera observer events are notifications.

Rendered-feature queries and projection helpers are map or render queries. Their
docs state whether they require an attached render target.

## Implementation Layout

Keep `include/maplibre_native_c.h` as the public product boundary. Place
implementation-only helpers outside the public header.

`src/c_api/` owns exported C definitions, no-exception entry points, and calls
into implementation code. Implementation semantics live in subsystem directories
such as `src/runtime/`, `src/map/`, `src/resources/`, `src/render/`,
`src/logging/`, and `src/diagnostics/`.

Use local MapLibre Native behavior as evidence. CMake fetches the pinned source
into `third_party/maplibre-native`; set `MLN_SOURCE_DIR` to inspect or develop
against a separate checkout. Use the native source to confirm behavior.

Add language or UI adapters to the core layout after the C ABI and
render-session contract justify them.

## Tests And Examples

Every feature should add or update a smoke test, example, or automated test that
demonstrates its acceptance criteria.

Prefer tests and examples that consume the public C ABI. Zig tests are the main
ABI smoke tests because `@cImport` exposes header shape quickly.

Keep example apps small. They validate the ABI, lifecycle, events, texture
ownership, and interactive camera control.
