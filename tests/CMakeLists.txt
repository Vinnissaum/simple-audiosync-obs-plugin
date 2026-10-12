# SPDX-License-Identifier: GPL-2.0-or-later
include("${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake")

add_executable(camera-sync-tests)

target_sources(camera-sync-tests PRIVATE main.cpp core/load-line.cpp)

target_include_directories(camera-sync-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

target_link_libraries(camera-sync-tests PRIVATE camera-sync-core)

# One CTest test per doctest test case, so ctest -R <name> runs the matching test cases.
doctest_discover_tests(camera-sync-tests)
