const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

extern fn usleep(useconds: c_uint) c_int;

test "runtime event polling reports empty queues" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    _ = try support.drainEvents(runtime);

    var event = support.emptyEvent();
    var has_event = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
    try testing.expect(!has_event);
}

test "runtime event polling rejects invalid outputs" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var has_event = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(runtime, null, &has_event));

    var event = support.emptyEvent();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(runtime, &event, null));

    event.size = @sizeOf(c.mln_runtime_event) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(runtime, &event, &has_event));
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
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
        if (has_event and
            event.type == c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED and
            event.source_type == c.MLN_RUNTIME_EVENT_SOURCE_MAP and
            event.source == @as(?*anyopaque, @ptrCast(map))) break;
        _ = usleep(1000);
    } else return error.EventNotFound;

    try testing.expectEqual(c.MLN_RUNTIME_EVENT_SOURCE_MAP, event.source_type);
    try testing.expect(event.source == @as(?*anyopaque, @ptrCast(map)));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_NONE, event.payload_type);
    try testing.expectEqual(@as(?*const anyopaque, null), event.payload);
    try testing.expectEqual(@as(usize, 0), event.payload_size);
    try testing.expect(event.message != null);
    const message = event.message[0..event.message_size];
    try testing.expect(message.len > 0);
    const copied_message = try testing.allocator.dupe(u8, message);
    defer testing.allocator.free(copied_message);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
    try testing.expect(event.message != null);
    try testing.expectEqualSlices(u8, copied_message, event.message[0..event.message_size]);
}

test "destroying a map discards its queued runtime events" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    var map_live = true;
    errdefer if (map_live) support.destroyMap(map);
    _ = try support.drainEvents(runtime);

    try testing.expectEqual(c.MLN_STATUS_NATIVE_ERROR, c.mln_map_set_style_json(map, "{"));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map));
    map_live = false;

    var event = support.emptyEvent();
    var has_event = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
    try testing.expect(!has_event);
}
