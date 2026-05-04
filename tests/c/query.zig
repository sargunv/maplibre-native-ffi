const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const query_style_json =
    \\{
    \\  "version": 8,
    \\  "name": "zig-c-query-test",
    \\  "sources": {
    \\    "point": {
    \\      "type": "geojson",
    \\      "data": {
    \\        "type": "FeatureCollection",
    \\        "features": [
    \\          {
    \\            "type": "Feature",
    \\            "id": "feature-1",
    \\            "geometry": {"type": "Point", "coordinates": [-122.4194, 37.7749]},
    \\            "properties": {"kind": "capital", "visible": true}
    \\          }
    \\        ]
    \\      }
    \\    }
    \\  },
    \\  "layers": [
    \\    {"id": "background", "type": "background", "paint": {"background-color": "#d8f1ff"}},
    \\    {"id": "point-circle", "type": "circle", "source": "point", "paint": {"circle-color": "#f97316", "circle-radius": 12}}
    \\  ]
    \\}
;

fn stringView(value: []const u8) c.mln_string_view {
    return .{ .data = value.ptr, .size = value.len };
}

fn jsonString(value: []const u8) c.mln_json_value {
    return .{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_STRING,
        .data = .{ .string_value = stringView(value) },
    };
}

fn jsonArray(values: []const c.mln_json_value) c.mln_json_value {
    return .{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_ARRAY,
        .data = .{ .array_value = .{ .values = values.ptr, .value_count = values.len } },
    };
}

fn viewBytes(view: c.mln_string_view) []const u8 {
    return view.data[0..view.size];
}

fn attachTextureSession(map: *c.mln_map) !*c.mln_render_session {
    var descriptor = c.mln_owned_texture_descriptor_default();
    descriptor.width = 64;
    descriptor.height = 64;

    var session: ?*c.mln_render_session = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_owned_texture_attach(map, &descriptor, &session));
    return session orelse error.SessionAttachFailed;
}

fn loadStyleAndRender(runtime: *c.mln_runtime, map: *c.mln_map, session: *c.mln_render_session) !void {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER | c.MLN_CAMERA_OPTION_ZOOM;
    camera.latitude = 37.7749;
    camera.longitude = -122.4194;
    camera.zoom = 10.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_jump_to(map, &camera));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, query_style_json));
    for (0..5) |_| {
        if (!try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE)) break;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_render_update(session));
    }
}

fn expectFeatureKind(feature: *const c.mln_queried_feature, expected: []const u8) !void {
    const properties = feature.feature.properties[0..feature.feature.property_count];
    for (properties) |property| {
        if (std.mem.eql(u8, viewBytes(property.key), "kind")) {
            const value = property.value.?;
            try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_STRING, value.*.type);
            try testing.expect(std.mem.eql(u8, viewBytes(value.*.data.string_value), expected));
            return;
        }
    }
    return error.MissingFeatureKind;
}

test "feature query ABI structs import" {
    const rendered_options = c.mln_rendered_feature_query_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_rendered_feature_query_options)), rendered_options.size);
    try testing.expectEqual(@as(u32, 0), rendered_options.fields);

    const source_options = c.mln_source_feature_query_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_source_feature_query_options)), source_options.size);
    try testing.expectEqual(@as(u32, 0), source_options.fields);

    const geometry = c.mln_rendered_query_geometry_point(.{ .x = 256.0, .y = 256.0 });
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_rendered_query_geometry)), geometry.size);
    try testing.expectEqual(c.MLN_RENDERED_QUERY_GEOMETRY_TYPE_POINT, geometry.type);
}

test "render session queries rendered and source features" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    const session = try attachTextureSession(map);
    defer testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_destroy(session)) catch @panic("session destroy failed");

    const point_geometry = c.mln_rendered_query_geometry_point(.{ .x = 256.0, .y = 256.0 });
    var rendered_result: ?*c.mln_feature_query_result = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_render_session_query_rendered_features(session, &point_geometry, null, &rendered_result));

    try loadStyleAndRender(runtime, map, session);

    var query_point: c.mln_screen_point = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pixel_for_lat_lng(map, .{ .latitude = 37.7749, .longitude = -122.4194 }, &query_point));
    const rendered_geometry = c.mln_rendered_query_geometry_box(.{
        .min = .{ .x = query_point.x - 20.0, .y = query_point.y - 20.0 },
        .max = .{ .x = query_point.x + 20.0, .y = query_point.y + 20.0 },
    });

    var rendered_options = c.mln_rendered_feature_query_options_default();
    const layer_ids = [_]c.mln_string_view{stringView("point-circle")};
    rendered_options.fields = c.MLN_RENDERED_FEATURE_QUERY_OPTION_LAYER_IDS;
    rendered_options.layer_ids = &layer_ids;
    rendered_options.layer_id_count = layer_ids.len;
    const rendered_get_values = [_]c.mln_json_value{ jsonString("get"), jsonString("kind") };
    const rendered_get_expr = jsonArray(&rendered_get_values);
    const rendered_filter_values = [_]c.mln_json_value{ jsonString("=="), rendered_get_expr, jsonString("capital") };
    const rendered_filter = jsonArray(&rendered_filter_values);
    rendered_options.filter = &rendered_filter;

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_query_rendered_features(session, &rendered_geometry, &rendered_options, &rendered_result));
    defer c.mln_feature_query_result_destroy(rendered_result.?);

    var count: usize = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_feature_query_result_count(rendered_result.?, &count));
    try testing.expectEqual(@as(usize, 1), count);

    var rendered_feature: c.mln_queried_feature = .{ .size = @sizeOf(c.mln_queried_feature), .fields = 0, .feature = undefined, .source_id = .{ .data = null, .size = 0 }, .source_layer_id = .{ .data = null, .size = 0 }, .state = null };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_feature_query_result_get(rendered_result.?, 0, &rendered_feature));
    try testing.expect((rendered_feature.fields & c.MLN_QUERIED_FEATURE_SOURCE_ID) != 0);
    try testing.expect(std.mem.eql(u8, viewBytes(rendered_feature.source_id), "point"));
    try expectFeatureKind(&rendered_feature, "capital");

    var source_options = c.mln_source_feature_query_options_default();
    const source_get_values = [_]c.mln_json_value{ jsonString("get"), jsonString("kind") };
    const source_get_expr = jsonArray(&source_get_values);
    const source_filter_values = [_]c.mln_json_value{ jsonString("=="), source_get_expr, jsonString("capital") };
    const source_filter = jsonArray(&source_filter_values);
    source_options.filter = &source_filter;

    var source_result: ?*c.mln_feature_query_result = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_query_source_features(session, stringView("point"), &source_options, &source_result));
    defer c.mln_feature_query_result_destroy(source_result.?);

    count = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_feature_query_result_count(source_result.?, &count));
    try testing.expectEqual(@as(usize, 1), count);

    var source_feature: c.mln_queried_feature = .{ .size = @sizeOf(c.mln_queried_feature), .fields = 0, .feature = undefined, .source_id = .{ .data = null, .size = 0 }, .source_layer_id = .{ .data = null, .size = 0 }, .state = null };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_feature_query_result_get(source_result.?, 0, &source_feature));
    try testing.expect((source_feature.fields & c.MLN_QUERIED_FEATURE_SOURCE_ID) != 0);
    try testing.expect(std.mem.eql(u8, viewBytes(source_feature.source_id), "point"));
    try expectFeatureKind(&source_feature, "capital");
}

test "feature query validation" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    const session = try attachTextureSession(map);
    defer testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_destroy(session)) catch @panic("session destroy failed");

    var result: ?*c.mln_feature_query_result = null;
    var geometry = c.mln_rendered_query_geometry_point(.{ .x = 256.0, .y = 256.0 });
    geometry.size = @sizeOf(c.mln_rendered_query_geometry) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_render_session_query_rendered_features(session, &geometry, null, &result));

    geometry = c.mln_rendered_query_geometry_point(.{ .x = std.math.inf(f64), .y = 0.0 });
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_render_session_query_rendered_features(session, &geometry, null, &result));

    var rendered_options = c.mln_rendered_feature_query_options_default();
    rendered_options.fields = @as(u32, 1) << 31;
    geometry = c.mln_rendered_query_geometry_point(.{ .x = 256.0, .y = 256.0 });
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_render_session_query_rendered_features(session, &geometry, &rendered_options, &result));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_render_session_query_source_features(session, .{ .data = null, .size = 1 }, null, &result));
}
