#include "MetalRendererFrontend.h"
#include <cassert>
#include <cstdio>
#include "MetalRendererBackend.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace
{

void test_create_destroy()
{
  printf("Running create_destroy... ");

  // Create Metal resources
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  assert(device != nullptr && "Failed to create Metal device");

  id<MTLCommandQueue> commandQueue = [device newCommandQueue];
  assert(commandQueue != nullptr && "Failed to create command queue");

  CAMetalLayer *layer = [[CAMetalLayer alloc] init];
  assert(layer != nullptr && "Failed to create Metal layer");
  layer.device = device;

  // Create backend
  MLN_MetalRendererBackend_Options const backend_options = {
    .metal_device = (__bridge void *)device,
    .metal_layer = (__bridge void *)layer,
    .command_queue = (__bridge void *)commandQueue,
    .width = 800,
    .height = 600,
    .pixel_ratio = 2.0F
  };

  MLN_MetalRendererBackend *backend =
    MLN_MetalRendererBackend_new(&backend_options);
  assert(backend != nullptr && "Failed to create Metal backend");

  // Create frontend
  MLN_MetalRendererFrontend_Options const frontend_options = {
    .backend = backend, .observer = nullptr, .continuous_rendering = false
  };

  MLN_MetalRendererFrontend *frontend =
    MLN_MetalRendererFrontend_new(&frontend_options);
  assert(frontend != nullptr && "Failed to create Metal frontend");

  // Cleanup
  MLN_MetalRendererFrontend_delete(frontend);
  MLN_MetalRendererBackend_delete(backend);
  [layer release];
  [commandQueue release];

  printf("OK\n");
}

} // namespace

void run_metal_renderer_frontend_tests()
{
  test_create_destroy();
}
