# SPDX-License-Identifier: GPL-2.0-or-later
# Stops the configure step unless the libobs version found is 30.0.x, the version the plugin builds against.
# Include this file and call camera_sync_check_libobs_version(<version>) right after find_package(libobs).
# Run as a script, cmake -DLIBOBS_VERSION=<version> -P check-libobs-version.cmake checks that version.

function(camera_sync_check_libobs_version version)
  if(NOT version MATCHES "^30\\.0\\.")
    message(FATAL_ERROR "Camera Sync builds against libobs 30.0.x; found ${version}")
  endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  camera_sync_check_libobs_version("${LIBOBS_VERSION}")
endif()
