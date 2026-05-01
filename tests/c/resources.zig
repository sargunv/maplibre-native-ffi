const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

extern fn usleep(useconds: c_uint) c_int;

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

const pmtiles_style_json =
    \\{
    \\  "version": 8,
    \\  "name": "zig-pmtiles-range-test",
    \\  "sources": {
    \\    "archive": {
    \\      "type": "vector",
    \\      "url": "pmtiles://http://example.invalid/test.pmtiles"
    \\    }
    \\  },
    \\  "layers": [
    \\    {"id":"archive-fill","type":"fill","source":"archive","source-layer":"land","paint":{"fill-color":"#8dd3c7"}}
    \\  ]
    \\}
;

const pmtiles_style_url = "custom://pmtiles-range-style.json";
const pmtiles_archive_url = "http://example.invalid/test.pmtiles";

const AsyncProviderState = struct {
    handle: std.atomic.Value(usize) = std.atomic.Value(usize).init(0),
    saw_style_kind: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_all_loading: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_regular_priority: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_online_usage: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_permanent_storage: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_no_range: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_no_prior: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),

    fn markRequest(self: *AsyncProviderState, request: *const c.mln_resource_request, handle: *c.mln_resource_request_handle) void {
        self.saw_style_kind.store(request.kind == c.MLN_RESOURCE_KIND_STYLE, .seq_cst);
        self.saw_all_loading.store(request.loading_method == c.MLN_RESOURCE_LOADING_METHOD_ALL, .seq_cst);
        self.saw_regular_priority.store(request.priority == c.MLN_RESOURCE_PRIORITY_REGULAR, .seq_cst);
        self.saw_online_usage.store(request.usage == c.MLN_RESOURCE_USAGE_ONLINE, .seq_cst);
        self.saw_permanent_storage.store(request.storage_policy == c.MLN_RESOURCE_STORAGE_POLICY_PERMANENT, .seq_cst);
        self.saw_no_range.store(!request.has_range, .seq_cst);
        self.saw_no_prior.store(!request.has_prior_modified and !request.has_prior_expires and request.prior_etag == null and request.prior_data == null and request.prior_data_size == 0, .seq_cst);
        self.handle.store(@intFromPtr(handle), .seq_cst);
    }

    fn currentHandle(self: *AsyncProviderState) ?*c.mln_resource_request_handle {
        const handle = self.handle.load(.seq_cst);
        if (handle == 0) return null;
        return @ptrFromInt(handle);
    }

    fn checkObservedRequest(self: *AsyncProviderState) !void {
        try testing.expect(self.saw_style_kind.load(.seq_cst));
        try testing.expect(self.saw_all_loading.load(.seq_cst));
        try testing.expect(self.saw_regular_priority.load(.seq_cst));
        try testing.expect(self.saw_online_usage.load(.seq_cst));
        try testing.expect(self.saw_permanent_storage.load(.seq_cst));
        try testing.expect(self.saw_no_range.load(.seq_cst));
        try testing.expect(self.saw_no_prior.load(.seq_cst));
    }
};

const PassThroughProviderState = struct {
    called: bool = false,
    saw_style_kind: bool = false,
};

const PmtilesRangeProviderState = struct {
    saw_style_absent_range: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_pmtiles_request: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_source_kind: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_network_only_loading: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    saw_range: std.atomic.Value(bool) = std.atomic.Value(bool).init(false),
    range_start: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    range_end: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),

    fn markStyle(self: *PmtilesRangeProviderState, request: *const c.mln_resource_request) void {
        self.saw_style_absent_range.store(!request.has_range and request.range_start == 0 and request.range_end == 0, .seq_cst);
    }

    fn markPmtilesRequest(self: *PmtilesRangeProviderState, request: *const c.mln_resource_request) void {
        self.saw_pmtiles_request.store(true, .seq_cst);
        self.saw_source_kind.store(request.kind == c.MLN_RESOURCE_KIND_SOURCE, .seq_cst);
        self.saw_network_only_loading.store(request.loading_method == c.MLN_RESOURCE_LOADING_METHOD_NETWORK_ONLY, .seq_cst);
        self.saw_range.store(request.has_range, .seq_cst);
        self.range_start.store(request.range_start, .seq_cst);
        self.range_end.store(request.range_end, .seq_cst);
    }

    fn checkObservedRequest(self: *PmtilesRangeProviderState) !void {
        const start = self.range_start.load(.seq_cst);
        const end = self.range_end.load(.seq_cst);

        try testing.expect(self.saw_style_absent_range.load(.seq_cst));
        try testing.expect(self.saw_pmtiles_request.load(.seq_cst));
        try testing.expect(self.saw_source_kind.load(.seq_cst));
        try testing.expect(self.saw_network_only_loading.load(.seq_cst));
        try testing.expect(self.saw_range.load(.seq_cst));
        try testing.expectEqual(@as(u64, 0), start);
        try testing.expectEqual(@as(u64, 126), end);
        try testing.expectEqual(@as(u64, 127), end - start + 1);
    }
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
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_STYLE_LOADED));
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

fn pmtilesStyleResponse() c.mln_resource_response {
    var response = styleResponse();
    response.bytes = pmtiles_style_json.ptr;
    response.byte_count = pmtiles_style_json.len;
    return response;
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

fn pmtilesRangeProvider(user_data: ?*anyopaque, request: [*c]const c.mln_resource_request, handle: ?*c.mln_resource_request_handle) callconv(.c) u32 {
    if (user_data == null or request == null or handle == null) return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;

    const state: *PmtilesRangeProviderState = @ptrCast(@alignCast(user_data.?));
    const url = std.mem.span(request.*.url);

    if (std.mem.eql(u8, url, pmtiles_style_url)) {
        state.markStyle(request);
        defer c.mln_resource_request_release(handle);
        var response = pmtilesStyleResponse();
        _ = c.mln_resource_request_complete(handle, &response);
        return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
    }

    if (std.mem.eql(u8, url, pmtiles_archive_url)) {
        state.markPmtilesRequest(request);
        defer c.mln_resource_request_release(handle);
        var response = errorResponse("pmtiles archive intentionally unavailable");
        _ = c.mln_resource_request_complete(handle, &response);
        return c.MLN_RESOURCE_PROVIDER_DECISION_HANDLE;
    }

    return c.MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH;
}

fn completeRequestOnThread(handle: *c.mln_resource_request_handle, out_status: *c.mln_status) void {
    var response = styleResponse();
    out_status.* = c.mln_resource_request_complete(handle, &response);
}

fn waitForProviderRequest(runtime: *c.mln_runtime, state: *AsyncProviderState) !*c.mln_resource_request_handle {
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        if (state.currentHandle()) |handle| return handle;
        _ = usleep(1000);
    }
    return error.ProviderNotCalled;
}

fn waitForPmtilesRangeRequest(runtime: *c.mln_runtime, state: *PmtilesRangeProviderState) !void {
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        if (state.saw_pmtiles_request.load(.seq_cst)) return;
        _ = usleep(1000);
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

    try state.checkObservedRequest();

    var cancelled = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_resource_request_cancelled(handle, &cancelled));
    try testing.expect(!cancelled);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_resource_request_cancelled(handle, null));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_resource_request_complete(handle, null));

    var response = styleResponse();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_resource_request_complete(handle, &response));
    try testing.expectEqual(c.MLN_STATUS_INVALID_STATE, c.mln_resource_request_complete(handle, &response));
    try pumpAndExpectStyleLoaded(runtime, map);
}

test "custom provider observes PMTiles range metadata" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var state = PmtilesRangeProviderState{};
    var provider = c.mln_resource_provider{
        .size = @sizeOf(c.mln_resource_provider),
        .callback = pmtilesRangeProvider,
        .user_data = &state,
    };
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_set_resource_provider(runtime, &provider));

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, pmtiles_style_url));
    try waitForPmtilesRangeRequest(runtime, &state);
    try state.checkObservedRequest();
}

test "custom provider request handles validate lifecycle" {
    c.mln_resource_request_release(null);

    var cancelled = true;
    var response = styleResponse();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_resource_request_cancelled(null, &cancelled));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_resource_request_complete(null, &response));
}

test "custom provider can complete request from another thread" {
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

    var status: c.mln_status = c.MLN_STATUS_INVALID_STATE;
    const thread = try std.Thread.spawn(.{}, completeRequestOnThread, .{ handle, &status });
    thread.join();
    try testing.expectEqual(c.MLN_STATUS_OK, status);
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
    try testing.expect(std.mem.len(c.mln_thread_last_error_message()) > 0);
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
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED));
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
    try testing.expect(try support.waitForEvent(runtime, map, c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED));
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
