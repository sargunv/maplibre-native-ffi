const builtin = @import("builtin");

pub const AutoreleasePool = struct {
    token: *anyopaque,

    pub fn init() !AutoreleasePool {
        if (builtin.os.tag != .macos) return error.SkipZigTest;
        return .{ .token = mln_test_autorelease_pool_push() orelse return error.AutoreleasePoolUnavailable };
    }

    pub fn deinit(self: AutoreleasePool) void {
        mln_test_autorelease_pool_pop(self.token);
    }
};

pub const WindowLayer = extern struct {
    window: ?*anyopaque,
    layer: ?*anyopaque,

    pub fn deinit(self: *WindowLayer) void {
        mln_test_destroy_window_metal_layer(self);
    }
};

extern "c" fn mln_test_autorelease_pool_push() ?*anyopaque;
extern "c" fn mln_test_autorelease_pool_pop(pool: *anyopaque) void;
extern "c" fn mln_test_create_metal_layer() ?*anyopaque;
extern "c" fn mln_test_create_window_metal_layer(width: u32, height: u32, out_layer: *WindowLayer) bool;
extern "c" fn mln_test_destroy_window_metal_layer(window_layer: *WindowLayer) void;

pub fn createLayer() !*anyopaque {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    return mln_test_create_metal_layer() orelse return error.MetalLayerUnavailable;
}

pub fn createWindowLayer(width: u32, height: u32) !WindowLayer {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    var window_layer = WindowLayer{ .window = null, .layer = null };
    if (!mln_test_create_window_metal_layer(width, height, &window_layer) or window_layer.layer == null) {
        return error.MetalWindowUnavailable;
    }
    return window_layer;
}
