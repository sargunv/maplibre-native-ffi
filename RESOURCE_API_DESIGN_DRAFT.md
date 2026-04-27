# Resource API Design Draft

This document is intentionally broader than M2.1 so the early local-resource
slice can be built in the final shape instead of as a throwaway implementation.

## Goals

- Provide a real MapLibre resource loader behind the C ABI.
- Let maps load styles, sprites, glyphs, tiles, and URL-backed sources through a
  stable runtime-owned resource subsystem.
- Support host-provided resource bytes for custom schemes such as `apk://`,
  `bundle://`, or application-specific package URLs.
- Support normal MapLibre network loading, ambient cache, and offline data as
  first-class ABI features in later slices.
- Keep platform policy explicit: callers configure paths and policy knobs, while
  the ABI and MapLibre own request routing, caching, database behavior, and
  offline APIs.

## Non-Goals

- Do not require callers to reimplement HTTP, ambient cache, or offline region
  handling just to load normal web maps.
- Do not silently enable network or persistent cache behavior without public ABI
  options and tests.
- Do not expose MapLibre C++ ownership or exceptions through the C ABI.
- Do not make custom providers a replacement for the built-in MapLibre stack.
  Custom providers are an extension point for host-specific resources.

## Target Architecture

The ABI should install a wrapper-owned `FileSourceManager` singleton that
returns an ABI-owned composite loader for `FileSourceType::ResourceLoader`.

The composite loader is the permanent resource-routing point for this wrapper.
It should be designed now with slots for all final provider families, even if
only local/custom providers are enabled in early milestones.

```text
mbgl::Map
  -> FileSourceManager::get()
  -> AbiFileSourceManager
  -> AbiCompositeResourceLoader
       -> CustomSchemeProvider(s)
       -> AssetProvider
       -> FileProvider
       -> MBTilesProvider
       -> PMTilesProvider
       -> CacheProvider / DatabaseFileSource
       -> NetworkProvider / OnlineFileSource or platform network source
```

The composite loader should be null-safe by construction. Disabled providers are
either absent and checked before use, or represented by no-op providers that
return unsupported. It must not reproduce MapLibre's `MainResourceLoader` issue
where `setResourceOptions()` dereferences omitted children.

## Runtime Resource Configuration

Resource configuration should be runtime-level. Maps created from a runtime use
resource options derived from that runtime.

Initial and eventual fields likely include:

- `asset_path`: filesystem root for default `asset://` resolution.
- `cache_path`: path for ambient cache/database storage.
- `offline_path` or `offline_database_path`: only if research confirms this is
  distinct from `cache_path` for the chosen MapLibre platform stack.
- `maximum_cache_size`: ambient cache size limit.
- `network_enabled`: whether built-in outbound network requests may run.
- `cache_enabled`: whether built-in ambient cache is used.
- `offline_enabled`: whether offline database/regions are available.
- `tile_server_options`: base URL, scheme alias, source templates, default style
  URLs, and API-key query parameter behavior.
- `api_key`: provider token where applicable.
- `platform_context`: narrow escape hatch for platform-specific integrations
  where MapLibre requires it.

Callers provide paths and policy. The ABI should translate those options to
MapLibre `ResourceOptions`, `ClientOptions`, and offline/cache APIs.

## Provider Dispatch

The composite loader should route deterministically. A likely order is:

1. Custom registered schemes, exact scheme match.
2. Filesystem `asset://`, using runtime `asset_path`.
3. Local `file://`.
4. `mbtiles://`, when enabled.
5. `pmtiles://`, when enabled.
6. Ambient cache/database, when enabled.
7. Network, when enabled and request policy allows it.
8. Unsupported-resource error.

Custom provider precedence should be explicit. In the first version, custom
providers should not override reserved built-in schemes such as `file`, `asset`,
`http`, or `https`. Override/intercept behavior can be designed later if needed.

## Custom Scheme Provider ABI

Custom providers are runtime-scoped and registered by URL scheme.

Expected shape:

```c
typedef struct mln_resource_provider {
  uint32_t size;
  const char* scheme;
  mln_resource_provider_callback callback;
  void* user_data;
} mln_resource_provider;

typedef struct mln_resource_provider_response {
  uint32_t size;
  const uint8_t* bytes;
  size_t byte_count;
  int32_t error_code;
  const char* error_message;
} mln_resource_provider_response;

typedef mln_status (*mln_resource_provider_callback)(
  void* user_data,
  const char* url,
  mln_resource_provider_response* out_response
);
```

Contract:

- Scheme registration is runtime-owned.
- Schemes are normalized and validated by the ABI.
- Reserved schemes are rejected initially: `file`, `asset`, `http`, `https`.
- Provider callbacks return `mln_status` and fill a response struct.
- Returned bytes only need to live until the callback returns; native code
  copies them before completing the MapLibre response.
- Provider failures convert to MapLibre `Response::Error` and then to map events
  and diagnostics where applicable.
- Provider callbacks must not call back into the same runtime unless a future
  async provider API explicitly supports reentrancy.
- Provider callbacks must not throw through the C ABI. Native code still guards
  C++ callback invocation paths and converts failures to status/errors.

Async host providers are out of scope for the first custom-provider slice. If a
host needs asynchronous network or IPC-backed resources, add an explicit future
ABI rather than overloading this synchronous callback contract.

## Cancellation and Lifetime

The composite loader must own cancellation state independent of `mln_runtime`.
This avoids use-after-free when a map is destroyed while resource callbacks are
queued.

Rules:

- Each request returns an `mbgl::AsyncRequest` implementation whose destructor
  cancels pending completion.
- Posted completions check cancellation before touching runtime-owned provider
  registry or invoking callbacks.
- The loader must not retain raw runtime state beyond the lifetime guaranteed by
  active maps and requests.
- Runtime destruction remains blocked while maps are live. Map teardown must
  release/cancel style/resource requests before runtime state is destroyed.
- If MapLibre caches a file source by `platformContext`, set `platformContext`
  to a runtime-unique value and avoid mutating resource identity while maps are
  live.

## Milestone Slices

### M2.1: Composite Loader Skeleton, Local Files, Custom Schemes

Goal: Establish the permanent composite-loader architecture and deterministic
local/custom resource loading.

Deliverables:

- Replace the null `FileSourceManager` with `AbiFileSourceManager`.
- Add `AbiCompositeResourceLoader` as the only registered
  `FileSourceType::ResourceLoader`.
- Add runtime `asset_path` and optional `cache_path` fields, passing them into
  MapLibre `ResourceOptions` at map creation.
- Implement `file://` loading.
- Implement filesystem `asset://` loading.
- Implement custom scheme provider registration and dispatch.
- Add cancellation-safe request handling.
- Add ABI tests for successful `file://`, successful `asset://`, successful
  custom scheme, missing resource, invalid provider registration, and inline
  style regression.
- Update the Zig headless smoke to load a visible local style by URL.

Out of scope:

- Built-in network loading.
- Persistent ambient cache behavior.
- Offline regions.
- MBTiles/PMTiles.
- Resource transforms and URL rewriting.

### M2.2: Built-In Network Provider

Goal: Add normal HTTP/HTTPS loading through MapLibre's native network source
behind explicit runtime options.

Expected deliverables:

- Add `network_enabled` runtime option, defaulting to disabled until documented.
- Wire the correct MapLibre network provider for the target platform.
- Add tile server/API key options only as needed for deterministic tests.
- Add tests using either a local HTTP fixture server or a fully controlled test
  endpoint. Avoid tests that depend on public internet availability.

Requires further MapLibre research:

- Whether to use `platform/default/src/mbgl/storage/online_file_source.cpp` with
  default `http_file_source.cpp` or Darwin `http_file_source.mm` on Apple.
- Which CMake source files, frameworks, and platform delegates are required for
  Darwin networking.
- How `ClientOptions`, resource transforms, and network status interact with the
  selected provider.
- Whether MapLibre's network provider requires process-global initialization or
  platform-specific lifecycle calls.

### M2.3: Ambient Cache Provider

Goal: Add MapLibre-backed persistent cache for styles, tiles, sprites, glyphs,
and source data where supported.

Expected deliverables:

- Add runtime cache options: `cache_path`, `maximum_cache_size`, and cache
  enabled/disabled behavior.
- Wire `DatabaseFileSource` or equivalent MapLibre cache provider.
- Define normal/cache-only/network-disabled request behavior.
- Add tests that prove cache files are created and cached data can satisfy a
  later request without network, using deterministic fixtures.

Requires further MapLibre research:

- Exact relationship between `ResourceOptions::cachePath()`,
  `DatabaseFileSource`, `OfflineDatabase`, and platform offline storage.
- Required SQLite/temp path setup on each platform.
- Whether ambient cache and offline regions share one database path.
- Which cache maintenance APIs map cleanly to public ABI functions.

### M2.4: Offline Regions and Offline Database APIs

Goal: Expose MapLibre offline data management as explicit ABI operations rather
than implicit resource-loader behavior.

Expected deliverables:

- APIs to list, create, delete, invalidate, and query offline regions if the
  MapLibre Native API supports them on the target platform.
- Observer/event plumbing for offline downloads.
- Tests using a controlled source or local server.

Requires further MapLibre research:

- Current MapLibre Native offline APIs, especially around `OfflineDatabase`,
  region definitions, metadata, merge packs, and observers.
- Platform differences between default, Darwin, and Android offline storage.
- Threading and run-loop constraints for offline operations.

### M2.5: MBTiles/PMTiles Providers

Goal: Support local package formats as built-in providers when needed by product
examples or tests.

Expected deliverables:

- Runtime feature flags or build detection for MBTiles/PMTiles support.
- URL tests for `mbtiles://` and/or `pmtiles://` fixtures.

Requires further MapLibre research:

- Whether the current build should link full PMTiles or stub PMTiles sources.
- Dependencies required by `MBTilesFileSource` and `PMTilesFileSource`.
- Expected URL forms and TileJSON metadata behavior.

### M2.6: Resource Transform / Request Interception

Goal: Add URL/request transformation only if a concrete product use case needs
headers, auth, URL rewriting, or request logging.

Expected deliverables:

- Public transform callback ABI, or a narrower set of request-header/auth APIs.
- Clear threading and memory ownership contract.

Requires further MapLibre research:

- `mbgl::ResourceTransform` behavior and which providers honor it.
- Android and Darwin transform usage.
- Interaction with custom scheme providers and network cache keys.

## MapLibre Source Areas To Research Further

- `include/mbgl/storage/file_source.hpp` — exact request, cancellation, resource
  transform, and option-update contracts.
- `include/mbgl/storage/file_source_manager.hpp` and
  `src/mbgl/storage/file_source_manager.cpp` — source cache identity and factory
  lifetime behavior.
- `platform/default/src/mbgl/storage/main_resource_loader.cpp` — final provider
  dispatch semantics to mirror or intentionally differ from.
- `platform/default/src/mbgl/storage/online_file_source.cpp` — built-in online
  provider behavior.
- `platform/default/src/mbgl/storage/http_file_source.cpp` — libcurl-based HTTP
  requirements and run-loop integration.
- `platform/darwin/core/http_file_source.mm` and
  `platform/darwin/core/native_apple_interface.m` — Darwin HTTP/session
  dependencies and delegate requirements.
- `platform/default/src/mbgl/storage/database_file_source.cpp` — ambient cache
  behavior and `OfflineDatabase` integration.
- `platform/default/src/mbgl/storage/offline.cpp` and related offline files —
  offline region APIs and observers.
- `platform/darwin/src/MLNOfflineStorage.mm` — Darwin cache/offline path setup
  and shared file-source initialization.
- `platform/android/MapLibreAndroid/src/cpp/file_source.cpp` — Android resource
  options, network/cache setup, resource transforms, and asset provider wiring.
- `platform/android/MapLibreAndroid/src/cpp/asset_manager_file_source.cpp` — APK
  asset provider behavior to compare with the custom-scheme ABI.
- `platform/default/src/mbgl/storage/mbtiles_file_source.cpp` — MBTiles URL and
  metadata behavior.
- `platform/default/src/mbgl/storage/pmtiles_file_source.cpp` and
  `pmtiles_file_source_stub.cpp` — PMTiles support/stub selection.

## Open Design Questions

- Should built-in `asset://` be overrideable by a custom provider, or should
  host-specific assets use custom schemes only?
- Should network be disabled by default for deterministic headless use, or
  should MapLibre-style default online behavior be enabled when network provider
  support is linked?
- Should `cache_path` and offline database path be the same ABI field?
- Should resource providers be registered only before map creation, or can later
  registration be made safe with loader-level synchronization and cache busting?
- Is a synchronous custom-provider callback sufficient for APK/bundled
  resources, or is an async completion API required before Android/iOS packaging
  work?
- Which providers should be compile-time optional versus runtime disabled?
- How should resource errors be surfaced in addition to MapLibre map events:
  only existing map failure events, or a richer resource-error event type?

## Current Recommendation

Build M2.1 as the first slice of the final ABI-owned composite loader. Do not
use MapLibre's full default `MainResourceLoader` for M2.1, but do mirror its
useful dispatch concepts and leave explicit provider slots for network, cache,
offline, MBTiles, and PMTiles.

The milestone boundary is provider enablement, not architecture. M2.1 should
create the permanent composite loader and enable only deterministic local/custom
providers. Later M2.x slices should plug in MapLibre's built-in provider
implementations behind explicit ABI options and tests.
