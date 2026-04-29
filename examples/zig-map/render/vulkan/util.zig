const std = @import("std");

const c = @import("../../c.zig").c;
const types = @import("../../types.zig");

pub fn expectVk(result: c.VkResult) !void {
    if (result != c.VK_SUCCESS) {
        std.debug.print("Vulkan call failed: {d}\n", .{result});
        return types.AppError.BackendSetupFailed;
    }
}

pub fn expectVkOrSuboptimal(result: c.VkResult) !void {
    if (result != c.VK_SUCCESS and result != c.VK_SUBOPTIMAL_KHR) {
        std.debug.print("Vulkan call failed: {d}\n", .{result});
        return types.AppError.BackendSetupFailed;
    }
}

pub fn expectSdl(ok: bool) !void {
    if (!ok) {
        std.debug.print("SDL Vulkan call failed: {s}\n", .{std.mem.span(c.SDL_GetError())});
        return types.AppError.BackendSetupFailed;
    }
}
