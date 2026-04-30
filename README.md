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

## Introduction

This is an experimental C ABI wrapper around
[MapLibre Native](https://github.com/maplibre/maplibre-native).

It's meant for writing language bindings to other language ecosystems,
especially those that don't easily interop with C++. Framework concerns
(gestures, widgets, declarative UI) are intentionally left out and belong in
downstream adapters.

The goal is to build, in this repo, a complete C wrapper and low level language
bindings to a wide variety of programming languages and runtimes.

The C ABI is **unstable**. Expect breaking changes until we're feature complete.

## Support Matrix

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

## API Coverage

| Domain                | Coverage | Tracking                                                                                                                                                                                                                                                          |
| --------------------- | -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ABI contract          | 🟢       |                                                                                                                                                                                                                                                                   |
| Diagnostics, logging  | 🟢       |                                                                                                                                                                                                                                                                   |
| Runtime lifecycle     | 🟢       |                                                                                                                                                                                                                                                                   |
| Resources, networking | 🟢       |                                                                                                                                                                                                                                                                   |
| Map lifecycle         | 🟢       |                                                                                                                                                                                                                                                                   |
| Camera                | 🟡       | [#26](https://github.com/sargunv/maplibre-native-ffi/issues/26)                                                                                                                                                                                                   |
| Map events            | 🟡       | [#39](https://github.com/sargunv/maplibre-native-ffi/issues/39)                                                                                                                                                                                                   |
| Texture rendering     | 🟡       | [#4](https://github.com/sargunv/maplibre-native-ffi/issues/4), [#9](https://github.com/sargunv/maplibre-native-ffi/issues/9), [#38](https://github.com/sargunv/maplibre-native-ffi/issues/38)                                                                     |
| Style mutation        | ❌       | [#15](https://github.com/sargunv/maplibre-native-ffi/issues/15)                                                                                                                                                                                                   |
| Feature queries       | ❌       | [#17](https://github.com/sargunv/maplibre-native-ffi/issues/17)                                                                                                                                                                                                   |
| Projection            | ❌       | [#18](https://github.com/sargunv/maplibre-native-ffi/issues/18)                                                                                                                                                                                                   |
| Geometry, values      | ❌       | [#19](https://github.com/sargunv/maplibre-native-ffi/issues/19)                                                                                                                                                                                                   |
| Expressions, filters  | ❌       | [#30](https://github.com/sargunv/maplibre-native-ffi/issues/30)                                                                                                                                                                                                   |
| Sources, layers       | ❌       | [#29](https://github.com/sargunv/maplibre-native-ffi/issues/29), [#31](https://github.com/sargunv/maplibre-native-ffi/issues/31), [#33](https://github.com/sargunv/maplibre-native-ffi/issues/33)-[#36](https://github.com/sargunv/maplibre-native-ffi/issues/36) |
| Feature state         | ❌       | [#27](https://github.com/sargunv/maplibre-native-ffi/issues/27)                                                                                                                                                                                                   |
| Offline regions       | ❌       | [#13](https://github.com/sargunv/maplibre-native-ffi/issues/13)                                                                                                                                                                                                   |

Current coverage is defined by
[`include/maplibre_native_c.h`](./include/maplibre_native_c.h).

## Trying It Out

See [`include/maplibre_native_c.h`](./include/maplibre_native_c.h) for the
public API.

Examples are standalone sub-projects under [`examples/`](examples/):

```bash
# macOS and Linux
mise run //examples/zig-map:run
```

```bash
# macOS only
mise run //examples/swift-map:run
```

## License

BSD 3-Clause. See [LICENSE](LICENSE).

Copyright (c) 2026 MapLibre contributors.
