#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

  using MLN_MapMode = enum MLN_MapMode
  {
    MLN_MapMode_Continuous,
    MLN_MapMode_Static,
    MLN_MapMode_Tile
  };

  using MLN_ConstrainMode = enum MLN_ConstrainMode
  {
    MLN_ConstrainMode_None,
    MLN_ConstrainMode_HeightOnly,
    MLN_ConstrainMode_WidthAndHeight
  };

  using MLN_ViewportMode = enum MLN_ViewportMode
  {
    MLN_ViewportMode_Default,
    MLN_ViewportMode_FlippedY
  };

  using MLN_Size = struct MLN_Size
  {
    float width;
    float height;
  };

  auto MLN_MapOptions_new() -> void *;
  void MLN_MapOptions_delete(void *options);

  void MLN_MapOptions_setMapMode(void *options, MLN_MapMode mode);
  auto MLN_MapOptions_mapMode(void *options) -> MLN_MapMode;

  void MLN_MapOptions_setConstrainMode(void *options, MLN_ConstrainMode mode);
  auto MLN_MapOptions_constrainMode(void *options) -> MLN_ConstrainMode;

  void MLN_MapOptions_setViewportMode(void *options, MLN_ViewportMode mode);
  auto MLN_MapOptions_viewportMode(void *options) -> MLN_ViewportMode;

  void MLN_MapOptions_setCrossSourceCollisions(
    void *options, bool enableCollisions
  );
  auto MLN_MapOptions_crossSourceCollisions(void *options) -> bool;

  void MLN_MapOptions_setSize(void *options, MLN_Size size);
  auto MLN_MapOptions_size(void *options) -> MLN_Size;

  void MLN_MapOptions_setPixelRatio(void *options, float ratio);
  auto MLN_MapOptions_pixelRatio(void *options) -> float;

#ifdef __cplusplus
}
#endif
