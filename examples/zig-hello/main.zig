const std = @import("std");

const c = @cImport({
    @cInclude("maplibre_native_abi.h");
});

pub fn main() !void {
    var message: [*c]const u8 = null;
    const status = c.mln_hello_world(&message);
    if (status != c.MLN_STATUS_OK or message == null) {
        return error.HelloExampleFailed;
    }

    std.debug.print("{d}: {s}\n", .{ c.mln_abi_version(), std.mem.span(message) });
}
