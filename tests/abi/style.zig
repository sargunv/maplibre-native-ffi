const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "map loads inline style and emits events" {
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
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_NATIVE_ERROR, c.mln_map_set_style_json(map, "{"));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    var saw_failed_event = false;
    while (true) {
        var event = support.emptyEvent();
        const status = c.mln_map_poll_event(map, &event);
        if (status == c.MLN_STATUS_ACCEPTED) break;
        try testing.expectEqual(c.MLN_STATUS_OK, status);
        if (event.type == c.MLN_MAP_EVENT_MAP_LOADING_FAILED) {
            saw_failed_event = true;
            break;
        }
    }
    try testing.expect(saw_failed_event);
}
