# Foundation Specification

> Phase 0 of the [technical design](../../../docs/tdd/multi-camera-sync.md#implementation-plan): the scaffold the spikes run inside and every later slice builds on. The [design](../../../.design/multi-camera-sync.md) and the [ADRs](../../../docs/adr/README.md) win over this spec where they disagree. Size: Medium.

## Problem Statement

The repository holds a design, eight ADRs and a technical design, and no code. Spike 1 needs a plugin that loads in OBS and runs Sync Dock's detector once per camera. Reading and Alignment need a test gate that runs without OBS, because the development machine has none. Every user's OBS from 30.0 up has to load whatever the rig tests. None of that exists yet.

## Goals

- [ ] Every push to main and every pull request builds and packages the plugin for Windows, macOS and Ubuntu against OBS 30.0 headers, and CI fails on any build, test or format error.
- [ ] Unit tests for an OBS-free core run in every CI build job, and on a machine that has only a compiler, CMake and Git.
- [ ] The built plugin loads in OBS 30.0.0 and in the latest stable OBS and shows an empty "Camera Sync" dock.
- [ ] Sync Dock's detector and quirc build inside the core without libobs, with their notices kept, ready for spike 1.

## Out of Scope

| Feature | Reason |
| ------- | ------ |
| Taps, per-camera detector rework, estimator, session, dock contents | Reading phase, specified after the spikes decide ADR-005 and ADR-007 |
| Spike 1 and spike 2 | Rig experiments outside tlc-spec-driven (TDD, Running the plan) |
| Calculator, settings port, Apply, Undo, the headless libobs integration job | Alignment phase |
| Windows installer, pattern files, release publishing, SHA-256 sums, forum page | Release phase |
| Refusing to load on OBS older than 30.0 (REL-05) | Release phase |
| Fuzz target for the QR payload parser | Arrives with the detector rework in Reading |
| Core line-coverage gate (90%) | The core holds no logic of its own yet; the gate starts with Reading |
| Flatpak, macOS signing and notarization | Release defaults 1 and 2 |
| A .deb for Ubuntu releases other than 24.04 | TDD Open question 13, decided in Release |
| The final product name | TDD Open question 14, decided in Release |

---

## Assumptions & Open Questions

Facts below were checked on 2026-10-12 against the plugin template at `3e7d7ac` (master, 2025-12-09), Sync Dock at `e4ad294` (main, 2026-02-04), quirc at `927d680` (master) and OBS Studio's 30.0.x tags.

| Assumption / decision | Chosen default | Rationale | Confirmed? |
| --------------------- | -------------- | --------- | ---------- |
| What "the first slice" means | Foundation, the TDD's phase 0, not the design's Reading slice | `CLAUDE.md` starts work here, and the TDD holds Reading's spec until the spikes decide ADR-005 and ADR-007 | y |
| Plugin identity | Module `camera-sync`, display name "Camera Sync", version 0.1.0 | The TDD's working name and its `[camera-sync]` log prefix. Renaming the module later moves only its config directory; nothing in a scene collection depends on the module name | y |
| Where the load checks run | The first OBS machine available, which needs no cameras. It runs OBS 30.0.0 and the latest stable side by side (portable installs on Windows). Platforms that machine does not run rely on CI until Release's clean installs | The rig is not set up; loading the plugin needs only OBS | y |
| Which OBS headers | OBS Studio 30.0.2, the last 30.0 patch (tags 30.0.0 to 30.0.2 exist). The load check runs on 30.0.0, the floor | Patch releases add no API. Loading on 30.0.0 catches any symbol that arrived in a patch | y |
| Ubuntu build route | Build libobs and the frontend API from the pinned OBS 30.0.2 sources on the template's ubuntu-24.04 runner, against Ubuntu 24.04's packages. If that costs more than a day, fall back to Sync Dock's route: an ubuntu-22.04 runner with a prebuilt OBS 30 development environment | The template installs the PPA's latest `obs-studio` on Ubuntu (`.github/scripts/utils.zsh/setup_ubuntu`), which breaks finding 8. Sync Dock builds its OBS 30 package on 22.04. OBS publishes no obs-deps for Ubuntu | y |
| Template versus OBS 30.0 sources | Adapt the template's OBS build step for 30.0.2 | The template configures OBS with `ENABLE_FRONTEND=OFF`, the 31-era option; 30.0.2 names it `ENABLE_UI` and keeps the frontend API under `UI/`. Its buildspec keys macOS as `macos`, where OBS 30.0.2 uses `macos-universal` | y |
| Template revision | Scaffold from the template at `3e7d7ac`, recorded in the scaffold commit's body | The commit checked above | y |
| "Latest stable OBS" | The newest stable release on obsproject.com on the day of the check (32.x when the TDD was written) | The target moves; the check names the version it used | y |
| Test framework | doctest, kept in the repository as a single header | The TDD's choice; the core-only build must work offline | y |
| How much of the detector Foundation vendors | `src/sync-test-output.cpp`, `src/sync-test-output.hpp` and `src/peak-finder.hpp` from Sync Dock at `e4ad294`, detection algorithm unchanged. Dropped: the OBS output wrapper, its signals and the debug monitor source. libobs's frame and audio structs, format helpers, `util_mul_div64` and `blog` are replaced by core types and an injected logger. The per-camera split waits for Reading | The TDD's detector-rework table. Those libobs uses are what the detector calls today | y |
| quirc revision | The commit Sync Dock pins, `542848d` | The detector's field record was built on it | y |
| quirc's version in the load line | The pinned commit's 7-character SHA | `quirc_version()` returns "1.0" even at quirc 1.2, so it identifies nothing | y |
| Licence expression | `LICENSE` holds the GPL-2.0 text; every C, C++ and CMake file the project writes starts with `SPDX-License-Identifier: GPL-2.0-or-later`; derived files keep their upstream notice instead | ADR-006: the GPL text alone does not say "or later" | y |
| CI triggers | The template's: pushes to main, pull requests targeting main, manual dispatch and tags | Solo development on main | y |
| What the empty dock shows | One line of text, "Camera Sync 0.1.0", and no controls | Proves the widget rendered; Reading replaces it | y |

**Open questions:** none. Every one is resolved or logged above.

---

## User Stories

### P1: The plugin builds and packages in CI against OBS 30.0 ⭐ MVP

**User Story**: As the maintainer, I want every push and pull request to build and package the plugin for Windows, macOS and Ubuntu against OBS 30.0 headers, so the rig always tests real builds that load on every OBS from 30.0 up.

**Why P1**: Finding 8: a plugin built against newer headers breaks on older OBS. CI packages are what the rig installs.

**Acceptance Criteria** (each line is one EARS pattern):

1. WHEN a commit is pushed to main or a pull request targeting main is opened or updated THEN CI SHALL build the plugin on Windows x64, macOS (universal) and Ubuntu 24.04 x86_64. **FND-01**
2. WHEN a platform's CI build succeeds THEN it SHALL upload that platform's package as a build artifact: a Windows .zip, a macOS .pkg and an Ubuntu .deb. **FND-02**
3. The plugin SHALL compile against OBS Studio 30.0.2's libobs and frontend API headers on all three platforms. **FND-03**
4. IF the plugin is compiled against libobs headers whose major and minor version are not 30.0 THEN the build SHALL fail with an error naming the found and the expected version. **FND-04**
5. WHERE a platform takes obs-deps and Qt 6 from OBS's prebuilt archives (Windows and macOS) the build SHALL use the 2023-11-03 archives that OBS 30.0.2 pins. **FND-05**
6. The macOS build SHALL set its deployment target to macOS 11.0, OBS 30.0's own minimum. **FND-06**
7. The build specification SHALL pin every dependency CI downloads to an exact version with a SHA-256 hash. **FND-07**
8. IF a downloaded dependency does not match its pinned hash THEN the CI build SHALL fail before compiling the plugin. **FND-08**
9. The CI workflows and actions SHALL reference every action outside the repository by full commit SHA. **FND-09**
10. The Ubuntu .deb SHALL declare its OBS dependency as `obs-studio (>= 30.0.0)`, with no upper bound. **FND-10**
11. WHEN CI runs on a push to main or a pull request THEN it SHALL run the template's format checks on C, C++ and CMake files and fail if any file is unformatted. **FND-11**
12. The repository root SHALL hold the GPL-2.0 licence text in `LICENSE`, and every C, C++ and CMake file the project writes SHALL start with `SPDX-License-Identifier: GPL-2.0-or-later`. **FND-12**

**Independent Test**: Push a commit to main and watch CI: three green build jobs, each with its package attached. On a branch, point the OBS dependency at 31.0 and watch the build fail with FND-04's error.

---

### P1: Unit tests run without OBS ⭐ MVP

**User Story**: As the maintainer, I want an OBS-free core library with unit tests that run in every CI build job and on a machine with only a compiler and CMake, so every later task has a test gate, including on the WSL machine.

**Why P1**: tlc-spec-driven gates every core task on tests, and the development machine has no OBS.

**Acceptance Criteria**:

1. The project SHALL build the core as a static library that includes no libobs, OBS frontend or Qt header and links none of their libraries. **FND-13**
2. The plugin module SHALL link the core library. **FND-14**
3. WHEN the core-only configuration is configured, built and tested on a machine with only a C and C++ compiler, CMake 3.28 or later and the repository with its submodules checked out THEN it SHALL build the core and its unit tests and run them through CTest with no network access. **FND-15**
4. WHEN a platform build job runs in CI THEN it SHALL run the unit tests through CTest and fail the job if any test fails. **FND-16**
5. WHEN CI runs on a push to main or a pull request THEN a separate Ubuntu job SHALL configure, build and test the core-only configuration without downloading OBS, obs-deps or Qt. **FND-17**
6. WHEN `ctest -R <name>` is run in a build directory THEN CTest SHALL run only the doctest test cases whose names match `<name>`. **FND-18**

**Independent Test**: On a clean Ubuntu container with only build-essential, CMake and Git, clone with submodules, cut the network, run the core-only configure, build and test, and see CTest report the tests passed.

---

### P1: An empty dock loads in OBS 30.0 and the latest stable ⭐ MVP

**User Story**: As the maintainer, I want the built plugin to load in OBS 30.0.0 and in the latest stable OBS and show an empty "Camera Sync" dock, so the spikes and Reading have a home and the 30.0 minimum is proven on a real OBS.

**Why P1**: It is half of Foundation's gate in the TDD: the plugin loads on the rig.

**Acceptance Criteria**:

1. WHEN OBS 30.0.0 starts with the plugin installed THEN its Docks menu SHALL list "Camera Sync", and choosing it SHALL show the dock. **FND-19**
2. WHEN the latest stable OBS starts with the plugin installed THEN its Docks menu SHALL list "Camera Sync", and choosing it SHALL show the dock. **FND-20**
3. WHEN OBS loads the plugin THEN the OBS log SHALL contain exactly one line `[camera-sync] load version=<plugin version> obs_built=<libobs version compiled against> obs_running=<running OBS version> quirc=<pinned quirc commit, 7 characters>`. **FND-21**
4. The dock SHALL show one line of text, "Camera Sync <plugin version>", and no controls. **FND-22**
5. The dock's title SHALL come from the plugin's en-US locale file. **FND-23**
6. WHEN OBS is closed with the plugin loaded THEN the OBS log's last "Number of memory leaks" line SHALL show the same count as the same OBS version shows when it closes without the plugin. **FND-24**

**Independent Test**: Install the CI package into a portable OBS 30.0.0 and into the latest stable, open the dock in each, close OBS, and compare the log's load line and leak count with a run without the plugin.

---

### P1: Sync Dock's detector and quirc build in the core ⭐ MVP

**User Story**: As the maintainer, I want Sync Dock's detector and quirc in the repository, building inside the core without libobs and keeping their upstream notices, so spike 1 can measure each camera with them and Reading can rework them under test.

**Why P1**: Spike 1 cannot run without a detector inside the plugin, and ADR-005 stays Proposed until the spikes run.

**Acceptance Criteria**:

1. The repository SHALL hold quirc as a Git submodule pinned to commit `542848d`. **FND-25**
2. WHEN the core-only configuration builds THEN it SHALL compile the vendored detector and quirc with no libobs header on the include path. **FND-26**
3. The vendored files SHALL keep Sync Dock's copyright and GPL-2.0-or-later notice verbatim at the top of every file copied or derived from it. **FND-27**
4. The vendored files SHALL each state that they derive from Audio Video Sync Dock, with the upstream file path and commit SHA. **FND-28**
5. WHEN a unit test feeds the vendored QR decoding a luma image of one frame of Sync Dock's pattern THEN it SHALL return exactly the payload values that frame was generated with. **FND-29**
6. IF the quirc submodule is not checked out THEN CMake configure SHALL fail with the message "quirc submodule missing - run: git submodule update --init". **FND-30**

**Independent Test**: In the core-only configuration, run the QR fixture test and see it pass; grep every vendored file for the upstream notice and the provenance line.

---

### P2: The build is documented in CLAUDE.md

**User Story**: As the maintainer, or an agent working in the repository, I want `CLAUDE.md` to say how to build, install and test the plugin, so the next session starts from the facts instead of re-deriving them.

**Why P2**: `CLAUDE.md` asks for it when the build lands. Nothing else depends on it.

**Acceptance Criteria**:

1. WHEN Foundation is complete THEN `CLAUDE.md` SHALL state the toolchain: the OBS version built against, the compiler on each platform, the CMake minimum and every preset name. **FND-31**
2. WHEN Foundation is complete THEN `CLAUDE.md` SHALL give the commands to build the plugin and to install and load it into OBS on Windows, macOS and Ubuntu. **FND-32**
3. WHEN Foundation is complete THEN `CLAUDE.md` SHALL give the commands to run every unit test and to run a single test. **FND-33**
4. WHEN Foundation is complete THEN `CLAUDE.md`'s Repository state section SHALL no longer say there is no code. **FND-34**

**Independent Test**: Follow `CLAUDE.md` from a fresh clone on WSL and run the unit tests and a single test without consulting anything else.

---

## Edge Cases

- IF a downloaded dependency's hash does not match THEN the CI build SHALL fail before compiling (FND-08).
- IF the plugin is compiled against libobs headers other than 30.0 THEN the build SHALL fail naming both versions (FND-04).
- IF the quirc submodule is missing THEN configure SHALL fail with the message in FND-30.
- IF a unit test fails on any platform THEN that platform's CI job SHALL fail (FND-16).

Implicit-requirement sweep, Medium scope: external-dependency failure is covered by FND-08 and FND-30, and observability by FND-21. The remaining dimensions are N/A: Foundation reads no camera or user input, writes no state and takes no concurrent action until Reading wires the taps.

---

## Requirement Traceability

| Requirement ID | Story | Phase | Status |
| -------------- | ----- | ----- | ------ |
| FND-01 | P1: Builds and packages in CI | Tasks | Done (T1) |
| FND-02 | P1: Builds and packages in CI | Tasks | Done (T1) |
| FND-03 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-04 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-05 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-06 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-07 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-08 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-09 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-10 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-11 | P1: Builds and packages in CI | Tasks | In Tasks |
| FND-12 | P1: Builds and packages in CI | Tasks | Partial (T1 done, T4 pending) |
| FND-13 | P1: Unit tests run without OBS | Tasks | In Tasks |
| FND-14 | P1: Unit tests run without OBS | Tasks | In Tasks |
| FND-15 | P1: Unit tests run without OBS | Tasks | In Tasks |
| FND-16 | P1: Unit tests run without OBS | Tasks | In Tasks |
| FND-17 | P1: Unit tests run without OBS | Tasks | In Tasks |
| FND-18 | P1: Unit tests run without OBS | Tasks | In Tasks |
| FND-19 | P1: Empty dock loads | Tasks | In Tasks |
| FND-20 | P1: Empty dock loads | Tasks | In Tasks |
| FND-21 | P1: Empty dock loads | Tasks | In Tasks |
| FND-22 | P1: Empty dock loads | Tasks | In Tasks |
| FND-23 | P1: Empty dock loads | Tasks | In Tasks |
| FND-24 | P1: Empty dock loads | Tasks | In Tasks |
| FND-25 | P1: Detector and quirc in the core | Tasks | In Tasks |
| FND-26 | P1: Detector and quirc in the core | Tasks | In Tasks |
| FND-27 | P1: Detector and quirc in the core | Tasks | In Tasks |
| FND-28 | P1: Detector and quirc in the core | Tasks | In Tasks |
| FND-29 | P1: Detector and quirc in the core | Tasks | In Tasks |
| FND-30 | P1: Detector and quirc in the core | Tasks | In Tasks |
| FND-31 | P2: Build documented | Tasks | In Tasks |
| FND-32 | P2: Build documented | Tasks | In Tasks |
| FND-33 | P2: Build documented | Tasks | In Tasks |
| FND-34 | P2: Build documented | Tasks | In Tasks |

**ID format:** `FND-NN`. The TDD suggests no IDs for phase 0; READ, ALIGN and REL stay reserved for the later slices.

**Coverage:** 34 total, 34 mapped to tasks in `tasks.md`, 0 unmapped.

---

## Success Criteria

- [ ] CI is green on Windows, macOS and Ubuntu for the commit that closes Foundation, with three packages attached.
- [ ] The core-only unit tests pass on the WSL machine once a compiler and CMake are installed there.
- [ ] The "Camera Sync" dock opens in OBS 30.0.0 and in the latest stable on the first OBS machine available, with `obs_built=30.0.2` in the load line.
- [ ] The QR fixture decodes to its generated payload inside the core, with no libobs on the include path.
