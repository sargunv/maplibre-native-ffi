#include "MapOptions.h"
#include <cassert>
#include <cstdio>

#define TEST(name) void test_##name()
#define RUN_TEST(name)                                                         \
  do                                                                           \
  {                                                                            \
    printf("Running " #name "...");                                            \
    test_##name();                                                             \
    printf(" OK\n");                                                           \
  } while (0)
#define ASSERT(cond) assert(cond)

TEST(create_destroy)
{
  void *options = MLN_MapOptions_new();
  ASSERT(options != nullptr);
  MLN_MapOptions_delete(options);
}

TEST(map_mode)
{
  void *options = MLN_MapOptions_new();
  MLN_MapOptions_setMapMode(options, MLN_MapMode_Static);
  ASSERT(MLN_MapOptions_mapMode(options) == MLN_MapMode_Static);
  MLN_MapOptions_delete(options);
}

TEST(constrain_mode)
{
  void *options = MLN_MapOptions_new();
  MLN_MapOptions_setConstrainMode(options, MLN_ConstrainMode_WidthAndHeight);
  ASSERT(
    MLN_MapOptions_constrainMode(options) == MLN_ConstrainMode_WidthAndHeight
  );
  MLN_MapOptions_delete(options);
}

TEST(viewport_mode)
{
  void *options = MLN_MapOptions_new();
  MLN_MapOptions_setViewportMode(options, MLN_ViewportMode_FlippedY);
  ASSERT(MLN_MapOptions_viewportMode(options) == MLN_ViewportMode_FlippedY);
  MLN_MapOptions_delete(options);
}

TEST(cross_source_collisions)
{
  void *options = MLN_MapOptions_new();
  MLN_MapOptions_setCrossSourceCollisions(options, false);
  ASSERT(MLN_MapOptions_crossSourceCollisions(options) == false);
  MLN_MapOptions_delete(options);
}

TEST(size)
{
  void *options = MLN_MapOptions_new();
  MLN_Size size = {800.0F, 600.0F};
  MLN_MapOptions_setSize(options, size);
  MLN_Size got = MLN_MapOptions_size(options);
  ASSERT(got.width == size.width);
  ASSERT(got.height == size.height);
  MLN_MapOptions_delete(options);
}

TEST(pixel_ratio)
{
  void *options = MLN_MapOptions_new();
  float ratio = 2.0F;
  MLN_MapOptions_setPixelRatio(options, ratio);
  ASSERT(MLN_MapOptions_pixelRatio(options) == ratio);
  MLN_MapOptions_delete(options);
}

auto main() -> int
{
  printf("Running MapOptions tests...\n");
  RUN_TEST(create_destroy);
  RUN_TEST(map_mode);
  RUN_TEST(constrain_mode);
  RUN_TEST(viewport_mode);
  RUN_TEST(cross_source_collisions);
  RUN_TEST(size);
  RUN_TEST(pixel_ratio);
  printf("All tests passed!\n");
  return 0;
}
