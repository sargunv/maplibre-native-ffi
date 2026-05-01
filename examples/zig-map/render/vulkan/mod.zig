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

pub const VulkanBackend = union(enum) {
    pub const window_flags = c.SDL_WINDOW_VULKAN;

    texture: VulkanTextureBackend,
    surface: VulkanSurfaceBackend,

    pub fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        mode: types.RenderTargetMode,
    ) !VulkanBackend {
        return switch (mode) {
            .native_texture => .{ .texture = try VulkanTextureBackend.init(allocator, window, viewport, .native) },
            .shared_texture => .{ .texture = try VulkanTextureBackend.init(allocator, window, viewport, .shared) },
            .native_surface => .{ .surface = try VulkanSurfaceBackend.init(allocator, window) },
        };
    }

    pub fn deinit(self: *VulkanBackend) void {
        switch (self.*) {
            .texture => |*backend| backend.deinit(),
            .surface => |*backend| backend.deinit(),
        }
    }

    pub fn resize(self: *VulkanBackend, viewport: types.Viewport) !void {
        switch (self.*) {
            .texture => |*backend| try backend.resize(viewport),
            .surface => |*backend| try backend.resize(viewport),
        }
    }

    pub fn finishFrame(self: *VulkanBackend) !void {
        switch (self.*) {
            .texture => |*backend| try backend.finishFrame(),
            .surface => |*backend| try backend.finishFrame(),
        }
    }

    pub fn attachRenderTarget(
        self: *VulkanBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        return switch (self.*) {
            .texture => |*backend| backend.attachRenderTarget(map, viewport),
            .surface => |*backend| backend.attachRenderTarget(map, viewport),
        };
    }

    pub fn drawTexture(
        self: *VulkanBackend,
        texture: render_target.TextureSession,
        viewport: types.Viewport,
    ) !bool {
        return switch (self.*) {
            .texture => |*backend| backend.drawTexture(texture, viewport),
            .surface => unreachable,
        };
    }
};

const VulkanTextureBackend = struct {
    context: Context,
    swapchain: Swapchain,
    pipeline: Pipeline,
    commands: Commands,
    mode: render_target.TextureMode,
    pending_texture: ?*c.mln_texture_session,
    pending_frame_kind: PendingFrameKind,
    pending_vulkan_frame: ?c.mln_vulkan_texture_frame,
    pending_shared_frame: ?c.mln_shared_texture_frame,

    fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        mode: render_target.TextureMode,
    ) !VulkanTextureBackend {
        var context = try Context.init(allocator, window);
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
            .mode = mode,
            .pending_texture = null,
            .pending_frame_kind = .none,
            .pending_vulkan_frame = null,
            .pending_shared_frame = null,
        };
    }

    fn deinit(self: *VulkanTextureBackend) void {
        self.context.waitIdle();
        self.releasePendingFrame();
        self.commands.deinit(self.context.device);
        self.swapchain.deinit(self.context.device);
        self.pipeline.deinit(self.context.device);
        self.context.deinit();
    }

    fn resize(self: *VulkanTextureBackend, viewport: types.Viewport) !void {
        self.context.waitIdle();
        self.releasePendingFrame();

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

    fn finishFrame(self: *VulkanTextureBackend) !void {
        if (self.pending_frame_kind == .none) return;
        try self.commands.waitForFrameFence(self.context.device);
        self.releasePendingFrame();
    }

    fn attachRenderTarget(
        self: *VulkanTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        return switch (self.mode) {
            .native => .{ .texture = .{
                .handle = try self.attachNativeTexture(map, viewport),
                .mode = .native,
            } },
            .shared => .{ .texture = .{
                .handle = try self.attachSharedTexture(map, viewport),
                .mode = .shared,
            } },
        };
    }

    fn attachNativeTexture(
        self: *VulkanTextureBackend,
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
        _: *VulkanTextureBackend,
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

    fn drawTexture(
        self: *VulkanTextureBackend,
        texture: render_target.TextureSession,
        _: types.Viewport,
    ) !bool {
        return switch (texture.mode) {
            .native => self.drawNativeTexture(texture.handle),
            .shared => self.drawSharedTexture(texture.handle),
        };
    }

    fn drawNativeTexture(
        self: *VulkanTextureBackend,
        texture: *c.mln_texture_session,
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
        if (!try self.presentImageView(image_view)) {
            releaseVulkanFrame(texture, &frame);
            return false;
        }

        self.pending_texture = texture;
        self.pending_frame_kind = .vulkan;
        self.pending_vulkan_frame = frame;
        return true;
    }

    fn drawSharedTexture(
        self: *VulkanTextureBackend,
        texture: *c.mln_texture_session,
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

    fn presentImageView(self: *VulkanTextureBackend, image_view: c.VkImageView) !bool {
        if (image_view != self.pipeline.descriptor_image_view) {
            self.pipeline.updateDescriptor(self.context.device, image_view);
        }

        try self.commands.waitForFrameFence(self.context.device);

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
            self.commands.waitForFrameFence(self.context.device) catch {};
            try util.expectVk(present);
        }
        return true;
    }

    fn releasePendingFrame(self: *VulkanTextureBackend) void {
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

const VulkanSurfaceBackend = struct {
    context: Context,

    fn init(allocator: std.mem.Allocator, window: *c.SDL_Window) !VulkanSurfaceBackend {
        return .{ .context = try Context.init(allocator, window) };
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
    if (c.mln_texture_release_shared_frame(texture, frame) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("shared texture release failed");
    }
}
