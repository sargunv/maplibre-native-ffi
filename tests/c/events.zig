const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

extern fn usleep(useconds: c_uint) c_int;

test "runtime event polling reports empty queues" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    _ = try support.drainEvents(runtime);

    var event = support.emptyEvent();
    var has_event = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
    try testing.expect(!has_event);
}

test "runtime event polling rejects invalid outputs" {
    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    var has_event = false;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(runtime, null, &has_event));

    var event = support.emptyEvent();
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(runtime, &event, null));

    event.size = @sizeOf(c.mln_runtime_event) - 1;
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_runtime_poll_event(runtime, &event, &has_event));
}

test "observer event payload ABI is visible to C import" {
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_RENDER_FRAME, @as(u32, 1));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_RENDER_MAP, @as(u32, 2));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_STYLE_IMAGE_MISSING, @as(u32, 3));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_TILE_ACTION, @as(u32, 4));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_STATUS, @as(u32, 5));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_RESPONSE_ERROR, @as(u32, 6));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_OFFLINE_REGION_TILE_COUNT_LIMIT, @as(u32, 7));

    try testing.expectEqual(c.MLN_RENDER_MODE_PARTIAL, @as(u32, 0));
    try testing.expectEqual(c.MLN_RENDER_MODE_FULL, @as(u32, 1));
    try testing.expectEqual(c.MLN_TILE_OPERATION_REQUESTED_FROM_CACHE, @as(u32, 0));
    try testing.expectEqual(c.MLN_TILE_OPERATION_NULL, @as(u32, 8));

    const stats = c.mln_rendering_stats{
        .size = @sizeOf(c.mln_rendering_stats),
        .encoding_time = 0,
        .rendering_time = 0,
        .frame_count = 0,
        .draw_call_count = 0,
        .total_draw_call_count = 0,
    };
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_rendering_stats)), stats.size);

    const frame = c.mln_runtime_event_render_frame{
        .size = @sizeOf(c.mln_runtime_event_render_frame),
        .mode = c.MLN_RENDER_MODE_FULL,
        .needs_repaint = false,
        .placement_changed = false,
        .stats = stats,
    };
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_event_render_frame)), frame.size);
    try testing.expectEqual(c.MLN_RENDER_MODE_FULL, frame.mode);

    const tile = c.mln_runtime_event_tile_action{
        .size = @sizeOf(c.mln_runtime_event_tile_action),
        .operation = c.MLN_TILE_OPERATION_START_PARSE,
        .tile_id = .{
            .overscaled_z = 1,
            .wrap = 0,
            .canonical_z = 1,
            .canonical_x = 0,
            .canonical_y = 0,
        },
        .source_id = null,
        .source_id_size = 0,
    };
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_event_tile_action)), tile.size);
    try testing.expectEqual(c.MLN_TILE_OPERATION_START_PARSE, tile.operation);

    const offline_status = c.mln_runtime_event_offline_region_status{
        .size = @sizeOf(c.mln_runtime_event_offline_region_status),
        .region_id = 1,
        .status = .{
            .size = @sizeOf(c.mln_offline_region_status),
            .download_state = c.MLN_OFFLINE_REGION_DOWNLOAD_INACTIVE,
            .completed_resource_count = 0,
            .completed_resource_size = 0,
            .completed_tile_count = 0,
            .required_tile_count = 0,
            .completed_tile_size = 0,
            .required_resource_count = 0,
            .required_resource_count_is_precise = false,
            .complete = false,
        },
    };
    try testing.expectEqual(@as(u32, @sizeOf(c.mln_runtime_event_offline_region_status)), offline_status.size);
}

test "event message storage is copied into caller output" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_set_style_url(map, "unsupported://style.json"));

    var event = support.emptyEvent();
    for (0..1000) |_| {
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
        var has_event = false;
        try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
        if (has_event and
            event.type == c.MLN_RUNTIME_EVENT_MAP_LOADING_FAILED and
            event.source_type == c.MLN_RUNTIME_EVENT_SOURCE_MAP and
            event.source == @as(?*anyopaque, @ptrCast(map))) break;
        _ = usleep(1000);
    } else return error.EventNotFound;

    try testing.expectEqual(c.MLN_RUNTIME_EVENT_SOURCE_MAP, event.source_type);
    try testing.expect(event.source == @as(?*anyopaque, @ptrCast(map)));
    try testing.expectEqual(c.MLN_RUNTIME_EVENT_PAYLOAD_NONE, event.payload_type);
    try testing.expectEqual(@as(?*const anyopaque, null), event.payload);
    try testing.expectEqual(@as(usize, 0), event.payload_size);
    try testing.expect(event.message != null);
    const message = event.message[0..event.message_size];
    try testing.expect(message.len > 0);
    const copied_message = try testing.allocator.dupe(u8, message);
    defer testing.allocator.free(copied_message);

    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_run_once(runtime));
    try testing.expect(event.message != null);
    try testing.expectEqualSlices(u8, copied_message, event.message[0..event.message_size]);
}

test "destroying a map discards its queued runtime events" {
    try support.suppressLogs();
    defer support.restoreLogs();

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    var map_live = true;
    errdefer if (map_live) support.destroyMap(map);
    _ = try support.drainEvents(runtime);

    try testing.expectEqual(c.MLN_STATUS_NATIVE_ERROR, c.mln_map_set_style_json(map, "{"));
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_map_destroy(map));
    map_live = false;

    var event = support.emptyEvent();
    var has_event = true;
    try testing.expectEqual(c.MLN_STATUS_OK, c.mln_runtime_poll_event(runtime, &event, &has_event));
    try testing.expect(!has_event);
}
