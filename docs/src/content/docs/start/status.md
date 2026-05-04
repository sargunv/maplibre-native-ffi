---
title: Status
description: Current API coverage, stability, platform support, and backend support.
---

MapLibre Native FFI is pre-1.0. The C ABI is unstable while `mln_c_version()`
returns `0`.

The C API covers most of the MapLibre Native API surface. It excludes advanced
extension APIs, including custom layers and shader registry access.

Reference documentation comes from public surfaces. The C reference is available
now. Generated binding references will appear when bindings are supported.

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
