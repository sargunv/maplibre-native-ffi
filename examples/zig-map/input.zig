const std = @import("std");

const c = @import("c.zig").c;
const diagnostics = @import("diagnostics.zig");
const types = @import("types.zig");

const DragMode = enum {
    none,
    pan,
    rotate,
    pitch,
};

pub const Result = struct {
    handled: bool = false,
    camera_changed: bool = false,
};

pub const Controller = struct {
    drag_mode: DragMode = .none,
    last_x: f64 = 0,
    last_y: f64 = 0,

    pub fn handleEvent(
        self: *Controller,
        event: *const c.SDL_Event,
        map: *c.mln_map,
        current_viewport: types.Viewport,
    ) !Result {
        return switch (event.type) {
            c.SDL_EVENT_MOUSE_BUTTON_DOWN => self.handleMouseButtonDown(event.button, map),
            c.SDL_EVENT_MOUSE_BUTTON_UP => self.handleMouseButtonUp(event.button),
            c.SDL_EVENT_MOUSE_MOTION => self.handleMouseMotion(event.motion, map),
            c.SDL_EVENT_MOUSE_WHEEL => handleMouseWheel(event.wheel, map),
            c.SDL_EVENT_KEY_DOWN => handleKeyDown(event.key, map, current_viewport),
            else => .{},
        };
    }

    fn handleMouseButtonDown(
        self: *Controller,
        button: c.SDL_MouseButtonEvent,
        map: *c.mln_map,
    ) !Result {
        self.last_x = button.x;
        self.last_y = button.y;

        const mode = dragModeForButton(button.button);
        if (mode == .none) return .{};

        try expectCameraStatus(c.mln_map_cancel_transitions(map), "cancel camera transitions failed");
        self.drag_mode = mode;
        return .{ .handled = true };
    }

    fn handleMouseButtonUp(self: *Controller, button: c.SDL_MouseButtonEvent) Result {
        if (button.button != c.SDL_BUTTON_LEFT and button.button != c.SDL_BUTTON_RIGHT) {
            return .{};
        }
        self.drag_mode = .none;
        self.last_x = button.x;
        self.last_y = button.y;
        return .{ .handled = true };
    }

    fn handleMouseMotion(
        self: *Controller,
        motion: c.SDL_MouseMotionEvent,
        map: *c.mln_map,
    ) !Result {
        const x: f64 = motion.x;
        const y: f64 = motion.y;
        defer {
            self.last_x = x;
            self.last_y = y;
        }

        switch (self.drag_mode) {
            .none => return .{},
            .pan => {
                const dx = x - self.last_x;
                const dy = y - self.last_y;
                if (dx == 0 and dy == 0) return .{ .handled = true };
                try expectCameraStatus(c.mln_map_move_by(map, dx, dy), "camera pan failed");
            },
            .rotate => {
                const dx = x - self.last_x;
                const dy = y - self.last_y;
                if (dx == 0 and dy == 0) return .{ .handled = true };
                try adjustBearing(map, dx * 0.5);
                try expectCameraStatus(c.mln_map_pitch_by(map, dy / 2.0), "camera pitch failed");
            },
            .pitch => {
                const dy = y - self.last_y;
                if (dy == 0) return .{ .handled = true };
                try expectCameraStatus(c.mln_map_pitch_by(map, dy / 2.0), "camera pitch failed");
            },
        }
        return .{ .handled = true, .camera_changed = true };
    }
};

pub fn logControls() void {
    std.debug.print(
        \\Controls:
        \\  left drag: pan
        \\  right drag or Ctrl+left drag: rotate with X, pitch with Y
        \\  scroll: zoom at cursor
        \\  arrows or WASD: pan
        \\  + / -: zoom at center
        \\  Q / E: rotate
        \\  PageUp / PageDown or [ / ]: pitch
        \\  0: reset pitch and bearing
        \\
    , .{});
}

fn handleMouseWheel(wheel: c.SDL_MouseWheelEvent, map: *c.mln_map) !Result {
    // Use SDL's OS-adjusted value directly. Undoing FLIPPED converts natural
    // scrolling back to physical wheel direction, which is backwards on Wayland.
    const delta: f64 = -wheel.y;
    if (delta == 0) return .{ .handled = true };

    const anchor = point(wheel.mouse_x, wheel.mouse_y);
    const scale = std.math.pow(f64, 2.0, delta * 0.25);
    try expectCameraStatus(c.mln_map_scale_by(map, scale, &anchor), "camera zoom failed");
    return .{ .handled = true, .camera_changed = true };
}

fn handleKeyDown(
    key: c.SDL_KeyboardEvent,
    map: *c.mln_map,
    current_viewport: types.Viewport,
) !Result {
    const pan_step = 120.0;
    const zoom_step = 1.25;
    const bearing_step = 10.0;
    const pitch_step = 5.0;
    const center = point(
        @as(f64, @floatFromInt(current_viewport.logical_width)) / 2.0,
        @as(f64, @floatFromInt(current_viewport.logical_height)) / 2.0,
    );

    switch (key.scancode) {
        scancode(c.SDL_SCANCODE_LEFT), scancode(c.SDL_SCANCODE_A) => {
            try expectCameraStatus(c.mln_map_move_by(map, pan_step, 0), "keyboard pan failed");
        },
        scancode(c.SDL_SCANCODE_RIGHT), scancode(c.SDL_SCANCODE_D) => {
            try expectCameraStatus(c.mln_map_move_by(map, -pan_step, 0), "keyboard pan failed");
        },
        scancode(c.SDL_SCANCODE_UP), scancode(c.SDL_SCANCODE_W) => {
            try expectCameraStatus(c.mln_map_move_by(map, 0, pan_step), "keyboard pan failed");
        },
        scancode(c.SDL_SCANCODE_DOWN), scancode(c.SDL_SCANCODE_S) => {
            try expectCameraStatus(c.mln_map_move_by(map, 0, -pan_step), "keyboard pan failed");
        },
        scancode(c.SDL_SCANCODE_EQUALS), scancode(c.SDL_SCANCODE_KP_PLUS) => {
            try expectCameraStatus(c.mln_map_scale_by(map, zoom_step, &center), "keyboard zoom failed");
        },
        scancode(c.SDL_SCANCODE_MINUS), scancode(c.SDL_SCANCODE_KP_MINUS) => {
            try expectCameraStatus(c.mln_map_scale_by(map, 1.0 / zoom_step, &center), "keyboard zoom failed");
        },
        scancode(c.SDL_SCANCODE_Q) => try adjustBearing(map, -bearing_step),
        scancode(c.SDL_SCANCODE_E) => try adjustBearing(map, bearing_step),
        scancode(c.SDL_SCANCODE_PAGEUP), scancode(c.SDL_SCANCODE_RIGHTBRACKET) => {
            try adjustPitch(map, pitch_step);
        },
        scancode(c.SDL_SCANCODE_PAGEDOWN), scancode(c.SDL_SCANCODE_LEFTBRACKET) => {
            try adjustPitch(map, -pitch_step);
        },
        scancode(c.SDL_SCANCODE_0) => try resetPitchAndBearing(map),
        else => return .{},
    }

    return .{ .handled = true, .camera_changed = true };
}

fn dragModeForButton(button: u8) DragMode {
    if (button == c.SDL_BUTTON_RIGHT) return .rotate;
    if (button != c.SDL_BUTTON_LEFT) return .none;

    const mod_state = @as(c_uint, c.SDL_GetModState());
    if ((mod_state & c.SDL_KMOD_CTRL) != 0) return .rotate;
    return .pan;
}

fn adjustBearing(map: *c.mln_map, delta: f64) !void {
    var camera = try currentCamera(map);
    camera.fields = c.MLN_CAMERA_OPTION_BEARING;
    camera.bearing += delta;
    try expectCameraStatus(c.mln_map_jump_to(map, &camera), "keyboard rotate failed");
}

fn adjustPitch(map: *c.mln_map, delta: f64) !void {
    var camera = try currentCamera(map);
    camera.fields = c.MLN_CAMERA_OPTION_PITCH;
    camera.pitch = clamp(camera.pitch + delta, 0.0, 60.0);
    try expectCameraStatus(c.mln_map_jump_to(map, &camera), "keyboard pitch failed");
}

fn resetPitchAndBearing(map: *c.mln_map) !void {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_BEARING | c.MLN_CAMERA_OPTION_PITCH;
    camera.bearing = 0;
    camera.pitch = 0;
    try expectCameraStatus(c.mln_map_jump_to(map, &camera), "camera reset failed");
}

fn currentCamera(map: *c.mln_map) !c.mln_camera_options {
    var camera = c.mln_camera_options_default();
    try expectCameraStatus(c.mln_map_get_camera(map, &camera), "camera snapshot failed");
    return camera;
}

fn expectCameraStatus(status: c.mln_status, message: []const u8) !void {
    if (status == c.MLN_STATUS_OK) return;
    diagnostics.logAbiError(message);
    return types.AppError.CameraCommandFailed;
}

fn point(x: f64, y: f64) c.mln_screen_point {
    return .{ .x = x, .y = y };
}

fn scancode(value: c_int) c.SDL_Scancode {
    return @intCast(value);
}

fn clamp(value: f64, min: f64, max: f64) f64 {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
