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

## Project Docs

- [`CONTRIBUTING.md`](CONTRIBUTING.md) explains how to prepare changes for
  review.
- [`docs/development.md`](docs/development.md) explains project scope and
  development conventions.
- [`docs/development-setup.md`](docs/development-setup.md) explains platform
  setup, pinned tools, and local commands.

## Current Status

Target support tracks renderer and platform combinations that can build and run
through this C API.

| Target             | Support | Tracking                                                        |
| ------------------ | ------- | --------------------------------------------------------------- |
| Linux Vulkan       | 🟢      |                                                                 |
| Linux OpenGL/EGL   | ❌      | [#23](https://github.com/sargunv/maplibre-native-ffi/issues/23) |
| macOS Metal        | 🟢      |                                                                 |
| macOS Vulkan       | ❌      | [#20](https://github.com/sargunv/maplibre-native-ffi/issues/20) |
| Windows Vulkan     | ❌      | [#21](https://github.com/sargunv/maplibre-native-ffi/issues/21) |
| Windows OpenGL/WGL | ❌      | [#22](https://github.com/sargunv/maplibre-native-ffi/issues/22) |
| Android Vulkan     | ❌      | [#24](https://github.com/sargunv/maplibre-native-ffi/issues/24) |
| Android OpenGL/EGL | ❌      | [#24](https://github.com/sargunv/maplibre-native-ffi/issues/24) |
| iOS Metal          | ❌      | [#25](https://github.com/sargunv/maplibre-native-ffi/issues/25) |
| WebGPU             | ❌      | [#37](https://github.com/sargunv/maplibre-native-ffi/issues/37) |

API coverage tracks MapLibre Native domains exposed through the C API.

| Domain                | Coverage | Tracking                                                                                                                                                                                                                                                          |
| --------------------- | -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ABI contract          | 🟢       |                                                                                                                                                                                                                                                                   |
| Diagnostics, logging  | 🟢       |                                                                                                                                                                                                                                                                   |
| Runtime lifecycle     | 🟢       |                                                                                                                                                                                                                                                                   |
| Resources, networking | 🟢       |                                                                                                                                                                                                                                                                   |
| Map lifecycle         | 🟢       |                                                                                                                                                                                                                                                                   |
| Camera                | 🟡       | [#26](https://github.com/sargunv/maplibre-native-ffi/issues/26)                                                                                                                                                                                                   |
| Projection            | 🟢       |                                                                                                                                                                                                                                                                   |
| Map events            | 🟢       |                                                                                                                                                                                                                                                                   |
| Texture rendering     | 🟢       |                                                                                                                                                                                                                                                                   |
| Style mutation        | ❌       | [#15](https://github.com/sargunv/maplibre-native-ffi/issues/15)                                                                                                                                                                                                   |
| Feature queries       | ❌       | [#17](https://github.com/sargunv/maplibre-native-ffi/issues/17)                                                                                                                                                                                                   |
| Geometry, values      | ❌       | [#19](https://github.com/sargunv/maplibre-native-ffi/issues/19)                                                                                                                                                                                                   |
| Expressions, filters  | ❌       | [#30](https://github.com/sargunv/maplibre-native-ffi/issues/30)                                                                                                                                                                                                   |
| Sources, layers       | ❌       | [#29](https://github.com/sargunv/maplibre-native-ffi/issues/29), [#31](https://github.com/sargunv/maplibre-native-ffi/issues/31), [#33](https://github.com/sargunv/maplibre-native-ffi/issues/33)-[#36](https://github.com/sargunv/maplibre-native-ffi/issues/36) |
| Feature state         | ❌       | [#27](https://github.com/sargunv/maplibre-native-ffi/issues/27)                                                                                                                                                                                                   |
| Offline regions       | 🟡       | [#13](https://github.com/sargunv/maplibre-native-ffi/issues/13)                                                                                                                                                                                                   |

Language binding support tracks low-level bindings intended to sit directly
above the C API. No bindings are implemented yet.

| Target               | Support | Example                           | Tracking                                                        |
| -------------------- | ------- | --------------------------------- | --------------------------------------------------------------- |
| Rust                 | ❌      |                                   | [#41](https://github.com/sargunv/maplibre-native-ffi/issues/41) |
| Zig                  | ❌      | [`zig-map`](examples/zig-map)     | [#42](https://github.com/sargunv/maplibre-native-ffi/issues/42) |
| Go                   | ❌      |                                   | [#43](https://github.com/sargunv/maplibre-native-ffi/issues/43) |
| Swift                | ❌      | [`swift-map`](examples/swift-map) | [#44](https://github.com/sargunv/maplibre-native-ffi/issues/44) |
| Kotlin/Native        | ❌      |                                   | [#46](https://github.com/sargunv/maplibre-native-ffi/issues/46) |
| Java FFM             | ❌      |                                   | [#45](https://github.com/sargunv/maplibre-native-ffi/issues/45) |
| Java JNI / Android   | ❌      |                                   | [#47](https://github.com/sargunv/maplibre-native-ffi/issues/47) |
| C# / .NET            | ❌      |                                   | [#48](https://github.com/sargunv/maplibre-native-ffi/issues/48) |
| Python               | ❌      |                                   | [#49](https://github.com/sargunv/maplibre-native-ffi/issues/49) |
| TypeScript / Node.js | ❌      |                                   | [#50](https://github.com/sargunv/maplibre-native-ffi/issues/50) |
| Dart                 | ❌      |                                   | [#51](https://github.com/sargunv/maplibre-native-ffi/issues/51) |

## License

BSD 3-Clause. See [LICENSE](LICENSE).

Copyright (c) 2026 MapLibre contributors.
