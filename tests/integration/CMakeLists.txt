# SPDX-License-Identifier: GPL-2.0-or-later
# Apply and Undo against a real libobs (ALIGN-77 to ALIGN-80). The Linux plugin build has what they need: libobs built
# from OBS 30.0.2's sources, which stay in .deps, and OBS's own Video Delay (Async) is compiled from them into the test
# with nothing copied into this repository. CI runs the tests under xvfb-run, since libobs's X11 hotkeys open a display.
include("${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake")

set(obs_filters "${PROJECT_SOURCE_DIR}/.deps/obs-studio-${libobs_VERSION}/plugins/obs-filters")
set(async_delay_filter "${obs_filters}/async-delay-filter.c")
if(NOT EXISTS "${async_delay_filter}")
  message(FATAL_ERROR "OBS sources missing: ${async_delay_filter}")
endif()

add_executable(camera-sync-integration-tests)

target_sources(
  camera-sync-integration-tests
  PRIVATE
    main.cpp
    obs-env.cpp
    settings-port.cpp
    alignment.cpp
    ${PROJECT_SOURCE_DIR}/src/obs/obs-settings-port.cpp
    ${async_delay_filter}
)

# OBS's own file is built as OBS builds it, with this project's warnings off.
set_source_files_properties(${async_delay_filter} PROPERTIES COMPILE_OPTIONS "-w")

target_include_directories(camera-sync-integration-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

# The OBS port from src/, and the shared test helpers as "support/<name>.hpp".
target_include_directories(camera-sync-integration-tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/tests)

target_link_libraries(camera-sync-integration-tests PRIVATE OBS::libobs camera-sync-core)

# One CTest test per doctest test case. Listing them runs the executable, which starts libobs only for a case.
doctest_discover_tests(camera-sync-integration-tests)
