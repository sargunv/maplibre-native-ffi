const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const LogState = struct {
    count: usize = 0,
    saw_parse_style: bool = false,
    saw_message: bool = false,
};

fn recordLog(user_data: ?*anyopaque, severity: u32, event: u32, _: i64, message: [*c]const u8) callconv(.c) u32 {
    const state: *LogState = @ptrCast(@alignCast(user_data.?));
    state.count += 1;
    if (severity == c.MLN_LOG_SEVERITY_ERROR and event == c.MLN_LOG_EVENT_PARSE_STYLE) {
        state.saw_parse_style = true;
    }
    if (std.mem.len(message) > 0) {
        state.saw_message = true;
    }
    return 1;
}

test "log callback receives and consumes native logs" {
    var state = LogState{};
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(0));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_callback(recordLog, &state));
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_NATIVE_ERROR, c.mln_map_set_style_json(map, "{"));
    try testing.expect(state.count > 0);
    try testing.expect(state.saw_parse_style);
    try testing.expect(state.saw_message);
}

test "log async severity mask validates unknown bits" {
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(c.MLN_LOG_SEVERITY_MASK_DEFAULT));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(c.MLN_LOG_SEVERITY_MASK_ALL));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_log_set_async_severity_mask(1));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_log_set_async_severity_mask(c.MLN_LOG_SEVERITY_MASK_ALL << 1));
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(c.MLN_LOG_SEVERITY_MASK_DEFAULT));
}

test "log callback can be cleared" {
    var state = LogState{};
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_callback(recordLog, &state));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_clear_callback());
}
