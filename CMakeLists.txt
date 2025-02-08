cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
include(cmake/CPM.cmake)

project(maplibre-native-ffi)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set renderer to opengl or metal depending on platform to pass to options
if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(renderer_option "MLN_WITH_METAL ON" "MLN_WITH_OPENGL OFF")
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(CORE_ANIMATION QuartzCore REQUIRED)
else ()
    set(renderer_option "MLN_WITH_OPENGL ON")
endif ()

CPMAddPackage(
        NAME maplibre-native
        GITHUB_REPOSITORY maplibre/maplibre-native
        GIT_TAG android-v11.7.1
        EXCLUDE_FROM_ALL ON
        SYSTEM ON
        OPTIONS
        "MLN_WITH_WERROR OFF"
        "MLN_LEGACY_RENDERER OFF"
        "MLN_DRAWABLE_RENDERER ON"
        ${renderer_option})

set(sources
        "src/ClientOptions.cpp"
        "src/ResourceOptions.cpp"
        "src/MapOptions.cpp")

if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(sources ${sources}
        "src/MetalRendererBackend.mm"
        "src/MetalRendererFrontend.mm")
endif ()

add_library(${PROJECT_NAME} SHARED ${sources})

set(public_headers
        "include/ClientOptions.h"
        "include/ResourceOptions.h"
        "include/MapOptions.h")

if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(public_headers ${public_headers}
        "include/MetalRendererBackend.h"
        "include/MetalRendererFrontend.h")
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${public_headers}")

include_directories("include")

target_link_libraries(${PROJECT_NAME} PRIVATE mbgl-core)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Tests
enable_testing()

set(test_sources
    "test/main.cpp"
    "test/MapOptionsTest.cpp"
    "test/ClientOptionsTest.cpp"
    "test/ResourceOptionsTest.cpp")

if(APPLE)
    set(test_sources ${test_sources}
        "test/MetalRendererBackendTest.mm"
        "test/MetalRendererFrontendTest.mm")
endif()

add_executable(tests ${test_sources})
target_link_libraries(tests PRIVATE ${PROJECT_NAME})

if(APPLE)
    target_link_libraries(tests
        PRIVATE
            ${METAL_LIBRARY}
            ${CORE_ANIMATION}
    )
endif()

add_test(NAME tests COMMAND tests)
