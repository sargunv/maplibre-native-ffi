const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

fn destroyRuntimeOnThread(runtime: *c.mln_runtime, out_status: *c.mln_status) void {
    out_status.* = c.mln_runtime_destroy(runtime);
}

test "runtime exposes ABI version and default options" {
    const options = c.mln_runtime_options_default();
    try testing.expectEqual(@as(u32, 0), c.mln_abi_version());
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_options)), options.size);
}

test "runtime rejects invalid arguments" {
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_create(null, null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);

    var small_options = c.mln_runtime_options_default();
    small_options.size = @sizeOf(c.mln_runtime_options) - 1;
    var runtime: ?*c.mln_runtime = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_create(&small_options, &runtime));
    try testing.expectEqual(@as(?*c.mln_runtime, null), runtime);

    runtime = @ptrFromInt(1);
    var options = c.mln_runtime_options_default();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_create(&options, &runtime));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_destroy(null));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
}

test "runtime rejects stale handles" {
    const runtime = try support.createRuntime();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_destroy(runtime));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_destroy(runtime));
}

test "runtime rejects wrong-thread destroy" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, destroyRuntimeOnThread, .{ runtime, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
}
