const std = @import("std");

const c = @import("../../c.zig").c;
const types = @import("../../types.zig");
const Context = @import("context.zig").Context;
const util = @import("util.zig");

pub const Swapchain = struct {
    allocator: std.mem.Allocator,
    handle: c.VkSwapchainKHR,
    format: c.VkFormat,
    extent: c.VkExtent2D,
    images: []c.VkImage,
    views: []c.VkImageView,
    framebuffers: []c.VkFramebuffer,

    pub fn init(
        allocator: std.mem.Allocator,
        context: *const Context,
        viewport: types.Viewport,
    ) !Swapchain {
        var self = Swapchain{
            .allocator = allocator,
            .handle = null,
            .format = c.VK_FORMAT_UNDEFINED,
            .extent = .{ .width = 0, .height = 0 },
            .images = &.{},
            .views = &.{},
            .framebuffers = &.{},
        };
        errdefer self.deinit(context.device);

        try self.create(context, viewport);
        return self;
    }

    pub fn deinit(self: *Swapchain, device: c.VkDevice) void {
        for (self.framebuffers) |framebuffer| {
            c.vkDestroyFramebuffer(device, framebuffer, null);
        }
        self.allocator.free(self.framebuffers);
        self.framebuffers = &.{};

        for (self.views) |view| c.vkDestroyImageView(device, view, null);
        self.allocator.free(self.views);
        self.views = &.{};

        self.allocator.free(self.images);
        self.images = &.{};

        if (self.handle != null) c.vkDestroySwapchainKHR(device, self.handle, null);
        self.handle = null;
    }

    pub fn createFramebuffers(
        self: *Swapchain,
        device: c.VkDevice,
        render_pass: c.VkRenderPass,
    ) !void {
        self.framebuffers = try self.allocator.alloc(c.VkFramebuffer, self.views.len);
        @memset(self.framebuffers, null);
        for (self.views, 0..) |view, index| {
            const framebuffer_info = c.VkFramebufferCreateInfo{
                .sType = c.VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = null,
                .flags = 0,
                .renderPass = render_pass,
                .attachmentCount = 1,
                .pAttachments = &view,
                .width = self.extent.width,
                .height = self.extent.height,
                .layers = 1,
            };
            try util.expectVk(c.vkCreateFramebuffer(
                device,
                &framebuffer_info,
                null,
                &self.framebuffers[index],
            ));
        }
    }

    fn create(self: *Swapchain, context: *const Context, viewport: types.Viewport) !void {
        var capabilities: c.VkSurfaceCapabilitiesKHR = undefined;
        try util.expectVk(c.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            context.physical_device,
            context.surface,
            &capabilities,
        ));
        var format_count: u32 = 0;
        try util.expectVk(c.vkGetPhysicalDeviceSurfaceFormatsKHR(
            context.physical_device,
            context.surface,
            &format_count,
            null,
        ));
        const formats = try self.allocator.alloc(c.VkSurfaceFormatKHR, format_count);
        defer self.allocator.free(formats);
        try util.expectVk(c.vkGetPhysicalDeviceSurfaceFormatsKHR(
            context.physical_device,
            context.surface,
            &format_count,
            formats.ptr,
        ));

        const format = chooseSurfaceFormat(formats);
        self.format = format.format;
        self.extent = chooseExtent(capabilities, viewport);

        var image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 and
            image_count > capabilities.maxImageCount)
        {
            image_count = capabilities.maxImageCount;
        }

        const create_info = c.VkSwapchainCreateInfoKHR{
            .sType = c.VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = null,
            .flags = 0,
            .surface = context.surface,
            .minImageCount = image_count,
            .imageFormat = self.format,
            .imageColorSpace = format.colorSpace,
            .imageExtent = self.extent,
            .imageArrayLayers = 1,
            .imageUsage = c.VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = c.VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = null,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = c.VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = c.VK_PRESENT_MODE_FIFO_KHR,
            .clipped = c.VK_TRUE,
            .oldSwapchain = null,
        };
        try util.expectVk(c.vkCreateSwapchainKHR(
            context.device,
            &create_info,
            null,
            &self.handle,
        ));

        var actual_count: u32 = 0;
        try util.expectVk(c.vkGetSwapchainImagesKHR(
            context.device,
            self.handle,
            &actual_count,
            null,
        ));
        self.images = try self.allocator.alloc(c.VkImage, actual_count);
        try util.expectVk(c.vkGetSwapchainImagesKHR(
            context.device,
            self.handle,
            &actual_count,
            self.images.ptr,
        ));
        self.views = try self.allocator.alloc(c.VkImageView, actual_count);
        @memset(self.views, null);
        for (self.images, 0..) |image, index| {
            self.views[index] = try createImageView(context.device, image, self.format);
        }
    }
};

fn chooseSurfaceFormat(formats: []const c.VkSurfaceFormatKHR) c.VkSurfaceFormatKHR {
    for (formats) |format| {
        const supported_format = format.format == c.VK_FORMAT_B8G8R8A8_UNORM or
            format.format == c.VK_FORMAT_R8G8B8A8_UNORM;
        const supported_color_space = format.colorSpace ==
            c.VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        if (supported_format and supported_color_space) return format;
    }
    return formats[0];
}

fn chooseExtent(capabilities: c.VkSurfaceCapabilitiesKHR, viewport: types.Viewport) c.VkExtent2D {
    if (capabilities.currentExtent.width != std.math.maxInt(u32)) {
        return capabilities.currentExtent;
    }
    return .{
        .width = std.math.clamp(
            viewport.physical_width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width,
        ),
        .height = std.math.clamp(
            viewport.physical_height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height,
        ),
    };
}

fn createImageView(device: c.VkDevice, image: c.VkImage, format: c.VkFormat) !c.VkImageView {
    const create_info = c.VkImageViewCreateInfo{
        .sType = c.VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .image = image,
        .viewType = c.VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
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
    var image_view: c.VkImageView = null;
    try util.expectVk(c.vkCreateImageView(device, &create_info, null, &image_view));
    return image_view;
}
