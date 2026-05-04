---
title: Quickstart
description: Install pinned tools and run a supported example.
---

Install [`mise`](https://mise.jdx.dev/). Then install the pinned project tools:

```bash
mise install
```

Run the Zig map example:

```bash
# macOS and Linux
mise run //examples/zig-map:run
```

On macOS, run the Swift map example too:

```bash
mise run //examples/swift-map:run
```

The first build configures CMake and fetches MapLibre Native into
`third_party/maplibre-native`. To use a separate MapLibre Native checkout, set
`MLN_SOURCE_DIR` before configuring or running builds.

## Next Steps

- Read the [runtime and event model](/concepts/).
- Follow the [runtime and map guide](/guides/create-a-runtime-and-map/) when it
  has content.
- Use the [C reference](/reference/c/) for exact declarations.
