# Issue 17 Feature Query API Plan

This note is for contributors implementing issue 17. It defines the intended C
ABI shape for MapLibre Native rendered feature queries, source feature queries,
and feature extension queries.

## Scope

Expose the useful native query surface without exposing renderer or frontend
objects directly. The C API should provide:

- rendered feature queries for screen points, boxes, and line strings;
- source feature queries for style source IDs;
- feature extension queries such as supercluster children, leaves, and expansion
  zoom.

The implementation can land in two stages. The ABI should be designed for all
three query families before stage 1 lands.

## Placement

Put the query entry points on `mln_render_session`.

MapLibre Native exposes these operations from `mbgl::Renderer`. This repository
already treats renderer-backed operations as render-session responsibilities, as
shown by feature-state APIs on `mln_render_session`. Query calls read the
session renderer's latest state, so they fit the same lifecycle and threading
rules.

Queries are immediate state snapshots. They return synchronous acceptance or
failure and copy result data into owned C handles. They do not enqueue commands
or runtime events.

## Public Header

Add `include/maplibre_native_c/query.h` and include it from
`include/maplibre_native_c.h`. Keep query-specific result handles and option
types in that header.

## Stage 1: Rendered And Source Features

Rendered and source queries share one result handle because both native APIs
return `std::vector<mbgl::Feature>`.

```c
typedef struct mln_feature_query_result mln_feature_query_result;

typedef enum mln_rendered_query_geometry_type : uint32_t {
  MLN_RENDERED_QUERY_GEOMETRY_TYPE_POINT = 1,
  MLN_RENDERED_QUERY_GEOMETRY_TYPE_BOX = 2,
  MLN_RENDERED_QUERY_GEOMETRY_TYPE_LINE_STRING = 3,
} mln_rendered_query_geometry_type;

typedef struct mln_screen_box {
  mln_screen_point min;
  mln_screen_point max;
} mln_screen_box;

typedef struct mln_screen_line_string {
  const mln_screen_point* points;
  size_t point_count;
} mln_screen_line_string;

typedef struct mln_rendered_query_geometry {
  uint32_t size;
  uint32_t type;
  union {
    mln_screen_point point;
    mln_screen_box box;
    mln_screen_line_string line_string;
  } data;
} mln_rendered_query_geometry;

typedef enum mln_rendered_feature_query_option_field : uint32_t {
  MLN_RENDERED_FEATURE_QUERY_OPTION_LAYER_IDS = 1U << 0U,
} mln_rendered_feature_query_option_field;

typedef struct mln_rendered_feature_query_options {
  uint32_t size;
  uint32_t fields;
  const mln_string_view* layer_ids;
  size_t layer_id_count;
  const mln_json_value* filter;
} mln_rendered_feature_query_options;

typedef enum mln_source_feature_query_option_field : uint32_t {
  MLN_SOURCE_FEATURE_QUERY_OPTION_SOURCE_LAYER_IDS = 1U << 0U,
} mln_source_feature_query_option_field;

typedef struct mln_source_feature_query_options {
  uint32_t size;
  uint32_t fields;
  const mln_string_view* source_layer_ids;
  size_t source_layer_id_count;
  const mln_json_value* filter;
} mln_source_feature_query_options;

typedef enum mln_queried_feature_field : uint32_t {
  MLN_QUERIED_FEATURE_SOURCE_ID = 1U << 0U,
  MLN_QUERIED_FEATURE_SOURCE_LAYER_ID = 1U << 1U,
  MLN_QUERIED_FEATURE_STATE = 1U << 2U,
} mln_queried_feature_field;

typedef struct mln_queried_feature {
  uint32_t size;
  uint32_t fields;
  mln_feature feature;
  mln_string_view source_id;
  mln_string_view source_layer_id;
  const mln_json_value* state;
} mln_queried_feature;
```

Expose default constructors and geometry helpers:

```c
MLN_API mln_rendered_feature_query_options
mln_rendered_feature_query_options_default(void) MLN_NOEXCEPT;

MLN_API mln_source_feature_query_options
mln_source_feature_query_options_default(void) MLN_NOEXCEPT;

MLN_API mln_rendered_query_geometry
mln_rendered_query_geometry_point(mln_screen_point point) MLN_NOEXCEPT;

MLN_API mln_rendered_query_geometry
mln_rendered_query_geometry_box(mln_screen_box box) MLN_NOEXCEPT;

MLN_API mln_rendered_query_geometry
mln_rendered_query_geometry_line_string(
  const mln_screen_point* points, size_t point_count
) MLN_NOEXCEPT;
```

Expose query and result access functions:

```c
MLN_API mln_status mln_render_session_query_rendered_features(
  mln_render_session* session,
  const mln_rendered_query_geometry* geometry,
  const mln_rendered_feature_query_options* options,
  mln_feature_query_result** out_result
) MLN_NOEXCEPT;

MLN_API mln_status mln_render_session_query_source_features(
  mln_render_session* session,
  mln_string_view source_id,
  const mln_source_feature_query_options* options,
  mln_feature_query_result** out_result
) MLN_NOEXCEPT;

MLN_API mln_status mln_feature_query_result_count(
  const mln_feature_query_result* result,
  size_t* out_count
) MLN_NOEXCEPT;

MLN_API mln_status mln_feature_query_result_get(
  const mln_feature_query_result* result,
  size_t index,
  mln_queried_feature* out_feature
) MLN_NOEXCEPT;

MLN_API void mln_feature_query_result_destroy(
  mln_feature_query_result* result
) MLN_NOEXCEPT;
```

## Stage 2: Feature Extensions

Feature extensions use a separate result handle because the native return type
is `mbgl::FeatureExtensionValue`, a variant of `mbgl::Value` or
`mbgl::FeatureCollection`. A single feature-list handle would not represent
scalar extension results such as supercluster expansion zoom.

```c
typedef struct mln_feature_extension_result mln_feature_extension_result;

typedef enum mln_feature_extension_result_type : uint32_t {
  MLN_FEATURE_EXTENSION_RESULT_TYPE_VALUE = 1,
  MLN_FEATURE_EXTENSION_RESULT_TYPE_FEATURE_COLLECTION = 2,
} mln_feature_extension_result_type;

typedef struct mln_feature_extension_result_info {
  uint32_t size;
  uint32_t type;
  union {
    const mln_json_value* value;
    mln_feature_collection feature_collection;
  } data;
} mln_feature_extension_result_info;
```

Expose query and result access functions:

```c
MLN_API mln_status mln_render_session_query_feature_extensions(
  mln_render_session* session,
  mln_string_view source_id,
  const mln_feature* feature,
  mln_string_view extension,
  mln_string_view extension_field,
  const mln_json_value* arguments,
  mln_feature_extension_result** out_result
) MLN_NOEXCEPT;

MLN_API mln_status mln_feature_extension_result_get(
  const mln_feature_extension_result* result,
  mln_feature_extension_result_info* out_info
) MLN_NOEXCEPT;

MLN_API void mln_feature_extension_result_destroy(
  mln_feature_extension_result* result
) MLN_NOEXCEPT;
```

## Filters And Arguments

`filter` uses the MapLibre style-spec filter JSON representation already used by
`mln_map_set_layer_filter()`. Null means no filter.

`arguments` is optional. When non-null, it must be a JSON object descriptor and
is copied before return. This covers native extension arguments such as
supercluster `limit` and `offset`.

## Ownership

Input strings, arrays, geometry descriptors, filters, and argument descriptors
are borrowed for the duration of the call.

Query result handles own copied result data. Pointers returned by
`mln_feature_query_result_get()` and `mln_feature_extension_result_get()` remain
valid until the corresponding result handle is destroyed.

`mln_feature_query_result_destroy()` and
`mln_feature_extension_result_destroy()` accept null as a no-op.

## Errors

Query functions return:

- `MLN_STATUS_OK` on success;
- `MLN_STATUS_INVALID_ARGUMENT` for null handles, non-live handles, undersized
  structs, unknown field bits, invalid strings, invalid filters, invalid
  geometry, invalid arguments, null outputs, or non-null `*out_result`;
- `MLN_STATUS_INVALID_STATE` when the session is detached or no renderer has
  been created for the session yet;
- `MLN_STATUS_WRONG_THREAD` when called from a thread other than the session
  owner thread;
- `MLN_STATUS_NATIVE_ERROR` when a native MapLibre error or C++ exception is
  converted to status.

Unknown rendered layer IDs and unknown source IDs should follow native behavior
and produce empty or null-like results rather than hard failures.

## Validation

Stage 1 should add C ABI tests for:

- rendered point query with no options;
- rendered query with layer IDs;
- rendered query with a filter;
- source query for GeoJSON;
- source query for vector source layers;
- result ownership after later map/session changes;
- invalid structs, invalid field bits, invalid filter, wrong thread, and
  not-yet-rendered session errors.

Stage 2 should add C ABI tests for:

- supercluster `children` returning a feature collection;
- supercluster `leaves` with `limit` and `offset` arguments;
- supercluster `expansion-zoom` returning a value;
- unknown extension returning the native null-like value;
- invalid argument objects and wrong result-handle initialization.
