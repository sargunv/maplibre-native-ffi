const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

fn destroyRuntimeOnThread(runtime: *c.mln_runtime, out_status: *c.mln_status) void {
    out_status.* = c.mln_runtime_destroy(runtime);
}

fn pollRuntimeOnThread(runtime: *c.mln_runtime, out_status: *c.mln_status) void {
    var event = support.emptyEvent();
    var has_event = false;
    out_status.* = c.mln_runtime_poll_event(runtime, &event, &has_event);
}

fn createRuntimeOnThread(out_status: *c.mln_status) void {
    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    out_status.* = c.mln_runtime_create(&options, &runtime);
    if (runtime) |handle| {
        if (out_status.* == c.MLN_STATUS_OK) {
            out_status.* = c.mln_runtime_destroy(handle);
        }
    }
}

test "runtime exposes C API version and default options" {
    const options = c.mln_runtime_options_default();
    try testing.expectEqual(@as(u32, 0), c.mln_c_version());
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

test "runtime rejects unknown flags" {
    var options = c.mln_runtime_options_default();
    options.flags = 1 << 31;

    var runtime: ?*c.mln_runtime = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_create(&options, &runtime));
    try testing.expectEqual(@as(?*c.mln_runtime, null), runtime);
}

test "runtime rejects stale handles" {
    const runtime = try support.createRuntime();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_destroy(runtime));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_destroy(runtime));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_run_once(runtime));
}

test "runtime run once rejects null runtime" {
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_run_once(null));
}

test "runtime event polling rejects null runtime" {
    var event = support.emptyEvent();
    var has_event = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(null, &event, &has_event));
}

test "runtime rejects wrong-thread destroy" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, destroyRuntimeOnThread, .{ runtime, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
}

test "runtime rejects wrong-thread event polling" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, pollRuntimeOnThread, .{ runtime, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
}

test "runtime owns pump before and after maps" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));

    const map = try support.createMap(runtime);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
}

test "runtime rejects second runtime on same owner thread" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var second: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_create(&options, &second));
    try testing.expectEqual(@as(?*c.mln_runtime, null), second);
}

test "runtime permits one runtime per distinct owner thread" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var status: c.mln_status = c.MLN_STATUS_INVALID_STATE;
    const thread = try std.Thread.spawn(.{}, createRuntimeOnThread, .{&status});
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_OK, status);
}
