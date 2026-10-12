# SPDX-License-Identifier: GPL-2.0-or-later
# Runs check-notices.cmake against a tree built here, with unmarked files in checked and in skipped places, and with
# files derived from Sync Dock in core/detector/ that miss its notice or the line that says where they derive from.
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

# Sync Dock's notice, exactly as the check reads it, and the line that follows it in a derived file.
get_filename_component(check_dir "${CHECK_SCRIPT}" DIRECTORY)
file(READ "${check_dir}/sync-dock-notice.txt" notice)
foreach(
  required
  "Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>"
  "either version 2 of the License, or\n(at your option) any later version."
)
  string(FIND "${notice}" "${required}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "sync-dock-notice.txt lacks: ${required}")
  endif()
endforeach()
set(sync_dock_commit e4ad2941ed5f2982e9f17b76ebaf22096319f3cb)
set(provenance "// Derived from Audio Video Sync Dock, src/sync-test-output.cpp at ${sync_dock_commit}")

function(write_derived path notice_text provenance_line)
  file(WRITE "${WORK_DIR}/${path}" "${notice_text}${provenance_line}\nint unused;\n")
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")

# Marked files the check must accept.
write_fixture(src/good-source.cpp "// ${spdx}")
write_fixture(src/good-source.c "// ${spdx}")
write_fixture(core/good-header.hpp "// ${spdx}")
write_fixture(core/good-header.h "// ${spdx}")
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

# Derived files the check must accept: Sync Dock's notice, then the provenance line.
write_derived(core/detector/good-detector.cpp "${notice}" "${provenance}")
write_derived(core/detector/good-detector.hpp "${notice}" "${provenance}")

# Files the check must name.
write_fixture(CMakeLists.txt "${hash} template line")
write_fixture(src/unmarked-source.cpp "// Copyright (C) somebody")
write_fixture(src/unmarked-c-source.c "// Copyright (C) somebody")
write_fixture(core/unmarked-c-header.h "// Copyright (C) somebody")
write_fixture(tests/unmarked-test.cpp "// Copyright (C) somebody")
write_fixture(cmake/camera-sync/unmarked-check.cmake "${hash} Copyright (C) somebody")
write_fixture(core/unmarked-second-line.hpp "// first line")
file(APPEND "${WORK_DIR}/core/unmarked-second-line.hpp" "// ${spdx}\n")

# Derived files the check must name: no provenance line, no notice, an altered notice, another commit.
write_derived(core/detector/missing-provenance.cpp "${notice}" "// Derived from Audio Video Sync Dock")
write_derived(core/detector/missing-notice.cpp "" "${provenance}")
string(REPLACE "2023" "2024" altered_notice "${notice}")
write_derived(core/detector/altered-notice.cpp "${altered_notice}" "${provenance}")
string(REPLACE "${sync_dock_commit}" "0000000000000000000000000000000000000000" other_commit "${provenance}")
write_derived(core/detector/other-commit.cpp "${notice}" "${other_commit}")

run_check()
if(check_result EQUAL 0)
  message(FATAL_ERROR "check-notices accepted a tree with unmarked files")
endif()
foreach(
  named
  src/unmarked-source.cpp
  src/unmarked-c-source.c
  core/unmarked-c-header.h
  tests/unmarked-test.cpp
  cmake/camera-sync/unmarked-check.cmake
  core/unmarked-second-line.hpp
  core/detector/missing-provenance.cpp
  core/detector/missing-notice.cpp
  core/detector/altered-notice.cpp
  core/detector/other-commit.cpp
)
  string(FIND "${check_output}" "${named}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "check-notices did not name ${named}. Output:\n${check_output}")
  endif()
endforeach()
# The top-level CMakeLists.txt is named on its own, not as the tail of another path.
if(NOT check_output MATCHES "(^|[ \n])CMakeLists\\.txt")
  message(FATAL_ERROR "check-notices did not name CMakeLists.txt. Output:\n${check_output}")
endif()
foreach(
  silent
  src/good-source.cpp
  src/good-source.c
  core/good-header.hpp
  core/good-header.h
  tests/good-test.cpp
  cmake/camera-sync/good-check.cmake
  skipped-common.cmake
  skipped-linux.cmake
  skipped-macos.cmake
  skipped-windows.cmake
  skipped-third-party.h
  skipped-deps.c
  skipped-fixture.cpp
  good-detector.cpp
  good-detector.hpp
)
  string(FIND "${check_output}" "${silent}" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "check-notices named ${silent}, which it must accept or skip. Output:\n${check_output}")
  endif()
endforeach()

# With the line on the first line of every checked file, the check passes.
write_fixture(CMakeLists.txt "${hash} ${spdx}")
write_fixture(src/unmarked-source.cpp "// ${spdx}")
write_fixture(src/unmarked-c-source.c "// ${spdx}")
write_fixture(core/unmarked-c-header.h "// ${spdx}")
write_fixture(tests/unmarked-test.cpp "// ${spdx}")
write_fixture(cmake/camera-sync/unmarked-check.cmake "${hash} ${spdx}")
write_fixture(core/unmarked-second-line.hpp "// ${spdx}")
foreach(fixed missing-provenance missing-notice altered-notice other-commit)
  write_derived(core/detector/${fixed}.cpp "${notice}" "${provenance}")
endforeach()
run_check()
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "check-notices rejected a fully marked tree. Output:\n${check_output}")
endif()
