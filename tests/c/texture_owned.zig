const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "owned texture descriptor exposes defaults" {
    const descriptor = c.mln_owned_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_owned_texture_descriptor)), descriptor.size);
    try testing.expect(descriptor.width > 0);
    try testing.expect(descriptor.height > 0);
    try testing.expect(descriptor.scale_factor > 0);
}

test "owned texture attach rejects invalid arguments" {
    var texture: ?*c.mln_texture_session = null;
    var descriptor = c.mln_owned_texture_descriptor_default();

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(null, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, null, &texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, &descriptor, null));

    texture = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, &descriptor, &texture));

    texture = null;
    var small_descriptor = descriptor;
    small_descriptor.size = @sizeOf(c.mln_owned_texture_descriptor) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, &small_descriptor, &texture));

    var invalid_descriptor = descriptor;
    invalid_descriptor.width = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = descriptor;
    invalid_descriptor.height = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = descriptor;
    invalid_descriptor.scale_factor = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_owned_texture_attach(map, &invalid_descriptor, &texture));

    descriptor.width = 64;
    descriptor.height = 64;
    descriptor.scale_factor = 2.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_owned_texture_attach(map, &descriptor, &texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture.?));
}

test "owned texture lifecycle and render update" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_owned_texture_descriptor_default();
    descriptor.width = 128;
    descriptor.height = 128;

    var texture: ?*c.mln_texture_session = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_owned_texture_attach(map, &descriptor, &texture));

    var second: ?*c.mln_texture_session = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_owned_texture_attach(map, &descriptor, &second));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), second);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render_update(texture.?));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_resize(texture.?, 64, 64, 1.0));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_detach(texture.?));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_render_update(texture.?));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture.?));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_destroy(texture.?));
}
