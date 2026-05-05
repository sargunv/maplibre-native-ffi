---
title: C API
description: C ABI contract and C/C++ implementation rules for contributors.
sidebar:
  order: 2
---

## Public API Layout

`include/` is the public C API boundary. Keep implementation-only helpers out of
public headers. Consumers include `maplibre_native_c.h`; domain headers under
`include/maplibre_native_c/` keep declarations maintainable and may be included
directly when useful.

```text
include/                 # public C API headers
  maplibre_native_c.h    # public umbrella header
  maplibre_native_c/     # public domain headers
src/
  c_api/                 # exported C definitions and C boundary validation
  <subsystem>/           # implementation semantics
```

## ABI Evolution

The ABI is unstable while `mln_c_version()` returns `0`. Do not add
compatibility shims or version-branching code for changed structs or functions
during this phase.

The public C header targets C23. ABI-crossing enum types use C23
fixed-underlying enum syntax: `int32_t` for status values and `uint32_t` for
non-negative domains and masks unless a native ABI field requires another width.

Shape structs for future ABI stability. Option and output structs that may grow
use `uint32_t size` fields. Default constructors populate them.

Use field masks or presence booleans for optional values when zero is valid.

Keep public struct fields friendly to binding generators. Prefer scalar fields,
pointers with length fields, structs, unions, and opaque handles. Do not use
fixed-size inline text buffers; expose borrowed ABI-owned text with a length or
provide an explicit copy or drain API.

Keep backend-native handles opaque as `void*`; document the backend type and
field-level requirements on the struct field. Document ownership and lifetime on
the function that accepts or returns the struct.

## Errors And Diagnostics

Status-returning C API functions return `mln_status`. Each function's public
comment lists its status values and meanings.

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
asynchronous native failures through copied runtime events.

## Ownership And Lifetime

Make ownership explicit at every boundary.

Struct definitions describe data shape, required fields, and pointer validity.
Function comments describe whether input pointers are borrowed, copied,
retained, or consumed. They also describe when returned views become invalid.

Borrow host-provided strings and buffers for call-duration inputs. Copy
host-provided strings and buffers that outlive the function or native callback.

Store host-provided callbacks and `user_data` by reference. Document how long
they must remain valid on the registering function. Document the invalidation
point for returned borrowed pointers.

Give owned handles and scoped resources explicit destroy or release functions.
Status-returning functions reject null handles. Void release functions accept
null as a no-op.

Output handle parameters reject non-null `*out_handle` values and preserve live
host-owned handles on failure. Document when scoped resource ownership begins,
when it ends, and whether completion may happen inline or later.

## Threading

The runtime and map use a host-pumped model. Runtime creation records the owner
thread. Runtime, map, map-projection, and render-target-session calls that touch
thread-affine state validate the owner thread and return
`MLN_STATUS_WRONG_THREAD` for mismatches.

Cross-thread dispatch belongs in public functions designed as enqueueing
commands. Document that behavior on the function. Higher-level adapters can
build threaded models above the C API.

MapLibre's `RunLoop` is owner-thread scheduler state. Each owner thread may hold
one live runtime. `mln_runtime_run_once()` pumps that runtime's run loop.

## Async And Events

Preserve MapLibre Native's imperative, observer-driven model. C API calls return
status for synchronous acceptance or failure. Drained events report later native
work.

Events are copied into runtime-owned storage and drained with C API calls. Event
payloads use plain data with documented lifetimes. Each event identifies its
source kind and source handle.

Classify each operation as one of:

- immediate, where the return status is the final result;
- a command, where return status means accepted and later effects arrive as
  events;
- a state snapshot, where the returned data is last-known state;
- a blocking query, used rarely and documented with deadlock risks;
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

Callbacks must not unwind through the C API. Bindings catch host exceptions,
panics, and errors inside the callback and convert them to the callback's
documented return behavior.

## Maps And Render Targets

Keep map state separate from render targets. `mln_map` owns style, camera,
observer events, and render invalidation state. Each map may have one live
render target session; that session owns backend-bound resources.

Texture sessions render offscreen into session-owned backend targets or
caller-owned borrowed backend targets. Surface sessions render and present
through caller-provided native surfaces. Future target kinds should preserve the
same separation from `mln_map`.

Render target APIs must document owner thread, backend handle ownership,
synchronization, borrowed pointer lifetimes, generation or stale-frame behavior,
and teardown rules.
