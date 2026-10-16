# SPDX-License-Identifier: GPL-2.0-or-later
include("${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake")

add_executable(camera-sync-tests)

target_sources(
  camera-sync-tests
  PRIVATE
    main.cpp
    core/load-line.cpp
    detector/quirc.cpp
    detector/video-detector.cpp
    detector/audio-detector.cpp
    reading/pattern.cpp
    reading/estimator.cpp
)

target_compile_definitions(camera-sync-tests PRIVATE CAMERA_SYNC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_include_directories(camera-sync-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

target_link_libraries(camera-sync-tests PRIVATE camera-sync-core)

# One CTest test per doctest test case, so ctest -R <name> runs the matching test cases.
doctest_discover_tests(camera-sync-tests)

# Licence notices. The fixture case builds its own tree, and the repository case runs the same check on this one.
set(check_notices ${PROJECT_SOURCE_DIR}/cmake/camera-sync/check-notices.cmake)
set(fixture_script ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-notices-fixture.cmake)
set(fixture_dir ${CMAKE_CURRENT_BINARY_DIR}/check-notices-fixture)

add_test(
  NAME check_notices_fixture
  COMMAND ${CMAKE_COMMAND} -DCHECK_SCRIPT=${check_notices} -DWORK_DIR=${fixture_dir} -P ${fixture_script}
)

add_test(NAME check_notices_repository COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -P ${check_notices})

# The libobs version guard. One case per version.
set(check_libobs ${PROJECT_SOURCE_DIR}/cmake/camera-sync/check-libobs-version.cmake)
set(libobs_case ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-libobs-version-case.cmake)

foreach(entry 31.1.1:fail 30.1.0:fail 30.0.0:pass 30.0.2:pass)
  string(REPLACE ":" ";" parts ${entry})
  list(GET parts 0 version)
  list(GET parts 1 expect)
  add_test(
    NAME check_libobs_version_${version}_${expect}
    COMMAND ${CMAKE_COMMAND} -DCHECK_SCRIPT=${check_libobs} -DVERSION=${version} -DEXPECT=${expect} -P ${libobs_case}
  )
endforeach()

# The submodule check. One case with the checkout missing and one with it present.
set(require_submodule ${PROJECT_SOURCE_DIR}/cmake/camera-sync/require-submodule.cmake)
set(submodule_case ${CMAKE_CURRENT_SOURCE_DIR}/cmake/require-submodule-case.cmake)
set(quirc_marker ${PROJECT_SOURCE_DIR}/deps/quirc/lib/quirc.c)
set(missing_marker ${CMAKE_CURRENT_BINARY_DIR}/no-such-submodule/lib/quirc.c)

add_test(
  NAME require_submodule_missing_fail
  COMMAND
    ${CMAKE_COMMAND} -DCHECK_SCRIPT=${require_submodule} -DMARKER=${missing_marker} -DEXPECT=fail -P ${submodule_case}
)

add_test(
  NAME require_submodule_present_pass
  COMMAND
    ${CMAKE_COMMAND} -DCHECK_SCRIPT=${require_submodule} -DMARKER=${quirc_marker} -DEXPECT=pass -P ${submodule_case}
)
