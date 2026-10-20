# LESSONS - auto-maintained by scripts/lessons.py

> Machine-owned. Do NOT hand-edit. Changes are overwritten on the next `lessons.py` write.
> Canonical state lives in `.specs/lessons.json`. Edit lessons only via the script.
> promote_threshold=2 distinct features · window_days=45 · quarantine_threshold=2

## Confirmed (load these at Specify/Design)

Corroborated across multiple features. Safe to apply as guidance.

_none_

## Candidates (under observation - do NOT load as guidance yet)

Seen once or not yet corroborated. Tracked, not trusted.

### L-001 - A file-scanning check's fixture plants one violating file in every directory and file type the check claims to cover
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `cmake-checks` · harmful: 0
- features: foundation
- evidence: M12,M13,M14,M16 tests/cmake/check-notices-fixture.cmake:67-69 (cmake-checks)
- last seen: 2026-10-12T16:12:40Z

### L-002 - Decoder fixtures carry values that set every bit of the decoded field and include a corrupted input the decoder must reject
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `detector` · harmful: 0
- features: foundation
- evidence: M25,M26 core/detector/audio-detector.cpp:83,97 (detector)
- last seen: 2026-10-12T16:12:40Z

### L-003 - Bit-field unpacking tests use values that set the high bits of each field, or the spec states the field's range
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `core` · harmful: 0
- features: foundation
- evidence: M1,M2 core/load-line.cpp:9-10 (core)
- last seen: 2026-10-12T16:12:40Z

### L-004 - Run documented build commands with only the tools the spec names on PATH, and name every tool a preset requires
- signal: `ac_gap` · recurrence: 1 feature(s) · scope: `build` · harmful: 0
- features: foundation
- evidence: FND-15 CMakePresets.json:22 (build)
- last seen: 2026-10-12T16:12:40Z

### L-005 - Take documented package file names from the packaging configuration, not from CI artifact names
- signal: `ac_gap` · recurrence: 1 feature(s) · scope: `docs` · harmful: 0
- features: foundation
- evidence: FND-32 CLAUDE.md:65 (docs)
- last seen: 2026-10-12T16:12:40Z

### L-006 - Dependency hash checks re-verify archives already on disk, not only fresh downloads
- signal: `ac_gap` · recurrence: 1 feature(s) · scope: `build` · harmful: 0
- features: foundation
- evidence: FND-08 cmake/common/buildspec_common.cmake:207 (build)
- last seen: 2026-10-12T16:12:40Z

### L-007 - ACs that inherit template CI behaviour state which events and which files they cover
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `ci` · harmful: 0
- features: foundation
- evidence: FND-02,FND-07,FND-11 .github/workflows/build-project.yaml:36 (ci)
- last seen: 2026-10-12T16:12:40Z

### L-008 - Decoder tests feed a rejected input end to end and assert the decoder emits nothing for it
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `detector` · harmful: 0
- features: reading
- evidence: M51 core/detector/video-detector.cpp:226 (READ-23) (detector)
- last seen: 2026-10-16T22:57:48Z

### L-009 - Test each reset of a timer through a path where no other call has already reset it
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `session` · harmful: 0
- features: reading
- evidence: E2 core/reading/session.cpp:111 (READ-43) (session)
- last seen: 2026-10-16T22:57:48Z

### L-010 - Assert decoded fixture values against the generator's own values, not against another decoder's output
- signal: `ac_gap` · recurrence: 1 feature(s) · scope: `fixtures` · harmful: 0
- features: reading
- evidence: READ-21 tests/detector/pattern-sync.cpp:61, READ-22 tests/reading/estimator-replay.cpp:127 (fixtures)
- last seen: 2026-10-16T22:57:49Z

### L-011 - Spec the exact text of every shown or logged string for each value class, zero and negatives included
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `spec` · harmful: 0
- features: reading
- evidence: SP1 READ-38, SP4 READ-67, SP5 READ-84, SP6 READ-87 (spec)
- last seen: 2026-10-16T22:57:49Z

### L-012 - Spec what each control and timer does in every session state, not only the states the story walks
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `spec` · harmful: 0
- features: reading
- evidence: SP2 READ-36/READ-47, SP7 READ-43, SP8 READ-34 (spec)
- last seen: 2026-10-16T22:57:49Z

### L-013 - When two rules can claim the same item, the spec states which rule wins
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `spec` · harmful: 0
- features: reading
- evidence: SP3 READ-51/READ-60 tests/selection/selection-policy.cpp:197 (spec)
- last seen: 2026-10-16T22:57:49Z

### L-014 - Spec examples use the rounded values the input actually carries
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `spec` · harmful: 0
- features: reading
- evidence: SP9 READ-01/READ-02 tests/reading/pattern.cpp:18 (spec)
- last seen: 2026-10-16T22:57:49Z

### L-015 - Check a libobs call path against the pinned OBS source before the design depends on it
- signal: `spec_deviation` · recurrence: 1 feature(s) · scope: `src/obs` · harmful: 0
- features: reading
- evidence: SPEC_DEVIATION src/obs/camera-tap.hpp:24 (src/obs)
- last seen: 2026-10-16T22:57:49Z

### L-016 - When several consumers share one input list, the design states whether using an item consumes it for all or for one
- signal: `spec_deviation` · recurrence: 1 feature(s) · scope: `estimator` · harmful: 0
- features: reading
- evidence: SPEC_DEVIATION core/reading/estimator.hpp:90 (estimator)
- last seen: 2026-10-16T22:57:49Z

### L-017 - A value read at each call is a method argument, not constructor configuration
- signal: `spec_deviation` · recurrence: 1 feature(s) · scope: `session` · harmful: 0
- features: reading
- evidence: SPEC_DEVIATION core/reading/session.hpp:71 (session)
- last seen: 2026-10-16T22:57:49Z

### L-018 - Build core changes with -Wconversion before a CI push, since MSVC warnings-as-errors rejects narrowing that gcc accepts
- signal: `gate_fail` · recurrence: 1 feature(s) · scope: `build` · harmful: 0
- features: reading
- evidence: 6c9896c core/detector/qr-payload.hpp:39 (MSVC C4838) (build)
- last seen: 2026-10-16T22:57:49Z

### L-019 - Keep backslashes out of doctest case names, since doctest_discover_tests writes them to CTest unescaped
- signal: `gate_fail` · recurrence: 1 feature(s) · scope: `tests` · harmful: 0
- features: reading
- evidence: tests/core/log-lines.cpp:39 (reproduced: 158 to 157 tests, Not Run) (tests)
- last seen: 2026-10-16T22:57:49Z

### L-020 - Make the target CI builds before CTest depend on every executable CTest runs
- signal: `gate_fail` · recurrence: 1 feature(s) · scope: `ci` · harmful: 0
- features: reading
- evidence: 0b43200 tests/CMakeLists.txt:57 (macOS CTest) (ci)
- last seen: 2026-10-16T22:57:49Z

### L-021 - Prove a dropped item left the selection by re-adding it or restarting, not only by its missing row
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `session` · harmful: 0
- features: reading
- evidence: F5 core/reading/session.cpp:228 (READ-57 untick) (session)
- last seen: 2026-10-16T23:19:52Z

### L-022 - Test each threshold comparison at its exact boundary, including the condition that restarts a timer
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `session` · harmful: 0
- features: reading
- evidence: F6 core/reading/session.cpp:256 (READ-42/43 restart boundary) (session)
- last seen: 2026-10-16T23:19:52Z

### L-023 - Spec when a timer starts over, not only when it fires and when it clears
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `spec` · harmful: 0
- features: reading
- evidence: SP10 READ-42/READ-43 .specs/features/reading/design.md:169 (spec)
- last seen: 2026-10-16T23:19:52Z

## Quarantined (failed when applied - ignore)

A confirmed lesson that recurred alongside failure. Kept for the maintainer to review.

_none_
