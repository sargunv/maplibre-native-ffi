#include "MetalRendererBackend.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <memory>

class MLN_MetalRendererBackend
{
public:
  MLN_MetalRendererBackend(const MLN_MetalRendererBackend_Options *options)
      : device_((__bridge id<MTLDevice>)options->metal_device),
        layer_((__bridge CAMetalLayer *)options->metal_layer),
        commandQueue_((__bridge id<MTLCommandQueue>)options->command_queue),
        width_(options->width), height_(options->height),
        pixelRatio_(options->pixel_ratio)
  {
    // Validate Metal resources
    if ((device_ == nullptr) || (layer_ == nullptr) || (commandQueue_ == nullptr))
    {
      throw std::runtime_error("Invalid Metal resources");
    }

    // Configure metal layer
    layer_.device = device_;
    layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer_.framebufferOnly = YES;
    updateDrawableSize();
  }

  void updateDrawableSize()
  {
    CGSize const size = CGSizeMake(
      static_cast<CGFloat>(width_) * pixelRatio_,
      static_cast<CGFloat>(height_) * pixelRatio_
    );
    layer_.drawableSize = size;
  }

  void setSize(std::uint32_t width, std::uint32_t height) noexcept
  {
    width_ = width;
    height_ = height;
    updateDrawableSize();
  }

  [[nodiscard]] auto device() const -> id<MTLDevice>
  {
    return device_;
  }
  [[nodiscard]] auto layer() const -> CAMetalLayer *
  {
    return layer_;
  }
  [[nodiscard]] auto commandQueue() const -> id<MTLCommandQueue>
  {
    return commandQueue_;
  }

private:
  id<MTLDevice> device_;
  CAMetalLayer *layer_;
  id<MTLCommandQueue> commandQueue_;
  std::uint32_t width_;
  std::uint32_t height_;
  float pixelRatio_;
};

// C API Implementation

extern "C"
{

  auto MLN_MetalRendererBackend_new(
    const MLN_MetalRendererBackend_Options *options
  ) -> MLN_MetalRendererBackend *
  {
    try
    {
      auto backend = std::make_unique<MLN_MetalRendererBackend>(options);
      return reinterpret_cast<MLN_MetalRendererBackend *>(backend.release());
    }
    catch (const std::exception &e)
    {
      // TODO: Add proper error handling
      return nullptr;
    }
  }

  auto MLN_MetalRendererBackend_getDevice(MLN_MetalRendererBackend *backend)
    -> void *
  {
    auto *metal_backend = backend;
    return (__bridge void *)metal_backend->device();
  }

  auto MLN_MetalRendererBackend_getCommandQueue(
    MLN_MetalRendererBackend *backend
  ) -> void *
  {
    auto *metal_backend = backend;
    return (__bridge void *)metal_backend->commandQueue();
  }

  auto MLN_MetalRendererBackend_getLayer(MLN_MetalRendererBackend *backend)
    -> void *
  {
    auto *metal_backend = backend;
    return (__bridge void *)metal_backend->layer();
  }

  void MLN_MetalRendererBackend_delete(MLN_MetalRendererBackend *backend)
  {
    delete backend;
  }

  void MLN_MetalRendererBackend_setSize(
    MLN_MetalRendererBackend *backend, std::uint32_t width, std::uint32_t height
  )
  {
    auto *metal_backend = backend;
    metal_backend->setSize(width, height);
  }
}
