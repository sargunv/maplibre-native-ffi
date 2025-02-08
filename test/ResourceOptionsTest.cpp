#include "ResourceOptions.h"
#include <cassert>
#include <cstdio>
#include <cstring>

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
  void *options = MLN_ResourceOptions_new();
  ASSERT(options != nullptr);
  MLN_ResourceOptions_delete(options);
}

TEST(cache_path)
{
  void *options = MLN_ResourceOptions_new();
  const char *path = "/tmp/cache";
  MLN_ResourceOptions_setCachePath(options, path);
  const char *got = MLN_ResourceOptions_cachePath(options);
  ASSERT(strcmp(got, path) == 0);
  MLN_ResourceOptions_delete(options);
}

TEST(asset_path)
{
  void *options = MLN_ResourceOptions_new();
  const char *path = "/tmp/assets";
  MLN_ResourceOptions_setAssetPath(options, path);
  const char *got = MLN_ResourceOptions_assetPath(options);
  ASSERT(strcmp(got, path) == 0);
  MLN_ResourceOptions_delete(options);
}

auto main() -> int
{
  printf("Running ResourceOptions tests...\n");
  RUN_TEST(create_destroy);
  RUN_TEST(cache_path);
  RUN_TEST(asset_path);
  printf("All tests passed!\n");
  return 0;
}
