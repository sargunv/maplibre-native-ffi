const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

extern fn usleep(useconds: c_uint) c_int;

const ThreadCall = enum { resize, render_update, acquire, release, detach, destroy };

pub fn expectDescriptorDefaults() !void {
    const metal = c.mln_metal_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_metal_texture_descriptor)), metal.size);
    try testing.expect(metal.width > 0);
    try testing.expect(metal.height > 0);
    try testing.expect(metal.scale_factor > 0);
    try testing.expect(metal.device == null);

    const vulkan = c.mln_vulkan_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_vulkan_texture_descriptor)), vulkan.size);
    try testing.expect(vulkan.width > 0);
    try testing.expect(vulkan.height > 0);
    try testing.expect(vulkan.scale_factor > 0);
    try testing.expect(vulkan.instance == null);
    try testing.expect(vulkan.physical_device == null);
    try testing.expect(vulkan.device == null);
    try testing.expect(vulkan.graphics_queue == null);
}

pub fn expectAttachRejectsInvalidArguments(comptime Backend: type) !void {
    var context = try Backend.AttachContext.init();
    defer context.deinit();

    var texture: ?*c.mln_texture_session = null;
    var descriptor = context.descriptor();

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(null, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, null, &texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &descriptor, null));

    texture = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &descriptor, &texture));

    texture = null;
    var small_descriptor = context.descriptor();
    small_descriptor.size = Backend.descriptor_size - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &small_descriptor, &texture));

    var invalid_descriptor = context.descriptor();
    invalid_descriptor.width = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = context.descriptor();
    invalid_descriptor.height = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = context.descriptor();
    invalid_descriptor.scale_factor = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = context.descriptor();
    Backend.clearRequiredHandle(&invalid_descriptor);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, Backend.attach(map, &invalid_descriptor, &texture));

    var scaled_descriptor = context.descriptor();
    scaled_descriptor.scale_factor = 2.0;
    try testing.expectEqual(c.MLN_STATUS_OK, Backend.attach(map, &scaled_descriptor, &texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture.?));
}

pub fn expectLifecycleEnforcesActiveSessionAndStaleHandles(comptime Backend: type) !void {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var fixture = try Backend.Fixture.create(map);
    var second: ?*c.mln_texture_session = null;
    var descriptor = fixture.descriptor();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, Backend.attach(map, &descriptor, &second));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), second);

    fixture.destroy();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_destroy(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_render_update(fixture.texture));

    var replacement = try Backend.Fixture.create(map);
    replacement.destroy();
}

pub fn expectWrongThreadCallsRejected(comptime Backend: type) !void {
    const ThreadCallArgs = struct {
        fixture: *Backend.Fixture,
        frame: *Backend.Frame,
        call: ThreadCall,
        out_status: *c.mln_status,
    };
    const ThreadFns = struct {
        fn callTextureOnThread(args: ThreadCallArgs) void {
            args.out_status.* = switch (args.call) {
                .resize => c.mln_texture_resize(args.fixture.texture, 128, 128, 1.0),
                .render_update => c.mln_texture_render_update(args.fixture.texture),
                .acquire => args.fixture.acquire(args.frame),
                .release => args.fixture.release(args.frame),
                .detach => c.mln_texture_detach(args.fixture.texture),
                .destroy => c.mln_texture_destroy(args.fixture.texture),
            };
        }

        fn renderTextureOnThread(texture: *c.mln_texture_session, out_status: *c.mln_status) void {
            out_status.* = c.mln_texture_render_update(texture);
        }
    };

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.destroy();

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, ThreadFns.renderTextureOnThread, .{ fixture.texture, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);

    var frame = Backend.Frame.empty(fixture.texture);
    inline for (.{ ThreadCall.resize, ThreadCall.acquire, ThreadCall.detach, ThreadCall.destroy }) |call| {
        status = c.MLN_STATUS_OK;
        const call_thread = try std.Thread.spawn(.{}, ThreadFns.callTextureOnThread, .{ThreadCallArgs{ .fixture = &fixture, .frame = &frame, .call = call, .out_status = &status }});
        call_thread.join();
        try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
    }
}

pub fn expectRenderAcquireReleaseAndResizeGeneration(comptime Backend: type) !void {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.destroy();

    var frame = Backend.Frame.empty(fixture.texture);
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.acquire(&frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_RENDER_UPDATE_AVAILABLE);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render_update(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_OK, fixture.acquire(&frame));
    var frame_acquired = true;
    errdefer {
        if (frame_acquired) _ = fixture.release(&frame);
    }
    try Backend.expectInitialFrame(&frame);

    var stale_same_generation = frame;
    stale_same_generation.bumpFrameId();

    var second_frame = frame;
    second_frame.clearNativeHandles();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.acquire(&second_frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_resize(fixture.texture, 128, 128, 1.0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_detach(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_destroy(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, fixture.release(&stale_same_generation));

    try testing.expectEqual(c.MLN_STATUS_OK, fixture.release(&frame));
    frame_acquired = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.release(&frame));

    const old_frame = frame;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_resize(fixture.texture, 128, 64, 2.0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.release(&old_frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render_update(fixture.texture));
    frame.resetSize();
    try testing.expectEqual(c.MLN_STATUS_OK, fixture.acquire(&frame));
    frame_acquired = true;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, fixture.release(&old_frame));
    try Backend.expectResizedFrame(&frame);
    try testing.expectEqual(c.MLN_STATUS_OK, fixture.release(&frame));
    frame_acquired = false;
}

pub fn expectStillModeStillImageRequest(comptime Backend: type, map_mode: u32) !void {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMapWithMode(runtime, map_mode);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.destroy();

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_render_update(fixture.texture));

    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_map_request_repaint(map));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_request_still_image(map));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_map_request_still_image(map));

    var rendered_frame = false;
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));

        while (true) {
            var event = support.emptyEvent();
            var has_event = false;
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_poll_event(map, &event, &has_event));
            if (!has_event) break;

            switch (event.type) {
                c.MLN_MAP_EVENT_RENDER_UPDATE_AVAILABLE => {
                    const render_status = c.mln_texture_render_update(fixture.texture);
                    if (render_status == c.MLN_STATUS_OK) {
                        rendered_frame = true;
                    } else if (render_status != c.MLN_STATUS_INVALID_STATE) {
                        try testing.expectEqual(c.MLN_STATUS_OK, render_status);
                    }
                },
                c.MLN_MAP_EVENT_STILL_IMAGE_FINISHED => {
                    try testing.expect(rendered_frame);
                    var frame = Backend.Frame.empty(fixture.texture);
                    try testing.expectEqual(c.MLN_STATUS_OK, fixture.acquire(&frame));
                    try Backend.expectInitialFrame(&frame);
                    try testing.expectEqual(c.MLN_STATUS_OK, fixture.release(&frame));
                    return;
                },
                c.MLN_MAP_EVENT_STILL_IMAGE_FAILED => return error.StillImageFailed,
                else => {},
            }
        }

        _ = usleep(1000);
    }
    return error.EventNotFound;
}

pub fn expectDetachLeavesHandleLiveButUnusable(comptime Backend: type) !void {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.deinitContextOnly();

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_detach(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_render_update(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_detach(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(fixture.texture));
    fixture.markDestroyed();
}

test "texture descriptors expose defaults" {
    try expectDescriptorDefaults();
}
