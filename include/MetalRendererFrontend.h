#pragma once

extern "C"
{
  // Opaque type for Metal renderer frontend
  struct MLN_MetalRendererFrontend;

  // Forward declarations
  struct MLN_MetalRendererBackend;
  struct MLN_MapObserver_Interface;

  // Metal-specific initialization parameters
  struct MLN_MetalRendererFrontend_Options
  {
    MLN_MetalRendererBackend *backend;
    MLN_MapObserver_Interface *observer;
    bool continuous_rendering;
  };

  // Create a new Metal renderer frontend
  auto MLN_MetalRendererFrontend_new(
    const MLN_MetalRendererFrontend_Options *options
  ) -> MLN_MetalRendererFrontend *;

  // Delete a Metal renderer frontend
  void MLN_MetalRendererFrontend_delete(MLN_MetalRendererFrontend *frontend);

  // Set the map observer for the frontend
  void MLN_MetalRendererFrontend_setObserver(
    MLN_MetalRendererFrontend *frontend, MLN_MapObserver_Interface *observer
  );

  // Update the frontend state (called before render)
  void MLN_MetalRendererFrontend_update(MLN_MetalRendererFrontend *frontend);

  // Render a new frame
  void MLN_MetalRendererFrontend_render(MLN_MetalRendererFrontend *frontend);

  // Enable/disable continuous rendering
  void MLN_MetalRendererFrontend_setContinuousRendering(
    MLN_MetalRendererFrontend *frontend, bool enabled
  );

  // Check if continuous rendering is enabled
  auto MLN_MetalRendererFrontend_isContinuousRendering(
    MLN_MetalRendererFrontend *frontend
  ) -> bool;
}
