const testing = @import("std").testing;
const support = @import("support.zig");
const c = support.c;

test "camera exposes default options" {
    const camera = c.mln_camera_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_camera_options)), camera.size);
    try testing.expectEqual(@as(u32, 0), camera.fields);
}

test "camera rejects invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_camera(map, null));

    var snapshot = c.mln_camera_options_default();
    snapshot.size = @sizeOf(c.mln_camera_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_camera(map, &snapshot));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_jump_to(map, null));

    var camera = support.testCamera();
    camera.size = @sizeOf(c.mln_camera_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_jump_to(map, &camera));
}

test "camera jump updates snapshot fields" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var camera = support.testCamera();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_jump_to(map, &camera));

    var snapshot = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_camera(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);
}

test "camera commands accept valid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    const anchor = c.mln_screen_point{ .x = 256, .y = 256 };
    const rotate_start = c.mln_screen_point{ .x = 200, .y = 200 };
    const rotate_end = c.mln_screen_point{ .x = 220, .y = 210 };

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_move_by(map, 4, -2));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_scale_by(map, 1.1, &anchor));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_scale_by(map, 0.95, null));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_rotate_by(map, rotate_start, rotate_end));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pitch_by(map, 3));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_cancel_transitions(map));
}
