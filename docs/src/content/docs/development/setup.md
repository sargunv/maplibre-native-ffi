---
title: Development Setup
description: Platform setup, pinned tools, and local commands for contributors.
---

## Getting Set Up

Install the platform toolchain first:

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

The first build configures CMake, which fetches MapLibre Native into
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

Run formatters and linters that can modify files:

```bash
mise run fix
```

Run targeted examples when they provide evidence that automated tests cannot,
such as rendering or host integration:

```bash
mise run //examples/<project>:run
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
[`conda-forge`](https://conda-forge.org/). CMake is run through Pixi so C and
C++ dependencies are declared in the repository instead of depending on a
particular system package manager.

[`hk`](https://github.com/jdx/hk) decides which checks run for pre-commit,
`mise run check`, and `mise run fix`. [`dprint`](https://dprint.dev/) owns
formatter selection and formatting defaults. Language-specific formatter and
linter configs tune the tools that those entry points invoke.

[`CMake`](https://cmake.org/) builds the native C/C++ library.
[`Zig`](https://ziglang.org/) consumes the CMake-built library for C ABI tests.
