#include <cmath>
#include <memory>
#include <stdexcept>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/mtl/context.hpp>
#include <mbgl/mtl/renderable_resource.hpp>
#include <mbgl/mtl/renderer_backend.hpp>
#include <mbgl/mtl/texture2d.hpp>
#include <mbgl/util/size.hpp>

#include <Foundation/NSSharedPtr.hpp>
#include <Metal/MTLBlitPass.hpp>
#include <Metal/MTLCommandBuffer.hpp>
#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLRenderPass.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include <QuartzCore/CAMetalLayer.hpp>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "render/render_session_common.hpp"
#include "render/surface_session.hpp"

namespace {

auto validate_descriptor(const mln_metal_surface_descriptor* descriptor)
  -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("surface descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_surface_descriptor)) {
    mln::core::set_thread_error(
      "mln_metal_surface_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "surface dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->layer == nullptr) {
    mln::core::set_thread_error("Metal surface layer must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

class MetalSurfaceBackend final : public mbgl::mtl::RendererBackend,
                                  public mbgl::gfx::Renderable {
 private:
  class MetalSurfaceRenderableResource final
      : public mbgl::mtl::RenderableResource {
   public:
    MetalSurfaceRenderableResource(
      MetalSurfaceBackend& backend_, CA::MetalLayer* layer_, mbgl::Size size_
    )
        : backend(backend_), layer(NS::RetainPtr(layer_)) {
      layer->setDevice(backend.getDevice().get());
      layer->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
      layer->setFramebufferOnly(false);
      setSize(size_);
    }
    MetalSurfaceRenderableResource(const MetalSurfaceRenderableResource&) =
      delete;
    auto operator=(const MetalSurfaceRenderableResource&)
      -> MetalSurfaceRenderableResource& = delete;
    MetalSurfaceRenderableResource(MetalSurfaceRenderableResource&&) = delete;
    auto operator=(MetalSurfaceRenderableResource&&)
      -> MetalSurfaceRenderableResource& = delete;
    ~MetalSurfaceRenderableResource() override = default;

    void setSize(mbgl::Size size_) {
      size = size_;
      layer->setDrawableSize(CGSizeMake(
        static_cast<CGFloat>(size.width), static_cast<CGFloat>(size.height)
      ));
      depthStencilDirty = true;
    }

    void bind() override {
      if (drawable && commandBuffer && renderPassDescriptor) {
        return;
      }

      auto* next_drawable = layer->nextDrawable();
      if (next_drawable == nullptr) {
        throw std::runtime_error("Metal surface did not provide a drawable");
      }
      drawable = NS::RetainPtr(next_drawable);

      commandBuffer = NS::RetainPtr(backend.getCommandQueue()->commandBuffer());
      renderPassDescriptor =
        NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
      renderPassDescriptor->colorAttachments()->object(0)->setTexture(
        drawable->texture()
      );

      if (depthStencilDirty || !depthTexture || !stencilTexture) {
        depthStencilDirty = false;
        // Metal renderables are always attached to a Metal backend/context.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        auto& context = static_cast<mbgl::mtl::Context&>(backend.getContext());
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
        // The texture was created by mbgl::mtl::Context above.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        static_cast<mbgl::mtl::Texture2D*>(depthTexture.get())
          ->setUsage(
            MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite |
            MTL::TextureUsageRenderTarget
          );

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
        // The texture was created by mbgl::mtl::Context above.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        static_cast<mbgl::mtl::Texture2D*>(stencilTexture.get())
          ->setUsage(
            MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite |
            MTL::TextureUsageRenderTarget
          );
      }

      depthTexture->create();
      if (auto* depthTarget = renderPassDescriptor->depthAttachment()) {
        depthTarget->setTexture(
          // The texture was created by mbgl::mtl::Context above.
          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
          static_cast<mbgl::mtl::Texture2D*>(depthTexture.get())
            ->getMetalTexture()
        );
      }
      stencilTexture->create();
      if (auto* stencilTarget = renderPassDescriptor->stencilAttachment()) {
        stencilTarget->setTexture(
          // The texture was created by mbgl::mtl::Context above.
          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
          static_cast<mbgl::mtl::Texture2D*>(stencilTexture.get())
            ->getMetalTexture()
        );
      }
    }

    void swap() override {
      commandBuffer->presentDrawable(drawable.get());
      commandBuffer->commit();
      commandBuffer.reset();
      drawable.reset();
      renderPassDescriptor.reset();
    }

    [[nodiscard]] auto getBackend() const
      -> const mbgl::mtl::RendererBackend& override {
      return backend;
    }

    [[nodiscard]] auto getCommandBuffer() const
      -> const mbgl::mtl::MTLCommandBufferPtr& override {
      return commandBuffer;
    }

    [[nodiscard]] auto getUploadPassDescriptor() const
      -> mbgl::mtl::MTLBlitPassDescriptorPtr override {
      return NS::TransferPtr(MTL::BlitPassDescriptor::alloc()->init());
    }

    [[nodiscard]] auto getRenderPassDescriptor() const
      -> const mbgl::mtl::MTLRenderPassDescriptorPtr& override {
      return renderPassDescriptor;
    }

   private:
    MetalSurfaceBackend& backend;
    NS::SharedPtr<CA::MetalLayer> layer;
    mbgl::Size size{0, 0};
    mbgl::mtl::MTLCommandBufferPtr commandBuffer;
    mbgl::mtl::MTLRenderPassDescriptorPtr renderPassDescriptor;
    mbgl::mtl::CAMetalDrawablePtr drawable;
    mbgl::gfx::Texture2DPtr depthTexture;
    mbgl::gfx::Texture2DPtr stencilTexture;
    bool depthStencilDirty = true;
  };

 public:
  MetalSurfaceBackend(
    CA::MetalLayer* layer, MTL::Device* host_device, mbgl::Size size_
  )
      : mbgl::mtl::RendererBackend(mbgl::gfx::ContextMode::Unique),
        mbgl::gfx::Renderable(size_, nullptr) {
    if (host_device != nullptr) {
      device = NS::RetainPtr(host_device);
      commandQueue = NS::TransferPtr(device->newCommandQueue());
    }
    setResource(
      std::make_unique<MetalSurfaceRenderableResource>(*this, layer, size_)
    );
  }
  MetalSurfaceBackend(const MetalSurfaceBackend&) = delete;
  auto operator=(const MetalSurfaceBackend&) -> MetalSurfaceBackend& = delete;
  MetalSurfaceBackend(MetalSurfaceBackend&&) = delete;
  auto operator=(MetalSurfaceBackend&&) -> MetalSurfaceBackend& = delete;

  ~MetalSurfaceBackend() override {
    auto guard = mbgl::gfx::BackendScope{
      *this, mbgl::gfx::BackendScope::ScopeType::Implicit
    };
    resource.reset();
    context.reset();
  }

  auto getDefaultRenderable() -> mbgl::gfx::Renderable& override {
    return *this;
  }

  void setSize(mbgl::Size size_) {
    size = size_;
    getResource<MetalSurfaceRenderableResource>().setSize(size_);
  }

  void activate() override {}
  void deactivate() override {}
  void updateAssumedState() override {}
};

class MetalSurfaceSessionBackend final
    : public mln::core::SurfaceSessionBackend {
 public:
  MetalSurfaceSessionBackend(
    CA::MetalLayer* layer, MTL::Device* host_device, mbgl::Size size
  )
      : backend_(layer, host_device, size) {}

  auto renderer_backend() -> mbgl::gfx::RendererBackend& override {
    return backend_;
  }

  void resize(uint32_t physical_width, uint32_t physical_height) override {
    backend_.setSize(mbgl::Size{physical_width, physical_height});
  }

 private:
  MetalSurfaceBackend backend_;
};

auto validate_vulkan_descriptor(const mln_vulkan_surface_descriptor* descriptor)
  -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("surface descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_vulkan_surface_descriptor)) {
    mln::core::set_thread_error(
      "mln_vulkan_surface_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "surface dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->instance == nullptr || descriptor->physical_device == nullptr ||
    descriptor->device == nullptr || descriptor->graphics_queue == nullptr ||
    descriptor->surface == nullptr
  ) {
    mln::core::set_thread_error("Vulkan surface handles must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

}  // namespace

namespace mln::core {

auto metal_surface_attach(
  mln_map* map, const mln_metal_surface_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(
    out_session, "out_session must not be null",
    "out_session must point to a null handle"
  );
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor,
    "scaled surface dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }

  auto session = std::make_unique<mln_render_session>();
  session->map = map;
  session->owner_thread = map_owner_thread(map);
  session->width = descriptor->width;
  session->height = descriptor->height;
  session->scale_factor = descriptor->scale_factor;
  session->physical_width =
    physical_dimension(descriptor->width, descriptor->scale_factor);
  session->physical_height =
    physical_dimension(descriptor->height, descriptor->scale_factor);
  session->surface.backend = std::make_unique<MetalSurfaceSessionBackend>(
    static_cast<CA::MetalLayer*>(descriptor->layer),
    static_cast<MTL::Device*>(descriptor->device),
    mbgl::Size{session->physical_width, session->physical_height}
  );
  return attach_render_session(
    std::move(session), out_session, RenderSessionKind::Surface,
    RenderSessionAttachMessages{
      .null_session = "surface session must not be null",
      .null_output = "out_session must not be null",
      .non_null_output = "out_session must point to a null handle"
    }
  );
}

auto vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_vulkan_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(
    out_session, "out_session must not be null",
    "out_session must point to a null handle"
  );
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor,
    "scaled surface dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Vulkan surface sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core
