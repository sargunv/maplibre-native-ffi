const std = @import("std");
const builtin = @import("builtin");
const testing = std.testing;
const metal_support = @import("metal_support.zig");
const support = @import("support.zig");
const c = support.c;

test "surface descriptors expose defaults" {
    const metal = c.mln_metal_surface_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_metal_surface_descriptor)), metal.size);
    try testing.expect(metal.width > 0);
    try testing.expect(metal.height > 0);
    try testing.expect(metal.scale_factor > 0);
    try testing.expect(metal.layer == null);
    try testing.expect(metal.device == null);

    const vulkan = c.mln_vulkan_surface_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_vulkan_surface_descriptor)), vulkan.size);
    try testing.expect(vulkan.width > 0);
    try testing.expect(vulkan.height > 0);
    try testing.expect(vulkan.scale_factor > 0);
    try testing.expect(vulkan.instance == null);
    try testing.expect(vulkan.physical_device == null);
    try testing.expect(vulkan.device == null);
    try testing.expect(vulkan.graphics_queue == null);
    try testing.expect(vulkan.surface == null);
}

test "Metal surface attach rejects invalid arguments" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_metal_surface_descriptor_default();
    var surface: ?*c.mln_surface_session = null;

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(null, &descriptor, &surface));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, null, &surface));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &descriptor, null));

    surface = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &descriptor, &surface));

    surface = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &descriptor, &surface));

    descriptor.layer = try metal_support.createLayer();
    var small_descriptor = descriptor;
    small_descriptor.size = @sizeOf(c.mln_metal_surface_descriptor) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &small_descriptor, &surface));

    var invalid_descriptor = descriptor;
    invalid_descriptor.width = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &invalid_descriptor, &surface));

    invalid_descriptor = descriptor;
    invalid_descriptor.height = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &invalid_descriptor, &surface));

    invalid_descriptor = descriptor;
    invalid_descriptor.scale_factor = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &invalid_descriptor, &surface));
}

test "Metal surface lifecycle and render update" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_metal_surface_descriptor_default();
    descriptor.width = 64;
    descriptor.height = 64;
    descriptor.layer = try metal_support.createLayer();

    var surface: ?*c.mln_surface_session = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_metal_surface_attach(map, &descriptor, &surface));

    var texture_descriptor = c.mln_owned_texture_descriptor_default();
    var texture: ?*c.mln_texture_session = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_owned_texture_attach(map, &texture_descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_surface_render_update(surface.?));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_surface_resize(surface.?, 32, 32, 2.0));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_surface_detach(surface.?));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_surface_render_update(surface.?));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_surface_destroy(surface.?));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_surface_destroy(surface.?));
}

test "Vulkan surface unsupported backend validates arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_vulkan_surface_descriptor_default();
    descriptor.instance = @ptrFromInt(1);
    descriptor.physical_device = @ptrFromInt(1);
    descriptor.device = @ptrFromInt(1);
    descriptor.graphics_queue = @ptrFromInt(1);
    descriptor.surface = @ptrFromInt(1);

    var surface: ?*c.mln_surface_session = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_surface_attach(map, &descriptor, &surface));
    try testing.expect(surface != null);

    surface = null;
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_vulkan_surface_attach(map, &descriptor, &surface));
    try testing.expectEqual(@as(?*c.mln_surface_session, null), surface);
}

test "Metal surface unsupported backend validates arguments" {
    if (builtin.os.tag == .macos) return error.SkipZigTest;

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_metal_surface_descriptor_default();
    descriptor.layer = @ptrFromInt(1);

    var surface: ?*c.mln_surface_session = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_surface_attach(map, &descriptor, &surface));
    try testing.expect(surface != null);

    surface = null;
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_metal_surface_attach(map, &descriptor, &surface));
    try testing.expectEqual(@as(?*c.mln_surface_session, null), surface);
}
