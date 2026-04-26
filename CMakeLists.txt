cmake_minimum_required(VERSION 3.24)

project(
  maplibre_native_experiments
  VERSION 0.1.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(maplibre_native_abi SHARED
  src/abi/hello.cpp
)
add_library(maplibre_native_experiments::abi ALIAS maplibre_native_abi)

target_include_directories(maplibre_native_abi
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(maplibre_native_abi PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  C_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  OUTPUT_NAME maplibre_native_abi
)
