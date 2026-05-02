const std = @import("std");

pub const AppError = error{
    InvalidArguments,
    SdlInitFailed,
    WindowCreateFailed,
    RuntimeCreateFailed,
    MapCreateFailed,
    TextureAttachFailed,
    StyleLoadFailed,
    CameraJumpFailed,
    CameraCommandFailed,
    TextureResizeFailed,
    TextureRenderFailed,
    SurfaceAttachFailed,
    SurfaceResizeFailed,
    SurfaceRenderFailed,
    BackendSetupFailed,
    BackendDrawFailed,
};

pub const Viewport = struct {
    logical_width: u32,
    logical_height: u32,
    physical_width: u32,
    physical_height: u32,
    scale_factor: f64,
};

pub const RenderTargetMode = enum {
    owned_texture,
    borrowed_texture,
    native_surface,

    pub fn parse(value: []const u8) ?RenderTargetMode {
        if (std.mem.eql(u8, value, "owned-texture")) return .owned_texture;
        if (std.mem.eql(u8, value, "borrowed-texture")) return .borrowed_texture;
        if (std.mem.eql(u8, value, "native-surface")) return .native_surface;
        return null;
    }

    pub fn label(self: RenderTargetMode) []const u8 {
        return switch (self) {
            .owned_texture => "owned-texture",
            .borrowed_texture => "borrowed-texture",
            .native_surface => "native-surface",
        };
    }
};
