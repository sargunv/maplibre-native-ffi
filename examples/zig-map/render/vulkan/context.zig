const std = @import("std");

const c = @import("../../c.zig").c;
const types = @import("../../types.zig");
const util = @import("util.zig");

pub const Context = struct {
    allocator: std.mem.Allocator,
    instance: c.VkInstance,
    surface: c.VkSurfaceKHR,
    physical_device: c.VkPhysicalDevice,
    device: c.VkDevice,
    queue: c.VkQueue,
    queue_family_index: u32,

    pub fn init(allocator: std.mem.Allocator, window: *c.SDL_Window) !Context {
        var self = Context{
            .allocator = allocator,
            .instance = null,
            .surface = null,
            .physical_device = null,
            .device = null,
            .queue = null,
            .queue_family_index = 0,
        };
        errdefer self.deinit();

        try self.createInstance();
        try util.expectSdl(c.SDL_Vulkan_CreateSurface(
            window,
            self.instance,
            null,
            &self.surface,
        ));
        try self.pickDevice();
        try self.createDevice();
        return self;
    }

    pub fn deinit(self: *Context) void {
        if (self.device != null) c.vkDestroyDevice(self.device, null);
        if (self.surface != null) {
            c.SDL_Vulkan_DestroySurface(self.instance, self.surface, null);
        }
        if (self.instance != null) c.vkDestroyInstance(self.instance, null);
    }

    pub fn waitIdle(self: *Context) void {
        if (self.device != null) _ = c.vkDeviceWaitIdle(self.device);
    }

    fn createInstance(self: *Context) !void {
        var extension_count: u32 = 0;
        const extensions = c.SDL_Vulkan_GetInstanceExtensions(&extension_count);
        if (extensions == null or extension_count == 0) {
            return types.AppError.BackendSetupFailed;
        }

        const app_info = c.VkApplicationInfo{
            .sType = c.VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = null,
            .pApplicationName = "zig-map",
            .applicationVersion = 1,
            .pEngineName = "zig-map",
            .engineVersion = 1,
            .apiVersion = c.VK_API_VERSION_1_0,
        };
        const create_info = c.VkInstanceCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = null,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = extensions,
        };
        try util.expectVk(c.vkCreateInstance(&create_info, null, &self.instance));
    }

    fn pickDevice(self: *Context) !void {
        var count: u32 = 0;
        try util.expectVk(c.vkEnumeratePhysicalDevices(self.instance, &count, null));
        if (count == 0) return types.AppError.BackendSetupFailed;
        const devices = try self.allocator.alloc(c.VkPhysicalDevice, count);
        defer self.allocator.free(devices);
        try util.expectVk(c.vkEnumeratePhysicalDevices(
            self.instance,
            &count,
            devices.ptr,
        ));

        for (devices) |device| {
            var family_count: u32 = 0;
            c.vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, null);
            const families = try self.allocator.alloc(
                c.VkQueueFamilyProperties,
                family_count,
            );
            defer self.allocator.free(families);
            c.vkGetPhysicalDeviceQueueFamilyProperties(
                device,
                &family_count,
                families.ptr,
            );
            for (families, 0..) |family, index| {
                if ((family.queueFlags & c.VK_QUEUE_GRAPHICS_BIT) == 0) continue;
                if (!c.SDL_Vulkan_GetPresentationSupport(
                    self.instance,
                    device,
                    @intCast(index),
                )) continue;
                self.physical_device = device;
                self.queue_family_index = @intCast(index);
                return;
            }
        }
        return types.AppError.BackendSetupFailed;
    }

    fn createDevice(self: *Context) !void {
        var priority: f32 = 1.0;
        const queue_info = c.VkDeviceQueueCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .queueFamilyIndex = self.queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        const extensions = [_][*:0]const u8{c.VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        var features = std.mem.zeroes(c.VkPhysicalDeviceFeatures);
        c.vkGetPhysicalDeviceFeatures(self.physical_device, &features);
        const create_info = c.VkDeviceCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue_info,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = null,
            .enabledExtensionCount = extensions.len,
            .ppEnabledExtensionNames = &extensions,
            .pEnabledFeatures = &features,
        };
        try util.expectVk(c.vkCreateDevice(
            self.physical_device,
            &create_info,
            null,
            &self.device,
        ));
        c.vkGetDeviceQueue(self.device, self.queue_family_index, 0, &self.queue);
    }
};
