const std = @import("std");
const c = @import("c.zig").c;
const types = @import("types.zig");

pub const window_width = 960;
pub const window_height = 640;

pub fn get(window: *c.SDL_Window) types.Viewport {
    var logical_width: c_int = window_width;
    var logical_height: c_int = window_height;
    var physical_width: c_int = window_width;
    var physical_height: c_int = window_height;
    _ = c.SDL_GetWindowSize(window, &logical_width, &logical_height);
    _ = c.SDL_GetWindowSizeInPixels(window, &physical_width, &physical_height);

    const safe_logical_width = @max(logical_width, 1);
    const safe_logical_height = @max(logical_height, 1);
    const safe_physical_width = @max(physical_width, 1);
    const safe_physical_height = @max(physical_height, 1);
    const density_x = density(safe_physical_width, safe_logical_width);
    const density_y = density(safe_physical_height, safe_logical_height);
    const size_density = @max(density_x, density_y);
    const window_density = c.SDL_GetWindowPixelDensity(window);
    const display_scale = c.SDL_GetWindowDisplayScale(window);
    const fallback_scale = if (window_density > 0)
        @as(f64, @floatCast(window_density))
    else
        size_density;
    const content_scale = if (display_scale > 0)
        @as(f64, @floatCast(display_scale))
    else
        fallback_scale;
    const scaled_logical_width = scaledLogicalSize(
        safe_physical_width,
        content_scale,
    );
    const scaled_logical_height = scaledLogicalSize(
        safe_physical_height,
        content_scale,
    );

    return .{
        .logical_width = scaled_logical_width,
        .logical_height = scaled_logical_height,
        .physical_width = @intCast(safe_physical_width),
        .physical_height = @intCast(safe_physical_height),
        .scale_factor = content_scale,
    };
}

pub fn log(label: []const u8, value: types.Viewport) void {
    std.debug.print(
        "{s}: logical={d}x{d} physical={d}x{d} scale={d:.2}\n",
        .{
            label,
            value.logical_width,
            value.logical_height,
            value.physical_width,
            value.physical_height,
            value.scale_factor,
        },
    );
}

fn scaledLogicalSize(physical_size: c_int, content_scale: f64) u32 {
    const scaled = @as(f64, @floatFromInt(physical_size)) / content_scale;
    return @max(@as(u32, @intFromFloat(@ceil(scaled))), 1);
}

fn density(physical_size: c_int, logical_size: c_int) f64 {
    return @as(f64, @floatFromInt(physical_size)) /
        @as(f64, @floatFromInt(logical_size));
}
