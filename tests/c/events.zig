const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

extern fn usleep(useconds: c_uint) c_int;

test "event polling reports empty queues" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    _ = try support.drainEvents(map);

    var event = support.emptyEvent();
    var has_event = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_poll_event(map, &event, &has_event));
    try testing.expect(!has_event);
}

test "event polling rejects invalid outputs" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var has_event = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_poll_event(map, null, &has_event));

    var event = support.emptyEvent();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_poll_event(map, &event, null));

    event.size = @sizeOf(c.mln_map_event) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_poll_event(map, &event, &has_event));
}

test "event message storage is copied into caller output" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "unsupported://style.json"));

    var event = support.emptyEvent();
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        var has_event = false;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_poll_event(map, &event, &has_event));
        if (has_event and event.type == c.MLN_MAP_EVENT_MAP_LOADING_FAILED) break;
        _ = usleep(1000);
    } else return error.EventNotFound;

    try testing.expect(event.message != null);
    const message = event.message[0..event.message_size];
    try testing.expect(message.len > 0);
    const copied_message = try testing.allocator.dupe(u8, message);
    defer testing.allocator.free(copied_message);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
    try testing.expect(event.message != null);
    try testing.expectEqualSlices(u8, copied_message, event.message[0..event.message_size]);
}
