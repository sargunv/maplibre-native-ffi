#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/vulkan/buffer_resource.hpp>
#include <mbgl/vulkan/context.hpp>
#include <mbgl/vulkan/renderable_resource.hpp>
#include <mbgl/vulkan/texture2d.hpp>

#include "render/vulkan/vulkan_texture_backend.hpp"

namespace {

auto vulkan_loader_library_name() noexcept -> const char* {
  return "libvulkan.so.1";
}

}  // namespace

namespace mln::core {

class VulkanTextureBackend::VulkanTextureRenderableResource final
    : public mbgl::vulkan::SurfaceRenderableResource {
 public:
  explicit VulkanTextureRenderableResource(VulkanTextureBackend& backend_)
      : SurfaceRenderableResource(backend_) {}

  void createPlatformSurface() override {}
  void bind() override {}

  void init_sampled(uint32_t width, uint32_t height) {
    init_sampled_color(width, height);
    create_color_image_views();
    initDepthStencil();
    create_sampled_render_pass();
    create_framebuffers();
  }

  void swap() override {
    SurfaceRenderableResource::swap();
    // This resource is only used by VulkanTextureBackend, so the downcast is
    // invariant within this file.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    static_cast<mbgl::vulkan::Context&>(backend.getContext()).waitFrame();
  }

  [[nodiscard]] auto image() const -> VkImage { return getAcquiredImage(); }

  [[nodiscard]] auto image_view() const -> VkImageView {
    return swapchainImageViews.at(getAcquiredImageIndex()).get();
  }

  [[nodiscard]] auto format() const -> VkFormat {
    return static_cast<VkFormat>(colorFormat);
  }

 private:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void init_sampled_color(uint32_t width, uint32_t height) {
    const auto image_count = backend.getMaxFrames();
    colorAllocations.reserve(image_count);
    swapchainImages.reserve(image_count);

    colorFormat = vk::Format::eR8G8B8A8Unorm;
    extent = vk::Extent2D(width, height);

    const auto image_usage = vk::ImageUsageFlagBits::eColorAttachment |
                             vk::ImageUsageFlagBits::eSampled |
                             vk::ImageUsageFlagBits::eTransferSrc;
    auto image_create_info = vk::ImageCreateInfo()
                               .setImageType(vk::ImageType::e2D)
                               .setFormat(colorFormat)
                               .setExtent({width, height, 1})
                               .setMipLevels(1)
                               .setArrayLayers(1)
                               .setSamples(vk::SampleCountFlagBits::e1)
                               .setTiling(vk::ImageTiling::eOptimal)
                               .setUsage(image_usage)
                               .setSharingMode(vk::SharingMode::eExclusive)
                               .setInitialLayout(vk::ImageLayout::eUndefined);

    auto allocation_create_info = VmaAllocationCreateInfo{};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocation_create_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    for (auto index = uint32_t{}; index < image_count; ++index) {
      auto allocation =
        std::make_unique<mbgl::vulkan::ImageAllocation>(backend.getAllocator());
      if (!allocation->create(allocation_create_info, image_create_info)) {
        throw std::runtime_error(
          "Vulkan sampled color texture allocation failed"
        );
      }
      swapchainImages.push_back(allocation->image);
      colorAllocations.push_back(std::move(allocation));
    }
  }

  void create_color_image_views() {
    const auto& device = backend.getDevice();
    const auto& dispatcher = backend.getDispatcher();

    swapchainImageViews.reserve(swapchainImages.size());
    auto image_view_create_info =
      vk::ImageViewCreateInfo()
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(colorFormat)
        .setComponents(vk::ComponentMapping())
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    for (const auto& image : swapchainImages) {
      image_view_create_info.setImage(image);
      swapchainImageViews.push_back(device->createImageViewUnique(
        image_view_create_info, nullptr, dispatcher
      ));
      const auto index = swapchainImageViews.size() - 1;
      backend.setDebugName(
        image, "TextureSessionImage_" + std::to_string(index)
      );
      backend.setDebugName(
        swapchainImageViews.back().get(),
        "TextureSessionImageView_" + std::to_string(index)
      );
    }
  }

  void create_sampled_render_pass() {
    const auto& device = backend.getDevice();
    const auto& dispatcher = backend.getDispatcher();

    const std::array<vk::AttachmentDescription, 2> attachments = {
      vk::AttachmentDescription()
        .setFormat(colorFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
      vk::AttachmentDescription()
        .setFormat(depthFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
    };
    const auto color_attachment_ref =
      vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
    const auto depth_attachment_ref = vk::AttachmentReference(
      1, vk::ImageLayout::eDepthStencilAttachmentOptimal
    );
    const auto subpass =
      vk::SubpassDescription()
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(1)
        .setColorAttachments(color_attachment_ref)
        .setPDepthStencilAttachment(&depth_attachment_ref);
    const std::array<vk::SubpassDependency, 3> dependencies = {
      vk::SubpassDependency()
        .setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setSrcAccessMask({})
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
      vk::SubpassDependency()
        .setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(
          vk::PipelineStageFlagBits::eEarlyFragmentTests |
          vk::PipelineStageFlagBits::eLateFragmentTests
        )
        .setDstStageMask(
          vk::PipelineStageFlagBits::eEarlyFragmentTests |
          vk::PipelineStageFlagBits::eLateFragmentTests
        )
        .setSrcAccessMask({})
        .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite),
      vk::SubpassDependency()
        .setSrcSubpass(0)
        .setDstSubpass(VK_SUBPASS_EXTERNAL)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
        .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
    };
    const auto render_pass_create_info = vk::RenderPassCreateInfo()
                                           .setAttachments(attachments)
                                           .setSubpasses(subpass)
                                           .setDependencies(dependencies);
    renderPass = device->createRenderPassUnique(
      render_pass_create_info, nullptr, dispatcher
    );
  }

  void create_framebuffers() {
    const auto& device = backend.getDevice();
    const auto& dispatcher = backend.getDispatcher();

    swapchainFramebuffers.reserve(swapchainImageViews.size());
    auto framebuffer_create_info = vk::FramebufferCreateInfo()
                                     .setRenderPass(renderPass.get())
                                     .setAttachmentCount(2)
                                     .setWidth(extent.width)
                                     .setHeight(extent.height)
                                     .setLayers(1);
    for (const auto& image_view : swapchainImageViews) {
      const std::array<vk::ImageView, 2> image_views = {
        image_view.get(), depthAllocation->imageView.get()
      };
      framebuffer_create_info.setAttachments(image_views);
      swapchainFramebuffers.push_back(device->createFramebufferUnique(
        framebuffer_create_info, nullptr, dispatcher
      ));
    }
  }
};

VulkanTextureBackend::VulkanTextureBackend(
  const mln_vulkan_texture_descriptor& descriptor, mbgl::Size size
)
    : mbgl::vulkan::RendererBackend(mbgl::gfx::ContextMode::Unique),
      mbgl::gfx::HeadlessBackend(size),
      descriptor_(descriptor) {
  dynamicLoader = vk::DynamicLoader{vulkan_loader_library_name()};
  initSharedDevice();
}

VulkanTextureBackend::~VulkanTextureBackend() {
  auto guard = mbgl::gfx::BackendScope{
    *this, mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  resource.reset();
  getThreadPool().runRenderJobs(true);
  context.reset();
}

void VulkanTextureBackend::initSharedDevice() {
  auto* get_instance_proc_addr =
    dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>(
      "vkGetInstanceProcAddr"
    );
  dispatcher = vk::DispatchLoaderDynamic(
    static_cast<VkInstance>(descriptor_.instance), get_instance_proc_addr
  );

  initFrameCapture();
  initInstance();
  initDebug();
  initSurface();
  initDevice();

  physicalDeviceProperties = physicalDevice.getProperties(dispatcher);
}

auto VulkanTextureBackend::getDefaultRenderable() -> mbgl::gfx::Renderable& {
  if (!resource) {
    resource = std::make_unique<VulkanTextureRenderableResource>(*this);
  }
  return *this;
}

auto VulkanTextureBackend::readStillImage() -> mbgl::PremultipliedImage {
  prepareRenderResources();

  auto image = mbgl::PremultipliedImage(size);
  const auto image_size = image.bytes();
  const auto& allocator = getAllocator();
  const auto buffer_info = vk::BufferCreateInfo()
                             .setSize(image_size)
                             .setUsage(vk::BufferUsageFlagBits::eTransferDst)
                             .setSharingMode(vk::SharingMode::eExclusive);

  auto allocation_info = VmaAllocationCreateInfo{};
  allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
  allocation_info.requiredFlags =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  allocation_info.flags =
    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
    VMA_ALLOCATION_CREATE_MAPPED_BIT;

  auto buffer_allocation = mbgl::vulkan::BufferAllocation{allocator};
  if (!buffer_allocation.create(allocation_info, buffer_info)) {
    return {};
  }

  auto& context_impl = static_cast<mbgl::vulkan::Context&>(getContext());
  auto& resource_impl = rendered_resource();
  const auto source_image = resource_impl.image();
  context_impl.waitFrame();
  context_impl.submitOneTimeCommand(
    [&](const vk::UniqueCommandBuffer& command_buffer) {
      const auto to_transfer =
        vk::ImageMemoryBarrier()
          .setImage(source_image)
          .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
          .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
          .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
          .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
      command_buffer->pipelineBarrier(
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, to_transfer,
        getDispatcher()
      );

      const auto region =
        vk::BufferImageCopy()
          .setBufferOffset(0)
          .setBufferRowLength(0)
          .setBufferImageHeight(0)
          .setImageSubresource(
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1)
          )
          .setImageOffset({0, 0, 0})
          .setImageExtent({size.width, size.height, 1});
      command_buffer->copyImageToBuffer(
        source_image, vk::ImageLayout::eTransferSrcOptimal,
        buffer_allocation.buffer, region, getDispatcher()
      );

      const auto to_shader_read =
        vk::ImageMemoryBarrier()
          .setImage(source_image)
          .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
          .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
          .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
          .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
      command_buffer->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr,
        to_shader_read, getDispatcher()
      );
    }
  );

  if (buffer_allocation.mappedBuffer == nullptr) {
    if (
      vmaMapMemory(
        allocator, buffer_allocation.allocation, &buffer_allocation.mappedBuffer
      ) != VK_SUCCESS
    ) {
      return {};
    }
    std::memcpy(image.data.get(), buffer_allocation.mappedBuffer, image_size);
    vmaUnmapMemory(allocator, buffer_allocation.allocation);
    buffer_allocation.mappedBuffer = nullptr;
  } else {
    std::memcpy(image.data.get(), buffer_allocation.mappedBuffer, image_size);
  }
  return image;
}

auto VulkanTextureBackend::getRendererBackend() -> mbgl::gfx::RendererBackend* {
  return this;
}

void VulkanTextureBackend::activate() {}

void VulkanTextureBackend::deactivate() {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto VulkanTextureBackend::rendered_resource()
  -> VulkanTextureRenderableResource& {
  return getResource<VulkanTextureRenderableResource>();
}

void VulkanTextureBackend::prepareRenderResources() {
  if (allocator == nullptr) {
    initAllocator();
  }
  if (!resource) {
    initSwapchain();
  }
  if (!commandPool) {
    initCommandPool();
  }
}

auto VulkanTextureBackend::frame_resources() -> VulkanTextureFrameResources {
  auto& rendered = rendered_resource();
  return VulkanTextureFrameResources{
    .image = rendered.image(),
    .image_view = rendered.image_view(),
    .device = device.get(),
    .format = rendered.format(),
  };
}

void VulkanTextureBackend::initInstance() {
  usingSharedContext = true;
  instance = vk::UniqueInstance(
    static_cast<VkInstance>(descriptor_.instance),
    vk::ObjectDestroy<vk::NoParent, vk::DispatchLoaderDynamic>(
      nullptr, dispatcher
    )
  );
}

void VulkanTextureBackend::initDebug() {}

void VulkanTextureBackend::initSurface() {}

void VulkanTextureBackend::initDevice() {
  const auto physical_devices = instance->enumeratePhysicalDevices(dispatcher);
  auto* const requested_physical_device =
    static_cast<VkPhysicalDevice>(descriptor_.physical_device);
  auto found_physical_device = false;
  for (const auto& candidate : physical_devices) {
    if (static_cast<VkPhysicalDevice>(candidate) == requested_physical_device) {
      physicalDevice = candidate;
      found_physical_device = true;
      break;
    }
  }
  if (!found_physical_device) {
    throw std::runtime_error(
      "Vulkan physical_device does not belong to instance"
    );
  }
  device = vk::UniqueDevice(
    static_cast<VkDevice>(descriptor_.device),
    vk::ObjectDestroy<vk::NoParent, vk::DispatchLoaderDynamic>(
      nullptr, dispatcher
    )
  );
  dispatcher.init(
    static_cast<VkInstance>(descriptor_.instance), ::vkGetInstanceProcAddr,
    static_cast<VkDevice>(descriptor_.device), ::vkGetDeviceProcAddr
  );
  graphicsQueueIndex =
    static_cast<int32_t>(descriptor_.graphics_queue_family_index);
  presentQueueIndex = -1;
  graphicsQueue = static_cast<VkQueue>(descriptor_.graphics_queue);
  physicalDeviceFeatures = physicalDevice.getFeatures(dispatcher);
}

void VulkanTextureBackend::initSwapchain() {
  auto& renderable = getDefaultRenderable();
  auto& renderable_resource =
    renderable.getResource<VulkanTextureRenderableResource>();
  const auto& size = renderable.getSize();

  maxFrames = 1;
  renderable_resource.init_sampled(size.width, size.height);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto VulkanTextureBackend::getDeviceExtensions() -> std::vector<const char*> {
  return {};
}

}  // namespace mln::core
