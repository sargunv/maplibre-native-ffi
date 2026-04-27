#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <mbgl/storage/database_file_source.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/file_source_manager.hpp>
#include <mbgl/storage/file_source_request.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/storage/resource_transform.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/util/async_request.hpp>
#include <mbgl/util/client_options.hpp>
#include <mbgl/util/run_loop.hpp>

#include "core/resource_loader.hpp"

#include "core/custom_resource_provider.hpp"
#include "core/resource_scheme.hpp"
#include "core/runtime.hpp"
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

auto post_response(mbgl::FileSource::Callback callback, mbgl::Response response)
  -> std::unique_ptr<mbgl::AsyncRequest> {
  auto request = std::make_unique<mbgl::FileSourceRequest>(std::move(callback));
  request->actor().invoke(&mbgl::FileSourceRequest::setResponse, response);
  return request;
}

struct ChainedRequestState {
  std::mutex mutex;
  bool cancelled = false;
  mbgl::FileSource::Callback callback;
  std::unique_ptr<mbgl::AsyncRequest> child;
  std::vector<std::unique_ptr<mbgl::AsyncRequest>> keep_alive;
};

class ChainedRequest final : public mbgl::AsyncRequest {
 public:
  explicit ChainedRequest(std::shared_ptr<ChainedRequestState> state)
      : state_(std::move(state)) {}
  ChainedRequest(const ChainedRequest&) = delete;
  auto operator=(const ChainedRequest&) -> ChainedRequest& = delete;
  ChainedRequest(ChainedRequest&&) = delete;
  auto operator=(ChainedRequest&&) -> ChainedRequest& = delete;

  ~ChainedRequest() override {
    auto child = std::unique_ptr<mbgl::AsyncRequest>{};
    auto keep_alive = std::vector<std::unique_ptr<mbgl::AsyncRequest>>{};
    {
      const std::scoped_lock lock(state_->mutex);
      state_->cancelled = true;
      child = std::move(state_->child);
      keep_alive = std::move(state_->keep_alive);
    }
  }

 private:
  std::shared_ptr<ChainedRequestState> state_;
};

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

auto is_network_scheme(const std::optional<std::string>& scheme) -> bool {
  return scheme && (*scheme == "http" || *scheme == "https");
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
        local_files_(make_child(mbgl::FileSourceType::FileSystem)),
        assets_(make_child(mbgl::FileSourceType::Asset)),
        database_(make_database_child()),
        network_(make_child(mbgl::FileSourceType::Network)),
        mbtiles_(make_child(mbgl::FileSourceType::Mbtiles)),
        pmtiles_(make_child(mbgl::FileSourceType::Pmtiles)) {
    apply_resource_transform();
  }

  auto request(const mbgl::Resource& resource, Callback callback)
    -> std::unique_ptr<mbgl::AsyncRequest> override {
    const auto scheme = scheme_for_url(resource.url);
    if (assets_ != nullptr && assets_->canRequest(resource)) {
      return assets_->request(resource, std::move(callback));
    }
    if (mbtiles_ != nullptr && mbtiles_->canRequest(resource)) {
      return mbtiles_->request(resource, std::move(callback));
    }
    if (pmtiles_ != nullptr && pmtiles_->canRequest(resource)) {
      return pmtiles_->request(resource, std::move(callback));
    }
    if (local_files_ != nullptr && local_files_->canRequest(resource)) {
      return local_files_->request(resource, std::move(callback));
    }
    if (scheme) {
      if (const auto* provider = custom_provider(*scheme)) {
        return request_custom_resource(
          resource, provider->callback, provider->user_data, run_loop_,
          std::move(callback)
        );
      }
    }
    if (
      is_network_scheme(scheme) && database_ != nullptr &&
      database_->canRequest(resource)
    ) {
      return request_with_cache(resource, std::move(callback));
    }
    if (
      is_network_scheme(scheme) && network_ != nullptr &&
      network_->canRequest(resource)
    ) {
      return request_from_network(
        network_, database_, resource, std::move(callback)
      );
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
    return (assets_ != nullptr && assets_->canRequest(resource)) ||
           (mbtiles_ != nullptr && mbtiles_->canRequest(resource)) ||
           (pmtiles_ != nullptr && pmtiles_->canRequest(resource)) ||
           (local_files_ != nullptr && local_files_->canRequest(resource)) ||
           (is_network_scheme(scheme) && database_ != nullptr &&
            database_->canRequest(resource)) ||
           (is_network_scheme(scheme) && network_ != nullptr &&
            network_->canRequest(resource)) ||
           custom_provider(*scheme) != nullptr;
  }

  [[nodiscard]] auto supportsCacheOnlyRequests() const -> bool override {
    return database_ != nullptr;
  }

  void pause() override {
    for (auto* source : children()) {
      source->pause();
    }
  }

  void resume() override {
    for (auto* source : children()) {
      source->resume();
    }
  }

  void setResourceOptions(mbgl::ResourceOptions options) override {
    resource_options_ = options.clone();
    run_loop_ = runtime_run_loop(resource_options_.platformContext());
    custom_providers_ =
      runtime_resource_providers(resource_options_.platformContext());
    for (auto* source : children()) {
      source->setResourceOptions(options.clone());
    }
    apply_maximum_cache_size();
    apply_resource_transform();
  }

  auto getResourceOptions() -> mbgl::ResourceOptions override {
    return resource_options_.clone();
  }

  void setClientOptions(mbgl::ClientOptions options) override {
    client_options_ = options.clone();
    for (auto* source : children()) {
      source->setClientOptions(options.clone());
    }
  }

  auto getClientOptions() -> mbgl::ClientOptions override {
    return client_options_.clone();
  }

  void setResourceTransform(mbgl::ResourceTransform transform) override {
    if (network_ != nullptr) {
      network_->setResourceTransform(std::move(transform));
    }
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

  auto make_child(mbgl::FileSourceType type)
    -> std::shared_ptr<mbgl::FileSource> {
    return mbgl::FileSourceManager::get()->getFileSource(
      type, resource_options_.clone(), client_options_.clone()
    );
  }

  auto make_database_child() -> std::shared_ptr<mbgl::DatabaseFileSource> {
    const auto* runtime =
      find_runtime_for_platform_context(resource_options_.platformContext());
    if (runtime == nullptr) {
      return nullptr;
    }
    auto source = make_child(mbgl::FileSourceType::Database);
    auto database = std::dynamic_pointer_cast<mbgl::DatabaseFileSource>(source);
    if (database != nullptr) {
      apply_maximum_cache_size(database.get());
    }
    return database;
  }

  [[nodiscard]] auto children() const -> std::vector<mbgl::FileSource*> {
    auto values = std::vector<mbgl::FileSource*>{};
    for (const auto& child :
         {assets_, mbtiles_, pmtiles_, local_files_, network_}) {
      if (child != nullptr) {
        values.push_back(child.get());
      }
    }
    if (database_ != nullptr) {
      values.push_back(database_.get());
    }
    return values;
  }

  void apply_maximum_cache_size(mbgl::DatabaseFileSource* database) const {
    const auto* runtime =
      find_runtime_for_platform_context(resource_options_.platformContext());
    if (runtime != nullptr && runtime->has_maximum_cache_size) {
      database->setMaximumAmbientCacheSize(
        runtime->maximum_cache_size, [](std::exception_ptr) -> void {}
      );
    }
  }

  void apply_maximum_cache_size() const {
    if (database_ != nullptr) {
      apply_maximum_cache_size(database_.get());
    }
  }

  void apply_resource_transform() {
    if (network_ != nullptr) {
      network_->setResourceTransform(
        make_resource_transform(resource_options_.platformContext())
      );
    }
  }

  static auto request_from_network(
    const std::shared_ptr<mbgl::FileSource>& network,
    const std::shared_ptr<mbgl::DatabaseFileSource>& database,
    const mbgl::Resource& resource, mbgl::FileSource::Callback callback
  ) -> std::unique_ptr<mbgl::AsyncRequest> {
    if (network == nullptr || !network->canRequest(resource)) {
      return nullptr;
    }
    auto shared_resource = std::make_shared<mbgl::Resource>(resource);
    return network->request(
      resource,
      [database, shared_resource,
       callback =
         std::move(callback)](mbgl::Response response) mutable -> void {
        if (database != nullptr) {
          database->forward(*shared_resource, response, nullptr);
        }
        callback(std::move(response));
      }
    );
  }

  static auto callback_if_active(
    const std::shared_ptr<ChainedRequestState>& state
  ) -> mbgl::FileSource::Callback {
    const std::scoped_lock lock(state->mutex);
    return state->cancelled ? mbgl::FileSource::Callback{} : state->callback;
  }

  static auto is_cancelled(const std::shared_ptr<ChainedRequestState>& state)
    -> bool {
    const std::scoped_lock lock(state->mutex);
    return state->cancelled;
  }

  static void set_child_if_active(
    const std::shared_ptr<ChainedRequestState>& state,
    std::unique_ptr<mbgl::AsyncRequest> child
  ) {
    if (child == nullptr) {
      return;
    }
    const std::scoped_lock lock(state->mutex);
    if (!state->cancelled) {
      if (state->child != nullptr) {
        state->keep_alive.push_back(std::move(state->child));
      }
      state->child = std::move(child);
    }
  }

  static void deliver_response_if_active(
    const std::shared_ptr<ChainedRequestState>& state, mbgl::Response response
  ) {
    auto callback = callback_if_active(state);
    if (callback) {
      callback(std::move(response));
    }
  }

  auto request_with_cache(
    const mbgl::Resource& resource, mbgl::FileSource::Callback callback
  ) -> std::unique_ptr<mbgl::AsyncRequest> {
    if (resource.loadingMethod == mbgl::Resource::LoadingMethod::CacheOnly) {
      return database_->request(resource, std::move(callback));
    }

    auto state = std::make_shared<ChainedRequestState>();
    state->callback = std::move(callback);
    auto request = std::make_unique<ChainedRequest>(state);
    auto cache_child = database_->request(
      resource,
      [network = network_, database = database_, state,
       shared_resource = std::make_shared<mbgl::Resource>(resource)](
        mbgl::Response response
      ) mutable -> void {
        auto network_resource = *shared_resource;
        if (is_cancelled(state)) {
          return;
        }

        if (!response.noContent) {
          if (response.isUsable()) {
            deliver_response_if_active(state, response);
            if (is_cancelled(state)) {
              return;
            }
            network_resource.setPriority(mbgl::Resource::Priority::Low);
          } else {
            network_resource.priorData = response.data;
          }
          network_resource.priorModified = response.modified;
          network_resource.priorExpires = response.expires;
          network_resource.priorEtag = response.etag;
        }
        auto child = request_from_network(
          network, database, network_resource,
          [state](mbgl::Response network_response) mutable -> void {
            deliver_response_if_active(state, std::move(network_response));
          }
        );
        set_child_if_active(state, std::move(child));
      }
    );
    {
      const std::scoped_lock lock(state->mutex);
      state->child = std::move(cache_child);
    }
    return request;
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
  std::shared_ptr<mbgl::FileSource> local_files_;
  std::shared_ptr<mbgl::FileSource> assets_;
  std::shared_ptr<mbgl::DatabaseFileSource> database_;
  std::shared_ptr<mbgl::FileSource> network_;
  std::shared_ptr<mbgl::FileSource> mbtiles_;
  std::shared_ptr<mbgl::FileSource> pmtiles_;
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
