---
title: Concepts
description: Core mental models for using and wrapping MapLibre Native FFI.
---

## Mental Model

MapLibre Native FFI exposes MapLibre Native as direct concepts, not as a full
application SDK.

The runtime owns scheduler state and event storage for one host owner thread.
The map owns style, camera, observer events, and render invalidation state. A
render target session attaches backend resources to a map for surface or texture
rendering.

Host integrations own application lifecycle, widgets, gestures, input routing,
and higher-level async models.

Core relationships:

- A runtime is host-pumped from its owner thread.
- A map owns map state independently of any particular render target.
- A render target session owns or borrows backend resources while attached.
- Events are copied into runtime-owned storage and drained by the host.
- Bindings sit above the C API and preserve the same core model.

## API Scope

The C API exposes core MapLibre Native features on supported native platforms:
runtime, resources, maps, cameras, events, diagnostics, logging, render target
primitives, texture readback, and low-level extension points such as resource
providers and URL transforms.

The C API excludes convenience APIs such as snapshotting and platform
integrations such as gestures and device sensors.

Language bindings sit directly above the C API. They manage C handles, struct
initialization, scoped lifetimes, status codes, diagnostics, borrowed data,
events, threading, and event draining in the target language.

Bindings preserve the C API's concepts. They do not provide full application
SDKs, higher-level async models over runtime events, view lifecycle
integrations, convenience workflows, or new abstractions.

## Runtime, Threading, And Events

The runtime and map use a host-pumped model. Runtime creation records the owner
thread. Runtime, map, map-projection, and render-target-session calls that touch
thread-affine state validate the owner thread.

MapLibre's `RunLoop` is owner-thread scheduler state. Each owner thread may hold
one live runtime, and the host pumps work through that runtime.

Calls return status for synchronous acceptance or failure. The runtime reports
later native work through copied events that the host drains explicitly.

Operation shapes:

- Immediate operations complete before the function returns.
- Commands report synchronous acceptance and later effects through events.
- State snapshots return last-known state.
- Blocking queries are rare and document deadlock risks.
- Event streams produce many events over time.

## Ownership And Lifetimes

MapLibre Native FFI makes ownership explicit at the boundary between the host
language and native MapLibre code.

Call-duration inputs are borrowed. Data that outlives a function call or native
callback is copied. Owned handles and scoped resources have explicit destroy or
release functions.

Returned views document when they become invalid. Callback APIs document how
long callbacks and `user_data` must remain valid, which thread may invoke them,
and whether output pointers are copied before return.

The generated reference is the source of truth for exact ownership and lifetime
contracts on each function and type.

## Rendering Targets

Map state is separate from render targets. An `mln_map` owns style, camera,
observer events, and render invalidation state. Each map may have one live
render target session, and that session owns backend-bound rendering resources.

Texture sessions render offscreen into session-owned backend targets or
caller-owned borrowed backend targets. Surface sessions render and present
through caller-provided native surfaces.

This separation lets bindings and host integrations keep application lifecycle
and graphics backend lifecycle outside the map object itself.

## Resources And Networking

MapLibre Native FFI exposes low-level resource loading extension points such as
URL transforms and resource providers.

These APIs let host integrations transform, fulfill, or fail MapLibre Native
requests while preserving the C API's runtime event and diagnostic model.

## Host Integration Boundaries

MapLibre Native FFI is a low-level integration layer. It exposes native map
capabilities across a C boundary and through direct language bindings.

Downstream adapters own framework-specific behavior: gestures, widgets,
declarative UI, application lifecycle, sensors, platform view integration, and
opinionated async APIs.

Keeping this boundary clear lets the C API and bindings remain predictable for
multiple host environments without choosing one application framework's model.
