const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "map loads inline style and emits events" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));

    for (0..8) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
    }

    try testing.expect(try support.drainEvents(runtime) > 0);
}

test "malformed style returns failure status and event" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_NATIVE_ERROR, c.mln_map_set_style_json(map, "{"));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    var saw_failed_event = false;
    while (true) {
        var event = support.emptyEvent();
        var has_event = false;
        const status = c.mln_runtime_poll_event(runtime, &event, &has_event);
        try testing.expectEqual(c.MLN_STATUS_OK, status);
        if (!has_event) break;
        if (event.type == c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED and
            event.source_type == c.MLN_RUNTIME_EVENT_SOURCE_MAP and
            event.source == @as(?*anyopaque, @ptrCast(map)))
        {
            saw_failed_event = true;
            break;
        }
    }
    try testing.expect(saw_failed_event);
}

test "style functions reject null inputs" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_style_json(map, null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_style_url(map, null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
}

test "unsupported style URL is accepted then emits failure event" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "unsupported://style.json"));
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED));
}
