const builtin = @import("builtin");

pub const Backend = switch (builtin.os.tag) {
    .macos => @import("metal/mod.zig").MetalBackend,
    .linux => @import("vulkan/mod.zig").VulkanBackend,
    else => @compileError("zig-map currently supports macOS Metal and Linux Vulkan"),
};
