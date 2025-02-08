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

add_library(${PROJECT_NAME} SHARED ${sources})

set(public_headers
        "include/ClientOptions.h"
        "include/ResourceOptions.h"
        "include/MapOptions.h")

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${public_headers}")

include_directories("include")

target_link_libraries(${PROJECT_NAME} PRIVATE mbgl-core)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Tests
enable_testing()

add_executable(test_map_options "test/MapOptionsTest.cpp")
target_link_libraries(test_map_options PRIVATE ${PROJECT_NAME})
add_test(NAME test_map_options COMMAND test_map_options)

add_executable(test_client_options "test/ClientOptionsTest.cpp")
target_link_libraries(test_client_options PRIVATE ${PROJECT_NAME})
add_test(NAME test_client_options COMMAND test_client_options)

add_executable(test_resource_options "test/ResourceOptionsTest.cpp")
target_link_libraries(test_resource_options PRIVATE ${PROJECT_NAME})
add_test(NAME test_resource_options COMMAND test_resource_options)
