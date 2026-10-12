# Foundation Tasks

## Execution Protocol (MANDATORY -- do not skip)

Implement these tasks with the `tlc-spec-driven` skill: **activate it by name and follow its Execute flow and Critical Rules.** Do not search for skill files by filesystem path. The skill is the source of truth for the full flow (per-task cycle, sub-agent delegation, adequacy review, Verifier, discrimination sensor).

**If the skill cannot be activated, STOP and tell the user - do not proceed without it.**

---

**Spec**: `.specs/features/foundation/spec.md`
**Design**: none. Foundation is Medium, and the [technical design](../../../docs/tdd/multi-camera-sync.md) already fixes the architecture; each task carries its own build decisions.
**Status**: Draft

**Prerequisites before Execute:**

- CMake 3.28 or later and Ninja on the WSL machine (`sudo apt install cmake ninja-build` gives CMake 3.28.3 on Ubuntu 24.04). gcc 13.3 is already there.
- A way to run CI and read its results: push access for each CI-gated task, and either `gh` authenticated or the maintainer reporting each run.
- For T16 only: an OBS machine with OBS 30.0.0 and the latest stable installed side by side.

**Commit messages** follow `CLAUDE.md` (gitmoji shortcode, Conventional Commits). The skill's `check_commit.py` does not know the gitmoji prefix, so it runs on the message with the leading `:shortcode: ` removed.

**Sources checked on 2026-10-12:** plugin template `3e7d7ac`, Sync Dock `e4ad294`, quirc `542848d` (Sync Dock's pin), OBS Studio `30.0.2`.

---

## Test Coverage Matrix

> Generated from the project's guidelines and the spec. There are no tests in the repository yet. Guidelines found: `docs/tdd/multi-camera-sync.md` (Testing Strategy, Running the plan with tlc-spec-driven) and `CLAUDE.md`.

| Code Layer | Required Test Type | Coverage Expectation | Location Pattern | Run Command |
| ---------- | ------------------ | -------------------- | ---------------- | ----------- |
| Core library (`core/`, no libobs, no Qt) | unit (doctest through CTest) | Every spec AC the layer carries, 1:1, with exact expected strings and values | `tests/core/*.cpp` | quick gate |
| Vendored detector (`core/detector/`) | unit | Builds with no libobs on the include path; the QR fixture decodes to its exact generated payload; the audio fixture decodes to its generated indices; notices and provenance present. Marker-time accuracy waits for Reading's detector rework (TDD Testing Strategy, Detector) | `tests/detector/*.cpp`, fixtures in `tests/fixtures/` | quick gate |
| CMake checks (`cmake/camera-sync/`) | unit (CTest running `cmake -P`) | Accept and reject paths, with the exact failure message | `tests/cmake/*.cmake` | quick gate |
| Plugin module and dock (`src/`) | build + UAT | Builds and packages on three platforms; loads, logs and exits cleanly in OBS 30.0.0 and the latest stable | - | ci gate, then rig gate (T16) |
| Build spec, presets, CI workflows, packaging | build | The named CI jobs pass; a negative check wherever the AC names a failure | - | ci gate |
| `CLAUDE.md` | none | Every documented local command runs as written | - | quick gate |

## Gate Check Commands

> Generated from the template's workflows and the core-only configuration that T3 adds. Confirm before Execute.

| Gate Level | When to Use | Command |
| ---------- | ----------- | ------- |
| Quick | Tasks that change the core, its tests, the CMake checks or the documented commands | `cmake --preset core && cmake --build --preset core && ctest --preset core --output-on-failure` |
| CI | Tasks that change the plugin, the build spec, the presets or the workflows | Push the task's commit and wait for the run: Check Formatting, Build for Windows, Build for macOS, Build for Ubuntu and (from T5) the core-only job all pass, with three packages attached |
| Rig | T16 only | Install the CI packages into OBS 30.0.0 and the latest stable on the OBS machine and walk FND-19 to FND-24 |

Every task also keeps the earlier gates green: a CI-gated task that touches CMake reruns the quick gate first.

---

## Execution Plan

Phases are ordered and run sequentially - each phase completes before the next begins, and tasks within a phase execute in order.

### Phase 1: Scaffold

The template, renamed, with its CI pinned.

```
T1 → T2
```

### Phase 2: Core and tests

The OBS-free core with a local test gate, then CI running it everywhere.

```
T3 → T4
T3 → T5
T3 → T6
```

### Phase 3: OBS 30.0.2

Every platform built against OBS 30.0.2, then a guard that keeps it that way.

```
T7 → T9
T8 → T9
```

### Phase 4: Detector in the core

quirc, then Sync Dock's video and audio detection, each under test.

```
T10 → T11 → T12
```

### Phase 5: Dock, docs and the load check

```
T13 → T14 → T15
T14 → T16
```

---

## Task Breakdown

### Phase 1 tasks: Scaffold

#### T1: Scaffold the plugin from the template with the Camera Sync identity

**What**: Copy the plugin template at `3e7d7ac` into the repository root, keeping every existing file, and give it the working identity.
**Where**: the template tree at the root (`.github/`, `build-aux/`, `cmake/`, `data/`, `src/`, `.clang-format`, `.gersemirc`, `.gitignore`, `CMakeLists.txt`, `CMakePresets.json`, `LICENSE`, `README.md`, `buildspec.json`)
**Depends on**: None
**Reuses**: obs-plugintemplate `3e7d7ac`
**Requirement**: FND-01, FND-02, FND-12

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [x] `buildspec.json`: `name` `camera-sync`, `displayName` "Camera Sync", `version` `0.1.0`, `author` "Vinicius Braz", `website` the GitHub repository URL, `email` the maintainer's address, macOS `bundleId` `io.github.vinnissaum.camera-sync`. OBS stays at the template's 31.1.1 until T7.
- [x] `.gitignore` keeps the template's allowlist and adds `CLAUDE.md`, `.claude/`, `.agents/`, `.design/`, `.specs/`, `docs/`, `core/`, `tests/`, `third_party/` and `deps/`, so new files there are not ignored; `git status --ignored` lists none of them.
- [x] `LICENSE` is the template's GPL-2.0 text, unchanged.
- [x] `src/plugin-main.c` starts with `// SPDX-License-Identifier: GPL-2.0-or-later`, and its header block names "Camera Sync" and "Copyright (C) 2026 Vinicius Braz" in place of the template placeholders.
- [x] `README.md` is replaced by one paragraph on what the plugin does and links to the design, the TDD and the ADRs.
- [x] The scaffold commit's body records the template commit `3e7d7ac`.
- [x] CI gate passes: the four template jobs green, with a `camera-sync-0.1.0` Windows .zip, macOS .pkg and Ubuntu .deb attached.

**Tests**: none
**Gate**: ci

**Commit**: `:tada: build: scaffold the plugin from obs's plugin template`

---

#### T2: Pin every external action to a commit SHA

**What**: Replace every tag reference to an action outside the repository (`actions/checkout@v4`, `actions/cache/*@v4`, `actions/upload-artifact@v4`, `actions/download-artifact@v4` and any other) with the full commit SHA of that tag, the tag kept as a trailing comment.
**Where**: `.github/workflows/*.yaml`, `.github/actions/*/action.yaml`
**Depends on**: T1
**Reuses**: the SHA-pinned `softprops/action-gh-release` line already in `push.yaml`
**Requirement**: FND-09

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [x] Each SHA resolved with `git ls-remote` against the action's repository for the tag in use.
- [x] `grep -rnE 'uses: [^.][^@]*@' .github | grep -vE '@[0-9a-f]{40}( |$)'` prints nothing.
- [x] CI gate passes.

**Tests**: none
**Gate**: ci

**Commit**: `:lock: ci: pin every external action to a commit sha`

---

### Phase 2 tasks: Core and tests

#### T3: Add the core-only build, the core library and its first tested function

**What**: A `core` configure, build and test preset that configures without the template's bootstrap or any OBS dependency; a static library `camera-sync-core`; doctest; a test executable whose test cases CTest lists one by one; and the core's first function, which formats the load line's fields.
**Where**: `CMakeLists.txt` (core-only branch before the bootstrap), `CMakePresets.json` (`core` presets), `core/`, `tests/`, `third_party/doctest/doctest.h`
**Depends on**: T1
**Reuses**: doctest's `doctest_discover_tests` CMake script
**Requirement**: FND-13, FND-15, FND-18, FND-21 (the formatted fields)

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [x] `cmake --preset core` takes project name and version from `buildspec.json` and never includes `cmake/common/bootstrap.cmake`.
- [x] `camera-sync-core` includes no libobs, OBS frontend or Qt header and links none of their libraries.
- [x] doctest is a single pinned header, its version and MIT notice at the top of the file.
- [x] `camera_sync::format_version(uint32_t packed)` turns a libobs packed version into `"30.0.2"`, and `camera_sync::load_line(...)` returns exactly `load version=0.1.0 obs_built=30.0.2 obs_running=32.0.1 quirc=542848d` for those inputs. The `[camera-sync] ` prefix is `obs_log`'s and stays out of the core.
- [x] Tests cover each field, a zero patch, and a two-digit minor.
- [x] `ctest --preset core -R load_line` runs only the `load_line` test cases (FND-18).
- [x] The quick gate passes under `unshare -rn` (no network) from a fresh clone (FND-15).
- [x] Test count: at least 4 test cases pass.

**Tests**: unit
**Gate**: quick

**Commit**: `:package: build: add the core-only build with doctest and the load line`

---

#### T4: Check licence notices in CTest

**What**: A `cmake -P` check, run by CTest in the core configuration, that fails naming each C, C++ or CMake file the project writes whose first line is not the SPDX GPL-2.0-or-later identifier.
**Where**: `cmake/camera-sync/check-notices.cmake`, `tests/cmake/`
**Depends on**: T3
**Reuses**: none
**Requirement**: FND-12

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [x] Checked: `src/`, `core/`, `tests/`, `cmake/camera-sync/` and the top-level `CMakeLists.txt`. Skipped: the template's own `cmake/common`, `cmake/linux`, `cmake/macos`, `cmake/windows`, plus `third_party/`, `deps/`, `tests/fixtures/` and `core/detector/` (T11 adds that rule).
- [x] A CTest case runs the check against a fixture tree with one unmarked file and passes only when the output names that file.
- [x] A CTest case runs the check on the repository and passes.
- [x] Every file T3 added starts with the SPDX line.
- [x] Quick gate passes. Test count: T3's cases plus 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:white_check_mark: test: check licence notices in ctest`

---

#### T5: Run the unit tests in every CI build job and in a core-only job

**What**: The full build adds the core and its tests, so each platform job runs `ctest` after building. A new core-only Ubuntu job configures, builds and tests the `core` preset without downloading OBS, obs-deps or Qt.
**Where**: `CMakeLists.txt` (add `core/` and `tests/` to the full build), `.github/workflows/build-project.yaml`
**Depends on**: T3
**Reuses**: the template's job layout and SHA-pinned actions from T2
**Requirement**: FND-16, FND-17

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [x] Windows, macOS and Ubuntu jobs each run `ctest -C RelWithDebInfo --output-on-failure` in their build directory, and a failing test fails the job.
- [x] The core-only job checks out submodules, installs only CMake and Ninja, and runs the quick gate's commands.
- [x] Verified once on a scratch branch that is never merged: a deliberately failing test turns each of the four jobs red.
- [x] Quick gate passes, then CI gate passes with the core-only job green.

**Tests**: unit
**Gate**: ci

**Commit**: `:construction_worker: ci: run the unit tests in every build job`

---

#### T6: Check formatting in the core, tests and new CMake files

**What**: Extend the template's clang-format and gersemi file lists beyond `src/` to `core/` and `tests/` and to `cmake/camera-sync/`, `core/CMakeLists.txt` and `tests/CMakeLists.txt`, excluding `third_party/` and `deps/`.
**Where**: `build-aux/run-clang-format`, `build-aux/run-gersemi`
**Depends on**: T3
**Reuses**: the template's format scripts and `.clang-format`, `.gersemirc`
**Requirement**: FND-11

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [x] Every file in `core/`, `tests/` and `cmake/camera-sync/` is formatted.
- [x] Verified once on a scratch branch that is never merged: a misformatted `core/` file turns Check Formatting red.
- [x] CI gate passes with Check Formatting green.

**Tests**: none
**Gate**: ci

**Commit**: `:construction_worker: ci: check formatting in the core and tests`

---

### Phase 3 tasks: OBS 30.0.2

#### T7: Build Windows and macOS against OBS 30.0.2

**What**: Pin OBS Studio 30.0.2 with obs-deps and Qt 6 `2023-11-03`, adapt the template's OBS build step to 30.0.2, and target macOS 11.0.
**Where**: `buildspec.json`, `cmake/common/buildspec_common.cmake`, `CMakePresets.json`
**Depends on**: T1
**Reuses**: OBS 30.0.2's `buildspec.json` for the obs-deps and Qt hashes
**Requirement**: FND-03, FND-05, FND-06, FND-07, FND-08

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] `obs-studio` is `30.0.2`, with SHA-256 hashes of the tag's source archive computed for `macos` and `windows-x64`.
- [ ] `prebuilt` and `qt6` are `2023-11-03`, with hashes copied from OBS 30.0.2's `buildspec.json` (its `macos-universal` hash under the template's `macos` key), including Qt's Windows debug symbols.
- [ ] The OBS configure step uses 30.0.2's option names (`ENABLE_UI` in place of `ENABLE_FRONTEND`) and still builds and installs only libobs and `obs-frontend-api`.
- [ ] The `macos` preset sets `CMAKE_OSX_DEPLOYMENT_TARGET` to `11.0`.
- [ ] Verified once on a scratch branch that is never merged: one altered hash fails the configure step before any compiling (FND-08).
- [ ] CI gate passes for Windows and macOS.

**Tests**: none
**Gate**: ci

**Commit**: `:package: build: build windows and macos against obs 30.0.2`

---

#### T8: Build Ubuntu against OBS 30.0.2

**What**: Stop installing the PPA's OBS on Ubuntu. Instead build libobs and `obs-frontend-api` from the pinned 30.0.2 source archive on ubuntu-24.04, and package the .deb with an explicit OBS dependency.
**Where**: `buildspec.json` (`ubuntu-x86_64` hash), `cmake/linux/`, `.github/scripts/utils.zsh/setup_ubuntu`, `.github/workflows/build-project.yaml`
**Depends on**: T1
**Reuses**: `_check_dependencies` and `_setup_obs_studio` from `cmake/common/buildspec_common.cmake`
**Requirement**: FND-03, FND-07, FND-10

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] The OBS source archive is downloaded with `EXPECTED_HASH` from the `ubuntu-x86_64` hash in `buildspec.json`.
- [ ] `setup_ubuntu` installs OBS 30.0.2's build dependencies from Ubuntu 24.04 and no longer adds `ppa:obsproject/obs-studio`.
- [ ] The .deb is built with `CPACK_DEBIAN_PACKAGE_SHLIBDEPS` off and `CPACK_DEBIAN_PACKAGE_DEPENDS` set to `obs-studio (>= 30.0.0)`.
- [ ] A CI step fails the job unless `dpkg-deb -f <package> Depends` prints exactly `obs-studio (>= 30.0.0)`.
- [ ] CI gate passes for Ubuntu.
- [ ] If this costs more than one working day, stop and take the spec's fallback (an ubuntu-22.04 runner with a prebuilt OBS 30 development environment, as Sync Dock does). Record the switch in `.specs/STATE.md` before continuing.

**Tests**: none
**Gate**: ci

**Commit**: `:package: build: build ubuntu against obs 30.0.2`

---

#### T9: Refuse libobs headers other than 30.0

**What**: A CMake function that stops the configure step unless the libobs version found is 30.0.x, called right after `find_package(libobs)`.
**Where**: `cmake/camera-sync/check-libobs-version.cmake`, `CMakeLists.txt`, `tests/cmake/`
**Depends on**: T7, T8, T3
**Reuses**: the `cmake -P` test pattern from T4
**Requirement**: FND-04

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] The failure message is exactly `Camera Sync builds against libobs 30.0.x; found <version>`.
- [ ] The version comes from `libobs_VERSION` if 30.0.2's package config sets it, and from `LIBOBS_API_MAJOR_VER` and `LIBOBS_API_MINOR_VER` in `obs-config.h` otherwise. The task records which one it used.
- [ ] CTest cases: `31.1.1` fails with the message naming `31.1.1`; `30.1.0` fails; `30.0.0` and `30.0.2` pass.
- [ ] Quick gate passes, then CI gate passes on all three platforms (the guard accepts 30.0.2 everywhere).

**Tests**: unit
**Gate**: quick, then ci

**Commit**: `:package: build: refuse libobs headers other than 30.0`

---

### Phase 4 tasks: Detector in the core

#### T10: Add quirc to the core

**What**: quirc as a submodule pinned at `542848d`, compiled into `camera-sync-core`, with a clear configure error when it is missing.
**Where**: `.gitmodules`, `deps/quirc`, `core/CMakeLists.txt`, `cmake/camera-sync/require-submodule.cmake`
**Depends on**: T3
**Reuses**: Sync Dock's `deps/quirc` layout
**Requirement**: FND-25, FND-30

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] `git submodule status deps/quirc` shows `542848dd6b9b0eaa9587bbf25b9bc67bd8a71fca`.
- [ ] quirc's `lib/*.c` compile into the core with quirc's own warnings kept out of the project's warnings-as-errors.
- [ ] A doctest case creates a decoder, sizes it to 640×360 and destroys it.
- [ ] A CTest case runs the submodule check against a missing path and passes only on the message `quirc submodule missing - run: git submodule update --init`.
- [ ] Quick gate passes. Test count: previous cases plus 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:package: build: add quirc 542848d to the core`

---

#### T11: Vendor Sync Dock's QR payload and video marker detection

**What**: The video half of Sync Dock's detector, built into the core with its detection algorithm unchanged. It is derived from `src/sync-test-output.hpp` (`st_qr_data`, the marker structs) and the video path of `src/sync-test-output.cpp`. libobs's frame struct, format helpers, `util_mul_div64` and `blog` are replaced by core types and a logger the caller injects.
**Where**: `core/detector/` (QR payload, video detector, core media types, logger), `tests/detector/`, `tests/fixtures/`
**Depends on**: T10
**Reuses**: Sync Dock `e4ad294`; `tool/videogen.py` for the fixture
**Requirement**: FND-26, FND-27, FND-28, FND-29

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] Each derived file starts with Sync Dock's notice verbatim (`Copyright (C) 2023 Norihiro Kamae` and the GPL-2.0-or-later text), followed by `Derived from Audio Video Sync Dock, <upstream path> at e4ad2941ed5f2982e9f17b76ebaf22096319f3cb`.
- [ ] T4's check covers `core/detector/`: each file there must hold the upstream notice and the provenance line, and a CTest fixture with the line removed fails naming the file.
- [ ] `tests/fixtures/generate.sh` regenerates the fixture with Sync Dock's `videogen.py` (`q=4,f=884,c=2`, 60 fps, 320×180), and the fixture is one QR frame's luma plane as a binary PGM.
- [ ] A doctest case feeds that frame to the vendored QR decoding and checks exactly the payload values it was generated with: `f=884`, `c=2`, the frame's `q_ms`, its index and `index_max=256`.
- [ ] The core-only build compiles the vendored files with no libobs header on the include path.
- [ ] Quick gate passes. Test count: previous cases plus at least 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): vendor sync dock's qr and video marker detection`

---

#### T12: Vendor Sync Dock's audio marker detection

**What**: The audio half of the detector, built into the core with its algorithm unchanged: quadrature demodulation, preamble detection with the peak finder, and the 8-bit index with CRC-4. It is derived from the audio path of `src/sync-test-output.cpp` and from `src/peak-finder.hpp`, with libobs's audio struct replaced by the core media types from T11.
**Where**: `core/detector/` (audio detector, peak finder), `tests/detector/`, `tests/fixtures/`
**Depends on**: T11
**Reuses**: T11's media types, logger and fixture script
**Requirement**: FND-26, FND-27, FND-28

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] Notices and provenance as in T11, enforced by the same check.
- [ ] `generate.sh` also writes a 2-second mono 48 kHz 16-bit PCM clip of the same pattern's sound, and the fixture records which indices it carries.
- [ ] A doctest case decodes the clip with `f=884`, `c=2` and gets exactly those indices, in order.
- [ ] A doctest case decodes 2 seconds of silence and gets no marker.
- [ ] Quick gate passes. Test count: previous cases plus at least 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): vendor sync dock's audio marker detection`

---

### Phase 5 tasks: Dock, docs and the load check

#### T13: Log the load line from the plugin

**What**: Link `camera-sync-core` into the plugin module, and log one load line from `obs_module_load` through the core's `load_line`.
**Where**: `src/plugin-main.c` (becomes `src/plugin-main.cpp`), `CMakeLists.txt`
**Depends on**: T3, T10
**Reuses**: `camera_sync::load_line` and `format_version` from T3; the template's `obs_log`
**Requirement**: FND-14, FND-21

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] The module links `camera-sync-core`.
- [ ] `obs_module_load` logs, once, `[camera-sync] load version=<PLUGIN_VERSION> obs_built=<format_version(LIBOBS_API_VER)> obs_running=<obs_get_version_string()> quirc=<7-character quirc commit>`. The quirc commit comes from a compile definition read from the submodule at configure time.
- [ ] The template's "plugin loaded successfully" line is gone.
- [ ] Quick gate passes, then CI gate passes.

**Tests**: none
**Gate**: quick, then ci

**Commit**: `:sparkles: feat: log the load line with the obs and quirc versions`

---

#### T14: Register an empty Camera Sync dock

**What**: Turn on the frontend API and Qt. Register a dock whose title comes from the locale file and which shows one label, "Camera Sync <version>".
**Where**: `src/camera-sync-dock.cpp` (with its header), `src/plugin-main.cpp`, `data/locale/en-US.ini`, `CMakePresets.json`
**Depends on**: T13
**Reuses**: Sync Dock's dock registration (`obs_frontend_add_dock_by_id`), without its OBS 29 compatibility shim
**Requirement**: FND-22, FND-23

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] `ENABLE_FRONTEND_API` and `ENABLE_QT` are on for every platform preset.
- [ ] The dock registers under id `camera-sync-dock` with the title `obs_module_text("CameraSync.Dock.Title")`, and `en-US.ini` holds `CameraSync.Dock.Title="Camera Sync"`.
- [ ] The dock shows one `QLabel` reading "Camera Sync 0.1.0" and no other widget.
- [ ] Every new file starts with the SPDX line, and the quick gate's notice check passes.
- [ ] CI gate passes on all three platforms.

**Tests**: none
**Gate**: quick, then ci

**Commit**: `:sparkles: feat: register an empty camera sync dock`

---

#### T15: Document the build, install and tests in CLAUDE.md

**What**: Replace `CLAUDE.md`'s "No code yet" state with the toolchain, the build, install and load steps per platform, and how to run all tests or one.
**Where**: `CLAUDE.md`
**Depends on**: T14
**Reuses**: the presets, workflows and commands from T3 to T14
**Requirement**: FND-31, FND-32, FND-33, FND-34

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] States OBS 30.0.2 headers with obs-deps and Qt `2023-11-03`, the compiler per platform (Visual Studio 2022, Xcode, the Ubuntu 24.04 GCC), CMake 3.28, and every preset name.
- [ ] Gives the build command per platform, and the plugin's install location in OBS on Windows, macOS and Ubuntu.
- [ ] Gives the quick gate's commands and `ctest --preset core -R <name>` for a single test.
- [ ] The Repository state section no longer says there is no code.
- [ ] The documented core-only commands run verbatim on WSL and pass.

**Tests**: none
**Gate**: quick

**Commit**: `:memo: docs: document the build, install and tests in claude.md`

---

#### T16: Check the plugin loads in OBS 30.0.0 and the latest stable

**What**: Install T14's CI packages on the OBS machine, walk FND-19 to FND-24, and record the evidence in this file.
**Where**: `.specs/features/foundation/tasks.md` (this task's evidence block)
**Depends on**: T14
**Reuses**: T14's CI packages
**Requirement**: FND-19, FND-20, FND-21, FND-22, FND-23, FND-24

**Tools**:

- MCP: NONE
- Skill: NONE

**Done when**:

- [ ] OBS 30.0.0: the Docks menu lists "Camera Sync", and the dock opens showing "Camera Sync 0.1.0".
- [ ] Latest stable (version recorded): the same.
- [ ] Each log has exactly one `[camera-sync] load` line, with `obs_built=30.0.2` and that OBS's version as `obs_running`.
- [ ] Each OBS's last "Number of memory leaks" count with the plugin equals its count without it.
- [ ] Evidence recorded here: platform, both OBS versions, both load lines and both leak counts.

**Tests**: uat
**Gate**: rig

**Commit**: `:white_check_mark: test: record the foundation load check`

---

## Phase Execution Map

```
Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5

Phase 1:  T1 → T2
Phase 2:  T3 → T4
          T3 → T5
          T3 → T6
Phase 3:  T7 → T9
          T8 → T9
Phase 4:  T10 → T11 → T12
Phase 5:  T13 → T14 → T15
          T14 → T16
```

Execution is strictly sequential, in task-number order. Dependencies on earlier phases (T3, T7, T8, T9, T10, T13) are listed in each task's `Depends on` and drawn only in the cross-check table.

---

## Task Granularity Check

`validate_tasks.py` warns on every ⚠️ row below and on each `Tests: none`. Each ⚠️ is one deliverable that cannot be tested in smaller pieces, and each `Tests: none` matches a matrix layer whose test type is build (ci gate) or none.

| Task | Scope | Status |
| ---- | ----- | ------ |
| T1: Scaffold from the template | One import plus its identity fields | ⚠️ Many files, one cohesive import |
| T2: Pin actions | One kind of edit across the workflow files | ✅ Granular |
| T3: Core-only build and first function | One build configuration, with the function that gives it a test | ⚠️ Several files, cohesive: no smaller piece is testable alone |
| T4: Notice check | One script and its tests | ✅ Granular |
| T5: CTest in CI | One workflow change and the build hook it needs | ⚠️ Two files, cohesive |
| T6: Format coverage | Two format scripts, one file list | ✅ Granular |
| T7: Windows and macOS on 30.0.2 | One dependency pin and the build step it needs | ⚠️ Three files, cohesive |
| T8: Ubuntu on 30.0.2 | One platform's dependency route and its package | ⚠️ Several files, cohesive |
| T9: Version guard | One function, its call and its tests | ⚠️ Two files, cohesive |
| T10: quirc | One submodule, its build and its missing-checkout check | ⚠️ Two files, cohesive |
| T11: Video half of the detector | One vendored path and its fixture test | ✅ Granular |
| T12: Audio half of the detector | One vendored path and its fixture test | ✅ Granular |
| T13: Load line | One log call and its link (the .c to .cpp rename counts twice) | ⚠️ Two files, cohesive |
| T14: Empty dock | One widget, its registration, its locale string and the presets that turn Qt on | ⚠️ Four files, cohesive: none is testable alone |
| T15: CLAUDE.md | One file | ✅ Granular |
| T16: Load check | One rig session | ✅ Granular |

---

## Diagram-Definition Cross-Check

| Task | Depends On (task body) | Diagram Shows | Status |
| ---- | ---------------------- | ------------- | ------ |
| T1 | None | Start of Phase 1 | ✅ Match |
| T2 | T1 | T1 → T2 | ✅ Match |
| T3 | T1 (Phase 1) | Start of Phase 2 | ✅ Match |
| T4 | T3 | T3 → T4 | ✅ Match |
| T5 | T3 | T3 → T5 | ✅ Match |
| T6 | T3 | T3 → T6 | ✅ Match |
| T7 | T1 (Phase 1) | Start of Phase 3 | ✅ Match |
| T8 | T1 (Phase 1) | Start of Phase 3 | ✅ Match |
| T9 | T7, T8, T3 (Phase 2) | T7 → T9, T8 → T9 | ✅ Match |
| T10 | T3 (Phase 2) | Start of Phase 4 | ✅ Match |
| T11 | T10 | T10 → T11 | ✅ Match |
| T12 | T11 | T11 → T12 | ✅ Match |
| T13 | T3 (Phase 2), T10 (Phase 4) | Start of Phase 5 | ✅ Match |
| T14 | T13 | T13 → T14 | ✅ Match |
| T15 | T14 | T14 → T15 | ✅ Match |
| T16 | T14 | T14 → T16 | ✅ Match |

---

## Test Co-location Validation

| Task | Code Layer Created/Modified | Matrix Requires | Task Says | Status |
| ---- | --------------------------- | --------------- | --------- | ------ |
| T1: Scaffold | Build spec, CI, plugin module | build | none (ci gate) | ✅ OK |
| T2: Pin actions | CI workflows | build | none (ci gate) | ✅ OK |
| T3: Core-only build | Core library | unit | unit | ✅ OK |
| T4: Notice check | CMake checks | unit | unit | ✅ OK |
| T5: CTest in CI | CI workflows, top-level CMake | build | unit (runs the suite in CI) | ✅ OK |
| T6: Format coverage | CI scripts | build | none (ci gate) | ✅ OK |
| T7: Windows and macOS on 30.0.2 | Build spec, presets | build | none (ci gate) | ✅ OK |
| T8: Ubuntu on 30.0.2 | Build spec, CI, packaging | build | none (ci gate) | ✅ OK |
| T9: Version guard | CMake checks | unit | unit | ✅ OK |
| T10: quirc | Core library | unit | unit | ✅ OK |
| T11: Video half | Vendored detector | unit | unit | ✅ OK |
| T12: Audio half | Vendored detector | unit | unit | ✅ OK |
| T13: Load line | Plugin module | build + UAT | none (ci gate; UAT in T16) | ✅ OK |
| T14: Empty dock | Plugin module and dock | build + UAT | none (ci gate; UAT in T16) | ✅ OK |
| T15: CLAUDE.md | Docs | none | none (quick gate) | ✅ OK |
| T16: Load check | Plugin module and dock | UAT | uat | ✅ OK |

T13 and T14 defer their UAT to T16 so the OBS machine is needed once. That is the matrix's own plan for the plugin layer, not deferred testing: both still carry the CI build gate, and the formatting T13 logs is unit-tested in T3.

---

## Requirement Coverage

| Requirement | Tasks |
| ----------- | ----- |
| FND-01, FND-02 | T1 (every later CI gate re-checks them) |
| FND-03 | T7, T8 |
| FND-04 | T9 |
| FND-05, FND-06 | T7 |
| FND-07 | T7, T8 |
| FND-08 | T7 (Windows, macOS), T8 (Ubuntu) |
| FND-09 | T2 |
| FND-10 | T8 |
| FND-11 | T6 |
| FND-12 | T1, T4 |
| FND-13, FND-15, FND-18 | T3 |
| FND-14 | T13 |
| FND-16, FND-17 | T5 |
| FND-19, FND-20, FND-24 | T16 |
| FND-21 | T3, T13, T16 |
| FND-22, FND-23 | T14, T16 |
| FND-25, FND-30 | T10 |
| FND-26, FND-27, FND-28 | T11, T12 |
| FND-29 | T11 |
| FND-31, FND-32, FND-33, FND-34 | T15 |

All 34 requirements map to at least one task.
