#include "MetalRendererFrontend.h"
#include "MetalRendererBackend.h"

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/renderer/renderer_frontend.hpp>
#include <mbgl/util/run_loop.hpp>

#include <memory>

// Forward declarations
struct MLN_MapObserver_Interface;

class MLN_MetalRendererFrontend final
{
private:
  MLN_MetalRendererBackend *backend_{nullptr};
  MLN_MapObserver_Interface *observer_{nullptr};
  bool continuous_rendering_{false};

public:
  explicit MLN_MetalRendererFrontend(
    const MLN_MetalRendererFrontend_Options *options
  )
      : backend_(options->backend), observer_(options->observer),
        continuous_rendering_(options->continuous_rendering)
  {
    if (backend_ == nullptr)
    {
      throw std::runtime_error("Invalid Metal backend");
    }
  }

  ~MLN_MetalRendererFrontend() = default;

  // Prevent copying
  MLN_MetalRendererFrontend(const MLN_MetalRendererFrontend &) = delete;
  auto operator=(const MLN_MetalRendererFrontend &)
    -> MLN_MetalRendererFrontend & = delete;

  void setObserver(MLN_MapObserver_Interface *observer) noexcept
  {
    observer_ = observer;
  }

  void update() noexcept
  {
    // TODO: Update state and trigger observer callbacks
  }

  void render() noexcept
  {
    // TODO: Implement rendering using Metal backend
  }

  void setContinuousRendering(bool enabled) noexcept
  {
    continuous_rendering_ = enabled;
  }

  [[nodiscard]] auto isContinuousRendering() const noexcept -> bool
  {
    return continuous_rendering_;
  }
};

// C API Implementation
extern "C"
{
  auto MLN_MetalRendererFrontend_new(
    const MLN_MetalRendererFrontend_Options *options
  ) -> MLN_MetalRendererFrontend *
  {
    try
    {
      auto frontend = std::make_unique<MLN_MetalRendererFrontend>(options);
      return reinterpret_cast<MLN_MetalRendererFrontend *>(frontend.release());
    }
    catch (const std::exception &e)
    {
      // TODO: Add proper error handling
      return nullptr;
    }
  }

  void MLN_MetalRendererFrontend_delete(MLN_MetalRendererFrontend *frontend)
  {
    delete frontend;
  }

  void MLN_MetalRendererFrontend_setObserver(
    MLN_MetalRendererFrontend *frontend, MLN_MapObserver_Interface *observer
  )
  {
    frontend->setObserver(observer);
  }

  void MLN_MetalRendererFrontend_update(MLN_MetalRendererFrontend *frontend)
  {
    frontend->update();
  }

  void MLN_MetalRendererFrontend_render(MLN_MetalRendererFrontend *frontend)
  {
    frontend->render();
  }

  void MLN_MetalRendererFrontend_setContinuousRendering(
    MLN_MetalRendererFrontend *frontend, bool enabled
  )
  {
    frontend->setContinuousRendering(enabled);
  }

  auto MLN_MetalRendererFrontend_isContinuousRendering(
    MLN_MetalRendererFrontend *frontend
  ) -> bool
  {
    return frontend->isContinuousRendering();
  }
}
