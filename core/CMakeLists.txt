# SPDX-License-Identifier: GPL-2.0-or-later
# The core holds the plugin's logic and includes no libobs, OBS frontend or Qt header.
include(${PROJECT_SOURCE_DIR}/cmake/camera-sync/require-submodule.cmake)

set(quirc_dir ${PROJECT_SOURCE_DIR}/deps/quirc)
camera_sync_require_submodule(quirc ${quirc_dir}/lib/quirc.c)

# quirc is third-party C, so it is built apart from the core's sources and the core's warnings stay off it.
add_library(
  camera-sync-quirc
  STATIC
  ${quirc_dir}/lib/decode.c
  ${quirc_dir}/lib/identify.c
  ${quirc_dir}/lib/quirc.c
  ${quirc_dir}/lib/version_db.c
)

target_include_directories(camera-sync-quirc SYSTEM PUBLIC ${quirc_dir}/lib)

set_target_properties(camera-sync-quirc PROPERTIES POSITION_INDEPENDENT_CODE ON COMPILE_WARNING_AS_ERROR OFF)

if(NOT MSVC)
  target_compile_options(camera-sync-quirc PRIVATE -w)
endif()

add_library(camera-sync-core STATIC)

target_sources(
  camera-sync-core
  PRIVATE
    load-line.cpp
    reading/pattern.cpp
    reading/estimator.cpp
    reading/camera-pipeline.cpp
    reading/mic-pipeline.cpp
    selection/selection-policy.cpp
    detector/video-detector.cpp
    detector/audio-detector.cpp
)

target_include_directories(camera-sync-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(camera-sync-core PUBLIC camera-sync-quirc)

target_compile_features(camera-sync-core PUBLIC cxx_std_17)

# The plugin module is a shared library, so the core has to be position independent.
set_target_properties(camera-sync-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT MSVC)
  target_compile_options(camera-sync-core PRIVATE -Wall -Wextra)
endif()
