#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mbgl/storage/asset_file_source.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_request.hpp>
#include <mbgl/storage/local_file_source.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/util/async_request.hpp>
#include <mbgl/util/client_options.hpp>
#include <mbgl/util/run_loop.hpp>

#include "core/resource_loader.hpp"

#include "core/custom_resource_provider.hpp"
#include "core/resource_scheme.hpp"
#include "core/runtime.hpp"

namespace mln::core {
namespace {

auto resource_with_scheme(
  const mbgl::Resource& resource, const std::string& scheme
) -> mbgl::Resource {
  auto normalized = resource;
  const auto separator = normalized.url.find("://");
  if (separator != std::string::npos) {
    normalized.url.replace(0, separator, scheme);
  }
  return normalized;
}

auto error_response(std::string message, mbgl::Response::Error::Reason reason)
  -> mbgl::Response {
  auto response = mbgl::Response{};
  response.error =
    std::make_unique<mbgl::Response::Error>(reason, std::move(message));
  return response;
}

auto post_response(mbgl::FileSource::Callback callback, mbgl::Response response)
  -> std::unique_ptr<mbgl::AsyncRequest> {
  auto request = std::make_unique<mbgl::FileSourceRequest>(std::move(callback));
  request->actor().invoke(&mbgl::FileSourceRequest::setResponse, response);
  return request;
}

class AbiCompositeResourceLoader final : public mbgl::FileSource {
 public:
  AbiCompositeResourceLoader(
    const mbgl::ResourceOptions& resource_options,
    const mbgl::ClientOptions& client_options
  )
      : resource_options_(resource_options.clone()),
        client_options_(client_options.clone()),
        run_loop_(runtime_run_loop(resource_options.platformContext())),
        custom_providers_(
          runtime_resource_providers(resource_options.platformContext())
        ),
        local_files_(
          std::make_unique<mbgl::LocalFileSource>(
            resource_options_.clone(), client_options_.clone()
          )
        ),
        assets_(
          std::make_unique<mbgl::AssetFileSource>(
            resource_options_.clone(), client_options_.clone()
          )
        ) {}

  auto request(const mbgl::Resource& resource, Callback callback)
    -> std::unique_ptr<mbgl::AsyncRequest> override {
    const auto scheme = scheme_for_url(resource.url);
    if (scheme && *scheme == "file") {
      return local_files_->request(
        resource_with_scheme(resource, *scheme), std::move(callback)
      );
    }
    if (scheme && *scheme == "asset") {
      return assets_->request(
        resource_with_scheme(resource, *scheme), std::move(callback)
      );
    }
    if (scheme) {
      if (const auto* provider = custom_provider(*scheme)) {
        return request_custom_resource(
          resource, provider->callback, provider->user_data, run_loop_,
          std::move(callback)
        );
      }
    }
    return post_response(
      std::move(callback), error_response(
                             "unsupported resource URL: " + resource.url,
                             mbgl::Response::Error::Reason::Other
                           )
    );
  }

  [[nodiscard]] auto canRequest(const mbgl::Resource& resource) const
    -> bool override {
    const auto scheme = scheme_for_url(resource.url);
    if (!scheme) {
      return false;
    }
    if (*scheme == "file" || *scheme == "asset") {
      return true;
    }
    return custom_provider(*scheme) != nullptr;
  }

  void pause() override {
    local_files_->pause();
    assets_->pause();
  }

  void resume() override {
    local_files_->resume();
    assets_->resume();
  }

  void setResourceOptions(mbgl::ResourceOptions options) override {
    resource_options_ = options.clone();
    run_loop_ = runtime_run_loop(resource_options_.platformContext());
    custom_providers_ =
      runtime_resource_providers(resource_options_.platformContext());
    local_files_->setResourceOptions(options.clone());
    assets_->setResourceOptions(options.clone());
  }

  auto getResourceOptions() -> mbgl::ResourceOptions override {
    return resource_options_.clone();
  }

  void setClientOptions(mbgl::ClientOptions options) override {
    client_options_ = options.clone();
    local_files_->setClientOptions(options.clone());
    assets_->setClientOptions(options.clone());
  }

  auto getClientOptions() -> mbgl::ClientOptions override {
    return client_options_.clone();
  }

 private:
  static auto runtime_run_loop(void* platform_context) noexcept
    -> mbgl::util::RunLoop* {
    const auto* runtime = find_runtime_for_platform_context(platform_context);
    if (runtime == nullptr) {
      return nullptr;
    }
    return runtime->run_loop.get();
  }

  static auto runtime_resource_providers(void* platform_context)
    -> std::vector<ResourceProvider> {
    const auto* runtime = find_runtime_for_platform_context(platform_context);
    return runtime == nullptr ? std::vector<ResourceProvider>{}
                              : runtime->resource_providers;
  }

  [[nodiscard]] auto custom_provider(const std::string& scheme) const
    -> const ResourceProvider* {
    const auto provider =
      std::ranges::find_if(custom_providers_, [&](const auto& entry) -> bool {
        return entry.scheme == scheme;
      });
    return provider == custom_providers_.end() ? nullptr : &*provider;
  }

  mbgl::ResourceOptions resource_options_;
  mbgl::ClientOptions client_options_;
  mbgl::util::RunLoop* run_loop_ = nullptr;
  std::vector<ResourceProvider> custom_providers_;
  std::unique_ptr<mbgl::LocalFileSource> local_files_;
  std::unique_ptr<mbgl::AssetFileSource> assets_;
};

}  // namespace

auto make_resource_loader(
  const mbgl::ResourceOptions& resource_options,
  const mbgl::ClientOptions& client_options
) noexcept -> std::unique_ptr<mbgl::FileSource> {
  try {
    return std::make_unique<AbiCompositeResourceLoader>(
      resource_options, client_options
    );
  } catch (...) {
    return nullptr;
  }
}

}  // namespace mln::core
