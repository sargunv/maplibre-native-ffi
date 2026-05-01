#include <cmath>
#include <memory>
#include <vector>

#include <mbgl/util/size.hpp>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "maplibre_native_c.h"
#include "render/texture_session.hpp"
#include "render/vulkan/vulkan_texture_backend.hpp"

namespace {
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

auto validate_metal_descriptor(const mln_metal_texture_descriptor* descriptor)
  -> mln_status {
  if (descriptor == nullptr) {
    mln::core::set_thread_error("texture descriptor must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (descriptor->size < sizeof(mln_metal_texture_descriptor)) {
    mln::core::set_thread_error(
      "mln_metal_texture_descriptor.size is too small"
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

void prepare_vulkan_render_resources(mln_texture_session* texture) {
  // Renderer::render creates the Vulkan context before requesting the default
  // renderable, so shared-device resources must be ready first.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  static_cast<mln::core::VulkanTextureBackend&>(*texture->backend)
    .prepareRenderResources();
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
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
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
  session->backend_kind = TextureSessionBackend::Vulkan;
  session->backend = std::make_unique<VulkanTextureBackend>(
    *descriptor, mbgl::Size{session->physical_width, session->physical_height}
  );
  session->prepare_render_resources = prepare_vulkan_render_resources;
  return texture_attach_session(std::move(session), out_texture);
}

auto shared_texture_attach(
  mln_map* map, const mln_shared_texture_descriptor* descriptor,
  mln_texture_session** out_texture
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_shared_texture_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("shared texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
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
  const auto resources = backend.frame_resources();
  *out_frame = mln_vulkan_texture_frame{
    .size = sizeof(mln_vulkan_texture_frame),
    .generation = texture->generation,
    .width = texture->physical_width,
    .height = texture->physical_height,
    .scale_factor = texture->scale_factor,
    .frame_id = texture->next_frame_id,
    .image = resources.image,
    .image_view = resources.image_view,
    .device = resources.device,
    .format = static_cast<uint32_t>(resources.format),
    .layout = static_cast<uint32_t>(vk::ImageLayout::eShaderReadOnlyOptimal),
  };
  texture->acquired = true;
  texture->acquired_frame_id = out_frame->frame_id;
  texture->acquired_frame_kind = TextureSessionFrameKind::Vulkan;
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
  if (
    !texture->acquired ||
    texture->acquired_frame_kind != TextureSessionFrameKind::Vulkan
  ) {
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
  texture->acquired_frame_kind = TextureSessionFrameKind::None;
  return MLN_STATUS_OK;
}

auto texture_acquire_shared_frame(
  mln_texture_session* texture, mln_shared_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_live_attached_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  const auto output_status = validate_shared_frame_output(out_frame);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  if (texture->acquired) {
    set_thread_error("a texture frame is already acquired");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->rendered_generation != texture->generation) {
    set_thread_error("no rendered frame is available for this generation");
    return MLN_STATUS_INVALID_STATE;
  }
  if (texture->backend_kind != TextureSessionBackend::Vulkan) {
    set_thread_error("texture session cannot expose a shared texture frame");
    return MLN_STATUS_UNSUPPORTED;
  }
  if (
    texture->shared_required_handle_type != MLN_SHARED_TEXTURE_HANDLE_NONE &&
    texture->shared_required_handle_type !=
      MLN_SHARED_TEXTURE_HANDLE_VULKAN_IMAGE
  ) {
    set_thread_error("requested shared texture handle type is unsupported");
    return MLN_STATUS_UNSUPPORTED;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  auto& backend = static_cast<VulkanTextureBackend&>(*texture->backend);
  const auto resources = backend.frame_resources();
  *out_frame = mln_shared_texture_frame{
    .size = sizeof(mln_shared_texture_frame),
    .generation = texture->generation,
    .width = texture->physical_width,
    .height = texture->physical_height,
    .scale_factor = texture->scale_factor,
    .frame_id = texture->next_frame_id,
    .producer_backend = MLN_TEXTURE_BACKEND_VULKAN,
    .native_handle_type = MLN_SHARED_TEXTURE_HANDLE_VULKAN_IMAGE,
    .native_handle = resources.image,
    .native_view = resources.image_view,
    .native_device = resources.device,
    .export_handle_type = MLN_SHARED_TEXTURE_HANDLE_NONE,
    .export_handle = nullptr,
    .format = static_cast<uint64_t>(resources.format),
    .layout = static_cast<uint32_t>(vk::ImageLayout::eShaderReadOnlyOptimal),
    .plane = 0,
  };
  texture->acquired = true;
  texture->acquired_frame_id = out_frame->frame_id;
  texture->acquired_frame_kind = TextureSessionFrameKind::Shared;
  ++texture->next_frame_id;
  return MLN_STATUS_OK;
}

auto texture_release_shared_frame(
  mln_texture_session* texture, const mln_shared_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (frame == nullptr || frame->size < sizeof(mln_shared_texture_frame)) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  if (
    !texture->acquired ||
    texture->acquired_frame_kind != TextureSessionFrameKind::Shared
  ) {
    set_thread_error("no shared texture frame is currently acquired");
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
  texture->acquired_frame_kind = TextureSessionFrameKind::None;
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
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_metal_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_attach_output(out_texture);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_texture_acquire_frame(
  mln_texture_session* texture, mln_metal_texture_frame* out_frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (
    out_frame == nullptr || out_frame->size < sizeof(mln_metal_texture_frame)
  ) {
    set_thread_error("out_frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto metal_texture_release_frame(
  mln_texture_session* texture, const mln_metal_texture_frame* frame
) -> mln_status {
  const auto status = validate_texture(texture);
  if (status != MLN_STATUS_OK) {
    return status;
  }
  if (frame == nullptr || frame->size < sizeof(mln_metal_texture_frame)) {
    set_thread_error("frame must not be null and must have a valid size");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  set_thread_error("Metal texture sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core
