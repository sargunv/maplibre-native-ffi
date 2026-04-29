#pragma once

#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/mtl/headless_backend.hpp>

#include <Metal/MTLDevice.hpp>
#include <Metal/MTLTexture.hpp>

namespace mln::core {

class MetalTextureBackend final : public mbgl::mtl::RendererBackend,
                                  public mbgl::gfx::HeadlessBackend {
 private:
  class MetalTextureRenderableResource;

 public:
  MetalTextureBackend(MTL::Device* host_device, mbgl::Size size);
  MetalTextureBackend(const MetalTextureBackend&) = delete;
  auto operator=(const MetalTextureBackend&) -> MetalTextureBackend& = delete;
  MetalTextureBackend(MetalTextureBackend&&) = delete;
  auto operator=(MetalTextureBackend&&) -> MetalTextureBackend& = delete;
  ~MetalTextureBackend() override;

  auto getDefaultRenderable() -> mbgl::gfx::Renderable& override;
  auto readStillImage() -> mbgl::PremultipliedImage override;
  auto getRendererBackend() -> mbgl::gfx::RendererBackend* override;
  void activate() override;
  void deactivate() override;
  void updateAssumedState() override;

  auto metal_texture() -> MTL::Texture*;
};

}  // namespace mln::core
