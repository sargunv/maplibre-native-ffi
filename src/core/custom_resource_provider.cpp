#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <mbgl/actor/actor_ref.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_request.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/util/async_request.hpp>
#include <mbgl/util/run_loop.hpp>

#include "core/custom_resource_provider.hpp"

#include "maplibre_native_abi.h"

namespace mln::core {
namespace {

auto error_response(std::string message, mbgl::Response::Error::Reason reason)
  -> mbgl::Response {
  auto response = mbgl::Response{};
  response.error =
    std::make_unique<mbgl::Response::Error>(reason, std::move(message));
  return response;
}

struct CustomProviderInvocation {
  mbgl::ActorRef<mbgl::FileSourceRequest> actor;
  std::shared_ptr<std::atomic_bool> cancelled;
  std::string url;
  mln_resource_provider_callback callback = nullptr;
  void* user_data = nullptr;
};

auto response_from_provider(
  mln_status status, const mln_resource_provider_response& provider_response
) -> mbgl::Response {
  if (status != MLN_STATUS_OK) {
    auto message = std::string{"resource provider failed"};
    if (
      provider_response.error_message != nullptr &&
      *provider_response.error_message != '\0'
    ) {
      message = provider_response.error_message;
    }
    return error_response(
      std::move(message), mbgl::Response::Error::Reason::Other
    );
  }
  if (provider_response.error_message != nullptr) {
    return error_response(
      provider_response.error_message, mbgl::Response::Error::Reason::Other
    );
  }
  if (provider_response.byte_count != 0 && provider_response.bytes == nullptr) {
    return error_response(
      "resource provider returned a null byte buffer",
      mbgl::Response::Error::Reason::Other
    );
  }

  auto response = mbgl::Response{};
  if (provider_response.byte_count == 0) {
    response.data = std::make_shared<const std::string>();
  } else {
    auto data = std::make_shared<std::string>();
    data->resize(provider_response.byte_count);
    const auto bytes =
      std::span{provider_response.bytes, provider_response.byte_count};
    std::ranges::copy(bytes, data->begin());
    response.data = std::move(data);
  }
  return response;
}

auto send_provider_response(
  const CustomProviderInvocation& invocation, mbgl::Response response
) noexcept -> void {
  if (invocation.cancelled->load()) {
    return;
  }
  try {
    invocation.actor.invoke(&mbgl::FileSourceRequest::setResponse, response);
  } catch (...) {
    return;
  }
}

auto invoke_custom_provider(CustomProviderInvocation invocation) noexcept
  -> void {
  try {
    if (invocation.cancelled->load()) {
      return;
    }

    auto provider_response = mln_resource_provider_response{
      .size = sizeof(mln_resource_provider_response),
      .bytes = nullptr,
      .byte_count = 0,
      .error_message = nullptr,
    };
    auto status = MLN_STATUS_NATIVE_ERROR;
    try {
      status = invocation.callback(
        invocation.user_data, invocation.url.c_str(), &provider_response
      );
    } catch (...) {
      status = MLN_STATUS_NATIVE_ERROR;
      provider_response.error_message = "resource provider threw an exception";
    }

    send_provider_response(
      invocation, response_from_provider(status, provider_response)
    );
  } catch (...) {
    try {
      send_provider_response(
        invocation,
        error_response(
          "resource provider failed", mbgl::Response::Error::Reason::Other
        )
      );
    } catch (...) {
      return;
    }
  }
}

}  // namespace

auto request_custom_resource(
  const mbgl::Resource& resource,
  mln_resource_provider_callback provider_callback, void* user_data,
  mbgl::util::RunLoop* run_loop, mbgl::FileSource::Callback file_source_callback
) -> std::unique_ptr<mbgl::AsyncRequest> {
  auto request =
    std::make_unique<mbgl::FileSourceRequest>(std::move(file_source_callback));
  auto actor = request->actor();
  try {
    assert(run_loop != nullptr);

    auto cancelled = std::make_shared<std::atomic_bool>(false);
    request->onCancel([cancelled]() -> void { cancelled->store(true); });
    auto invocation = CustomProviderInvocation{
      .actor = actor,
      .cancelled = cancelled,
      .url = resource.url,
      .callback = provider_callback,
      .user_data = user_data,
    };
    run_loop->invoke(
      [invocation = std::move(invocation)]() mutable noexcept -> void {
        invoke_custom_provider(std::move(invocation));
      }
    );
    return request;
  } catch (...) {
    try {
      actor.invoke(
        &mbgl::FileSourceRequest::setResponse,
        error_response(
          "resource request setup failed", mbgl::Response::Error::Reason::Other
        )
      );
    } catch (...) {
      static_cast<void>(actor);
    }
    return request;
  }
}

}  // namespace mln::core
