# Offline Region C API Design

This temporary design note is for contributors adding the first C ABI slice of
MapLibre Native offline region management. It records the API boundary before
implementation so the initial patch can stay focused on tile-pyramid regions.

## Scope

The first slice exposes tile-pyramid offline regions through the runtime. It
does not expose geometry regions yet. Geometry region creation returns
`MLN_STATUS_UNSUPPORTED` until the shared geometry/value ABI is designed in
[#19](https://github.com/sargunv/maplibre-native-ffi/issues/19).

The API uses runtime-owned event delivery because offline downloads are
runtime-level operations. The prerequisite runtime event model was tracked in
[#14](https://github.com/sargunv/maplibre-native-ffi/issues/14). Offline region
coverage is tracked in
[#13](https://github.com/sargunv/maplibre-native-ffi/issues/13).

## Native References

MapLibre Native provides the lower-level building blocks:

- `mbgl::OfflineTilePyramidRegionDefinition`,
  `mbgl::OfflineGeometryRegionDefinition`, `mbgl::OfflineRegionDefinition`,
  `mbgl::OfflineRegionMetadata`, `mbgl::OfflineRegionStatus`,
  `mbgl::OfflineRegionObserver`, and `mbgl::OfflineRegion` in
  `third_party/maplibre-native/include/mbgl/storage/offline.hpp`.
- `mbgl::DatabaseFileSource` asynchronous offline methods in
  `third_party/maplibre-native/include/mbgl/storage/database_file_source.hpp`.
- Default-platform database and download implementations in
  `third_party/maplibre-native/platform/default/include/mbgl/storage/offline_database.hpp`
  and
  `third_party/maplibre-native/platform/default/include/mbgl/storage/offline_download.hpp`.

## Public Model

Offline management is a runtime feature. Region commands take an `mln_runtime*`,
validate the runtime owner thread, and block until MapLibre's database callback
reports completion when the native operation is asynchronous.

Regions are addressed by stable `int64_t` IDs instead of live handles. Returned
region data uses snapshot/list handles that own copied native values. Borrowed
strings and metadata pointers inside a snapshot or list remain valid until that
snapshot or list is destroyed.

This avoids implying that a public region handle stays attached to live native
download state. Download control uses the runtime plus region ID.

## ABI Shape

The public types should include:

- `mln_offline_region_id` as `int64_t`;
- `mln_offline_region_definition_type` with tile-pyramid and geometry values;
- `mln_offline_region_download_state` with inactive and active values;
- `mln_lat_lng_bounds` for tile-pyramid bounds;
- `mln_offline_tile_pyramid_region_definition`;
- `mln_offline_region_definition`, tagged by definition type;
- `mln_offline_region_status` mirroring `mbgl::OfflineRegionStatus`;
- opaque `mln_offline_region_snapshot` and `mln_offline_region_list` handles;
- `mln_offline_region_info` for copied region data borrowed from those handles.

The first implementation validates tile-pyramid definitions and returns
`MLN_STATUS_UNSUPPORTED` for geometry definitions. A TODO comment in the
geometry branch should point to
`https://github.com/sargunv/maplibre-native-ffi/issues/19`.

## Operations

The first slice should expose database operations that can be validated without
network downloads:

- create a tile-pyramid offline region with opaque binary metadata;
- get a region by ID;
- list regions;
- update metadata;
- get completed status;
- invalidate a region;
- delete a region.

`mergeDatabase` and active download control can be added after the basic storage
surface is covered. Download progress/error events should use
`mln_runtime_poll_event()` rather than host callbacks.

## Validation

C ABI tests should cover:

- validation failures for null pointers, unknown definition types, undersized
  structs, invalid zooms, invalid bounds, and unsupported geometry;
- create/list/get/update metadata through a persistent cache path;
- reload from the same cache path in a new runtime;
- completed-status query for a newly-created region;
- invalidate and delete operations.

Use `mise run test` for the final verification pass.
