---
title: Overview
description: Contributor setup, project scope, workflow commands, tests, and examples.
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
C API's concepts. They do not provide full SDKs, higher-level async models,
view lifecycle integrations, convenience workflows, or new abstractions.

## Getting Set Up

Install the platform toolchain:

- On macOS, install a recent version of Xcode.
- On Windows, install a recent version of Visual Studio Community with the
  `Desktop development with C++` workload.

Install [`mise`](https://mise.jdx.dev/), then install the pinned project tools:

```bash
mise install
```

Run the Zig map example as a smoke test:

```bash
mise run //examples/zig-map:run
```

The first build configures CMake and fetches MapLibre Native into
`third_party/maplibre-native`. To use a separate MapLibre Native checkout, set
`MLN_SOURCE_DIR` before configuring or running builds.

## Common Commands

Build and test:

```bash
mise run test
```

Build without running tests:

```bash
mise run build
```

Run checks:

```bash
mise run check
```

Run formatters and linters that may edit files:

```bash
mise run fix
```

Run targeted examples when they provide evidence that automated tests cannot,
such as rendering or host integration:

```bash
mise run //examples/<project>:run
```

Build the documentation site and regenerate API reference pages:

```bash
mise run //docs:build
```

Regenerate only the C API reference:

```bash
mise run //docs:api:c
```

Pre-commit runs the configured formatters and linters on changed files.

## How Tools Fit Together

[`mise`](https://mise.jdx.dev/) is the contributor entrypoint. It pins tools,
installs Git hooks, and runs repository tasks. Use `mise run ...` for the tasks
defined by the repository, such as build, test, check, and example tasks. Use
`mise exec -- ...` for one-off commands that need the pinned tools and
environment, or activate mise to run tools such as `pixi` and `dprint` directly
from the project directory.

[`pixi`](https://pixi.sh/) supplies native packages from
[`conda-forge`](https://conda-forge.org/). Pixi runs CMake so the repository
declares C and C++ dependencies instead of relying on a system package manager.

[`hk`](https://github.com/jdx/hk) decides which checks run for pre-commit,
`mise run check`, and `mise run fix`. [`dprint`](https://dprint.dev/) owns
formatter selection and formatting defaults. Language-specific formatter and
linter configs tune the tools that those entry points invoke.

[`CMake`](https://cmake.org/) builds the native C/C++ library.
[`Zig`](https://ziglang.org/) consumes the CMake-built library for C ABI tests.

Astro and Starlight build the documentation site. Generated reference
documentation is exported as Markdown into `docs/src/content/docs/reference/`.

## Tests And Examples

Every feature needs CI coverage through an automated test when practical. Tests
consume the public C API. Zig tests also check header shape because `@cImport`
catches C API issues quickly.

Use examples for demos and for behavior that needs manual validation, such as
visual output, interactive input, or host graphics integration.

Keep examples small. This repository may include low-level language bindings and
focused integration examples. Full application SDKs live outside this
repository.
