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

const PendingFrameKind = enum { none, vulkan, shared };

pub const VulkanBackend = struct {
    pub const window_flags = c.SDL_WINDOW_VULKAN;

    context: Context,
    swapchain: ?Swapchain,
    pipeline: ?Pipeline,
    commands: ?Commands,
    pending_texture: ?*c.mln_texture_session,
    pending_frame_kind: PendingFrameKind,
    pending_vulkan_frame: ?c.mln_vulkan_texture_frame,
    pending_shared_frame: ?c.mln_shared_texture_frame,

    pub fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        mode: types.RenderTargetMode,
    ) !VulkanBackend {
        var context = try Context.init(allocator, window);
        errdefer context.deinit();

        if (mode == .native_surface) {
            return .{
                .context = context,
                .swapchain = null,
                .pipeline = null,
                .commands = null,
                .pending_texture = null,
                .pending_frame_kind = .none,
                .pending_vulkan_frame = null,
                .pending_shared_frame = null,
            };
        }

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
            .pending_texture = null,
            .pending_frame_kind = .none,
            .pending_vulkan_frame = null,
            .pending_shared_frame = null,
        };
    }

    pub fn deinit(self: *VulkanBackend) void {
        self.context.waitIdle();
        self.releasePendingFrame();
        if (self.commands) |*commands| commands.deinit(self.context.device);
        if (self.swapchain) |*swapchain| swapchain.deinit(self.context.device);
        if (self.pipeline) |*pipeline| pipeline.deinit(self.context.device);
        self.context.deinit();
    }

    pub fn resize(self: *VulkanBackend, viewport: types.Viewport) !void {
        const swapchain = if (self.swapchain) |*value| value else return;
        const pipeline = if (self.pipeline) |*value| value else return;
        self.context.waitIdle();
        self.releasePendingFrame();

        const previous_format = swapchain.format;
        swapchain.deinit(self.context.device);
        swapchain.* = try Swapchain.init(
            swapchain.allocator,
            &self.context,
            viewport,
        );

        if (swapchain.format != previous_format) {
            pipeline.deinit(self.context.device);
            pipeline.* = try Pipeline.init(
                pipeline.allocator,
                self.context.device,
                swapchain.format,
            );
        }
        try swapchain.createFramebuffers(
            self.context.device,
            pipeline.render_pass,
        );
    }

    pub fn attachRenderTarget(
        self: *VulkanBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
        mode: types.RenderTargetMode,
    ) !render_target.Session {
        return switch (mode) {
            .native_texture => .{ .texture = try self.attachNativeTexture(map, viewport) },
            .shared_texture => .{ .texture = try self.attachSharedTexture(map, viewport) },
            .native_surface => .{ .surface = try self.attachNativeSurface(map, viewport) },
        };
    }

    fn attachNativeTexture(
        self: *VulkanBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !*c.mln_texture_session {
        var descriptor = c.mln_vulkan_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.instance = self.context.instance;
        descriptor.physical_device = self.context.physical_device;
        descriptor.device = self.context.device;
        descriptor.graphics_queue = self.context.queue;
        descriptor.graphics_queue_family_index = self.context.queue_family_index;

        var texture: ?*c.mln_texture_session = null;
        if (c.mln_vulkan_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Vulkan texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return texture.?;
    }

    fn attachSharedTexture(
        _: *VulkanBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !*c.mln_texture_session {
        var descriptor = c.mln_shared_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.required_handle_type = c.MLN_SHARED_TEXTURE_HANDLE_VULKAN_IMAGE;

        var texture: ?*c.mln_texture_session = null;
        if (c.mln_shared_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("shared texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return texture.?;
    }

    fn attachNativeSurface(
        self: *VulkanBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !*c.mln_surface_session {
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
        return surface.?;
    }

    pub fn draw(
        self: *VulkanBackend,
        texture: *c.mln_texture_session,
        mode: types.RenderTargetMode,
        _: types.Viewport,
    ) !bool {
        return switch (mode) {
            .native_texture => self.drawNativeTexture(texture),
            .shared_texture => self.drawSharedTexture(texture),
            .native_surface => true,
        };
    }

    fn drawNativeTexture(self: *VulkanBackend, texture: *c.mln_texture_session) !bool {
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
        if (!try self.presentImageView(image_view)) {
            releaseVulkanFrame(texture, &frame);
            return false;
        }

        self.pending_texture = texture;
        self.pending_frame_kind = .vulkan;
        self.pending_vulkan_frame = frame;
        return true;
    }

    fn drawSharedTexture(self: *VulkanBackend, texture: *c.mln_texture_session) !bool {
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
            frame.native_handle_type != c.MLN_SHARED_TEXTURE_HANDLE_VULKAN_IMAGE or
            frame.native_view == null)
        {
            return types.AppError.BackendDrawFailed;
        }
        const image_view: c.VkImageView = @ptrCast(frame.native_view.?);
        if (!try self.presentImageView(image_view)) {
            releaseSharedFrame(texture, &frame);
            return false;
        }

        self.pending_texture = texture;
        self.pending_frame_kind = .shared;
        self.pending_shared_frame = frame;
        return true;
    }

    fn presentImageView(self: *VulkanBackend, image_view: c.VkImageView) !bool {
        const swapchain = if (self.swapchain) |*value| value else return types.AppError.BackendDrawFailed;
        const pipeline = if (self.pipeline) |*value| value else return types.AppError.BackendDrawFailed;
        const commands = if (self.commands) |*value| value else return types.AppError.BackendDrawFailed;

        if (image_view != pipeline.descriptor_image_view) {
            pipeline.updateDescriptor(self.context.device, image_view);
        }

        try commands.waitForFrameFence(self.context.device);

        var image_index: u32 = 0;
        const acquire = c.vkAcquireNextImageKHR(
            self.context.device,
            swapchain.handle,
            std.math.maxInt(u64),
            commands.image_available,
            null,
            &image_index,
        );
        if (acquire == c.VK_ERROR_OUT_OF_DATE_KHR) return false;
        try util.expectVkOrSuboptimal(acquire);
        try commands.resetFence(self.context.device);

        try commands.record(
            self.context.device,
            swapchain,
            pipeline,
            image_index,
        );
        try commands.submit(self.context.queue);

        const present_info = c.VkPresentInfoKHR{
            .sType = c.VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = null,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &commands.render_finished,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.handle,
            .pImageIndices = &image_index,
            .pResults = null,
        };
        const present = c.vkQueuePresentKHR(self.context.queue, &present_info);
        if (present != c.VK_SUCCESS and
            present != c.VK_SUBOPTIMAL_KHR and
            present != c.VK_ERROR_OUT_OF_DATE_KHR)
        {
            commands.waitForFrameFence(self.context.device) catch {};
            try util.expectVk(present);
        }
        return true;
    }

    pub fn finishFrame(self: *VulkanBackend) !void {
        if (self.pending_frame_kind == .none) return;
        const commands = if (self.commands) |*value| value else {
            self.releasePendingFrame();
            return;
        };
        try commands.waitForFrameFence(self.context.device);
        self.releasePendingFrame();
    }

    fn releasePendingFrame(self: *VulkanBackend) void {
        if (self.pending_texture) |texture| switch (self.pending_frame_kind) {
            .none => {},
            .vulkan => if (self.pending_vulkan_frame) |*frame| {
                releaseVulkanFrame(texture, frame);
            },
            .shared => if (self.pending_shared_frame) |*frame| {
                releaseSharedFrame(texture, frame);
            },
        };
        self.pending_texture = null;
        self.pending_frame_kind = .none;
        self.pending_vulkan_frame = null;
        self.pending_shared_frame = null;
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
    if (c.mln_texture_release_shared_frame(texture, frame) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("shared texture release failed");
    }
}
