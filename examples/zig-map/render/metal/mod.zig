const std = @import("std");
const objc = @import("objc");

const c = @import("../../c.zig").c;
const diagnostics = @import("../../diagnostics.zig");
const render_target = @import("../../render_target.zig");
const types = @import("../../types.zig");

extern "c" fn MTLCreateSystemDefaultDevice() objc.c.id;

const MTLPixelFormatBGRA8Unorm: u64 = 80;
const MTLPixelFormatRGBA8Unorm: u64 = 70;
const MTLLoadActionClear: u64 = 2;
const MTLStoreActionStore: u64 = 1;
const MTLPrimitiveTypeTriangle: u64 = 3;
const MTLTextureUsageShaderRead: u64 = 1;
const MTLTextureUsageRenderTarget: u64 = 4;

const CGSize = extern struct { width: f64, height: f64 };
const MTLClearColor = extern struct {
    red: f64,
    green: f64,
    blue: f64,
    alpha: f64,
};

pub const MetalBackend = union(enum) {
    pub const window_flags = c.SDL_WINDOW_METAL;

    owned_texture: MetalOwnedTextureBackend,
    borrowed_texture: MetalBorrowedTextureBackend,
    native_surface: MetalSurfaceBackend,

    pub fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        mode: types.RenderTargetMode,
    ) !MetalBackend {
        _ = allocator;
        return switch (mode) {
            .owned_texture => .{ .owned_texture = try MetalOwnedTextureBackend.init(window, viewport) },
            .borrowed_texture => .{ .borrowed_texture = try MetalBorrowedTextureBackend.init(window, viewport) },
            .native_surface => .{ .native_surface = try MetalSurfaceBackend.init(window, viewport) },
        };
    }

    pub fn deinit(self: *MetalBackend) void {
        switch (self.*) {
            .owned_texture => |*backend| backend.deinit(),
            .borrowed_texture => |*backend| backend.deinit(),
            .native_surface => |*backend| backend.deinit(),
        }
    }

    pub fn resize(self: *MetalBackend, viewport: types.Viewport) !void {
        switch (self.*) {
            .owned_texture => |*backend| try backend.resize(viewport),
            .borrowed_texture => |*backend| try backend.resize(viewport),
            .native_surface => |*backend| try backend.resize(viewport),
        }
    }

    pub fn needsRenderTargetReattachOnResize(self: *const MetalBackend) bool {
        return switch (self.*) {
            .owned_texture, .native_surface => false,
            .borrowed_texture => true,
        };
    }

    pub fn finishFrame(_: *MetalBackend) !void {}

    pub fn attachRenderTarget(
        self: *MetalBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        return switch (self.*) {
            .owned_texture => |*backend| backend.attachRenderTarget(map, viewport),
            .borrowed_texture => |*backend| backend.attachRenderTarget(map, viewport),
            .native_surface => |*backend| backend.attachRenderTarget(map, viewport),
        };
    }

    pub fn drawTexture(
        self: *MetalBackend,
        texture: *c.mln_render_session,
        viewport: types.Viewport,
    ) !bool {
        return switch (self.*) {
            .owned_texture => |*backend| backend.drawTexture(texture, viewport),
            .borrowed_texture => |*backend| backend.drawTexture(texture, viewport),
            .native_surface => unreachable,
        };
    }
};

const MetalView = struct {
    view: c.SDL_MetalView,
    device: objc.Object,
    layer: objc.Object,

    fn init(window: *c.SDL_Window, viewport: types.Viewport) !MetalView {
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

        return .{ .view = view, .device = device, .layer = layer };
    }

    fn deinit(self: *MetalView) void {
        self.device.release();
        c.SDL_Metal_DestroyView(self.view);
    }

    fn resize(self: *MetalView, viewport: types.Viewport) void {
        self.layer.setProperty("drawableSize", drawableSize(viewport));
    }
};

const MetalTextureCompositor = struct {
    view: MetalView,
    queue: objc.Object,
    pipeline: objc.Object,

    fn init(window: *c.SDL_Window, viewport: types.Viewport) !MetalTextureCompositor {
        var view = try MetalView.init(window, viewport);
        errdefer view.deinit();

        const queue = view.device.msgSend(objc.Object, "newCommandQueue", .{});
        if (queue.value == null) return types.AppError.BackendSetupFailed;
        errdefer queue.release();

        const pipeline = try createPipeline(view.device);
        errdefer pipeline.release();

        return .{ .view = view, .queue = queue, .pipeline = pipeline };
    }

    fn deinit(self: *MetalTextureCompositor) void {
        self.pipeline.release();
        self.queue.release();
        self.view.deinit();
    }

    fn resize(self: *MetalTextureCompositor, viewport: types.Viewport) void {
        self.view.resize(viewport);
    }

    fn drawMetalTexture(self: *MetalTextureCompositor, metal_texture: *anyopaque) !bool {
        const drawable = self.view.layer.msgSend(objc.Object, "nextDrawable", .{});
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
            objc.Object.fromId(metal_texture),
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
};

const MetalOwnedTextureBackend = struct {
    compositor: MetalTextureCompositor,

    fn init(window: *c.SDL_Window, viewport: types.Viewport) !MetalOwnedTextureBackend {
        return .{ .compositor = try MetalTextureCompositor.init(window, viewport) };
    }

    fn deinit(self: *MetalOwnedTextureBackend) void {
        self.compositor.deinit();
    }

    fn resize(self: *MetalOwnedTextureBackend, viewport: types.Viewport) !void {
        self.compositor.resize(viewport);
    }

    fn attachRenderTarget(
        self: *MetalOwnedTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_metal_owned_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.device = self.compositor.view.device.value.?;
        var texture: ?*c.mln_render_session = null;
        if (c.mln_metal_owned_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Metal texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return .{ .texture = texture.? };
    }

    fn drawTexture(
        self: *MetalOwnedTextureBackend,
        texture: *c.mln_render_session,
        _: types.Viewport,
    ) !bool {
        var frame: c.mln_metal_owned_texture_frame = .{
            .size = @sizeOf(c.mln_metal_owned_texture_frame),
            .generation = 0,
            .width = 0,
            .height = 0,
            .scale_factor = 0,
            .frame_id = 0,
            .texture = null,
            .device = null,
            .pixel_format = 0,
        };
        const acquire_status = c.mln_metal_owned_texture_acquire_frame(texture, &frame);
        if (acquire_status == c.MLN_STATUS_INVALID_STATE) return false;
        if (acquire_status != c.MLN_STATUS_OK) {
            diagnostics.logAbiError("Metal texture acquire failed");
            return types.AppError.BackendDrawFailed;
        }
        defer releaseMetalFrame(texture, &frame);

        return try self.compositor.drawMetalTexture(frame.texture.?);
    }
};

const MetalBorrowedTextureBackend = struct {
    compositor: MetalTextureCompositor,
    borrowed_texture: objc.Object,

    fn init(window: *c.SDL_Window, viewport: types.Viewport) !MetalBorrowedTextureBackend {
        var compositor = try MetalTextureCompositor.init(window, viewport);
        errdefer compositor.deinit();
        return .{
            .compositor = compositor,
            .borrowed_texture = try createBorrowedTexture(compositor.view.device, viewport),
        };
    }

    fn deinit(self: *MetalBorrowedTextureBackend) void {
        self.borrowed_texture.release();
        self.compositor.deinit();
    }

    fn resize(self: *MetalBorrowedTextureBackend, viewport: types.Viewport) !void {
        const new_texture = try createBorrowedTexture(self.compositor.view.device, viewport);
        self.borrowed_texture.release();
        self.borrowed_texture = new_texture;
        self.compositor.resize(viewport);
    }

    fn attachRenderTarget(
        self: *MetalBorrowedTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_metal_borrowed_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.texture = self.borrowed_texture.value.?;
        var texture: ?*c.mln_render_session = null;
        if (c.mln_metal_borrowed_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Metal borrowed texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return .{ .texture = texture.? };
    }

    fn drawTexture(
        self: *MetalBorrowedTextureBackend,
        texture: *c.mln_render_session,
        _: types.Viewport,
    ) !bool {
        _ = texture;
        return try self.compositor.drawMetalTexture(self.borrowed_texture.value.?);
    }
};

const MetalSurfaceBackend = struct {
    view: MetalView,

    fn init(window: *c.SDL_Window, viewport: types.Viewport) !MetalSurfaceBackend {
        return .{ .view = try MetalView.init(window, viewport) };
    }

    fn deinit(self: *MetalSurfaceBackend) void {
        self.view.deinit();
    }

    fn resize(_: *MetalSurfaceBackend, _: types.Viewport) !void {}

    fn attachRenderTarget(
        self: *MetalSurfaceBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_metal_surface_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.layer = self.view.layer.value.?;
        descriptor.device = self.view.device.value.?;
        var surface: ?*c.mln_render_session = null;
        if (c.mln_metal_surface_attach(map, &descriptor, &surface) !=
            c.MLN_STATUS_OK or surface == null)
        {
            diagnostics.logAbiError("Metal surface attach failed");
            return types.AppError.SurfaceAttachFailed;
        }
        return .{ .surface = surface.? };
    }
};

fn releaseMetalFrame(
    texture: *c.mln_render_session,
    frame: *const c.mln_metal_owned_texture_frame,
) void {
    if (c.mln_metal_owned_texture_release_frame(texture, frame) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("Metal texture release failed");
    }
}

fn createBorrowedTexture(device: objc.Object, viewport: types.Viewport) !objc.Object {
    const descriptor = objc.getClass("MTLTextureDescriptor").?
        .msgSend(objc.Object, "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", .{
        @as(u64, MTLPixelFormatRGBA8Unorm),
        @as(c_ulong, viewport.physical_width),
        @as(c_ulong, viewport.physical_height),
        false,
    });
    if (descriptor.value == null) return types.AppError.BackendSetupFailed;
    descriptor.setProperty(
        "usage",
        @as(u64, MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget),
    );
    const texture = device.msgSend(objc.Object, "newTextureWithDescriptor:", .{descriptor});
    if (texture.value == null) return types.AppError.BackendSetupFailed;
    return texture;
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

const metal_shader_source = @embedFile("shader.metal");
