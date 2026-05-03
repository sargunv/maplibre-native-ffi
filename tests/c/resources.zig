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

const offline_style_url = "http://example.com/offline-style.json";

fn offlineTileDefinition() c.mln_offline_region_definition {
    var definition: c.mln_offline_region_definition = undefined;
    definition.size = @sizeOf(c.mln_offline_region_definition);
    definition.type = c.MLN_OFFLINE_REGION_DEFINITION_TILE_PYRAMID;
    definition.data.tile_pyramid = .{
        .size = @sizeOf(c.mln_offline_tile_pyramid_region_definition),
        .style_url = offline_style_url,
        .bounds = .{
            .southwest = .{ .latitude = 1.0, .longitude = 2.0 },
            .northeast = .{ .latitude = 3.0, .longitude = 4.0 },
        },
        .min_zoom = 5.0,
        .max_zoom = 6.0,
        .pixel_ratio = 2.0,
        .include_ideographs = true,
    };
    return definition;
}

fn offlineGeometryDefinition(geometry: *const c.mln_geometry) c.mln_offline_region_definition {
    var definition: c.mln_offline_region_definition = undefined;
    definition.size = @sizeOf(c.mln_offline_region_definition);
    definition.type = c.MLN_OFFLINE_REGION_DEFINITION_GEOMETRY;
    definition.data.geometry = .{
        .size = @sizeOf(c.mln_offline_geometry_region_definition),
        .style_url = offline_style_url,
        .geometry = geometry,
        .min_zoom = 5.0,
        .max_zoom = 6.0,
        .pixel_ratio = 2.0,
        .include_ideographs = true,
    };
    return definition;
}

fn offlineRegionInfo() c.mln_offline_region_info {
    var info: c.mln_offline_region_info = undefined;
    info.size = @sizeOf(c.mln_offline_region_info);
    return info;
}

fn checkOfflineRegionInfo(info: *const c.mln_offline_region_info, expected_id: c.mln_offline_region_id, expected_metadata: []const u8) !void {
    try testing.expectEqual(expected_id, info.id);
    try testing.expectEqual(@as(u32, c.MLN_OFFLINE_REGION_DEFINITION_TILE_PYRAMID), info.definition.type);
    try testing.expectEqualStrings(offline_style_url, std.mem.span(info.definition.data.tile_pyramid.style_url));
    try testing.expectEqual(@as(f64, 1.0), info.definition.data.tile_pyramid.bounds.southwest.latitude);
    try testing.expectEqual(@as(f64, 2.0), info.definition.data.tile_pyramid.bounds.southwest.longitude);
    try testing.expectEqual(@as(f64, 3.0), info.definition.data.tile_pyramid.bounds.northeast.latitude);
    try testing.expectEqual(@as(f64, 4.0), info.definition.data.tile_pyramid.bounds.northeast.longitude);
    try testing.expectEqual(@as(f64, 5.0), info.definition.data.tile_pyramid.min_zoom);
    try testing.expectEqual(@as(f64, 6.0), info.definition.data.tile_pyramid.max_zoom);
    try testing.expectEqual(@as(f32, 2.0), info.definition.data.tile_pyramid.pixel_ratio);
    try testing.expect(info.definition.data.tile_pyramid.include_ideographs);
    try testing.expectEqual(expected_metadata.len, info.metadata_size);
    try testing.expectEqualSlices(u8, expected_metadata, info.metadata[0..info.metadata_size]);
}

fn checkOfflineGeometryRegionInfo(info: *const c.mln_offline_region_info, expected_id: c.mln_offline_region_id, expected_metadata: []const u8) !void {
    try testing.expectEqual(expected_id, info.id);
    try testing.expectEqual(@as(u32, c.MLN_OFFLINE_REGION_DEFINITION_GEOMETRY), info.definition.type);
    const definition = info.definition.data.geometry;
    try testing.expectEqualStrings(offline_style_url, std.mem.span(definition.style_url));
    try testing.expectEqual(@as(f64, 5.0), definition.min_zoom);
    try testing.expectEqual(@as(f64, 6.0), definition.max_zoom);
    try testing.expectEqual(@as(f32, 2.0), definition.pixel_ratio);
    try testing.expect(definition.include_ideographs);
    const geometry = definition.geometry orelse return error.MissingGeometry;
    try testing.expectEqual(@as(u32, c.MLN_GEOMETRY_TYPE_LINE_STRING), geometry[0].type);
    const line = geometry[0].data.line_string;
    try testing.expectEqual(@as(usize, 2), line.coordinate_count);
    try testing.expect(line.coordinates != null);
    try testing.expectEqual(@as(f64, 1.0), line.coordinates[0].latitude);
    try testing.expectEqual(@as(f64, 2.0), line.coordinates[0].longitude);
    try testing.expectEqual(@as(f64, 3.0), line.coordinates[1].latitude);
    try testing.expectEqual(@as(f64, 4.0), line.coordinates[1].longitude);
    try testing.expectEqual(expected_metadata.len, info.metadata_size);
    try testing.expectEqualSlices(u8, expected_metadata, info.metadata[0..info.metadata_size]);
}

fn waitForOfflineEvent(runtime: *c.mln_runtime, event_type: u32) !c.mln_runtime_event {
    for (0..5000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        while (true) {
            var event = support.emptyEvent();
            var has_event = false;
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
            if (!has_event) break;
            if (event.type == event_type and event.source_type == c.MLN_RUNTIME_EVENT_SOURCE_RUNTIME and event.source == @as(?*anyopaque, @ptrCast(runtime))) return event;
        }
        _ = usleep(1000);
    }
    return error.EventNotFound;
}

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

test "offline tile-pyramid regions validate inputs" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var definition = offlineTileDefinition();
    const metadata = [_]u8{ 1, 2, 3 };
    var snapshot: ?*c.mln_offline_region_snapshot = null;

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, null, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    definition.type = 999;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    definition = offlineTileDefinition();
    definition.data.tile_pyramid.style_url = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    definition = offlineTileDefinition();
    definition.data.tile_pyramid.min_zoom = 7.0;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    definition = offlineTileDefinition();
    definition.data.tile_pyramid.max_zoom = std.math.inf(f64);
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    if (snapshot) |handle| {
        c.mln_offline_region_snapshot_destroy(handle);
        snapshot = null;
    }

    var coordinates = [_]c.mln_lat_lng{
        .{ .latitude = 1.0, .longitude = 2.0 },
        .{ .latitude = 3.0, .longitude = 4.0 },
    };
    var geometry = c.mln_geometry{
        .size = @sizeOf(c.mln_geometry),
        .type = c.MLN_GEOMETRY_TYPE_LINE_STRING,
        .data = .{ .line_string = .{ .coordinates = coordinates[0..].ptr, .coordinate_count = coordinates.len } },
    };
    definition = offlineGeometryDefinition(&geometry);
    definition.data.geometry.style_url = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    definition = offlineGeometryDefinition(&geometry);
    definition.data.geometry.geometry = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    geometry.type = c.MLN_GEOMETRY_TYPE_EMPTY;
    definition = offlineGeometryDefinition(&geometry);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);

    var nested_geometries: [66]c.mln_geometry = undefined;
    nested_geometries[nested_geometries.len - 1] = .{
        .size = @sizeOf(c.mln_geometry),
        .type = c.MLN_GEOMETRY_TYPE_POINT,
        .data = .{ .point = .{ .latitude = 1.0, .longitude = 2.0 } },
    };
    var nested_index = nested_geometries.len - 1;
    while (nested_index > 0) {
        nested_index -= 1;
        nested_geometries[nested_index] = .{
            .size = @sizeOf(c.mln_geometry),
            .type = c.MLN_GEOMETRY_TYPE_GEOMETRY_COLLECTION,
            .data = .{ .geometry_collection = .{ .geometries = &nested_geometries[nested_index + 1], .geometry_count = 1 } },
        };
    }
    definition = offlineGeometryDefinition(&nested_geometries[0]);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &snapshot));
    try testing.expectEqual(@as(?*c.mln_offline_region_snapshot, null), snapshot);
    try testing.expectEqualStrings("GeoJSON value nesting is too deep", std.mem.span(c.mln_thread_last_error_message()));
}

test "offline tile-pyramid regions persist and support metadata lifecycle" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    const cwd = try std.process.currentPathAlloc(testing.io, testing.allocator);
    defer testing.allocator.free(cwd);
    const cache_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}/cache.db", .{ cwd, tmp.sub_path[0..] }, 0);
    defer testing.allocator.free(cache_path);

    const metadata = [_]u8{ 1, 2, 3 };
    const updated_metadata = [_]u8{ 4, 5, 6, 7 };
    var region_id: c.mln_offline_region_id = 0;

    {
        var runtime: ?*c.mln_runtime = null;
        var options = c.mln_runtime_options_default();
        options.cache_path = cache_path.ptr;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
        const runtime_handle = runtime orelse return error.RuntimeCreateFailed;
        defer support.destroyRuntime(runtime_handle);

        var definition = offlineTileDefinition();
        var created: ?*c.mln_offline_region_snapshot = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_create(runtime_handle, &definition, metadata[0..].ptr, metadata.len, &created));
        const created_handle = created orelse return error.RegionCreateFailed;
        defer c.mln_offline_region_snapshot_destroy(created_handle);

        var created_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(created_handle, &created_info));
        try testing.expect(created_info.id > 0);
        region_id = created_info.id;
        try checkOfflineRegionInfo(&created_info, region_id, metadata[0..]);

        var status: c.mln_offline_region_status = undefined;
        status.size = @sizeOf(c.mln_offline_region_status);
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_get_status(runtime_handle, region_id, &status));
        try testing.expectEqual(@as(u32, c.MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE), status.download_state);

        var list: ?*c.mln_offline_region_list = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_regions_list(runtime_handle, &list));
        const list_handle = list orelse return error.RegionListFailed;
        defer c.mln_offline_region_list_destroy(list_handle);

        var count: usize = 0;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_count(list_handle, &count));
        try testing.expectEqual(@as(usize, 1), count);
        var list_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_get(list_handle, 0, &list_info));
        try checkOfflineRegionInfo(&list_info, region_id, metadata[0..]);

        var updated: ?*c.mln_offline_region_snapshot = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_update_metadata(runtime_handle, region_id, updated_metadata[0..].ptr, updated_metadata.len, &updated));
        const updated_handle = updated orelse return error.RegionUpdateFailed;
        defer c.mln_offline_region_snapshot_destroy(updated_handle);

        var updated_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(updated_handle, &updated_info));
        try checkOfflineRegionInfo(&updated_info, region_id, updated_metadata[0..]);
    }

    {
        var runtime: ?*c.mln_runtime = null;
        var options = c.mln_runtime_options_default();
        options.cache_path = cache_path.ptr;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
        const runtime_handle = runtime orelse return error.RuntimeCreateFailed;
        defer support.destroyRuntime(runtime_handle);

        var found = false;
        var reloaded: ?*c.mln_offline_region_snapshot = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_get(runtime_handle, region_id, &reloaded, &found));
        try testing.expect(found);
        const reloaded_handle = reloaded orelse return error.RegionReloadFailed;
        defer c.mln_offline_region_snapshot_destroy(reloaded_handle);

        var reloaded_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(reloaded_handle, &reloaded_info));
        try checkOfflineRegionInfo(&reloaded_info, region_id, updated_metadata[0..]);

        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_invalidate(runtime_handle, region_id));
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_delete(runtime_handle, region_id));

        var list: ?*c.mln_offline_region_list = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_regions_list(runtime_handle, &list));
        const list_handle = list orelse return error.RegionListFailed;
        defer c.mln_offline_region_list_destroy(list_handle);

        var count: usize = 0;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_count(list_handle, &count));
        try testing.expectEqual(@as(usize, 0), count);
    }
}

test "offline geometry regions persist and expose geometry views" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    const cwd = try std.process.currentPathAlloc(testing.io, testing.allocator);
    defer testing.allocator.free(cwd);
    const cache_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}/cache.db", .{ cwd, tmp.sub_path[0..] }, 0);
    defer testing.allocator.free(cache_path);

    const metadata = [_]u8{ 7, 8, 9 };
    var region_id: c.mln_offline_region_id = 0;

    {
        var runtime: ?*c.mln_runtime = null;
        var options = c.mln_runtime_options_default();
        options.cache_path = cache_path.ptr;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
        const runtime_handle = runtime orelse return error.RuntimeCreateFailed;
        defer support.destroyRuntime(runtime_handle);

        var coordinates = [_]c.mln_lat_lng{
            .{ .latitude = 1.0, .longitude = 2.0 },
            .{ .latitude = 3.0, .longitude = 4.0 },
        };
        const geometry = c.mln_geometry{
            .size = @sizeOf(c.mln_geometry),
            .type = c.MLN_GEOMETRY_TYPE_LINE_STRING,
            .data = .{ .line_string = .{ .coordinates = coordinates[0..].ptr, .coordinate_count = coordinates.len } },
        };
        var definition = offlineGeometryDefinition(&geometry);
        var created: ?*c.mln_offline_region_snapshot = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_create(runtime_handle, &definition, metadata[0..].ptr, metadata.len, &created));
        const created_handle = created orelse return error.RegionCreateFailed;
        defer c.mln_offline_region_snapshot_destroy(created_handle);

        var created_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(created_handle, &created_info));
        try testing.expect(created_info.id > 0);
        region_id = created_info.id;
        try checkOfflineGeometryRegionInfo(&created_info, region_id, metadata[0..]);

        var list: ?*c.mln_offline_region_list = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_regions_list(runtime_handle, &list));
        const list_handle = list orelse return error.RegionListFailed;
        defer c.mln_offline_region_list_destroy(list_handle);
        var count: usize = 0;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_count(list_handle, &count));
        try testing.expectEqual(@as(usize, 1), count);
        var list_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_get(list_handle, 0, &list_info));
        try checkOfflineGeometryRegionInfo(&list_info, region_id, metadata[0..]);
    }

    {
        var runtime: ?*c.mln_runtime = null;
        var options = c.mln_runtime_options_default();
        options.cache_path = cache_path.ptr;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&options, &runtime));
        const runtime_handle = runtime orelse return error.RuntimeCreateFailed;
        defer support.destroyRuntime(runtime_handle);

        var found = false;
        var reloaded: ?*c.mln_offline_region_snapshot = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_get(runtime_handle, region_id, &reloaded, &found));
        try testing.expect(found);
        const reloaded_handle = reloaded orelse return error.RegionReloadFailed;
        defer c.mln_offline_region_snapshot_destroy(reloaded_handle);

        var reloaded_info = offlineRegionInfo();
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(reloaded_handle, &reloaded_info));
        try checkOfflineGeometryRegionInfo(&reloaded_info, region_id, metadata[0..]);

        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_delete(runtime_handle, region_id));
    }
}

test "offline region download control emits status events" {
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

    const metadata = [_]u8{9};
    var definition = offlineTileDefinition();
    definition.data.tile_pyramid.style_url = "custom://style.json";
    var created: ?*c.mln_offline_region_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &created));
    const created_handle = created orelse return error.RegionCreateFailed;
    defer c.mln_offline_region_snapshot_destroy(created_handle);

    var info = offlineRegionInfo();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(created_handle, &info));
    const region_id = info.id;

    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_set_observed(runtime, region_id + 1000, true));
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_region_set_download_state(runtime, region_id, 999));

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_set_observed(runtime, region_id, true));
    defer _ = c.mln_runtime_offline_region_set_observed(runtime, region_id, false);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_set_download_state(runtime, region_id, c.MLN_OFFLINE_REGION_DOWNLOAD_ACTIVE));
    const event = try waitForOfflineEvent(runtime, c.MLN_RUNTIME_EVENT_OFFLINE_REGION_STATUS_CHANGED);
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_STATUS, event.payload_type);
    try testing.expectEqual(@as(usize, @sizeOf(c.mln_runtime_event_offline_region_status)), event.payload_size);
    const payload: *const c.mln_runtime_event_offline_region_status = @ptrCast(@alignCast(event.payload.?));
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_event_offline_region_status)), payload.size);
    try testing.expectEqual(region_id, payload.region_id);
    try testing.expect(payload.status.download_state == c.MLN_OFFLINE_REGION_DOWNLOAD_ACTIVE or payload.status.download_state == c.MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_set_download_state(runtime, region_id, c.MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE));
}

test "offline region download errors are runtime events" {
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

    const metadata = [_]u8{8};
    var definition = offlineTileDefinition();
    definition.data.tile_pyramid.style_url = "custom://offline-error-style.json";
    var created: ?*c.mln_offline_region_snapshot = null;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_create(runtime, &definition, metadata[0..].ptr, metadata.len, &created));
    const created_handle = created orelse return error.RegionCreateFailed;
    defer c.mln_offline_region_snapshot_destroy(created_handle);

    var info = offlineRegionInfo();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_snapshot_get(created_handle, &info));
    const region_id = info.id;

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_set_observed(runtime, region_id, true));
    defer _ = c.mln_runtime_offline_region_set_observed(runtime, region_id, false);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_set_download_state(runtime, region_id, c.MLN_OFFLINE_REGION_DOWNLOAD_ACTIVE));
    const event = try waitForOfflineEvent(runtime, c.MLN_RUNTIME_EVENT_OFFLINE_REGION_RESPONSE_ERROR);
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_RESPONSE_ERROR, event.payload_type);
    const payload: *const c.mln_runtime_event_offline_region_response_error = @ptrCast(@alignCast(event.payload.?));
    try testing.expectEqual(region_id, payload.region_id);
    try testing.expectEqual(c.MLN_RESOURCE_ERROR_REASON_NOT_FOUND, payload.reason);
    try testing.expect(event.message != null);
    try testing.expect(event.message_size > 0);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_set_download_state(runtime, region_id, c.MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE));
}

test "offline database merge returns native region list" {
    var main_tmp = testing.tmpDir(.{});
    defer main_tmp.cleanup();
    var side_tmp = testing.tmpDir(.{});
    defer side_tmp.cleanup();
    const cwd = try std.process.currentPathAlloc(testing.io, testing.allocator);
    defer testing.allocator.free(cwd);
    const main_cache_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}/cache.db", .{ cwd, main_tmp.sub_path[0..] }, 0);
    defer testing.allocator.free(main_cache_path);
    const side_cache_path = try std.fmt.allocPrintSentinel(testing.allocator, "{s}/.zig-cache/tmp/{s}/cache.db", .{ cwd, side_tmp.sub_path[0..] }, 0);
    defer testing.allocator.free(side_cache_path);

    const metadata = [_]u8{ 5, 4, 3 };

    {
        var side_runtime: ?*c.mln_runtime = null;
        var side_options = c.mln_runtime_options_default();
        side_options.cache_path = side_cache_path.ptr;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&side_options, &side_runtime));
        const side_runtime_handle = side_runtime orelse return error.RuntimeCreateFailed;
        defer support.destroyRuntime(side_runtime_handle);

        var definition = offlineTileDefinition();
        var created: ?*c.mln_offline_region_snapshot = null;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_region_create(side_runtime_handle, &definition, metadata[0..].ptr, metadata.len, &created));
        const created_handle = created orelse return error.RegionCreateFailed;
        defer c.mln_offline_region_snapshot_destroy(created_handle);
    }

    var main_runtime: ?*c.mln_runtime = null;
    var main_options = c.mln_runtime_options_default();
    main_options.cache_path = main_cache_path.ptr;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_create(&main_options, &main_runtime));
    const main_runtime_handle = main_runtime orelse return error.RuntimeCreateFailed;
    defer support.destroyRuntime(main_runtime_handle);

    var merged: ?*c.mln_offline_region_list = null;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_offline_regions_merge_database(main_runtime_handle, null, &merged));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_offline_regions_merge_database(main_runtime_handle, side_cache_path.ptr, &merged));
    const merged_handle = merged orelse return error.RegionListFailed;
    defer c.mln_offline_region_list_destroy(merged_handle);

    var count: usize = 0;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_count(merged_handle, &count));
    try testing.expectEqual(@as(usize, 1), count);
    var info = offlineRegionInfo();
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_offline_region_list_get(merged_handle, 0, &info));
    try testing.expectEqualSlices(u8, metadata[0..], info.metadata[0..info.metadata_size]);
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
