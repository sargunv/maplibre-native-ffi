const std = @import("std");
const builtin = @import("builtin");
const testing = std.testing;
const support = @import("support.zig");
const common = @import("texture.zig");
const c = support.c;

const vk = if (builtin.os.tag == .linux) @cImport({
    @cInclude("vulkan/vulkan.h");
}) else struct {};

const Backend = struct {
    pub const descriptor_size = @sizeOf(c.mln_vulkan_owned_texture_descriptor);

    pub const AttachContext = struct {
        instance: vk.VkInstance,
        physical_device: vk.VkPhysicalDevice,
        device: vk.VkDevice,
        queue: vk.VkQueue,
        queue_family_index: u32,

        pub fn init() !AttachContext {
            return initWithDeviceExtensions(&.{});
        }

        fn initWithDeviceExtensions(required_extensions: []const [*c]const u8) !AttachContext {
            var app_info = std.mem.zeroes(vk.VkApplicationInfo);
            app_info.sType = vk.VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pApplicationName = "maplibre-native-c-tests";
            app_info.applicationVersion = 1;
            app_info.pEngineName = "maplibre-native-c-tests";
            app_info.engineVersion = 1;
            app_info.apiVersion = vk.VK_API_VERSION_1_1;

            var instance_info = std.mem.zeroes(vk.VkInstanceCreateInfo);
            instance_info.sType = vk.VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pApplicationInfo = &app_info;

            var instance: vk.VkInstance = null;
            try expectVk(vk.vkCreateInstance(&instance_info, null, &instance));
            errdefer vk.vkDestroyInstance(instance, null);

            var physical_device_count: u32 = 0;
            try expectVk(vk.vkEnumeratePhysicalDevices(instance, &physical_device_count, null));
            try testing.expect(physical_device_count != 0);

            const physical_devices = try testing.allocator.alloc(vk.VkPhysicalDevice, physical_device_count);
            defer testing.allocator.free(physical_devices);
            try expectVk(vk.vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.ptr));

            for (physical_devices) |physical_device| {
                var queue_family_count: u32 = 0;
                vk.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, null);
                if (queue_family_count == 0) continue;

                const queue_families = try testing.allocator.alloc(vk.VkQueueFamilyProperties, queue_family_count);
                defer testing.allocator.free(queue_families);
                vk.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.ptr);

                for (queue_families, 0..) |queue_family, index| {
                    if ((queue_family.queueFlags & vk.VK_QUEUE_GRAPHICS_BIT) == 0 or queue_family.queueCount == 0) continue;

                    var priority: f32 = 1.0;
                    var queue_info = std.mem.zeroes(vk.VkDeviceQueueCreateInfo);
                    queue_info.sType = vk.VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    queue_info.queueFamilyIndex = @intCast(index);
                    queue_info.queueCount = 1;
                    queue_info.pQueuePriorities = &priority;

                    var features = std.mem.zeroes(vk.VkPhysicalDeviceFeatures);
                    features.samplerAnisotropy = vk.VK_TRUE;
                    features.wideLines = vk.VK_TRUE;

                    var device_info = std.mem.zeroes(vk.VkDeviceCreateInfo);
                    device_info.sType = vk.VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                    device_info.queueCreateInfoCount = 1;
                    device_info.pQueueCreateInfos = &queue_info;
                    device_info.enabledExtensionCount = @intCast(required_extensions.len);
                    device_info.ppEnabledExtensionNames = if (required_extensions.len == 0) null else required_extensions.ptr;
                    device_info.pEnabledFeatures = &features;

                    var device: vk.VkDevice = null;
                    if (vk.vkCreateDevice(physical_device, &device_info, null, &device) != vk.VK_SUCCESS) continue;

                    var queue: vk.VkQueue = null;
                    vk.vkGetDeviceQueue(device, @intCast(index), 0, &queue);
                    return .{
                        .instance = instance,
                        .physical_device = physical_device,
                        .device = device,
                        .queue = queue,
                        .queue_family_index = @intCast(index),
                    };
                }
            }

            return error.NoUsableVulkanGraphicsQueue;
        }

        pub fn deinit(self: *AttachContext) void {
            _ = vk.vkDeviceWaitIdle(self.device);
            vk.vkDestroyDevice(self.device, null);
            vk.vkDestroyInstance(self.instance, null);
        }

        pub fn descriptor(self: *const AttachContext) c.mln_vulkan_owned_texture_descriptor {
            var value = c.mln_vulkan_owned_texture_descriptor_default();
            value.instance = self.instance;
            value.physical_device = self.physical_device;
            value.device = self.device;
            value.graphics_queue = self.queue;
            value.graphics_queue_family_index = self.queue_family_index;
            return value;
        }
    };

    pub const Fixture = struct {
        texture: *c.mln_texture_session,
        context: ?AttachContext,

        pub fn create(map: *c.mln_map) !Fixture {
            var context = try AttachContext.init();
            errdefer context.deinit();

            var texture_descriptor = context.descriptor();
            texture_descriptor.width = 256;
            texture_descriptor.height = 256;

            var texture: ?*c.mln_texture_session = null;
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_vulkan_owned_texture_attach(map, &texture_descriptor, &texture));
            return .{ .texture = texture orelse return error.TextureCreateFailed, .context = context };
        }

        pub fn descriptor(self: *const Fixture) c.mln_vulkan_owned_texture_descriptor {
            return self.context.?.descriptor();
        }

        pub fn destroy(self: *Fixture) void {
            testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(self.texture)) catch @panic("texture destroy failed");
            self.deinitContextOnly();
        }

        pub fn deinitContextOnly(self: *Fixture) void {
            if (self.context) |*context| context.deinit();
            self.context = null;
        }

        pub fn markDestroyed(self: *Fixture) void {
            self.context = null;
        }

        pub fn acquire(_: *Fixture, frame: *Frame) c.mln_status {
            return c.mln_vulkan_owned_texture_acquire_frame(frame.texture, &frame.vulkan);
        }

        pub fn release(_: *Fixture, frame: *const Frame) c.mln_status {
            return c.mln_vulkan_owned_texture_release_frame(frame.texture, &frame.vulkan);
        }
    };

    pub const Frame = struct {
        texture: *c.mln_texture_session,
        vulkan: c.mln_vulkan_owned_texture_frame,

        pub fn empty(texture: *c.mln_texture_session) Frame {
            return .{
                .texture = texture,
                .vulkan = .{
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
                },
            };
        }

        pub fn bumpFrameId(self: *Frame) void {
            self.vulkan.frame_id += 1;
        }

        pub fn clearNativeHandles(self: *Frame) void {
            self.vulkan.image = null;
            self.vulkan.image_view = null;
            self.vulkan.device = null;
        }

        pub fn resetSize(self: *Frame) void {
            self.vulkan.size = @sizeOf(c.mln_vulkan_owned_texture_frame);
        }
    };

    pub fn attach(map: ?*c.mln_map, descriptor: ?*const c.mln_vulkan_owned_texture_descriptor, out_texture: ?*?*c.mln_texture_session) c.mln_status {
        return c.mln_vulkan_owned_texture_attach(map, descriptor, out_texture);
    }

    pub fn clearRequiredHandle(descriptor: *c.mln_vulkan_owned_texture_descriptor) void {
        descriptor.device = null;
    }

    pub fn expectInitialFrame(frame: *const Frame) !void {
        try testing.expect(frame.vulkan.image != null);
        try testing.expect(frame.vulkan.image_view != null);
        try testing.expect(frame.vulkan.device != null);
        try testing.expectEqual(@as(u32, vk.VK_FORMAT_R8G8B8A8_UNORM), frame.vulkan.format);
        try testing.expectEqual(@as(u32, vk.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), frame.vulkan.layout);
        try testing.expectEqual(@as(u32, 256), frame.vulkan.width);
        try testing.expectEqual(@as(u32, 256), frame.vulkan.height);
        try testing.expectEqual(@as(u64, 1), frame.vulkan.generation);
        try testing.expect(frame.vulkan.frame_id != 0);
    }

    pub fn expectResizedFrame(frame: *const Frame) !void {
        try testing.expectEqual(@as(u32, vk.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), frame.vulkan.layout);
        try testing.expectEqual(@as(u32, 256), frame.vulkan.width);
        try testing.expectEqual(@as(u32, 128), frame.vulkan.height);
        try testing.expectEqual(@as(f64, 2.0), frame.vulkan.scale_factor);
        try testing.expectEqual(@as(u64, 2), frame.vulkan.generation);
    }
};

const BorrowedImage = struct {
    context: Backend.AttachContext,
    image: vk.VkImage,
    image_view: vk.VkImageView,
    memory: vk.VkDeviceMemory,
    width: u32,
    height: u32,

    pub fn create(width: u32, height: u32) !BorrowedImage {
        var context = try Backend.AttachContext.init();
        errdefer context.deinit();

        var image: vk.VkImage = null;
        var memory: vk.VkDeviceMemory = null;
        var image_view: vk.VkImageView = null;
        errdefer {
            if (image_view != null) vk.vkDestroyImageView(context.device, image_view, null);
            if (image != null) vk.vkDestroyImage(context.device, image, null);
            if (memory != null) vk.vkFreeMemory(context.device, memory, null);
        }

        var image_info = std.mem.zeroes(vk.VkImageCreateInfo);
        image_info.sType = vk.VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = vk.VK_IMAGE_TYPE_2D;
        image_info.format = vk.VK_FORMAT_R8G8B8A8_UNORM;
        image_info.extent = .{ .width = width, .height = height, .depth = 1 };
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = vk.VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = vk.VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = vk.VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk.VK_IMAGE_USAGE_SAMPLED_BIT | vk.VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.sharingMode = vk.VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = vk.VK_IMAGE_LAYOUT_UNDEFINED;
        try expectVk(vk.vkCreateImage(context.device, &image_info, null, &image));

        var requirements: vk.VkMemoryRequirements = undefined;
        vk.vkGetImageMemoryRequirements(context.device, image, &requirements);

        var allocate_info = std.mem.zeroes(vk.VkMemoryAllocateInfo);
        allocate_info.sType = vk.VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = requirements.size;
        allocate_info.memoryTypeIndex = try findMemoryType(context.physical_device, requirements.memoryTypeBits, vk.VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        try expectVk(vk.vkAllocateMemory(context.device, &allocate_info, null, &memory));
        try expectVk(vk.vkBindImageMemory(context.device, image, memory, 0));

        var view_info = std.mem.zeroes(vk.VkImageViewCreateInfo);
        view_info.sType = vk.VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = vk.VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = vk.VK_FORMAT_R8G8B8A8_UNORM;
        view_info.subresourceRange = .{
            .aspectMask = vk.VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        try expectVk(vk.vkCreateImageView(context.device, &view_info, null, &image_view));

        return .{
            .context = context,
            .image = image,
            .image_view = image_view,
            .memory = memory,
            .width = width,
            .height = height,
        };
    }

    pub fn deinit(self: *BorrowedImage) void {
        _ = vk.vkDeviceWaitIdle(self.context.device);
        vk.vkDestroyImageView(self.context.device, self.image_view, null);
        vk.vkDestroyImage(self.context.device, self.image, null);
        vk.vkFreeMemory(self.context.device, self.memory, null);
        self.context.deinit();
    }

    pub fn descriptor(self: *const BorrowedImage) c.mln_vulkan_borrowed_texture_descriptor {
        var value = c.mln_vulkan_borrowed_texture_descriptor_default();
        value.width = self.width;
        value.height = self.height;
        value.instance = self.context.instance;
        value.physical_device = self.context.physical_device;
        value.device = self.context.device;
        value.graphics_queue = self.context.queue;
        value.graphics_queue_family_index = self.context.queue_family_index;
        value.image = self.image;
        value.image_view = self.image_view;
        value.format = @as(u32, vk.VK_FORMAT_R8G8B8A8_UNORM);
        value.initial_layout = @as(u32, vk.VK_IMAGE_LAYOUT_UNDEFINED);
        value.final_layout = @as(u32, vk.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return value;
    }
};

test "Vulkan texture unsupported backend validates arguments" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_vulkan_owned_texture_descriptor_default();
    descriptor.instance = @ptrFromInt(1);
    descriptor.physical_device = @ptrFromInt(1);
    descriptor.device = @ptrFromInt(1);
    descriptor.graphics_queue = @ptrFromInt(1);

    var texture: ?*c.mln_texture_session = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_owned_texture_attach(map, &descriptor, &texture));
    try testing.expect(texture != null);

    texture = null;
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_vulkan_owned_texture_attach(map, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);
}

fn expectVk(result: vk.VkResult) !void {
    try testing.expectEqual(vk.VK_SUCCESS, result);
}

fn findMemoryType(physical_device: vk.VkPhysicalDevice, type_filter: u32, properties: vk.VkMemoryPropertyFlags) !u32 {
    var memory_properties: vk.VkPhysicalDeviceMemoryProperties = undefined;
    vk.vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (0..memory_properties.memoryTypeCount) |index| {
        const type_bit = @as(u32, 1) << @as(u5, @intCast(index));
        const memory_type = memory_properties.memoryTypes[index];
        if ((type_filter & type_bit) != 0 and (memory_type.propertyFlags & properties) == properties) {
            return @intCast(index);
        }
    }
    return error.NoSuitableVulkanMemoryType;
}

test "Vulkan texture attach rejects invalid arguments" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectAttachRejectsInvalidArguments(Backend);
}

test "Vulkan texture lifecycle enforces active session and stale handles" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectLifecycleEnforcesActiveSessionAndStaleHandles(Backend);
}

test "Vulkan texture rejects wrong-thread calls" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectWrongThreadCallsRejected(Backend);
}

test "Vulkan texture render acquire release and resize generation" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectRenderAcquireReleaseAndResizeGeneration(Backend);
}

test "Vulkan owned texture supports readback" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectOwnedTextureReadback(Backend);
}

test "Vulkan borrowed texture renders into caller image" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try support.suppressLogs();
    defer support.restoreLogs();

    var borrowed = try BorrowedImage.create(128, 128);
    defer borrowed.deinit();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = borrowed.descriptor();
    var texture: ?*c.mln_texture_session = null;
    var missing_image_descriptor = descriptor;
    missing_image_descriptor.image = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_borrowed_texture_attach(map, &missing_image_descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_vulkan_borrowed_texture_attach(map, &descriptor, &texture));
    defer if (texture) |live_texture| {
        testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(live_texture)) catch @panic("texture destroy failed");
    };

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render_update(texture.?));

    var frame = Backend.Frame.empty(texture.?);
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_vulkan_owned_texture_acquire_frame(texture.?, &frame.vulkan));
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_texture_resize(texture.?, 64, 64, 1.0));

    var image_info = c.mln_texture_image_info_default();
    var pixel: [4]u8 = .{ 0, 0, 0, 0 };
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_texture_read_premultiplied_rgba8(texture.?, pixel[0..].ptr, pixel.len, &image_info));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture.?));
    texture = null;
}

test "Vulkan texture render emits observer events" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectRenderObserverEvents(Backend);
}

test "Vulkan texture still modes render requested still images" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    inline for (.{ c.MLN_MAP_MODE_STATIC, c.MLN_MAP_MODE_TILE }) |map_mode| {
        try common.expectStillModeStillImageRequest(Backend, map_mode);
    }
}

test "Vulkan texture detach leaves handle live but unusable for rendering" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectDetachLeavesHandleLiveButUnusable(Backend);
}
