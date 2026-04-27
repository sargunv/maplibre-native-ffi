const testing = @import("std").testing;
const support = @import("support.zig");
const c = support.c;

test "event polling reports empty queues" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    _ = try support.drainEvents(map);

    var event = support.emptyEvent();
    try testing.expectEqual(c.MLN_STATUS_ACCEPTED, c.mln_map_poll_event(map, &event));
}

test "event polling rejects invalid output buffers" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_poll_event(map, null));

    var event = support.emptyEvent();
    event.size = @sizeOf(c.mln_map_event) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_poll_event(map, &event));
}
