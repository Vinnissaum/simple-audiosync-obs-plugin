# SPDX-License-Identifier: GPL-2.0-or-later
# Runs check-libobs-version.cmake for one version and checks that it passes or fails as expected.
# Usage: cmake -DCHECK_SCRIPT=<check-libobs-version.cmake> -DVERSION=<version> -DEXPECT=<pass|fail> -P <this file>

execute_process(
  COMMAND ${CMAKE_COMMAND} -DLIBOBS_VERSION=${VERSION} -P ${CHECK_SCRIPT}
  RESULT_VARIABLE result
  ERROR_VARIABLE errors
)
# CMake wraps long messages, so compare the message with its whitespace collapsed.
string(REGEX REPLACE "[ \n]+" " " errors "${errors}")

if(EXPECT STREQUAL pass)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "libobs ${VERSION} was rejected. Output:\n${errors}")
  endif()
elseif(EXPECT STREQUAL fail)
  if(result EQUAL 0)
    message(FATAL_ERROR "libobs ${VERSION} was accepted")
  endif()
  set(expected "Camera Sync builds against libobs 30.0.x; found ${VERSION}")
  string(FIND "${errors}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "The message '${expected}' is missing. Output:\n${errors}")
  endif()
else()
  message(FATAL_ERROR "EXPECT must be pass or fail")
endif()
