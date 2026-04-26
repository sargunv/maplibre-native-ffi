cmake_minimum_required(VERSION 3.24)

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

add_library(maplibre_native_abi SHARED src/abi/hello.cpp)
add_library(maplibre_native_experiments::abi ALIAS maplibre_native_abi)

target_include_directories(
  maplibre_native_abi
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(maplibre_native_abi PRIVATE Mapbox::Map)

set_target_properties(
  maplibre_native_abi
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    OUTPUT_NAME maplibre_native_abi
)
