# SPDX-License-Identifier: GPL-2.0-or-later
# Runs check-notices.cmake against a tree built here, with unmarked files in checked and in skipped places.
# Usage: cmake -DCHECK_SCRIPT=<check-notices.cmake> -DWORK_DIR=<scratch dir> -P check-notices-fixture.cmake

string(ASCII 35 hash)
set(spdx "SPDX-License-Identifier: GPL-2.0-or-later")

function(write_fixture path first_line)
  file(WRITE "${WORK_DIR}/${path}" "${first_line}\nint unused;\n")
endfunction()

function(run_check)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -DROOT=${WORK_DIR} -P ${CHECK_SCRIPT}
    RESULT_VARIABLE result
    ERROR_VARIABLE errors
    OUTPUT_VARIABLE output
  )
  set(check_result ${result} PARENT_SCOPE)
  set(check_output "${errors}${output}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")

# Marked files the check must accept.
write_fixture(CMakeLists.txt "${hash} ${spdx}")
write_fixture(src/good-source.cpp "// ${spdx}")
write_fixture(core/good-header.hpp "// ${spdx}")
write_fixture(tests/good-test.cpp "// ${spdx}")
write_fixture(cmake/camera-sync/good-check.cmake "${hash} ${spdx}")

# Files outside the check's reach, without the line.
write_fixture(cmake/common/skipped-common.cmake "${hash} template file")
write_fixture(cmake/linux/skipped-linux.cmake "${hash} template file")
write_fixture(cmake/macos/skipped-macos.cmake "${hash} template file")
write_fixture(cmake/windows/skipped-windows.cmake "${hash} template file")
write_fixture(third_party/doctest/skipped-third-party.h "// vendored file")
write_fixture(deps/quirc/skipped-deps.c "// vendored file")
write_fixture(tests/fixtures/skipped-fixture.cpp "// generated file")
write_fixture(core/detector/skipped-detector.cpp "// vendored file")

# Files the check must name.
write_fixture(src/unmarked-source.cpp "// Copyright (C) somebody")
write_fixture(core/unmarked-second-line.hpp "// first line")
file(APPEND "${WORK_DIR}/core/unmarked-second-line.hpp" "// ${spdx}\n")

run_check()
if(check_result EQUAL 0)
  message(FATAL_ERROR "check-notices accepted a tree with unmarked files")
endif()
foreach(named src/unmarked-source.cpp core/unmarked-second-line.hpp)
  string(FIND "${check_output}" "${named}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "check-notices did not name ${named}. Output:\n${check_output}")
  endif()
endforeach()
foreach(
  silent
  CMakeLists.txt
  src/good-source.cpp
  core/good-header.hpp
  tests/good-test.cpp
  cmake/camera-sync/good-check.cmake
  skipped-common.cmake
  skipped-linux.cmake
  skipped-macos.cmake
  skipped-windows.cmake
  skipped-third-party.h
  skipped-deps.c
  skipped-fixture.cpp
  skipped-detector.cpp
)
  string(FIND "${check_output}" "${silent}" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "check-notices named ${silent}, which it must accept or skip. Output:\n${check_output}")
  endif()
endforeach()

# With the line on the first line of every checked file, the check passes.
write_fixture(src/unmarked-source.cpp "// ${spdx}")
write_fixture(core/unmarked-second-line.hpp "// ${spdx}")
run_check()
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "check-notices rejected a fully marked tree. Output:\n${check_output}")
endif()
