# SPDX-License-Identifier: GPL-2.0-or-later
# Runs require-submodule.cmake for one marker file and checks that it passes or fails as expected.
# Usage: cmake -DCHECK_SCRIPT=<require-submodule.cmake> -DMARKER=<file> -DEXPECT=<pass|fail> -P <this file>

execute_process(
  COMMAND ${CMAKE_COMMAND} -DSUBMODULE_NAME=quirc -DSUBMODULE_MARKER=${MARKER} -P ${CHECK_SCRIPT}
  RESULT_VARIABLE result
  ERROR_VARIABLE errors
)
# CMake wraps long messages, so compare the message with its whitespace collapsed.
string(REGEX REPLACE "[ \n]+" " " errors "${errors}")

if(EXPECT STREQUAL pass)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "The checkout at ${MARKER} was rejected. Output:\n${errors}")
  endif()
elseif(EXPECT STREQUAL fail)
  if(result EQUAL 0)
    message(FATAL_ERROR "The missing checkout at ${MARKER} was accepted")
  endif()
  set(expected "quirc submodule missing - run: git submodule update --init")
  string(FIND "${errors}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "The message '${expected}' is missing. Output:\n${errors}")
  endif()
else()
  message(FATAL_ERROR "EXPECT must be pass or fail")
endif()
