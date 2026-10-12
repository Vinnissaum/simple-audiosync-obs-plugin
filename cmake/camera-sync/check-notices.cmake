# SPDX-License-Identifier: GPL-2.0-or-later
# Fails, naming each file, when a C, C++ or CMake file the project writes does not start with the SPDX line, or when a
# file derived from Audio Video Sync Dock, which sits in core/detector/, does not start with Sync Dock's notice followed
# by the line that says where the file derives from.
# Usage: cmake -DROOT=<repository root> -P check-notices.cmake

if(NOT ROOT)
  message(FATAL_ERROR "check-notices: pass the tree to check with -DROOT=<dir>")
endif()

# The first line of a file is "// <identifier>" in C and C++, and "# <identifier>" in CMake.
string(ASCII 35 hash)
set(identifier "SPDX-License-Identifier: GPL-2.0-or-later")
set(c_first_line "// ${identifier}")
set(cmake_first_line "${hash} ${identifier}")

# Directories the project writes. The template's cmake/common, cmake/linux, cmake/macos and cmake/windows are not here.
set(checked_dirs src core tests cmake/camera-sync)
# Inside those, generated files carry no notice.
set(skipped_prefixes tests/fixtures/)
# Inside those, the files derived from Sync Dock carry Sync Dock's notice instead of the SPDX line.
set(derived_prefix core/detector/)
set(derived_commit e4ad2941ed5f2982e9f17b76ebaf22096319f3cb)
file(READ "${CMAKE_CURRENT_LIST_DIR}/sync-dock-notice.txt" sync_dock_notice)
string(REPLACE "\r\n" "\n" sync_dock_notice "${sync_dock_notice}")
string(LENGTH "${sync_dock_notice}" sync_dock_notice_length)

set(files "${ROOT}/CMakeLists.txt")
foreach(dir IN LISTS checked_dirs)
  file(GLOB_RECURSE found "${ROOT}/${dir}/*.c" "${ROOT}/${dir}/*.cpp" "${ROOT}/${dir}/*.h" "${ROOT}/${dir}/*.hpp")
  file(GLOB_RECURSE found_cmake "${ROOT}/${dir}/*.cmake" "${ROOT}/${dir}/CMakeLists.txt")
  list(APPEND files ${found} ${found_cmake})
endforeach()
list(SORT files)

set(unmarked "")
set(underived "")
foreach(path IN LISTS files)
  file(RELATIVE_PATH relative "${ROOT}" "${path}")

  set(skip FALSE)
  foreach(prefix IN LISTS skipped_prefixes)
    if(relative MATCHES "^${prefix}")
      set(skip TRUE)
    endif()
  endforeach()
  if(skip)
    continue()
  endif()

  if(relative MATCHES "^${derived_prefix}")
    file(READ "${path}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(LENGTH "${content}" content_length)
    set(derived_ok FALSE)
    if(content_length GREATER_EQUAL sync_dock_notice_length)
      string(SUBSTRING "${content}" 0 ${sync_dock_notice_length} head)
      string(SUBSTRING "${content}" ${sync_dock_notice_length} -1 rest)
      set(provenance "^// Derived from Audio Video Sync Dock, src/[A-Za-z0-9_./-]+ at ${derived_commit}\n")
      if("${head}" STREQUAL "${sync_dock_notice}" AND "${rest}" MATCHES "${provenance}")
        set(derived_ok TRUE)
      endif()
    endif()
    if(NOT derived_ok)
      list(APPEND underived "${relative}")
    endif()
    continue()
  endif()

  if(path MATCHES "\\.cmake$" OR path MATCHES "CMakeLists\\.txt$")
    set(expected "${cmake_first_line}")
  else()
    set(expected "${c_first_line}")
  endif()

  file(STRINGS "${path}" first_line LIMIT_COUNT 1)
  if(NOT "${first_line}" STREQUAL "${expected}")
    list(APPEND unmarked "${relative}")
  endif()
endforeach()

set(problems "")
if(unmarked)
  list(JOIN unmarked "\n  " unmarked_text)
  string(APPEND problems "The first line must be the SPDX line '${identifier}' in:\n  ${unmarked_text}\n")
endif()
if(underived)
  list(JOIN underived "\n  " underived_text)
  set(derived_line "'// Derived from Audio Video Sync Dock, <upstream path> at ${derived_commit}'")
  set(derived_help "The file must start with Sync Dock's notice, then ${derived_line}, in:")
  string(APPEND problems "${derived_help}\n  ${underived_text}\n")
endif()
if(problems)
  message(FATAL_ERROR "${problems}")
endif()
