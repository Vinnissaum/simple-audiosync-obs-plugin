# SPDX-License-Identifier: GPL-2.0-or-later
# The core holds the plugin's logic and includes no libobs, OBS frontend or Qt header.
add_library(camera-sync-core STATIC)

target_sources(camera-sync-core PRIVATE load-line.cpp)

target_include_directories(camera-sync-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(camera-sync-core PUBLIC cxx_std_17)

# The plugin module is a shared library, so the core has to be position independent.
set_target_properties(camera-sync-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT MSVC)
  target_compile_options(camera-sync-core PRIVATE -Wall -Wextra)
endif()
