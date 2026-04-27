#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <mbgl/storage/database_file_source.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/main_resource_loader.hpp>
#include <mbgl/storage/online_file_source.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/storage/resource_transform.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/util/async_request.hpp>
#include <mbgl/util/client_options.hpp>
#include <mbgl/util/event.hpp>
#include <mbgl/util/logging.hpp>
#include <mbgl/util/string.hpp>

#include "core/resource_loader.hpp"

#include "core/custom_resource_provider.hpp"
#include "core/diagnostics.hpp"
#include "core/runtime.hpp"
#include "maplibre_native_abi.h"

namespace mln::core {
namespace {

auto can_request_network(const mbgl::Resource& resource) -> bool {
  return resource.hasLoadingMethod(mbgl::Resource::LoadingMethod::Network);
}

auto resource_kind_to_abi(mbgl::Resource::Kind kind) -> uint32_t {
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

auto make_resource_transform(void* platform_context)
  -> mbgl::ResourceTransform {
  const auto* runtime = find_runtime_for_platform_context(platform_context);
  if (runtime == nullptr || runtime->resource_transform_callback == nullptr) {
    return mbgl::ResourceTransform{};
  }

  auto callback = runtime->resource_transform_callback;
  auto* user_data = runtime->resource_transform_user_data;
  return mbgl::ResourceTransform{
    [callback, user_data](
      mbgl::Resource::Kind kind, const std::string& url,
      mbgl::ResourceTransform::FinishedCallback finished
    ) -> void {
      auto response = mln_resource_transform_response{
        .size = sizeof(mln_resource_transform_response), .url = nullptr
      };
      try {
        const auto status = callback(
          user_data, resource_kind_to_abi(kind), url.c_str(), &response
        );
        if (
          status == MLN_STATUS_OK && response.url != nullptr &&
          *response.url != '\0'
        ) {
          finished(std::string{response.url});
          return;
        }
      } catch (...) {
        finished(url);
        return;
      }
      finished(url);
    }
  };
}

class AbiNetworkFileSource final : public mbgl::FileSource {
 public:
  AbiNetworkFileSource(
    const mbgl::ResourceOptions& resource_options,
    const mbgl::ClientOptions& client_options
  )
      : resource_options_(resource_options.clone()),
        client_options_(client_options.clone()),
        provider_(
          runtime_resource_provider(resource_options.platformContext())
        ),
        native_(
          std::make_unique<mbgl::OnlineFileSource>(
            resource_options, client_options
          )
        ) {
    apply_resource_transform();
  }

  auto request(const mbgl::Resource& resource, Callback callback)
    -> std::unique_ptr<mbgl::AsyncRequest> override {
    if (can_request_network(resource) && provider_.has_value()) {
      auto request = request_custom_resource(
        resource, provider_->callback, provider_->user_data, callback
      );
      if (request != nullptr) {
        return request;
      }
    }
    if (!native_->canRequest(resource)) {
      return nullptr;
    }
    return native_->request(resource, std::move(callback));
  }

  [[nodiscard]] auto canRequest(const mbgl::Resource& resource) const
    -> bool override {
    return (can_request_network(resource) && provider_.has_value()) ||
           native_->canRequest(resource);
  }

  void forward(
    const mbgl::Resource& resource, const mbgl::Response& response,
    std::function<void()> callback
  ) override {
    native_->forward(resource, response, std::move(callback));
  }

  [[nodiscard]] auto supportsCacheOnlyRequests() const -> bool override {
    return native_->supportsCacheOnlyRequests();
  }

  void pause() override { native_->pause(); }

  void resume() override { native_->resume(); }

  void setResourceTransform(mbgl::ResourceTransform transform) override {
    native_->setResourceTransform(std::move(transform));
  }

  void setResourceOptions(mbgl::ResourceOptions options) override {
    resource_options_ = options.clone();
    provider_ = runtime_resource_provider(resource_options_.platformContext());
    native_->setResourceOptions(std::move(options));
    apply_resource_transform();
  }

  auto getResourceOptions() -> mbgl::ResourceOptions override {
    return resource_options_.clone();
  }

  void setClientOptions(mbgl::ClientOptions options) override {
    client_options_ = options.clone();
    native_->setClientOptions(std::move(options));
  }

  auto getClientOptions() -> mbgl::ClientOptions override {
    return client_options_.clone();
  }

 private:
  static auto runtime_resource_provider(void* platform_context)
    -> std::optional<ResourceProvider> {
    const auto* runtime = find_runtime_for_platform_context(platform_context);
    if (runtime == nullptr || !runtime->has_resource_provider) {
      return std::nullopt;
    }
    return runtime->resource_provider;
  }

  void apply_resource_transform() {
    native_->setResourceTransform(
      make_resource_transform(resource_options_.platformContext())
    );
  }

  mbgl::ResourceOptions resource_options_;
  mbgl::ClientOptions client_options_;
  std::optional<ResourceProvider> provider_;
  std::unique_ptr<mbgl::FileSource> native_;
};

}  // namespace

auto make_network_file_source(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource> {
  try {
    return std::make_unique<AbiNetworkFileSource>(
      resource_options, client_options
    );
  } catch (const std::exception& exception) {
    set_thread_error(exception);
    return nullptr;
  } catch (...) {
    set_thread_error("network file source construction failed");
    return nullptr;
  }
}

auto make_database_file_source(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource> {
  try {
    auto source = std::make_unique<mbgl::DatabaseFileSource>(
      resource_options, client_options
    );
    const auto* runtime =
      find_runtime_for_platform_context(resource_options.platformContext());
    if (runtime != nullptr && runtime->has_maximum_cache_size) {
      source->setMaximumAmbientCacheSize(
        runtime->maximum_cache_size, [](std::exception_ptr exception) -> void {
          if (exception != nullptr) {
            mbgl::Log::Error(
              mbgl::Event::Database,
              "Failed to apply maximum ambient cache size: " +
                mbgl::util::toString(exception)
            );
          }
        }
      );
    }
    return source;
  } catch (const std::exception& exception) {
    set_thread_error(exception);
    return nullptr;
  } catch (...) {
    set_thread_error("database file source construction failed");
    return nullptr;
  }
}

auto make_main_resource_loader(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource> {
  try {
    return std::make_unique<mbgl::MainResourceLoader>(
      resource_options, client_options
    );
  } catch (const std::exception& exception) {
    set_thread_error(exception);
    return nullptr;
  } catch (...) {
    set_thread_error("main resource loader construction failed");
    return nullptr;
  }
}

}  // namespace mln::core
