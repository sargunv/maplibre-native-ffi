#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <stdbool.h>
#include <stdint.h>

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
