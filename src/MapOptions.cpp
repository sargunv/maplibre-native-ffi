#include "MapOptions.h"
#include <mbgl/gfx/renderable.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/util/size.hpp>

using namespace mbgl;

auto MLN_MapOptions_new() -> void *
{
  return new MapOptions();
}

void MLN_MapOptions_delete(void *options)
{
  delete static_cast<MapOptions *>(options);
}

void MLN_MapOptions_setMapMode(void *options, MLN_MapMode mode)
{
  static_cast<MapOptions *>(options)->withMapMode(static_cast<MapMode>(mode));
}

auto MLN_MapOptions_mapMode(void *options) -> MLN_MapMode
{
  return static_cast<MLN_MapMode>(static_cast<MapOptions *>(options)->mapMode()
  );
}

void MLN_MapOptions_setConstrainMode(void *options, MLN_ConstrainMode mode)
{
  static_cast<MapOptions *>(options)->withConstrainMode(
    static_cast<ConstrainMode>(mode)
  );
}

auto MLN_MapOptions_constrainMode(void *options) -> MLN_ConstrainMode
{
  return static_cast<MLN_ConstrainMode>(
    static_cast<MapOptions *>(options)->constrainMode()
  );
}

void MLN_MapOptions_setViewportMode(void *options, MLN_ViewportMode mode)
{
  static_cast<MapOptions *>(options)->withViewportMode(
    static_cast<ViewportMode>(mode)
  );
}

auto MLN_MapOptions_viewportMode(void *options) -> MLN_ViewportMode
{
  return static_cast<MLN_ViewportMode>(
    static_cast<MapOptions *>(options)->viewportMode()
  );
}

void MLN_MapOptions_setCrossSourceCollisions(
  void *options, bool enableCollisions
)
{
  static_cast<MapOptions *>(options)->withCrossSourceCollisions(enableCollisions
  );
}

auto MLN_MapOptions_crossSourceCollisions(void *options) -> bool
{
  return static_cast<MapOptions *>(options)->crossSourceCollisions();
}

void MLN_MapOptions_setSize(void *options, MLN_Size size)
{
  static_cast<MapOptions *>(options)->withSize(
    Size{static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height)}
  );
}

auto MLN_MapOptions_size(void *options) -> MLN_Size
{
  auto size = static_cast<MapOptions *>(options)->size();
  return MLN_Size{
    static_cast<float>(size.width), static_cast<float>(size.height)
  };
}

void MLN_MapOptions_setPixelRatio(void *options, float ratio)
{
  static_cast<MapOptions *>(options)->withPixelRatio(ratio);
}

auto MLN_MapOptions_pixelRatio(void *options) -> float
{
  return static_cast<MapOptions *>(options)->pixelRatio();
}
