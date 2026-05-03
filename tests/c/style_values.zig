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

fn jsonObject(members: []const c.mln_json_member) c.mln_json_value {
    return .{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_OBJECT,
        .data = .{ .object_value = .{ .members = members.ptr, .member_count = members.len } },
    };
}

fn jsonMember(key: []const u8, value: *const c.mln_json_value) c.mln_json_member {
    return .{ .key = stringView(key), .value = value };
}

fn viewBytes(view: c.mln_string_view) []const u8 {
    return view.data[0..view.size];
}

fn listId(list: *c.mln_style_id_list, index: usize) ![]const u8 {
    var id: c.mln_string_view = .{ .data = null, .size = 0 };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_style_id_list_get(list, index, &id));
    return viewBytes(id);
}

fn expectListContains(list: *c.mln_style_id_list, expected: []const u8) !void {
    var count: usize = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_style_id_list_count(list, &count));
    for (0..count) |index| {
        if (std.mem.eql(u8, try listId(list, index), expected)) return;
    }
    return error.MissingListEntry;
}

fn listIndexOf(list: *c.mln_style_id_list, expected: []const u8) !usize {
    var count: usize = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_style_id_list_count(list, &count));
    for (0..count) |index| {
        if (std.mem.eql(u8, try listId(list, index), expected)) return index;
    }
    return error.MissingListEntry;
}

fn expectObjectString(root: *const c.mln_json_value, key: []const u8, expected: []const u8) !void {
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_OBJECT, root.type);
    const members = root.data.object_value.members[0..root.data.object_value.member_count];
    for (members) |member| {
        if (std.mem.eql(u8, viewBytes(member.key), key)) {
            try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_STRING, member.value.*.type);
            try testing.expect(std.mem.eql(u8, viewBytes(member.value.*.data.string_value), expected));
            return;
        }
    }
    return error.MissingObjectMember;
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

test "style registry exposes primary source and layer ID APIs" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    var source_ids: ?*c.mln_style_id_list = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_list_style_source_ids(map, &source_ids));
    defer c.mln_style_id_list_destroy(source_ids.?);
    var source_count: usize = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_style_id_list_count(source_ids.?, &source_count));
    try testing.expect(source_count >= 1);
    try expectListContains(source_ids.?, "point");

    const feature_collection_type = jsonString("FeatureCollection");
    const empty_features = [_]c.mln_json_value{};
    const features = jsonArray(&empty_features);
    const data_members = [_]c.mln_json_member{
        jsonMember("type", &feature_collection_type),
        jsonMember("features", &features),
    };
    const data = jsonObject(&data_members);
    const source_type = jsonString("geojson");
    const source_members = [_]c.mln_json_member{
        jsonMember("type", &source_type),
        jsonMember("data", &data),
    };
    const source = jsonObject(&source_members);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_add_style_source_json(map, stringView("empty"), &source));

    var exists = false;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_style_source_exists(map, stringView("empty"), &exists));
    try testing.expect(exists);

    var found = false;
    var source_type_value: u32 = c.MLN_STYLE_SOURCE_TYPE_UNKNOWN;
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_source_type(map, stringView("empty"), &source_type_value, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(c.MLN_STYLE_SOURCE_TYPE_GEOJSON, source_type_value);

    var info: c.mln_style_source_info = .{
        .size = @sizeOf(c.mln_style_source_info),
        .type = c.MLN_STYLE_SOURCE_TYPE_UNKNOWN,
        .id_size = 0,
        .is_volatile = false,
        .has_attribution = false,
        .attribution_size = 0,
    };
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_source_info(map, stringView("empty"), &info, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(c.MLN_STYLE_SOURCE_TYPE_GEOJSON, info.type);
    try testing.expectEqual(@as(usize, "empty".len), info.id_size);
    try testing.expect(!info.has_attribution);

    const vector_type = jsonString("vector");
    const tile_url = jsonString("https://example.com/{z}/{x}/{y}.pbf");
    const tile_values = [_]c.mln_json_value{tile_url};
    const tiles = jsonArray(&tile_values);
    const attribution = jsonString("Example attribution");
    const vector_members = [_]c.mln_json_member{
        jsonMember("type", &vector_type),
        jsonMember("tiles", &tiles),
        jsonMember("attribution", &attribution),
    };
    const vector_source = jsonObject(&vector_members);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_add_style_source_json(map, stringView("vector-meta"), &vector_source));
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_source_info(map, stringView("vector-meta"), &info, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(c.MLN_STYLE_SOURCE_TYPE_VECTOR, info.type);
    try testing.expect(info.has_attribution);
    try testing.expectEqual(@as(usize, "Example attribution".len), info.attribution_size);

    var attribution_buffer: [64]u8 = undefined;
    var attribution_size: usize = 0;
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_copy_style_source_attribution(
            map,
            stringView("vector-meta"),
            &attribution_buffer,
            attribution_buffer.len,
            &attribution_size,
            &found,
        ),
    );
    try testing.expect(found);
    try testing.expect(std.mem.eql(u8, attribution_buffer[0..attribution_size], "Example attribution"));

    const layer_id = jsonString("empty-circle");
    const layer_type = jsonString("circle");
    const layer_source = jsonString("empty");
    const layer_members = [_]c.mln_json_member{
        jsonMember("id", &layer_id),
        jsonMember("type", &layer_type),
        jsonMember("source", &layer_source),
    };
    const layer = jsonObject(&layer_members);

    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_add_style_layer_json(map, &layer, stringView("point-circle")),
    );

    var layer_ids: ?*c.mln_style_id_list = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_list_style_layer_ids(map, &layer_ids));
    defer c.mln_style_id_list_destroy(layer_ids.?);
    var layer_count: usize = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_style_id_list_count(layer_ids.?, &layer_count));
    try testing.expect(layer_count >= 3);
    try testing.expect((try listIndexOf(layer_ids.?, "empty-circle")) < (try listIndexOf(layer_ids.?, "point-circle")));

    var layer_type_view: c.mln_string_view = .{ .data = null, .size = 0 };
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_layer_type(map, stringView("empty-circle"), &layer_type_view, &found),
    );
    try testing.expect(found);
    try testing.expect(std.mem.eql(u8, viewBytes(layer_type_view), "circle"));

    var layer_snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_layer_json(map, stringView("empty-circle"), &layer_snapshot, &found),
    );
    defer c.mln_json_snapshot_destroy(layer_snapshot.?);
    try testing.expect(found);
    try expectObjectString(try snapshotRoot(layer_snapshot.?), "id", "empty-circle");

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_move_style_layer(map, stringView("empty-circle"), stringView("")));

    var used_source_removed = false;
    try testing.expectEqual(
        c.MLN_STATUS_INVALID_STATE,
        c.mln_map_remove_style_source(map, stringView("empty"), &used_source_removed),
    );

    var layer_removed = false;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_remove_style_layer(map, stringView("empty-circle"), &layer_removed));
    try testing.expect(layer_removed);
    var source_removed = false;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_remove_style_source(map, stringView("empty"), &source_removed));
    try testing.expect(source_removed);
    var vector_source_removed = false;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_remove_style_source(map, stringView("vector-meta"), &vector_source_removed));
    try testing.expect(vector_source_removed);
}

test "style light accepts full JSON and property updates" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    const light_color = jsonString("blue");
    const light_intensity = jsonDouble(0.3);
    const position_values = [_]c.mln_json_value{ jsonDouble(1.0), jsonDouble(2.0), jsonDouble(3.0) };
    const light_position = jsonArray(&position_values);
    const light_members = [_]c.mln_json_member{
        jsonMember("color", &light_color),
        jsonMember("intensity", &light_intensity),
        jsonMember("position", &light_position),
    };
    const light = jsonObject(&light_members);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_light_json(map, &light));

    var snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_style_light_property(map, stringView("intensity"), &snapshot));
    defer c.mln_json_snapshot_destroy(snapshot.?);
    var root = try snapshotRoot(snapshot.?);
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_DOUBLE, root.type);
    try testing.expectApproxEqAbs(@as(f64, 0.3), root.data.double_value, 0.000001);

    const updated_intensity = jsonDouble(0.75);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_light_property(map, stringView("intensity"), &updated_intensity));

    var updated_snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_style_light_property(map, stringView("intensity"), &updated_snapshot));
    defer c.mln_json_snapshot_destroy(updated_snapshot.?);
    root = try snapshotRoot(updated_snapshot.?);
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_DOUBLE, root.type);
    try testing.expectApproxEqAbs(@as(f64, 0.75), root.data.double_value, 0.000001);

    var missing_snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_style_light_property(map, stringView("unknown-light-property"), &missing_snapshot));
    try testing.expectEqual(@as(?*c.mln_json_snapshot, null), missing_snapshot);

    const invalid_intensity = jsonBool(false);
    try testing.expectEqual(
        c.MLN_STATUS_INVALID_ARGUMENT,
        c.mln_map_set_style_light_property(map, stringView("intensity"), &invalid_intensity),
    );
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
}
