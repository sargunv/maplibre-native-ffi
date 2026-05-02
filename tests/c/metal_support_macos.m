#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct mln_test_window_metal_layer {
  void* window;
  void* layer;
} mln_test_window_metal_layer;

@interface MLNTestCountingMetalLayer : CAMetalLayer
@property(nonatomic) uint32_t nextDrawableCount;
@end

@implementation MLNTestCountingMetalLayer
- (id<CAMetalDrawable>)nextDrawable {
  _nextDrawableCount += 1;
  return [super nextDrawable];
}
@end

extern void* objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void* pool);

void* mln_test_autorelease_pool_push(void) {
  return objc_autoreleasePoolPush();
}

void mln_test_autorelease_pool_pop(void* pool) {
  objc_autoreleasePoolPop(pool);
}

void* mln_test_create_metal_layer(void) { return [CAMetalLayer layer]; }

void* mln_test_create_metal_texture(
  void* device, uint32_t width, uint32_t height
) {
  if (device == NULL || width == 0 || height == 0) {
    return NULL;
  }
  MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
    texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                 width:(NSUInteger)width
                                height:(NSUInteger)height
                             mipmapped:NO];
  descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  return [(id<MTLDevice>)device newTextureWithDescriptor:descriptor];
}

bool mln_test_metal_texture_clear_rgba8(
  void* texture, uint8_t r, uint8_t g, uint8_t b, uint8_t a
) {
  id<MTLTexture> metal_texture = (id<MTLTexture>)texture;
  if (metal_texture == nil) {
    return false;
  }

  id<MTLCommandQueue> queue = [[metal_texture device] newCommandQueue];
  if (queue == nil) {
    return false;
  }

  MTLRenderPassDescriptor* descriptor =
    [MTLRenderPassDescriptor renderPassDescriptor];
  MTLRenderPassColorAttachmentDescriptor* color_attachment =
    [[descriptor colorAttachments] objectAtIndexedSubscript:0];
  [color_attachment setTexture:metal_texture];
  [color_attachment setLoadAction:MTLLoadActionClear];
  [color_attachment setStoreAction:MTLStoreActionStore];
  [color_attachment setClearColor:MTLClearColorMake(
                                    (double)r / 255.0, (double)g / 255.0,
                                    (double)b / 255.0, (double)a / 255.0
                                  )];

  id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder =
    [command_buffer renderCommandEncoderWithDescriptor:descriptor];
  if (encoder == nil) {
    [queue release];
    return false;
  }
  [encoder endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilCompleted];
  const bool success =
    [command_buffer status] == MTLCommandBufferStatusCompleted;
  [queue release];
  return success;
}

bool mln_test_metal_texture_read_pixel_rgba8(
  void* texture, uint32_t x, uint32_t y, uint8_t* out_rgba
) {
  id<MTLTexture> metal_texture = (id<MTLTexture>)texture;
  if (
    metal_texture == nil || out_rgba == NULL || x >= [metal_texture width] ||
    y >= [metal_texture height]
  ) {
    return false;
  }

  id<MTLCommandQueue> queue = [[metal_texture device] newCommandQueue];
  if (queue == nil) {
    return false;
  }
  const NSUInteger bytes_per_row = 256;
  id<MTLBuffer> buffer =
    [[metal_texture device] newBufferWithLength:bytes_per_row
                                        options:MTLResourceStorageModeShared];
  if (buffer == nil) {
    [queue release];
    return false;
  }

  id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
  id<MTLBlitCommandEncoder> encoder = [command_buffer blitCommandEncoder];
  [encoder copyFromTexture:metal_texture
                 sourceSlice:0
                 sourceLevel:0
                sourceOrigin:MTLOriginMake(x, y, 0)
                  sourceSize:MTLSizeMake(1, 1, 1)
                    toBuffer:buffer
           destinationOffset:0
      destinationBytesPerRow:bytes_per_row
    destinationBytesPerImage:bytes_per_row];
  [encoder endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilCompleted];

  const bool success =
    [command_buffer status] == MTLCommandBufferStatusCompleted;
  if (success) {
    memcpy(out_rgba, [buffer contents], 4);
  }
  [buffer release];
  [queue release];
  return success;
}

void mln_test_release_metal_object(void* object) { [(id)object release]; }

static bool create_window_metal_layer(
  uint32_t width, uint32_t height, bool counted,
  mln_test_window_metal_layer* out_layer
) {
  if (out_layer == NULL || width == 0 || height == 0) {
    return false;
  }

  NSWindow* window = nil;
  @try {
    [NSApplication sharedApplication];

    const NSRect frame = NSMakeRect(0.0, 0.0, (CGFloat)width, (CGFloat)height);
    window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:NSWindowStyleMaskBorderless
                                           backing:NSBackingStoreBuffered
                                             defer:YES];
    if (window == nil) {
      return false;
    }

    [window setReleasedWhenClosed:NO];
    NSView* content_view = [window contentView];
    CAMetalLayer* layer =
      counted ? [MLNTestCountingMetalLayer layer] : [CAMetalLayer layer];
    if (content_view == nil || layer == nil) {
      [window release];
      return false;
    }

    [content_view setWantsLayer:YES];
    [content_view setLayer:layer];
    out_layer->window = window;
    out_layer->layer = layer;
    return true;
  } @catch (NSException* exception) {
    (void)exception;
    [window release];
    return false;
  }
}

bool mln_test_create_window_metal_layer(
  uint32_t width, uint32_t height, mln_test_window_metal_layer* out_layer
) {
  return create_window_metal_layer(width, height, false, out_layer);
}

bool mln_test_create_counting_window_metal_layer(
  uint32_t width, uint32_t height, mln_test_window_metal_layer* out_layer
) {
  return create_window_metal_layer(width, height, true, out_layer);
}

uint32_t mln_test_metal_layer_next_drawable_count(void* layer) {
  id object = (id)layer;
  if (![object isKindOfClass:[MLNTestCountingMetalLayer class]]) {
    return 0;
  }
  return [(MLNTestCountingMetalLayer*)object nextDrawableCount];
}

void mln_test_destroy_window_metal_layer(
  mln_test_window_metal_layer* window_layer
) {
  if (window_layer == NULL) {
    return;
  }
  [(NSWindow*)window_layer->window release];
  window_layer->window = NULL;
  window_layer->layer = NULL;
}
