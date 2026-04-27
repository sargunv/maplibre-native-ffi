const std = @import("std");

const c = @cImport({
    @cInclude("maplibre_native_abi.h");
});

pub fn main() !void {
    std.debug.print("ABI version: {d}\n", .{c.mln_abi_version()});

    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    if (c.mln_runtime_create(&options, &runtime) != c.MLN_STATUS_OK or runtime == null) {
        std.debug.print("runtime create failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.RuntimeCreateFailed;
    }

    if (c.mln_runtime_destroy(runtime) != c.MLN_STATUS_OK) {
        std.debug.print("runtime destroy failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.RuntimeDestroyFailed;
    }

    if (c.mln_runtime_destroy(null) != c.MLN_STATUS_INVALID_ARGUMENT) {
        return error.InvalidArgumentCheckFailed;
    }

    std.debug.print("runtime smoke passed\n", .{});
}
