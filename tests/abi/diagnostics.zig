const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "failing status sets and successful status clears diagnostics" {
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_destroy(null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    try testing.expectEqual(@as(usize, 0), std.mem.len(c.mln_thread_last_error_message()));
}

test "stale and invalid lifecycle errors set diagnostics" {
    const runtime = try support.createRuntime();
    const map = try support.createMap(runtime);

    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_destroy(runtime));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_destroy(map));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_destroy(runtime));
}
