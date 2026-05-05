// Vulkan-Hpp exposes vk::* wrapper types through generated headers that
// clang-tidy include-cleaner does not map reliably.
// NOLINTBEGIN(misc-include-cleaner)

#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/util/size.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c/base.h"
#include "maplibre_native_c/texture.h"
#include "render/render_session_common.hpp"
#include "render/texture_session.hpp"
#include "render/vulkan/vulkan_texture_backend.hpp"

namespace {
auto validate_owned_descriptor(
  const mln_vulkan_owned_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_vulkan_owned_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_vulkan_owned_texture_descriptor.size is too small"
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

auto validate_borrowed_descriptor(
  const mln_vulkan_borrowed_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_vulkan_borrowed_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_vulkan_borrowed_texture_descriptor.size is too small"
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
    descriptor->device == nullptr || descriptor->graphics_queue == nullptr ||
    descriptor->image == nullptr || descriptor->image_view == nullptr
  ) {
    mln::core::set_thread_error("Vulkan handles must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    descriptor->format == VK_FORMAT_UNDEFINED || descriptor->final_layout == 0
  ) {
    mln::core::set_thread_error(
      "Vulkan format and final_layout must be specified"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_metal_owned_descriptor(
  const mln_metal_owned_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_owned_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_metal_owned_texture_descriptor.size is too small"
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
  if (descriptor->device == nullptr) {
    mln::core::set_thread_error("Metal device must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_metal_borrowed_descriptor(
  const mln_metal_borrowed_texture_descriptor* descriptor
) -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_borrowed_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_metal_borrowed_texture_descriptor.size is too small"
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
  if (descriptor->texture == nullptr) {
    mln::core::set_thread_error("Metal texture must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  return MLN_STATUS_OK;
}

auto validate_vulkan_handles(
  const mln_vulkan_owned_texture_descriptor& descriptor
) -> mln_status {
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

class VulkanTextureSessionBackend final
    : public mln::core::TextureSessionBackend {
 public:
  VulkanTextureSessionBackend(
    const mln_vulkan_owned_texture_descriptor& descriptor, mbgl::Size size
  )
      : backend_(descriptor, size) {}

  VulkanTextureSessionBackend(
    const mln_vulkan_borrowed_texture_descriptor& descriptor, mbgl::Size size
  )
      : backend_(descriptor, size) {}

  auto headless_backend() -> mbgl::gfx::HeadlessBackend& override {
    return backend_;
  }

  void prepare_render_resources() override {
    // Renderer::render creates the Vulkan context before requesting the default
    // renderable, so shared-device resources must be ready first.
    backend_.prepareRenderResources();
  }

  auto acquire_vulkan_owned_frame(
    const mln_render_session& texture, mln_vulkan_owned_texture_frame& out_frame
  ) -> mln_status override {
    const auto resources = backend_.frame_resources();
    out_frame = mln_vulkan_owned_texture_frame{
      .size = sizeof(mln_vulkan_owned_texture_frame),
      .generation = texture.generation,
      .width = texture.physical_width,
      .height = texture.physical_height,
      .scale_factor = texture.scale_factor,
      .frame_id = texture.texture.next_frame_id,
      .image = resources.image,
      .image_view = resources.image_view,
      .device = resources.device,
      .format = static_cast<uint32_t>(resources.format),
      .layout = static_cast<uint32_t>(vk::ImageLayout::eShaderReadOnlyOptimal),
    };
    return MLN_STATUS_OK;
  }

 private:
  mln::core::VulkanTextureBackend backend_;
};

}  // namespace

namespace mln::core {

auto vulkan_owned_texture_descriptor_default() noexcept
  -> mln_vulkan_owned_texture_descriptor {
  return mln_vulkan_owned_texture_descriptor{
    .size = sizeof(mln_vulkan_owned_texture_descriptor),
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

auto vulkan_borrowed_texture_descriptor_default() noexcept
  -> mln_vulkan_borrowed_texture_descriptor {
  return mln_vulkan_borrowed_texture_descriptor{
    .size = sizeof(mln_vulkan_borrowed_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .instance = nullptr,
    .physical_device = nullptr,
    .device = nullptr,
    .graphics_queue = nullptr,
    .graphics_queue_family_index = 0,
    .image = nullptr,
    .image_view = nullptr,
    .format = VK_FORMAT_UNDEFINED,
    .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
    .final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
}

auto vulkan_owned_texture_attach(
  mln_map* map, const mln_vulkan_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_owned_descriptor(descriptor);
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
    "scaled texture dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  const auto vulkan_status = validate_vulkan_handles(*descriptor);
  if (vulkan_status != MLN_STATUS_OK) {
    return vulkan_status;
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
  session->texture.api_kind = TextureSessionApi::Vulkan;
  session->texture.mode = TextureSessionMode::Owned;
  session->texture.backend = std::make_unique<VulkanTextureSessionBackend>(
    *descriptor, mbgl::Size{session->physical_width, session->physical_height}
  );
  return attach_render_session(
    std::move(session), out_session, RenderSessionKind::Texture,
    RenderSessionAttachMessages{
      .null_session = "texture session must not be null",
      .null_output = "out_session must not be null",
      .non_null_output = "out_session must point to a null handle"
    }
  );
}

auto vulkan_borrowed_texture_attach(
  mln_map* map, const mln_vulkan_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_borrowed_descriptor(descriptor);
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
    "scaled texture dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  auto handle_descriptor = mln_vulkan_owned_texture_descriptor{
    .size = sizeof(mln_vulkan_owned_texture_descriptor),
    .width = descriptor->width,
    .height = descriptor->height,
    .scale_factor = descriptor->scale_factor,
    .instance = descriptor->instance,
    .physical_device = descriptor->physical_device,
    .device = descriptor->device,
    .graphics_queue = descriptor->graphics_queue,
    .graphics_queue_family_index = descriptor->graphics_queue_family_index,
  };
  const auto vulkan_status = validate_vulkan_handles(handle_descriptor);
  if (vulkan_status != MLN_STATUS_OK) {
    return vulkan_status;
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
  session->texture.api_kind = TextureSessionApi::Vulkan;
  session->texture.mode = TextureSessionMode::Borrowed;
  session->texture.backend = std::make_unique<VulkanTextureSessionBackend>(
    *descriptor, mbgl::Size{session->physical_width, session->physical_height}
  );
  return attach_render_session(
    std::move(session), out_session, RenderSessionKind::Texture,
    RenderSessionAttachMessages{
      .null_session = "texture session must not be null",
      .null_output = "out_session must not be null",
      .non_null_output = "out_session must point to a null handle"
    }
  );
}

auto vulkan_owned_texture_acquire_frame(
  mln_render_session* texture, mln_vulkan_owned_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_frame == nullptr ||
    out_frame->size < sizeof(mln_vulkan_owned_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (texture->texture.acquired) {
    set_thread_error("a texture frame is already acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->rendered_generation != texture->generation) {
    set_thread_error("no rendered frame is available for this generation");
    return MLN_STATUS_INVALID_STATE;
  }
  if (
    texture->texture.mode != TextureSessionMode::Owned ||
    texture->texture.api_kind != TextureSessionApi::Vulkan
  ) {
    set_thread_error("texture session cannot expose a Vulkan texture frame");
    return MLN_STATUS_UNSUPPORTED;
  }

  const auto acquire_status =
    texture->texture.backend->acquire_vulkan_owned_frame(*texture, *out_frame);
  if (acquire_status != MLN_STATUS_OK) {
    return acquire_status;
  }
  texture->texture.acquired = true;
  texture->texture.acquired_frame_id = out_frame->frame_id;
  texture->texture.acquired_frame_kind = TextureSessionFrameKind::VulkanOwned;
  ++texture->texture.next_frame_id;
  return MLN_STATUS_OK;
}

auto vulkan_owned_texture_release_frame(
  mln_render_session* texture, const mln_vulkan_owned_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    frame == nullptr || frame->size < sizeof(mln_vulkan_owned_texture_frame)
  ) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    !texture->texture.acquired ||
    texture->texture.acquired_frame_kind != TextureSessionFrameKind::VulkanOwned
  ) {
    set_thread_error("no texture frame is currently acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (frame->generation != texture->generation) {
    set_thread_error("frame generation does not match acquired frame");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (frame->frame_id != texture->texture.acquired_frame_id) {
    set_thread_error("frame identity does not match acquired frame");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  texture->texture.acquired = false;
  texture->texture.acquired_frame_id = 0;
  texture->texture.acquired_frame_kind = TextureSessionFrameKind::None;
  return MLN_STATUS_OK;
}

auto metal_owned_texture_descriptor_default() noexcept
  -> mln_metal_owned_texture_descriptor {
  return mln_metal_owned_texture_descriptor{
    .size = sizeof(mln_metal_owned_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .device = nullptr,
  };
}

auto metal_borrowed_texture_descriptor_default() noexcept
  -> mln_metal_borrowed_texture_descriptor {
  return mln_metal_borrowed_texture_descriptor{
    .size = sizeof(mln_metal_borrowed_texture_descriptor),
    .width = 256,
    .height = 256,
    .scale_factor = 1.0,
    .texture = nullptr,
  };
}

auto metal_owned_texture_attach(
  mln_map* map, const mln_metal_owned_texture_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_metal_owned_descriptor(descriptor);
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
    "scaled texture dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_borrowed_texture_attach(
  mln_map* map, const mln_metal_borrowed_texture_descriptor* descriptor,
  mln_render_session** out_session
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_metal_borrowed_descriptor(descriptor);
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
    "scaled texture dimensions are too large"
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_owned_texture_acquire_frame(
  mln_render_session* texture, mln_metal_owned_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_frame == nullptr ||
    out_frame->size < sizeof(mln_metal_owned_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_owned_texture_release_frame(
  mln_render_session* texture, const mln_metal_owned_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (frame == nullptr || frame->size < sizeof(mln_metal_owned_texture_frame)) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core

// NOLINTEND(misc-include-cleaner)
