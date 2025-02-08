#include "ClientOptions.h"
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
  void *options = MLN_ClientOptions_new();
  ASSERT(options != nullptr);
  MLN_ClientOptions_delete(options);
}

TEST(name)
{
  void *options = MLN_ClientOptions_new();
  const char *name = "MyTestApp";
  MLN_ClientOptions_setName(options, name);
  const char *got = MLN_ClientOptions_name(options);
  ASSERT(strcmp(got, name) == 0);
  MLN_ClientOptions_delete(options);
}

auto main() -> int
{
  printf("Running ClientOptions tests...\n");
  RUN_TEST(create_destroy);
  RUN_TEST(name);
  printf("All tests passed!\n");
  return 0;
}
