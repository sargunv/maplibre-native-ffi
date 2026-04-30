# maplibre-native-ffi

Experimental C ABI wrapper around
[MapLibre Native](https://github.com/maplibre/maplibre-native).

This project produces a shared library with a plain C public API wrapping
MapLibre Native. It is meant for writing straightforward language bindings to
other language ecosystems, especially those that don't easily interop with C++.
Framework concerns (gestures, widgets, declarative UI) are intentionally left
out and belong in downstream adapters.

The goal is to build a complete C wrapper and low level language bindings to a
wide variety of programming languages and runtimes.

## Status

The C ABI is **unstable**. Expect breaking changes until we're feature complete.

## Support Matrix

| Platform | Vulkan | Metal | OpenGL |
| -------- | ------ | ----- | ------ |
| Linux    | 🟢     | -     | ❌     |
| macOS    | ❌     | 🟢    | -      |
| Windows  | ❌     | -     | ❌     |
| Android  | ❌     | -     | ❌     |
| iOS      | -      | ❌    | -      |

🟢 current, 🟡 partial, ❌ planned, - not applicable.

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

# macOS only
mise run //examples/swift-map:run
```
