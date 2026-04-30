const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const center = c.mln_lat_lng{ .latitude = 37.7749, .longitude = -122.4194 };

fn centeredCamera() c.mln_camera_options {
    var camera = c.mln_camera_options_default();
    camera.fields = c.MLN_CAMERA_OPTION_CENTER | c.MLN_CAMERA_OPTION_ZOOM;
    camera.latitude = center.latitude;
    camera.longitude = center.longitude;
    camera.zoom = 10.0;
    return camera;
}

fn expectCenterPoint(point: c.mln_screen_point) !void {
    try testing.expectApproxEqAbs(@as(f64, 256.0), point.x, 0.001);
    try testing.expectApproxEqAbs(@as(f64, 256.0), point.y, 0.001);
}

fn expectLatLngApprox(expected: c.mln_lat_lng, actual: c.mln_lat_lng) !void {
    try testing.expectApproxEqAbs(expected.latitude, actual.latitude, 0.000001);
    try testing.expectApproxEqAbs(expected.longitude, actual.longitude, 0.000001);
}

test "projection mode exposes default options" {
    const mode = c.mln_projection_mode_default();
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_projection_mode)), mode.size);
    try testing.expectEqual(@as(u32, 0), mode.fields);
}

test "map projection mode updates snapshot fields" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var mode = c.mln_projection_mode_default();
    mode.fields = c.MLN_PROJECTION_MODE_AXONOMETRIC | c.MLN_PROJECTION_MODE_X_SKEW | c.MLN_PROJECTION_MODE_Y_SKEW;
    mode.axonometric = true;
    mode.x_skew = 0.25;
    mode.y_skew = -0.125;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_projection_mode(map, &mode));

    var snapshot = c.mln_projection_mode_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_get_projection_mode(map, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_PROJECTION_MODE_AXONOMETRIC) != 0);
    try testing.expect((snapshot.fields & c.MLN_PROJECTION_MODE_X_SKEW) != 0);
    try testing.expect((snapshot.fields & c.MLN_PROJECTION_MODE_Y_SKEW) != 0);
    try testing.expect(snapshot.axonometric);
    try testing.expectApproxEqAbs(mode.x_skew, snapshot.x_skew, 0.000001);
    try testing.expectApproxEqAbs(mode.y_skew, snapshot.y_skew, 0.000001);
}

test "map projection mode rejects invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_projection_mode(map, null));

    var snapshot = c.mln_projection_mode_default();
    snapshot.size = @sizeOf(c.mln_projection_mode) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_get_projection_mode(map, &snapshot));

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_projection_mode(map, null));

    var mode = c.mln_projection_mode_default();
    mode.size = @sizeOf(c.mln_projection_mode) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_projection_mode(map, &mode));

    mode = c.mln_projection_mode_default();
    mode.fields = @as(u32, 1) << 31;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_projection_mode(map, &mode));

    mode = c.mln_projection_mode_default();
    mode.fields = c.MLN_PROJECTION_MODE_X_SKEW;
    mode.x_skew = std.math.inf(f64);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_set_projection_mode(map, &mode));
}

test "map converts between lat lngs and screen points" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var camera = centeredCamera();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_jump_to(map, &camera));

    var point: c.mln_screen_point = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pixel_for_lat_lng(map, center, &point));
    try expectCenterPoint(point);

    var coordinate: c.mln_lat_lng = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_lat_lng_for_pixel(map, point, &coordinate));
    try expectLatLngApprox(center, coordinate);

    var coordinates = [_]c.mln_lat_lng{
        center,
        .{ .latitude = 0.0, .longitude = 0.0 },
    };
    var points: [coordinates.len]c.mln_screen_point = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pixels_for_lat_lngs(map, coordinates[0..].ptr, coordinates.len, points[0..].ptr));
    try expectCenterPoint(points[0]);

    var roundtrip: [points.len]c.mln_lat_lng = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_lat_lngs_for_pixels(map, points[0..].ptr, points.len, roundtrip[0..].ptr));
    try expectLatLngApprox(coordinates[0], roundtrip[0]);
    try expectLatLngApprox(coordinates[1], roundtrip[1]);
}

test "map coordinate conversion rejects invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var point: c.mln_screen_point = undefined;
    var coordinate: c.mln_lat_lng = undefined;

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_pixel_for_lat_lng(map, center, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_pixel_for_lat_lng(map, .{ .latitude = 91.0, .longitude = 0.0 }, &point));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_lat_lng_for_pixel(map, .{ .x = std.math.inf(f64), .y = 0.0 }, &coordinate));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_lat_lng_for_pixel(map, .{ .x = 0.0, .y = 0.0 }, null));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_pixels_for_lat_lngs(map, null, 0, null));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_lat_lngs_for_pixels(map, null, 0, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_pixels_for_lat_lngs(map, null, 1, &point));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_lat_lngs_for_pixels(map, null, 1, &coordinate));
}

test "standalone projection converts and updates camera" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var camera = centeredCamera();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_jump_to(map, &camera));

    var projection: ?*c.mln_map_projection = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_create(map, &projection));
    const helper = projection orelse return error.ProjectionCreateFailed;
    defer testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_destroy(helper)) catch @panic("projection destroy failed");

    var point: c.mln_screen_point = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_pixel_for_lat_lng(helper, center, &point));
    try expectCenterPoint(point);

    var coordinate: c.mln_lat_lng = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_lat_lng_for_pixel(helper, point, &coordinate));
    try expectLatLngApprox(center, coordinate);

    var helper_camera = c.mln_camera_options_default();
    helper_camera.fields = c.MLN_CAMERA_OPTION_CENTER | c.MLN_CAMERA_OPTION_ZOOM;
    helper_camera.latitude = 0.0;
    helper_camera.longitude = 0.0;
    helper_camera.zoom = 3.0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_set_camera(helper, &helper_camera));

    var snapshot = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_get_camera(helper, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);
    try testing.expectApproxEqAbs(helper_camera.latitude, snapshot.latitude, 0.000001);
    try testing.expectApproxEqAbs(helper_camera.longitude, snapshot.longitude, 0.000001);
    try testing.expectApproxEqAbs(helper_camera.zoom, snapshot.zoom, 0.000001);

    var visible = [_]c.mln_lat_lng{
        .{ .latitude = -10.0, .longitude = -10.0 },
        .{ .latitude = 10.0, .longitude = 10.0 },
    };
    const padding = c.mln_edge_insets{ .top = 10.0, .left = 20.0, .bottom = 10.0, .right = 20.0 };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_set_visible_coordinates(helper, visible[0..].ptr, visible.len, padding));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_get_camera(helper, &snapshot));
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_CENTER) != 0);
    try testing.expect((snapshot.fields & c.MLN_CAMERA_OPTION_ZOOM) != 0);
}

test "standalone projection rejects invalid arguments" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_create(map, null));

    var non_null_projection: ?*c.mln_map_projection = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_create(map, &non_null_projection));

    var projection: ?*c.mln_map_projection = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_create(map, &projection));
    const helper = projection orelse return error.ProjectionCreateFailed;
    defer testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_projection_destroy(helper)) catch @panic("projection destroy failed");

    var camera = c.mln_camera_options_default();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_get_camera(helper, null));
    camera.size = @sizeOf(c.mln_camera_options) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_get_camera(helper, &camera));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_set_camera(helper, null));

    const padding = c.mln_edge_insets{ .top = 0.0, .left = 0.0, .bottom = 0.0, .right = 0.0 };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_set_visible_coordinates(helper, null, 0, padding));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_set_visible_coordinates(helper, null, 1, padding));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_set_visible_coordinates(helper, &center, 1, .{ .top = -1.0, .left = 0.0, .bottom = 0.0, .right = 0.0 }));

    var point: c.mln_screen_point = undefined;
    var coordinate: c.mln_lat_lng = undefined;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_pixel_for_lat_lng(helper, center, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_pixel_for_lat_lng(helper, .{ .latitude = std.math.nan(f64), .longitude = 0.0 }, &point));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_lat_lng_for_pixel(helper, .{ .x = 0.0, .y = std.math.inf(f64) }, &coordinate));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_map_projection_lat_lng_for_pixel(helper, .{ .x = 0.0, .y = 0.0 }, null));
}

test "projected meters convert to and from lat lng" {
    const origin = c.mln_lat_lng{ .latitude = 0.0, .longitude = 0.0 };
    var meters: c.mln_projected_meters = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_projected_meters_for_lat_lng(origin, &meters));
    try testing.expectApproxEqAbs(@as(f64, 0.0), meters.northing, 0.000001);
    try testing.expectApproxEqAbs(@as(f64, 0.0), meters.easting, 0.000001);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_projected_meters_for_lat_lng(center, &meters));
    var roundtrip: c.mln_lat_lng = undefined;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_lat_lng_for_projected_meters(meters, &roundtrip));
    try expectLatLngApprox(center, roundtrip);
}

test "projected meters reject invalid arguments" {
    var meters: c.mln_projected_meters = undefined;
    var coordinate: c.mln_lat_lng = undefined;

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_projected_meters_for_lat_lng(center, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_projected_meters_for_lat_lng(.{ .latitude = 0.0, .longitude = std.math.inf(f64) }, &meters));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_lat_lng_for_projected_meters(.{ .northing = std.math.nan(f64), .easting = 0.0 }, &coordinate));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_lat_lng_for_projected_meters(.{ .northing = 0.0, .easting = 0.0 }, null));
}
