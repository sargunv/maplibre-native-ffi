#include "MapOptions.h"
#include <cassert>
#include <cstdio>

namespace
{

void test_create_destroy()
{
  printf("Running create_destroy... ");
  auto *options = MLN_MapOptions_new();
  assert(options != nullptr && "Failed to create MapOptions");
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

void test_map_mode()
{
  printf("Running map_mode... ");
  auto *options = MLN_MapOptions_new();
  MLN_MapOptions_setMapMode(options, MLN_MapMode_Static);
  assert(MLN_MapOptions_mapMode(options) == MLN_MapMode_Static);
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

void test_constrain_mode()
{
  printf("Running constrain_mode... ");
  auto *options = MLN_MapOptions_new();
  MLN_MapOptions_setConstrainMode(options, MLN_ConstrainMode_HeightOnly);
  assert(MLN_MapOptions_constrainMode(options) == MLN_ConstrainMode_HeightOnly);
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

void test_viewport_mode()
{
  printf("Running viewport_mode... ");
  auto *options = MLN_MapOptions_new();
  MLN_MapOptions_setViewportMode(options, MLN_ViewportMode_FlippedY);
  assert(MLN_MapOptions_viewportMode(options) == MLN_ViewportMode_FlippedY);
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

void test_cross_source_collisions()
{
  printf("Running cross_source_collisions... ");
  auto *options = MLN_MapOptions_new();
  MLN_MapOptions_setCrossSourceCollisions(options, false);
  assert(!MLN_MapOptions_crossSourceCollisions(options));
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

void test_size()
{
  printf("Running size... ");
  auto *options = MLN_MapOptions_new();
  MLN_Size const size = {800.0F, 600.0F};
  MLN_MapOptions_setSize(options, size);
  MLN_Size const got = MLN_MapOptions_size(options);
  assert(got.width == size.width);
  assert(got.height == size.height);
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

void test_pixel_ratio()
{
  printf("Running pixel_ratio... ");
  auto *options = MLN_MapOptions_new();
  MLN_MapOptions_setPixelRatio(options, 2.0F);
  assert(MLN_MapOptions_pixelRatio(options) == 2.0F);
  MLN_MapOptions_delete(options);
  printf("OK\n");
}

} // namespace

void run_map_options_tests()
{
  test_create_destroy();
  test_map_mode();
  test_constrain_mode();
  test_viewport_mode();
  test_cross_source_collisions();
  test_size();
  test_pixel_ratio();
}
