---
title: Guidelines
description: General project scope, layering, documentation, testing, and example guidance.
---

## Project Scope

The project exposes MapLibre Native through two layers.

The C API exposes core MapLibre Native features on supported native platforms:
runtime, resources, maps, cameras, events, diagnostics, logging, render target
primitives, texture readback, and low-level extension points such as resource
providers and URL transforms. It excludes convenience APIs such as snapshotting
and platform integrations such as gestures and device sensors.

Language bindings sit directly above the C API. They manage C handles, struct
initialization, scoped lifetimes, status codes, diagnostics, borrowed data,
events, threading, and event draining in the target language. They preserve the
C API's concepts rather than provide fully idiomatic SDKs, higher-level async
models, view lifecycle integrations, convenience workflows, or new abstractions.

## Documentation

User guides are language-agnostic. As bindings become supported, guides should
use tabbed snippets for each relevant language. Do not split one task into
separate language-specific guides.

Reference pages come from public surfaces. Keep exact symbol contracts in source
comments that feed the generated reference. Hand-written guides and concepts
link to generated reference pages instead of duplicating status values, field
lists, ownership contracts, or callback tables.

Contributor pages should describe rules that affect future changes. Keep command
lists and local workflow details in [development setup](/development/setup/).

## Tests And Examples

Every feature needs CI coverage through an automated test when practical. Tests
consume the public C API. Zig tests also check header shape because `@cImport`
catches C API issues quickly.

Use examples for demos and for behavior that needs manual validation, such as
visual output, interactive input, or host graphics integration.

Keep examples small. This repository may include low-level language bindings and
focused integration examples. Full application SDKs live outside this
repository.
