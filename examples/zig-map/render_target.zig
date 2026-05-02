const c = @import("c.zig").c;
const diagnostics = @import("diagnostics.zig");
const types = @import("types.zig");

pub const Session = union(enum) {
    none,
    texture: *c.mln_texture_session,
    surface: *c.mln_surface_session,

    pub fn deinit(self: *Session) void {
        switch (self.*) {
            .none => {},
            .texture => |texture| _ = c.mln_texture_destroy(texture),
            .surface => |surface| _ = c.mln_surface_destroy(surface),
        }
        self.* = .none;
    }

    pub fn resize(self: *Session, viewport: types.Viewport) !void {
        const status = switch (self.*) {
            .none => c.MLN_STATUS_INVALID_STATE,
            .texture => |texture| c.mln_texture_resize(
                texture,
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
            .none, .texture => {
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
            .none => c.MLN_STATUS_INVALID_STATE,
            .texture => |texture| c.mln_texture_render_update(texture),
            .surface => |surface| c.mln_surface_render_update(surface),
        };
        if (status == c.MLN_STATUS_OK) return true;
        if (status == c.MLN_STATUS_INVALID_STATE) return false;
        switch (self.*) {
            .none, .texture => {
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
