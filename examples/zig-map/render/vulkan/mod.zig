const std = @import("std");

const c = @import("../../c.zig").c;
const diagnostics = @import("../../diagnostics.zig");
const render_target = @import("../../render_target.zig");
const types = @import("../../types.zig");
const Commands = @import("commands.zig").Commands;
const Context = @import("context.zig").Context;
const Pipeline = @import("pipeline.zig").Pipeline;
const Swapchain = @import("swapchain.zig").Swapchain;
const util = @import("util.zig");

extern fn close(fd: c_int) c_int;

pub const VulkanBackend = union(enum) {
    pub const window_flags = c.SDL_WINDOW_VULKAN;

    native_texture: VulkanNativeTextureBackend,
    shared_texture: VulkanSharedTextureBackend,
    native_surface: VulkanSurfaceBackend,

    pub fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        mode: types.RenderTargetMode,
    ) !VulkanBackend {
        return switch (mode) {
            .native_texture => .{ .native_texture = try VulkanNativeTextureBackend.init(allocator, window, viewport) },
            .shared_texture => .{ .shared_texture = try VulkanSharedTextureBackend.init(allocator, window, viewport) },
            .native_surface => .{ .native_surface = try VulkanSurfaceBackend.init(allocator, window) },
        };
    }

    pub fn deinit(self: *VulkanBackend) void {
        switch (self.*) {
            .native_texture => |*backend| backend.deinit(),
            .shared_texture => |*backend| backend.deinit(),
            .native_surface => |*backend| backend.deinit(),
        }
    }

    pub fn resize(self: *VulkanBackend, viewport: types.Viewport) !void {
        switch (self.*) {
            .native_texture => |*backend| try backend.resize(viewport),
            .shared_texture => |*backend| try backend.resize(viewport),
            .native_surface => |*backend| try backend.resize(viewport),
        }
    }

    pub fn finishFrame(self: *VulkanBackend) !void {
        switch (self.*) {
            .native_texture => |*backend| try backend.finishFrame(),
            .shared_texture => |*backend| try backend.finishFrame(),
            .native_surface => |*backend| try backend.finishFrame(),
        }
    }

    pub fn attachRenderTarget(
        self: *VulkanBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        return switch (self.*) {
            .native_texture => |*backend| backend.attachRenderTarget(map, viewport),
            .shared_texture => |*backend| backend.attachRenderTarget(map, viewport),
            .native_surface => |*backend| backend.attachRenderTarget(map, viewport),
        };
    }

    pub fn drawTexture(
        self: *VulkanBackend,
        texture: *c.mln_texture_session,
        viewport: types.Viewport,
    ) !bool {
        return switch (self.*) {
            .native_texture => |*backend| backend.drawTexture(texture, viewport),
            .shared_texture => |*backend| backend.drawTexture(texture, viewport),
            .native_surface => unreachable,
        };
    }
};

const VulkanTextureCompositor = struct {
    context: Context,
    swapchain: Swapchain,
    pipeline: Pipeline,
    commands: Commands,

    fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        exportable_textures: bool,
    ) !VulkanTextureCompositor {
        var context = try Context.init(allocator, window, exportable_textures);
        errdefer context.deinit();

        var swapchain = try Swapchain.init(allocator, &context, viewport);
        errdefer swapchain.deinit(context.device);

        var pipeline = try Pipeline.init(allocator, context.device, swapchain.format);
        errdefer pipeline.deinit(context.device);
        try swapchain.createFramebuffers(context.device, pipeline.render_pass);

        var commands = try Commands.init(context.device, context.queue_family_index);
        errdefer commands.deinit(context.device);

        return .{
            .context = context,
            .swapchain = swapchain,
            .pipeline = pipeline,
            .commands = commands,
        };
    }

    fn deinit(self: *VulkanTextureCompositor) void {
        self.context.waitIdle();
        self.commands.deinit(self.context.device);
        self.swapchain.deinit(self.context.device);
        self.pipeline.deinit(self.context.device);
        self.context.deinit();
    }

    fn waitIdle(self: *VulkanTextureCompositor) void {
        self.context.waitIdle();
    }

    fn resize(self: *VulkanTextureCompositor, viewport: types.Viewport) !void {
        const previous_format = self.swapchain.format;
        self.swapchain.deinit(self.context.device);
        self.swapchain = try Swapchain.init(
            self.swapchain.allocator,
            &self.context,
            viewport,
        );

        if (self.swapchain.format != previous_format) {
            self.pipeline.deinit(self.context.device);
            self.pipeline = try Pipeline.init(
                self.pipeline.allocator,
                self.context.device,
                self.swapchain.format,
            );
        }
        try self.swapchain.createFramebuffers(
            self.context.device,
            self.pipeline.render_pass,
        );
    }

    fn waitForFrame(self: *VulkanTextureCompositor) !void {
        try self.commands.waitForFrameFence(self.context.device);
    }

    fn presentImageView(self: *VulkanTextureCompositor, image_view: c.VkImageView) !bool {
        if (image_view != self.pipeline.descriptor_image_view) {
            self.pipeline.updateDescriptor(self.context.device, image_view);
        }

        try self.waitForFrame();

        var image_index: u32 = 0;
        const acquire = c.vkAcquireNextImageKHR(
            self.context.device,
            self.swapchain.handle,
            std.math.maxInt(u64),
            self.commands.image_available,
            null,
            &image_index,
        );
        if (acquire == c.VK_ERROR_OUT_OF_DATE_KHR) return false;
        try util.expectVkOrSuboptimal(acquire);
        try self.commands.resetFence(self.context.device);

        try self.commands.record(
            self.context.device,
            &self.swapchain,
            &self.pipeline,
            image_index,
        );
        try self.commands.submit(self.context.queue);

        const present_info = c.VkPresentInfoKHR{
            .sType = c.VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = null,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &self.commands.render_finished,
            .swapchainCount = 1,
            .pSwapchains = &self.swapchain.handle,
            .pImageIndices = &image_index,
            .pResults = null,
        };
        const present = c.vkQueuePresentKHR(self.context.queue, &present_info);
        if (present != c.VK_SUCCESS and
            present != c.VK_SUBOPTIMAL_KHR and
            present != c.VK_ERROR_OUT_OF_DATE_KHR)
        {
            self.waitForFrame() catch {};
            try util.expectVk(present);
        }
        return true;
    }
};

const VulkanNativeTextureBackend = struct {
    compositor: VulkanTextureCompositor,
    pending_texture: ?*c.mln_texture_session,
    pending_frame: ?c.mln_vulkan_texture_frame,

    fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
    ) !VulkanNativeTextureBackend {
        return .{
            .compositor = try VulkanTextureCompositor.init(allocator, window, viewport, false),
            .pending_texture = null,
            .pending_frame = null,
        };
    }

    fn deinit(self: *VulkanNativeTextureBackend) void {
        self.compositor.waitIdle();
        self.releasePendingFrame();
        self.compositor.deinit();
    }

    fn resize(self: *VulkanNativeTextureBackend, viewport: types.Viewport) !void {
        self.compositor.waitIdle();
        self.releasePendingFrame();
        try self.compositor.resize(viewport);
    }

    fn finishFrame(self: *VulkanNativeTextureBackend) !void {
        if (self.pending_frame == null) return;
        try self.compositor.waitForFrame();
        self.releasePendingFrame();
    }

    fn attachRenderTarget(
        self: *VulkanNativeTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_vulkan_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.instance = self.compositor.context.instance;
        descriptor.physical_device = self.compositor.context.physical_device;
        descriptor.device = self.compositor.context.device;
        descriptor.graphics_queue = self.compositor.context.queue;
        descriptor.graphics_queue_family_index = self.compositor.context.queue_family_index;

        var texture: ?*c.mln_texture_session = null;
        if (c.mln_vulkan_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Vulkan texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return .{ .texture = texture.? };
    }

    fn drawTexture(
        self: *VulkanNativeTextureBackend,
        texture: *c.mln_texture_session,
        _: types.Viewport,
    ) !bool {
        var frame: c.mln_vulkan_texture_frame = .{
            .size = @sizeOf(c.mln_vulkan_texture_frame),
            .generation = 0,
            .width = 0,
            .height = 0,
            .scale_factor = 0,
            .frame_id = 0,
            .image = null,
            .image_view = null,
            .device = null,
            .format = 0,
            .layout = 0,
        };
        const acquire_status = c.mln_vulkan_texture_acquire_frame(texture, &frame);
        if (acquire_status == c.MLN_STATUS_INVALID_STATE) return false;
        if (acquire_status != c.MLN_STATUS_OK) {
            diagnostics.logAbiError("Vulkan texture acquire failed");
            return types.AppError.BackendDrawFailed;
        }
        errdefer releaseVulkanFrame(texture, &frame);

        if (frame.image_view == null) return types.AppError.BackendDrawFailed;
        const image_view: c.VkImageView = @ptrCast(frame.image_view.?);
        if (!try self.compositor.presentImageView(image_view)) {
            releaseVulkanFrame(texture, &frame);
            return false;
        }

        self.pending_texture = texture;
        self.pending_frame = frame;
        return true;
    }

    fn releasePendingFrame(self: *VulkanNativeTextureBackend) void {
        if (self.pending_texture) |texture| {
            if (self.pending_frame) |*frame| releaseVulkanFrame(texture, frame);
        }
        self.pending_texture = null;
        self.pending_frame = null;
    }
};

const VulkanSharedTextureBackend = struct {
    compositor: VulkanTextureCompositor,
    pending_texture: ?*c.mln_texture_session,
    pending_frame: ?c.mln_shared_texture_frame,

    fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
    ) !VulkanSharedTextureBackend {
        return .{
            .compositor = try VulkanTextureCompositor.init(allocator, window, viewport, true),
            .pending_texture = null,
            .pending_frame = null,
        };
    }

    fn deinit(self: *VulkanSharedTextureBackend) void {
        self.compositor.waitIdle();
        self.releasePendingFrame();
        self.compositor.deinit();
    }

    fn resize(self: *VulkanSharedTextureBackend, viewport: types.Viewport) !void {
        self.compositor.waitIdle();
        self.releasePendingFrame();
        try self.compositor.resize(viewport);
    }

    fn finishFrame(self: *VulkanSharedTextureBackend) !void {
        if (self.pending_frame == null) return;
        try self.compositor.waitForFrame();
        self.releasePendingFrame();
    }

    fn attachRenderTarget(
        self: *VulkanSharedTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_shared_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.required_export_type = c.MLN_SHARED_TEXTURE_EXPORT_DMA_BUF;
        descriptor.instance = self.compositor.context.instance;
        descriptor.physical_device = self.compositor.context.physical_device;
        descriptor.device = self.compositor.context.device;
        descriptor.graphics_queue = self.compositor.context.queue;
        descriptor.graphics_queue_family_index = self.compositor.context.queue_family_index;

        var texture: ?*c.mln_texture_session = null;
        if (c.mln_shared_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("shared texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return .{ .texture = texture.? };
    }

    fn drawTexture(
        self: *VulkanSharedTextureBackend,
        texture: *c.mln_texture_session,
        _: types.Viewport,
    ) !bool {
        var frame = std.mem.zeroes(c.mln_shared_texture_frame);
        frame.size = @sizeOf(c.mln_shared_texture_frame);
        const acquire_status = c.mln_texture_acquire_shared_frame(texture, &frame);
        if (acquire_status == c.MLN_STATUS_INVALID_STATE) return false;
        if (acquire_status != c.MLN_STATUS_OK) {
            diagnostics.logAbiError("shared texture acquire failed");
            return types.AppError.BackendDrawFailed;
        }
        errdefer releaseSharedFrame(texture, &frame);

        if (frame.producer_backend != c.MLN_TEXTURE_BACKEND_VULKAN or
            frame.native_view == null or
            frame.export_type != c.MLN_SHARED_TEXTURE_EXPORT_DMA_BUF or
            frame.export_fd < 0)
        {
            return types.AppError.BackendDrawFailed;
        }
        const image_view: c.VkImageView = @ptrCast(frame.native_view.?);
        if (!try self.compositor.presentImageView(image_view)) {
            releaseSharedFrame(texture, &frame);
            return false;
        }

        self.pending_texture = texture;
        self.pending_frame = frame;
        return true;
    }

    fn releasePendingFrame(self: *VulkanSharedTextureBackend) void {
        if (self.pending_texture) |texture| {
            if (self.pending_frame) |*frame| releaseSharedFrame(texture, frame);
        }
        self.pending_texture = null;
        self.pending_frame = null;
    }
};

const VulkanSurfaceBackend = struct {
    context: Context,

    fn init(allocator: std.mem.Allocator, window: *c.SDL_Window) !VulkanSurfaceBackend {
        return .{ .context = try Context.init(allocator, window, false) };
    }

    fn deinit(self: *VulkanSurfaceBackend) void {
        self.context.waitIdle();
        self.context.deinit();
    }

    fn resize(_: *VulkanSurfaceBackend, _: types.Viewport) !void {}

    fn finishFrame(_: *VulkanSurfaceBackend) !void {}

    fn attachRenderTarget(
        self: *VulkanSurfaceBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_vulkan_surface_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.instance = self.context.instance;
        descriptor.physical_device = self.context.physical_device;
        descriptor.device = self.context.device;
        descriptor.graphics_queue = self.context.queue;
        descriptor.graphics_queue_family_index = self.context.queue_family_index;
        descriptor.surface = self.context.surface;

        var surface: ?*c.mln_surface_session = null;
        if (c.mln_vulkan_surface_attach(map, &descriptor, &surface) !=
            c.MLN_STATUS_OK or surface == null)
        {
            diagnostics.logAbiError("Vulkan surface attach failed");
            return types.AppError.SurfaceAttachFailed;
        }
        return .{ .surface = surface.? };
    }
};

fn releaseVulkanFrame(
    texture: *c.mln_texture_session,
    frame: *const c.mln_vulkan_texture_frame,
) void {
    if (c.mln_vulkan_texture_release_frame(texture, frame) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("Vulkan texture release failed");
    }
}

fn releaseSharedFrame(
    texture: *c.mln_texture_session,
    frame: *const c.mln_shared_texture_frame,
) void {
    if (frame.export_fd >= 0) _ = close(frame.export_fd);
    if (c.mln_texture_release_shared_frame(texture, frame) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("shared texture release failed");
    }
}
