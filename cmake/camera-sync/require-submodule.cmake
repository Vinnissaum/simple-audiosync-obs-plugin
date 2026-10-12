# SPDX-License-Identifier: GPL-2.0-or-later
# Stops the configure step, naming the fix, when a Git submodule is not checked out.
# Include this file and call camera_sync_require_submodule(<name> <file the checkout holds>).
# Run as a script, cmake -DSUBMODULE_NAME=<name> -DSUBMODULE_MARKER=<file> -P require-submodule.cmake checks that file.

function(camera_sync_require_submodule name marker)
  if(NOT EXISTS "${marker}")
    message(FATAL_ERROR "${name} submodule missing - run: git submodule update --init")
  endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  camera_sync_require_submodule("${SUBMODULE_NAME}" "${SUBMODULE_MARKER}")
endif()
