const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const HttpServerState = struct {
    server: *std.Io.net.Server,
    served: bool = false,
    err: ?anyerror = null,
};

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

fn serveOneHttpStyleInner(state: *HttpServerState) !void {
    var stream = try state.server.accept(testing.io);
    defer stream.close(testing.io);

    var request_buffer: [1024]u8 = undefined;
    _ = try std.posix.read(stream.socket.handle, &request_buffer);

    var header_buffer: [256]u8 = undefined;
    const header = try std.fmt.bufPrint(&header_buffer, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {d}\r\nConnection: close\r\n\r\n", .{support.style_json.len});
    var response_buffer: [1024]u8 = undefined;
    var writer = stream.writer(testing.io, &response_buffer);
    try writer.interface.writeAll(header);
    try writer.interface.writeAll(support.style_json);
    try writer.interface.flush();
    state.served = true;
}

fn serveOneHttpStyle(state: *HttpServerState) void {
    serveOneHttpStyleInner(state) catch |err| {
        state.err = err;
    };
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

test "network status APIs wrap process-global MapLibre status" {
    var status: u32 = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_get(&status));
    const original_status = status;
    defer _ = c.mln_network_status_set(original_status);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_network_status_get(null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_network_status_set(999));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_set(c.MLN_NETWORK_STATUS_OFFLINE));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_get(&status));
    try testing.expectEqual(@as(u32, c.MLN_NETWORK_STATUS_OFFLINE), status);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_set(c.MLN_NETWORK_STATUS_ONLINE));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_get(&status));
    try testing.expectEqual(@as(u32, c.MLN_NETWORK_STATUS_ONLINE), status);
}

test "http URL style loads through network provider" {
    try support.suppressLogs();
    defer support.restoreLogs();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_set(c.MLN_NETWORK_STATUS_ONLINE));
    defer _ = c.mln_network_status_set(c.MLN_NETWORK_STATUS_ONLINE);

    var address = try std.Io.net.IpAddress.parse("127.0.0.1", 0);
    var server = try address.listen(testing.io, .{ .reuse_address = true });

    var server_state = HttpServerState{ .server = &server };
    const server_thread = try std.Thread.spawn(.{}, serveOneHttpStyle, .{&server_state});
    var server_thread_joined = false;
    defer {
        server.deinit(testing.io);
        if (!server_thread_joined) server_thread.join();
    }

    const style_url = try std.fmt.allocPrintSentinel(testing.allocator, "http://127.0.0.1:{d}/style.json", .{server.socket.address.getPort()}, 0);
    defer testing.allocator.free(style_url);

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, style_url));
    try pumpAndExpectStyleLoaded(runtime, map);

    server_thread.join();
    server_thread_joined = true;
    try testing.expect(server_state.served);
    try testing.expectEqual(@as(?anyerror, null), server_state.err);
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
