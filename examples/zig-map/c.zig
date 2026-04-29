const builtin = @import("builtin");

pub const c = switch (builtin.os.tag) {
    .macos => @cImport({
        @cInclude("maplibre_native_abi.h");
        @cInclude("SDL3/SDL.h");
        @cInclude("SDL3/SDL_metal.h");
    }),
    .linux => @cImport({
        @cInclude("maplibre_native_abi.h");
        @cInclude("SDL3/SDL.h");
        @cInclude("SDL3/SDL_vulkan.h");
        @cInclude("vulkan/vulkan.h");
    }),
    else => @compileError("zig-map currently supports macOS Metal and Linux Vulkan"),
};
