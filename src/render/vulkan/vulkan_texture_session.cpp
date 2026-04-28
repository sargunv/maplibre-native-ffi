#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/util/size.hpp>
#include <mbgl/vulkan/context.hpp>
#include <mbgl/vulkan/renderable_resource.hpp>
#include <mbgl/vulkan/renderer_backend.hpp>
#include <mbgl/vulkan/texture2d.hpp>

#include <vulkan/vulkan.hpp>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_abi.h"
#include "render/texture_session.hpp"

// NOLINTBEGIN(misc-include-cleaner)

struct mln_texture_session {
  mln_map* map = nullptr;
  std::thread::id owner_thread;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t physical_width = 0;
  uint32_t physical_height = 0;
  double scale_factor = 1.0;
  uint64_t generation = 1;
  uint64_t next_frame_id = 1;
  uint64_t acquired_frame_id = 0;
  uint64_t rendered_generation = 0;
  bool attached = true;
  bool acquired = false;
  std::unique_ptr<mbgl::gfx::HeadlessBackend> backend;
  std::unique_ptr<mbgl::Renderer> renderer;
};

namespace {

class VulkanTextureBackend final : public mbgl::vulkan::RendererBackend,
                                   public mbgl::gfx::HeadlessBackend {
 private:
  class VulkanTextureRenderableResource;

 public:
  VulkanTextureBackend(
    const mln_vulkan_texture_descriptor& descriptor, mbgl::Size size_
  )
      : mbgl::vulkan::RendererBackend(mbgl::gfx::ContextMode::Unique),
        mbgl::gfx::HeadlessBackend(size_),
        descriptor_(descriptor) {
    dynamicLoader = vk::DynamicLoader{"libvulkan.so.1"};
    initSharedDevice();
  }

  VulkanTextureBackend(const VulkanTextureBackend&) = delete;
  auto operator=(const VulkanTextureBackend&) -> VulkanTextureBackend& = delete;
  VulkanTextureBackend(VulkanTextureBackend&&) = delete;
  auto operator=(VulkanTextureBackend&&) -> VulkanTextureBackend& = delete;

  ~VulkanTextureBackend() override {
    auto guard = mbgl::gfx::BackendScope{
      *this, mbgl::gfx::BackendScope::ScopeType::Implicit
    };
    resource.reset();
    getThreadPool().runRenderJobs(true);
    context.reset();
  }

  void initSharedDevice() {
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

  auto getDefaultRenderable() -> mbgl::gfx::Renderable& override {
    if (!resource) {
      resource = std::make_unique<VulkanTextureRenderableResource>(*this);
    }
    return *this;
  }

  auto readStillImage() -> mbgl::PremultipliedImage override { return {}; }

  auto getRendererBackend() -> mbgl::gfx::RendererBackend* override {
    return this;
  }

  void activate() override {}
  void deactivate() override {}

  auto rendered_resource() -> VulkanTextureRenderableResource& {
    return getResource<VulkanTextureRenderableResource>();
  }

  void prepareRenderResources() {
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

 protected:
  void initInstance() override {
    usingSharedContext = true;
    instance = vk::UniqueInstance(
      static_cast<VkInstance>(descriptor_.instance),
      vk::ObjectDestroy<vk::NoParent, vk::DispatchLoaderDynamic>(
        nullptr, dispatcher
      )
    );
  }

  void initDebug() override {}

  void initSurface() override {}

  void initDevice() override {
    const auto physical_devices =
      instance->enumeratePhysicalDevices(dispatcher);
    auto* const requested_physical_device =
      static_cast<VkPhysicalDevice>(descriptor_.physical_device);
    auto found_physical_device = false;
    for (const auto& candidate : physical_devices) {
      if (
        static_cast<VkPhysicalDevice>(candidate) == requested_physical_device
      ) {
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

  void initSwapchain() override {
    auto& renderable = getDefaultRenderable();
    auto& renderable_resource =
      renderable.getResource<VulkanTextureRenderableResource>();
    const auto& size = renderable.getSize();

    maxFrames = 1;
    renderable_resource.init_sampled(size.width, size.height);
  }

  auto getDeviceExtensions() -> std::vector<const char*> override { return {}; }

 private:
  class VulkanTextureRenderableResource final
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
    void init_sampled_color(uint32_t width, uint32_t height) {
      const auto image_count = backend.getMaxFrames();
      colorAllocations.reserve(image_count);
      swapchainImages.reserve(image_count);

      colorFormat = vk::Format::eR8G8B8A8Unorm;
      extent = vk::Extent2D(width, height);

      const auto image_usage = vk::ImageUsageFlagBits::eColorAttachment |
                               vk::ImageUsageFlagBits::eSampled |
                               vk::ImageUsageFlagBits::eTransferSrc;
      const auto image_create_info =
        vk::ImageCreateInfo()
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
      allocation_create_info.requiredFlags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      for (auto index = uint32_t{}; index < image_count; ++index) {
        auto allocation = std::make_unique<mbgl::vulkan::ImageAllocation>(
          backend.getAllocator()
        );
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

  mln_vulkan_texture_descriptor descriptor_;
};

using TextureRegistry = std::unordered_map<
  mln_texture_session*, std::unique_ptr<mln_texture_session>>;

auto texture_registry_mutex() -> std::mutex& {
  static auto mutex = std::mutex{};
  return mutex;
}

auto texture_registry() -> TextureRegistry& {
  static auto registry = TextureRegistry{};
  return registry;
}

auto validate_descriptor(const mln_vulkan_texture_descriptor* descriptor)
  -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_vulkan_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_vulkan_texture_descriptor.size is too small"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->width == 0 || descriptor->height == 0 ||
    !std::isfinite(descriptor->scale_factor) || descriptor->scale_factor <= 0.0
  ) {
    mln::core::set_thread_error(
      "texture dimensions and scale_factor must be positive"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->instance == nullptr || descriptor->physical_device == nullptr ||
    descriptor->device == nullptr || descriptor->graphics_queue == nullptr
  ) {
    mln::core::set_thread_error("Vulkan handles must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_vulkan_handles(const mln_vulkan_texture_descriptor& descriptor)
  -> mln_status {
  auto* const instance = static_cast<VkInstance>(descriptor.instance);
  auto* const physical_device =
    static_cast<VkPhysicalDevice>(descriptor.physical_device);

  auto physical_device_count = uint32_t{};
  auto result =
    ::vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
  if (result != VK_SUCCESS || physical_device_count == 0) {
    mln::core::set_thread_error(
      "Vulkan instance must expose at least one physical device"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto physical_devices = std::vector<VkPhysicalDevice>(physical_device_count);
  result = ::vkEnumeratePhysicalDevices(
    instance, &physical_device_count, physical_devices.data()
  );
  if (result != VK_SUCCESS) {
    mln::core::set_thread_error("failed to enumerate Vulkan physical devices");
    return MLN_STATUS_NATIVE_ERROR;
  }

  auto found_physical_device = false;
  for (auto* const candidate : physical_devices) {
    if (candidate == physical_device) {
      found_physical_device = true;
      break;
    }
  }
  if (!found_physical_device) {
    mln::core::set_thread_error(
      "Vulkan physical_device must belong to instance"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto queue_family_count = uint32_t{};
  ::vkGetPhysicalDeviceQueueFamilyProperties(
    physical_device, &queue_family_count, nullptr
  );
  if (descriptor.graphics_queue_family_index >= queue_family_count) {
    mln::core::set_thread_error(
      "Vulkan graphics_queue_family_index is out of range"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  auto queue_families =
    std::vector<VkQueueFamilyProperties>(queue_family_count);
  ::vkGetPhysicalDeviceQueueFamilyProperties(
    physical_device, &queue_family_count, queue_families.data()
  );
  const auto& queue_family =
    queue_families.at(descriptor.graphics_queue_family_index);
  if (
    (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
    queue_family.queueCount == 0
  ) {
    mln::core::set_thread_error(
      "Vulkan graphics_queue_family_index must support graphics"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }

  return MLN_STATUS_OK;
}

auto physical_dimension(uint32_t logical, double scale_factor) -> uint32_t {
  return static_cast<uint32_t>(std::ceil(logical * scale_factor));
}

auto validate_physical_size(
  uint32_t width, uint32_t height, double scale_factor
) -> mln_status {
  constexpr auto max_dimension =
    static_cast<double>(std::numeric_limits<uint32_t>::max());
  if (
    std::ceil(width * scale_factor) > max_dimension ||
    std::ceil(height * scale_factor) > max_dimension
  ) {
    mln::core::set_thread_error("scaled texture dimensions are too large");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_texture(mln_texture_session* texture) -> mln_status {
  if (texture == nullptr) {
    mln::core::set_thread_error("texture session must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(texture_registry_mutex());
  if (!texture_registry().contains(texture)) {
    mln::core::set_thread_error("texture session is not a live handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->owner_thread != std::this_thread::get_id()) {
    mln::core::set_thread_error(
      "texture session call must be made on its owner thread"
    );
    return MLN_STATUS_WRONG_THREAD;
  }
  return MLN_STATUS_OK;
}

auto validate_live_attached_texture(mln_texture_session* texture)
  -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (!texture->attached || texture->backend == nullptr) {
    mln::core::set_thread_error("texture session is detached");
    return MLN_STATUS_INVALID_STATE;
  }
  return MLN_STATUS_OK;
}

}  // namespace

namespace mln::core {

auto vulkan_texture_descriptor_default() noexcept
  -> mln_vulkan_texture_descriptor {
  return mln_vulkan_texture_descriptor{
    .size = sizeof(mln_vulkan_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .instance = nullptr,
    .physical_device = nullptr,
    .device = nullptr,
    .graphics_queue = nullptr,
    .graphics_queue_family_index = 0,
  };
}

auto vulkan_texture_attach(
  mln_map* map, const mln_vulkan_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  if (out_texture == nullptr) {
    set_thread_error("out_texture must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (*out_texture != nullptr) {
    set_thread_error("out_texture must point to a null handle");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  const auto vulkan_status = validate_vulkan_handles(*descriptor);
  if (vulkan_status != MLN_STATUS_OK) {
    return vulkan_status;
  }

  auto session = std::make_unique<mln_texture_session>();
  session->map = map;
  session->owner_thread = map_owner_thread(map);
  session->width = descriptor->width;
  session->height = descriptor->height;
  session->scale_factor = descriptor->scale_factor;
  session->physical_width =
    physical_dimension(descriptor->width, descriptor->scale_factor);
  session->physical_height =
    physical_dimension(descriptor->height, descriptor->scale_factor);
  session->backend = std::make_unique<VulkanTextureBackend>(
    *descriptor, mbgl::Size{session->physical_width, session->physical_height}
  );
  auto* handle = session.get();

  const auto attach_status = map_attach_texture_session(map, handle);
  if (attach_status != MLN_STATUS_OK) {
    return attach_status;
  }
  try {
    if (auto* native_map = map_native(map); native_map != nullptr) {
      native_map->setSize(mbgl::Size{descriptor->width, descriptor->height});
    }
    const std::scoped_lock lock(texture_registry_mutex());
    texture_registry().emplace(handle, std::move(session));
  } catch (...) {
    static_cast<void>(map_detach_texture_session(map, handle));
    throw;
  }

  *out_texture = handle;
  return MLN_STATUS_OK;
}

auto texture_resize(
  mln_texture_session* texture, uint32_t width, uint32_t height,
  double scale_factor
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    width == 0 || height == 0 || !std::isfinite(scale_factor) ||
    scale_factor <= 0.0
  ) {
    set_thread_error("texture dimensions and scale_factor must be positive");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->acquired) {
    set_thread_error("cannot resize while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  const auto physical_status =
    validate_physical_size(width, height, scale_factor);
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  const auto physical_width = physical_dimension(width, scale_factor);
  const auto physical_height = physical_dimension(height, scale_factor);

  texture->backend->setSize(mbgl::Size{physical_width, physical_height});
  if (auto* map = map_native(texture->map); map != nullptr) {
    map->setSize(mbgl::Size{width, height});
  }
  texture->renderer.reset();
  texture->rendered_generation = 0;
  texture->width = width;
  texture->height = height;
  texture->physical_width = physical_width;
  texture->physical_height = physical_height;
  texture->scale_factor = scale_factor;
  ++texture->generation;
  return MLN_STATUS_OK;
}

auto texture_render(mln_texture_session* texture) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->acquired) {
    set_thread_error("cannot render while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }

  auto update = map_latest_update(texture->map);
  if (!update) {
    set_thread_error("no map render update is available");
    return MLN_STATUS_INVALID_STATE;
  }

  // Renderer::render creates the Vulkan context before requesting the default
  // renderable, so shared-device resources must be ready first.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  static_cast<VulkanTextureBackend&>(*texture->backend)
    .prepareRenderResources();
  auto guard = mbgl::gfx::BackendScope{
    *texture->backend->getRendererBackend(),
    mbgl::gfx::BackendScope::ScopeType::Implicit
  };
  map_run_render_jobs(texture->map);
  if (texture->renderer == nullptr) {
    texture->renderer = std::make_unique<mbgl::Renderer>(
      *texture->backend->getRendererBackend(),
      static_cast<float>(texture->scale_factor)
    );
    texture->renderer->setObserver(map_renderer_observer(texture->map));
  }

  texture->renderer->render(update);
  texture->rendered_generation = texture->generation;
  return MLN_STATUS_OK;
}

auto vulkan_texture_acquire_frame(
  mln_texture_session* texture, mln_vulkan_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_frame == nullptr || out_frame->size < sizeof(mln_vulkan_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->acquired) {
    set_thread_error("a texture frame is already acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->rendered_generation != texture->generation) {
    set_thread_error("no rendered frame is available for this generation");
    return MLN_STATUS_INVALID_STATE;
  }

  // The Vulkan acquire path is only valid for sessions created by
  // vulkan_texture_attach, and this Linux build only creates Vulkan sessions.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  auto& backend = static_cast<VulkanTextureBackend&>(*texture->backend);
  auto& resource = backend.rendered_resource();
  *out_frame = mln_vulkan_texture_frame{
    .size = sizeof(mln_vulkan_texture_frame),
    .generation = texture->generation,
    .width = texture->physical_width,
    .height = texture->physical_height,
    .scale_factor = texture->scale_factor,
    .frame_id = texture->next_frame_id,
    .image = resource.image(),
    .image_view = resource.image_view(),
    .device = backend.getDevice().get(),
    .format = static_cast<uint32_t>(resource.format()),
    .layout = static_cast<uint32_t>(vk::ImageLayout::eShaderReadOnlyOptimal),
  };
  texture->acquired = true;
  texture->acquired_frame_id = out_frame->frame_id;
  ++texture->next_frame_id;
  return MLN_STATUS_OK;
}

auto vulkan_texture_release_frame(
  mln_texture_session* texture, const mln_vulkan_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (frame == nullptr || frame->size < sizeof(mln_vulkan_texture_frame)) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (!texture->acquired) {
    set_thread_error("no texture frame is currently acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (frame->generation != texture->generation) {
    set_thread_error("frame generation does not match acquired frame");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (frame->frame_id != texture->acquired_frame_id) {
    set_thread_error("frame identity does not match acquired frame");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  texture->acquired = false;
  texture->acquired_frame_id = 0;
  return MLN_STATUS_OK;
}

auto texture_detach(mln_texture_session* texture) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->acquired) {
    set_thread_error("cannot detach while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }

  const auto detach_status = map_detach_texture_session(texture->map, texture);
  if (detach_status != MLN_STATUS_OK) {
    return detach_status;
  }
  texture->renderer.reset();
  texture->backend.reset();
  texture->attached = false;
  texture->rendered_generation = 0;
  ++texture->generation;
  return MLN_STATUS_OK;
}

auto texture_destroy(mln_texture_session* texture) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (texture->acquired) {
    set_thread_error("cannot destroy while a texture frame is acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->attached) {
    const auto detach_status = texture_detach(texture);
    if (detach_status != MLN_STATUS_OK) {
      return detach_status;
    }
  }

  auto owned_texture = std::unique_ptr<mln_texture_session>{};
  {
    const std::scoped_lock lock(texture_registry_mutex());
    auto found = texture_registry().find(texture);
    owned_texture = std::move(found->second);
    texture_registry().erase(found);
  }
  owned_texture.reset();
  return MLN_STATUS_OK;
}

auto metal_texture_descriptor_default() noexcept
  -> mln_metal_texture_descriptor {
  return mln_metal_texture_descriptor{
    .size = sizeof(mln_metal_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .device = nullptr,
  };
}

auto metal_texture_attach(
  mln_map* map, const mln_metal_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  (void)map;
  (void)descriptor;
  if (out_texture != nullptr) {
    *out_texture = nullptr;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) -> mln_status {
  (void)texture;
  (void)out_frame;
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) -> mln_status {
  (void)texture;
  (void)frame;
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core

// NOLINTEND(misc-include-cleaner)
