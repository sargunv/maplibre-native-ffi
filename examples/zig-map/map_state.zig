const c = @import("c.zig").c;
const diagnostics = @import("diagnostics.zig");
const render_target = @import("render_target.zig");
const types = @import("types.zig");

pub const MapState = struct {
    runtime: *c.mln_runtime,
    map: *c.mln_map,
    target: render_target.Session,

    pub fn init(viewport: types.Viewport, backend: anytype) !MapState {
        var runtime: ?*c.mln_runtime = null;
        var runtime_options = c.mln_runtime_options_default();
        runtime_options.cache_path = ":memory:";
        if (c.mln_runtime_create(&runtime_options, &runtime) !=
            c.MLN_STATUS_OK or runtime == null)
        {
            diagnostics.logAbiError("runtime create failed");
            return types.AppError.RuntimeCreateFailed;
        }
        errdefer _ = c.mln_runtime_destroy(runtime.?);

        var map: ?*c.mln_map = null;
        var map_options = c.mln_map_options_default();
        map_options.width = viewport.logical_width;
        map_options.height = viewport.logical_height;
        map_options.scale_factor = viewport.scale_factor;
        map_options.map_mode = c.MLN_MAP_MODE_CONTINUOUS;
        if (c.mln_map_create(runtime.?, &map_options, &map) != c.MLN_STATUS_OK or map == null) {
            diagnostics.logAbiError("map create failed");
            return types.AppError.MapCreateFailed;
        }
        errdefer _ = c.mln_map_destroy(map.?);

        try loadStyle(map.?);
        try setCamera(map.?);

        var target = try backend.attachRenderTarget(map.?, viewport);
        errdefer target.deinit();
        return .{ .runtime = runtime.?, .map = map.?, .target = target };
    }

    pub fn deinit(self: *MapState) void {
        self.target.deinit();
        _ = c.mln_map_destroy(self.map);
        _ = c.mln_runtime_destroy(self.runtime);
    }

    pub fn resize(self: *MapState, viewport: types.Viewport) !void {
        try self.target.resize(viewport);
    }

    pub fn resizeWithReattachedTarget(
        self: *MapState,
        viewport: types.Viewport,
        backend: anytype,
    ) !void {
        self.target.deinit();
        try backend.resize(viewport);
        self.target = try backend.attachRenderTarget(self.map, viewport);
    }
};

pub fn drainEvents(runtime: *c.mln_runtime, map: *c.mln_map) !bool {
    var render_update_available = false;
    while (true) {
        var event: c.mln_runtime_event = .{
            .size = @sizeOf(c.mln_runtime_event),
            .type = 0,
            .source_type = c.MLN_RUNTIME_EVENT_SOURCE_RUNTIME,
            .source = null,
            .code = 0,
            .payload_type = c.MLN_RUNTIME_EVENT_PAYLOAD_NONE,
            .payload = null,
            .payload_size = 0,
            .message = null,
            .message_size = 0,
        };
        var has_event = false;
        if (c.mln_runtime_poll_event(runtime, &event, &has_event) != c.MLN_STATUS_OK) {
            diagnostics.logAbiError("event poll failed");
            return types.AppError.TextureRenderFailed;
        }
        if (!has_event) return render_update_available;
        if (event.source_type == c.MLN_RUNTIME_EVENT_SOURCE_MAP and
            event.source == @as(?*anyopaque, @ptrCast(map)) and
            event.type == c.MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE) render_update_available = true;
    }
}

fn loadStyle(map: *c.mln_map) !void {
    if (c.mln_map_set_style_url(
        map,
        "https://tiles.openfreemap.org/styles/bright",
    ) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("style load failed");
        return types.AppError.StyleLoadFailed;
    }
}

fn setCamera(map: *c.mln_map) !void {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER |
        c.MLN_CAMERA_OPTION_ZOOM |
        c.MLN_CAMERA_OPTION_BEARING |
        c.MLN_CAMERA_OPTION_PITCH;
    camera.latitude = 37.7749;
    camera.longitude = -122.4194;
    camera.zoom = 13.0;
    camera.bearing = 12.0;
    camera.pitch = 30.0;
    if (c.mln_map_jump_to(map, &camera) != c.MLN_STATUS_OK) {
        diagnostics.logAbiError("camera jump failed");
        return types.AppError.CameraJumpFailed;
    }
}
