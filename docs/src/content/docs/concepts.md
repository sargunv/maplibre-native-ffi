---
title: Concepts
description: Core mental models for using MapLibre Native FFI.
---

## Mental Model

MapLibre Native FFI exposes MapLibre Native concepts directly. Applications can
use it directly or through language bindings, and higher-level adapters can
build on the same model. It provides a common portable API surface for native
map integration.

Three concepts form the core API: the runtime, the map, and the render session.
Events and bindings connect those concepts to host code.

## Runtime

The runtime owns scheduler state and event storage for one host owner thread.
Host code creates a runtime on the thread that will pump it. Runtime work and
events flow through that owner thread.

Each owner thread may have one live runtime. The host pumps that runtime to let
MapLibre Native make progress and to collect completed work.

## Map

A map belongs to a runtime. It owns map state: style, camera, observer events,
and render invalidation.

A map is independent of any particular render target. Host code can create,
configure, query, and observe the map without tying that map state to a window,
surface, or texture.

## Render Session

A render session renders one map to one render target. Render targets are
surfaces or textures.

Surface sessions render and present through caller-provided native surfaces.
Texture sessions render offscreen into session-owned backend targets or
caller-owned borrowed backend targets.

A map may have one live render session at a time. Keeping render sessions
separate from maps lets host code manage graphics backend lifecycle outside the
map object itself.

## Events

Events preserve MapLibre Native's observer-driven model across the FFI boundary.
The runtime copies events into host-visible storage, and host code drains those
events from the runtime.

Events report map lifecycle, rendering progress, resource activity, diagnostics,
and asynchronous failures.

## Language Bindings

Language bindings preserve the same runtime, map, render session, and event
model in the target language. They keep the API portable while matching the
target language's handle and error conventions.

Bindings sit directly above the C API and stay close to its shape. They expose
the same core objects and relationships with language-appropriate safety around
handles, lifetimes, errors, and event draining.
