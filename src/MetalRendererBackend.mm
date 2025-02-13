#include "MetalRendererBackend.h"

#include <mbgl/gfx/renderable.hpp>
#include <mbgl/mtl/renderer_backend.hpp>

#import <MetalKit/MetalKit.h>

namespace
{

class MLNMetalRenderableResource final : public mbgl::gfx::RenderableResource
{
public:
  explicit MLNMetalRenderableResource(MTKView *view) : view_(view)
  {
  }

  void bind() override
  {
    // Nothing to do - MTKView handles this
  }

private:
  MTKView *view_;
};

class MLNMetalRenderable final : public mbgl::gfx::Renderable
{
public:
  MLNMetalRenderable(
    mbgl::Size size, std::unique_ptr<mbgl::gfx::RenderableResource> resource
  )
      : mbgl::gfx::Renderable(size, std::move(resource))
  {
  }

  void setSize(mbgl::Size newSize)
  {
    size = newSize;
  }
};

class MLNMetalBackend final : public mbgl::mtl::RendererBackend
{
public:
  MLNMetalBackend(MTKView *view)
      : mbgl::mtl::RendererBackend(mbgl::gfx::ContextMode::Unique), view_(view)
  {
    // Configure the Metal view
    if (view.device == nil)
    {
      view.device = MTLCreateSystemDefaultDevice();
    }
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    view.sampleCount = 1;
    view.framebufferOnly = YES;

    // Store Metal device and create command queue
    device = (__bridge id<MTLDevice>)view.device;
    commandQueue = (__bridge id<MTLCommandQueue>)[device newCommandQueue];

    // Create renderable
    renderable_ = std::make_unique<MLNMetalRenderable>(
      mbgl::Size{
        static_cast<uint32_t>(view.drawableSize.width),
        static_cast<uint32_t>(view.drawableSize.height)
      },
      std::make_unique<MLNMetalRenderableResource>(view)
    );
  }

  ~MLNMetalBackend() override = default;

  auto getDefaultRenderable() -> mbgl::gfx::Renderable & override
  {
    return *renderable_;
  }

  void activate() override
  {
  }
  void deactivate() override
  {
  }
  void updateAssumedState() override
  {
  }

  void setSize(mbgl::Size size)
  {
    static_cast<MLNMetalRenderable *>(renderable_.get())->setSize(size);
    view_.drawableSize = CGSizeMake(
      static_cast<CGFloat>(size.width), static_cast<CGFloat>(size.height)
    );
  }

  [[nodiscard]] auto getSize() const -> mbgl::Size
  {
    return renderable_->getSize();
  }

  void invalidate()
  {
    [view_ setNeedsDisplay:YES];
  }

private:
  MTKView *view_;
  std::unique_ptr<MLNMetalRenderable> renderable_;
};

} // namespace

extern "C"
{

  auto MLN_MetalRendererBackend_new(MTKView *view) -> MLN_MetalRendererBackend
  {
    try
    {
      return new MLNMetalBackend(view);
    }
    catch (...)
    {
      return nullptr;
    }
  }

  void MLN_MetalRendererBackend_delete(MLN_MetalRendererBackend backend)
  {
    delete static_cast<MLNMetalBackend *>(backend);
  }

} // extern "C"
