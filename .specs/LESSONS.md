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

## Quarantined (failed when applied - ignore)

A confirmed lesson that recurred alongside failure. Kept for the maintainer to review.

_none_
