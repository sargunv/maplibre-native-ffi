# Offline Region Follow-Up Design

This temporary design note is for contributors adding the next C ABI slice of
MapLibre Native offline region management. The tile-pyramid storage lifecycle is
already implemented; this note focuses on download control, runtime events, and
database merge while keeping behavior aligned with native MapLibre APIs.

## Scope

The next slice should expose native offline download control and merge behavior
without adding higher-level execution models. Geometry offline regions remain
out of scope until the shared geometry/value ABI lands in
[#19](https://github.com/sargunv/maplibre-native-ffi/issues/19).

Offline region coverage is tracked in
[#13](https://github.com/sargunv/maplibre-native-ffi/issues/13). Runtime-owned
event delivery was tracked in
[#14](https://github.com/sargunv/maplibre-native-ffi/issues/14).

## Native References

Follow these native APIs directly:

- `mbgl::DatabaseFileSource::setOfflineRegionObserver`,
  `setOfflineRegionDownloadState`, `getOfflineRegionStatus`, and
  `mergeOfflineRegions` in
  `third_party/maplibre-native/include/mbgl/storage/database_file_source.hpp`.
- `mbgl::OfflineRegionObserver::statusChanged`, `responseError`, and
  `mapboxTileCountLimitExceeded` in
  `third_party/maplibre-native/include/mbgl/storage/offline.hpp`.
- `mbgl::OfflineDownload` in
  `third_party/maplibre-native/platform/default/include/mbgl/storage/offline_download.hpp`.
- `mbgl::Response::Error::Reason` in
  `third_party/maplibre-native/include/mbgl/storage/response.hpp`.

## Download Control

MapLibre Native exposes observer registration and download state changes as
separate operations. The C ABI should keep that explicit shape instead of
implicitly observing when activating a download.

Add runtime-level commands that take a region ID:

- `mln_runtime_offline_region_set_observed(runtime, region_id, observed)`;
- `mln_runtime_offline_region_set_download_state(runtime, region_id, state)`.

Both commands validate the runtime owner thread and synchronously validate that
the region exists before forwarding to `DatabaseFileSource`. Return status means
the command was accepted by the C ABI/native entry point. Later progress or
download failures are reported as runtime events.

Disabling observation should unregister the native observer. Deleting a region
should also remove any native observer/download state for that region.

## Runtime Events

Observer callbacks run on MapLibre's database thread. The C ABI observer must
copy callback data into runtime-owned event storage and must not expose callback
thread pointers to hosts.

Add runtime event types for:

- offline region status changed;
- offline region response error;
- offline region tile count limit exceeded.

Use `source_type = MLN_RUNTIME_EVENT_SOURCE_RUNTIME` and `source = runtime` for
all offline events. Payloads should contain plain copied data only:

- status events carry `region_id` and `mln_offline_region_status`;
- response-error events carry `region_id` and a reason mapped from
  `mbgl::Response::Error::Reason` to `mln_resource_error_reason`; the native
  error message goes in `mln_runtime_event.message`;
- tile-count-limit events carry `region_id` and the native limit.

Queued events for a region should be discarded when that region is deleted. Late
native callbacks should check that the region is still observed before
enqueueing. This keeps deletion from producing stale region events after the C
ABI has accepted the delete operation.

## Database Merge

Expose native merge as a runtime-level blocking database operation:

- `mln_runtime_offline_regions_merge_database(runtime, side_database_path,
  out_regions)`.

The function should return exactly the `OfflineRegions` value delivered by
`DatabaseFileSource::mergeOfflineRegions`, wrapped in the existing
`mln_offline_region_list` snapshot handle. Do not reinterpret the result as a
full post-merge list or as only newly-created regions unless MapLibre Native
does so.

Document native side effects: MapLibre may upgrade the side database in place,
so the side database path must be writable when native merge requires it.

## Errors

Synchronous validation failures use the existing status categories from
`docs/development.md`.

Native database callback failures convert to `MLN_STATUS_NATIVE_ERROR` with a
thread-local diagnostic. Native download/response failures delivered through
`OfflineRegionObserver::responseError` are runtime events, not synchronous
status failures.

For response-error event payloads, reuse `mln_resource_error_reason` rather than
adding a second equivalent enum:

- `Response::Error::Reason::Success` -> `MLN_RESOURCE_ERROR_REASON_NONE`;
- `NotFound` -> `MLN_RESOURCE_ERROR_REASON_NOT_FOUND`;
- `Server` -> `MLN_RESOURCE_ERROR_REASON_SERVER`;
- `Connection` -> `MLN_RESOURCE_ERROR_REASON_CONNECTION`;
- `RateLimit` -> `MLN_RESOURCE_ERROR_REASON_RATE_LIMIT`;
- `Other` -> `MLN_RESOURCE_ERROR_REASON_OTHER`.

## Validation

C ABI tests should cover:

- explicit observe/unobserve calls and invalid region IDs;
- active/inactive download state commands;
- status-changed events for a small downloadable region;
- response-error events from a failing resource provider or local test server;
- tile-count-limit events when native limits are exceeded, if practical;
- delete discarding queued or late events for that region;
- merge returning the native callback region list and preserving metadata.

Use `mise run test` for the final verification pass.
