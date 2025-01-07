cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
include(cmake/CPM.cmake)

project(maplibre-native-ffi)

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
        ${renderer_option}
)

add_library(${PROJECT_NAME} SHARED
        "src/ClientOptions.cpp")

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER
        "include/ClientOptions.h")

include_directories("include")

target_link_libraries(${PROJECT_NAME} PRIVATE mbgl-core)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
