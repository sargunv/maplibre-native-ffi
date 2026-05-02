const std = @import("std");

const c = @import("c.zig").c;

const width = 512;
const height = 512;
const style_url = "https://tiles.openfreemap.org/styles/bright";

pub fn main(init_args: std.process.Init) !void {
    const allocator = init_args.gpa;
    var args = try std.process.Args.Iterator.initAllocator(init_args.minimal.args, allocator);
    defer args.deinit();
    _ = args.skip();
    const output_path = args.next() orelse "map.ppm";

    _ = c.mln_log_set_async_severity_mask(0);
    defer _ = c.mln_log_set_async_severity_mask(c.MLN_LOG_SEVERITY_MASK_DEFAULT);

    var runtime: ?*c.mln_runtime = null;
    var runtime_options = c.mln_runtime_options_default();
    runtime_options.cache_path = ":memory:";
    try check(c.mln_runtime_create(&runtime_options, &runtime), "runtime create failed");
    defer _ = c.mln_runtime_destroy(runtime.?);

    var map: ?*c.mln_map = null;
    var map_options = c.mln_map_options_default();
    map_options.width = width;
    map_options.height = height;
    map_options.scale_factor = 1.0;
    map_options.map_mode = c.MLN_MAP_MODE_STATIC;
    try check(c.mln_map_create(runtime.?, &map_options, &map), "map create failed");
    defer _ = c.mln_map_destroy(map.?);

    var texture: ?*c.mln_render_session = null;
    var texture_descriptor = c.mln_owned_texture_descriptor_default();
    texture_descriptor.width = width;
    texture_descriptor.height = height;
    texture_descriptor.scale_factor = 1.0;
    try check(c.mln_owned_texture_attach(map.?, &texture_descriptor, &texture), "owned texture attach failed");
    defer _ = c.mln_render_session_destroy(texture.?);

    try setInitialCamera(map.?);
    try check(c.mln_map_set_style_url(map.?, style_url), "style load failed");
    try check(c.mln_map_request_still_image(map.?), "still image request failed");
    try renderTexture(runtime.?, map.?, texture.?);

    var info = c.mln_texture_image_info_default();
    const probe_status = c.mln_texture_read_premultiplied_rgba8(texture.?, null, 0, &info);
    if (probe_status != c.MLN_STATUS_INVALID_ARGUMENT) {
        try check(probe_status, "texture readback size query failed");
    }

    const rgba = try allocator.alloc(u8, info.byte_length);
    defer allocator.free(rgba);
    try check(c.mln_texture_read_premultiplied_rgba8(texture.?, rgba.ptr, rgba.len, &info), "texture readback failed");
    try writePpm(init_args.io, allocator, output_path, rgba, info);
    std.debug.print("wrote {s} ({d}x{d})\n", .{ output_path, info.width, info.height });
}

fn setInitialCamera(map: *c.mln_map) !void {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER |
        c.MLN_CAMERA_OPTION_ZOOM |
        c.MLN_CAMERA_OPTION_BEARING |
        c.MLN_CAMERA_OPTION_PITCH;
    camera.latitude = 37.7749;
    camera.longitude = -122.4194;
    camera.zoom = 13.0;
    camera.bearing = 12.0;
    camera.pitch = 30.0;
    try check(c.mln_map_jump_to(map, &camera), "camera jump failed");
}

fn renderTexture(runtime: *c.mln_runtime, map: *c.mln_map, texture: *c.mln_render_session) !void {
    var rendered_frame = false;
    while (true) {
        try check(c.mln_runtime_run_once(runtime), "runtime pump failed");

        while (true) {
            var event = emptyEvent();
            var has_event = false;
            try check(c.mln_runtime_poll_event(runtime, &event, &has_event), "event poll failed");
            if (!has_event) break;
            if (event.source_type != c.MLN_RUNTIME_EVENT_SOURCE_MAP or
                event.source != @as(?*anyopaque, @ptrCast(map))) continue;

            switch (event.type) {
                c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE => {
                    const status = c.mln_render_session_render_update(texture);
                    if (status == c.MLN_STATUS_OK) {
                        rendered_frame = true;
                    } else if (status != c.MLN_STATUS_INVALID_STATE) {
                        try check(status, "texture render failed");
                    }
                },
                c.MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FINISHED => {
                    if (!rendered_frame) return error.StillImageFinishedWithoutFrame;
                    return;
                },
                c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED => return error.MapLoadingFailed,
                c.MLN_RUNTIME_EVENT_MAP_RENDER_ERROR => return error.MapRenderFailed,
                c.MLN_RUNTIME_EVENT_MAP_STILL_IMAGE_FAILED => return error.StillImageFailed,
                else => {},
            }
        }
    }
}

fn emptyEvent() c.mln_runtime_event {
    return .{
        .size = @sizeOf(c.mln_runtime_event),
        .type = 0,
        .source_type = c.MLN_RUNTIME_EVENT_SOURCE_RUNTIME,
        .source = null,
        .code = 0,
        .payload_type = c.MLN_RUNTIME_EVENT_PAYLOAD_NONE,
        .payload = null,
        .payload_size = 0,
        .message = null,
        .message_size = 0,
    };
}

fn writePpm(
    io: std.Io,
    allocator: std.mem.Allocator,
    output_path: []const u8,
    rgba: []const u8,
    info: c.mln_texture_image_info,
) !void {
    const pixel_count = @as(usize, @intCast(info.width)) * @as(usize, @intCast(info.height));
    const rgb = try allocator.alloc(u8, pixel_count * 3);
    defer allocator.free(rgb);

    for (0..pixel_count) |index| {
        rgb[index * 3 + 0] = rgba[index * 4 + 0];
        rgb[index * 3 + 1] = rgba[index * 4 + 1];
        rgb[index * 3 + 2] = rgba[index * 4 + 2];
    }

    var file = try std.Io.Dir.cwd().createFile(io, output_path, .{});
    defer file.close(io);

    var header_buffer: [64]u8 = undefined;
    const header = try std.fmt.bufPrint(
        &header_buffer,
        "P6\n{d} {d}\n255\n",
        .{ info.width, info.height },
    );
    try file.writeStreamingAll(io, header);
    try file.writeStreamingAll(io, rgb);
}

fn check(status: c.mln_status, context: []const u8) !void {
    if (status == c.MLN_STATUS_OK) return;
    const diagnostic = std.mem.span(c.mln_thread_last_error_message());
    if (diagnostic.len == 0) {
        std.debug.print("{s}: status {d}\n", .{ context, status });
    } else {
        std.debug.print("{s}: {s}\n", .{ context, diagnostic });
    }
    return error.CApiFailed;
}
