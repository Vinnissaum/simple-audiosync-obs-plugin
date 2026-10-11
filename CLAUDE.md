# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A simple OBS Studio plugin that keeps the camera video and the audio in sync. It lets the user compensate for the delay between a camera source and an audio source (typically by applying a configurable offset), so the two line up in recordings and streams.

Keep it small: one focused feature, minimal settings, no unrelated functionality.

## Repository state

Freshly initialized. No build system, source or tests exist yet. When they are added, document here:

- the toolchain and setup (OBS SDK / libobs version, compiler, CMake presets),
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

**Scopes** follow the plugin's areas: `sync` (offset/delay logic), `ui` (properties and settings), `build`, `docs`, `ci`.

```
:tada: init project
:sparkles: feat(sync): add configurable audio delay offset
:bug: fix(ui): persist the offset between sessions
:memo: docs(build): document cmake presets
```
