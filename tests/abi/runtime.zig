const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "runtime rejects invalid arguments" {
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_create(null, null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_destroy(null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
}
