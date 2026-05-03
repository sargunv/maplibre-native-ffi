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

test "core source helpers add and update ordinary sources" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    const point = c.mln_geometry{
        .size = @sizeOf(c.mln_geometry),
        .type = c.MLN_GEOMETRY_TYPE_POINT,
        .data = .{ .point = .{ .latitude = 37.7749, .longitude = -122.4194 } },
    };
    const geojson = c.mln_geojson{
        .size = @sizeOf(c.mln_geojson),
        .type = c.MLN_GEOJSON_TYPE_GEOMETRY,
        .data = .{ .geometry = &point },
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_add_geojson_source_data(map, stringView("geo-helper"), &geojson));

    var found = false;
    var source_type: u32 = c.MLN_STYLE_SOURCE_TYPE_UNKNOWN;
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_source_type(map, stringView("geo-helper"), &source_type, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(c.MLN_STYLE_SOURCE_TYPE_GEOJSON, source_type);

    const empty_collection = c.mln_geojson{
        .size = @sizeOf(c.mln_geojson),
        .type = c.MLN_GEOJSON_TYPE_FEATURE_COLLECTION,
        .data = .{ .feature_collection = .{ .features = null, .feature_count = 0 } },
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_geojson_source_data(map, stringView("geo-helper"), &empty_collection));
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_set_geojson_source_url(map, stringView("geo-helper"), stringView("https://example.com/data.geojson")),
    );

    var tile_options = c.mln_style_tile_source_options_default();
    try testing.expectEqual(@as(u32, 512), tile_options.tile_size);
    tile_options.fields = c.MLN_STYLE_TILE_SOURCE_OPTION_MIN_ZOOM |
        c.MLN_STYLE_TILE_SOURCE_OPTION_MAX_ZOOM |
        c.MLN_STYLE_TILE_SOURCE_OPTION_ATTRIBUTION |
        c.MLN_STYLE_TILE_SOURCE_OPTION_SCHEME |
        c.MLN_STYLE_TILE_SOURCE_OPTION_BOUNDS |
        c.MLN_STYLE_TILE_SOURCE_OPTION_VECTOR_ENCODING;
    tile_options.min_zoom = 1.0;
    tile_options.max_zoom = 14.0;
    tile_options.attribution = stringView("Helper attribution");
    tile_options.scheme = c.MLN_STYLE_TILE_SCHEME_TMS;
    tile_options.bounds = .{
        .southwest = .{ .latitude = -45.0, .longitude = -120.0 },
        .northeast = .{ .latitude = 45.0, .longitude = 120.0 },
    };
    tile_options.vector_encoding = c.MLN_STYLE_VECTOR_TILE_ENCODING_MLT;
    const vector_tiles = [_]c.mln_string_view{stringView("https://example.com/vector/{z}/{x}/{y}.mvt")};
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_add_vector_source_tiles(map, stringView("vector-helper"), &vector_tiles, vector_tiles.len, &tile_options),
    );

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
        c.mln_map_get_style_source_info(map, stringView("vector-helper"), &info, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(c.MLN_STYLE_SOURCE_TYPE_VECTOR, info.type);
    try testing.expect(info.has_attribution);
    try testing.expectEqual(@as(usize, "Helper attribution".len), info.attribution_size);

    var raster_options = c.mln_style_tile_source_options_default();
    raster_options.fields = c.MLN_STYLE_TILE_SOURCE_OPTION_TILE_SIZE;
    raster_options.tile_size = 256;
    const raster_tiles = [_]c.mln_string_view{stringView("https://example.com/raster/{z}/{x}/{y}.png")};
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_add_raster_source_tiles(map, stringView("raster-helper"), &raster_tiles, raster_tiles.len, &raster_options),
    );
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_get_style_source_type(map, stringView("raster-helper"), &source_type, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(c.MLN_STYLE_SOURCE_TYPE_RASTER, source_type);

    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_add_vector_source_url(map, stringView("vector-url-helper"), stringView("https://example.com/vector.json"), null),
    );
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_add_raster_source_url(map, stringView("raster-url-helper"), stringView("https://example.com/raster.json"), &raster_options),
    );

    try testing.expectEqual(
        c.MLN_STATUS_INVALID_ARGUMENT,
        c.mln_map_add_geojson_source_url(map, stringView("geo-helper"), stringView("https://example.com/again.geojson")),
    );
    try testing.expectEqual(
        c.MLN_STATUS_INVALID_ARGUMENT,
        c.mln_map_set_geojson_source_url(map, stringView("vector-helper"), stringView("https://example.com/not-geojson")),
    );
}

test "runtime style images copy premultiplied RGBA8 pixels" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED);

    var pixels = [_]u8{
        10,  20,  30,  255, 40, 50, 60, 255,
        0,   0,   0,   0,   70, 80, 90, 255,
        100, 110, 120, 255, 0,  0,  0,  0,
    };
    var image = c.mln_premultiplied_rgba8_image_default();
    try testing.expectEqual(@as(u32, 0), image.width);
    image.width = 2;
    image.height = 2;
    image.stride = 12;
    image.pixels = &pixels;
    image.byte_length = pixels.len;

    var options = c.mln_style_image_options_default();
    options.fields = c.MLN_STYLE_IMAGE_OPTION_PIXEL_RATIO | c.MLN_STYLE_IMAGE_OPTION_SDF;
    options.pixel_ratio = 2.0;
    options.sdf = true;

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_image(map, stringView("runtime-icon"), &image, &options));
    pixels[0] = 200;

    var exists = false;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_style_image_exists(map, stringView("runtime-icon"), &exists));
    try testing.expect(exists);

    var found = false;
    var info = c.mln_style_image_info_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_style_image_info(map, stringView("runtime-icon"), &info, &found));
    try testing.expect(found);
    try testing.expectEqual(@as(u32, 2), info.width);
    try testing.expectEqual(@as(u32, 2), info.height);
    try testing.expectEqual(@as(u32, 8), info.stride);
    try testing.expectEqual(@as(usize, 16), info.byte_length);
    try testing.expectApproxEqAbs(@as(f32, 2.0), info.pixel_ratio, 0.000001);
    try testing.expect(info.sdf);

    var required: usize = 0;
    try testing.expectEqual(
        c.MLN_STATUS_INVALID_ARGUMENT,
        c.mln_map_copy_style_image_premultiplied_rgba8(map, stringView("runtime-icon"), null, 0, &required, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(@as(usize, 16), required);

    var copied: [16]u8 = undefined;
    try testing.expectEqual(
        c.MLN_STATUS_OK,
        c.mln_map_copy_style_image_premultiplied_rgba8(map, stringView("runtime-icon"), &copied, copied.len, &required, &found),
    );
    try testing.expect(found);
    try testing.expectEqual(@as(usize, 16), required);
    try testing.expect(std.mem.eql(u8, copied[0..], &[_]u8{
        10, 20, 30, 255, 40,  50,  60,  255,
        70, 80, 90, 255, 100, 110, 120, 255,
    }));

    var replacement_pixels = [_]u8{ 1, 2, 3, 4 };
    image.width = 1;
    image.height = 1;
    image.stride = 4;
    image.pixels = &replacement_pixels;
    image.byte_length = replacement_pixels.len;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_image(map, stringView("runtime-icon"), &image, null));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_style_image_info(map, stringView("runtime-icon"), &info, &found));
    try testing.expect(found);
    try testing.expectEqual(@as(u32, 1), info.width);
    try testing.expectEqual(@as(u32, 1), info.height);
    try testing.expectEqual(@as(u32, 4), info.stride);
    try testing.expectApproxEqAbs(@as(f32, 1.0), info.pixel_ratio, 0.000001);
    try testing.expect(!info.sdf);

    var removed = false;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_remove_style_image(map, stringView("runtime-icon"), &removed));
    try testing.expect(removed);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_style_image_exists(map, stringView("runtime-icon"), &exists));
    try testing.expect(!exists);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_remove_style_image(map, stringView("runtime-icon"), &removed));
    try testing.expect(!removed);
}
