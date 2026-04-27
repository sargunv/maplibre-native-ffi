#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <mbgl/actor/actor_ref.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_request.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/util/async_request.hpp>
#include <mbgl/util/chrono.hpp>

#include "core/custom_resource_provider.hpp"

#include "core/diagnostics.hpp"
#include "maplibre_native_abi.h"

struct mln_resource_request_handle {
  explicit mln_resource_request_handle(
    mbgl::ActorRef<mbgl::FileSourceRequest> actor_
  )
      : actor(std::move(actor_)) {}

  std::atomic_size_t refs{2};
  mutable std::mutex mutex;
  bool cancelled = false;
  bool completed = false;
  mbgl::ActorRef<mbgl::FileSourceRequest> actor;
};

namespace mln::core {
namespace {

auto error_response(std::string message, mbgl::Response::Error::Reason reason)
  -> mbgl::Response {
  auto response = mbgl::Response{};
  response.error =
    std::make_unique<mbgl::Response::Error>(reason, std::move(message));
  return response;
}

auto to_unix_ms(const mbgl::Timestamp& timestamp) -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
           timestamp.time_since_epoch()
  )
    .count();
}

auto from_unix_ms(std::int64_t unix_ms) -> mbgl::Timestamp {
  return std::chrono::time_point_cast<mbgl::Seconds>(
    std::chrono::time_point<
      std::chrono::system_clock, std::chrono::milliseconds>{
      std::chrono::milliseconds{unix_ms}
    }
  );
}

auto kind_to_abi(mbgl::Resource::Kind kind) -> std::uint32_t {
  switch (kind) {
    case mbgl::Resource::Kind::Style:
      return MLN_RESOURCE_KIND_STYLE;
    case mbgl::Resource::Kind::Source:
      return MLN_RESOURCE_KIND_SOURCE;
    case mbgl::Resource::Kind::Tile:
      return MLN_RESOURCE_KIND_TILE;
    case mbgl::Resource::Kind::Glyphs:
      return MLN_RESOURCE_KIND_GLYPHS;
    case mbgl::Resource::Kind::SpriteImage:
      return MLN_RESOURCE_KIND_SPRITE_IMAGE;
    case mbgl::Resource::Kind::SpriteJSON:
      return MLN_RESOURCE_KIND_SPRITE_JSON;
    case mbgl::Resource::Kind::Image:
      return MLN_RESOURCE_KIND_IMAGE;
    case mbgl::Resource::Kind::Unknown:
    default:
      return MLN_RESOURCE_KIND_UNKNOWN;
  }
}

auto loading_method_to_abi(mbgl::Resource::LoadingMethod method)
  -> std::uint32_t {
  switch (method) {
    case mbgl::Resource::LoadingMethod::CacheOnly:
      return MLN_RESOURCE_LOADING_METHOD_CACHE_ONLY;
    case mbgl::Resource::LoadingMethod::NetworkOnly:
      return MLN_RESOURCE_LOADING_METHOD_NETWORK_ONLY;
    case mbgl::Resource::LoadingMethod::All:
    case mbgl::Resource::LoadingMethod::None:
    default:
      return MLN_RESOURCE_LOADING_METHOD_ALL;
  }
}

auto error_reason_from_abi(std::uint32_t reason)
  -> mbgl::Response::Error::Reason {
  switch (reason) {
    case MLN_RESOURCE_ERROR_REASON_NOT_FOUND:
      return mbgl::Response::Error::Reason::NotFound;
    case MLN_RESOURCE_ERROR_REASON_SERVER:
      return mbgl::Response::Error::Reason::Server;
    case MLN_RESOURCE_ERROR_REASON_CONNECTION:
      return mbgl::Response::Error::Reason::Connection;
    case MLN_RESOURCE_ERROR_REASON_RATE_LIMIT:
      return mbgl::Response::Error::Reason::RateLimit;
    case MLN_RESOURCE_ERROR_REASON_OTHER:
    case MLN_RESOURCE_ERROR_REASON_NONE:
    default:
      return mbgl::Response::Error::Reason::Other;
  }
}

auto response_from_abi(const mln_resource_response& provider_response)
  -> mbgl::Response {
  if (provider_response.size < sizeof(mln_resource_response)) {
    return error_response(
      "mln_resource_response.size is too small",
      mbgl::Response::Error::Reason::Other
    );
  }
  switch (provider_response.status) {
    case MLN_RESOURCE_RESPONSE_STATUS_OK:
    case MLN_RESOURCE_RESPONSE_STATUS_ERROR:
    case MLN_RESOURCE_RESPONSE_STATUS_NO_CONTENT:
    case MLN_RESOURCE_RESPONSE_STATUS_NOT_MODIFIED:
      break;
    default:
      return error_response(
        "resource provider returned an unknown response status",
        mbgl::Response::Error::Reason::Other
      );
  }
  if (provider_response.byte_count != 0 && provider_response.bytes == nullptr) {
    return error_response(
      "resource provider returned a null byte buffer",
      mbgl::Response::Error::Reason::Other
    );
  }

  auto response = mbgl::Response{};
  response.noContent =
    provider_response.status == MLN_RESOURCE_RESPONSE_STATUS_NO_CONTENT;
  response.notModified =
    provider_response.status == MLN_RESOURCE_RESPONSE_STATUS_NOT_MODIFIED;
  response.mustRevalidate = provider_response.must_revalidate;
  if (provider_response.has_modified) {
    response.modified = from_unix_ms(provider_response.modified_unix_ms);
  }
  if (provider_response.has_expires) {
    response.expires = from_unix_ms(provider_response.expires_unix_ms);
  }
  if (provider_response.etag != nullptr) {
    response.etag = std::string{provider_response.etag};
  }

  if (provider_response.status == MLN_RESOURCE_RESPONSE_STATUS_ERROR) {
    auto message = std::string{"resource provider failed"};
    if (
      provider_response.error_message != nullptr &&
      *provider_response.error_message != '\0'
    ) {
      message = provider_response.error_message;
    }
    auto retry_after = std::optional<mbgl::Timestamp>{};
    if (provider_response.has_retry_after) {
      retry_after = from_unix_ms(provider_response.retry_after_unix_ms);
    }
    response.error = std::make_unique<mbgl::Response::Error>(
      error_reason_from_abi(provider_response.error_reason), std::move(message),
      retry_after
    );
    return response;
  }

  if (!response.notModified && !response.noContent) {
    auto data = std::make_shared<std::string>();
    data->resize(provider_response.byte_count);
    if (provider_response.byte_count != 0) {
      const auto bytes =
        std::span{provider_response.bytes, provider_response.byte_count};
      std::ranges::copy(bytes, data->begin());
    }
    response.data = std::move(data);
  }
  return response;
}

void release(mln_resource_request_handle* handle) noexcept {
  if (handle == nullptr) {
    return;
  }
  if (handle->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete handle;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

auto bytes_from_string(const std::string& value) -> const std::uint8_t* {
  // C APIs conventionally expose byte buffers as uint8_t even when native data
  // is stored as std::string.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<const std::uint8_t*>(value.data());
}

auto make_request_view(const mbgl::Resource& resource) -> mln_resource_request {
  const auto* prior_data = resource.priorData == nullptr
                             ? nullptr
                             : bytes_from_string(*resource.priorData);
  auto request = mln_resource_request{
    .size = sizeof(mln_resource_request),
    .url = resource.url.c_str(),
    .kind = kind_to_abi(resource.kind),
    .loading_method = loading_method_to_abi(resource.loadingMethod),
    .priority = resource.priority == mbgl::Resource::Priority::Low
                  ? MLN_RESOURCE_PRIORITY_LOW
                  : MLN_RESOURCE_PRIORITY_REGULAR,
    .usage = resource.usage == mbgl::Resource::Usage::Offline
               ? MLN_RESOURCE_USAGE_OFFLINE
               : MLN_RESOURCE_USAGE_ONLINE,
    .storage_policy =
      resource.storagePolicy == mbgl::Resource::StoragePolicy::Volatile
        ? MLN_RESOURCE_STORAGE_POLICY_VOLATILE
        : MLN_RESOURCE_STORAGE_POLICY_PERMANENT,
    .has_range = resource.dataRange.has_value(),
    .range_start = resource.dataRange ? resource.dataRange->first : 0,
    .range_end = resource.dataRange ? resource.dataRange->second : 0,
    .has_prior_modified = resource.priorModified.has_value(),
    .prior_modified_unix_ms =
      resource.priorModified ? to_unix_ms(*resource.priorModified) : 0,
    .has_prior_expires = resource.priorExpires.has_value(),
    .prior_expires_unix_ms =
      resource.priorExpires ? to_unix_ms(*resource.priorExpires) : 0,
    .prior_etag = resource.priorEtag ? resource.priorEtag->c_str() : nullptr,
    .prior_data = prior_data,
    .prior_data_size =
      resource.priorData == nullptr ? 0 : resource.priorData->size(),
  };
  return request;
}

struct CustomProviderInvocation {
  mln_resource_request_handle* handle = nullptr;
  mbgl::Resource resource;
  mln_resource_provider_callback callback = nullptr;
  void* user_data = nullptr;
};

auto invoke_custom_provider(CustomProviderInvocation invocation) noexcept
  -> bool {
  try {
    auto was_cancelled = false;
    {
      const std::scoped_lock lock(invocation.handle->mutex);
      if (invocation.handle->cancelled) {
        was_cancelled = true;
      }
    }
    if (was_cancelled) {
      release(invocation.handle);
      return true;
    }
    const auto request = make_request_view(invocation.resource);
    const auto decision =
      invocation.callback(invocation.user_data, &request, invocation.handle);
    if (decision == MLN_RESOURCE_PROVIDER_DECISION_PASS_THROUGH) {
      release(invocation.handle);
      return false;
    }
    if (decision != MLN_RESOURCE_PROVIDER_DECISION_HANDLE) {
      auto response = mln_resource_response{
        .size = sizeof(mln_resource_response),
        .status = MLN_RESOURCE_RESPONSE_STATUS_ERROR,
        .error_reason = MLN_RESOURCE_ERROR_REASON_OTHER,
        .bytes = nullptr,
        .byte_count = 0,
        .error_message = "resource provider returned an unknown decision",
        .must_revalidate = false,
        .has_modified = false,
        .modified_unix_ms = 0,
        .has_expires = false,
        .expires_unix_ms = 0,
        .etag = nullptr,
        .has_retry_after = false,
        .retry_after_unix_ms = 0,
      };
      static_cast<void>(
        complete_resource_request(invocation.handle, &response)
      );
      release(invocation.handle);
    }
    return true;
  } catch (...) {
    auto response = mln_resource_response{
      .size = sizeof(mln_resource_response),
      .status = MLN_RESOURCE_RESPONSE_STATUS_ERROR,
      .error_reason = MLN_RESOURCE_ERROR_REASON_OTHER,
      .bytes = nullptr,
      .byte_count = 0,
      .error_message = "resource provider threw an exception",
      .must_revalidate = false,
      .has_modified = false,
      .modified_unix_ms = 0,
      .has_expires = false,
      .expires_unix_ms = 0,
      .etag = nullptr,
      .has_retry_after = false,
      .retry_after_unix_ms = 0,
    };
    try {
      static_cast<void>(
        complete_resource_request(invocation.handle, &response)
      );
    } catch (...) {
      static_cast<void>(response);
    }
    release(invocation.handle);
    return true;
  }
}

}  // namespace

auto request_custom_resource(
  const mbgl::Resource& resource,
  mln_resource_provider_callback provider_callback, void* user_data,
  mbgl::FileSource::Callback file_source_callback
) -> std::unique_ptr<mbgl::AsyncRequest> {
  auto request =
    std::make_unique<mbgl::FileSourceRequest>(std::move(file_source_callback));
  auto* handle = new mln_resource_request_handle{request->actor()};
  request->onCancel([handle]() noexcept -> void {
    {
      const std::scoped_lock lock(handle->mutex);
      handle->cancelled = true;
    }
    release(handle);
  });
  try {
    const auto handled = invoke_custom_provider(
      CustomProviderInvocation{
        .handle = handle,
        .resource = resource,
        .callback = provider_callback,
        .user_data = user_data,
      }
    );
    return handled ? std::move(request) : nullptr;
  } catch (...) {
    auto response = mln_resource_response{
      .size = sizeof(mln_resource_response),
      .status = MLN_RESOURCE_RESPONSE_STATUS_ERROR,
      .error_reason = MLN_RESOURCE_ERROR_REASON_OTHER,
      .bytes = nullptr,
      .byte_count = 0,
      .error_message = "resource request setup failed",
      .must_revalidate = false,
      .has_modified = false,
      .modified_unix_ms = 0,
      .has_expires = false,
      .expires_unix_ms = 0,
      .etag = nullptr,
      .has_retry_after = false,
      .retry_after_unix_ms = 0,
    };
    static_cast<void>(complete_resource_request(handle, &response));
    release(handle);
    return request;
  }
}

auto complete_resource_request(
  mln_resource_request_handle* handle, const mln_resource_response* response
) -> mln_status {
  if (handle == nullptr || response == nullptr) {
    set_thread_error("resource request handle and response must not be null");
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  auto native_response = response_from_abi(*response);
  {
    const std::scoped_lock lock(handle->mutex);
    if (handle->cancelled) {
      return MLN_STATUS_INVALID_STATE;
    }
    if (handle->completed) {
      set_thread_error("resource request is already completed");
      return MLN_STATUS_INVALID_STATE;
    }
    handle->completed = true;
    try {
      handle->actor.invoke(
        &mbgl::FileSourceRequest::setResponse, std::move(native_response)
      );
    } catch (...) {
      set_thread_error("resource request can no longer accept a response");
      return MLN_STATUS_INVALID_STATE;
    }
  }
  return MLN_STATUS_OK;
}

auto resource_request_cancelled(
  const mln_resource_request_handle* handle, bool* out_cancelled
) -> mln_status {
  if (handle == nullptr || out_cancelled == nullptr) {
    set_thread_error(
      "resource request handle and out_cancelled must not be null"
    );
    return MLN_STATUS_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(handle->mutex);
  *out_cancelled = handle->cancelled;
  return MLN_STATUS_OK;
}

void release_resource_request(mln_resource_request_handle* handle) noexcept {
  release(handle);
}

}  // namespace mln::core
