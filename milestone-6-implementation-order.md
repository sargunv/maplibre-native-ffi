# Milestone 6 Implementation Order

Milestone 6 builds the style API surface. Implement it by dependency shape, not
strict issue order: first define shared value and mutation contracts, then add
source and layer families that reuse those contracts.

`partial #XX` means the PR should leave the issue open. `resolves #XX` marks the
step that should close the issue if its planned scope is complete.

## Order

1. Style value foundation:
   [partial #30](https://github.com/sargunv/maplibre-native-ffi/issues/30)

   Define the shared ABI contract for style-spec values, expressions, filters,
   and property conversion. Reuse or extend the existing `mln_json_value` model
   where practical, because feature state and GeoJSON already depend on it.
   Establish parse and conversion diagnostics before source and layer APIs rely
   on the representation.

2. Base style mutation surface:
   [partial #15](https://github.com/sargunv/maplibre-native-ffi/issues/15),
   [resolves #30](https://github.com/sargunv/maplibre-native-ffi/issues/30)

   Add the generic style source and layer registry APIs: add, remove, get, list,
   layer insertion before another layer, layer property get/set by style-spec
   name, and filter get/set. Keep this JSON-first and avoid generated per-layer
   C APIs.

3. Core source and data helpers:
   [partial #15](https://github.com/sargunv/maplibre-native-ffi/issues/15)

   Add focused helpers for ordinary `GeoJSONSource`, `VectorSource`, and plain
   `RasterSource`: URL, tileset, inline GeoJSON insertion, and GeoJSON data
   updates. Use the existing geometry, feature, and value ABI to prove the base
   registry through public C API tests.

4. Style light APIs:
   [resolves #34](https://github.com/sargunv/maplibre-native-ffi/issues/34)

   Add style-level light set/get/property mutation through the same generic
   property conversion path. This validates style property conversion without
   source and layer ordering complexity.

5. Runtime style images and shared pixel descriptor:
   [resolves #15](https://github.com/sargunv/maplibre-native-ffi/issues/15),
   [partial #36](https://github.com/sargunv/maplibre-native-ffi/issues/36),
   [partial #33](https://github.com/sargunv/maplibre-native-ffi/issues/33)

   Add style image add/remove and optional get if native readback is useful.
   Define caller-owned premultiplied RGBA8 image descriptor and ownership rules.
   This prepares image sources and location indicator image-name workflows.

6. Image source APIs:
   [resolves #36](https://github.com/sargunv/maplibre-native-ffi/issues/36)

   Add `ImageSource` creation and updates for coordinates, URL, image pixels,
   and coordinate readback. This depends on base source mutation and the shared
   pixel descriptor.

7. Raster DEM, hillshade, and color relief APIs:
   [resolves #35](https://github.com/sargunv/maplibre-native-ffi/issues/35)

   Add `RasterDEMSource` plus `HillshadeLayer` and `ColorReliefLayer` support.
   Prefer the base JSON/property APIs, with specific diagnostics for raster
   encoding and color-ramp conversion.

8. Location indicator layer APIs:
   [resolves #33](https://github.com/sargunv/maplibre-native-ffi/issues/33)

   Add `LocationIndicatorLayer` insertion and property updates, including
   location, bearing, accuracy, and image-name properties. Keep location
   services, tracking, permissions, and camera-following outside the C ABI.

9. Custom geometry source APIs:
   [resolves #29](https://github.com/sargunv/maplibre-native-ffi/issues/29)

   Add host tile callbacks, cancel callbacks, tile data submission, tile and
   region invalidation, zoom range, tile options, and lifecycle rules. Reuse
   GeoJSON/value conversion and runtime-owned callback conventions.

10. Custom render layer investigation:
    [resolves #31](https://github.com/sargunv/maplibre-native-ffi/issues/31)

    Timebox this as a design investigation. Decide whether to expose
    `CustomLayer`, `CustomDrawableLayer`, both, or neither. Treat implementation
    as last in the milestone, or move it out, because it introduces
    backend-specific host render callbacks and render-session coupling.

## Dependency Summary

`#30` blocks the generic style mutation surface. `#15` provides the base source
and layer substrate. `#34`, `#35`, `#36`, and `#33` are incremental slices on
top of that substrate. `#29` is separate but should reuse the same GeoJSON,
value, and callback conventions. `#31` is backend-specific and should not block
the rest of the milestone.
