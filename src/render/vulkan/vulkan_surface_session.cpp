#include <cmath>

#include "diagnostics/diagnostics.hpp"
#include "map/map.hpp"
#include "render/surface_session.hpp"

namespace {

auto validate_metal_descriptor(const mln_metal_surface_descriptor* descriptor)
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
  mln_surface_session** out_surface
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_metal_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_surface_attach_output(out_surface);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_surface_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Metal surface sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

auto vulkan_surface_attach(
  mln_map* map, const mln_vulkan_surface_descriptor* descriptor,
  mln_surface_session** out_surface
) -> mln_status {
  const auto map_status = validate_map(map);
  if (map_status != MLN_STATUS_OK) {
    return map_status;
  }
  const auto descriptor_status = validate_vulkan_descriptor(descriptor);
  if (descriptor_status != MLN_STATUS_OK) {
    return descriptor_status;
  }
  const auto output_status = validate_surface_attach_output(out_surface);
  if (output_status != MLN_STATUS_OK) {
    return output_status;
  }
  const auto physical_status = validate_surface_physical_size(
    descriptor->width, descriptor->height, descriptor->scale_factor
  );
  if (physical_status != MLN_STATUS_OK) {
    return physical_status;
  }
  set_thread_error("Vulkan surface sessions are not supported by this build");
  return MLN_STATUS_UNSUPPORTED;
}

}  // namespace mln::core
