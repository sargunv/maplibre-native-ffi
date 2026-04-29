const std = @import("std");
const objc = @import("objc");

const c = @import("../../c.zig").c;
const diagnostics = @import("../../diagnostics.zig");
const types = @import("../../types.zig");

extern "c" fn MTLCreateSystemDefaultDevice() objc.c.id;

pub const MetalBackend = struct {
    pub const window_flags = c.SDL_WINDOW_METAL;
    const MTLPixelFormatBGRA8Unorm: u64 = 80;
    const MTLLoadActionClear: u64 = 2;
    const MTLStoreActionStore: u64 = 1;
    const MTLPrimitiveTypeTriangle: u64 = 3;

    const CGSize = extern struct { width: f64, height: f64 };
    const MTLClearColor = extern struct {
        red: f64,
        green: f64,
        blue: f64,
        alpha: f64,
    };

    view: c.SDL_MetalView,
    device: objc.Object,
    layer: objc.Object,
    queue: objc.Object,
    pipeline: objc.Object,

    pub fn init(
        _: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
    ) !MetalBackend {
        const view = c.SDL_Metal_CreateView(window);
        if (view == null) return types.AppError.BackendSetupFailed;
        errdefer c.SDL_Metal_DestroyView(view);

        const device_id = MTLCreateSystemDefaultDevice();
        if (device_id == null) return types.AppError.BackendSetupFailed;
        const device = objc.Object.fromId(device_id);
        errdefer device.release();

        const layer_ptr = c.SDL_Metal_GetLayer(view) orelse
            return types.AppError.BackendSetupFailed;
        const layer = objc.Object.fromId(layer_ptr);
        layer.setProperty("device", device);
        layer.setProperty("pixelFormat", @as(u64, MTLPixelFormatBGRA8Unorm));
        layer.setProperty("drawableSize", drawableSize(viewport));

        const queue = device.msgSend(objc.Object, "newCommandQueue", .{});
        if (queue.value == null) return types.AppError.BackendSetupFailed;
        errdefer queue.release();

        const pipeline = try createPipeline(device);
        errdefer pipeline.release();

        return .{
            .view = view,
            .device = device,
            .layer = layer,
            .queue = queue,
            .pipeline = pipeline,
        };
    }

    pub fn deinit(self: *MetalBackend) void {
        self.pipeline.release();
        self.queue.release();
        self.device.release();
        c.SDL_Metal_DestroyView(self.view);
    }

    pub fn resize(self: *MetalBackend, viewport: types.Viewport) !void {
        self.layer.setProperty("drawableSize", drawableSize(viewport));
    }

    pub fn finishFrame(_: *MetalBackend) !void {}

    pub fn attachTexture(
        self: *MetalBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !*c.mln_texture_session {
        var descriptor = c.mln_metal_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.device = self.device.value.?;
        var texture: ?*c.mln_texture_session = null;
        if (c.mln_metal_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Metal texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return texture.?;
    }

    pub fn draw(
        self: *MetalBackend,
        texture: *c.mln_texture_session,
        _: types.Viewport,
    ) !bool {
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
        const acquire_status = c.mln_metal_texture_acquire_frame(texture, &frame);
        if (acquire_status == c.MLN_STATUS_INVALID_STATE) return false;
        if (acquire_status != c.MLN_STATUS_OK) {
            diagnostics.logAbiError("Metal texture acquire failed");
            return types.AppError.BackendDrawFailed;
        }
        defer releaseMetalFrame(texture, &frame);

        const drawable = self.layer.msgSend(objc.Object, "nextDrawable", .{});
        if (drawable.value == null) return types.AppError.BackendDrawFailed;

        const drawable_texture = drawable.getProperty(objc.Object, "texture");
        const pass_descriptor = objc.getClass("MTLRenderPassDescriptor").?
            .msgSend(objc.Object, "renderPassDescriptor", .{});
        const color_attachments = pass_descriptor.getProperty(objc.Object, "colorAttachments");
        const attachment = color_attachments.msgSend(
            objc.Object,
            "objectAtIndexedSubscript:",
            .{@as(c_ulong, 0)},
        );
        attachment.setProperty("texture", drawable_texture);
        attachment.setProperty("loadAction", @as(u64, MTLLoadActionClear));
        attachment.setProperty("storeAction", @as(u64, MTLStoreActionStore));
        attachment.setProperty("clearColor", clearColor());

        const command_buffer = self.queue.msgSend(objc.Object, "commandBuffer", .{});
        if (command_buffer.value == null) return types.AppError.BackendDrawFailed;
        const encoder = command_buffer.msgSend(
            objc.Object,
            "renderCommandEncoderWithDescriptor:",
            .{pass_descriptor},
        );
        if (encoder.value == null) return types.AppError.BackendDrawFailed;

        encoder.msgSend(void, "setRenderPipelineState:", .{self.pipeline});
        encoder.msgSend(void, "setFragmentTexture:atIndex:", .{
            objc.Object.fromId(frame.texture.?),
            @as(c_ulong, 0),
        });
        encoder.msgSend(void, "drawPrimitives:vertexStart:vertexCount:", .{
            @as(u64, MTLPrimitiveTypeTriangle),
            @as(c_ulong, 0),
            @as(c_ulong, 6),
        });
        encoder.msgSend(void, "endEncoding", .{});
        command_buffer.msgSend(void, "presentDrawable:", .{drawable});
        command_buffer.msgSend(void, "commit", .{});
        command_buffer.msgSend(void, "waitUntilCompleted", .{});
        return true;
    }

    fn releaseMetalFrame(
        texture: *c.mln_texture_session,
        frame: *const c.mln_metal_texture_frame,
    ) void {
        if (c.mln_metal_texture_release_frame(texture, frame) != c.MLN_STATUS_OK) {
            diagnostics.logAbiError("Metal texture release failed");
        }
    }

    fn drawableSize(viewport: types.Viewport) CGSize {
        return .{
            .width = @floatFromInt(viewport.physical_width),
            .height = @floatFromInt(viewport.physical_height),
        };
    }

    fn clearColor() MTLClearColor {
        return .{ .red = 0.08, .green = 0.09, .blue = 0.11, .alpha = 1.0 };
    }

    fn createPipeline(device: objc.Object) !objc.Object {
        const NSString = objc.getClass("NSString").?;
        const source = NSString.msgSend(
            objc.Object,
            "stringWithUTF8String:",
            .{metal_shader_source.ptr},
        );
        if (source.value == null) return types.AppError.BackendSetupFailed;

        var error_object: objc.c.id = null;
        const library = device.msgSend(
            objc.Object,
            "newLibraryWithSource:options:error:",
            .{ source, @as(objc.c.id, null), &error_object },
        );
        if (library.value == null) return types.AppError.BackendSetupFailed;
        defer library.release();

        const vertex_name = NSString.msgSend(
            objc.Object,
            "stringWithUTF8String:",
            .{"vertex_main"},
        );
        const fragment_name = NSString.msgSend(
            objc.Object,
            "stringWithUTF8String:",
            .{"fragment_main"},
        );
        const vertex = library.msgSend(objc.Object, "newFunctionWithName:", .{vertex_name});
        if (vertex.value == null) return types.AppError.BackendSetupFailed;
        defer vertex.release();
        const fragment = library.msgSend(objc.Object, "newFunctionWithName:", .{fragment_name});
        if (fragment.value == null) return types.AppError.BackendSetupFailed;
        defer fragment.release();

        const descriptor = objc.getClass("MTLRenderPipelineDescriptor").?
            .msgSend(objc.Object, "alloc", .{})
            .msgSend(objc.Object, "init", .{});
        if (descriptor.value == null) return types.AppError.BackendSetupFailed;
        defer descriptor.release();
        descriptor.setProperty("vertexFunction", vertex);
        descriptor.setProperty("fragmentFunction", fragment);
        const attachments = descriptor.getProperty(objc.Object, "colorAttachments");
        const attachment = attachments.msgSend(
            objc.Object,
            "objectAtIndexedSubscript:",
            .{@as(c_ulong, 0)},
        );
        attachment.setProperty("pixelFormat", @as(u64, MTLPixelFormatBGRA8Unorm));

        var pipeline_error: objc.c.id = null;
        const pipeline = device.msgSend(
            objc.Object,
            "newRenderPipelineStateWithDescriptor:error:",
            .{ descriptor, &pipeline_error },
        );
        if (pipeline.value == null) return types.AppError.BackendSetupFailed;
        return pipeline;
    }
};

const metal_shader_source = @embedFile("shader.metal");
