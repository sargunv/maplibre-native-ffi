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

    try testing.expect(try support.drainEvents(map) > 0);
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
        const status = c.mln_map_poll_event(map, &event, &has_event);
        try testing.expectEqual(c.MLN_STATUS_OK, status);
        if (!has_event) break;
        if (event.type == c.MLN_MAP_EVENT_MAP_LOADING_FAILED) {
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

test "style URL reports current null resource provider failure" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_NATIVE_ERROR, c.mln_map_set_style_url(map, "file:///tmp/missing-style.json"));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
}
