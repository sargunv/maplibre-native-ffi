const std = @import("std");

const c = @import("../../c.zig").c;
const diagnostics = @import("../../diagnostics.zig");
const types = @import("../../types.zig");
const Commands = @import("commands.zig").Commands;
const Context = @import("context.zig").Context;
const Pipeline = @import("pipeline.zig").Pipeline;
const Swapchain = @import("swapchain.zig").Swapchain;
const util = @import("util.zig");

pub const VulkanBackend = struct {
    pub const window_flags = c.SDL_WINDOW_VULKAN;

    context: Context,
    swapchain: Swapchain,
    pipeline: Pipeline,
    commands: Commands,
    pending_texture: ?*c.mln_texture_session,
    pending_frame: ?c.mln_vulkan_texture_frame,

    pub fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
    ) !VulkanBackend {
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
            .pending_texture = null,
            .pending_frame = null,
        };
    }

    pub fn deinit(self: *VulkanBackend) void {
        self.context.waitIdle();
        self.releasePendingFrame();
        self.commands.deinit(self.context.device);
        self.swapchain.deinit(self.context.device);
        self.pipeline.deinit(self.context.device);
        self.context.deinit();
    }

    pub fn resize(self: *VulkanBackend, viewport: types.Viewport) !void {
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

    pub fn attachTexture(
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

    pub fn draw(self: *VulkanBackend, texture: *c.mln_texture_session, _: types.Viewport) !bool {
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

        const image_view: c.VkImageView = @ptrCast(frame.image_view);
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
        if (acquire == c.VK_ERROR_OUT_OF_DATE_KHR) {
            releaseVulkanFrame(texture, &frame);
            return false;
        }
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

        self.pending_texture = texture;
        self.pending_frame = frame;
        return true;
    }

    pub fn finishFrame(self: *VulkanBackend) !void {
        if (self.pending_frame == null) return;
        try self.commands.waitForFrameFence(self.context.device);
        self.releasePendingFrame();
    }

    fn releasePendingFrame(self: *VulkanBackend) void {
        if (self.pending_texture) |texture| {
            if (self.pending_frame) |*frame| releaseVulkanFrame(texture, frame);
        }
        self.pending_texture = null;
        self.pending_frame = null;
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
