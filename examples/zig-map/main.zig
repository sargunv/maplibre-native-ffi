const std = @import("std");
const objc = @import("objc");

const c = @cImport({
    @cInclude("maplibre_native_abi.h");
    @cInclude("SDL3/SDL.h");
    @cInclude("SDL3/SDL_metal.h");
});

const window_width = 960;
const window_height = 640;

const MTLPixelFormatBGRA8Unorm: u64 = 80;
const MTLLoadActionClear: u64 = 2;
const MTLStoreActionStore: u64 = 1;
const MTLPrimitiveTypeTriangle: u64 = 3;

const CGSize = extern struct {
    width: f64,
    height: f64,
};

const MTLClearColor = extern struct {
    red: f64,
    green: f64,
    blue: f64,
    alpha: f64,
};

const Viewport = struct {
    logical_width: u32,
    logical_height: u32,
    physical_width: u32,
    physical_height: u32,
    scale_factor: f64,
};

const AppError = error{
    SdlInitFailed,
    WindowCreateFailed,
    MetalViewCreateFailed,
    RuntimeCreateFailed,
    MapCreateFailed,
    TextureAttachFailed,
    StyleLoadFailed,
    CameraJumpFailed,
    TextureRenderFailed,
    MetalSetupFailed,
    MetalDrawFailed,
};

const MetalSampler = struct {
    layer: objc.Object,
    queue: objc.Object,
    pipeline: objc.Object,

    fn init(layer_ptr: *anyopaque, device_ptr: *anyopaque, drawable_size: CGSize) !MetalSampler {
        const layer = objc.Object.fromId(layer_ptr);
        const device = objc.Object.fromId(device_ptr);

        layer.setProperty("device", device);
        layer.setProperty("pixelFormat", @as(u64, MTLPixelFormatBGRA8Unorm));
        layer.setProperty("drawableSize", drawable_size);

        const queue = device.msgSend(objc.Object, "newCommandQueue", .{});
        if (queue.value == null) return AppError.MetalSetupFailed;
        errdefer queue.release();

        const pipeline = try createPipeline(device);
        errdefer pipeline.release();

        return .{
            .layer = layer,
            .queue = queue,
            .pipeline = pipeline,
        };
    }

    fn deinit(self: *MetalSampler) void {
        self.pipeline.release();
        self.queue.release();
    }

    fn resize(self: *MetalSampler, drawable_size: CGSize) void {
        self.layer.setProperty("drawableSize", drawable_size);
    }

    fn draw(self: *MetalSampler, texture_ptr: *anyopaque) !void {
        const drawable = self.layer.msgSend(objc.Object, "nextDrawable", .{});
        if (drawable.value == null) return AppError.MetalDrawFailed;

        const drawable_texture = drawable.getProperty(objc.Object, "texture");
        const pass_descriptor = objc.getClass("MTLRenderPassDescriptor").?.msgSend(objc.Object, "renderPassDescriptor", .{});
        const color_attachments = pass_descriptor.getProperty(objc.Object, "colorAttachments");
        const attachment = color_attachments.msgSend(objc.Object, "objectAtIndexedSubscript:", .{@as(c_ulong, 0)});
        attachment.setProperty("texture", drawable_texture);
        attachment.setProperty("loadAction", @as(u64, MTLLoadActionClear));
        attachment.setProperty("storeAction", @as(u64, MTLStoreActionStore));
        attachment.setProperty("clearColor", MTLClearColor{ .red = 0.08, .green = 0.09, .blue = 0.11, .alpha = 1.0 });

        const command_buffer = self.queue.msgSend(objc.Object, "commandBuffer", .{});
        if (command_buffer.value == null) return AppError.MetalDrawFailed;
        const encoder = command_buffer.msgSend(objc.Object, "renderCommandEncoderWithDescriptor:", .{pass_descriptor});
        if (encoder.value == null) return AppError.MetalDrawFailed;

        encoder.msgSend(void, "setRenderPipelineState:", .{self.pipeline});
        encoder.msgSend(void, "setFragmentTexture:atIndex:", .{ objc.Object.fromId(texture_ptr), @as(c_ulong, 0) });
        encoder.msgSend(void, "drawPrimitives:vertexStart:vertexCount:", .{ @as(u64, MTLPrimitiveTypeTriangle), @as(c_ulong, 0), @as(c_ulong, 6) });
        encoder.msgSend(void, "endEncoding", .{});
        command_buffer.msgSend(void, "presentDrawable:", .{drawable});
        command_buffer.msgSend(void, "commit", .{});
        command_buffer.msgSend(void, "waitUntilCompleted", .{});
    }

    fn createPipeline(device: objc.Object) !objc.Object {
        const NSString = objc.getClass("NSString").?;
        const source = NSString.msgSend(objc.Object, "stringWithUTF8String:", .{shader_source.ptr});
        if (source.value == null) return AppError.MetalSetupFailed;

        var error_object: objc.c.id = null;
        const library = device.msgSend(objc.Object, "newLibraryWithSource:options:error:", .{ source, @as(objc.c.id, null), &error_object });
        if (library.value == null) return AppError.MetalSetupFailed;
        defer library.release();

        const vertex_name = NSString.msgSend(objc.Object, "stringWithUTF8String:", .{"vertex_main"});
        const fragment_name = NSString.msgSend(objc.Object, "stringWithUTF8String:", .{"fragment_main"});
        const vertex = library.msgSend(objc.Object, "newFunctionWithName:", .{vertex_name});
        if (vertex.value == null) return AppError.MetalSetupFailed;
        defer vertex.release();
        const fragment = library.msgSend(objc.Object, "newFunctionWithName:", .{fragment_name});
        if (fragment.value == null) return AppError.MetalSetupFailed;
        defer fragment.release();

        const descriptor = objc.getClass("MTLRenderPipelineDescriptor").?.msgSend(objc.Object, "alloc", .{}).msgSend(objc.Object, "init", .{});
        if (descriptor.value == null) return AppError.MetalSetupFailed;
        defer descriptor.release();
        descriptor.setProperty("vertexFunction", vertex);
        descriptor.setProperty("fragmentFunction", fragment);
        const attachments = descriptor.getProperty(objc.Object, "colorAttachments");
        const attachment = attachments.msgSend(objc.Object, "objectAtIndexedSubscript:", .{@as(c_ulong, 0)});
        attachment.setProperty("pixelFormat", @as(u64, MTLPixelFormatBGRA8Unorm));

        var pipeline_error: objc.c.id = null;
        const pipeline = device.msgSend(objc.Object, "newRenderPipelineStateWithDescriptor:error:", .{ descriptor, &pipeline_error });
        if (pipeline.value == null) return AppError.MetalSetupFailed;
        return pipeline;
    }
};

const MapState = struct {
    runtime: *c.mln_runtime,
    map: *c.mln_map,
    texture: *c.mln_texture_session,

    fn init(viewport: Viewport) !MapState {
        var runtime: ?*c.mln_runtime = null;
        var runtime_options = c.mln_runtime_options_default();
        runtime_options.cache_path = ":memory:";
        if (c.mln_runtime_create(&runtime_options, &runtime) != c.MLN_STATUS_OK or runtime == null) {
            logAbiError("runtime create failed");
            return AppError.RuntimeCreateFailed;
        }
        errdefer _ = c.mln_runtime_destroy(runtime.?);

        var map: ?*c.mln_map = null;
        var map_options = c.mln_map_options_default();
        map_options.width = viewport.logical_width;
        map_options.height = viewport.logical_height;
        map_options.scale_factor = viewport.scale_factor;
        if (c.mln_map_create(runtime.?, &map_options, &map) != c.MLN_STATUS_OK or map == null) {
            logAbiError("map create failed");
            return AppError.MapCreateFailed;
        }
        errdefer _ = c.mln_map_destroy(map.?);

        try loadStyle(map.?);
        try setCamera(map.?);

        var descriptor = c.mln_metal_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        var texture: ?*c.mln_texture_session = null;
        if (c.mln_texture_attach(map.?, &descriptor, &texture) != c.MLN_STATUS_OK or texture == null) {
            logAbiError("texture attach failed");
            return AppError.TextureAttachFailed;
        }

        return .{ .runtime = runtime.?, .map = map.?, .texture = texture.? };
    }

    fn deinit(self: *MapState) void {
        _ = c.mln_texture_destroy(self.texture);
        _ = c.mln_map_destroy(self.map);
        _ = c.mln_runtime_destroy(self.runtime);
    }

    fn resize(self: *MapState, width: u32, height: u32, scale_factor: f64) void {
        _ = c.mln_texture_resize(self.texture, width, height, scale_factor);
    }
};

pub fn main(init_args: std.process.Init) !void {
    _ = init_args;

    _ = c.mln_log_set_callback(logCallback, null);
    defer _ = c.mln_log_clear_callback();

    if (!c.SDL_Init(c.SDL_INIT_VIDEO)) {
        std.debug.print("SDL_Init failed: {s}\n", .{std.mem.span(c.SDL_GetError())});
        return AppError.SdlInitFailed;
    }
    defer c.SDL_Quit();

    const window_flags = c.SDL_WINDOW_METAL | c.SDL_WINDOW_RESIZABLE | c.SDL_WINDOW_HIGH_PIXEL_DENSITY;
    const window = c.SDL_CreateWindow("MapLibre SDL3 Map", window_width, window_height, window_flags);
    if (window == null) {
        std.debug.print("SDL_CreateWindow failed: {s}\n", .{std.mem.span(c.SDL_GetError())});
        return AppError.WindowCreateFailed;
    }
    defer c.SDL_DestroyWindow(window);

    const metal_view = c.SDL_Metal_CreateView(window);
    if (metal_view == null) {
        std.debug.print("SDL_Metal_CreateView failed: {s}\n", .{std.mem.span(c.SDL_GetError())});
        return AppError.MetalViewCreateFailed;
    }
    defer c.SDL_Metal_DestroyView(metal_view);

    const window_handle = window.?;
    var viewport = getViewport(window_handle);
    logViewport("initial viewport", viewport);
    var map_state = try MapState.init(viewport);
    defer map_state.deinit();

    var sampler: ?MetalSampler = null;
    defer if (sampler) |*value| value.deinit();

    var running = true;
    var has_presented_frame = false;
    while (running) {
        var pool = objc.AutoreleasePool.init();
        defer pool.deinit();

        var event: c.SDL_Event = undefined;
        while (c.SDL_PollEvent(&event)) {
            switch (event.type) {
                c.SDL_EVENT_QUIT => running = false,
                c.SDL_EVENT_WINDOW_CLOSE_REQUESTED => running = false,
                c.SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED => {
                    viewport = getViewport(window_handle);
                    logViewport("resized viewport", viewport);
                    map_state.resize(viewport.logical_width, viewport.logical_height, viewport.scale_factor);
                    if (sampler) |*value| value.resize(.{ .width = @floatFromInt(viewport.physical_width), .height = @floatFromInt(viewport.physical_height) });
                },
                else => {},
            }
        }

        _ = c.mln_runtime_run_once(map_state.runtime);
        try drainMapEvents(map_state.map);
        const render_status = c.mln_texture_render(map_state.texture);
        if (render_status != c.MLN_STATUS_OK and render_status != c.MLN_STATUS_INVALID_STATE) {
            logAbiError("texture render failed");
            return AppError.TextureRenderFailed;
        }

        var frame: c.mln_metal_texture_frame = .{
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
        if (c.mln_texture_acquire_frame(map_state.texture, &frame) == c.MLN_STATUS_OK) {
            defer {
                if (c.mln_texture_release_frame(map_state.texture, &frame) != c.MLN_STATUS_OK) {
                    logAbiError("texture release failed");
                }
            }

            if (sampler == null) {
                const layer = c.SDL_Metal_GetLayer(metal_view) orelse return AppError.MetalViewCreateFailed;
                sampler = try MetalSampler.init(layer, frame.device.?, .{ .width = @floatFromInt(viewport.physical_width), .height = @floatFromInt(viewport.physical_height) });
            }
            try sampler.?.draw(frame.texture.?);
            has_presented_frame = true;
        } else if (!has_presented_frame) {
            _ = c.mln_runtime_run_once(map_state.runtime);
        }
        c.SDL_Delay(16);
    }
}

fn getViewport(window: *c.SDL_Window) Viewport {
    var logical_width: c_int = window_width;
    var logical_height: c_int = window_height;
    var physical_width: c_int = window_width;
    var physical_height: c_int = window_height;
    _ = c.SDL_GetWindowSize(window, &logical_width, &logical_height);
    _ = c.SDL_GetWindowSizeInPixels(window, &physical_width, &physical_height);

    const safe_logical_width = @max(logical_width, 1);
    const safe_logical_height = @max(logical_height, 1);
    const safe_physical_width = @max(physical_width, 1);
    const safe_physical_height = @max(physical_height, 1);
    const scale_x = @as(f64, @floatFromInt(safe_physical_width)) / @as(f64, @floatFromInt(safe_logical_width));
    const scale_y = @as(f64, @floatFromInt(safe_physical_height)) / @as(f64, @floatFromInt(safe_logical_height));

    return .{
        .logical_width = @intCast(safe_logical_width),
        .logical_height = @intCast(safe_logical_height),
        .physical_width = @intCast(safe_physical_width),
        .physical_height = @intCast(safe_physical_height),
        .scale_factor = @max(scale_x, scale_y),
    };
}

fn logViewport(label: []const u8, viewport: Viewport) void {
    std.debug.print("{s}: logical={d}x{d} physical={d}x{d} scale={d:.2}\n", .{
        label,
        viewport.logical_width,
        viewport.logical_height,
        viewport.physical_width,
        viewport.physical_height,
        viewport.scale_factor,
    });
}

fn loadStyle(map: *c.mln_map) !void {
    if (c.mln_map_set_style_url(map, "https://tiles.openfreemap.org/styles/bright") != c.MLN_STATUS_OK) {
        logAbiError("style load failed");
        return AppError.StyleLoadFailed;
    }
}

fn setCamera(map: *c.mln_map) !void {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER | c.MLN_CAMERA_OPTION_ZOOM | c.MLN_CAMERA_OPTION_BEARING | c.MLN_CAMERA_OPTION_PITCH;
    camera.latitude = 37.7749;
    camera.longitude = -122.4194;
    camera.zoom = 13.0;
    camera.bearing = 12.0;
    camera.pitch = 30.0;
    if (c.mln_map_jump_to(map, &camera) != c.MLN_STATUS_OK) {
        logAbiError("camera jump failed");
        return AppError.CameraJumpFailed;
    }
}

fn drainMapEvents(map: *c.mln_map) !void {
    while (true) {
        var event: c.mln_map_event = .{
            .size = @sizeOf(c.mln_map_event),
            .type = c.MLN_MAP_EVENT_NONE,
            .code = 0,
            .message = [_:0]u8{0} ** 512,
        };
        var has_event = false;
        if (c.mln_map_poll_event(map, &event, &has_event) != c.MLN_STATUS_OK) {
            logAbiError("event poll failed");
            return AppError.TextureRenderFailed;
        }
        if (!has_event) return;
    }
}

fn logAbiError(message: []const u8) void {
    std.debug.print("{s}: {s}\n", .{ message, std.mem.span(c.mln_thread_last_error_message()) });
}

fn logCallback(_: ?*anyopaque, severity: u32, event: u32, code: i64, message: [*c]const u8) callconv(.c) u32 {
    std.debug.print("maplibre log severity={d} event={d} code={d}: {s}\n", .{ severity, event, code, std.mem.span(message) });
    return 1;
}

const shader_source =
    \\#include <metal_stdlib>
    \\using namespace metal;
    \\
    \\struct VertexOut {
    \\  float4 position [[position]];
    \\  float2 uv;
    \\};
    \\
    \\vertex VertexOut vertex_main(uint vertex_id [[vertex_id]]) {
    \\  float2 positions[6] = {
    \\    float2(-1.0, -1.0), float2( 1.0, -1.0), float2(-1.0,  1.0),
    \\    float2( 1.0, -1.0), float2( 1.0,  1.0), float2(-1.0,  1.0),
    \\  };
    \\  float2 uvs[6] = {
    \\    float2(0.0, 1.0), float2(1.0, 1.0), float2(0.0, 0.0),
    \\    float2(1.0, 1.0), float2(1.0, 0.0), float2(0.0, 0.0),
    \\  };
    \\  VertexOut out;
    \\  out.position = float4(positions[vertex_id], 0.0, 1.0);
    \\  out.uv = uvs[vertex_id];
    \\  return out;
    \\}
    \\
    \\fragment float4 fragment_main(VertexOut in [[stage_in]], texture2d<float> map_texture [[texture(0)]]) {
    \\  constexpr sampler map_sampler(address::clamp_to_edge, filter::linear);
    \\  return map_texture.sample(map_sampler, in.uv);
    \\}
;
