#include <cstdio>

void run_map_options_tests();
void run_client_options_tests();
void run_resource_options_tests();
#ifdef __APPLE__

#endif

auto main() -> int
{
  printf("Running all tests...\n\n");

  printf("Running MapOptions tests...\n");
  run_map_options_tests();
  printf("\n");

  printf("Running ClientOptions tests...\n");
  run_client_options_tests();
  printf("\n");

  printf("Running ResourceOptions tests...\n");
  run_resource_options_tests();
  printf("\n");

#ifdef __APPLE__

#endif

  printf("All tests passed!\n");
  return 0;
}
