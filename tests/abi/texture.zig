const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const ThreadCall = enum { resize, render, acquire, release, detach, destroy };

const ThreadCallArgs = struct {
    texture: *c.mln_texture_session,
    frame: *c.mln_metal_texture_frame,
    call: ThreadCall,
    out_status: *c.mln_status,
};

fn callTextureOnThread(args: ThreadCallArgs) void {
    args.out_status.* = switch (args.call) {
        .resize => c.mln_texture_resize(args.texture, 128, 128, 1.0),
        .render => c.mln_texture_render(args.texture),
        .acquire => c.mln_texture_acquire_frame(args.texture, args.frame),
        .release => c.mln_texture_release_frame(args.texture, args.frame),
        .detach => c.mln_texture_detach(args.texture),
        .destroy => c.mln_texture_destroy(args.texture),
    };
}

fn renderTextureOnThread(texture: *c.mln_texture_session, out_status: *c.mln_status) void {
    out_status.* = c.mln_texture_render(texture);
}

fn createTexture(map: *c.mln_map) !*c.mln_texture_session {
    var texture: ?*c.mln_texture_session = null;
    var descriptor = c.mln_metal_texture_descriptor_default();
    descriptor.width = 256;
    descriptor.height = 256;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_attach(map, &descriptor, &texture));
    return texture orelse error.TextureCreateFailed;
}

fn destroyTexture(texture: *c.mln_texture_session) void {
    testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture)) catch @panic("texture destroy failed");
}

fn emptyFrame() c.mln_metal_texture_frame {
    return .{
        .size = @sizeOf(c.mln_metal_texture_frame),
        .generation = 0,
        .width = 0,
        .height = 0,
        .scale_factor = 0,
        .frame_id = 0,
        .texture = null,
        .device = null,
        .pixel_format = 0,
    };
}

test "texture descriptor exposes defaults" {
    const descriptor = c.mln_metal_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_metal_texture_descriptor)), descriptor.size);
    try testing.expect(descriptor.width > 0);
    try testing.expect(descriptor.height > 0);
    try testing.expect(descriptor.scale_factor > 0);
}

test "texture attach rejects invalid arguments" {
    var texture: ?*c.mln_texture_session = null;
    var descriptor = c.mln_metal_texture_descriptor_default();

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(null, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, null, &texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, &descriptor, null));

    texture = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, &descriptor, &texture));

    texture = null;
    var small_descriptor = c.mln_metal_texture_descriptor_default();
    small_descriptor.size = @sizeOf(c.mln_metal_texture_descriptor) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, &small_descriptor, &texture));

    var invalid_descriptor = c.mln_metal_texture_descriptor_default();
    invalid_descriptor.width = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = c.mln_metal_texture_descriptor_default();
    invalid_descriptor.height = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = c.mln_metal_texture_descriptor_default();
    invalid_descriptor.scale_factor = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_attach(map, &invalid_descriptor, &texture));

    var scaled_descriptor = c.mln_metal_texture_descriptor_default();
    scaled_descriptor.scale_factor = 2.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_attach(map, &scaled_descriptor, &texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture.?));
    texture = null;
}

test "texture lifecycle enforces active session and stale handles" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    const texture = try createTexture(map);
    var second: ?*c.mln_texture_session = null;
    var descriptor = c.mln_metal_texture_descriptor_default();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_attach(map, &descriptor, &second));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), second);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_destroy(texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_render(texture));

    const replacement = try createTexture(map);
    destroyTexture(replacement);
}

test "texture rejects wrong-thread calls" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    const texture = try createTexture(map);
    defer destroyTexture(texture);

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, renderTextureOnThread, .{ texture, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);

    var frame = emptyFrame();
    inline for (.{ ThreadCall.resize, ThreadCall.acquire, ThreadCall.detach, ThreadCall.destroy }) |call| {
        status = c.MLN_STATUS_OK;
        const call_thread = try std.Thread.spawn(.{}, callTextureOnThread, .{ThreadCallArgs{ .texture = texture, .frame = &frame, .call = call, .out_status = &status }});
        call_thread.join();
        try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
    }
}

test "texture render acquire release and resize generation" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    const texture = try createTexture(map);
    defer destroyTexture(texture);

    var frame = emptyFrame();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_acquire_frame(texture, &frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_RENDER_INVALIDATED);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render(texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_acquire_frame(texture, &frame));
    try testing.expect(frame.texture != null);
    try testing.expect(frame.device != null);
    try testing.expectEqual(@as(u32, 256), frame.width);
    try testing.expectEqual(@as(u32, 256), frame.height);
    try testing.expectEqual(@as(u64, 1), frame.generation);
    try testing.expect(frame.frame_id != 0);

    var stale_same_generation = frame;
    stale_same_generation.frame_id += 1;

    var second_frame = frame;
    second_frame.texture = null;
    second_frame.device = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_acquire_frame(texture, &second_frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_resize(texture, 128, 128, 1.0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_detach(texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_destroy(texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_release_frame(texture, &stale_same_generation));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_release_frame(texture, &frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_release_frame(texture, &frame));

    const old_frame = frame;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_resize(texture, 128, 64, 2.0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_release_frame(texture, &old_frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render(texture));
    frame.size = @sizeOf(c.mln_metal_texture_frame);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_acquire_frame(texture, &frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_release_frame(texture, &old_frame));
    try testing.expectEqual(@as(u32, 256), frame.width);
    try testing.expectEqual(@as(u32, 128), frame.height);
    try testing.expectEqual(@as(f64, 2.0), frame.scale_factor);
    try testing.expectEqual(@as(u64, 2), frame.generation);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_release_frame(texture, &frame));
}

test "texture detach leaves handle live but unusable for rendering" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    const texture = try createTexture(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_detach(texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_render(texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_detach(texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture));
}
