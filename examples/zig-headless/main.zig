const std = @import("std");

const c = @cImport({
    @cInclude("maplibre_native_abi.h");
});

pub fn main() !void {
    std.debug.print("ABI version: {d}\n", .{c.mln_abi_version()});

    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    if (c.mln_runtime_create(&options, &runtime) != c.MLN_STATUS_OK or runtime == null) {
        std.debug.print("runtime create failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.RuntimeCreateFailed;
    }
    const runtime_handle = runtime.?;
    errdefer _ = c.mln_runtime_destroy(runtime_handle);

    var map: ?*c.mln_map = null;
    var map_options = c.mln_map_options_default();
    map_options.width = 512;
    map_options.height = 512;
    if (c.mln_map_create(runtime_handle, &map_options, &map) != c.MLN_STATUS_OK or map == null) {
        std.debug.print("map create failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.MapCreateFailed;
    }
    const map_handle = map.?;
    errdefer _ = c.mln_map_destroy(map_handle);

    const style_json =
        \\{
        \\  "version": 8,
        \\  "name": "zig-headless-smoke",
        \\  "sources": {
        \\    "point": {
        \\      "type": "geojson",
        \\      "data": {
        \\        "type": "FeatureCollection",
        \\        "features": [
        \\          {"type":"Feature","geometry":{"type":"Point","coordinates":[-122.4194,37.7749]},"properties":{}}
        \\        ]
        \\      }
        \\    }
        \\  },
        \\  "layers": [
        \\    {"id":"background","type":"background","paint":{"background-color":"#d8f1ff"}},
        \\    {"id":"point-circle","type":"circle","source":"point","paint":{"circle-color":"#f97316","circle-radius":12}}
        \\  ]
        \\}
    ;

    if (c.mln_map_set_style_json(map_handle, style_json) != c.MLN_STATUS_OK) {
        std.debug.print("style load failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.StyleLoadFailed;
    }

    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER | c.MLN_CAMERA_OPTION_ZOOM | c.MLN_CAMERA_OPTION_BEARING | c.MLN_CAMERA_OPTION_PITCH;
    camera.latitude = 37.7749;
    camera.longitude = -122.4194;
    camera.zoom = 11.0;
    camera.bearing = 12.0;
    camera.pitch = 30.0;
    if (c.mln_map_jump_to(map_handle, &camera) != c.MLN_STATUS_OK) {
        std.debug.print("camera jump failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.CameraJumpFailed;
    }

    if (c.mln_map_move_by(map_handle, 4, -2) != c.MLN_STATUS_OK) {
        return error.CameraMoveFailed;
    }

    var snapshot = c.mln_camera_options_default();
    if (c.mln_map_get_camera(map_handle, &snapshot) != c.MLN_STATUS_OK) {
        return error.CameraSnapshotFailed;
    }

    for (0..8) |_| {
        _ = c.mln_runtime_run_once(runtime_handle);
    }

    var saw_event = false;
    while (true) {
        var event: c.mln_map_event = .{ .size = @sizeOf(c.mln_map_event), .type = c.MLN_MAP_EVENT_NONE, .code = 0, .message = [_:0]u8{0} ** 512 };
        var has_event = false;
        const status = c.mln_map_poll_event(map_handle, &event, &has_event);
        if (status != c.MLN_STATUS_OK) return error.EventPollFailed;
        if (!has_event) break;
        saw_event = true;
        std.debug.print("event type={d} code={d} message={s}\n", .{ event.type, event.code, std.mem.sliceTo(&event.message, 0) });
    }

    if (!saw_event) {
        return error.NoMapEvents;
    }

    if (c.mln_map_destroy(map_handle) != c.MLN_STATUS_OK) {
        std.debug.print("map destroy failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.MapDestroyFailed;
    }

    if (c.mln_runtime_destroy(runtime_handle) != c.MLN_STATUS_OK) {
        std.debug.print("runtime destroy failed: {s}\n", .{std.mem.span(c.mln_thread_last_error_message())});
        return error.RuntimeDestroyFailed;
    }

    std.debug.print("headless map lifecycle smoke passed\n", .{});
}
