#include <memory>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/offscreen_texture.hpp>
#include <mbgl/mtl/context.hpp>
#include <mbgl/mtl/renderable_resource.hpp>
#include <mbgl/mtl/texture2d.hpp>

#include <Metal/MTLBlitPass.hpp>
#include <Metal/MTLCommandBuffer.hpp>
#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLRenderPass.hpp>

#include "render/metal/metal_texture_backend.inc"

namespace mln::core {

class MetalTextureBackend::MetalTextureRenderableResource final
    : public mbgl::mtl::RenderableResource {
 public:
  MetalTextureRenderableResource(
    MetalTextureBackend& backend_, mbgl::mtl::Context& context_,
    mbgl::Size size_, MTL::Texture* borrowed_texture_
  )
      : backend(backend_),
        context(context_),
        size(size_),
        borrowedTexture(borrowed_texture_) {
    if (borrowedTexture == nullptr) {
      offscreenTexture = context.createOffscreenTexture(
        size, mbgl::gfx::TextureChannelDataType::UnsignedByte, true, true
      );
      return;
    }

    depthTexture = context.createTexture2D();
    depthTexture->setSize(size);
    depthTexture->setFormat(
      mbgl::gfx::TexturePixelType::Depth,
      mbgl::gfx::TextureChannelDataType::Float
    );
    depthTexture->setSamplerConfiguration(
      {.filter = mbgl::gfx::TextureFilterType::Linear,
       .wrapU = mbgl::gfx::TextureWrapType::Clamp,
       .wrapV = mbgl::gfx::TextureWrapType::Clamp}
    );
    static_cast<mbgl::mtl::Texture2D*>(depthTexture.get())
      ->setUsage(
        MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite |
        MTL::TextureUsageRenderTarget
      );

#if !TARGET_OS_SIMULATOR
    stencilTexture = context.createTexture2D();
    stencilTexture->setSize(size);
    stencilTexture->setFormat(
      mbgl::gfx::TexturePixelType::Stencil,
      mbgl::gfx::TextureChannelDataType::UnsignedByte
    );
    stencilTexture->setSamplerConfiguration(
      {.filter = mbgl::gfx::TextureFilterType::Linear,
       .wrapU = mbgl::gfx::TextureWrapType::Clamp,
       .wrapV = mbgl::gfx::TextureWrapType::Clamp}
    );
    static_cast<mbgl::mtl::Texture2D*>(stencilTexture.get())
      ->setUsage(
        MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite |
        MTL::TextureUsageRenderTarget
      );
#endif

    context.renderingStats().numFrameBuffers++;
  }

  ~MetalTextureRenderableResource() override {
    if (borrowedTexture != nullptr) {
      context.renderingStats().numFrameBuffers--;
    }
  }

  void bind() override {
    if (offscreenTexture != nullptr) {
      offscreenTexture->getResource<mbgl::mtl::RenderableResource>().bind();
      return;
    }

    assert(context.getBackend().getCommandQueue());
    commandBuffer =
      NS::RetainPtr(context.getBackend().getCommandQueue()->commandBuffer());
    renderPassDescriptor =
      NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
    if (
      auto* colorTarget = renderPassDescriptor->colorAttachments()->object(0)
    ) {
      colorTarget->setTexture(borrowedTexture);
    }
    if (depthTexture != nullptr) {
      depthTexture->create();
      if (auto* depthTarget = renderPassDescriptor->depthAttachment()) {
        depthTarget->setTexture(
          static_cast<mbgl::mtl::Texture2D*>(depthTexture.get())
            ->getMetalTexture()
        );
      }
    }
    if (stencilTexture != nullptr) {
      stencilTexture->create();
      if (auto* stencilTarget = renderPassDescriptor->stencilAttachment()) {
        stencilTarget->setTexture(
          static_cast<mbgl::mtl::Texture2D*>(stencilTexture.get())
            ->getMetalTexture()
        );
      }
    }
  }

  void swap() override {
    if (offscreenTexture != nullptr) {
      offscreenTexture->getResource<mbgl::mtl::RenderableResource>().swap();
      return;
    }

    assert(commandBuffer);
    commandBuffer->commit();
    commandBuffer->waitUntilCompleted();
    commandBuffer.reset();
    renderPassDescriptor.reset();
  }

  auto readStillImage() -> mbgl::PremultipliedImage {
    if (offscreenTexture == nullptr) {
      return {};
    }
    return offscreenTexture->readStillImage();
  }

  auto metal_texture() -> MTL::Texture* {
    if (borrowedTexture != nullptr) {
      return borrowedTexture;
    }
    return static_cast<mbgl::mtl::Texture2D*>(
             offscreenTexture->getTexture().get()
    )
      ->getMetalTexture();
  }

  [[nodiscard]] auto getBackend() const
    -> const mbgl::mtl::RendererBackend& override {
    return backend;
  }

  [[nodiscard]] auto getCommandBuffer() const
    -> const mbgl::mtl::MTLCommandBufferPtr& override {
    if (offscreenTexture != nullptr) {
      return offscreenTexture->getResource<mbgl::mtl::RenderableResource>()
        .getCommandBuffer();
    }
    return commandBuffer;
  }

  [[nodiscard]] auto getUploadPassDescriptor() const
    -> mbgl::mtl::MTLBlitPassDescriptorPtr override {
    if (offscreenTexture != nullptr) {
      return offscreenTexture->getResource<mbgl::mtl::RenderableResource>()
        .getUploadPassDescriptor();
    }
    return NS::TransferPtr(MTL::BlitPassDescriptor::alloc()->init());
  }

  [[nodiscard]] auto getRenderPassDescriptor() const
    -> const mbgl::mtl::MTLRenderPassDescriptorPtr& override {
    if (offscreenTexture != nullptr) {
      return offscreenTexture->getResource<mbgl::mtl::RenderableResource>()
        .getRenderPassDescriptor();
    }
    assert(renderPassDescriptor);
    return renderPassDescriptor;
  }

 private:
  MetalTextureBackend& backend;
  mbgl::mtl::Context& context;
  mbgl::Size size;
  MTL::Texture* borrowedTexture = nullptr;
  std::unique_ptr<mbgl::gfx::OffscreenTexture> offscreenTexture;
  mbgl::gfx::Texture2DPtr depthTexture;
  mbgl::gfx::Texture2DPtr stencilTexture;
  mbgl::mtl::MTLCommandBufferPtr commandBuffer;
  mbgl::mtl::MTLRenderPassDescriptorPtr renderPassDescriptor;
};

MetalTextureBackend::MetalTextureBackend(
  MTL::Device* host_device, mbgl::Size size
)
    : mbgl::mtl::RendererBackend(mbgl::gfx::ContextMode::Unique),
      mbgl::gfx::HeadlessBackend(size) {
  device = NS::RetainPtr(host_device);
  commandQueue = NS::TransferPtr(device->newCommandQueue());
}

MetalTextureBackend::MetalTextureBackend(
  MTL::Texture* borrowed_texture, mbgl::Size size
)
    : mbgl::mtl::RendererBackend(mbgl::gfx::ContextMode::Unique),
      mbgl::gfx::HeadlessBackend(size),
      borrowed_texture_(borrowed_texture) {
  device = NS::RetainPtr(borrowed_texture->device());
  commandQueue = NS::TransferPtr(device->newCommandQueue());
}

MetalTextureBackend::~MetalTextureBackend() {
  auto guard = mbgl::gfx::BackendScope{
    *this, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  resource.reset();
  context.reset();
}

auto MetalTextureBackend::getDefaultRenderable() -> mbgl::gfx::Renderable& {
  if (!resource) {
    resource = std::make_unique<MetalTextureRenderableResource>(
      *this, static_cast<mbgl::mtl::Context&>(getContext()), size,
      borrowed_texture_
    );
  }
  return *this;
}

auto MetalTextureBackend::readStillImage() -> mbgl::PremultipliedImage {
  return getResource<MetalTextureRenderableResource>().readStillImage();
}

auto MetalTextureBackend::getRendererBackend() -> mbgl::gfx::RendererBackend* {
  return this;
}

void MetalTextureBackend::activate() {}

void MetalTextureBackend::deactivate() {}

void MetalTextureBackend::updateAssumedState() {}

auto MetalTextureBackend::metal_texture() -> MTL::Texture* {
  getDefaultRenderable();
  return getResource<MetalTextureRenderableResource>().metal_texture();
}

}  // namespace mln::core
