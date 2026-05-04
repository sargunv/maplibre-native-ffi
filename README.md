<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://maplibre.org/img/maplibre-logos/maplibre-logo-for-dark-bg.svg">
    <source media="(prefers-color-scheme: light)" srcset="https://maplibre.org/img/maplibre-logos/maplibre-logo-for-light-bg.svg">
    <img alt="MapLibre Logo" src="https://maplibre.org/img/maplibre-logos/maplibre-logo-for-light-bg.svg" width="200">
  </picture>
</p>

# MapLibre Native FFI

[![MapLibre](https://img.shields.io/badge/MapLibre-396CB2)](https://maplibre.org/)
[![Slack](https://img.shields.io/badge/Slack-4A154B?logo=slack&logoColor=white)](https://slack.openstreetmap.us/)
[![License](https://img.shields.io/github/license/sargunv/maplibre-native-ffi?label=License)](./LICENSE)

This project provides an experimental C API for
[MapLibre Native](https://github.com/maplibre/maplibre-native). It is built for
low-level language bindings and host integrations that need a C boundary instead
of direct C++ interop.

The API keeps MapLibre Native concepts direct. Framework concerns such as
gestures, widgets, declarative UI, and application lifecycle integration belong
in downstream adapters.

The C ABI is unstable while the project is pre-1.0.

## Try It

Install [mise](https://mise.jdx.dev/), then install the pinned tools:

```bash
mise install
```

Run a supported example:

```bash
# macOS and Linux
mise run //examples/zig-map:run
```

```bash
# macOS only
mise run //examples/swift-map:run
```

## Current Status

The vast majority of the MapLibre Native API surface is already covered.
Advanced extension APIs (custom layers, shader registry) are not yet included.

Legend: 🟢 built and tested in CI; ❌ not yet implemented.

| Platform | OpenGL             | Vulkan             | Metal              | WebGPU             |
| -------- | ------------------ | ------------------ | ------------------ | ------------------ |
| Linux    | ❌ [#23][issue-23] | 🟢                 | -                  | ❌ [#37][issue-37] |
| Android  | ❌ [#24][issue-24] | ❌ [#24][issue-24] | -                  | ❌ [#37][issue-37] |
| macOS    | -                  | ❌ [#20][issue-20] | 🟢                 | ❌ [#37][issue-37] |
| iOS      | -                  | -                  | ❌ [#25][issue-25] | ❌ [#37][issue-37] |
| Windows  | ❌ [#22][issue-22] | ❌ [#21][issue-21] | -                  | ❌ [#37][issue-37] |

[issue-20]: https://github.com/sargunv/maplibre-native-ffi/issues/20
[issue-21]: https://github.com/sargunv/maplibre-native-ffi/issues/21
[issue-22]: https://github.com/sargunv/maplibre-native-ffi/issues/22
[issue-23]: https://github.com/sargunv/maplibre-native-ffi/issues/23
[issue-24]: https://github.com/sargunv/maplibre-native-ffi/issues/24
[issue-25]: https://github.com/sargunv/maplibre-native-ffi/issues/25
[issue-37]: https://github.com/sargunv/maplibre-native-ffi/issues/37

## License

BSD 3-Clause. See [LICENSE](LICENSE).

Copyright (c) 2026 MapLibre contributors.
