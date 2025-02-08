#include "ResourceOptions.h"
#include <cassert>
#include <cstdio>
#include <cstring>

void test_create_destroy()
{
  printf("Running create_destroy... ");
  auto *options = MLN_ResourceOptions_new();
  assert(options != nullptr && "Failed to create ResourceOptions");
  MLN_ResourceOptions_delete(options);
  printf("OK\n");
}

void test_cache_path()
{
  printf("Running cache_path... ");
  auto *options = MLN_ResourceOptions_new();
  const char *path = "/tmp/cache";
  MLN_ResourceOptions_setCachePath(options, path);
  assert(strcmp(MLN_ResourceOptions_cachePath(options), path) == 0);
  MLN_ResourceOptions_delete(options);
  printf("OK\n");
}

void test_asset_path()
{
  printf("Running asset_path... ");
  auto *options = MLN_ResourceOptions_new();
  const char *path = "/tmp/assets";
  MLN_ResourceOptions_setAssetPath(options, path);
  assert(strcmp(MLN_ResourceOptions_assetPath(options), path) == 0);
  MLN_ResourceOptions_delete(options);
  printf("OK\n");
}

void run_resource_options_tests()
{
  test_create_destroy();
  test_cache_path();
  test_asset_path();
}
