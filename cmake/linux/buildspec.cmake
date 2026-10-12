# CMake Linux build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# _setup_obs_studio_linux: Build libobs and obs-frontend-api from the OBS sources and install them for find_package.
# OBS 30.0.2 builds on Linux only through its legacy CMake, which has no OBS_CMAKE_VERSION switch and installs its
# development files in the obs_libraries component.
function(_setup_obs_studio_linux)
  set(_build_dir "${dependencies_dir}/${_obs_destination}/build_${arch}")

  message(STATUS "Configure ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${dependencies_dir}/${_obs_destination}" -B "${_build_dir}" -G Ninja
      -DCMAKE_BUILD_TYPE=Release -DENABLE_PLUGINS:BOOL=OFF -DENABLE_UI:BOOL=OFF -DENABLE_SCRIPTING:BOOL=OFF
      -DOBS_VERSION_OVERRIDE:STRING=${_obs_version} -DCMAKE_COMPILE_WARNING_AS_ERROR:BOOL=OFF
      -DCMAKE_C_FLAGS=-Wno-error=deprecated-declarations -DCMAKE_CXX_FLAGS=-Wno-error=deprecated-declarations
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Configure ${label} (${arch}) - done")

  message(STATUS "Build ${label} (${arch})")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build_dir}" --target libobs obs-frontend-api --parallel
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Build ${label} (${arch}) - done")

  message(STATUS "Install ${label} (${arch})")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${_build_dir}" --component obs_libraries --prefix "${dependencies_dir}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Install ${label} (${arch}) - done")
endfunction()

# _check_dependencies_linux: Set up Linux slice for _check_dependencies
function(_check_dependencies_linux)
  set(arch ${CMAKE_SYSTEM_PROCESSOR})
  set(platform ubuntu-${arch})

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(obs-studio_filename "VERSION.tar.gz")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list obs-studio)

  _check_dependencies()
endfunction()

_check_dependencies_linux()
