# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.28...3.30)

# The core-only configuration builds the OBS-free core and its unit tests. It needs no OBS, obs-deps or Qt, so it skips
# the bootstrap, which sets up the OBS dependencies.
option(CAMERA_SYNC_CORE_ONLY "Build only the OBS-free core and its unit tests" OFF)

if(CAMERA_SYNC_CORE_ONLY)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)
  string(JSON _name GET ${buildspec} name)
  string(JSON _version GET ${buildspec} version)

  project(${_name} VERSION ${_version} LANGUAGES C CXX)

  enable_testing()
  add_subdirectory(core)
  add_subdirectory(tests)
  return()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_subdirectory(core)

enable_testing()
add_subdirectory(tests)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
