const testing = @import("std").testing;
const support = @import("support.zig");
const c = support.c;

test "map lifecycle rejects invalid state and stale handles" {
    const runtime = try support.createRuntime();

    const map = try support.createMap(runtime);
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_destroy(runtime));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_destroy(map));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_destroy(runtime));
}
