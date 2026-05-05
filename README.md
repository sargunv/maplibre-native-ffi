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

The API exposes MapLibre Native concepts directly. Framework concerns such as
gestures, widgets, declarative UI, and application lifecycle integration belong
in downstream adapters.

MapLibre Native FFI is pre-1.0. The C ABI is unstable while `mln_c_version()`
returns `0`.

## Documentation

Read the documentation site for concepts, usage guides, the generated C API
reference, and contributor notes.

- [Concepts](https://sargunv.github.io/maplibre-native-ffi/concepts/)
- [Usage guides](https://sargunv.github.io/maplibre-native-ffi/guides/)
- [C API reference](https://sargunv.github.io/maplibre-native-ffi/reference/c/)
- [Development overview](https://sargunv.github.io/maplibre-native-ffi/development/overview/)

## Status

The C API covers most of the MapLibre Native API surface. Advanced extension
APIs, including custom layers and shader registry access, are not yet included.

Current CI-backed render targets include Vulkan on Linux and Metal on macOS. See
the documentation site for full platform status.

## License

BSD 3-Clause. See [LICENSE](LICENSE).

Copyright (c) 2026 MapLibre contributors.
