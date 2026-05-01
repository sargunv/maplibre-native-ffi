const builtin = @import("builtin");

const objc = struct {
    extern "c" fn objc_getClass(name: [*:0]const u8) ?*anyopaque;
    extern "c" fn sel_registerName(name: [*:0]const u8) ?*anyopaque;
    extern "c" fn objc_msgSend(receiver: ?*anyopaque, selector: ?*anyopaque) ?*anyopaque;
};

pub fn createLayer() !*anyopaque {
    if (builtin.os.tag != .macos) return error.SkipZigTest;
    const cls = objc.objc_getClass("CAMetalLayer") orelse return error.MetalLayerUnavailable;
    const sel = objc.sel_registerName("layer") orelse return error.MetalLayerUnavailable;
    return objc.objc_msgSend(cls, sel) orelse return error.MetalLayerUnavailable;
}
