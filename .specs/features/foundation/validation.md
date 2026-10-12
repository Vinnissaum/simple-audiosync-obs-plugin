# Foundation Validation

## Validation: Foundation - PASS

**Verdict (iteration 2 of max 3)**: PASS for the in-scope ACs (T1 to T15). The three blocking items from iteration 1 are fixed and re-verified. FND-12's regression guard now kills M12, M13, M14 and M16. The core-only build passes with Ninja off `PATH`, also offline from a fresh clone (FND-15). The Ubuntu install command matches the CPack name (FND-32). Every in-scope AC has evidence, and no surviving mutant targets behavior an FND AC specifies. The T16-dependent ACs stay Deferred. **This report must be amended after T16** before Foundation is done.

**Date**: 2026-10-12
**Spec**: `.specs/features/foundation/spec.md` (FND-01 to FND-34)
**Diff range**: `a5ae6f6..4ce783e`. Iteration 1 covered `a5ae6f6..62f55d8`. Iteration 2 adds `987383a`, `c980de4` and `4ce783e`, which touch only `tests/cmake/check-notices-fixture.cmake`, `CMakePresets.json` and `CLAUDE.md`.
**Verifier**: independent sub-agent (author is not the verifier). I re-derived every row below from the repository, from a fresh clone, from the upstream sources and from CI metadata. The authors' reports and the ticks in `tasks.md` were not taken as evidence.

**Scope**: T1 to T15. T16 (the load check in OBS 30.0.0 and the latest stable OBS on the rig) has not run. FND-19, FND-20 and FND-24, and the runtime halves of FND-21, FND-22 and FND-23, are marked **Deferred to T16 (rig)** below. On their own they do not cause FAIL. **Re-run or amend this report after T16.**

---

## Iteration History

| Iteration | Commit | Verdict | Blocking gaps |
| --------- | ------ | ------- | ------------- |
| 1 | `62f55d8` | Failed | Gap 1: FND-12 notice fixture weak (M12, M13, M14, M16 survived). Gap 2: FND-15 core preset needed Ninja. Gap 3: FND-32 wrong Ubuntu .deb name |
| 2 | `4ce783e` | Passed | None. Known, non-blocking items are listed under "Known Non-Blocking Items" |

### Iteration 2 re-verification

- **Fix 1** (`987383a`, FND-12). The fixture now plants unmarked files and expects each one named:
  - `src/unmarked-c-source.c` and `core/unmarked-c-header.h` (`tests/cmake/check-notices-fixture.cmake:70-71`)
  - `tests/unmarked-test.cpp` (`:72`)
  - `cmake/camera-sync/unmarked-check.cmake` (`:73`)
  - an unmarked top-level `CMakeLists.txt` (`:68`), with its own match `if(NOT check_output MATCHES "(^|[ \n])CMakeLists\\.txt")` (`:108-109`)

  The fixed tree passes again (`:136-141`). In a scratch clone of `4ce783e`, the baseline `check_notices` cases pass (2 of 2). These mutants are now killed: M12, M13, M14, M16, M16a (drop `*.c` only), M16b (drop `*.h` only), and a new M34 (skip all of `tests/` instead of `tests/fixtures/`). I confirmed each failure has the intended cause: M14 reports "did not name CMakeLists.txt", M12 "did not name tests/unmarked-test.cpp", and M16a "did not name src/unmarked-c-source.c". As a regression check on the new fixture, M15, M17, M18 and M19 are still killed.
- **Fix 2** (`c980de4`, FND-15). The `core` preset no longer sets a generator (`CMakePresets.json:17-27`). From a fresh clone of `4ce783e`, with `PATH` limited to CMake 3.31.6's `bin`, `/usr/bin` and `/bin` (`command -v ninja` finds nothing), and under `unshare -rn` (`getent hosts github.com` fails), these all succeed: `cmake --preset core`, `cmake --build --preset core` and `ctest --preset core` (24 of 24 passed). `-R load_line` ran 6 of 6. The cache shows `CMAKE_GENERATOR=Unix Makefiles` and `CMAKE_MAKE_PROGRAM=/usr/bin/gmake`. A `build_core` already configured with Ninja keeps working, because CMake reuses the cached generator. The real tree's quick gate also passes (24 of 24; its `build_core` is now Unix Makefiles).
- **Fix 3** (`4ce783e`, FND-32). `CLAUDE.md:65` now reads `sudo apt install ./camera-sync-0.1.0-x86_64-linux-gnu.deb`. This matches `cmake/linux/defaults.cmake:17` (`${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}`, which is `x86_64-linux-gnu` with Ubuntu's GCC). The upload glob `camera-sync-0.1.0-x86_64*.*` (`.github/workflows/build-project.yaml:294`) is consistent with it. I re-checked `CLAUDE.md`'s other claims. Toolchain (`:30-34`) now says the core needs a compiler, CMake and `make`, which is true. The Windows layout matches `cmake/windows/helpers.cmake:25,78`. Nothing was installed.
- **CI**: run 38295639176 (push to main, `4ce783e`) finished with conclusion success. Every job succeeded: Build and Test Core, clang-format, gersemi (which checked the changed fixture), Build for Windows, macOS and Ubuntu with their `Run Unit Tests ✅` steps, and Ubuntu `Check Package Dependency 🔍`. Artifacts: `…-windows-x64-4ce783e7b` (399,984 B), `…-macos-universal-4ce783e7b` (14,795 B), `…-ubuntu-24.04-x86_64-4ce783e7b` (165,016 B). Logs and contents need authentication and were not read.
- **Remaining Ninja wording**: none contradicts the spec.
  - `.specs/features/foundation/tasks.md:17` lists the Execute prerequisites for the WSL machine. That describes a tool the author had installed, not a requirement of the core build.
  - `tasks.md:239` (T5) and `.github/workflows/build-project.yaml:91` still install `ninja-build` in the CI core job. It is now unused there but harmless: FND-17 requires only that the job download no OBS, obs-deps or Qt.
  - `CLAUDE.md:33` and `CMakePresets.json:94` tie Ninja only to the Ubuntu presets, which is accurate.
  - Stale wording to tidy later: `tasks.md:17` and `:239`.
- **Not verified, non-blocking**: on a multi-config default generator (Visual Studio on Windows, or Xcode if chosen), `ctest --preset core` would likely need a configuration (`-C`), because the test preset sets none. FND-15's own test environment is Ubuntu (build-essential, CMake, Git), where this does not arise. Untested here.
- **Isolation**: the sensor ran in a fresh scratch clone (`clone2`), with each mutation restored inside that clone. The real tree's `git status --porcelain` was the same before and after: only `.specs/LESSONS.md`, `.specs/lessons.json` and this report, all from iteration 1.

---

## Known Non-Blocking Items (carried from iteration 1)

1. **Audio detector test is weak** (T12, TDD; no FND AC). M25 (CRC rejection disabled) and M26 (10 of 12 bits decoded) survive, because the clip only carries indices 0 to 9 with valid CRCs. Fix 4 remains recommended before Reading reworks this code.
2. **Hash check bypassed on a re-configure** (FND-08 robustness, inherited from the template). CMake leaves a mismatching download on disk, and `cmake/common/buildspec_common.cmake:207` skips both the download and the hash check for an existing file. CI run 38263613975 went green with altered hashes. CI's single fresh configure still fails on a mismatch, so FND-08 holds as written. Fix 5 remains recommended.
3. **`format_version` mask widths** (M1 and M2; spec-precision gap). These mutants are equivalent within the 30.0.x domain that the FND-04 guard admits.
4. **Unreached vendored paths** (M22, M23, M29 and M31). These are QR range boundaries, the downscale step and the mirrored-QR retry. No FND AC covers them, and the test matrix defers detector accuracy to Reading.
5. **Minor items**:
   - The provenance lines of `video-detector.hpp` and `audio-detector.hpp` omit upstream `src/sync-test-output.hpp`.
   - `.gitmodules` matches the `/*` ignore rule; it works only because it is tracked.
   - FND-02 PR runs attach no .pkg or .deb unless labelled.
   - FND-11 format checks cover only changed files.
   - FND-07: CI also installs apt and brew packages without pinned hashes.
   - The pairing drop is recorded only in `tasks.md`.
   - Stale Ninja prerequisite wording in `tasks.md:17` and `:239`.
   - Multi-config `ctest --preset core` is untested.

---

## Iteration 1 Report (at `62f55d8`, kept for the record; rows below are updated where iteration 2 changed them)

---

## Task Completion

| Task | Status | Notes |
| ---- | ------ | ----- |
| T1 | ✅ Done | Scaffold commit `85b90c3` records template `3e7d7ac3…` in its body. `buildspec.json` identity fields match. The `.gitignore` allowlist covers every listed directory (checked with `git check-ignore --no-index`). Minor: `.gitmodules` matches the `/*` rule. It works only because it is already tracked. |
| T2 | ✅ Done | The pinning grep prints nothing. `git ls-remote` shows each SHA is the current `v4` tag: checkout `11d5960a` (v4.4.0), cache `0057852b` (v4.3.0), upload-artifact `ea165f8d` (v4.6.2), download-artifact `d3f86a10` (v4.3.0). gh-release `9d7c94cf` is v2.0.4. |
| T3 | ✅ Done (fixed in `c980de4`) | Quick gate and `-R load_line` (6 tests) pass. Iteration 1 found the `core` preset needed Ninja (Gap 2). Since iteration 2, the preset uses the default generator and passes with Ninja off `PATH` and offline. |
| T4 | ✅ Done (fixed in `987383a`) | Iteration 1: the fixture planted unmarked files only in `src/` and `core/` (`.cpp`/`.hpp`), so M12, M13, M14 and M16 survived (Gap 1). Iteration 2: they and M16a, M16b and M34 are killed. |
| T5 | ✅ Done | Every platform job runs ctest. The core-only job runs on its own. Scratch run 38259542768 (`e3db9bb`, a failing test) turned the core, Windows, Ubuntu and macOS test steps red. |
| T6 | ✅ Done | Scratch run 38259648575 (`fd3946d`, a misformatted `core/` file, with check-format enabled on that branch by `e3c3fd9`) failed the clang-format check. |
| T7 | ✅ Done | Hashes checked independently (see FND-05 and FND-07). The macOS preset targets 11.0. |
| T8 | ✅ Done | No PPA. CPack has SHLIBDEPS off and Depends `obs-studio (>= 30.0.0)`. The dpkg check step is green on both main runs. |
| T9 | ✅ Done | Guard and four CTest cases. Mutants M7, M8 and M9 were killed. |
| T10 | ✅ Done | Submodule at `542848dd…`. The missing-submodule message reproduces from a real `cmake --preset core` in a clone without the submodule. |
| T11 | ✅ Done | Notices match upstream byte for byte. I re-derived the payload values from upstream `tool/videogen.py`. |
| T12 | ⚠️ Done with weak test | The clip decodes to 0..9. The CRC rejection and index bits 6-11 are untested (M25 and M26 survive; Gap 4). |
| T13 | ✅ Done (static) | Plugin links the core and logs one load line. Runtime is deferred to T16. |
| T14 | ✅ Done (static) | Dock registration, label and locale are in place. Runtime is deferred to T16. |
| T15 | ✅ Done (fixed in `c980de4`, `4ce783e`) | The core commands run verbatim. The Ubuntu install filename now matches CPack (Gap 3 closed). |
| T16 | ⏳ Not done | Needs the rig. Out of scope for this report. |

---

## Gate Check

- **Gate command**: `cmake --preset core && cmake --build --preset core && ctest --preset core --output-on-failure`, with cmake 3.31.6, ninja 1.12.1 and gcc 13.3.0 on WSL.
- **Gate outcome**: 24 passed, 0 failed, 0 skipped. Exit 0.
- **Repository tree**: 24 of 24 passed. `ctest --preset core -R load_line` ran exactly the 6 `load_line` cases. `-R "check_notices_fixture"` ran 1 test. `-N` lists 24.
- **Fresh clone, no network** (`git clone` of the local repository, quirc fetched from the `.gitmodules` URL, then `unshare -rn`, where `getent hosts github.com` fails): configure, build and ctest all succeed, 24 of 24.
- **Test count before feature**: 0 (the repository had no code). **After**: 24. **Delta**: +24.
- **Skipped tests**: none. **Failures**: none.
- **CMake 3.28 exactly**: not exercised locally (3.31.6 here). The CI core job installs Ubuntu 24.04's `cmake` package, but the runner image also has a newer CMake on `PATH`, so CI does not prove 3.28 either.

---

## CI Evidence

The run, job and artifact metadata come from the public API (about 12 unauthenticated calls). Job logs and artifact contents need authentication, so I could not read them. Every CI conclusion below rests on step names, step outcomes, artifact names and sizes, and the workflow and script logic.

| Run | Commit | Result |
| --- | ------ | ------ |
| 38268565626 (push main) | `62f55d8` | All jobs succeeded: Check Formatting (clang-format, gersemi), Build and Test Core, Build for Windows, Build for macOS, Build for Ubuntu. Each platform job ran its `Run Unit Tests ✅` step successfully. Ubuntu `Check Package Dependency 🔍` succeeded. Artifacts: `camera-sync-0.1.0-windows-x64-62f55d88b` (399,876 B), `…-macos-universal-62f55d88b` (14,845 B), `…-ubuntu-24.04-x86_64-62f55d88b` (164,991 B), plus sources and dbgsym. |
| 38267906642 (push main) | `d3b1956` | Same job and step outcomes. Same five artifacts. |
| 38259542768 | `e3db9bb` (scratch, failing test) | The core job and the Windows, Ubuntu and macOS `Run Unit Tests` steps failed (FND-16 negative). |
| 38259648575 | `fd3946d` (scratch, misformatted `core/zz-misformatted.cpp`) | The clang-format check failed (FND-11 negative). |
| 38259924323 | `8f658ba` (scratch, altered prebuilt hashes) | The Windows and macOS `Build Plugin` steps failed; Ubuntu (not yet on 30.0.2) succeeded. |
| 38264041999 | `ff44a05` (scratch, altered Ubuntu obs-studio and macOS/Windows prebuilt hashes) | The `Build Plugin` step failed on all three platforms (FND-08 negative). The cause is inferred from the diff, which changes only hashes. |
| 38263613975 | `db38514` (scratch, the same altered hashes plus a diagnostic configure step before Build Plugin) | **All green.** See Gap 5: once the diagnostic configure failed, the second configure accepted the mismatched archive. |

What I could not confirm: whether the macOS artifact holds a `.pkg` and the Windows artifact a `.zip`. Both follow from the packaging scripts on a push (`package: true`, build-project.yaml:44). The Ubuntu `.deb` is confirmed indirectly, because the dependency step requires exactly one `release/*.deb` (build-project.yaml:272-276).

---

## Spec-Anchored Acceptance Criteria

### P1: Builds and packages in CI

| Criterion | Spec-defined outcome | Evidence (`file:line` + assertion or check) | Result |
| --------- | -------------------- | ------------------------------------------- | ------ |
| FND-01: push to main or PR to main builds Windows x64, macOS universal and Ubuntu 24.04 | Three platform builds on push and PR | `.github/workflows/push.yaml:5-7` (push on main), `.github/workflows/pr-pull.yaml:5-8` (pull_request on main), `.github/workflows/build-project.yaml:168` (`target: macos-universal`), `:217` (`ubuntu-24.04`), `:340` (`target: x64`, windows-2022). CI 38268565626 and 38267906642: three build jobs succeeded. No PR run exists yet, so the PR trigger is checked statically only. PRs that change only `**.md` skip CI (pr-pull.yaml `paths-ignore`, a template trigger that the spec's CI-triggers assumption accepts). | ✅ PASS |
| FND-02: a successful build uploads Windows .zip, macOS .pkg, Ubuntu .deb | Those three package types | Uploads: `build-project.yaml:194-198`, `:290-294`, `:353-357`. Artifacts listed above. Package types follow from `package-macos` (.pkg branch) and the Ubuntu one-.deb check. On `pull_request`, `package` is false (`build-project.yaml:36`), so macOS and Ubuntu upload `.tar.xz` instead | ✅ PASS for push. ⚠️ Spec-precision gap: FND-02's trigger ("a platform's CI build succeeds") also covers PR builds, which upload no .pkg or .deb unless the PR is labelled "Seeking Testers" |
| FND-03: compiles against OBS 30.0.2 libobs and frontend API on three platforms | 30.0.2 headers | `buildspec.json:4` `"version": "30.0.2"`. I downloaded both source archives and they match the pinned hashes: tar.gz `be12c3ad…5e83c27`, zip `970058c4…8141d9c7`. `CMakeLists.txt:39-42` runs the guard after `find_package(libobs)`, and CI is green on all three, so `libobs_VERSION` matched `^30\.0\.` | ✅ PASS |
| FND-04: non-30.0 headers fail naming found and expected | `Camera Sync builds against libobs 30.0.x; found <version>` | `cmake/camera-sync/check-libobs-version.cmake:7-8`. Test `tests/cmake/check-libobs-version-case.cmake:21-22` checks `string(FIND "${errors}" "Camera Sync builds against libobs 30.0.x; found ${VERSION}")` for 31.1.1 and 30.1.0, and `result EQUAL 0` for 30.0.0 and 30.0.2 (`tests/CMakeLists.txt:36`). M7, M8 and M9 were killed | ✅ PASS (no CI run against 31.x headers; the call site is checked statically) |
| FND-05: Windows and macOS use the 2023-11-03 obs-deps and Qt 6 that 30.0.2 pins | Same archives as OBS 30.0.2 | `buildspec.json:14,23`. I fetched OBS 30.0.2's own `buildspec.json`: prebuilt `macos-universal` `90c2fc06…`, `windows-x64` `d0825a6f…`; qt6 `ba4a7152…`, `bc57dedf…`; debugSymbols `fd8ecd1d…`. All equal the repository's values | ✅ PASS |
| FND-06: macOS deployment target 11.0 | `CMAKE_OSX_DEPLOYMENT_TARGET=11.0` | `CMakePresets.json:43` `"CMAKE_OSX_DEPLOYMENT_TARGET": "11.0"`. The value is passed on to the OBS build (`cmake/common/buildspec_common.cmake`, `_cmake_extra`) | ✅ PASS |
| FND-07: every dependency CI downloads is pinned with SHA-256 | Exact version and hash | `buildspec.json:2-33`: obs-studio (three hashes), prebuilt, qt6 and debugSymbols, each with a version and SHA-256. The obs-studio hashes were recomputed (FND-03) | ✅ PASS. ⚠️ Spec-precision gap: CI also downloads unpinned apt packages (Ubuntu build dependencies, Qt 6, cmake and ninja in the core job) and brew packages (zsh, clang-format@19, gersemi). The spec's Ubuntu assumption accepts distro packages, but FND-07's wording does not limit itself to the build spec |
| FND-08: a hash mismatch fails CI before compiling | Configure fails | `cmake/common/buildspec_common.cmake:209` `file(DOWNLOAD … EXPECTED_HASH SHA256=${hash})`, inside the configure step. CI negatives 38264041999 and 38259924323 | ✅ PASS for CI's single fresh configure. ❌ Robustness gap (Gap 5): a second configure in the same tree uses the mismatched file. Reproduced locally (CMake leaves the file on disk) and seen in CI run 38263613975 |
| FND-09: external actions pinned by full SHA | 40-hex SHA on every external `uses:` | `grep -rnE 'uses: [^.][^@]*@' .github \| grep -vE '@[0-9a-f]{40}( \|$)'` prints nothing. SHAs resolved to tags with `git ls-remote` | ✅ PASS |
| FND-10: .deb Depends is exactly `obs-studio (>= 30.0.0)` | That string, no upper bound | `cmake/linux/defaults.cmake:22-23`. CI step `build-project.yaml:278-282` compares `dpkg-deb -f … Depends` with `'obs-studio (>= 30.0.0)'` and succeeded on both main runs | ✅ PASS |
| FND-11: format checks on push and PR fail on unformatted files | Failing check | `.github/workflows/check-format.yaml:15,27` `failCondition: error`. Called from `push.yaml:16` (main only) and from `pr-pull.yaml`. `build-aux/.run-format.zsh` covers `core/` and `tests/`. Negative run 38259648575 | ✅ PASS. ⚠️ Spec-precision gap: the template checks only files changed by the push or PR (`check-changes`), not "any file" |
| FND-12: GPL-2.0 `LICENSE`; every C, C++ and CMake file the project writes starts with the SPDX line | First line `// SPDX-…` or `# SPDX-…` | `LICENSE:2` "Version 2, June 1991". I read the first line of all 24 project files in `src/`, `core/` (except `core/detector/`), `tests/`, `cmake/camera-sync/` and `CMakeLists.txt`: all carry the SPDX line. `check_notices_repository` passes | ⚠️ Satisfied today, but the regression test is weak: M12 (drop `tests/`), M13 (drop `cmake/camera-sync/`), M14 (drop the top-level `CMakeLists.txt`) and M16 (drop the `*.c`/`*.h` globs) survived in iteration 1 (Gap 1). Iteration 2 (`987383a`): `tests/cmake/check-notices-fixture.cmake:68-73,108-109` plants and expects unmarked `CMakeLists.txt`, `.c`, `.h`, `tests/` and `cmake/camera-sync/` files, and all four mutants are killed. ✅ PASS |

### P1: Unit tests run without OBS

| Criterion | Spec-defined outcome | Evidence | Result |
| --------- | -------------------- | -------- | ------ |
| FND-13: the core is static, with no libobs, frontend or Qt header or library | No such include or link | `core/CMakeLists.txt:26-32` (`add_library(camera-sync-core STATIC)`, links only `camera-sync-quirc`). The `#include` grep of `core/` finds only standard headers, core headers and `quirc.h`. The include flags in `build_core/build.ninja` are only `core/`, `deps/quirc/lib` and `third_party`. This machine has no `/usr/include/obs` and no Qt | ✅ PASS (build-level evidence; no dedicated test) |
| FND-14: the plugin links the core | Link | `CMakeLists.txt:44` `target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs camera-sync-core)`. CI green | ✅ PASS |
| FND-15: core-only configure, build and test with only a C/C++ compiler, CMake ≥ 3.28 and the checkout, offline | Works with no other tool and no network | The offline half holds: 24 of 24 under `unshare -rn` from a fresh clone. But `CMakePresets.json:22` sets `"generator": "Ninja"`. With Ninja off `PATH`, `cmake --preset core` fails: "CMake was unable to find a build program corresponding to "Ninja"". Ninja is not in the spec's list, nor in its Independent Test ("build-essential, CMake and Git") | ❌ GAP in iteration 1 (Gap 2). ✅ PASS in iteration 2: `c980de4` removed the generator (`CMakePresets.json:17-27`). From a fresh clone with no Ninja on `PATH`, under `unshare -rn`, configure, build and ctest all pass (24 of 24, Unix Makefiles with `/usr/bin/gmake`) |
| FND-16: each platform job runs ctest and fails on a failing test | Red job | `build-project.yaml:173-178` (macOS builds `camera-sync-tests`, then `ctest -C`), `:255-256` (Ubuntu), `:343-344` (Windows). Negative run 38259542768 | ✅ PASS |
| FND-17: a separate Ubuntu core-only job with no OBS, obs-deps or Qt download | Job exists and passes | `build-project.yaml:74-100`: no `needs`, installs only `cmake ninja-build`, runs the quick gate's three commands. Succeeded on both main runs | ✅ PASS |
| FND-18: `ctest -R <name>` runs only the matching doctest cases | Only matches run | `tests/CMakeLists.txt:18` `doctest_discover_tests(camera-sync-tests)`. `ctest --preset core -R load_line` ran 6 of 24, exactly the six `load_line …` cases | ✅ PASS |

### P1: Empty dock loads in OBS 30.0 and the latest stable

| Criterion | Spec-defined outcome | Static evidence | Result |
| --------- | -------------------- | --------------- | ------ |
| FND-19: OBS 30.0.0 Docks menu lists "Camera Sync" and the dock shows | Runtime | Registration at `src/camera-sync-dock.cpp:18` `obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("CameraSync.Dock.Title"), label)`, called from `src/plugin-main.cpp:35`. The CI packages build against 30.0.2 | ⏳ Deferred to T16 (rig) |
| FND-20: same in the latest stable OBS | Runtime | Same code | ⏳ Deferred to T16 (rig) |
| FND-21: exactly one line `[camera-sync] load version=… obs_built=… obs_running=… quirc=<7 chars>` | Exact line | Unit: `tests/core/load-line.cpp:32-33` `CHECK(load_line("0.1.0", pack(30, 0, 2), "32.0.1", "542848d") == "load version=0.1.0 obs_built=30.0.2 obs_running=32.0.1 quirc=542848d")`, plus per-field cases at `:36-57` and no prefix at `:62`. Module: `src/plugin-main.cpp:31-33` (one `obs_log` call with `PLUGIN_VERSION`, `LIBOBS_API_VER`, `obs_get_version_string()`, `CAMERA_SYNC_QUIRC_COMMIT`). Prefix: `src/plugin-support.c.in` `"[%s] %s"` with `PLUGIN_NAME` = `camera-sync`. quirc: `CMakeLists.txt:48-58` keeps the first 7 characters of `git rev-parse HEAD` in `deps/quirc`. M3, M4, M5 and M6 killed | ✅ Static half. ⏳ Runtime half deferred to T16 |
| FND-22: one line "Camera Sync <version>", no controls | One label | `src/camera-sync-dock.cpp:16` `new QLabel(QString("Camera Sync %1").arg(QString::fromUtf8(PLUGIN_VERSION)))` is the only widget | ✅ Static. ⏳ Runtime deferred to T16 |
| FND-23: title from en-US locale | Locale key | `src/camera-sync-dock.cpp:18` `obs_module_text("CameraSync.Dock.Title")`, `data/locale/en-US.ini:1` `CameraSync.Dock.Title="Camera Sync"`, `src/plugin-main.cpp:27` `OBS_MODULE_USE_DEFAULT_LOCALE` | ✅ Static. ⏳ Runtime deferred to T16 |
| FND-24: leak count equals the no-plugin count | Runtime | No static evidence possible. The label is handed to OBS, and `obs_module_unload` frees nothing (`src/plugin-main.cpp:39-42`) | ⏳ Deferred to T16 (rig) |

### P1: Detector and quirc in the core

| Criterion | Spec-defined outcome | Evidence | Result |
| --------- | -------------------- | -------- | ------ |
| FND-25: quirc submodule pinned at `542848d` | Gitlink `542848d…` | `.gitmodules` (`deps/quirc`). `git submodule status` shows `542848dd6b9b0eaa9587bbf25b9bc67bd8a71fca deps/quirc (v1.2)` | ✅ PASS |
| FND-26: the core-only build compiles the detector and quirc with no libobs header on the include path | Compiles without libobs | `core/CMakeLists.txt:9-32` (quirc `lib/*.c`, `detector/*.cpp`). The include flags (FND-13 row) contain no libobs path, and the build succeeds on a machine with no libobs | ✅ PASS |
| FND-27: Sync Dock's notice verbatim at the top of every vendored file | Byte-exact notice | `cmp` shows `cmake/camera-sync/sync-dock-notice.txt` equals lines 1-18 of upstream `src/sync-test-output.cpp` at `e4ad294`. Lines 1-18 of all six `core/detector/*` files equal that file. Enforced at `cmake/camera-sync/check-notices.cmake:51-66` and tested at `tests/cmake/check-notices-fixture.cmake:78-83` (line numbers at `4ce783e`) (missing, altered, other-commit). M17, M18 and M19 killed. Upstream `sync-test-output.hpp` and `peak-finder.hpp` carry no notice, so their vendored copies carry the project notice from `sync-test-output.cpp`. That is Sync Dock's notice, so the AC holds | ✅ PASS |
| FND-28: each vendored file states it derives from Sync Dock, with upstream path and SHA | Provenance line | Line 19 of each file, for example `core/detector/peak-finder.hpp:19` `// Derived from Audio Video Sync Dock, src/peak-finder.hpp at e4ad2941…`. The check regex is at `check-notices.cmake:59` | ✅ PASS. Minor: `video-detector.hpp` and `audio-detector.hpp` also take structs from upstream `src/sync-test-output.hpp` (`video_marker_found_s`, `audio_marker_found_s`) but name only `src/sync-test-output.cpp` |
| FND-29: QR decoding of one pattern frame returns exactly its generated payload | `f=884, c=2, q=66, i=37, t=0, I=256` | `tests/detector/video-detector.cpp:31-37` `CHECK(payload.valid); CHECK(payload.f == 884); CHECK(payload.c == 2); CHECK(payload.q_ms == 66); CHECK(payload.index == 37); CHECK(payload.index_max == 256); CHECK(payload.type_flags == 0)`. Expected values re-derived from upstream `tool/videogen.py:196-197`: `q_ms = 4*1000*1//60 = 66`, payload `q=…,i=…,f=…,c=…,t=…,I=…`. `tests/fixtures/generate.py:17,46` sets `i = 37`. `Pattern.index_max = 256`, `type_flags = 0`. M20, M21, M28 and M30 killed. I did not regenerate the fixture, because that would execute untrusted upstream code | ✅ PASS |
| FND-30: missing submodule fails configure with the exact message | `quirc submodule missing - run: git submodule update --init` | `cmake/camera-sync/require-submodule.cmake:8`. Test `tests/cmake/require-submodule-case.cmake:21-22` (`string(FIND … "quirc submodule missing - run: git submodule update --init")`). Reproduced end to end: a clone without the submodule fails `cmake --preset core` with that message at `core/CMakeLists.txt:6`. M10 and M11 killed | ✅ PASS |

### P2: Build documented in CLAUDE.md

| Criterion | Spec-defined outcome | Evidence | Result |
| --------- | -------------------- | -------- | ------ |
| FND-31: toolchain, OBS version, compilers, CMake minimum, every preset | Stated | `CLAUDE.md:30` (OBS 30.0.2), `:32` (VS 2022, Xcode 16+, Ubuntu GCC 13), `:33` (CMake 3.28), `:34` (all seven non-hidden presets; `template` is hidden) | ✅ PASS |
| FND-32: build, install and load commands per platform | Working commands | `CLAUDE.md:45-56` (build) and `:59-65` (install). The Windows layout matches `cmake/windows/helpers.cmake:25,78`. **Defect**: `CLAUDE.md:65` `sudo apt install ./camera-sync-0.1.0-ubuntu-24.04-x86_64.deb`, but CPack names the file `camera-sync-0.1.0-x86_64-linux-gnu.deb` (`cmake/linux/defaults.cmake:17`, `${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}`). The `ubuntu-24.04` name is the artifact zip's name. This conclusion comes from the CPack configuration; I did not install anything | ❌ GAP in iteration 1 (Gap 3). ✅ PASS in iteration 2: `CLAUDE.md:65` now reads `camera-sync-0.1.0-x86_64-linux-gnu.deb`, which matches `cmake/linux/defaults.cmake:17` |
| FND-33: commands for every test and a single test | Commands run | `CLAUDE.md:48,74,80-81,84`. All of them ran verbatim here and in the offline fresh clone | ✅ PASS |
| FND-34: Repository state no longer says no code | Text | `CLAUDE.md:19` "Foundation, the TDD's phase 0, has landed" | ✅ PASS |

**Status**: ✅ All in-scope ACs covered after iteration 2. Iteration 1 had gaps on FND-12 (regression guard), FND-15 and FND-32. ⚠️ Spec-precision gaps remain flagged (FND-02, FND-07, FND-11, format_version domain, audio decoding has no AC). Six rows are deferred to T16.

---

## Detector Faithfulness (FND-26 to FND-29 and the "algorithm unchanged" assumption)

I fetched upstream Sync Dock at `e4ad2941ed5f2982e9f17b76ebaf22096319f3cb` into a fresh scratch directory and read it as data, without executing it.

**Notices and provenance**: byte-exact (see FND-27 and FND-28).

**Algorithm read-through**: I compared every vendored function with its upstream counterpart: `PeakFinder` (normalised diff), `find_marker` (normalised diff), `decode_qrcode`, `start`, `adjust_corners`, `qrcode_length`, `sqrt_u32`, `AudioBuffer`, `crc4_check`, `decode_data`, `test_preamble`, `process`, and `QrPayload` parse and check. Only the following changed, and each is equivalent:

- Types: `video_data` and `audio_data` became `VideoFrame` and `AudioBlock`.
- Logging: `blog` became the injected `Logger`, with the same messages and levels.
- Helpers: `util_mul_div64` became `mul_div64`, the same as libobs's non-MSVC path. libobs's MSVC 128-bit path agrees whenever `rem*mul` does not overflow, which holds here.
- Maths: `hypotf` became `std::hypot(float, float)`, and `M_PI`/`M_SQRT1_2` became local constants.
- Casts: explicit casts in `adjust_corners`, `mul_div64` and `peak-finder` produce the same arithmetic. The cast comes before the unsigned `/24`, and float division by a float-converted `uint64_t` is unchanged.
- Formats: libobs format enums became `LumaFormat`, with the same pixel size and offset per group.
- Stereo: upstream's `audio_channels >= 2` became `data[1] != nullptr`.

Removed: the output wrapper, signals (`qrcode_found`, `*_marker_found`, `sync_found`) and the commented debug prints.

**Disclosed deviations, judged against the spec and ADR-005**:

1. *Timestamps as given, not relative to the first video frame.* Upstream subtracts one `start_ts` from both video and audio markers, so every video-to-audio difference is unchanged. With one detector per camera there is no single first frame, and TDD "Reading arithmetic" needs camera and mic times on one clock. This is reporting, not detection. It contradicts neither the spec nor ADR-005, which asks for timestamps compatible with ADR-003. *Undisclosed side effect*: upstream ignored audio until the first video frame (`if (!st->start_ts) return;`). The vendored audio half waits only for `set_pattern`. Upstream's `f` and `c` also came only from a decoded QR code, which needs a video frame, so in practice the two are equivalent.
2. *`sync_indices` pairing and `identify_audio_index_max` not vendored* (`AudioMarker` has no `index_max`). Pairing is not detection. The TDD's rework table replaces it in Reading ("one video marker list per camera against one shared audio marker list"), and ADR-005 expects that rework. The spec's assumption lists only "the OBS output wrapper, its signals and the debug monitor source" as dropped, so this is a small, undocumented scope deviation. It is disclosed in `tasks.md` notes only, not in the spec or a `SPEC_DEVIATION` marker. It contradicts no AC.
3. *`set_pattern` hand-off*. Upstream copied `f`, `c` and `q_ms` from each decoded QR code inside the same object, guarded by `f>0 && c>0`. `QrPayload::check` already guarantees that guard. The caller now does the copy. Nothing in Foundation wires it, which matches the TDD ("It starts when the first camera decodes `f` and `c`") and ADR-005 ("one audio decoder for the mic, fed QR parameters by whichever camera decodes them first"). No contradiction.

Splitting the detector into video and audio halves goes slightly beyond the spec assumption ("The per-camera split waits for Reading"), but T11 and T12 planned it and the detection inside each half is unchanged.

---

## Discrimination Sensor

**Isolation**: all mutations ran in a fresh `git clone` of `62f55d8` in the scratchpad (with quirc from its `.gitmodules` URL), never in the real tree. After each mutation the file was restored with `git checkout --` *inside the clone*. `git stash` was never used. The clone was deleted afterwards. The real tree's `git status --porcelain` was empty before the sensor run, and empty again after cleanup ("REAL-TREE-MATCHES-BASELINE"). `validation.md` is the only file this report adds.

**Sensor depth**: expanded. 33 behavior-level mutations across the core, the CMake checks and the vendored detector.

| # | File | Mutation | Tests run | Outcome |
| - | ---- | -------- | --------- | ------- |
| M1 | `core/load-line.cpp:10` | patch mask `0xffff` → `0xff` | format_version, load_line | ❌ Survived (equivalent inside the specified domain, see below) |
| M2 | `core/load-line.cpp:9` | minor mask `0xff` → `0x0f` | format_version, load_line | ❌ Survived (equivalent inside the specified domain; the "two-digit minor" case uses 12, which fits in 4 bits) |
| M3 | `core/load-line.cpp:11` | swap minor and patch in the output | format_version, load_line | ✅ Killed (6 cases) |
| M4 | `core/load-line.cpp:16` | `load version=` → `loaded version=` | load_line | ✅ Killed |
| M5 | `core/load-line.cpp:19-22` | `quirc=` field before `obs_running=` | load_line | ✅ Killed |
| M6 | `core/load-line.cpp:17` | `version` → `version.substr(0, 3)` | load_line | ✅ Killed |
| M7 | `cmake/camera-sync/check-libobs-version.cmake:7` | accept `^30\.` (so 30.1.x passes) | check_libobs | ✅ Killed (30.1.0_fail) |
| M8 | same | accept only `^30\.0\.[01]$` (rejects 30.0.2) | check_libobs | ✅ Killed (30.0.2_pass) |
| M9 | `check-libobs-version.cmake:8` | message `found ${version}` → `found libobs` | check_libobs | ✅ Killed |
| M10 | `cmake/camera-sync/require-submodule.cmake:8` | message drops ` --init` | require_submodule | ✅ Killed |
| M11 | `require-submodule.cmake:7` | never fail (`… AND FALSE`) | require_submodule | ✅ Killed |
| M12 | `cmake/camera-sync/check-notices.cmake:18` | stop checking `tests/` | check_notices | ❌ **Survived** |
| M13 | `check-notices.cmake:18` | stop checking `cmake/camera-sync/` | check_notices | ❌ **Survived** |
| M14 | `check-notices.cmake:28` | stop checking the top-level `CMakeLists.txt` | check_notices | ❌ **Survived** |
| M15 | `check-notices.cmake:18` | stop checking `src/` | check_notices | ✅ Killed |
| M16 | `check-notices.cmake:30` | drop the `*.c` and `*.h` globs | check_notices | ❌ **Survived** |
| M17 | `check-notices.cmake:59` | provenance accepts any commit | check_notices | ✅ Killed |
| M18 | `check-notices.cmake:22` | derived-file rule never applies | check_notices | ✅ Killed (repository case) |
| M19 | `check-notices.cmake:60` | notice bytes not compared | check_notices | ✅ Killed |
| M20 | `core/detector/qr-payload.hpp:65` | `i` decoded as value+1 | qr payload, video detector | ✅ Killed |
| M21 | `qr-payload.hpp:68` | `I` decoded as value−1 | qr payload, video detector | ✅ Killed |
| M22 | `qr-payload.hpp:87` | `f < c` → `f <= c` (rejects c == f) | qr payload, video detector | ❌ Survived (boundary untested; not an FND AC) |
| M23 | `qr-payload.hpp:83` | `32000 < f` → `32000 <= f` | qr payload, video detector | ❌ Survived (boundary untested; not an FND AC) |
| M24 | `core/detector/audio-detector.cpp:111` | `index >> 4` → `(index >> 4) + 1` | audio detector | ✅ Killed |
| M25 | `core/detector/audio-detector.cpp:97` | CRC mismatch never rejects | audio detector | ❌ Survived (CRC rejection untested) |
| M26 | `core/detector/audio-detector.cpp:83` | decode 10 bits instead of 12 (`i < 10`) | audio detector | ❌ Survived (indices 0-9 never set bits 10-11) |
| M27 | `core/detector/audio-detector.cpp:48` | polynomial `0x13` → `0x11` | audio detector | ✅ Killed |
| M28 | `core/detector/video-detector.cpp:155` | downscale threshold 640×480 → 64×48 | video detector, qr payload, quirc | ✅ Killed |
| M29 | `video-detector.cpp:158` | `qr_step_ *= 2` → `*= 3` | video detector, qr payload, quirc | ❌ Survived (the 320×180 fixture never downscales) |
| M30 | `video-detector.cpp:184` | first sampled row offset +40 | video detector | ✅ Killed |
| M31 | `video-detector.cpp:217` | no `quirc_flip` retry | video detector | ❌ Survived (fixture is not mirrored) |
| M32 | `core/detector/audio-detector.cpp:172-173` | swap sin and cos oscillators | audio detector | ✅ Killed |
| M33 | `core/detector/audio-detector.cpp:155` | never process | audio detector | ✅ Killed |

Iteration 1 sensor outcome: 33 injected, 21 killed, 12 survived. Four survivors (M12, M13, M14, M16) targeted specified behavior.

**Iteration 2 re-run** (scratch clone of `4ce783e`; only the notice-check fixture changed, so the check's mutants were re-run, and the detector and core mutants still stand because neither they nor their tests changed):

| # | Mutation | Outcome |
| - | -------- | ------- |
| M12 | stop checking `tests/` | ✅ Killed ("did not name tests/unmarked-test.cpp") |
| M13 | stop checking `cmake/camera-sync/` | ✅ Killed |
| M14 | stop checking the top-level `CMakeLists.txt` | ✅ Killed ("did not name CMakeLists.txt") |
| M16 | drop the `*.c` and `*.h` globs | ✅ Killed |
| M16a | drop `*.c` only | ✅ Killed ("did not name src/unmarked-c-source.c") |
| M16b | drop `*.h` only | ✅ Killed |
| M34 | skip all of `tests/` (`skipped_prefixes tests/`) | ✅ Killed |
| M15, M17, M18, M19 | as in iteration 1 | ✅ Killed (fixture regression check) |

Iteration 2 sensor outcome: 36 distinct mutations, 28 killed, 8 survived (M1, M2, M22, M23, M25, M26, M29, M31). None of the 8 targets behavior an FND AC specifies (see "How I judged the survivors" and Known Non-Blocking Items).

**How I judged the survivors**:

- **Specified behavior, weak test (these drove the iteration 1 FAIL; killed in iteration 2)**: M12, M13, M14 and M16. FND-12 covers "every C, C++ and CMake file the project writes". T4's Done-when names `tests/`, `cmake/camera-sync/` and the top-level `CMakeLists.txt` as checked, and T4's What names C files. The fixture (`tests/cmake/check-notices-fixture.cmake:46-77`) plants unmarked files only as `src/*.cpp` and `core/*.hpp`, so the check could stop covering three of its five locations, plus `.c` and `.h` files (the repository has `src/plugin-support.h`), and no test would notice.
- **Named in T12 and the TDD, but in no FND AC**: M25 and M26. The audio fixture (indices 0 to 9) exercises only index bits 4 to 7, and every block carries a valid CRC. A decoder that breaks indices 64 to 255, or accepts corrupt CRCs, passes. Reading will rework exactly this code, so this is the most useful test to strengthen even though no Foundation AC asks for it.
- **Equivalent inside the specified domain (spec-precision gap)**: M1 and M2. The guard (FND-04) admits only 30.0.x, so the packed value that `obs_built` formats always has minor 0 and a small patch. Neither the spec nor T3 defines `format_version` outside that domain. The test comment's contract ("major << 24 | minor << 16 | patch") would justify a wider case, for example minor 0x1F and patch 0x1FF, but nothing requires one.
- **Unreached vendored paths, not specified by any Foundation AC**: M22, M23, M29 and M31. The test matrix defers detector accuracy to Reading. I compared these paths with upstream by reading them (see Detector Faithfulness).

---

## Code Quality

| Principle | Status |
| --------- | ------ |
| Minimum code | ✅ The core holds only `load_line` and the vendored detector. No speculative abstractions |
| Surgical changes | ✅ The template edits are confined to the OBS 30.0.2 build step, Ubuntu packaging, format lists and pins |
| No scope creep | ⚠️ Pairing was dropped and the detector split into halves ahead of Reading. This was planned in T11 and T12, but not recorded against the spec assumption |
| Matches patterns | ✅ The CMake checks share one `cmake -P` case pattern. Tests follow doctest naming |
| Spec-anchored outcome check (asserted values match the spec) | ✅ Exact strings and values for FND-04, FND-21, FND-29 and FND-30 |
| Per-layer Coverage Expectation met | ✅ After iteration 2. In iteration 1 the CMake-checks layer ("accept and reject paths") does not reject in `tests/`, `cmake/camera-sync/`, the top-level `CMakeLists.txt` or `.c`/`.h` files |
| Every test maps to a spec requirement (no unclaimed tests) | ✅ load_line → FND-21 and T3. quirc → T10. video and qr payload → FND-29 and T11. audio → T12 Done-when. check_* → FND-12, FND-04 and FND-30 |
| Documented guidelines followed: `CLAUDE.md`, `docs/tdd/multi-camera-sync.md` (Testing Strategy), `.specs/features/foundation/tasks.md` (Test Coverage Matrix) | ⚠️ Except for the gaps above |

---

## Edge Cases

- [x] A downloaded dependency's hash does not match, so CI fails before compiling (FND-08). True for CI's fresh configure, but see Gap 5.
- [x] Headers other than 30.0 fail naming both versions (FND-04).
- [x] A missing quirc submodule gives the FND-30 message. Reproduced end to end.
- [x] A failing unit test fails that platform's job (FND-16). Run 38259542768.

---

## Fix Plans

### Fix 1 (Major): Make the notice-check fixture cover every checked location. Done in `987383a`, verified in iteration 2

- **Root cause**: `tests/cmake/check-notices-fixture.cmake:67-69` plants unmarked files only in `src/` and `core/`.
- **Fix task**: Add unmarked fixture files `tests/unmarked-test.cpp`, `cmake/camera-sync/unmarked-check.cmake`, `src/unmarked-c.c` and `core/unmarked-header.h`. Assert the check names each one. Add a run with an unmarked top-level `CMakeLists.txt` and assert it is named. Keep the existing positive pass.
- **Verify**: Re-run M12, M13, M14 and M16. All must be killed.

### Fix 2 (Major): Reconcile FND-15 with the Ninja-only core preset. Done in `c980de4` (generator dropped), verified in iteration 2

- **Root cause**: `CMakePresets.json:22` fixes the core generator to Ninja. FND-15 allows only a compiler and CMake.
- **Fix task**: Either drop `"generator"` from the `core` preset, so CMake's default generator is used (`make` comes with build-essential, as in the spec's Independent Test), or record a spec change that adds Ninja to FND-15 and its Independent Test. Update `CLAUDE.md:33,45` to match.
- **Verify**: `cmake --preset core && cmake --build --preset core && ctest --preset core` passes with only gcc, g++, make and CMake on `PATH`. Or, if the spec changes instead, the amended FND-15 text names Ninja.

### Fix 3 (Minor): Correct the Ubuntu install command in CLAUDE.md. Done in `4ce783e`, verified in iteration 2

- **Root cause**: `CLAUDE.md:65` uses the artifact's name, not the .deb's.
- **Fix task**: Say that the artifact zip holds `camera-sync-0.1.0-x86_64-linux-gnu.deb`, and use `sudo apt install ./camera-sync-0.1.0-x86_64-linux-gnu.deb`.
- **Verify**: Download a CI Ubuntu artifact and list its contents.

### Fix 4 (Minor, recommended before Reading): Strengthen the audio detector test. Open, non-blocking

- **Root cause**: The audio fixture carries indices 0 to 9 only, all with valid CRCs.
- **Fix task**: Extend `generate.py` to set a starting index so the clip spans the high bits, for example 60 to 69 or 250 to 255 plus a wrap to 0 to 3. Add a case that flips one symbol, or decodes with a wrong carrier phase, and expects the marker to be rejected.
- **Verify**: M25 and M26 are killed.

### Fix 5 (Minor, inherited from the template): Re-check the hash of an archive already on disk. Open, non-blocking

- **Root cause**: `cmake/common/buildspec_common.cmake:207` skips both the download and the hash check when the file exists. CMake leaves a mismatching download on disk. The second configure then extracts the file and records the pinned hash as verified. CI run 38263613975 went green with altered hashes.
- **Fix task**: Before using an existing file, compare `file(SHA256 …)` with the pinned hash, and delete and re-download (or fail) on mismatch.
- **Verify**: Configure twice with an altered hash; both attempts fail.

---

## Requirement Traceability Update

| Requirement | Previous Status | New Status |
| ----------- | --------------- | ---------- |
| FND-01 to FND-11 | Done | ✅ Verified (spec-precision notes on FND-02, FND-07, FND-11; Gap 5 on FND-08 robustness) |
| FND-12 | Done | ✅ Verified (iteration 2, Fix 1 in `987383a`) |
| FND-13, FND-14 | Done | ✅ Verified |
| FND-15 | Done | ✅ Verified (iteration 2, Fix 2 in `c980de4`) |
| FND-16 to FND-18 | Done | ✅ Verified |
| FND-19, FND-20, FND-24 | In Tasks | ⏳ Deferred to T16 (rig) |
| FND-21, FND-22, FND-23 | Partial | ✅ Static half verified; ⏳ runtime deferred to T16 |
| FND-25 to FND-30 | Done | ✅ Verified |
| FND-31, FND-33, FND-34 | Done | ✅ Verified |
| FND-32 | Done | ✅ Verified (iteration 2, Fix 3 in `4ce783e`) |

---

## Summary

**Overall**: ✅ Ready for T16 (iteration 2). Iteration 1 was not ready.

**Spec-anchored check**: 28 of 28 in-scope ACs match the spec outcome after iteration 2. In iteration 1, FND-12 (regression guard), FND-15 and FND-32 had gaps. Five spec-precision gaps are flagged. Six ACs are deferred to T16, three of them in full and three in their runtime half.
**Sensor**: Iteration 2: 36 mutations, 28 killed, 8 survived, none of them against specified behavior. Iteration 1: 33 mutations, 21 killed, 12 survived.
**Gate**: 24 passed, 0 failed (real tree at `4ce783e`). Also 24 of 24 from a fresh clone, offline and without Ninja.

**What works**: CI is green on three platforms against verified OBS 30.0.2 sources and deps, and every action is pinned to a tag SHA. The .deb Depends string is checked in CI, as are the libobs 30.0.x guard and the quirc pin and its missing-checkout message. The OBS-free core builds and tests offline. The load line is tested with exact strings. The vendored detector has byte-exact notices, and I confirmed its algorithm matches upstream by reading every function. The QR payload test asserts the exact generated values.

**Next steps**: Run T16 on the rig and amend this report to settle FND-19 to FND-24. Fixes 4 and 5 are recommended but non-blocking. Fix 4 should land before Reading reworks the audio detector.
