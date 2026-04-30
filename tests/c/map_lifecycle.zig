const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

fn pollMapOnThread(map: *c.mln_map, out_status: *c.mln_status) void {
    var event = support.emptyEvent();
    var has_event = false;
    out_status.* = c.mln_map_poll_event(map, &event, &has_event);
}

fn requestRenderOnThread(map: *c.mln_map, out_status: *c.mln_status) void {
    out_status.* = c.mln_map_request_render(map);
}

test "map exposes default options" {
    const options = c.mln_map_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_map_options)), options.size);
    try testing.expect(options.width > 0);
    try testing.expect(options.height > 0);
    try testing.expect(options.scale_factor > 0);
    try testing.expectEqual(@as(u32, c.MLN_MAP_MODE_CONTINUOUS), options.map_mode);
}

test "map create rejects invalid arguments" {
    var map: ?*c.mln_map = null;
    var options = c.mln_map_options_default();

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(null, &options, &map));
    try testing.expectEqual(@as(?*c.mln_map, null), map);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &options, null));

    map = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &options, &map));

    map = null;
    var small_options = c.mln_map_options_default();
    small_options.size = @sizeOf(c.mln_map_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &small_options, &map));

    var invalid_options = c.mln_map_options_default();
    invalid_options.width = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &invalid_options, &map));

    invalid_options = c.mln_map_options_default();
    invalid_options.height = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &invalid_options, &map));

    invalid_options = c.mln_map_options_default();
    invalid_options.scale_factor = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &invalid_options, &map));

    invalid_options = c.mln_map_options_default();
    invalid_options.map_mode = 999;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_create(runtime, &invalid_options, &map));
}

test "map lifecycle rejects invalid state and stale handles" {
    const runtime = try support.createRuntime();

    const map = try support.createMap(runtime);
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_destroy(runtime));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_destroy(map));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_style_json(map, support.style_json));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_request_render(map));

    var camera = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_camera(map, &camera));

    var event = support.emptyEvent();
    var has_event = true;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_poll_event(map, &event, &has_event));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_destroy(runtime));
}

test "continuous render request invalidates map" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    _ = try support.drainEvents(map);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_request_render(map));
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_RENDER_INVALIDATED));
}

test "runtime supports multiple maps" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const first = try support.createMap(runtime);
    defer support.destroyMap(first);

    const second = try support.createMap(runtime);
    defer support.destroyMap(second);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
}

test "map rejects wrong-thread calls" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, pollMapOnThread, .{ map, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);

    const request_thread = try std.Thread.spawn(.{}, requestRenderOnThread, .{ map, &status });
    request_thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
}
