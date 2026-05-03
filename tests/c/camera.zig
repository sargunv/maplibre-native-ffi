const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "camera exposes default options" {
    const camera = c.mln_camera_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_camera_options)), camera.size);
    try testing.expectEqual(@as(u32, 0), camera.fields);

    const animation = c.mln_animation_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_animation_options)), animation.size);
    try testing.expectEqual(@as(u32, 0), animation.fields);

    const fit = c.mln_camera_fit_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_camera_fit_options)), fit.size);
    try testing.expectEqual(@as(u32, 0), fit.fields);

    const bounds = c.mln_bound_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_bound_options)), bounds.size);
    try testing.expectEqual(@as(u32, 0), bounds.fields);

    const free_camera = c.mln_free_camera_options_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_free_camera_options)), free_camera.size);
    try testing.expectEqual(@as(u32, 0), free_camera.fields);
}

test "camera rejects invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_camera(map, null));

    var snapshot = c.mln_camera_options_default();
    snapshot.size = @sizeOf(c.mln_camera_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_camera(map, &snapshot));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_jump_to(map, null));

    var camera = support.testCamera();
    camera.size = @sizeOf(c.mln_camera_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_jump_to(map, &camera));

    camera = support.testCamera();
    camera.fields |= @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_jump_to(map, &camera));

    camera = support.testCamera();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER;
    camera.latitude = std.math.inf(f64);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_jump_to(map, &camera));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_ease_to(map, null, null));

    camera = support.testCamera();
    var animation = c.mln_animation_options_default();
    animation.size = @sizeOf(c.mln_animation_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_ease_to(map, &camera, &animation));

    animation = c.mln_animation_options_default();
    animation.fields = @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_fly_to(map, &camera, &animation));

    animation = c.mln_animation_options_default();
    animation.fields = c.MLN_ANIMATION_OPTION_DURATION;
    animation.duration_ms = -1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_ease_to(map, &camera, &animation));

    animation = c.mln_animation_options_default();
    animation.fields = c.MLN_ANIMATION_OPTION_EASING;
    animation.easing.x1 = 2;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_fly_to(map, &camera, &animation));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_move_by(map, std.math.nan(f64), 0));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_scale_by(map, 0, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_rotate_by(map, .{ .x = std.math.inf(f64), .y = 0 }, .{ .x = 0, .y = 0 }));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_pitch_by(map, std.math.nan(f64)));
}

test "camera jump updates snapshot fields" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var camera = support.testCamera();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_jump_to(map, &camera));

    var snapshot = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_camera(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);
}

test "camera commands accept valid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    const anchor = c.mln_screen_point{ .x = 256, .y = 256 };
    const rotate_start = c.mln_screen_point{ .x = 200, .y = 200 };
    const rotate_end = c.mln_screen_point{ .x = 220, .y = 210 };

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_move_by(map, 4, -2));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_scale_by(map, 1.1, &anchor));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_scale_by(map, 0.95, null));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_rotate_by(map, rotate_start, rotate_end));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pitch_by(map, 3));

    var animation = c.mln_animation_options_default();
    animation.fields = c.MLN_ANIMATION_OPTION_DURATION | c.MLN_ANIMATION_OPTION_EASING;
    animation.duration_ms = 0;
    animation.easing = .{ .x1 = 0.0, .y1 = 0.0, .x2 = 0.25, .y2 = 1.0 };
    var camera = support.testCamera();
    camera.zoom = 12.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_ease_to(map, &camera, &animation));
    camera.zoom = 10.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_fly_to(map, &camera, &animation));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_move_by_animated(map, 1, 2, &animation));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_scale_by_animated(map, 1.01, &anchor, &animation));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_rotate_by_animated(map, rotate_start, rotate_end, &animation));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pitch_by_animated(map, 1, &animation));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_cancel_transitions(map));
}

test "camera fitting computes cameras and visible bounds" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    const bounds = c.mln_lat_lng_bounds{
        .southwest = .{ .latitude = 35.0, .longitude = -125.0 },
        .northeast = .{ .latitude = 39.0, .longitude = -120.0 },
    };
    var fit = c.mln_camera_fit_options_default();
    fit.fields = c.MLN_CAMERA_FIT_OPTION_PADDING | c.MLN_CAMERA_FIT_OPTION_BEARING | c.MLN_CAMERA_FIT_OPTION_PITCH;
    fit.padding = .{ .top = 8, .left = 12, .bottom = 8, .right = 12 };
    fit.bearing = 5;
    fit.pitch = 15;

    var camera = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_camera_for_lat_lng_bounds(map, bounds, &fit, &camera));
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_PADDING) != 0);
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_BEARING) != 0);
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_PITCH) != 0);

    const coordinates = [_]c.mln_lat_lng{ bounds.southwest, bounds.northeast };
    camera = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_camera_for_lat_lngs(map, coordinates[0..].ptr, coordinates.len, null, &camera));
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);

    const line = c.mln_geometry{
        .size = @sizeOf(c.mln_geometry),
        .type = c.MLN_GEOMETRY_TYPE_LINE_STRING,
        .data = .{ .line_string = .{ .coordinates = coordinates[0..].ptr, .coordinate_count = coordinates.len } },
    };
    camera = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_camera_for_geometry(map, &line, null, &camera));
    try testing.expect((camera.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);

    var visible_bounds: c.mln_lat_lng_bounds = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_lat_lng_bounds_for_camera(map, &camera, &visible_bounds));
    try testing.expect(visible_bounds.southwest.latitude <= visible_bounds.northeast.latitude);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_lat_lng_bounds_for_camera_unwrapped(map, &camera, &visible_bounds));
    try testing.expect(visible_bounds.southwest.latitude <= visible_bounds.northeast.latitude);
}

test "camera fitting rejects invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var camera = c.mln_camera_options_default();
    const bounds = c.mln_lat_lng_bounds{
        .southwest = .{ .latitude = 10.0, .longitude = 10.0 },
        .northeast = .{ .latitude = -10.0, .longitude = 20.0 },
    };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_lat_lng_bounds(map, bounds, null, &camera));

    var fit = c.mln_camera_fit_options_default();
    fit.size = @sizeOf(c.mln_camera_fit_options) - 1;
    const valid_bounds = c.mln_lat_lng_bounds{
        .southwest = .{ .latitude = -10.0, .longitude = -10.0 },
        .northeast = .{ .latitude = 10.0, .longitude = 10.0 },
    };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_lat_lng_bounds(map, valid_bounds, &fit, &camera));

    const coordinate = c.mln_lat_lng{ .latitude = 0.0, .longitude = 0.0 };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_lat_lngs(map, null, 0, null, &camera));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_lat_lngs(map, null, 1, null, &camera));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_lat_lngs(map, &coordinate, 1, null, null));

    var empty_geometry = c.mln_geometry{
        .size = @sizeOf(c.mln_geometry),
        .type = c.MLN_GEOMETRY_TYPE_EMPTY,
        .data = .{ .point = coordinate },
    };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_geometry(map, null, null, &camera));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_camera_for_geometry(map, &empty_geometry, null, &camera));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_lat_lng_bounds_for_camera(map, null, null));
}

test "camera bounds constraints round trip" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var options = c.mln_bound_options_default();
    options.fields = c.MLN_BOUND_OPTION_BOUNDS |
        c.MLN_BOUND_OPTION_MIN_ZOOM |
        c.MLN_BOUND_OPTION_MAX_ZOOM |
        c.MLN_BOUND_OPTION_MIN_PITCH |
        c.MLN_BOUND_OPTION_MAX_PITCH;
    options.bounds = .{
        .southwest = .{ .latitude = -20.0, .longitude = -30.0 },
        .northeast = .{ .latitude = 20.0, .longitude = 30.0 },
    };
    options.min_zoom = 1.0;
    options.max_zoom = 8.0;
    options.min_pitch = 0.0;
    options.max_pitch = 45.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_bounds(map, &options));

    var snapshot = c.mln_bound_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_bounds(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_BOUND_OPTION_BOUNDS) != 0);
    try testing.expect((snapshot.fields & c.MLN_BOUND_OPTION_MIN_ZOOM) != 0);
    try testing.expect((snapshot.fields & c.MLN_BOUND_OPTION_MAX_ZOOM) != 0);
    try testing.expectApproxEqAbs(options.bounds.southwest.latitude, snapshot.bounds.southwest.latitude, 0.000001);
    try testing.expectApproxEqAbs(options.min_zoom, snapshot.min_zoom, 0.000001);
    try testing.expectApproxEqAbs(options.max_zoom, snapshot.max_zoom, 0.000001);
    try testing.expectApproxEqAbs(options.max_pitch, snapshot.max_pitch, 0.000001);
}

test "camera bounds constraints reject invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_bounds(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_bounds(map, null));

    var options = c.mln_bound_options_default();
    options.size = @sizeOf(c.mln_bound_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_bounds(map, &options));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_bounds(map, &options));

    options = c.mln_bound_options_default();
    options.fields = @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_bounds(map, &options));

    options = c.mln_bound_options_default();
    options.fields = c.MLN_BOUND_OPTION_MIN_ZOOM | c.MLN_BOUND_OPTION_MAX_ZOOM;
    options.min_zoom = 10;
    options.max_zoom = 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_bounds(map, &options));
}

test "free camera options round trip and reject invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var snapshot = c.mln_free_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_free_camera_options(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_FREE_CAMERA_OPTION_POSITION) != 0);
    try testing.expect((snapshot.fields & c.MLN_FREE_CAMERA_OPTION_ORIENTATION) != 0);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_free_camera_options(map, &snapshot));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_free_camera_options(map, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_free_camera_options(map, null));

    var options = c.mln_free_camera_options_default();
    options.size = @sizeOf(c.mln_free_camera_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_free_camera_options(map, &options));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_free_camera_options(map, &options));

    options = c.mln_free_camera_options_default();
    options.fields = @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_free_camera_options(map, &options));

    options = c.mln_free_camera_options_default();
    options.fields = c.MLN_FREE_CAMERA_OPTION_POSITION;
    options.position.x = std.math.inf(f64);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_free_camera_options(map, &options));

    options = c.mln_free_camera_options_default();
    options.fields = c.MLN_FREE_CAMERA_OPTION_ORIENTATION;
    options.orientation = .{ .x = 0, .y = 0, .z = 0, .w = 0 };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_free_camera_options(map, &options));
}
