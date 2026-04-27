const testing = @import("std").testing;
const support = @import("support.zig");
const c = support.c;

test "camera jump updates snapshot fields" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var camera = support.testCamera();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_jump_to(map, &camera));

    var snapshot = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_camera(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);
}
