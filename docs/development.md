# Development Conventions

## Project Boundary

The project exposes MapLibre Native through two layers.

The C API wraps core MapLibre Native features on supported MapLibre Native
platforms. It includes runtime, resource, map, camera, event, diagnostics,
logging, render target primitives, and low-level extension points such as
resource providers and URL transforms. It excludes convenience APIs such as
snapshotting and platform integrations such as gestures and device sensors.

Language bindings sit directly above the C API. They manage C handles, struct
initialization, scoped lifetimes, status codes, diagnostics, borrowed data,
events, threading, and event draining in the target language. They do not aim to
provide fully idiomatic APIs, higher-level async models over map events, view
lifecycle integrations, convenience workflows, or new abstractions beyond the C
API's concepts.

## Implementation Layout

`include/` is the public C API boundary. Keep implementation-only helpers out of
public headers.

```text
include/                 # public C API headers
src/
  c_api/                 # exported C definitions and C boundary validation
  <subsystem>/           # implementation semantics
```

## ABI Evolution

The ABI is unstable while `mln_c_version()` returns `0`. Do not add
compatibility shims or version-branching code for changed structs or functions
during this phase.

Still shape structs for future ABI stability. Use `size` fields for option and
output structs that may grow over time, and populate them in default
constructors.

Use field masks or presence booleans for optional values where zero is valid.

Keep backend-native handles opaque as `void*`; document the backend type and
ownership rules on the function or struct field.

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

Set thread-local diagnostic strings for synchronous non-OK returns. Report
asynchronous native failures through copied map events.

## Ownership And Lifetime

Make ownership explicit at every boundary.

Borrow host-provided strings and buffers for call-duration inputs. Copy
host-provided strings and buffers that outlive the function or native callback.

Store host-provided callbacks and `user_data` by reference. Document how long
they must remain valid on the registering function. Document the invalidation
point for returned borrowed pointers.

Give owned handles and scoped resources explicit destroy or release functions.
Status-returning functions reject null handles. Void release functions accept
null as a no-op.

Output handle parameters reject non-null `*out_handle` values, preserving live
host-owned handles on failure. Document when scoped resource ownership begins,
when it ends, and whether completion may happen inline or later.

## Threading

The runtime and map model is host-pumped. Runtime creation records the owner
thread. Runtime, map, and texture-session calls that touch thread-affine state
validate that owner thread and return `MLN_STATUS_WRONG_THREAD` for mismatches.

Cross-thread dispatch belongs in public functions designed as enqueueing
commands. Document that behavior on the function. Higher-level adapters can
build threaded models above the C API.

MapLibre's `RunLoop` is owner-thread scheduler state. Each owner thread may hold
one live runtime. `mln_runtime_run_once()` pumps that runtime's run loop.

## Async Model And Events

Preserve MapLibre Native's imperative, observer-driven model. C API calls return
status for synchronous acceptance or failure. Later native work is reported
through drained events.

Map events are copied into map-owned storage and drained with C API calls. Event
payloads use plain data with documented lifetimes.

Classify each operation as one of:

- immediate, where the return status is the final result;
- a command, where return status means accepted and later effects arrive as
  events;
- a snapshot, where the returned data is last-known state;
- a blocking query, used sparingly and documented with deadlock risks;
- an event stream, where many events are expected over time.

## Native Callbacks

Prefer polled events for native-to-host notifications about map state,
lifecycle, rendering, and errors. Use native callbacks for low-level extension
points where MapLibre needs a synchronous decision, an asynchronous request
handle, or process-global integration such as logging.

Low-level native callbacks can run outside the owner thread. Logging, resource
transform, and resource provider callbacks may run on MapLibre worker, network,
logging, or render-related threads.

A callback API documents:

- which thread may invoke it;
- how long the callback and `user_data` must remain valid;
- whether input pointers are borrowed or copied;
- whether output pointers are copied before return;
- whether it may call back into any C API function;
- what happens when it returns an error or unknown decision value.

Callbacks must not unwind through the C API. Bindings must catch host
exceptions, panics, and errors inside the callback and convert them to the
callback's documented return behavior.

## Maps And Render Targets

Keep map state separate from render targets. `mln_map` owns style, camera,
observer events, and render invalidation state. Render target sessions own
backend-bound resources.

Each render target kind should preserve the same separation from `mln_map`,
including texture sessions, native surface sessions, and future targets.

Render target APIs must document owner thread, backend handle ownership,
synchronization, borrowed pointer lifetimes, generation or stale-frame behavior,
and teardown rules.

## Tests And Examples

Every feature needs CI coverage through an automated test when practical. Tests
should consume the public C API. Zig tests also check header shape because
`@cImport` catches C API issues quickly.

Use examples for human demos and for behavior that needs manual validation, such
as visual output, interactive input, or host graphics integration.

Keep examples small. This repository may include low-level language bindings and
focused integration examples. Full application SDKs live outside this
repository.
