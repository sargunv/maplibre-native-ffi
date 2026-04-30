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
    pub const descriptor_size = @sizeOf(c.mln_vulkan_texture_descriptor);

    pub const AttachContext = struct {
        instance: vk.VkInstance,
        physical_device: vk.VkPhysicalDevice,
        device: vk.VkDevice,
        queue: vk.VkQueue,
        queue_family_index: u32,

        pub fn init() !AttachContext {
            var app_info = std.mem.zeroes(vk.VkApplicationInfo);
            app_info.sType = vk.VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pApplicationName = "maplibre-native-c-tests";
            app_info.applicationVersion = 1;
            app_info.pEngineName = "maplibre-native-c-tests";
            app_info.engineVersion = 1;
            app_info.apiVersion = vk.VK_API_VERSION_1_0;

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

        pub fn descriptor(self: *const AttachContext) c.mln_vulkan_texture_descriptor {
            var value = c.mln_vulkan_texture_descriptor_default();
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
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_vulkan_texture_attach(map, &texture_descriptor, &texture));
            return .{ .texture = texture orelse return error.TextureCreateFailed, .context = context };
        }

        pub fn descriptor(self: *const Fixture) c.mln_vulkan_texture_descriptor {
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
            return c.mln_vulkan_texture_acquire_frame(frame.texture, &frame.vulkan);
        }

        pub fn release(_: *Fixture, frame: *const Frame) c.mln_status {
            return c.mln_vulkan_texture_release_frame(frame.texture, &frame.vulkan);
        }
    };

    pub const Frame = struct {
        texture: *c.mln_texture_session,
        vulkan: c.mln_vulkan_texture_frame,

        pub fn empty(texture: *c.mln_texture_session) Frame {
            return .{
                .texture = texture,
                .vulkan = .{
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
            self.vulkan.size = @sizeOf(c.mln_vulkan_texture_frame);
        }
    };

    pub fn attach(map: ?*c.mln_map, descriptor: ?*const c.mln_vulkan_texture_descriptor, out_texture: ?*?*c.mln_texture_session) c.mln_status {
        return c.mln_vulkan_texture_attach(map, descriptor, out_texture);
    }

    pub fn clearRequiredHandle(descriptor: *c.mln_vulkan_texture_descriptor) void {
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

test "Vulkan texture unsupported backend validates arguments" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_vulkan_texture_descriptor_default();
    descriptor.instance = @ptrFromInt(1);
    descriptor.physical_device = @ptrFromInt(1);
    descriptor.device = @ptrFromInt(1);
    descriptor.graphics_queue = @ptrFromInt(1);

    var texture: ?*c.mln_texture_session = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &descriptor, &texture));
    try testing.expect(texture != null);

    texture = null;
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_vulkan_texture_attach(map, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);
}

fn expectVk(result: vk.VkResult) !void {
    try testing.expectEqual(vk.VK_SUCCESS, result);
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

test "Vulkan texture detach leaves handle live but unusable for rendering" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;
    try common.expectDetachLeavesHandleLiveButUnusable(Backend);
}
