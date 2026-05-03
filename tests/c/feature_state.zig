const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

fn stringView(value: []const u8) c.mln_string_view {
    return .{ .data = value.ptr, .size = value.len };
}

fn featureStateSelector(source_id: []const u8, feature_id: []const u8) c.mln_feature_state_selector {
    return .{
        .size = @sizeOf(c.mln_feature_state_selector),
        .fields = c.MLN_FEATURE_STATE_SELECTOR_FEATURE_ID,
        .source_id = stringView(source_id),
        .source_layer_id = .{ .data = null, .size = 0 },
        .feature_id = stringView(feature_id),
        .state_key = .{ .data = null, .size = 0 },
    };
}

fn expectObjectMemberBool(root: *const c.mln_json_value, key: []const u8, expected: bool) !void {
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_OBJECT, root.type);
    const object = root.data.object_value;
    for (object.members[0..object.member_count]) |member| {
        if (std.mem.eql(u8, member.key.data[0..member.key.size], key)) {
            const value = member.value.?;
            try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_BOOL, value.*.type);
            try testing.expectEqual(expected, value.*.data.bool_value);
            return;
        }
    }
    return error.MissingJsonMember;
}

fn expectObjectMemberUint(root: *const c.mln_json_value, key: []const u8, expected: u64) !void {
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_OBJECT, root.type);
    const object = root.data.object_value;
    for (object.members[0..object.member_count]) |member| {
        if (std.mem.eql(u8, member.key.data[0..member.key.size], key)) {
            const value = member.value.?;
            try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_UINT, value.*.type);
            try testing.expectEqual(expected, value.*.data.uint_value);
            return;
        }
    }
    return error.MissingJsonMember;
}

test "feature state ABI structs import" {
    const selector = featureStateSelector("point", "feature-1");
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_feature_state_selector)), selector.size);
    try testing.expectEqual(@as(u32, c.MLN_FEATURE_STATE_SELECTOR_FEATURE_ID), selector.fields);
}

test "render session feature state set get and remove" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_owned_texture_descriptor_default();
    descriptor.width = 64;
    descriptor.height = 64;

    var session: ?*c.mln_render_session = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_owned_texture_attach(map, &descriptor, &session));
    defer testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_destroy(session.?)) catch @panic("session destroy failed");

    var selector = featureStateSelector("point", "feature-1");

    const hover_key = "hover";
    const radius_key = "radius";
    const hover = c.mln_json_value{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_BOOL,
        .data = .{ .bool_value = true },
    };
    const radius = c.mln_json_value{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_UINT,
        .data = .{ .uint_value = 20 },
    };
    const members = [_]c.mln_json_member{
        .{ .key = stringView(hover_key), .value = &hover },
        .{ .key = stringView(radius_key), .value = &radius },
    };
    const state = c.mln_json_value{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_OBJECT,
        .data = .{ .object_value = .{ .members = &members, .member_count = members.len } },
    };

    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_render_session_set_feature_state(session.?, &selector, &state));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_render_update(session.?));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_set_feature_state(session.?, &selector, &state));

    var snapshot: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_get_feature_state(session.?, &selector, &snapshot));
    defer c.mln_json_snapshot_destroy(snapshot.?);

    var root: ?*const c.mln_json_value = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_json_snapshot_get(snapshot.?, &root));
    try testing.expectEqual(@as(usize, 2), root.?.*.data.object_value.member_count);
    try expectObjectMemberBool(root.?, hover_key, true);
    try expectObjectMemberUint(root.?, radius_key, 20);

    selector.fields |= c.MLN_FEATURE_STATE_SELECTOR_STATE_KEY;
    selector.state_key = stringView(hover_key);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_remove_feature_state(session.?, &selector));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_render_update(session.?));

    var after_remove: ?*c.mln_json_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_get_feature_state(session.?, &selector, &after_remove));
    defer c.mln_json_snapshot_destroy(after_remove.?);

    root = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_json_snapshot_get(after_remove.?, &root));
    try testing.expectEqual(c.MLN_JSON_VALUE_TYPE_OBJECT, root.?.*.type);
    try testing.expectEqual(@as(usize, 1), root.?.*.data.object_value.member_count);
    try expectObjectMemberUint(root.?, radius_key, 20);
}

test "feature state selector validation" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_owned_texture_descriptor_default();
    descriptor.width = 64;
    descriptor.height = 64;

    var session: ?*c.mln_render_session = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_owned_texture_attach(map, &descriptor, &session));
    defer testing.expectEqual(c.MLN_STATUS_OK, c.mln_render_session_destroy(session.?)) catch @panic("session destroy failed");

    var selector = featureStateSelector("point", "feature-1");
    selector.fields = c.MLN_FEATURE_STATE_SELECTOR_STATE_KEY;
    selector.state_key = stringView("hover");
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_render_session_remove_feature_state(session.?, &selector));
}
