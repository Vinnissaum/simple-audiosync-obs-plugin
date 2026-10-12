# SPDX-License-Identifier: GPL-2.0-or-later
# Fails, naming each file, when a C, C++ or CMake file the project writes does not start with the SPDX line.
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
# Inside those, derived and generated files carry another notice or none.
set(skipped_prefixes tests/fixtures/ core/detector/)

set(files "${ROOT}/CMakeLists.txt")
foreach(dir IN LISTS checked_dirs)
  file(GLOB_RECURSE found "${ROOT}/${dir}/*.c" "${ROOT}/${dir}/*.cpp" "${ROOT}/${dir}/*.h" "${ROOT}/${dir}/*.hpp")
  file(GLOB_RECURSE found_cmake "${ROOT}/${dir}/*.cmake" "${ROOT}/${dir}/CMakeLists.txt")
  list(APPEND files ${found} ${found_cmake})
endforeach()
list(SORT files)

set(unmarked "")
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

if(unmarked)
  list(JOIN unmarked "\n  " unmarked_text)
  message(FATAL_ERROR "The first line must be the SPDX line '${identifier}' in:\n  ${unmarked_text}")
endif()
