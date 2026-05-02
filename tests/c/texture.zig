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

    const shared = c.mln_shared_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_shared_texture_descriptor)), shared.size);
    try testing.expect(shared.width > 0);
    try testing.expect(shared.height > 0);
    try testing.expect(shared.scale_factor > 0);
    try testing.expectEqual(@as(u32, c.MLN_SHARED_TEXTURE_EXPORT_NONE), shared.required_export_type);
    try testing.expect(shared.device == null);
    try testing.expect(shared.instance == null);
    try testing.expect(shared.physical_device == null);
    try testing.expect(shared.graphics_queue == null);

    const image_info = c.mln_texture_image_info_default();
    try testing.expectEqual(@as(@TypeOf(image_info.size), @sizeOf(c.mln_texture_image_info)), image_info.size);
    try testing.expectEqual(@as(u32, 0), image_info.width);
    try testing.expectEqual(@as(u32, 0), image_info.height);
    try testing.expectEqual(@as(u32, 0), image_info.stride);
    try testing.expectEqual(@as(usize, 0), image_info.byte_length);
}

pub fn emptySharedFrame() c.mln_shared_texture_frame {
    return .{
        .size = @sizeOf(c.mln_shared_texture_frame),
        .generation = 0,
        .width = 0,
        .height = 0,
        .scale_factor = 0,
        .frame_id = 0,
        .producer_backend = c.MLN_TEXTURE_BACKEND_NONE,
        .native_handle = null,
        .native_view = null,
        .native_device = null,
        .export_type = c.MLN_SHARED_TEXTURE_EXPORT_NONE,
        .export_handle = null,
        .export_fd = -1,
        .dma_buf_drm_format = 0,
        .dma_buf_drm_modifier = 0,
        .dma_buf_plane_offset = 0,
        .dma_buf_plane_stride = 0,
        .format = 0,
        .layout = 0,
        .plane = 0,
    };
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
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);

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

pub fn expectSharedFrameMetadata(comptime Backend: type) !void {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.destroy();

    var frame = emptySharedFrame();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_acquire_shared_frame(fixture.texture, &frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render_update(fixture.texture));
    frame = emptySharedFrame();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_acquire_shared_frame(fixture.texture, &frame));
    var frame_acquired = true;
    errdefer {
        if (frame_acquired) _ = c.mln_texture_release_shared_frame(fixture.texture, &frame);
    }
    try Backend.expectSharedFrame(&frame);

    var second_frame = emptySharedFrame();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_acquire_shared_frame(fixture.texture, &second_frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_release_shared_frame(fixture.texture, &frame));
    frame_acquired = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_release_shared_frame(fixture.texture, &frame));
}

pub fn expectNativeTextureRejectsSharedAcquireAndReadback(comptime Backend: type) !void {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.destroy();

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render_update(fixture.texture));

    var shared_frame = emptySharedFrame();
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_texture_acquire_shared_frame(fixture.texture, &shared_frame));

    var image_info = c.mln_texture_image_info_default();
    var pixel: [4]u8 = .{ 0, 0, 0, 0 };
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_texture_read_premultiplied_rgba8(fixture.texture, pixel[0..].ptr, pixel.len, &image_info));
}

pub fn expectRenderObserverEvents(comptime Backend: type) !void {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try Backend.Fixture.create(map);
    defer fixture.destroy();

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));

    var rendered_update = false;
    var pending_render_update = false;
    var saw_frame_started = false;
    var saw_frame_finished = false;
    var saw_map_started = false;
    var saw_map_finished = false;
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));

        if (pending_render_update) {
            pending_render_update = false;
            const render_status = c.mln_texture_render_update(fixture.texture);
            if (render_status == c.MLN_STATUS_OK) {
                rendered_update = true;
            } else if (render_status != c.MLN_STATUS_INVALID_STATE) {
                try testing.expectEqual(c.MLN_STATUS_OK, render_status);
            }
        }

        while (true) {
            var event = support.emptyEvent();
            var has_event = false;
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
            if (!has_event) break;
            if (event.source_type != c.MLN_RUNTIME_EVENT_SOURCE_MAP or event.source != @as(?*anyopaque, @ptrCast(map))) continue;

            switch (event.type) {
                c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE => {
                    pending_render_update = true;
                },
                c.MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_STARTED => {
                    saw_frame_started = true;
                    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_NONE, event.payload_type);
                    try testing.expectEqual(@as(?*const anyopaque, null), event.payload);
                },
                c.MLN_RUNTIME_EVENT_MAP_RENDER_FRAME_FINISHED => {
                    saw_frame_finished = true;
                    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_RENDER_FRAME, event.payload_type);
                    try testing.expectEqual(@as(usize, @sizeOf(c.mln_runtime_event_render_frame)), event.payload_size);
                    const payload: *const c.mln_runtime_event_render_frame = @ptrCast(@alignCast(event.payload.?));
                    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_event_render_frame)), payload.size);
                    try testing.expect(payload.mode == c.MLN_RENDER_MODE_PARTIAL or payload.mode == c.MLN_RENDER_MODE_FULL);
                    try testing.expectEqual(@as(u32, @sizeOf(c.mln_rendering_stats)), payload.stats.size);
                    try testing.expect(payload.stats.encoding_time >= 0);
                    try testing.expect(payload.stats.rendering_time >= 0);
                },
                c.MLN_RUNTIME_EVENT_MAP_RENDER_MAP_STARTED => {
                    saw_map_started = true;
                    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_NONE, event.payload_type);
                    try testing.expectEqual(@as(?*const anyopaque, null), event.payload);
                },
                c.MLN_RUNTIME_EVENT_MAP_RENDER_MAP_FINISHED => {
                    saw_map_finished = true;
                    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_RENDER_MAP, event.payload_type);
                    try testing.expectEqual(@as(usize, @sizeOf(c.mln_runtime_event_render_map)), event.payload_size);
                    const payload: *const c.mln_runtime_event_render_map = @ptrCast(@alignCast(event.payload.?));
                    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_event_render_map)), payload.size);
                    try testing.expect(payload.mode == c.MLN_RENDER_MODE_PARTIAL or payload.mode == c.MLN_RENDER_MODE_FULL);
                },
                else => {},
            }
        }

        if (saw_frame_started and saw_frame_finished and saw_map_started and saw_map_finished) break;
        _ = usleep(1000);
    }

    try testing.expect(rendered_update);
    try testing.expect(saw_frame_started);
    try testing.expect(saw_frame_finished);
    try testing.expect(saw_map_started);
    try testing.expect(saw_map_finished);
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
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
            if (!has_event) break;
            if (event.source_type != c.MLN_RUNTIME_EVENT_SOURCE_MAP or event.source != @as(?*anyopaque, @ptrCast(map))) continue;

            switch (event.type) {
                c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE => {
                    const render_status = c.mln_texture_render_update(fixture.texture);
                    if (render_status == c.MLN_STATUS_OK) {
                        rendered_frame = true;
                    } else if (render_status != c.MLN_STATUS_INVALID_STATE) {
                        try testing.expectEqual(c.MLN_STATUS_OK, render_status);
                    }
                },
                c.MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED => {
                    try testing.expect(rendered_frame);
                    var frame = Backend.Frame.empty(fixture.texture);
                    try testing.expectEqual(c.MLN_STATUS_OK, fixture.acquire(&frame));
                    try Backend.expectInitialFrame(&frame);
                    try testing.expectEqual(c.MLN_STATUS_OK, fixture.release(&frame));
                    return;
                },
                c.MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED => return error.StillImageFailed,
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
