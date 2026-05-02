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

pub const VulkanBackend = union(enum) {
    pub const window_flags = c.SDL_WINDOW_VULKAN;

    owned_texture: VulkanOwnedTextureBackend,
    borrowed_texture: VulkanBorrowedTextureBackend,
    native_surface: VulkanSurfaceBackend,

    pub fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
        mode: types.RenderTargetMode,
    ) !VulkanBackend {
        return switch (mode) {
            .owned_texture => .{ .owned_texture = try VulkanOwnedTextureBackend.init(allocator, window, viewport) },
            .borrowed_texture => .{ .borrowed_texture = try VulkanBorrowedTextureBackend.init(allocator, window, viewport) },
            .native_surface => .{ .native_surface = try VulkanSurfaceBackend.init(allocator, window) },
        };
    }

    pub fn deinit(self: *VulkanBackend) void {
        switch (self.*) {
            .owned_texture => |*backend| backend.deinit(),
            .borrowed_texture => |*backend| backend.deinit(),
            .native_surface => |*backend| backend.deinit(),
        }
    }

    pub fn resize(self: *VulkanBackend, viewport: types.Viewport) !void {
        switch (self.*) {
            .owned_texture => |*backend| try backend.resize(viewport),
            .borrowed_texture => |*backend| try backend.resize(viewport),
            .native_surface => |*backend| try backend.resize(viewport),
        }
    }

    pub fn needsRenderTargetReattachOnResize(self: *const VulkanBackend) bool {
        return switch (self.*) {
            .owned_texture, .native_surface => false,
            .borrowed_texture => true,
        };
    }

    pub fn finishFrame(self: *VulkanBackend) !void {
        switch (self.*) {
            .owned_texture => |*backend| try backend.finishFrame(),
            .borrowed_texture => |*backend| try backend.finishFrame(),
            .native_surface => |*backend| try backend.finishFrame(),
        }
    }

    pub fn attachRenderTarget(
        self: *VulkanBackend,
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
        self: *VulkanBackend,
        texture: *c.mln_texture_session,
        viewport: types.Viewport,
    ) !bool {
        return switch (self.*) {
            .owned_texture => |*backend| backend.drawTexture(texture, viewport),
            .borrowed_texture => |*backend| backend.drawTexture(texture, viewport),
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

const VulkanOwnedTextureBackend = struct {
    compositor: VulkanTextureCompositor,
    pending_texture: ?*c.mln_texture_session,
    pending_frame: ?c.mln_vulkan_owned_texture_frame,

    fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
    ) !VulkanOwnedTextureBackend {
        return .{
            .compositor = try VulkanTextureCompositor.init(allocator, window, viewport, false),
            .pending_texture = null,
            .pending_frame = null,
        };
    }

    fn deinit(self: *VulkanOwnedTextureBackend) void {
        self.compositor.waitIdle();
        self.releasePendingFrame();
        self.compositor.deinit();
    }

    fn resize(self: *VulkanOwnedTextureBackend, viewport: types.Viewport) !void {
        self.compositor.waitIdle();
        self.releasePendingFrame();
        try self.compositor.resize(viewport);
    }

    fn finishFrame(self: *VulkanOwnedTextureBackend) !void {
        if (self.pending_frame == null) return;
        try self.compositor.waitForFrame();
        self.releasePendingFrame();
    }

    fn attachRenderTarget(
        self: *VulkanOwnedTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_vulkan_owned_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.instance = self.compositor.context.instance;
        descriptor.physical_device = self.compositor.context.physical_device;
        descriptor.device = self.compositor.context.device;
        descriptor.graphics_queue = self.compositor.context.queue;
        descriptor.graphics_queue_family_index = self.compositor.context.queue_family_index;

        var texture: ?*c.mln_texture_session = null;
        if (c.mln_vulkan_owned_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Vulkan texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return .{ .texture = texture.? };
    }

    fn drawTexture(
        self: *VulkanOwnedTextureBackend,
        texture: *c.mln_texture_session,
        _: types.Viewport,
    ) !bool {
        var frame: c.mln_vulkan_owned_texture_frame = .{
            .size = @sizeOf(c.mln_vulkan_owned_texture_frame),
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
        const acquire_status = c.mln_vulkan_owned_texture_acquire_frame(texture, &frame);
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

    fn releasePendingFrame(self: *VulkanOwnedTextureBackend) void {
        if (self.pending_texture) |texture| {
            if (self.pending_frame) |*frame| releaseVulkanFrame(texture, frame);
        }
        self.pending_texture = null;
        self.pending_frame = null;
    }
};

const BorrowedImage = struct {
    image: c.VkImage,
    memory: c.VkDeviceMemory,
    view: c.VkImageView,

    fn init(context: *const Context, viewport: types.Viewport) !BorrowedImage {
        var self = BorrowedImage{ .image = null, .memory = null, .view = null };
        errdefer self.deinit(context.device);

        const image_info = c.VkImageCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .imageType = c.VK_IMAGE_TYPE_2D,
            .format = c.VK_FORMAT_R8G8B8A8_UNORM,
            .extent = .{
                .width = viewport.physical_width,
                .height = viewport.physical_height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = c.VK_SAMPLE_COUNT_1_BIT,
            .tiling = c.VK_IMAGE_TILING_OPTIMAL,
            .usage = c.VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | c.VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = c.VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = null,
            .initialLayout = c.VK_IMAGE_LAYOUT_UNDEFINED,
        };
        try util.expectVk(c.vkCreateImage(context.device, &image_info, null, &self.image));

        var requirements: c.VkMemoryRequirements = undefined;
        c.vkGetImageMemoryRequirements(context.device, self.image, &requirements);
        const memory_type_index = try findMemoryType(
            context.physical_device,
            requirements.memoryTypeBits,
            c.VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        );
        const allocate_info = c.VkMemoryAllocateInfo{
            .sType = c.VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = null,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memory_type_index,
        };
        try util.expectVk(c.vkAllocateMemory(context.device, &allocate_info, null, &self.memory));
        try util.expectVk(c.vkBindImageMemory(context.device, self.image, self.memory, 0));

        const view_info = c.VkImageViewCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .image = self.image,
            .viewType = c.VK_IMAGE_VIEW_TYPE_2D,
            .format = c.VK_FORMAT_R8G8B8A8_UNORM,
            .components = .{
                .r = c.VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = c.VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = c.VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = c.VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = .{
                .aspectMask = c.VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        try util.expectVk(c.vkCreateImageView(context.device, &view_info, null, &self.view));
        return self;
    }

    fn deinit(self: *BorrowedImage, device: c.VkDevice) void {
        if (self.view != null) c.vkDestroyImageView(device, self.view, null);
        if (self.image != null) c.vkDestroyImage(device, self.image, null);
        if (self.memory != null) c.vkFreeMemory(device, self.memory, null);
        self.* = .{ .image = null, .memory = null, .view = null };
    }
};

const VulkanBorrowedTextureBackend = struct {
    compositor: VulkanTextureCompositor,
    borrowed_image: BorrowedImage,

    fn init(
        allocator: std.mem.Allocator,
        window: *c.SDL_Window,
        viewport: types.Viewport,
    ) !VulkanBorrowedTextureBackend {
        var compositor = try VulkanTextureCompositor.init(allocator, window, viewport, false);
        errdefer compositor.deinit();
        return .{
            .borrowed_image = try BorrowedImage.init(&compositor.context, viewport),
            .compositor = compositor,
        };
    }

    fn deinit(self: *VulkanBorrowedTextureBackend) void {
        self.compositor.waitIdle();
        self.borrowed_image.deinit(self.compositor.context.device);
        self.compositor.deinit();
    }

    fn resize(self: *VulkanBorrowedTextureBackend, viewport: types.Viewport) !void {
        self.compositor.waitIdle();
        var borrowed_image = try BorrowedImage.init(&self.compositor.context, viewport);
        errdefer borrowed_image.deinit(self.compositor.context.device);
        try self.compositor.resize(viewport);
        self.borrowed_image.deinit(self.compositor.context.device);
        self.borrowed_image = borrowed_image;
    }

    fn finishFrame(self: *VulkanBorrowedTextureBackend) !void {
        try self.compositor.waitForFrame();
    }

    fn attachRenderTarget(
        self: *VulkanBorrowedTextureBackend,
        map: *c.mln_map,
        viewport: types.Viewport,
    ) !render_target.Session {
        var descriptor = c.mln_vulkan_borrowed_texture_descriptor_default();
        descriptor.width = viewport.logical_width;
        descriptor.height = viewport.logical_height;
        descriptor.scale_factor = viewport.scale_factor;
        descriptor.instance = self.compositor.context.instance;
        descriptor.physical_device = self.compositor.context.physical_device;
        descriptor.device = self.compositor.context.device;
        descriptor.graphics_queue = self.compositor.context.queue;
        descriptor.graphics_queue_family_index = self.compositor.context.queue_family_index;
        descriptor.image = self.borrowed_image.image;
        descriptor.image_view = self.borrowed_image.view;
        descriptor.format = c.VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.initial_layout = c.VK_IMAGE_LAYOUT_UNDEFINED;
        descriptor.final_layout = c.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        var texture: ?*c.mln_texture_session = null;
        if (c.mln_vulkan_borrowed_texture_attach(map, &descriptor, &texture) !=
            c.MLN_STATUS_OK or texture == null)
        {
            diagnostics.logAbiError("Vulkan borrowed texture attach failed");
            return types.AppError.TextureAttachFailed;
        }
        return .{ .texture = texture.? };
    }

    fn drawTexture(
        self: *VulkanBorrowedTextureBackend,
        texture: *c.mln_texture_session,
        _: types.Viewport,
    ) !bool {
        _ = texture;
        return try self.compositor.presentImageView(self.borrowed_image.view);
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
    frame: *const c.mln_vulkan_owned_texture_frame,
) void {
    if (c.mln_vulkan_owned_texture_release_frame(texture, frame) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("Vulkan texture release failed");
    }
}

fn findMemoryType(
    physical_device: c.VkPhysicalDevice,
    type_bits: u32,
    properties: c.VkMemoryPropertyFlags,
) !u32 {
    var memory_properties: c.VkPhysicalDeviceMemoryProperties = undefined;
    c.vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (0..memory_properties.memoryTypeCount) |index| {
        const bit = @as(u32, 1) << @intCast(index);
        if ((type_bits & bit) == 0) continue;
        const memory_type = memory_properties.memoryTypes[index];
        if ((memory_type.propertyFlags & properties) == properties) {
            return @intCast(index);
        }
    }
    return types.AppError.BackendSetupFailed;
}
