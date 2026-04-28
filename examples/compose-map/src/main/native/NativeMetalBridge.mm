#include <cstdint>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <jni.h>

namespace {

struct MetalTextureState {
  id<MTLDevice> device;
  id<MTLCommandQueue> queue;
  id<MTLTexture> texture;
  id<MTLLibrary> library;
  id<MTLRenderPipelineState> pipeline;
  uint32_t width;
  uint32_t height;
};

auto shaderSource() -> NSString* {
  return @"#include <metal_stdlib>\n"
          "using namespace metal;\n"
          "struct VertexOut { float4 position [[position]]; float3 color; };\n"
          "vertex VertexOut vertex_main(uint vertex_id [[vertex_id]], constant "
          "float& phase [[buffer(0)]]) {\n"
          "  float2 positions[3] = { float2(-0.82, -0.70), float2(0.86, "
          "-0.54), float2(0.20, 0.78) };\n"
          "  float3 colors[3] = { float3(0.12 + phase * 0.35, 0.92, 0.74), "
          "float3(1.0, 0.76, 0.18), float3(0.28, 0.55, 1.0) };\n"
          "  VertexOut out;\n"
          "  out.position = float4(positions[vertex_id], 0.0, 1.0);\n"
          "  out.color = colors[vertex_id];\n"
          "  return out;\n"
          "}\n"
          "fragment float4 fragment_main(VertexOut in [[stage_in]]) { return "
          "float4(in.color, 1.0); }\n";
}

auto makePipeline(id<MTLDevice> device) -> id<MTLRenderPipelineState> {
  NSError* error = nil;
  id<MTLLibrary> library = [device newLibraryWithSource:shaderSource()
                                                options:nil
                                                  error:&error];
  if (library == nil) {
    NSLog(@"Failed to create Metal library: %@", error);
    return nil;
  }

  id<MTLFunction> vertex = [library newFunctionWithName:@"vertex_main"];
  id<MTLFunction> fragment = [library newFunctionWithName:@"fragment_main"];
  MTLRenderPipelineDescriptor* descriptor =
    [[MTLRenderPipelineDescriptor alloc] init];
  descriptor.vertexFunction = vertex;
  descriptor.fragmentFunction = fragment;
  descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

  id<MTLRenderPipelineState> pipeline =
    [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
  if (pipeline == nil) {
    NSLog(@"Failed to create Metal pipeline: %@", error);
  }
  return pipeline;
}

auto createState(uint32_t width, uint32_t height) -> MetalTextureState* {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) return nullptr;

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                   width:width
                                  height:height
                               mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModePrivate;

    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLRenderPipelineState> pipeline = makePipeline(device);
    if (texture == nil || queue == nil || pipeline == nil) return nullptr;

    auto* state = new MetalTextureState{
      .device = device,
      .queue = queue,
      .texture = texture,
      .library = nil,
      .pipeline = pipeline,
      .width = width,
      .height = height,
    };
    return state;
  }
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_NativeMetalBridge_create(JNIEnv*, jclass, jint width, jint height) {
  if (width <= 0 || height <= 0) return 0;
  return reinterpret_cast<jlong>(
    createState(static_cast<uint32_t>(width), static_cast<uint32_t>(height))
  );
}

extern "C" JNIEXPORT void JNICALL
Java_NativeMetalBridge_dispose(JNIEnv*, jclass, jlong handle) {
  auto* state = reinterpret_cast<MetalTextureState*>(handle);
  delete state;
}

extern "C" JNIEXPORT jlong JNICALL
Java_NativeMetalBridge_texturePtr(JNIEnv*, jclass, jlong handle) {
  auto* state = reinterpret_cast<MetalTextureState*>(handle);
  if (state == nullptr) return 0;
  return reinterpret_cast<jlong>((__bridge void*)state->texture);
}

extern "C" JNIEXPORT void JNICALL
Java_NativeMetalBridge_render(JNIEnv*, jclass, jlong handle, jint frame) {
  auto* state = reinterpret_cast<MetalTextureState*>(handle);
  if (state == nullptr) return;

  @autoreleasepool {
    const auto phase = static_cast<float>((frame % 240) / 239.0f);
    MTLRenderPassDescriptor* pass =
      [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = state->texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(
      0.04 + 0.18 * phase, 0.08, 0.18 + 0.36 * (1.0 - phase), 1.0
    );

    id<MTLCommandBuffer> commandBuffer = [state->queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder =
      [commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:state->pipeline];
    float phaseCopy = phase;
    [encoder setVertexBytes:&phaseCopy length:sizeof(phaseCopy) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:3];
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
  }
}
