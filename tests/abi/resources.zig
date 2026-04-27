const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const TempStyle = struct {
    tmp: std.testing.TmpDir,
    dir_path: [:0]u8,
    style_path: [:0]u8,
    style_url: [:0]u8,
};

fn writeTempStyle() !TempStyle {
    var tmp = testing.tmpDir(.{});
    try tmp.dir.writeFile(testing.io, .{ .sub_path = "style.json", .data = support.style_json });

    const cwd = try std.process.currentPathAlloc(testing.io, testing.allocator);
    defer testing.allocator.free(cwd);
    const dir_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}", .{ cwd, tmp.sub_path[0..] }, 0);
    errdefer testing.allocator.free(dir_path);

    const style_path = try std.fs.path.joinZ(testing.allocator, &.{ dir_path, "style.json" });
    errdefer testing.allocator.free(style_path);

    const style_url = try std.fmt.allocPrintSentinel(testing.allocator, "file://{s}", .{style_path}, 0);
    errdefer testing.allocator.free(style_url);

    return .{ .tmp = tmp, .dir_path = dir_path, .style_path = style_path, .style_url = style_url };
}

fn freeTempStyle(fixture: *const TempStyle) void {
    testing.allocator.free(fixture.style_url);
    testing.allocator.free(fixture.style_path);
    testing.allocator.free(fixture.dir_path);
}

fn pumpAndExpectStyleLoaded(runtime: *c.mln_runtime, map: *c.mln_map) !void {
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_STYLE_LOADED));
}

fn customStyleProvider(_: ?*anyopaque, url: [*c]const u8, out_response: [*c]c.mln_resource_provider_response) callconv(.c) c.mln_status {
    if (out_response == null) return c.MLN_STATUS_INVALID_ARGUMENT;
    if (!std.mem.eql(u8, std.mem.span(url), "custom://style.json")) {
        out_response.*.error_message = "custom resource not found";
        return c.MLN_STATUS_OK;
    }
    out_response.*.bytes = support.style_json.ptr;
    out_response.*.byte_count = support.style_json.len;
    out_response.*.error_message = null;
    return c.MLN_STATUS_OK;
}

test "file URL style loads through resource loader" {
    try support.suppressLogs();
    defer support.restoreLogs();

    var fixture = try writeTempStyle();
    defer fixture.tmp.cleanup();
    defer freeTempStyle(&fixture);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, fixture.style_url));
    try pumpAndExpectStyleLoaded(runtime, map);
}

test "asset URL style loads from runtime asset path" {
    try support.suppressLogs();
    defer support.restoreLogs();

    var fixture = try writeTempStyle();
    defer fixture.tmp.cleanup();
    defer freeTempStyle(&fixture);

    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    options.asset_path = fixture.dir_path.ptr;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
    const runtime_handle = runtime orelse return error.RuntimeCreateFailed;
    defer support.destroyRuntime(runtime_handle);

    const map = try support.createMap(runtime_handle);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "asset://style.json"));
    try pumpAndExpectStyleLoaded(runtime_handle, map);
}

test "custom URL style loads through registered provider" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .scheme = "custom",
        .callback = customStyleProvider,
        .user_data = null,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_register_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "custom://style.json"));
    try pumpAndExpectStyleLoaded(runtime, map);
}

test "missing file URL reports map loading failure" {
    try support.suppressLogs();
    defer support.restoreLogs();

    var fixture = try writeTempStyle();
    defer fixture.tmp.cleanup();
    defer freeTempStyle(&fixture);

    const missing_path = try std.fs.path.joinZ(testing.allocator, &.{ fixture.dir_path, "missing-style.json" });
    defer testing.allocator.free(missing_path);
    const missing_url = try std.fmt.allocPrintSentinel(testing.allocator, "file://{s}", .{missing_path}, 0);
    defer testing.allocator.free(missing_url);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, missing_url));
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_MAP_LOADING_FAILED));
}

test "invalid resource provider registrations are rejected" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_register_resource_provider(runtime, null));

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .scheme = "file",
        .callback = customStyleProvider,
        .user_data = null,
    };
    provider.size = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_register_resource_provider(runtime, &provider));

    provider.size = @sizeOf(c.mln_resource_provider);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_register_resource_provider(runtime, &provider));

    provider.scheme = "1bad";
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_register_resource_provider(runtime, &provider));

    provider.scheme = "custom";
    provider.callback = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_register_resource_provider(runtime, &provider));

    provider.callback = customStyleProvider;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_register_resource_provider(runtime, &provider));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_register_resource_provider(runtime, &provider));
}

test "resource provider registration is rejected after map creation" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .scheme = "custom",
        .callback = customStyleProvider,
        .user_data = null,
    };
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_register_resource_provider(runtime, &provider));
}
