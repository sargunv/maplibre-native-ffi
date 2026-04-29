pub const AppError = error{
    SdlInitFailed,
    WindowCreateFailed,
    RuntimeCreateFailed,
    MapCreateFailed,
    TextureAttachFailed,
    StyleLoadFailed,
    CameraJumpFailed,
    TextureResizeFailed,
    TextureRenderFailed,
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
