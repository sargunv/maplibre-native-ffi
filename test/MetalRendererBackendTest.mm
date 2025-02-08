#include "MetalRendererBackend.h"
#include <cassert>
#include <cstdio>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

void run_metal_renderer_backend_tests()
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

  // Create backend options
  MLN_MetalRendererBackend_Options options = {
    .metal_device = (__bridge void *)device,
    .metal_layer = (__bridge void *)layer,
    .command_queue = (__bridge void *)commandQueue,
    .width = 800,
    .height = 600,
    .pixel_ratio = 2.0F
  };

  // Create backend
  auto *backend = MLN_MetalRendererBackend_new(&options);
  assert(backend != nullptr && "Failed to create Metal backend");

  // Test getting Metal resources
  auto *retrieved_device = MLN_MetalRendererBackend_getDevice(backend);
  assert(retrieved_device == (__bridge void *)device && "Device mismatch");

  auto *retrieved_queue = MLN_MetalRendererBackend_getCommandQueue(backend);
  assert(
    retrieved_queue == (__bridge void *)commandQueue && "Command queue mismatch"
  );

  auto *retrieved_layer = MLN_MetalRendererBackend_getLayer(backend);
  assert(retrieved_layer == (__bridge void *)layer && "Layer mismatch");

  // Test resizing
  MLN_MetalRendererBackend_setSize(backend, 1024, 768);

  // Cleanup
  MLN_MetalRendererBackend_delete(backend);
  [layer release];
  [commandQueue release];

  printf("OK\n");
}
