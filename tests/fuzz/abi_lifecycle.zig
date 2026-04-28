const std = @import("std");
const testing = std.testing;

const c = @cImport({
    @cInclude("maplibre_native_abi.h");
});

const valid_style_json =
    \\{
    \\  "version": 8,
    \\  "name": "zig-fuzz-test",
    \\  "sources": {},
    \\  "layers": [
    \\    {"id":"background","type":"background","paint":{"background-color":"#d8f1ff"}}
    \\  ]
    \\}
;

const corpus = [_][]const u8{
    "",
    "{}",
    valid_style_json,
    "not json",
};

test "ABI lifecycle fuzz" {
    try testing.fuzz({}, fuzzAbiLifecycle, .{ .corpus = &corpus });
}

fn fuzzAbiLifecycle(_: void, smith: *testing.Smith) !void {
    var runtime: ?*c.mln_runtime = null;
    var map: ?*c.mln_map = null;

    const operation_count = smith.valueRangeAtMost(u8, 1, 32);
    for (0..operation_count) |_| {
        switch (smith.valueRangeAtMost(u8, 0, 13)) {
            0 => maybeCreateRuntime(smith, &runtime),
            1 => maybeCreateMap(smith, runtime, &map),
            2 => {
                if (runtime) |handle| _ = c.mln_runtime_run_once(handle);
            },
            3 => fuzzStyleJson(smith, map),
            4 => fuzzCamera(smith, map),
            5 => {
                if (map) |handle| _ = c.mln_map_move_by(handle, smallDouble(smith), smallDouble(smith));
            },
            6 => fuzzScale(smith, map),
            7 => fuzzRotate(smith, map),
            8 => {
                if (map) |handle| _ = c.mln_map_pitch_by(handle, smallDouble(smith));
            },
            9 => {
                if (map) |handle| _ = c.mln_map_cancel_transitions(handle);
            },
            10 => fuzzEventPoll(smith, map),
            11 => _ = c.mln_thread_last_error_message(),
            12 => _ = c.mln_network_status_set(smith.value(u32)),
            13 => fuzzNetworkStatusGet(smith),
            else => unreachable,
        }
    }

    if (map) |handle| _ = c.mln_map_destroy(handle);
    if (runtime) |handle| _ = c.mln_runtime_destroy(handle);
    _ = c.mln_network_status_set(c.MLN_NETWORK_STATUS_ONLINE);
}

fn maybeCreateRuntime(smith: *testing.Smith, runtime: *?*c.mln_runtime) void {
    if (runtime.* != null) return;

    var options = c.mln_runtime_options_default();
    if (smith.value(bool)) options.size = smith.value(u32);
    if (smith.value(bool)) options.flags = smith.value(u32);
    _ = c.mln_runtime_create(&options, runtime);
}

fn maybeCreateMap(smith: *testing.Smith, runtime: ?*c.mln_runtime, map: *?*c.mln_map) void {
    if (runtime == null or map.* != null) return;

    var options = c.mln_map_options_default();
    switch (smith.valueRangeAtMost(u8, 0, 3)) {
        0 => options.size = smith.value(u32),
        1 => options.width = smith.valueRangeAtMost(u32, 0, 4096),
        2 => options.height = smith.valueRangeAtMost(u32, 0, 4096),
        3 => options.scale_factor = smallDouble(smith),
        else => unreachable,
    }
    _ = c.mln_map_create(runtime, &options, map);
}

fn fuzzStyleJson(smith: *testing.Smith, map: ?*c.mln_map) void {
    const handle = map orelse return;

    var buffer: [4096:0]u8 = undefined;
    const length = smith.slice(buffer[0..4096]);
    buffer[length] = 0;
    _ = c.mln_map_set_style_json(handle, @ptrCast(&buffer));
}

fn fuzzCamera(smith: *testing.Smith, map: ?*c.mln_map) void {
    const handle = map orelse return;

    var camera = c.mln_camera_options_default();
    if (smith.value(bool)) camera.size = smith.value(u32);
    camera.fields = smith.value(u32);
    camera.latitude = smallDouble(smith);
    camera.longitude = smallDouble(smith);
    camera.zoom = smallDouble(smith);
    camera.bearing = smallDouble(smith);
    camera.pitch = smallDouble(smith);
    _ = c.mln_map_jump_to(handle, &camera);
    _ = c.mln_map_get_camera(handle, &camera);
}

fn fuzzScale(smith: *testing.Smith, map: ?*c.mln_map) void {
    const handle = map orelse return;

    const anchor = c.mln_screen_point{ .x = smallDouble(smith), .y = smallDouble(smith) };
    _ = c.mln_map_scale_by(handle, smallDouble(smith), if (smith.value(bool)) &anchor else null);
}

fn fuzzRotate(smith: *testing.Smith, map: ?*c.mln_map) void {
    const handle = map orelse return;

    const first = c.mln_screen_point{ .x = smallDouble(smith), .y = smallDouble(smith) };
    const second = c.mln_screen_point{ .x = smallDouble(smith), .y = smallDouble(smith) };
    _ = c.mln_map_rotate_by(handle, first, second);
}

fn fuzzEventPoll(smith: *testing.Smith, map: ?*c.mln_map) void {
    const handle = map orelse return;

    var event = std.mem.zeroes(c.mln_map_event);
    event.size = if (smith.value(bool)) smith.value(u32) else @sizeOf(c.mln_map_event);
    var has_event = false;
    _ = c.mln_map_poll_event(handle, &event, &has_event);
}

fn fuzzNetworkStatusGet(smith: *testing.Smith) void {
    var status: u32 = 0;
    _ = c.mln_network_status_get(if (smith.value(bool)) &status else null);
}

fn smallDouble(smith: *testing.Smith) f64 {
    return @as(f64, @floatFromInt(smith.valueRangeAtMost(i32, -100000, 100000))) / 100.0;
}
