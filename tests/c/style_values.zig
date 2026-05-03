const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

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

fn jsonDouble(value: f64) c.mln_json_value {
    return .{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_DOUBLE,
        .data = .{ .double_value = value },
    };
}

fn jsonBool(value: bool) c.mln_json_value {
    return .{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_BOOL,
        .data = .{ .bool_value = value },
    };
}

fn jsonArray(values: []const c.mln_json_value) c.mln_json_value {
    return .{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_ARRAY,
        .data = .{ .array_value = .{ .values = values.ptr, .value_count = values.len } },
    };
}

fn snapshotRoot(snapshot: *c.mln_json_snapshot) !*const c.mln_json_value {
    var root: ?*const c.mln_json_value = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_json_snapshot_get(snapshot, &root));
    return root orelse error.MissingSnapshotRoot;
}

test "layer properties accept style JSON values" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    const radius = jsonDouble(18.0);
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_set_layer_property(map, stringView("point-circle"), stringView("circle-radius"), &radius),
    );

    var snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_layer_property(map, stringView("point-circle"), stringView("circle-radius"), &snapshot),
    );
    defer c.mln_json_snapshot_destroy(snapshot.?);

    const root = try snapshotRoot(snapshot.?);
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_DOUBLE, root.type);
    try testing.expectEqual(@as(f64, 18.0), root.data.double_value);
}

test "layer filters accept style expression JSON arrays" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    const get_values = [_]c.mln_json_value{ jsonString("get"), jsonString("visible") };
    const get_expr = jsonArray(&get_values);
    const filter_values = [_]c.mln_json_value{ jsonString("=="), get_expr, jsonBool(true) };
    const filter = jsonArray(&filter_values);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_layer_filter(map, stringView("point-circle"), &filter));

    var snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_layer_filter(map, stringView("point-circle"), &snapshot));
    defer c.mln_json_snapshot_destroy(snapshot.?);

    const root = try snapshotRoot(snapshot.?);
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_ARRAY, root.type);
    try testing.expectEqual(@as(usize, 3), root.data.array_value.value_count);
    const first = root.data.array_value.values[0];
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_STRING, first.type);
    try testing.expect(std.mem.eql(u8, first.data.string_value.data[0..first.data.string_value.size], "=="));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_layer_filter(map, stringView("point-circle"), null));
}

test "style value conversion reports invalid descriptors and conversion errors" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    var invalid_descriptor = jsonDouble(std.math.inf(f64));
    try testing.expectEqual(
        c.MLN_STATUS_INVALID_ARGUMENT,
        c.mln_map_set_layer_property(map, stringView("point-circle"), stringView("circle-radius"), &invalid_descriptor),
    );
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    const invalid_property_value = jsonString("not a radius");
    try testing.expectEqual(
        c.MLN_STATUS_INVALID_ARGUMENT,
        c.mln_map_set_layer_property(map, stringView("point-circle"), stringView("circle-radius"), &invalid_property_value),
    );
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
}
