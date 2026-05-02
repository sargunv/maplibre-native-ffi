const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "map tuning exposes default options" {
    const viewport = c.mln_map_viewport_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_map_viewport_options)), viewport.size);
    try testing.expectEqual(@as(u32, 0), viewport.fields);
    try testing.expectEqual(@as(u32, c.MLN_NORTH_ORIENTATION_UP), viewport.north_orientation);
    try testing.expectEqual(@as(u32, c.MLN_CONSTRAIN_MODE_HEIGHT_ONLY), viewport.constrain_mode);
    try testing.expectEqual(@as(u32, c.MLN_VIEWPORT_MODE_DEFAULT), viewport.viewport_mode);

    const tile = c.mln_map_tile_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_map_tile_options)), tile.size);
    try testing.expectEqual(@as(u32, 0), tile.fields);
    try testing.expectEqual(@as(u32, c.MLN_TILE_LOD_MODE_DEFAULT), tile.lod_mode);
}

test "map debug options round trip and diagnostics toggles" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    const debug = c.MLN_MAP_DEBUG_TILE_BORDERS | c.MLN_MAP_DEBUG_COLLISION | c.MLN_MAP_DEBUG_DEPTH_BUFFER;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_debug_options(map, debug));

    var snapshot: u32 = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_debug_options(map, &snapshot));
    try testing.expectEqual(@as(u32, debug), snapshot);

    var stats_enabled = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_rendering_stats_view_enabled(map, &stats_enabled));
    try testing.expect(!stats_enabled);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_rendering_stats_view_enabled(map, true));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_rendering_stats_view_enabled(map, &stats_enabled));
    try testing.expect(stats_enabled);

    var loaded = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_is_fully_loaded(map, &loaded));
}

test "map debug options reject invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var out_options: u32 = 0;
    var out_bool = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_debug_options(null, 0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_debug_options(map, @as(u32, 1) << 31));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_debug_options(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_debug_options(null, &out_options));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_rendering_stats_view_enabled(null, true));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_rendering_stats_view_enabled(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_rendering_stats_view_enabled(null, &out_bool));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_is_fully_loaded(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_is_fully_loaded(null, &out_bool));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_dump_debug_logs(null));
}

test "map viewport options update selected fields" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var options = c.mln_map_viewport_options_default();
    options.fields = c.MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION |
        c.MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE |
        c.MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE |
        c.MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET;
    options.north_orientation = c.MLN_NORTH_ORIENTATION_RIGHT;
    options.constrain_mode = c.MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT;
    options.viewport_mode = c.MLN_VIEWPORT_MODE_FLIPPED_Y;
    options.frustum_offset = .{ .top = 1.0, .left = -2.0, .bottom = 3.0, .right = -4.0 };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_viewport_options(map, &options));

    var snapshot = c.mln_map_viewport_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_viewport_options(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION) != 0);
    try testing.expect((snapshot.fields & c.MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE) != 0);
    try testing.expect((snapshot.fields & c.MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE) != 0);
    try testing.expect((snapshot.fields & c.MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET) != 0);
    try testing.expectEqual(@as(u32, c.MLN_NORTH_ORIENTATION_RIGHT), snapshot.north_orientation);
    try testing.expectEqual(@as(u32, c.MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT), snapshot.constrain_mode);
    try testing.expectEqual(@as(u32, c.MLN_VIEWPORT_MODE_FLIPPED_Y), snapshot.viewport_mode);
    try testing.expectApproxEqAbs(@as(f64, 1.0), snapshot.frustum_offset.top, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, -2.0), snapshot.frustum_offset.left, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, 3.0), snapshot.frustum_offset.bottom, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, -4.0), snapshot.frustum_offset.right, 0.000001);

    options = c.mln_map_viewport_options_default();
    options.fields = c.MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION;
    options.north_orientation = c.MLN_NORTH_ORIENTATION_DOWN;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_viewport_options(map, &options));
    snapshot = c.mln_map_viewport_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_viewport_options(map, &snapshot));
    try testing.expectEqual(@as(u32, c.MLN_NORTH_ORIENTATION_DOWN), snapshot.north_orientation);
    try testing.expectEqual(@as(u32, c.MLN_CONSTRAIN_MODE_WIDTH_AND_HEIGHT), snapshot.constrain_mode);
}

test "map viewport options reject invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_viewport_options(map, null));

    var options = c.mln_map_viewport_options_default();
    options.size = @sizeOf(c.mln_map_viewport_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_viewport_options(map, &options));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, &options));

    options = c.mln_map_viewport_options_default();
    options.fields = @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, &options));

    options = c.mln_map_viewport_options_default();
    options.fields = c.MLN_MAP_VIEWPORT_OPTION_NORTH_ORIENTATION;
    options.north_orientation = 99;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, &options));

    options = c.mln_map_viewport_options_default();
    options.fields = c.MLN_MAP_VIEWPORT_OPTION_CONSTRAIN_MODE;
    options.constrain_mode = 99;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, &options));

    options = c.mln_map_viewport_options_default();
    options.fields = c.MLN_MAP_VIEWPORT_OPTION_VIEWPORT_MODE;
    options.viewport_mode = 99;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, &options));

    options = c.mln_map_viewport_options_default();
    options.fields = c.MLN_MAP_VIEWPORT_OPTION_FRUSTUM_OFFSET;
    options.frustum_offset.top = std.math.inf(f64);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_viewport_options(map, &options));
}

test "map tile options update selected fields" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var options = c.mln_map_tile_options_default();
    options.fields = c.MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA |
        c.MLN_MAP_TILE_OPTION_LOD_MIN_RADIUS |
        c.MLN_MAP_TILE_OPTION_LOD_SCALE |
        c.MLN_MAP_TILE_OPTION_LOD_PITCH_THRESHOLD |
        c.MLN_MAP_TILE_OPTION_LOD_ZOOM_SHIFT |
        c.MLN_MAP_TILE_OPTION_LOD_MODE;
    options.prefetch_zoom_delta = 2;
    options.lod_min_radius = 1.5;
    options.lod_scale = 2.5;
    options.lod_pitch_threshold = 0.75;
    options.lod_zoom_shift = -1.0;
    options.lod_mode = c.MLN_TILE_LOD_MODE_DISTANCE;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_tile_options(map, &options));

    var snapshot = c.mln_map_tile_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_tile_options(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA) != 0);
    try testing.expect((snapshot.fields & c.MLN_MAP_TILE_OPTION_LOD_MODE) != 0);
    try testing.expectEqual(@as(u32, 2), snapshot.prefetch_zoom_delta);
    try testing.expectApproxEqAbs(@as(f64, 1.5), snapshot.lod_min_radius, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, 2.5), snapshot.lod_scale, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, 0.75), snapshot.lod_pitch_threshold, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, -1.0), snapshot.lod_zoom_shift, 0.000001);
    try testing.expectEqual(@as(u32, c.MLN_TILE_LOD_MODE_DISTANCE), snapshot.lod_mode);

    options = c.mln_map_tile_options_default();
    options.fields = c.MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA;
    options.prefetch_zoom_delta = 7;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_tile_options(map, &options));
    snapshot = c.mln_map_tile_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_tile_options(map, &snapshot));
    try testing.expectEqual(@as(u32, 7), snapshot.prefetch_zoom_delta);
    try testing.expectEqual(@as(u32, c.MLN_TILE_LOD_MODE_DISTANCE), snapshot.lod_mode);
}

test "map tile options reject invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_tile_options(map, null));

    var options = c.mln_map_tile_options_default();
    options.size = @sizeOf(c.mln_map_tile_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_tile_options(map, &options));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_tile_options(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_tile_options(map, &options));

    options = c.mln_map_tile_options_default();
    options.fields = @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_tile_options(map, &options));

    options = c.mln_map_tile_options_default();
    options.fields = c.MLN_MAP_TILE_OPTION_PREFETCH_ZOOM_DELTA;
    options.prefetch_zoom_delta = 256;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_tile_options(map, &options));

    options = c.mln_map_tile_options_default();
    options.fields = c.MLN_MAP_TILE_OPTION_LOD_SCALE;
    options.lod_scale = std.math.nan(f64);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_tile_options(map, &options));

    options = c.mln_map_tile_options_default();
    options.fields = c.MLN_MAP_TILE_OPTION_LOD_MODE;
    options.lod_mode = 99;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_tile_options(map, &options));
}
