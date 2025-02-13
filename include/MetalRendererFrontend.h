#pragma once

#include "MetalRendererBackend.h"

#ifdef __cplusplus
extern "C"
{
#endif

  using MLN_MetalRendererFrontend = void *;

  auto MLN_MetalRendererFrontend_new(MLN_MetalRendererBackend backend)
    -> MLN_MetalRendererFrontend;

  auto MLN_MetalRendererFrontend_delete(MLN_MetalRendererFrontend frontend)
    -> void;

#ifdef __cplusplus
}
#endif
