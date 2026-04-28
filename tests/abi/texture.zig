const std = @import("std");
const builtin = @import("builtin");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const vk = if (builtin.os.tag == .linux) @cImport({
    @cInclude("vulkan/vulkan.h");
}) else struct {};

const ThreadCall = enum { resize, render, acquire, release, detach, destroy };

const VulkanContext = if (builtin.os.tag == .linux) struct {
    instance: vk.VkInstance,
    physical_device: vk.VkPhysicalDevice,
    device: vk.VkDevice,
    queue: vk.VkQueue,
    queue_family_index: u32,

    fn init() !VulkanContext {
        var app_info = std.mem.zeroes(vk.VkApplicationInfo);
        app_info.sType = vk.VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "maplibre-native-ffi-tests";
        app_info.applicationVersion = 1;
        app_info.pEngineName = "maplibre-native-ffi-tests";
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

    fn deinit(self: *VulkanContext) void {
        _ = vk.vkDeviceWaitIdle(self.device);
        vk.vkDestroyDevice(self.device, null);
        vk.vkDestroyInstance(self.instance, null);
    }

    fn descriptor(self: *const VulkanContext) c.mln_vulkan_texture_descriptor {
        var value = c.mln_vulkan_texture_descriptor_default();
        value.instance = self.instance;
        value.physical_device = self.physical_device;
        value.device = self.device;
        value.graphics_queue = self.queue;
        value.graphics_queue_family_index = self.queue_family_index;
        return value;
    }
} else struct {};

const TextureFixture = struct {
    texture: *c.mln_texture_session,
    vk_context: if (builtin.os.tag == .linux) ?VulkanContext else void,

    fn create(map: *c.mln_map) !TextureFixture {
        if (builtin.os.tag != .linux) return error.SkipZigTest;

        var vk_context = try VulkanContext.init();
        errdefer vk_context.deinit();

        var descriptor = vk_context.descriptor();
        descriptor.width = 256;
        descriptor.height = 256;

        var texture: ?*c.mln_texture_session = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_vulkan_texture_attach(map, &descriptor, &texture));
        return .{ .texture = texture orelse return error.TextureCreateFailed, .vk_context = vk_context };
    }

    fn destroy(self: *TextureFixture) void {
        testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(self.texture)) catch @panic("texture destroy failed");
        if (builtin.os.tag == .linux) {
            if (self.vk_context) |*context| context.deinit();
            self.vk_context = null;
        }
    }

    fn acquire(self: *TextureFixture, frame: *Frame) c.mln_status {
        _ = self;
        return c.mln_vulkan_texture_acquire_frame(frame.texture, &frame.vulkan);
    }

    fn release(self: *TextureFixture, frame: *const Frame) c.mln_status {
        _ = self;
        return c.mln_vulkan_texture_release_frame(frame.texture, &frame.vulkan);
    }
};

const Frame = struct {
    texture: *c.mln_texture_session,
    vulkan: c.mln_vulkan_texture_frame,

    fn empty(texture: *c.mln_texture_session) Frame {
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
};

const ThreadCallArgs = struct {
    fixture: *TextureFixture,
    frame: *Frame,
    call: ThreadCall,
    out_status: *c.mln_status,
};

fn callTextureOnThread(args: ThreadCallArgs) void {
    args.out_status.* = switch (args.call) {
        .resize => c.mln_texture_resize(args.fixture.texture, 128, 128, 1.0),
        .render => c.mln_texture_render(args.fixture.texture),
        .acquire => args.fixture.acquire(args.frame),
        .release => args.fixture.release(args.frame),
        .detach => c.mln_texture_detach(args.fixture.texture),
        .destroy => c.mln_texture_destroy(args.fixture.texture),
    };
}

fn renderTextureOnThread(texture: *c.mln_texture_session, out_status: *c.mln_status) void {
    out_status.* = c.mln_texture_render(texture);
}

fn expectVk(result: vk.VkResult) !void {
    try testing.expectEqual(vk.VK_SUCCESS, result);
}

test "texture descriptors expose defaults" {
    const metal = c.mln_metal_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_metal_texture_descriptor)), metal.size);
    try testing.expect(metal.width > 0);
    try testing.expect(metal.height > 0);
    try testing.expect(metal.scale_factor > 0);
    try testing.expect(metal.device == null);

    const vulkan = c.mln_vulkan_texture_descriptor_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_vulkan_texture_descriptor)), vulkan.size);
    try testing.expect(vulkan.width > 0);
    try testing.expect(vulkan.height > 0);
    try testing.expect(vulkan.scale_factor > 0);
    try testing.expect(vulkan.instance == null);
    try testing.expect(vulkan.physical_device == null);
    try testing.expect(vulkan.device == null);
    try testing.expect(vulkan.graphics_queue == null);
}

test "texture attach rejects invalid arguments" {
    if (builtin.os.tag != .linux) return error.SkipZigTest;

    var context = try VulkanContext.init();
    defer context.deinit();

    var texture: ?*c.mln_texture_session = null;
    var descriptor = context.descriptor();

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(null, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, null, &texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &descriptor, null));

    texture = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &descriptor, &texture));

    texture = null;
    var small_descriptor = context.descriptor();
    small_descriptor.size = @sizeOf(c.mln_vulkan_texture_descriptor) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &small_descriptor, &texture));

    var invalid_descriptor = context.descriptor();
    invalid_descriptor.width = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = context.descriptor();
    invalid_descriptor.height = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = context.descriptor();
    invalid_descriptor.scale_factor = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &invalid_descriptor, &texture));

    invalid_descriptor = context.descriptor();
    invalid_descriptor.device = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_vulkan_texture_attach(map, &invalid_descriptor, &texture));

    var scaled_descriptor = context.descriptor();
    scaled_descriptor.scale_factor = 2.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_vulkan_texture_attach(map, &scaled_descriptor, &texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(texture.?));
}

test "texture lifecycle enforces active session and stale handles" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var fixture = try TextureFixture.create(map);
    var second: ?*c.mln_texture_session = null;
    var descriptor = fixture.vk_context.?.descriptor();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_vulkan_texture_attach(map, &descriptor, &second));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), second);

    fixture.destroy();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_destroy(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_texture_render(fixture.texture));

    var replacement = try TextureFixture.create(map);
    replacement.destroy();
}

test "texture rejects wrong-thread calls" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try TextureFixture.create(map);
    defer fixture.destroy();

    var status: c.mln_status = c.MLN_STATUS_OK;
    const thread = try std.Thread.spawn(.{}, renderTextureOnThread, .{ fixture.texture, &status });
    thread.join();

    try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);

    var frame = Frame.empty(fixture.texture);
    inline for (.{ ThreadCall.resize, ThreadCall.acquire, ThreadCall.detach, ThreadCall.destroy }) |call| {
        status = c.MLN_STATUS_OK;
        const call_thread = try std.Thread.spawn(.{}, callTextureOnThread, .{ThreadCallArgs{ .fixture = &fixture, .frame = &frame, .call = call, .out_status = &status }});
        call_thread.join();
        try testing.expectEqual(c.MLN_STATUS_WRONG_THREAD, status);
    }
}

test "texture render acquire release and resize generation" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try TextureFixture.create(map);
    defer fixture.destroy();

    var frame = Frame.empty(fixture.texture);
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.acquire(&frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_json(map, support.style_json));
    _ = try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_RENDER_INVALIDATED);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_OK, fixture.acquire(&frame));
    try testing.expect(frame.vulkan.image != null);
    try testing.expect(frame.vulkan.image_view != null);
    try testing.expect(frame.vulkan.device != null);
    try testing.expectEqual(@as(u32, vk.VK_FORMAT_R8G8B8A8_UNORM), frame.vulkan.format);
    try testing.expectEqual(@as(u32, vk.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), frame.vulkan.layout);
    try testing.expectEqual(@as(u32, 256), frame.vulkan.width);
    try testing.expectEqual(@as(u32, 256), frame.vulkan.height);
    try testing.expectEqual(@as(u64, 1), frame.vulkan.generation);
    try testing.expect(frame.vulkan.frame_id != 0);

    var stale_same_generation = frame;
    stale_same_generation.vulkan.frame_id += 1;

    var second_frame = frame;
    second_frame.vulkan.image = null;
    second_frame.vulkan.image_view = null;
    second_frame.vulkan.device = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.acquire(&second_frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_resize(fixture.texture, 128, 128, 1.0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_detach(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_destroy(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, fixture.release(&stale_same_generation));

    try testing.expectEqual(c.MLN_STATUS_OK, fixture.release(&frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.release(&frame));

    const old_frame = frame;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_resize(fixture.texture, 128, 64, 2.0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, fixture.release(&old_frame));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_render(fixture.texture));
    frame.vulkan.size = @sizeOf(c.mln_vulkan_texture_frame);
    try testing.expectEqual(c.MLN_STATUS_OK, fixture.acquire(&frame));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, fixture.release(&old_frame));
    try testing.expectEqual(@as(u32, vk.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), frame.vulkan.layout);
    try testing.expectEqual(@as(u32, 256), frame.vulkan.width);
    try testing.expectEqual(@as(u32, 128), frame.vulkan.height);
    try testing.expectEqual(@as(f64, 2.0), frame.vulkan.scale_factor);
    try testing.expectEqual(@as(u64, 2), frame.vulkan.generation);
    try testing.expectEqual(c.MLN_STATUS_OK, fixture.release(&frame));
}

test "texture detach leaves handle live but unusable for rendering" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);
    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    var fixture = try TextureFixture.create(map);
    defer if (fixture.vk_context) |*context| context.deinit();

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_detach(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_render(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_texture_detach(fixture.texture));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(fixture.texture));
    fixture.vk_context = null;
}
