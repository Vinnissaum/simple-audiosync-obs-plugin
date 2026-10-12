# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

An OBS Studio plugin that aligns several cameras with one mic. Each camera reaches OBS with its own lag, and one audio offset can only match one of them. A phone plays a test pattern where every camera can see it and the mic can hear it. The plugin measures every selected camera against the mic at the same time, then writes the corrections into OBS's own settings in one undoable Apply: a Video Delay (Async) filter on each camera and the mic's Sync Offset.

The design is `.design/multi-camera-sync.md`; plan from it. Its Key decisions are binding, and its Boundary lists what is deliberately out: drift, more than one mic, screen captures, and measuring without the pattern. Use its terms in code and prose: camera, mic, pattern, reading, alignment, Apply, Undo.

Each Key decision is recorded as an ADR in `docs/adr/` (MADR; index in `docs/adr/README.md`). ADRs are immutable: a changed decision gets a new ADR that supersedes the old one. ADR-005 (reusing Sync Dock's detector) and ADR-007 (measuring all cameras in one pass) stay Proposed until the Reading spikes pass.

The technical design is `docs/tdd/multi-camera-sync.md`: how to build what the design describes, with the libobs findings that shape the measuring taps, the component architecture, the alignment arithmetic, the template build changes and the phased plan. Where it disagrees with the design or an ADR, they win.

The pattern and its detector are reused from Audio Video Sync Dock (GPL-2.0-or-later), so this plugin is GPL-2.0-or-later as well. Keep the upstream copyright notices on anything taken from it.

## Repository state

Foundation, the TDD's phase 0, has landed (`.specs/features/foundation/`). The repository holds:

- `core/`: the OBS-free static library `camera-sync-core`. It includes no libobs, OBS frontend or Qt header. It formats the load line and holds Sync Dock's detector, vendored in `core/detector/` with its notices (`core/media.hpp` and `core/log.hpp` are the core types and the injected logger that replace libobs's).
- `src/`: the plugin module. It logs the load line and registers an empty "Camera Sync" dock.
- `tests/`: doctest unit tests, CMake checks run by CTest, and `tests/fixtures/` (written by `tests/fixtures/generate.sh` with Sync Dock's generator, committed so tests never need Python).
- `deps/quirc` (Git submodule, pinned to `542848d`), `third_party/doctest`, and `cmake/camera-sync/` (the project's own CMake checks).

Not yet done: the Reading spikes, and the load check of the dock in OBS 30.0.0 and the latest stable (Foundation T16). Both need an OBS machine with two cameras that lag differently, a mic, and a phone to play the pattern. This WSL machine has no OBS or cameras, so anything that needs OBS is tested on that machine. Everything in `core/` and `tests/` builds and tests here.

## Toolchain

- OBS Studio 30.0.2 headers (libobs and the frontend API) on every platform, so the plugin loads in OBS 30.0.0 and up. The build refuses libobs headers other than 30.0.x.
- Windows and macOS take obs-deps and Qt 6 from OBS's prebuilt archives dated 2023-11-03, which OBS 30.0.2 pins. Ubuntu builds libobs and the frontend API from the OBS 30.0.2 source archive and takes Qt 6 from Ubuntu 24.04's packages. `buildspec.json` pins every download with a SHA-256 hash.
- Compilers: Visual Studio 2022 on Windows (x64, Windows SDK 10.0.20348 or later), Xcode 16 or later on macOS (universal arm64 and x86_64, deployment target macOS 11.0), the Ubuntu 24.04 GCC (13) on Ubuntu. The core-only build needs any C and C++17 compiler.
- CMake 3.28 or later. Ninja for the `core` and Ubuntu presets. `cmake` and `ninja` must be on `PATH`; on the WSL machine they live in `~/.local/bin`, so run `export PATH=$HOME/.local/bin:$PATH` first.
- Presets (`CMakePresets.json`; each configure preset has a build preset of the same name): `core`, `macos`, `macos-ci`, `windows-x64`, `windows-ci-x64`, `ubuntu-x86_64`, `ubuntu-ci-x86_64`. `core` also has a test preset.
- Formatting: CI's Check Formatting runs clang-format (`.clang-format`) on `src/`, `core/` and `tests/`, and gersemi 0.21.0 (`.gersemirc`) on the CMake files. `third_party/` and `deps/` are skipped. Neither tool is installed on the WSL machine.

## Build

Clone with the submodule, or run `git submodule update --init` in an existing clone. CMake stops with "quirc submodule missing - run: git submodule update --init" otherwise.

```
git clone --recurse-submodules https://github.com/Vinnissaum/simple-audiosync-obs-plugin.git
```

Core only, on any machine with a compiler, CMake and Ninja, with no network access after the clone (this is the quick gate):

```
cmake --preset core && cmake --build --preset core && ctest --preset core --output-on-failure
```

The whole plugin. The first configure downloads the pinned OBS dependencies into `.deps/`.

- Windows (PowerShell, from the repository root): `cmake --preset windows-x64`, then `cmake --build --preset windows-x64 --config RelWithDebInfo`, then `cmake --install build_x64 --config RelWithDebInfo --prefix release`.
- macOS (needs the Xcode generator): `.github/scripts/build-macos --config RelWithDebInfo`. Add `--package` to build the .pkg.
- Ubuntu 24.04 (the script installs the build dependencies with `sudo apt-get`): `.github/scripts/build-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo`, then `.github/scripts/package-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo --package` for the .deb.

CI builds all three on every push to main and every pull request, runs the unit tests in each job and in a separate core-only job, and attaches a Windows .zip, a macOS .pkg and an Ubuntu .deb to the run as build artifacts.

## Install and load into OBS

Use the CI artifacts, or the files a local build puts under `release/`. Restart OBS, then open the Docks menu and choose "Camera Sync". The OBS log shows one line, `[camera-sync] load version=... obs_built=30.0.2 obs_running=... quirc=542848d`.

- Windows: unzip the .zip into `%ProgramData%\obs-studio\plugins\`, so that `camera-sync\bin\64bit\camera-sync.dll` and `camera-sync\data\locale\en-US.ini` exist. For a portable OBS, copy the DLL to `<OBS>\obs-plugins\64bit\` and the `data` folder to `<OBS>\data\obs-plugins\camera-sync\`.
- macOS: run the .pkg, or copy `camera-sync.plugin` to `~/Library/Application Support/obs-studio/plugins/`. The package is unsigned, so macOS asks you to allow it.
- Ubuntu: `sudo apt install ./camera-sync-0.1.0-ubuntu-24.04-x86_64.deb`. The package depends on `obs-studio (>= 30.0.0)`.

## Tests

The tests are doctest cases, listed one by one in CTest, plus CMake checks that run through `cmake -P`. Run them in the `build_core` directory after the core build above.

Run every test:

```
ctest --preset core
```

Run a single test, or the tests whose names match a pattern (the name is the doctest case name or the CMake check's name):

```
ctest --preset core -R load_line
ctest --preset core -R "check_notices_fixture"
```

List the test names with `ctest --preset core -N`.

The notice check (`check_notices_repository`) fails, naming the file, when a C, C++ or CMake file the project writes does not start with `SPDX-License-Identifier: GPL-2.0-or-later`, or when a file in `core/detector/` does not start with Sync Dock's notice followed by its `Derived from Audio Video Sync Dock, <path> at <commit>` line.

## Commit convention

Conventional Commits with a leading gitmoji **shortcode**.

```
:emoji: type(scope): imperative description

[optional body]

[optional footer]
```

Rules: shortcode form (`:sparkles:`), not the literal glyph. Lowercase description, imperative mood, no trailing period, under ~72 characters. Breaking changes use `!` after the scope (`:boom: feat(api)!: ...`) plus a `BREAKING CHANGE:` footer.

| Type       | Emoji                   | Use for                                |
| ---------- | ----------------------- | -------------------------------------- |
| `feat`     | `:sparkles:`            | New capability                         |
| `fix`      | `:bug:`                 | Bug fix                                |
| `docs`     | `:memo:`                | Documentation                          |
| `refactor` | `:recycle:`             | Restructuring with no behavior change  |
| `perf`     | `:zap:`                 | Performance                            |
| `test`     | `:white_check_mark:`    | Adding or fixing tests                 |
| `build`    | `:package:`             | Build system, dependencies             |
| `ci`       | `:construction_worker:` | Pipeline configuration                 |
| `chore`    | `:wrench:`              | Tooling, config, housekeeping          |
| `style`    | `:art:`                 | Formatting only                        |
| `revert`   | `:rewind:`              | Reverting a previous commit            |

Situational emoji override the table when more specific: `:lock:` security, `:arrow_up:`/`:arrow_down:` dependency bumps, `:fire:` removing code, `:construction:` work in progress, `:tada:` project initialization.

**Scopes** follow the design's slices: `reading` (measuring cameras against the mic), `alignment` (Apply and Undo), `release` (packaging and publishing). Changes that span the plugin, such as build setup, CI or cross-cutting docs, omit the scope.

```
:tada: init project
:sparkles: feat(reading): measure every selected camera against the mic
:bug: fix(alignment): restore every value when a write fails
:memo: docs: record the multi-camera sync design
```
