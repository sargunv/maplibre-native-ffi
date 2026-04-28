cmake_minimum_required(VERSION 3.25)

project(maplibre_native_experiments VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(
  MLN_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/third_party/maplibre-native"
  CACHE PATH
  "MapLibre Native source directory"
)
if(NOT EXISTS "${MLN_SOURCE_DIR}/CMakeLists.txt")
  message(
    FATAL_ERROR
    "MLN_SOURCE_DIR must point to a MapLibre Native checkout: ${MLN_SOURCE_DIR}"
  )
endif()

set(MLN_WITH_CORE_ONLY ON CACHE BOOL "Build only MapLibre Native core" FORCE)
set(MLN_WITH_GLFW OFF CACHE BOOL "Disable MapLibre Native GLFW platform" FORCE)
set(
  MLN_WITH_PMTILES
  ON
  CACHE BOOL
  "Build MapLibre Native PMTiles support"
  FORCE
)
if(APPLE)
  set(MLN_WITH_METAL ON CACHE BOOL "Build MapLibre Native Metal backend" FORCE)
else()
  set(
    MLN_WITH_VULKAN
    ON
    CACHE BOOL
    "Build MapLibre Native Vulkan backend"
    FORCE
  )
endif()
set(
  MLN_WITH_WERROR
  OFF
  CACHE BOOL
  "Do not fail wrapper builds on MapLibre Native warnings"
  FORCE
)
add_subdirectory("${MLN_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/maplibre-native")

set(
  MLN_PLATFORM_SUPPORT_SOURCES
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/util/logging_stderr.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/util/monotonic_timer.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/util/thread_local.cpp
  ${MLN_SOURCE_DIR}/platform/qt/src/mbgl/bidi.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/gfx/headless_backend.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/layermanager/layer_manager.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/asset_file_source.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/database_file_source.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/file_source_request.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/local_file_request.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/local_file_source.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/main_resource_loader.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/mbtiles_file_source.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/offline.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/offline_database.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/offline_download.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/online_file_source.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/$<IF:$<BOOL:${MLN_WITH_PMTILES}>,pmtiles_file_source.cpp,pmtiles_file_source_stub.cpp>
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/storage/sqlite3.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/platform/time.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/util/compression.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/util/filesystem.cpp
  ${MLN_SOURCE_DIR}/platform/default/src/mbgl/util/utf.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/background_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/circle_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/color_relief_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/custom_drawable_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/custom_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/fill_extrusion_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/fill_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/heatmap_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/hillshade_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/layer_manager.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/line_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/location_indicator_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/raster_layer_factory.cpp
  ${MLN_SOURCE_DIR}/src/mbgl/layermanager/symbol_layer_factory.cpp
)
set(MLN_PLATFORM_SUPPORT_LIBRARIES)
if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
  list(
    APPEND MLN_PLATFORM_SUPPORT_SOURCES
    ${MLN_SOURCE_DIR}/platform/default/src/mbgl/mtl/headless_backend.cpp
    ${MLN_SOURCE_DIR}/platform/darwin/core/async_task.cpp
    ${MLN_SOURCE_DIR}/platform/darwin/core/collator.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/http_file_source.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/image.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/local_glyph_rasterizer.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/native_apple_interface.m
    ${MLN_SOURCE_DIR}/platform/darwin/core/number_format.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/nsthread.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/run_loop.cpp
    ${MLN_SOURCE_DIR}/platform/darwin/core/string_nsstring.mm
    ${MLN_SOURCE_DIR}/platform/darwin/core/timer.cpp
  )
  list(
    APPEND MLN_PLATFORM_SUPPORT_LIBRARIES
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework CoreText"
    "-framework Foundation"
    "-framework ImageIO"
    "-framework Metal"
  )
  set_source_files_properties(
    ${MLN_SOURCE_DIR}/platform/default/src/mbgl/mtl/headless_backend.cpp
    PROPERTIES COMPILE_OPTIONS "-fno-rtti"
  )
else()
  message(
    FATAL_ERROR
    "maplibre_native_abi platform support is currently wired for Apple targets only"
  )
endif()

add_library(
  maplibre_native_abi
  SHARED
  src/abi/diagnostics.cpp
  src/abi/logging.cpp
  src/abi/map.cpp
  src/abi/network.cpp
  src/abi/runtime.cpp
  src/abi/texture.cpp
  src/abi/version.cpp
  src/diagnostics/diagnostics.cpp
  src/logging/logging.cpp
  src/map/map.cpp
  src/resources/custom_resource_provider.cpp
  src/resources/file_source_manager.cpp
  src/resources/network_status.cpp
  src/resources/resource_loader.cpp
  src/render/metal/metal_texture_session.mm
  src/runtime/runtime.cpp
  ${MLN_PLATFORM_SUPPORT_SOURCES}
)
add_library(maplibre_native_experiments::abi ALIAS maplibre_native_abi)

target_include_directories(
  maplibre_native_abi
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_include_directories(
  maplibre_native_abi
  SYSTEM
  PRIVATE
    ${MLN_SOURCE_DIR}/src
    ${MLN_SOURCE_DIR}/platform/darwin/core
    ${MLN_SOURCE_DIR}/platform/darwin/include
    ${MLN_SOURCE_DIR}/platform/darwin/src
    ${MLN_SOURCE_DIR}/platform/default/include
    ${MLN_SOURCE_DIR}/vendor/icu/include
    ${MLN_SOURCE_DIR}/vendor/PMTiles/cpp
)
target_link_libraries(
  maplibre_native_abi
  PRIVATE
    Mapbox::Map
    mbgl-vendor-metal-cpp
    mbgl-vendor-pmtiles
    sqlite3
    z
    ${MLN_PLATFORM_SUPPORT_LIBRARIES}
)
target_compile_options(
  maplibre_native_abi
  PRIVATE $<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>
)

set_target_properties(
  maplibre_native_abi
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    OUTPUT_NAME maplibre_native_abi
)
