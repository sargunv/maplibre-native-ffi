#pragma once

#include <cstdint>

extern "C"
{

  // Opaque type for Metal-specific data
  struct MLN_MetalRendererBackend;

  // Metal-specific initialization parameters
  struct MLN_MetalRendererBackend_Options
  {
    void *metal_device;  // MTLDevice*
    void *metal_layer;   // CAMetalLayer*
    void *command_queue; // MTLCommandQueue*
    std::uint32_t width;
    std::uint32_t height;
    float pixel_ratio;
  };

  // Create a new Metal renderer backend
  auto MLN_MetalRendererBackend_new(
    const MLN_MetalRendererBackend_Options *options
  ) -> MLN_MetalRendererBackend *;

  // Get the Metal device from the backend
  auto MLN_MetalRendererBackend_getDevice(MLN_MetalRendererBackend *backend)
    -> void *; // Returns MTLDevice*

  // Get the Metal command queue from the backend
  auto MLN_MetalRendererBackend_getCommandQueue(
    MLN_MetalRendererBackend *backend
  ) -> void *; // Returns MTLCommandQueue*

  // Get the Metal layer from the backend
  auto MLN_MetalRendererBackend_getLayer(MLN_MetalRendererBackend *backend)
    -> void *; // Returns CAMetalLayer*

  // Delete a Metal renderer backend
  void MLN_MetalRendererBackend_delete(MLN_MetalRendererBackend *backend);

  // Set the size of the Metal renderer backend
  void MLN_MetalRendererBackend_setSize(
    MLN_MetalRendererBackend *backend, std::uint32_t width, std::uint32_t height
  );
}
