# SPDX-License-Identifier: GPL-2.0-or-later
include("${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake")

add_executable(camera-sync-tests)

target_sources(
  camera-sync-tests
  PRIVATE
    main.cpp
    alignment/types.cpp
    alignment/calculator.cpp
    core/load-line.cpp
    core/log-lines.cpp
    detector/quirc.cpp
    detector/video-detector.cpp
    detector/audio-detector.cpp
    detector/video-cycles.cpp
    detector/pattern-sync.cpp
    reading/pattern.cpp
    reading/estimator.cpp
    reading/estimator-replay.cpp
    reading/camera-pipeline.cpp
    reading/mic-pipeline.cpp
    reading/session.cpp
    selection/selection-policy.cpp
    viewmodel/strings.cpp
    viewmodel/view-model.cpp
)

target_compile_definitions(
  camera-sync-tests
  PRIVATE
    CAMERA_SYNC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    CAMERA_SYNC_LOCALE_FILE="${PROJECT_SOURCE_DIR}/data/locale/en-US.ini"
)

target_include_directories(camera-sync-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

# Shared test helpers, included as "support/<name>.hpp".
target_include_directories(camera-sync-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(camera-sync-tests PRIVATE camera-sync-core)

# Core header code that only the tests instantiate counts toward the core's coverage, so the tests are instrumented too.
# The core is linked whole, because gcovr reads only the counts that a linked object writes at exit: a core source that
# no test reaches would otherwise drop out of the report instead of showing as uncovered.
if(CAMERA_SYNC_COVERAGE)
  target_compile_options(camera-sync-tests PRIVATE --coverage)
  target_link_libraries(camera-sync-tests PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,camera-sync-core>")
endif()

# The QR payload parser takes text from whatever a camera sees. Every build replays the fuzzer's seed corpus through the
# fuzz target's function, and the test fails if a payload that is out of range is accepted.
add_executable(qr-payload-replay fuzz/qr-payload-replay.cpp fuzz/qr-payload-fuzz.cpp)

target_link_libraries(qr-payload-replay PRIVATE camera-sync-core)

# The macOS CI step builds only camera-sync-tests before it runs CTest, so building that target builds the replay too.
add_dependencies(camera-sync-tests qr-payload-replay)

add_test(NAME fuzz_corpus_replay COMMAND qr-payload-replay ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus)

# With Clang, the same function builds as a libFuzzer target. The fuzzer and the parser's code are instrumented, and the
# core library, which the parser's header does not need, is not.
if(CAMERA_SYNC_FUZZ)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "CAMERA_SYNC_FUZZ needs Clang, found ${CMAKE_CXX_COMPILER_ID}")
  endif()

  add_executable(qr-payload-fuzz fuzz/qr-payload-fuzz.cpp)

  target_compile_options(qr-payload-fuzz PRIVATE -fsanitize=fuzzer,address)
  target_link_options(qr-payload-fuzz PRIVATE -fsanitize=fuzzer,address)
  target_link_libraries(qr-payload-fuzz PRIVATE camera-sync-core)
endif()

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
