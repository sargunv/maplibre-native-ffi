const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

const HttpServerState = struct {
    server: *std.Io.net.Server,
    served: bool = false,
    err: ?anyerror = null,
};

const TransformState = struct {
    replacement_url: [*:0]const u8,
    expected_original_url: []const u8,
    calls: std.atomic.Value(usize) = std.atomic.Value(usize).init(0),
    saw_style: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_original_url: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
};

const TempStyle = struct {
    tmp: std.testing.TmpDir,
    dir_path: [:0]u8,
    style_path: [:0]u8,
    style_url: [:0]u8,
};

const AsyncProviderState = struct {
    called: bool = false,
    handle: ?*c.mln_resource_request_handle = null,
    saw_style_kind: bool = false,
    saw_all_loading: bool = false,
    saw_regular_priority: bool = false,
    saw_online_usage: bool = false,
    saw_permanent_storage: bool = false,
    saw_no_range: bool = false,
    saw_no_prior: bool = false,

    fn markRequest(self: *AsyncProviderState, request: *const c.mln_resource_request, handle: *c.mln_resource_request_handle) void {
        self.called = true;
        self.handle = handle;
        self.saw_style_kind = request.kind == c.MLN_RESOURCE_KIND_STYLE;
        self.saw_all_loading = request.loading_method == c.MLN_RESOURCE_LOADING_METHOD_ALL;
        self.saw_regular_priority = request.priority == c.MLN_RESOURCE_PRIORITY_REGULAR;
        self.saw_online_usage = request.usage == c.MLN_RESOURCE_USAGE_ONLINE;
        self.saw_permanent_storage = request.storage_policy == c.MLN_RESOURCE_STORAGE_POLICY_PERMANENT;
        self.saw_no_range = !request.has_range;
        self.saw_no_prior = !request.has_prior_modified and !request.has_prior_expires and request.prior_etag == null and request.prior_data == null and request.prior_data_size == 0;
    }

    fn currentHandle(self: *AsyncProviderState) ?*c.mln_resource_request_handle {
        return self.handle;
    }
};

const PassThroughProviderState = struct {
    called: bool = false,
    saw_style_kind: bool = false,
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
    const header = try std.fmt.bufPrint(&header_buffer, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nCache-Control: public, max-age=3600\r\nETag: \"zig-style\"\r\nContent-Length: {d}\r\nConnection: close\r\n\r\n", .{support.style_json.len});
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

fn customStyleProvider(_: ?*anyopaque, request: [*c]const c.mln_resource_request, handle: ?*c.mln_resource_request_handle) callconv(.c) u32 {
    if (request == null or handle == null) return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
    if (!std.mem.startsWith(u8, std.mem.span(request.*.url), "custom://")) return c.MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH;
    defer c.mln_resource_request_release(handle);
    var response = c.mln_resource_response{
        .size = @sizeOf(c.mln_resource_response),
        .status = c.MLN_RESOURCE_RESPONSE_STATUS_OK,
        .error_reason = c.MLN_RESOURCE_ERROR_REASON_NONE,
        .bytes = support.style_json.ptr,
        .byte_count = support.style_json.len,
        .error_message = null,
        .must_revalidate = false,
        .has_modified = false,
        .modified_unix_ms = 0,
        .has_expires = false,
        .expires_unix_ms = 0,
        .etag = null,
        .has_retry_after = false,
        .retry_after_unix_ms = 0,
    };
    if (!std.mem.eql(u8, std.mem.span(request.*.url), "custom://style.json")) {
        response.status = c.MLN_RESOURCE_RESPONSE_STATUS_ERROR;
        response.error_reason = c.MLN_RESOURCE_ERROR_REASON_NOT_FOUND;
        response.bytes = null;
        response.byte_count = 0;
        response.error_message = "custom resource not found";
    }
    _ = c.mln_resource_request_complete(handle, &response);
    return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
}

fn styleResponse() c.mln_resource_response {
    return .{
        .size = @sizeOf(c.mln_resource_response),
        .status = c.MLN_RESOURCE_RESPONSE_STATUS_OK,
        .error_reason = c.MLN_RESOURCE_ERROR_REASON_NONE,
        .bytes = support.style_json.ptr,
        .byte_count = support.style_json.len,
        .error_message = null,
        .must_revalidate = false,
        .has_modified = false,
        .modified_unix_ms = 0,
        .has_expires = false,
        .expires_unix_ms = 0,
        .etag = null,
        .has_retry_after = false,
        .retry_after_unix_ms = 0,
    };
}

fn errorResponse(message: [*:0]const u8) c.mln_resource_response {
    var response = styleResponse();
    response.status = c.MLN_RESOURCE_RESPONSE_STATUS_ERROR;
    response.error_reason = c.MLN_RESOURCE_ERROR_REASON_NOT_FOUND;
    response.bytes = null;
    response.byte_count = 0;
    response.error_message = message;
    return response;
}

fn delayedStyleProvider(user_data: ?*anyopaque, request: [*c]const c.mln_resource_request, handle: ?*c.mln_resource_request_handle) callconv(.c) u32 {
    if (user_data == null or request == null or handle == null) return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
    if (!std.mem.startsWith(u8, std.mem.span(request.*.url), "custom://")) return c.MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH;
    const state: *AsyncProviderState = @ptrCast(@alignCast(user_data.?));
    state.markRequest(request, handle.?);
    return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
}

fn errorStyleProvider(_: ?*anyopaque, request: [*c]const c.mln_resource_request, handle: ?*c.mln_resource_request_handle) callconv(.c) u32 {
    if (request == null or handle == null) return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
    if (!std.mem.startsWith(u8, std.mem.span(request.*.url), "custom://")) return c.MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH;
    defer c.mln_resource_request_release(handle);
    var response = errorResponse("custom style failed");
    _ = c.mln_resource_request_complete(handle, &response);
    return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
}

fn passThroughStyleProvider(user_data: ?*anyopaque, request: [*c]const c.mln_resource_request, _: ?*c.mln_resource_request_handle) callconv(.c) u32 {
    if (user_data != null and request != null) {
        const state: *PassThroughProviderState = @ptrCast(@alignCast(user_data.?));
        state.called = true;
        state.saw_style_kind = request.*.kind == c.MLN_RESOURCE_KIND_STYLE;
    }
    return c.MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH;
}

fn waitForProviderRequest(runtime: *c.mln_runtime, state: *AsyncProviderState) !*c.mln_resource_request_handle {
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        if (state.currentHandle()) |handle| return handle;
    }
    return error.ProviderNotCalled;
}

fn rewriteStyleUrl(user_data: ?*anyopaque, kind: u32, url: [*c]const u8, out_response: [*c]c.mln_resource_transform_response) callconv(.c) c.mln_status {
    if (user_data == null or out_response == null) return c.MLN_STATUS_INVALID_ARGUMENT;
    const state: *TransformState = @ptrCast(@alignCast(user_data.?));
    _ = state.calls.fetchAdd(1, .seq_cst);
    state.saw_style.store(kind == c.MLN_RESOURCE_KIND_STYLE, .seq_cst);
    state.saw_original_url.store(std.mem.eql(u8, std.mem.span(url), state.expected_original_url), .seq_cst);
    out_response.*.url = state.replacement_url;
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

test "custom URL style loads through network provider" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = customStyleProvider,
        .user_data = null,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "custom://style.json"));
    try pumpAndExpectStyleLoaded(runtime, map);
}

test "custom provider can complete style request later" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var state = AsyncProviderState{};
    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = delayedStyleProvider,
        .user_data = &state,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "custom://style.json"));
    const handle = try waitForProviderRequest(runtime, &state);
    defer c.mln_resource_request_release(handle);

    try testing.expect(state.saw_style_kind);
    try testing.expect(state.saw_all_loading);
    try testing.expect(state.saw_regular_priority);
    try testing.expect(state.saw_online_usage);
    try testing.expect(state.saw_permanent_storage);
    try testing.expect(state.saw_no_range);
    try testing.expect(state.saw_no_prior);

    var cancelled = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_resource_request_cancelled(handle, &cancelled));
    try testing.expect(!cancelled);

    var response = styleResponse();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_resource_request_complete(handle, &response));
    try pumpAndExpectStyleLoaded(runtime, map);
}

test "custom provider observes cancellation before late completion" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var state = AsyncProviderState{};
    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = delayedStyleProvider,
        .user_data = &state,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "custom://style.json"));
    const handle = try waitForProviderRequest(runtime, &state);
    defer c.mln_resource_request_release(handle);

    support.destroyMap(map);

    var cancelled = false;
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_resource_request_cancelled(handle, &cancelled));
        if (cancelled) break;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
    }
    try testing.expect(cancelled);

    var response = styleResponse();
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_resource_request_complete(handle, &response));
}

test "custom provider error response fails style load" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = errorStyleProvider,
        .user_data = null,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "custom://style.json"));
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_MAP_EVENT_MAP_LOADING_FAILED));
}

test "network provider pass-through delegates to native HTTP" {
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

    var provider_state = PassThroughProviderState{};
    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = passThroughStyleProvider,
        .user_data = &provider_state,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, style_url));
    try pumpAndExpectStyleLoaded(runtime, map);

    server_thread.join();
    server_thread_joined = true;
    try testing.expect(provider_state.called);
    try testing.expect(provider_state.saw_style_kind);
    try testing.expect(server_state.served);
    try testing.expectEqual(@as(?anyerror, null), server_state.err);
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

test "ambient cache operations validate cache configuration" {
    const runtime_without_cache = try support.createRuntime();

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_run_ambient_cache_operation(runtime_without_cache, 999));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_ambient_cache_operation(runtime_without_cache, c.MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE));
    support.destroyRuntime(runtime_without_cache);

    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    const cwd = try std.process.currentPathAlloc(testing.io, testing.allocator);
    defer testing.allocator.free(cwd);
    const cache_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}/cache.db", .{ cwd, tmp.sub_path[0..] }, 0);
    defer testing.allocator.free(cache_path);

    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    options.cache_path = cache_path.ptr;
    options.flags |= c.MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE;
    options.maximum_cache_size = 1024 * 1024;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
    const runtime_handle = runtime orelse return error.RuntimeCreateFailed;
    defer support.destroyRuntime(runtime_handle);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_ambient_cache_operation(runtime_handle, c.MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_ambient_cache_operation(runtime_handle, c.MLN_AMBIENT_CACHE_OPERATION_INVALIDATE));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_ambient_cache_operation(runtime_handle, c.MLN_AMBIENT_CACHE_OPERATION_CLEAR));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_ambient_cache_operation(runtime_handle, c.MLN_AMBIENT_CACHE_OPERATION_RESET_DATABASE));
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

test "resource transform rewrites network style URL" {
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

    const replacement_url = try std.fmt.allocPrintSentinel(testing.allocator, "http://127.0.0.1:{d}/style.json", .{server.socket.address.getPort()}, 0);
    defer testing.allocator.free(replacement_url);
    const original_url = "http://example.invalid/original-style.json";
    var transform_state = TransformState{ .replacement_url = replacement_url.ptr, .expected_original_url = original_url };

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_set_resource_transform(runtime, null));
    var transform = c.mln_resource_transform{ .size = 0, .callback = rewriteStyleUrl, .user_data = &transform_state };
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_set_resource_transform(runtime, &transform));
    transform.size = @sizeOf(c.mln_resource_transform);
    transform.callback = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_set_resource_transform(runtime, &transform));
    transform.callback = rewriteStyleUrl;

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_transform(runtime, &transform));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_set_resource_transform(runtime, &transform));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, original_url));
    try pumpAndExpectStyleLoaded(runtime, map);

    server_thread.join();
    server_thread_joined = true;
    try testing.expect(server_state.served);
    try testing.expect(transform_state.calls.load(.seq_cst) > 0);
    try testing.expect(transform_state.saw_style.load(.seq_cst));
    try testing.expect(transform_state.saw_original_url.load(.seq_cst));
    try testing.expectEqual(@as(?anyerror, null), server_state.err);
}

test "http style can load from ambient cache after online load" {
    try support.suppressLogs();
    defer support.restoreLogs();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_set(c.MLN_NETWORK_STATUS_ONLINE));
    defer _ = c.mln_network_status_set(c.MLN_NETWORK_STATUS_ONLINE);

    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    const cwd = try std.process.currentPathAlloc(testing.io, testing.allocator);
    defer testing.allocator.free(cwd);
    const cache_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}/cache.db", .{ cwd, tmp.sub_path[0..] }, 0);
    defer testing.allocator.free(cache_path);

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

    var runtime: ?*c.mln_runtime = null;
    var options = c.mln_runtime_options_default();
    options.cache_path = cache_path.ptr;
    options.flags |= c.MLN_RUNTIME_OPTION_MAXIMUM_CACHE_SIZE;
    options.maximum_cache_size = 1024 * 1024;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
    const runtime_handle = runtime orelse return error.RuntimeCreateFailed;

    {
        const map = try support.createMap(runtime_handle);
        defer support.destroyMap(map);
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, style_url));
        try pumpAndExpectStyleLoaded(runtime_handle, map);
    }
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_ambient_cache_operation(runtime_handle, c.MLN_AMBIENT_CACHE_OPERATION_PACK_DATABASE));
    support.destroyRuntime(runtime_handle);

    server_thread.join();
    server_thread_joined = true;
    try testing.expect(server_state.served);
    try testing.expectEqual(@as(?anyerror, null), server_state.err);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_network_status_set(c.MLN_NETWORK_STATUS_OFFLINE));
    var cached_runtime: ?*c.mln_runtime = null;
    var cached_options = c.mln_runtime_options_default();
    cached_options.cache_path = cache_path.ptr;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&cached_options, &cached_runtime));
    const cached_runtime_handle = cached_runtime orelse return error.RuntimeCreateFailed;
    defer support.destroyRuntime(cached_runtime_handle);

    const cached_map = try support.createMap(cached_runtime_handle);
    defer support.destroyMap(cached_map);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(cached_map, style_url));
    try pumpAndExpectStyleLoaded(cached_runtime_handle, cached_map);
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

test "invalid resource provider settings are rejected" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_set_resource_provider(runtime, null));

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = customStyleProvider,
        .user_data = null,
    };
    provider.size = 0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_set_resource_provider(runtime, &provider));

    provider.size = @sizeOf(c.mln_resource_provider);
    provider.callback = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_set_resource_provider(runtime, &provider));

    provider.callback = customStyleProvider;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));
}

test "resource provider setting is rejected after map creation" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = customStyleProvider,
        .user_data = null,
    };
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_runtime_set_resource_provider(runtime, &provider));
}
