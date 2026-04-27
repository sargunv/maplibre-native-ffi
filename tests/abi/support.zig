const std = @import("std");
const testing = std.testing;

pub const c = @cImport({
    @cInclude("maplibre_native_abi.h");
});

extern fn usleep(useconds: c_uint) c_int;

fn consumeLog(_: ?*anyopaque, _: u32, _: u32, _: i64, _: [*c]const u8) callconv(.c) u32 {
    return 1;
}

pub const style_json =
    \\{
    \\  "version": 8,
    \\  "name": "zig-abi-test",
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

pub fn createRuntime() !*c.mln_runtime {
    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
    return runtime orelse error.RuntimeCreateFailed;
}

pub fn createMap(runtime: *c.mln_runtime) !*c.mln_map {
    var map: ?*c.mln_map = null;
    var options = c.mln_map_options_default();
    options.width = 512;
    options.height = 512;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_create(runtime, &options, &map));
    return map orelse error.MapCreateFailed;
}

pub fn destroyRuntime(runtime: *c.mln_runtime) void {
    testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_destroy(runtime)) catch @panic("runtime destroy failed");
}

pub fn destroyMap(map: *c.mln_map) void {
    testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map)) catch @panic("map destroy failed");
}

pub fn suppressLogs() !void {
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(0));
    errdefer restoreLogs();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_callback(consumeLog, null));
}

pub fn restoreLogs() void {
    testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_clear_callback()) catch @panic("log clear failed");
    testing.expectEqual(c.MLN_STATUS_OK, c.mln_log_set_async_severity_mask(c.MLN_LOG_SEVERITY_MASK_DEFAULT)) catch @panic("log async mask restore failed");
}

pub fn drainEvents(map: *c.mln_map) !usize {
    var count: usize = 0;
    while (true) {
        var event = emptyEvent();
        var has_event = false;
        const status = c.mln_map_poll_event(map, &event, &has_event);
        try testing.expectEqual(c.MLN_STATUS_OK, status);
        if (!has_event) break;
        count += 1;
    }
    return count;
}

pub fn waitForEvent(runtime: *c.mln_runtime, map: *c.mln_map, event_type: u32) !bool {
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        while (true) {
            var event = emptyEvent();
            var has_event = false;
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_poll_event(map, &event, &has_event));
            if (!has_event) break;
            if (event.type == event_type) return true;
        }
        _ = usleep(1000);
    }
    return false;
}

pub fn emptyEvent() c.mln_map_event {
    return .{ .size = @sizeOf(c.mln_map_event), .type = c.MLN_MAP_EVENT_NONE, .code = 0, .message = [_:0]u8{0} ** 512 };
}

pub fn testCamera() c.mln_camera_options {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER | c.MLN_CAMERA_OPTION_ZOOM | c.MLN_CAMERA_OPTION_BEARING | c.MLN_CAMERA_OPTION_PITCH;
    camera.latitude = 37.7749;
    camera.longitude = -122.4194;
    camera.zoom = 11.0;
    camera.bearing = 12.0;
    camera.pitch = 30.0;
    return camera;
}
