#pragma once

#include <vector>

#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/vulkan/renderer_backend.hpp>

#include <vulkan/vulkan.hpp>  // NOLINT(misc-include-cleaner)

#include "maplibre_native_abi.h"

// NOLINTBEGIN(misc-include-cleaner)

namespace mln::core {

struct VulkanTextureFrameResources {
  VkImage image = nullptr;
  VkImageView image_view = nullptr;
  VkDevice device = nullptr;
  VkFormat format = VK_FORMAT_UNDEFINED;
};

class VulkanTextureBackend final : public mbgl::vulkan::RendererBackend,
                                   public mbgl::gfx::HeadlessBackend {
 private:
  class VulkanTextureRenderableResource;

 public:
  VulkanTextureBackend(
    const mln_vulkan_texture_descriptor& descriptor, mbgl::Size size
  );
  VulkanTextureBackend(const VulkanTextureBackend&) = delete;
  auto operator=(const VulkanTextureBackend&) -> VulkanTextureBackend& = delete;
  VulkanTextureBackend(VulkanTextureBackend&&) = delete;
  auto operator=(VulkanTextureBackend&&) -> VulkanTextureBackend& = delete;
  ~VulkanTextureBackend() override;

  auto getDefaultRenderable() -> mbgl::gfx::Renderable& override;
  auto readStillImage() -> mbgl::PremultipliedImage override;
  auto getRendererBackend() -> mbgl::gfx::RendererBackend* override;
  void activate() override;
  void deactivate() override;

  void prepareRenderResources();
  auto frame_resources() -> VulkanTextureFrameResources;

 protected:
  void initInstance() override;
  void initDebug() override;
  void initSurface() override;
  void initDevice() override;
  void initSwapchain() override;
  auto getDeviceExtensions() -> std::vector<const char*> override;

 private:
  void initSharedDevice();
  auto rendered_resource() -> VulkanTextureRenderableResource&;

  mln_vulkan_texture_descriptor descriptor_;
};

}  // namespace mln::core

// NOLINTEND(misc-include-cleaner)
