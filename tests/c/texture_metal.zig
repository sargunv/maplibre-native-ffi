const builtin = @import("builtin");
const testing = @import("std").testing;
const support = @import("support.zig");
const common = @import("texture.zig");
const c = support.c;

extern "c" fn MTLCreateSystemDefaultDevice() ?*anyopaque;

const Backend = struct {
    pub const descriptor_size = @sizeOf(c.mln_metal_texture_descriptor);
    const expected_pixel_format_rgba8_unorm: u64 = 70;

    pub const AttachContext = struct {
        device: *anyopaque,

        pub fn init() !AttachContext {
            return .{ .device = MTLCreateSystemDefaultDevice() orelse return error.SkipZigTest };
        }

        pub fn deinit(_: *AttachContext) void {}

        pub fn descriptor(self: *const AttachContext) c.mln_metal_texture_descriptor {
            var value = c.mln_metal_texture_descriptor_default();
            value.device = self.device;
            return value;
        }
    };

    pub const Fixture = struct {
        texture: *c.mln_texture_session,
        context: ?AttachContext,

        pub fn create(map: *c.mln_map) !Fixture {
            var context = try AttachContext.init();
            var texture_descriptor = context.descriptor();
            texture_descriptor.width = 256;
            texture_descriptor.height = 256;

            var texture: ?*c.mln_texture_session = null;
            try testing.expectEqual(c.MLN_STATUS_OK, c.mln_metal_texture_attach(map, &texture_descriptor, &texture));
            return .{ .texture = texture orelse return error.TextureCreateFailed, .context = context };
        }

        pub fn descriptor(self: *const Fixture) c.mln_metal_texture_descriptor {
            return self.context.?.descriptor();
        }

        pub fn destroy(self: *Fixture) void {
            testing.expectEqual(c.MLN_STATUS_OK, c.mln_texture_destroy(self.texture)) catch @panic("texture destroy failed");
            self.context = null;
        }

        pub fn deinitContextOnly(self: *Fixture) void {
            self.context = null;
        }

        pub fn markDestroyed(self: *Fixture) void {
            self.context = null;
        }

        pub fn acquire(_: *Fixture, frame: *Frame) c.mln_status {
            return c.mln_metal_texture_acquire_frame(frame.texture, &frame.metal);
        }

        pub fn release(_: *Fixture, frame: *const Frame) c.mln_status {
            return c.mln_metal_texture_release_frame(frame.texture, &frame.metal);
        }
    };

    pub const Frame = struct {
        texture: *c.mln_texture_session,
        metal: c.mln_metal_texture_frame,

        pub fn empty(texture: *c.mln_texture_session) Frame {
            return .{
                .texture = texture,
                .metal = .{
                    .size = @sizeOf(c.mln_metal_texture_frame),
                    .generation = 0,
                    .width = 0,
                    .height = 0,
                    .scale_factor = 0,
                    .frame_id = 0,
                    .texture = null,
                    .device = null,
                    .pixel_format = 0,
                },
            };
        }

        pub fn bumpFrameId(self: *Frame) void {
            self.metal.frame_id += 1;
        }

        pub fn clearNativeHandles(self: *Frame) void {
            self.metal.texture = null;
            self.metal.device = null;
        }

        pub fn resetSize(self: *Frame) void {
            self.metal.size = @sizeOf(c.mln_metal_texture_frame);
        }
    };

    pub fn attach(map: ?*c.mln_map, descriptor: ?*const c.mln_metal_texture_descriptor, out_texture: ?*?*c.mln_texture_session) c.mln_status {
        return c.mln_metal_texture_attach(map, descriptor, out_texture);
    }

    pub fn clearRequiredHandle(descriptor: *c.mln_metal_texture_descriptor) void {
        descriptor.device = null;
    }

    pub fn expectInitialFrame(frame: *const Frame) !void {
        try testing.expect(frame.metal.texture != null);
        try testing.expect(frame.metal.device != null);
        try testing.expectEqual(expected_pixel_format_rgba8_unorm, frame.metal.pixel_format);
        try testing.expectEqual(@as(u32, 256), frame.metal.width);
        try testing.expectEqual(@as(u32, 256), frame.metal.height);
        try testing.expectEqual(@as(u64, 1), frame.metal.generation);
        try testing.expect(frame.metal.frame_id != 0);
    }

    pub fn expectResizedFrame(frame: *const Frame) !void {
        try testing.expectEqual(expected_pixel_format_rgba8_unorm, frame.metal.pixel_format);
        try testing.expectEqual(@as(u32, 256), frame.metal.width);
        try testing.expectEqual(@as(u32, 128), frame.metal.height);
        try testing.expectEqual(@as(f64, 2.0), frame.metal.scale_factor);
        try testing.expectEqual(@as(u64, 2), frame.metal.generation);
    }
};

test "Metal texture unsupported backend validates arguments" {
    if (builtin.os.tag == .macos) return error.SkipZigTest;

    const runtime = try support.createRuntime();
    defer support.destroyRuntime(runtime);

    const map = try support.createMap(runtime);
    defer support.destroyMap(map);

    var descriptor = c.mln_metal_texture_descriptor_default();
    descriptor.device = @ptrFromInt(1);

    var texture: ?*c.mln_texture_session = @ptrFromInt(1);
    try testing.expectEqual(c.MLN_STATUS_INVALID_ARGUMENT, c.mln_metal_texture_attach(map, &descriptor, &texture));
    try testing.expect(texture != null);

    texture = null;
    try testing.expectEqual(c.MLN_STATUS_UNSUPPORTED, c.mln_metal_texture_attach(map, &descriptor, &texture));
    try testing.expectEqual(@as(?*c.mln_texture_session, null), texture);
}

test "Metal texture attach rejects invalid arguments" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectAttachRejectsInvalidArguments(Backend);
}

test "Metal texture lifecycle enforces active session and stale handles" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectLifecycleEnforcesActiveSessionAndStaleHandles(Backend);
}

test "Metal texture rejects wrong-thread calls" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectWrongThreadCallsRejected(Backend);
}

test "Metal texture render acquire release and resize generation" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectRenderAcquireReleaseAndResizeGeneration(Backend);
}

test "Metal texture supports static render requests" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectStillModeRenderRequest(Backend, c.MLN_MAP_MODE_STATIC);
}

test "Metal texture supports tile render requests" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectStillModeRenderRequest(Backend, c.MLN_MAP_MODE_TILE);
}

test "Metal texture detach leaves handle live but unusable for rendering" {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    try common.expectDetachLeavesHandleLiveButUnusable(Backend);
}
