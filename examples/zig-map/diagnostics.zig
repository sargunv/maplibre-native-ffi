const std = @import("std");
const c = @import("c.zig").c;

pub fn logAbiError(message: []const u8) void {
    std.debug.print("{s}: {s}\n", .{ message, std.mem.span(c.mln_thread_last_error_message()) });
}

pub fn logCallback(
    _: ?*anyopaque,
    severity: u32,
    event: u32,
    code: i64,
    message: [*c]const u8,
) callconv(.c) u32 {
    std.debug.print(
        "maplibre log severity={d} event={d} code={d}: {s}\n",
        .{ severity, event, code, std.mem.span(message) },
    );
    return 1;
}
