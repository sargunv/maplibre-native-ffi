#include <cstdio>

void run_map_options_tests();
void run_client_options_tests();
void run_resource_options_tests();
#ifdef __APPLE__
void run_metal_renderer_backend_tests();
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
  printf("Running MetalRendererBackend tests...\n");
  run_metal_renderer_backend_tests();
  printf("\n");
#endif

  printf("All tests passed!\n");
  return 0;
}
