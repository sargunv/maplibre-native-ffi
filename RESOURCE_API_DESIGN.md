# Resource API Design

## Recommendation

Use MapLibre Native's `MainResourceLoader` for built-in resource dispatch and
wrap only the `Network` file source to expose a runtime-scoped provider
extension point. This keeps the ABI focused on ownership, threading, C structs,
and error conversion instead of mirroring MapLibre's resource waterfall.

The milestone boundary is implementation sequencing, not architecture. Runtime
options control caller-owned paths and cache size; process-global network status
is exposed as a separate ABI surface matching MapLibre Native. Built-in local,
package, cache, and network behavior stays native except where the ABI must
bridge callbacks across the C boundary.

Implementation status: M2.1, M2.2, M2.3, M2.5, and M2.6 are implemented in the
wrapper. M2.4 offline region APIs remain deferred because region handle
ownership, metadata buffer ownership, observer/event delivery, and download
status semantics need a separate ABI design pass; the shared
`DatabaseFileSource` prerequisite is in place through the ambient-cache
implementation.

## Goals

- Provide a real MapLibre resource loader behind the C ABI.
- Let maps load styles, sprites, glyphs, tiles, and URL-backed sources through a
  stable runtime-owned resource subsystem.
- Support host-provided bytes for custom schemes such as `apk://`, `bundle://`,
  or application package URLs.
- Add normal MapLibre network loading, ambient cache, and offline data as
  first-class ABI features in later slices.
- Keep platform policy explicit: callers configure paths and policy knobs; the
  ABI and MapLibre own request routing, caching, database behavior, and offline
  APIs.

## Non-Goals

- Do not require callers to reimplement HTTP, ambient cache, or offline region
  handling for normal web maps.
- Do not silently enable network or persistent cache behavior without public ABI
  options and tests.
- Do not expose MapLibre C++ ownership, types, or exceptions through the C ABI.
- Do not mirror MapLibre's built-in loader waterfall in wrapper code.

## Ownership

- `FileSourceManager` hook: process-global, matching MapLibre's singleton entry
  point.
- Resource configuration, provider registry, and network provider interception:
  runtime-scoped.
- Maps: share the loader and provider state owned by their `mln_runtime`.
- Requests: own cancellation state through their `mbgl::AsyncRequest` handle.
- Runtime identity: each runtime owns a unique `platformContext` so MapLibre's
  internal file-source cache does not accidentally share sources across
  runtimes.

Runtime-scoped resource behavior must not mutate while maps are live. Runtime
destruction remains blocked while maps exist; map teardown must release or
cancel style/resource requests before runtime state is destroyed.

Runtime isolation depends on consistently using the runtime-unique
`platformContext`. MapLibre's file-source cache key does not include
`assetPath`, maximum cache size, client options, or wrapper policy flags.

## Architecture

```text
mbgl::Map
  -> FileSourceManager::get()
  -> AbiFileSourceManager
  -> MainResourceLoader
       -> AssetProvider
       -> MBTilesProvider
       -> PMTilesProvider
       -> FileProvider
       -> CacheProvider / DatabaseFileSource
       -> AbiNetworkFileSource
             -> runtime ABI network provider
            -> OnlineFileSource
```

`FileSourceType::ResourceLoader` is native `MainResourceLoader`.
`FileSourceType::Network` is an ABI wrapper that invokes the runtime provider
extension point and delegates pass-through network-capable resources to native
`OnlineFileSource`.

The wrapper must integrate at MapLibre's process-global `FileSourceManager`
entry point. In practice this means providing the platform
`FileSourceManager::get()` implementation or registering a global
`FileSourceType::ResourceLoader` factory that returns runtime-isolated loaders
keyed by the runtime's `platformContext`; `mbgl::Map` does not accept a
per-runtime `FileSourceManager` instance directly.

All native children required by `MainResourceLoader` are registered in the ABI
`FileSourceManager`. Runtime-specific policy that native sources do not read
directly, such as maximum ambient cache size, is applied at the corresponding
source factory boundary.

## Runtime Configuration

Resource configuration is runtime-level. Maps created from a runtime use options
derived from that runtime.

Fields by milestone:

- `asset_path`: filesystem root for default `asset://` resolution.
- `cache_path`: path for ambient cache and offline database storage.
- no `cache_path`: MapLibre's default in-memory database path (`:memory:`),
  useful for transient runtime cache behavior without filesystem persistence.
- `maximum_cache_size`: ambient cache size limit.
- `tile_server_options`: network-slice option for base URL, scheme alias, source
  templates, default style URLs, and API-key query parameter behavior.
- `api_key`: network-slice provider token where applicable.
- runtime identity: ABI-owned runtime-unique `platformContext`.

Callers provide paths and policy. The ABI translates those options to MapLibre
`ResourceOptions`, `ClientOptions`, and offline/cache APIs.

Resource identity options are creation-time policy for active maps. Changing
`asset_path` or `cache_path` while maps are live is out of scope. Cache size is
applied through `DatabaseFileSource` APIs, not by relying only on
`ResourceOptions::maximumCacheSize()`.

## Dispatch

Native `MainResourceLoader` owns dispatch order for asset, MBTiles, PMTiles,
file, database/cache, and network. When a request reaches the network source,
the ABI wrapper invokes the runtime provider. The provider either handles the
request through the ABI request handle or returns pass-through so native
`OnlineFileSource` handles it.

Cache and network dispatch must respect MapLibre `Resource::LoadingMethod`:
database handles cache requests, and online sources handle network requests.

## Custom Providers

Custom providers are runtime-scoped network extension points registered before
map creation. Dynamic registration is out of scope until there is a concrete
need for loader synchronization, cache invalidation, in-flight request handling,
and style/source reload semantics.

ABI shape:

```c
typedef struct mln_resource_provider {
  uint32_t size;
  mln_resource_provider_callback callback;
  void* user_data;
} mln_resource_provider;

typedef uint32_t (*mln_resource_provider_callback)(
  void* user_data,
  const mln_resource_request* request,
  mln_resource_request_handle* handle
);
```

Contract:

- The ABI exposes a network provider extension point, not a scheme registry.
- Requests that native `MainResourceLoader` handles before network dispatch are
  never presented to the provider.
- Providers are an ABI-owned extension implemented behind a Core `FileSource`;
  request completion must still follow Core `FileSource` threading and callback
  semantics.
- The callback is a runtime-scoped native callback, not a process-global log
  callback and not a polled map event.
- The callback is invoked from the network file-source request path and must
  return a `mln_resource_provider_decision` immediately.
- Returning pass-through delegates the request to native `OnlineFileSource`.
- Returning handle means the callback may complete inline or later by calling
  `mln_resource_request_complete` with the request handle.
- Returned bytes and strings only need to live until completion returns; native
  code copies them before completing the MapLibre response.
- Provider failures convert to MapLibre `Response::Error`, then flow through the
  same native observer paths as other resource failures.
- The callback must not call back into the same runtime unless a future async
  provider API explicitly supports reentrancy.
- The callback must not throw through the C ABI. Native code guards C++ callback
  invocation paths and converts failures to status/errors.
- The callback should return quickly. Slow I/O should retain the handle, finish
  on host-owned work, and complete later from any thread.

This preserves the universal ABI model: MapLibre observer notifications are
queued for polling with the payloads Core exposes, custom providers are explicit
runtime-owner-thread callbacks, and only the process-global log callback may run
on MapLibre logging or worker threads.

## Cancellation

- Each request returns an `mbgl::AsyncRequest` implementation whose destructor
  cancels pending completion.
- Posted provider invocations check cancellation before touching runtime-owned
  provider registry or invoking callbacks.
- Posted completions check cancellation before touching MapLibre response
  callbacks or map observer event state.
- The loader does not retain raw runtime state beyond the lifetime guaranteed by
  active maps and requests.

## Provider Decisions

- Built-in `asset://` is reserved and not overrideable by custom providers. This
  preserves MapLibre platform meaning: filesystem assets on default platforms,
  default asset paths such as app bundle resources on Darwin, and APK assets on
  Android.
- Network follows MapLibre Native's online behavior, including its
  process-global network status model.
- `cache_path` and offline database path are the same ABI field when persistence
  is desired. If omitted, MapLibre's default `:memory:` SQLite database is still
  used for transient cache/database behavior.
- Do not expose a runtime provider matrix. Built-in scheme providers are wired
  as Core providers; runtime options only control policy that callers reasonably
  need to choose, such as paths and cache size.
- Expose resource failures with the detail MapLibre Native Core actually
  provides through observer callbacks first. Do not invent richer resource-error
  payloads unless Core exposes that detail or a later wrapper API directly wraps
  a native source that provides it.

## Process-Global Network Status ABI

MapLibre Native exposes `mbgl::NetworkStatus` as process-global state with
`Online` and `Offline` values. The C ABI should wrap that model directly, like
the process-global log callback, rather than adding runtime-scoped network
policy.

Expected ABI shape:

```c
typedef enum mln_network_status {
  MLN_NETWORK_STATUS_ONLINE = 1,
  MLN_NETWORK_STATUS_OFFLINE = 2,
} mln_network_status;

MLN_API mln_status mln_network_status_get(uint32_t* out_status) MLN_NOEXCEPT;
MLN_API mln_status mln_network_status_set(uint32_t status) MLN_NOEXCEPT;
```

Contract:

- The status is process-global, not runtime-scoped.
- `set(ONLINE)` maps to `mbgl::NetworkStatus::Set(Online)`, which wakes native
  subscribers through MapLibre's `Reachable()` path when transitioning from
  offline to online.
- `set(OFFLINE)` maps to `mbgl::NetworkStatus::Set(Offline)`.
- The ABI does not invent per-runtime network enablement.

## Milestones

### M2.1: Resource Manager, Local Files, Network Provider

Goal: establish the permanent `FileSourceManager` architecture and deterministic
local/custom resource loading.

Deliverables:

- Provide/register through MapLibre's process-global `FileSourceManager` entry
  point.
- Register native built-ins, native `MainResourceLoader`, and the ABI network
  wrapper.
- Add runtime `asset_path` and runtime-unique `platformContext`, passing them
  into MapLibre `ResourceOptions` at map creation.
- Implement `file://` loading.
- Implement filesystem `asset://` loading.
- Implement custom network provider interception and dispatch.
- Add cancellation-safe request handling.
- Add ABI tests for successful `file://`, successful `asset://`, successful
  custom scheme, missing resource, invalid provider setting, and inline style
  regression.
- Update the Zig headless smoke to load a visible local style by URL.

Out of scope:

- Built-in network loading.
- Persistent ambient cache behavior.
- Offline regions.
- MBTiles/PMTiles.
- Resource transforms and URL rewriting.

### M2.2: Built-In Network Provider

Goal: add HTTP/HTTPS loading through MapLibre's native network source with
explicit provider wiring, process-global network status, and deterministic
tests.

Expected deliverables:

- Wire the correct MapLibre network provider for the target platform.
- Add process-global `mln_network_status_get` and `mln_network_status_set` APIs
  that wrap `mbgl::NetworkStatus`.
- Add tile server/API key options only as needed for deterministic tests.
- Add tests using a local HTTP fixture server or controlled test endpoint.

### M2.3: Ambient Cache Provider

Goal: add MapLibre-backed persistent cache for styles, tiles, sprites, glyphs,
and source data.

Expected deliverables:

- Add `cache_path`, `maximum_cache_size`, and defined no-cache-path behavior.
- Wire `DatabaseFileSource` or equivalent MapLibre cache provider.
- Apply `maximum_cache_size` through `DatabaseFileSource` cache-maintenance
  APIs.
- Define normal/cache-only/no-network-available request behavior.
- Add tests proving cache files are created and cached data can satisfy a later
  request without network.

### M2.4: Offline Regions and Database APIs

Goal: expose MapLibre offline data management as explicit ABI operations.

Expected deliverables:

- APIs to list, create, delete, invalidate, and query offline regions through
  `DatabaseFileSource`.
- Observer/event plumbing for offline downloads.
- Tests using a controlled source or local server.

### M2.5: MBTiles/PMTiles Providers

Goal: support local package formats as built-in providers.

Expected deliverables:

- Wire MBTiles/PMTiles providers as standard built-in Core scheme handlers.
- URL tests for `mbtiles://` and/or `pmtiles://` fixtures.

### M2.6: Resource Transform

Goal: expose MapLibre Native's `ResourceTransform` behavior if a concrete
product need requires URL transformation.

Expected deliverables:

- Public ABI that maps directly to `mbgl::ResourceTransform` semantics.
- Clear threading and memory ownership contract matching the native callback.

## Source Findings

Core contracts:

- `FileSource::request` callbacks must be asynchronous, delivered on the same
  thread as the request, and that thread must have an active MapLibre `RunLoop`.
- `FileSourceManager` caches sources by tile-server base URL, API key, cache
  path, and `platformContext`; it does not include `assetPath`, maximum cache
  size, client options, or wrapper policy flags.
- Native `MainResourceLoader` should own built-in dispatch and cache-forwarding
  semantics; the ABI wrapper should intercept only at the network source.

Network:

- Network is wrapped at `FileSourceType::Network`; provider-handled requests are
  completed by the ABI and pass-through requests delegate to `OnlineFileSource`.
- On Apple, use default `OnlineFileSource` plus Darwin `HTTPFileSource`
  (`NSURLSession`). Do not add default libcurl `http_file_source.cpp` on Apple
  unless deliberately building a curl platform.
- Darwin networking needs `platform/darwin/core/http_file_source.mm`,
  `platform/darwin/core/native_apple_interface.m`, and the global
  `MLNNativeNetworkManager`/network-configuration behavior. Nil delegate
  fallback exists, but production request/session policy requires later explicit
  ABI design.
- `NetworkStatus` is process-global; the ABI should wrap that model directly
  rather than inventing per-runtime network state.
- `ResourceTransform` rewrites URLs only and is honored by `OnlineFileSource`,
  not by asset, file, or database sources.

Cache and offline:

- `DatabaseFileSource` constructs `OfflineDatabase` from
  `ResourceOptions::cachePath()`.
- `ResourceOptions::maximumCacheSize()` is not enough by itself; maximum ambient
  cache size must be applied through `DatabaseFileSource` APIs.
- Ambient cache and offline regions share one `OfflineDatabase`/SQLite file.
  Ambient clear/evict operations preserve resources linked to offline regions.
- Android sets SQLite temp path explicitly; Darwin creates a cache directory and
  uses a default `cache.db` path.
- Offline region management can be wrapped through `DatabaseFileSource`; it does
  not require using Darwin `MLNOfflineStorage` directly.
- Offline downloads use a `Network` file source internally, so M2.4 depends on a
  functioning network provider from M2.2.
- Cache fallback should try database, return usable cached data when available,
  otherwise fetch network and forward successful responses into the database.

MBTiles and PMTiles:

- `mbtiles://` accepts an absolute local path after the scheme, for example
  `mbtiles:///absolute/path/to/file.mbtiles`.
- `pmtiles://` wraps an inner resource URL, such as
  `pmtiles://file:///absolute/path/to/file.pmtiles` or `pmtiles://https://...`.
- MBTiles is a straightforward optional child provider once SQLite/zlib-related
  sources are wired.
- PMTiles full support is controlled upstream by `MLN_WITH_PMTILES`; the wrapper
  build deliberately forces it on and wires the full Core provider rather than
  the upstream stub.
- Full PMTiles recursively asks `FileSourceManager` for `ResourceLoader` to
  fetch byte ranges from the inner URL. The ABI network wrapper must pass ranged
  requests through to providers or `OnlineFileSource` and otherwise preserve
  MapLibre Native's PMTiles behavior rather than adding wrapper-specific
  nested-URL policy.

Assets and custom schemes:

- `asset://` is already a reserved platform asset scheme: default platforms map
  it to `assetPath`, while Android maps it to `AAssetManager`.
- Async custom provider callbacks are the ABI shape for bundled resources.
  Android's `AssetManagerFileSource` also performs synchronous open/read on its
  worker thread and copies bytes before completing the request.
- Whole-resource inline completions are fine for styles, sprites, glyphs, and
  small bundled resources. Large packages or streaming resources should retain
  the request handle and complete later.
