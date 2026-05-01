const c = @import("c.zig").c;
const diagnostics = @import("diagnostics.zig");
const types = @import("types.zig");

pub const TextureMode = enum {
    native,
    shared,
};

pub const TextureSession = struct {
    handle: *c.mln_texture_session,
    mode: TextureMode,
};

pub const Session = union(enum) {
    texture: TextureSession,
    surface: *c.mln_surface_session,

    pub fn deinit(self: *Session) void {
        switch (self.*) {
            .texture => |texture| _ = c.mln_texture_destroy(texture.handle),
            .surface => |surface| _ = c.mln_surface_destroy(surface),
        }
    }

    pub fn resize(self: *Session, viewport: types.Viewport) !void {
        const status = switch (self.*) {
            .texture => |texture| c.mln_texture_resize(
                texture.handle,
                viewport.logical_width,
                viewport.logical_height,
                viewport.scale_factor,
            ),
            .surface => |surface| c.mln_surface_resize(
                surface,
                viewport.logical_width,
                viewport.logical_height,
                viewport.scale_factor,
            ),
        };
        if (status == c.MLN_STATUS_OK) return;
        switch (self.*) {
            .texture => {
                diagnostics.logAbiError("texture resize failed");
                return types.AppError.TextureResizeFailed;
            },
            .surface => {
                diagnostics.logAbiError("surface resize failed");
                return types.AppError.SurfaceResizeFailed;
            },
        }
    }

    pub fn renderUpdate(self: *Session) !bool {
        const status = switch (self.*) {
            .texture => |texture| c.mln_texture_render_update(texture.handle),
            .surface => |surface| c.mln_surface_render_update(surface),
        };
        if (status == c.MLN_STATUS_OK) return true;
        if (status == c.MLN_STATUS_INVALID_STATE) return false;
        switch (self.*) {
            .texture => {
                diagnostics.logAbiError("texture render failed");
                return types.AppError.TextureRenderFailed;
            },
            .surface => {
                diagnostics.logAbiError("surface render failed");
                return types.AppError.SurfaceRenderFailed;
            },
        }
    }
};
