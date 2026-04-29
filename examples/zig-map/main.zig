const std = @import("std");
const builtin = @import("builtin");
const objc = if (builtin.os.tag == .macos) @import("objc") else struct {};

const c = @import("c.zig").c;
const diagnostics = @import("diagnostics.zig");
const input = @import("input.zig");
const map_state = @import("map_state.zig");
const render = @import("render/mod.zig");
const types = @import("types.zig");
const viewport = @import("viewport.zig");

const Backend = render.Backend;

pub fn main(init_args: std.process.Init) !void {
    _ = init_args;

    _ = c.mln_log_set_callback(diagnostics.logCallback, null);
    defer _ = c.mln_log_clear_callback();

    if (!c.SDL_Init(c.SDL_INIT_VIDEO)) {
        std.debug.print("SDL_Init failed: {s}\n", .{std.mem.span(c.SDL_GetError())});
        return types.AppError.SdlInitFailed;
    }
    defer c.SDL_Quit();

    const window_flags = Backend.window_flags |
        c.SDL_WINDOW_RESIZABLE |
        c.SDL_WINDOW_HIGH_PIXEL_DENSITY;
    const window = c.SDL_CreateWindow(
        "MapLibre SDL3 Map",
        viewport.window_width,
        viewport.window_height,
        window_flags,
    );
    if (window == null) {
        std.debug.print("SDL_CreateWindow failed: {s}\n", .{std.mem.span(c.SDL_GetError())});
        return types.AppError.WindowCreateFailed;
    }
    defer c.SDL_DestroyWindow(window);

    const window_handle = window.?;
    var current_viewport = viewport.get(window_handle);
    viewport.log("initial viewport", current_viewport);
    input.logControls();

    var gpa = std.heap.DebugAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var backend = try Backend.init(allocator, window_handle, current_viewport);
    defer backend.deinit();

    var map = try map_state.MapState.init(current_viewport, &backend);
    defer map.deinit();

    var running = true;
    var has_presented_frame = false;
    var render_pending = true;
    var input_controller = input.Controller{};
    while (running) {
        const pool = if (builtin.os.tag == .macos) objc.AutoreleasePool.init() else {};
        defer if (builtin.os.tag == .macos) pool.deinit();

        var did_work = false;
        var event: c.SDL_Event = undefined;
        while (c.SDL_PollEvent(&event)) {
            did_work = true;
            switch (event.type) {
                c.SDL_EVENT_QUIT => running = false,
                c.SDL_EVENT_WINDOW_CLOSE_REQUESTED => running = false,
                c.SDL_EVENT_WINDOW_RESIZED,
                c.SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
                c.SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
                => {
                    current_viewport = viewport.get(window_handle);
                    viewport.log("resized viewport", current_viewport);
                    try backend.resize(current_viewport);
                    try map.resize(current_viewport);
                    render_pending = true;
                },
                else => {
                    const input_result = try input_controller.handleEvent(
                        &event,
                        map.map,
                        current_viewport,
                    );
                    render_pending = render_pending or input_result.camera_changed;
                },
            }
        }

        _ = c.mln_runtime_run_once(map.runtime);
        const render_invalidated = try map_state.drainEvents(map.map);
        render_pending = render_pending or render_invalidated;
        did_work = did_work or render_invalidated;

        try backend.finishFrame();

        if (render_pending) {
            const render_status = c.mln_texture_render(map.texture);
            if (render_status == c.MLN_STATUS_OK) {
                render_pending = false;
                did_work = true;
                if (try backend.draw(map.texture, current_viewport)) {
                    has_presented_frame = true;
                }
            } else if (render_status != c.MLN_STATUS_INVALID_STATE) {
                diagnostics.logAbiError("texture render failed");
                return types.AppError.TextureRenderFailed;
            }
        }

        if (!did_work) c.SDL_Delay(if (has_presented_frame) 8 else 1);
    }
}
