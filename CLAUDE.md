# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

An OBS Studio plugin that aligns several cameras with one mic. Each camera reaches OBS with its own lag, and one audio offset can only match one of them. A phone plays a test pattern where every camera can see it and the mic can hear it. The plugin measures every selected camera against the mic at the same time, then writes the corrections into OBS's own settings in one undoable Apply: a Video Delay (Async) filter on each camera and the mic's Sync Offset.

The design is `.design/multi-camera-sync.md`; plan from it. Its Key decisions are binding, and its Boundary lists what is deliberately out: drift, more than one mic, screen captures, and measuring without the pattern. Use its terms in code and prose: camera, mic, pattern, reading, alignment, Apply, Undo.

Each Key decision is recorded as an ADR in `docs/adr/` (MADR; index in `docs/adr/README.md`). ADRs are immutable: a changed decision gets a new ADR that supersedes the old one. ADR-005 (reusing Sync Dock's detector) and ADR-007 (measuring all cameras in one pass) stay Proposed until the Reading spikes pass.

The technical design is `docs/tdd/multi-camera-sync.md`: how to build what the design describes, with the libobs findings that shape the measuring taps, the component architecture, the alignment arithmetic, the template build changes and the phased plan. Where it disagrees with the design or an ADR, they win.

The pattern and its detector are reused from Audio Video Sync Dock (GPL-2.0-or-later), so this plugin is GPL-2.0-or-later as well. Keep the upstream copyright notices on anything taken from it.

## Repository state

No code yet. The build will follow OBS's official plugin template: CMake presets, with CI building for Windows, macOS and Linux. Work starts with the Reading slice's two spikes. They need an OBS machine with two cameras that lag differently, a mic, and a phone to play the pattern. This WSL machine has no OBS, CMake or cameras, so testing happens on that OBS machine.

When the build lands, document here:

- the toolchain and setup (libobs version, compiler, CMake presets),
- how to build, install and load the plugin into OBS,
- how to run tests, including a single test.

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
