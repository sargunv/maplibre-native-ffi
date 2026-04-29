#include <memory>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/offscreen_texture.hpp>
#include <mbgl/mtl/context.hpp>
#include <mbgl/mtl/renderable_resource.hpp>
#include <mbgl/mtl/texture2d.hpp>

#include <Metal/MTLBlitPass.hpp>
#include <Metal/MTLCommandQueue.hpp>

#include "render/metal/metal_texture_backend.inc"

namespace mln::core {

class MetalTextureBackend::MetalTextureRenderableResource final
    : public mbgl::mtl::RenderableResource {
 public:
  MetalTextureRenderableResource(
    MetalTextureBackend& backend_, mbgl::mtl::Context& context_,
    mbgl::Size size_
  )
      : backend(backend_), context(context_) {
    offscreenTexture = context.createOffscreenTexture(
      size_, mbgl::gfx::TextureChannelDataType::UnsignedByte, true, true
    );
  }

  void bind() override {
    offscreenTexture->getResource<mbgl::mtl::RenderableResource>().bind();
  }

  void swap() override {
    offscreenTexture->getResource<mbgl::mtl::RenderableResource>().swap();
  }

  auto readStillImage() -> mbgl::PremultipliedImage {
    return offscreenTexture->readStillImage();
  }

  auto metal_texture() -> MTL::Texture* {
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
    return offscreenTexture->getResource<mbgl::mtl::RenderableResource>()
      .getCommandBuffer();
  }

  [[nodiscard]] auto getUploadPassDescriptor() const
    -> mbgl::mtl::MTLBlitPassDescriptorPtr override {
    return offscreenTexture->getResource<mbgl::mtl::RenderableResource>()
      .getUploadPassDescriptor();
  }

  [[nodiscard]] auto getRenderPassDescriptor() const
    -> const mbgl::mtl::MTLRenderPassDescriptorPtr& override {
    return offscreenTexture->getResource<mbgl::mtl::RenderableResource>()
      .getRenderPassDescriptor();
  }

 private:
  MetalTextureBackend& backend;
  mbgl::mtl::Context& context;
  std::unique_ptr<mbgl::gfx::OffscreenTexture> offscreenTexture;
};

MetalTextureBackend::MetalTextureBackend(
  MTL::Device* host_device, mbgl::Size size
)
    : mbgl::mtl::RendererBackend(mbgl::gfx::ContextMode::Unique),
      mbgl::gfx::HeadlessBackend(size) {
  device = NS::RetainPtr(host_device);
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
      *this, static_cast<mbgl::mtl::Context&>(getContext()), size
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
