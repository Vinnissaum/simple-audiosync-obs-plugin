# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.28...3.30)

# The core-only configuration builds the OBS-free core and its unit tests. It needs no OBS, obs-deps or Qt, so it skips
# the bootstrap, which sets up the OBS dependencies.
option(CAMERA_SYNC_CORE_ONLY "Build only the OBS-free core and its unit tests" OFF)

# Instruments the core and its tests so that gcovr can measure the core's line coverage. It needs GCC or Clang.
option(CAMERA_SYNC_COVERAGE "Instrument the core and its tests for line coverage" OFF)

# Builds the libFuzzer target for the QR payload parser. It needs Clang. The corpus replay in CTest builds without it.
option(CAMERA_SYNC_FUZZ "Build the libFuzzer target for the QR payload parser" OFF)

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

include(cmake/camera-sync/check-libobs-version.cmake)
camera_sync_check_libobs_version("${libobs_VERSION}")

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs camera-sync-core)

# The load line names the quirc commit, because quirc_version() says "1.0" at every revision. It is the first 7
# characters of the commit the submodule is checked out at.
execute_process(
  COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR}/deps/quirc rev-parse HEAD
  OUTPUT_VARIABLE quirc_commit
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE quirc_commit_result
)
if(NOT quirc_commit_result EQUAL 0 OR NOT quirc_commit MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "Cannot read the quirc commit from deps/quirc with git")
endif()
string(SUBSTRING "${quirc_commit}" 0 7 quirc_commit)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE CAMERA_SYNC_QUIRC_COMMIT="${quirc_commit}")

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
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

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/camera-sync-dock.cpp
    src/controller.cpp
    src/obs/obs-catalog.cpp
    src/obs/selection-store.cpp
    src/obs/mic-tap.cpp
    src/obs/camera-tap.cpp
    src/obs/obs-tap-factory.cpp
    src/obs/obs-settings-port.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Apply and Undo against a real libobs. Only the Linux build has libobs built from OBS's sources, which the tests also
# compile OBS's Video Delay (Async) from.
if(OS_LINUX)
  add_subdirectory(tests/integration)
endif()
