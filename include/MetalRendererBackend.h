#pragma once

#ifdef __OBJC__
@class MTKView;
#else
using MTKView = void;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  using MLN_MetalRendererBackend = void *;

  auto MLN_MetalRendererBackend_new(MTKView *view) -> MLN_MetalRendererBackend;

  auto MLN_MetalRendererBackend_delete(MLN_MetalRendererBackend backend)
    -> void;

#ifdef __cplusplus
}
#endif
