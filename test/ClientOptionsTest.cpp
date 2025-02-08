#include "ClientOptions.h"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace
{

void test_create_destroy()
{
  printf("Running create_destroy... ");
  auto *options = MLN_ClientOptions_new();
  assert(options != nullptr && "Failed to create ClientOptions");
  MLN_ClientOptions_delete(options);
  printf("OK\n");
}

void test_name()
{
  printf("Running name... ");
  auto *options = MLN_ClientOptions_new();
  const char *name = "test-client";
  MLN_ClientOptions_setName(options, name);
  assert(strcmp(MLN_ClientOptions_name(options), name) == 0);
  MLN_ClientOptions_delete(options);
  printf("OK\n");
}

} // namespace

void run_client_options_tests()
{
  test_create_destroy();
  test_name();
}
