# Alignment Tasks

> **Superseded on 2026-10-28.** At the maintainer's choice, Alignment is built with tlc-spec-lean from `plan.md` and `checks.md` in this folder. This task list and its Execution Protocol no longer apply; the file is kept for its history, and its "Done when" examples are carried into `plan.md`'s criteria.

## Execution Protocol (MANDATORY -- do not skip)

Implement these tasks with the `tlc-spec-driven` skill: **activate it by name and follow its Execute flow and Critical Rules.** Do not search for skill files by filesystem path. The skill is the source of truth for the full flow (per-task cycle, sub-agent delegation, adequacy review, Verifier, discrimination sensor).

**If the skill cannot be activated, STOP and tell the user - do not proceed without it.**

---

**Spec**: `.specs/features/alignment/spec.md` (ALIGN-01 to ALIGN-81)
**Design**: `.specs/features/alignment/design.md`
**Context**: `.specs/features/alignment/context.md`
**Status**: Approved by the maintainer, 2026-10-20. Not started.

**Prerequisites before Execute:**

- `export PATH=$HOME/.local/bin:$PATH` in every shell: CMake and Ninja live there on the WSL machine. gcc 13 is installed.
- Every `ci` gate: push access, and either `gh` authenticated or the maintainer reporting each run, as in Reading.
- T17 to T19: the Ubuntu CI job, which builds libobs from OBS 30.0.2's sources. The WSL machine has no OBS build; adapter code is syntax-checked locally against OBS 30.0.2's headers in a scratch directory before the push, as Reading's T27 did.
- Phase 7 (T22, T23): the rig, after Reading's T34 to T37, with OBS 30.0.0 and the latest stable, two cameras of different lags (one with a Render Delay), a mic, a phone, and four cameras for T23's timing.

**Commit messages** follow `CLAUDE.md` (gitmoji shortcode, Conventional Commits, scope `alignment`). The skill's `check_commit.py` does not know the gitmoji prefix, so it runs on the message with the leading `:shortcode: ` removed.

**Execution order is task-number order.** Dependencies on earlier phases are listed in each task's `Depends on` and drawn only in the cross-check table.

**Tools for every task:** MCP none, Skill none. Ask before changing that.

---

## Test Coverage Matrix

> Generated from the repository's tests and guidelines. Guidelines found: `CLAUDE.md` (Tests, Build), `docs/tdd/multi-camera-sync.md` (Testing Strategy: every Alignment row and test vector as a unit test, a failure at each write position, Apply and Undo against a headless libobs, 90% core line coverage), `CMakePresets.json` (the `core` preset), `.github/workflows/build-project.yaml` (the core-only job's coverage gate, the Ubuntu job's CTest step). Samples read: `tests/reading/session.cpp` (fakes, harness), `tests/viewmodel/view-model.cpp`, `tests/viewmodel/strings.cpp`, `tests/core/log-lines.cpp`.

| Code Layer | Required Test Type | Coverage Expectation | Location Pattern | Run Command |
| ---------- | ------------------ | -------------------- | ---------------- | ----------- |
| Core logic (`core/alignment/`, the session hooks in `core/reading/session.*`, `core/viewmodel/`, `core/log-lines.*`) | unit (doctest through CTest) | Every spec AC the task carries, 1:1, with exact expected values and strings; all eight TDD vectors; a boundary on each side of every threshold (±F, 20,000 ms); a failure injected at every write and every read-back position; at least 90% of lines across `core/` excluding `core/detector/` (the core-only job's gate) | `tests/alignment/*.cpp`, `tests/reading/session.cpp`, `tests/viewmodel/*.cpp`, `tests/core/log-lines.cpp` | quick gate |
| Settings port's OBS implementation (`src/obs/obs-settings-port.*`) | integration (doctest under headless libobs, Ubuntu CI) | Every port operation, happy and failure paths; the plugin filter found in chain order by type and name; other filters unchanged and in order; Apply and Undo through it; a source removed between reads and writes | `tests/integration/*.cpp` | ci gate |
| Plugin adapters, controller and dock (`src/`, other than the settings port) | build + UAT | Builds and packages on three platforms; rules stay in the core; every behavior walked on the rig in phase 7 | - | ci gate, then rig gate |
| CMake wiring, CI workflows | build | The named CI jobs pass | - | ci gate |

## Gate Check Commands

> Generated from the `core` preset and the existing CI. Confirm before Execute.

| Gate Level | When to Use | Command |
| ---------- | ----------- | ------- |
| Quick | Tasks that change the core or its tests | `export PATH=$HOME/.local/bin:$PATH && cmake --preset core && cmake --build --preset core && ctest --preset core --output-on-failure` |
| CI | Tasks that change `src/`, `tests/integration/`, CMake wiring or the workflows | Run the quick gate, push the task's commit, and wait: Check Formatting, Build for Windows, Build for macOS, Build for Ubuntu (with the integration tests from T17 on) and the core-only job (with its 90% coverage gate and the 60 s fuzz) all pass |
| Rig | T22, T23 | Install the CI packages into OBS 30.0.0 and the latest stable on the rig and walk the task's requirements through interactive UAT |

Every task keeps the earlier gates green. A test count never drops: record the `ctest --preset core -N` total before and after. The count before this feature is 164.

---

## Execution Plan

Phases run in order, and tasks in a phase run in order.

### Phase 1: Shared types and session hooks

```
T1
T2
```

### Phase 2: Calculator

```
T3 → T4
```

### Phase 3: Writing and log lines

```
T5
T6
```

### Phase 4: Apply and Undo

```
T7 → T8 → T9 → T10 → T11
```

### Phase 5: View-model

```
T12 → T13 → T14 → T15
T12 → T16
```

### Phase 6: OBS adapter, integration tests, controller and dock

```
T17 → T18 → T19
T18 → T20 → T21
```

### Phase 7: Rig

```
T22 → T23
```

---

## Task Breakdown

### Phase 1 tasks: Shared types and session hooks

#### T1: Add the alignment types and the settings port interface

**What**: `core/alignment/types.hpp` with the design's shared data (`FilterState` with `effective_ms()` and `==`, `NamedSource`, `Counts`, `OutcomeKind`, `NamedValue`, `Outcome`, `AlignmentStatus`) and `core/alignment/settings-port.hpp` with the `SettingsPort` interface and `recognised_filter_names`.
**Where**: `core/alignment/types.hpp` (with `core/alignment/settings-port.hpp`)
**Depends on**: None
**Reuses**: `SourceId` from `core/reading/types.hpp`; the abstract-interface style of `TapFactory`
**Requirement**: ALIGN-03 (effective delay), ALIGN-41 (the port's operations), ALIGN-42 (the recognised names)

**Done when**:

- [ ] `FilterState::effective_ms()` is `delay_ms` when present and enabled, and 0 when absent or disabled; `==` compares present, `delay_ms` and enabled.
- [ ] `SettingsPort` declares exactly the eight operations of ALIGN-41 with the design's signatures, and nothing else.
- [ ] `recognised_filter_names` holds "Camera Sync Delay" and nothing else; its first entry is the name a new filter gets.
- [ ] Both headers include no libobs, frontend or Qt header (the core's rule).
- [ ] `tests/alignment/types.cpp` added to `tests/CMakeLists.txt`. Added doctest cases: at least 3 (effective delay for absent, disabled and enabled filters; equality on each field; the names list). Quick gate passes. Test count: 164 plus the added.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): add the alignment types and the settings port`

---

#### T2: Let Apply clear the readings and read F from the session

**What**: `Session::clear_readings()` returns every camera to Looking and keeps the pattern; `on_mic_sync_offset_changed()` calls it; `SessionSnapshot::frame_interval_ms` carries the F read at the last Start.
**Where**: `core/reading/session.cpp` (with `core/reading/session.hpp`)
**Depends on**: None
**Reuses**: `estimator_.clear()`, `frame_interval_ms`; the Reading harness in `tests/reading/session.cpp`
**Requirement**: ALIGN-28 (readings cleared, pattern kept), ALIGN-58 (F for the check marks)

**Done when**:

- [ ] After markers make two cameras Stable, `clear_readings()` leaves both Looking with 0 samples, and a new pair of markers with the same pattern forms a sample at once (the pattern was kept). The session's state is unchanged, in Measuring and in Stopped.
- [ ] `on_mic_sync_offset_changed()` behaves as before (the READ-59 case still passes unchanged).
- [ ] `snapshot().frame_interval_ms` is 0 before any Start, 33.333… after Start at 30/1 and 16.683… at 60000/1001 (each within 0.001), and keeps its value through Stop.
- [ ] Added doctest cases: at least 2, in `tests/reading/session.cpp`. No existing case changed. Quick gate passes. Test count: T1's total plus the added.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): let the session clear readings and report its frame interval`

---

### Phase 2 tasks: Calculator

#### T3: Decide whether Apply can go ahead

**What**: `precheck(readings, frame_interval_ms)` returns Waiting with the first camera that is not Stable, AlreadyAligned when every reading is within ±F, and Ready otherwise.
**Where**: `core/alignment/calculator.cpp` (with `core/alignment/calculator.hpp`)
**Depends on**: T1
**Reuses**: nothing
**Requirement**: ALIGN-01, ALIGN-02

**Done when**:

- [ ] Stable, Settling, Stable → Waiting with index 1; Looking, Settling → Waiting with index 0 (the first in the given order, ALIGN-01).
- [ ] With F = 33.333: readings (+20, −30) → AlreadyAligned; (+33, −33) → AlreadyAligned; (+34, 0) → Ready; (0, −34) → Ready. With F = 33.0: +33 → AlreadyAligned, +34 → Ready (both sides of ±F, ALIGN-02).
- [ ] A Settling camera with any reading gives Waiting, never AlreadyAligned (waiting is decided first).
- [ ] `core/CMakeLists.txt` and `tests/CMakeLists.txt` list the new files. Added doctest cases: at least 3, in `tests/alignment/calculator.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): decide when apply waits or is already aligned`

---

#### T4: Compute the alignment plan

**What**: `compute_plan(readings_ms, filters, sync_offset_ns)` returns the plan (Δ, each camera's old and new delay and changed flag, the mic's old and new offset and changed flag) or the first source over 20,000 ms.
**Where**: `core/alignment/calculator.cpp` (modify, with `core/alignment/calculator.hpp`)
**Depends on**: T3
**Reuses**: `round_half_away` (`core/reading/pattern.hpp`), `FilterState::effective_ms`
**Requirement**: ALIGN-03, ALIGN-04, ALIGN-05, ALIGN-06, ALIGN-07, ALIGN-08, ALIGN-09, ALIGN-10

**Done when**:

- [ ] The TDD's seven computed vectors at F = 33 give exactly their Δ and after-values: (+100, +40) → Δ 100, d′ (0, 60), s′ 100; (−50, −120) → 0, (50, 120), 0; (+40, +40) with d (0, 60), s 100 → 40, (0, 60), 140; (+150, +30) with d (200, 0), s 100 → 30, (80, 0), 130; (−50, −30) with s 100 → −30, (20, 0), 70; (+480, +30) → 480, (0, 450), 480; (+60, +10) with s −200 → 200, (140, 190), 0 (ALIGN-08; the eighth vector, already aligned, is T3's).
- [ ] A disabled filter carrying 60 ms counts as 0: reading +100 and a second camera at +40 with no filter gives d′ (0, 60) and marks the first camera unchanged (effective 0 → 0), and the same with the filter enabled at 60 gives Δ 40 (ALIGN-03).
- [ ] A Sync Offset of −200,400,000 ns rounds to −200 ms and −200,600,000 ns to −201 ms before Δ is taken (ALIGN-04).
- [ ] Every vector, and a property loop over a grid of readings from −1,000 to +1,000 ms in steps of 37 with d in {0, 200} and s in {−950, 0, 300}, gives no negative new value (ALIGN-06).
- [ ] A new delay of exactly 20,000 ms is accepted and 20,001 ms refused naming that camera; a new offset of exactly 20,000 accepted and 20,001 refused naming the mic (index = cameras.size()); with two cameras over, the first in order is named (ALIGN-07).
- [ ] Changed flags: vector 1 marks camera A unchanged, B and the mic changed; vector 2 marks the mic unchanged (ALIGN-09). A camera with no filter and a new delay of 0 is unchanged, so no step will create a filter (ALIGN-10).
- [ ] A single camera: +75 with s 0 → Δ 75, d′ 0, s′ 75.
- [ ] Added doctest cases: at least 8, in `tests/alignment/calculator.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): compute each camera's delay and the mic's sync offset`

---

### Phase 3 tasks: Writing and log lines

#### T5: Write steps with read-back and restore

**What**: `write_steps(port, steps)` writes each step, reads it back, and on the first failure puts back every step written so far, the failed one included, in reverse order, carrying on past failures while putting back.
**Where**: `core/alignment/write-steps.cpp` (with `core/alignment/write-steps.hpp`; the test double `tests/support/fake-port.hpp`)
**Depends on**: T1
**Reuses**: `SettingsPort`
**Requirement**: ALIGN-24, ALIGN-25, ALIGN-26 (the routine), ALIGN-38, ALIGN-39 (the same routine for Undo)

**Done when**:

- [ ] `FakePort` in `tests/support/fake-port.hpp` holds filters and offsets per source, records every call in order, and can be told to fail a chosen write, return a different value on a chosen read-back, or forget a source. Later tasks reuse it.
- [ ] Three steps (two cameras, the mic) all land: the call log is write, read, write, read, write, read in step order; `ok`, `written == 3`.
- [ ] A camera step with an absent target calls `remove_filter`; one with a present target calls `write_filter` with its delay and enabled state.
- [ ] A failure injected at each of the three write positions, and at each of the three read-backs, leaves every fake value equal to the steps' found values, with filters a step created removed; `failed` names the step; `written` counts the steps attempted (ALIGN-24, ALIGN-25).
- [ ] The failed step is put back too: a created filter whose read-back delay differs is removed.
- [ ] A failure injected into one restore write still restores the other steps and reports that step in `restore_failures` with the value read back (ALIGN-26).
- [ ] Added doctest cases: at least 6, in `tests/alignment/write-steps.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): write values with read-back and restore`

---

#### T6: Format the Apply and Undo log lines

**What**: The log-line functions of the design: plan, ok, failed, restore failed (camera and mic), refused, and the already-aligned and cancelled events, each with an `apply` or `undo` prefix.
**Where**: `core/log-lines.cpp` (with `core/log-lines.hpp`)
**Depends on**: T1
**Reuses**: `quote`
**Requirement**: ALIGN-70, ALIGN-71, ALIGN-72, ALIGN-73, ALIGN-74, ALIGN-75, ALIGN-76 (the formats; emitting them is T7, T8 and T10)

**Done when**:

- [ ] Vector 1's plan for cameras "Cam A" (no filter) and "Cam B" (disabled at 60) and mic "Shure MV7" reads `apply.plan camera="Cam A" delay_ms=none->0 camera="Cam B" delay_ms=60(off)->60 mic="Shure MV7" sync_offset_ms=0->100` (ALIGN-70).
- [ ] `apply.result ok written=2 skipped=1 duration_ms=4` (ALIGN-71); `apply.result failed step=camera:"Cam B" restored=yes` and `step=mic:"Shure MV7" restored=no` (ALIGN-72).
- [ ] `restore.failed camera="Cam B" intended_ms=0 actual_ms=60`, `intended_ms=none` for an absent filter, and `restore.failed mic="Shure MV7" intended_ns=-50000000 actual_ns=0` (ALIGN-73).
- [ ] `apply.refused reason=waiting camera="Cam A"`, `reason=source_gone source="<uuid>"`, `reason=no_delay_filter`, `reason=over_limit source="Cam B" value_ms=20001`, `reason=changed`; `apply.already_aligned`, `apply.cancelled` (ALIGN-74).
- [ ] Each of the above with the `undo` prefix reads the same with `undo.` in place of `apply.` (ALIGN-75).
- [ ] A camera named `Cam "B"` is written `camera="Cam \"B\""` in every line that names it (ALIGN-76).
- [ ] Added doctest cases: at least 6, in `tests/core/log-lines.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): format the apply and undo log lines`

---

### Phase 4 tasks: Apply and Undo

#### T7: Refuse Apply before it writes

**What**: `Alignment` with its constructor, `status()`, and `apply()` up to the plan: the precheck, resolving every source, the delay-filter check, reading the restore point, and the over-limit refusal, each with its log line and outcome.
**Where**: `core/alignment/alignment.cpp` (with `core/alignment/alignment.hpp`)
**Depends on**: T4, T5, T6
**Reuses**: `precheck`, `compute_plan`, `FakePort`, the new log lines; `FakeClock` and the recording logger style of `tests/reading/session.cpp`
**Requirement**: ALIGN-11, ALIGN-12, ALIGN-13, ALIGN-14, ALIGN-15, ALIGN-16, ALIGN-74 (emitted)

**Done when**:

- [ ] `apply()` with no mic, with no camera, and with a ticked camera Settling makes no port call at all (the fake's call log is empty) and returns Waiting naming the first such camera in the request's order; Waiting is not stored as the last result (ALIGN-11).
- [ ] With every reading within ±F, `apply()` makes no port call, returns AlreadyAligned, stores it as the last result, and logs `apply.already_aligned` at Info (ALIGN-12).
- [ ] A ticked camera or the mic the fake has forgotten → no write call, outcome ApplyFailed, `apply.refused reason=source_gone` at Warning (ALIGN-13).
- [ ] `delay_filter_available()` false → no write call, ApplyFailed, `reason=no_delay_filter` (ALIGN-14).
- [ ] With every source present, `apply()` reads each camera's filter and the mic's offset once, in request order, before any write (ALIGN-15).
- [ ] A plan over the limit → no write call, OverLimit with the named source's name, `reason=over_limit` with the value (ALIGN-16).
- [ ] A refused or failed Apply before any write leaves the fake measuring control untouched (no clear, no resume; ALIGN-29's second half).
- [ ] `core/CMakeLists.txt` and `tests/CMakeLists.txt` list the new files. Added doctest cases: at least 6, in `tests/alignment/apply.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): refuse apply before anything is written`

---

#### T8: Write the plan and keep it for Undo

**What**: `apply()` turns the plan into steps (changed cameras in order, then a changed mic), writes them with `write_steps`, and on success keeps the restore point for Undo, clears the readings and resumes a Stopped session; on failure it clears the readings if a write was attempted.
**Where**: `core/alignment/alignment.cpp` (modify, with `core/alignment/alignment.hpp`)
**Depends on**: T7
**Reuses**: `write_steps`, `FakePort`; a `FakeMeasuring` recording `clear_readings` and `resume`
**Requirement**: ALIGN-20, ALIGN-21, ALIGN-22, ALIGN-23, ALIGN-25, ALIGN-26, ALIGN-27, ALIGN-28, ALIGN-29, ALIGN-30, ALIGN-70, ALIGN-71, ALIGN-72, ALIGN-73 (emitted)

**Done when**:

- [ ] Vector 1 off air: the write calls are Cam B's filter (created, 60 ms, enabled), then the mic at 100,000,000 ns; Cam A is not written; `apply.plan` is logged before the first write and `apply.result ok written=2 skipped=1 duration_ms=<d>` after, with d from the fake clock (ALIGN-20, ALIGN-21, ALIGN-23, ALIGN-70, ALIGN-71).
- [ ] A camera with an enabled filter at 200 and a new delay of 80 gets `write_filter(80, enabled)`; one whose new delay is 0 gets `write_filter(0, enabled)` and no `remove_filter`; a disabled filter at 60 given 60 is written enabled (ALIGN-22).
- [ ] On success: `status().undo_available`, the snapshot holds every ticked camera's found `FilterState` and the mic's found ns exactly, replacing an earlier one; outcome Applied with each ticked camera's new delay in order, then the mic's new offset; `aligned_after_apply` set (ALIGN-27).
- [ ] On success the fake measuring sees `clear_readings()` and then, for a Stopped session, `resume(rate)` with the request's rate; for a Measuring session `clear_readings()` only (ALIGN-28).
- [ ] A failure at each write position: every fake value back as before, outcome ApplyFailed, `apply.result failed step=... restored=yes`, `clear_readings()` called, no `resume`, and an earlier Undo snapshot unchanged (ALIGN-25, ALIGN-29, ALIGN-30, ALIGN-72).
- [ ] A failed restore: ApplyRestoreFailed, `restored=no` with a `restore.failed` line per value, the Undo snapshot dropped and `aligned_after_apply` cleared (ALIGN-26, ALIGN-73).
- [ ] Added doctest cases: at least 7, in `tests/alignment/apply.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): write the alignment all or nothing`

---

#### T9: Ask before Apply writes on air

**What**: While the port reports on air, `apply()` asks the `Confirm` callback with the plan's counts before writing; after a yes it resolves and reads again and checks the generation counter that `collection_changing()` bumps.
**Where**: `core/alignment/alignment.cpp` (modify, with `core/alignment/alignment.hpp`)
**Depends on**: T8
**Reuses**: `FakePort`; a fake `Confirm` that records its counts and can change the fake port or call `collection_changing()` before answering
**Requirement**: ALIGN-17, ALIGN-18, ALIGN-19, ALIGN-32 (collection switch), ALIGN-74 (`reason=changed`, `apply.cancelled`)

**Done when**:

- [ ] Off air, `Confirm` is never called. On air with vector 1, it is called once, after the reads and before any write, with `{cameras = 1, mic = true}`; vector 2 gives `{cameras = 2, mic = false}` (ALIGN-17).
- [ ] A no → no write call, outcome Cancelled, `apply.cancelled` at Info, the last result and the Undo snapshot as they were (ALIGN-18).
- [ ] A yes after the fake changed a camera's delay, the mic's offset, or forgot a source while the question was open → no write call, ApplyFailed, `reason=changed` or `reason=source_gone` (ALIGN-19).
- [ ] A yes after `collection_changing()` was called from inside the question → no write call, ApplyFailed, `reason=changed`, even though every source still resolves with the same values.
- [ ] `collection_changing()` drops the Undo snapshot, `aligned_after_apply` and the last result (ALIGN-32's collection switch).
- [ ] `duration_ms` counts from the answer, not from the request: a fake clock advanced 3 s inside the question and 4 ms by the writes logs `duration_ms=4`.
- [ ] Added doctest cases: at least 5, in `tests/alignment/apply.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): ask before applying on air`

---

#### T10: Undo the last Apply

**What**: `undo()` resolves the snapshot's sources, reads its restore point, asks while on air, writes the values that differ from the snapshot's, and on success drops the snapshot and clears the readings without changing the session's state.
**Where**: `core/alignment/alignment.cpp` (modify, with `core/alignment/alignment.hpp`)
**Depends on**: T9
**Reuses**: `write_steps`, the question and generation path of T9, `FakePort`, `FakeMeasuring`
**Requirement**: ALIGN-31, ALIGN-33, ALIGN-34, ALIGN-35, ALIGN-36, ALIGN-37, ALIGN-38, ALIGN-39, ALIGN-40, ALIGN-75 (emitted)

**Done when**:

- [ ] `undo()` with no snapshot makes no port call (ALIGN-31).
- [ ] A snapshot source the fake has forgotten, or no delay filter available → no write call, UndoFailed, `undo.refused ...`, snapshot kept (ALIGN-33).
- [ ] The hand-set negative vector (s = −200,000,000 ns) applied then undone leaves the fake's offset at exactly −200,000,000 ns; a vector that created Cam B's filter, undone, leaves Cam B with no filter (ALIGN-36).
- [ ] A filter that existed before Apply at 120 ms disabled, deleted by hand after Apply, is created again at 120 ms disabled; a filter Apply created and the fake no longer finds (deleted or renamed) gives no step; a value changed by hand after Apply is written back to the snapshot's (ALIGN-36, ALIGN-37).
- [ ] Only differing values are written: an Apply that changed only Cam B and the mic gives an Undo of two steps (ALIGN-37).
- [ ] On air, `Confirm` gets the counts of the values Undo changes; a no writes nothing and keeps the snapshot (ALIGN-35).
- [ ] A failure at each write position leaves the post-Apply values, UndoFailed, the snapshot kept; a failed restore gives UndoRestoreFailed and drops it (ALIGN-38, ALIGN-39).
- [ ] On success: Undone with each snapshot camera's restored delay (0 for a removed filter) in Apply's order, then the mic's offset rounded to whole ms; the snapshot gone; `aligned_after_apply` cleared; `clear_readings()` and never `resume` (ALIGN-40). The lines use the `undo.` prefix (ALIGN-75).
- [ ] Added doctest cases: at least 8, in `tests/alignment/undo.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): undo the last apply`

---

#### T11: Keep the alignment's state for its lifetime

**What**: `reset()` and the lifetime rules of the design's state table, checked across sequences of Apply, Undo, Reset and collection switches.
**Where**: `core/alignment/alignment.cpp` (modify, with `core/alignment/alignment.hpp`)
**Depends on**: T10
**Reuses**: `FakePort`, `FakeMeasuring`
**Requirement**: ALIGN-32, ALIGN-57 (the result's lifetime), ALIGN-58 (the flag's lifetime)

**Done when**:

- [ ] `reset()` clears `aligned_after_apply` and the last result and keeps the Undo snapshot.
- [ ] Apply, Apply: the second snapshot replaces the first; Undo then restores the values from before the second Apply (ALIGN-32).
- [ ] Apply, Undo, Undo: the second Undo makes no port call.
- [ ] Apply, then a failed Apply with a clean restore: the first Apply's snapshot and `aligned_after_apply` stay.
- [ ] The last result after each of: AlreadyAligned, OverLimit, ApplyFailed, Applied, Undone is that result; after Cancelled and Waiting it is the one before (ALIGN-57).
- [ ] Added doctest cases: at least 5, in `tests/alignment/lifetime.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): keep undo and results until they expire`

---

### Phase 5 tasks: View-model

#### T12: Add the Alignment strings

**What**: The design's new locale keys in `strings.hpp` and `all_keys`, and their en-US text in `data/locale/en-US.ini`.
**Where**: `core/viewmodel/strings.hpp` (with `data/locale/en-US.ini`)
**Depends on**: None
**Reuses**: the locale test in `tests/viewmodel/strings.cpp`
**Requirement**: ALIGN-63

**Done when**:

- [ ] Keys and text: Apply, Undo, Cancel buttons; "✓ {status}"; "Waiting for {camera}"; "Already aligned"; "{camera} needs more delay than OBS allows"; "Apply failed - nothing changed"; "Apply failed - check the log"; "Undo failed - nothing changed"; "Undo failed - check the log"; "Applied - {values}"; "Undone - {values}"; "{name} {value} ms"; "Aligned"; the five question wordings; "Cameras whose delay changes pause until their new delay fills."
- [ ] `all_keys` lists every key, and the existing locale test passes with them (every key present in `en-US.ini`, every `en-US.ini` key but the dock title listed).
- [ ] The en-US text is byte-for-byte the spec's, including U+2713 in the check-mark key.
- [ ] Quick gate passes. Test count unchanged unless the locale test is parametrised per key.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): add the apply and undo strings`

---

#### T13: Enable Apply and Undo and say what Apply waits for

**What**: `DockView::apply_enabled` and `undo_enabled`, `build_view`'s defaulted `AlignmentStatus` parameter, and "Waiting for {camera}" at the bottom of the message line's precedence.
**Where**: `core/viewmodel/view-model.cpp` (with `core/viewmodel/view-model.hpp`)
**Depends on**: T12
**Reuses**: `precheck`, `camera_list`, `message_text`
**Requirement**: ALIGN-47, ALIGN-48, ALIGN-49

**Done when**:

- [ ] Mic chosen, two cameras ticked and Stable → Apply enabled; either Settling → disabled; no mic or no camera → disabled (ALIGN-47).
- [ ] Cameras "Brio" Stable, "cam b" Settling, "Cam C" Looking → "Waiting for cam b" (checklist order ignores case); with "No pattern sound" showing, that message shows instead (ALIGN-48).
- [ ] Undo enabled exactly when `undo_available` (ALIGN-49).
- [ ] Every existing Reading view-model test passes unchanged (the default parameter).
- [ ] Added doctest cases: at least 4, in `tests/viewmodel/view-model.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): enable apply and undo in the dock view`

---

#### T14: Show what Apply and Undo did

**What**: The message line for each Apply and Undo result, `format_value_ms`, and the precedence of ALIGN-60 for the result.
**Where**: `core/viewmodel/view-model.cpp` (modify, with `core/viewmodel/view-model.hpp`)
**Depends on**: T13
**Reuses**: `strings::fill`
**Requirement**: ALIGN-50, ALIGN-51, ALIGN-52, ALIGN-53, ALIGN-54, ALIGN-55, ALIGN-56, ALIGN-57, ALIGN-60

**Done when**:

- [ ] Exact text for AlreadyAligned, OverLimit naming "Cam B" and naming the mic "Shure MV7", ApplyFailed, ApplyRestoreFailed, UndoFailed, UndoRestoreFailed (ALIGN-50 to ALIGN-53, ALIGN-56).
- [ ] Applied with Cam A 0, Cam B 60, Shure MV7 100 → "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms" (ALIGN-54).
- [ ] Undone with a mic of −200 ms → "Undone - Cam A 0 ms, Cam B 0 ms, Shure MV7 −200 ms" with U+2212; `format_value_ms` gives "60", "0", "−200" (ALIGN-55).
- [ ] With no last result, nothing shows (ALIGN-57's lifetime is held by `Alignment`, T11).
- [ ] Precedence: Pick over the result; "Could not listen" over the result; "No pattern sound" over the result; the result over "Waiting for {camera}" (ALIGN-60; "Aligned" joins in T15).
- [ ] Added doctest cases: at least 6, in `tests/viewmodel/view-model.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): show the result of apply and undo`

---

#### T15: Mark aligned cameras and show "Aligned"

**What**: After a successful Apply, a ticked Stable camera within ±F gets "✓ " before its status, and "Aligned" shows once every ticked camera has it, between "No pattern sound" and the last result.
**Where**: `core/viewmodel/view-model.cpp` (modify)
**Depends on**: T14, T2
**Reuses**: `status_text`, `SessionSnapshot::frame_interval_ms`
**Requirement**: ALIGN-58, ALIGN-59, ALIGN-60 (Aligned's place)

**Done when**:

- [ ] With `aligned_after_apply`, F 33.333, Cam A Stable +4 → "✓ +4 ms - video late"; Cam B Stable 0 → "✓ 0 ms - in sync"; Cam C Stable +34 → no mark; Cam D Settling → no mark (ALIGN-58).
- [ ] Without `aligned_after_apply`, no row is marked even when every reading is within ±F.
- [ ] Every ticked camera marked → "Aligned"; one unmarked → the last result (or "Waiting for ...") instead (ALIGN-59).
- [ ] "No pattern sound" outranks "Aligned"; "Aligned" outranks an "Applied - ..." result (ALIGN-60).
- [ ] Added doctest cases: at least 4, in `tests/viewmodel/view-model.cpp`. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): mark aligned cameras in the dock view`

---

#### T16: Word the on-air question

**What**: `confirmation_question(counts, translate)` and `confirmation_detail(counts, translate)`.
**Where**: `core/viewmodel/view-model.cpp` (modify, with `core/viewmodel/view-model.hpp`)
**Depends on**: T12
**Reuses**: `strings::fill`
**Requirement**: ALIGN-61, ALIGN-62

**Done when**:

- [ ] {3, mic} → "Shift 3 cameras and the mic while on air?"; {1, mic} → "Shift 1 camera and the mic while on air?"; {2, no mic} → "Shift 2 cameras while on air?"; {1, no mic} → "Shift 1 camera while on air?"; {0, mic} → "Shift the mic while on air?" (ALIGN-61).
- [ ] The detail is "Cameras whose delay changes pause until their new delay fills." for any count with a camera, and empty for {0, mic} (ALIGN-62).
- [ ] Added doctest cases: at least 2, in `tests/viewmodel/view-model.cpp`. Quick gate passes. The core-only job's coverage gate holds at 90% on the push that ends phase 5.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(alignment): word the on-air question`

---

### Phase 6 tasks: OBS adapter, integration tests, controller and dock

#### T17: Run libobs headless in the Ubuntu job

**What**: A Linux-only doctest executable `camera-sync-integration-tests` that starts libobs on first use, registers OBS 30.0.2's `async_delay_filter` compiled from `.deps` and the design's test source and filter types, and runs under `xvfb-run` in the Ubuntu CI job.
**Where**: `tests/integration/CMakeLists.txt` (with `tests/integration/main.cpp`, `tests/integration/obs-env.hpp`, the root `CMakeLists.txt` hook and the Ubuntu job in `.github/workflows/build-project.yaml`)
**Depends on**: None
**Reuses**: `third_party/doctest`, `doctest_discover_tests`; the OBS sources the Ubuntu build downloads (`cmake/linux/buildspec.cmake`)
**Requirement**: ALIGN-77 (the harness), ALIGN-80

**Done when**:

- [ ] The root `CMakeLists.txt` adds `tests/integration` only on Linux plugin builds, after `find_package(libobs)`; the core preset and the Windows and macOS builds are unchanged.
- [ ] `async-delay-filter.c` is compiled from `.deps/obs-studio-30.0.2/plugins/obs-filters/` with warnings off; a stub `obs_module_text` returns its key; nothing from OBS is copied into the repository.
- [ ] libobs starts on the first test that needs it, not in `main`, so `--list-test-cases` at build time starts nothing; it shuts down after the run.
- [ ] A smoke case: libobs starts; `obs_source_get_display_name("async_delay_filter")` is not null; a test camera accepts a Video Delay (Async) filter and lists it.
- [ ] The Ubuntu job installs `xvfb` and runs its CTest step as `xvfb-run -a ctest ...`; the smoke case passes there.
- [ ] If libobs cannot start on the runner even under `xvfb-run`, the job is not left red: the case is skipped with the reason logged, and ALIGN-80's rig run is recorded as the plan (a note in this task).
- [ ] `CLAUDE.md`'s Tests section names the integration tests and how CI runs them. Every new file starts with the SPDX line (`check_notices_repository` passes).
- [ ] CI gate passes on all three platforms.

**Tests**: integration
**Gate**: ci

**Commit**: `:white_check_mark: test(alignment): run libobs headless in the ubuntu job`

---

#### T18: Implement the settings port over libobs

**What**: `ObsSettingsPort`: resolve, delay-filter availability, find the plugin filter in chain order by type and recognised name, read, write (creating at the end of the chain), remove, the Sync Offset in ns, and the injected on-air query.
**Where**: `src/obs/obs-settings-port.cpp` (with `src/obs/obs-settings-port.hpp`; its cases in `tests/integration/settings-port.cpp`)
**Depends on**: T17, T1
**Reuses**: the UUID lookup and release pattern of `src/controller.cpp:46-53`; `recognised_filter_names`
**Requirement**: ALIGN-41, ALIGN-42, ALIGN-43, ALIGN-44, ALIGN-45 (the injected query), ALIGN-46, ALIGN-77 (the port's operations), ALIGN-79

**Done when**:

- [ ] Write on a camera without a plugin filter creates `async_delay_filter` named "Camera Sync Delay" last in the chain with `delay_ms` and the enabled state; read returns them; write again updates in place; remove removes only it (ALIGN-44, ALIGN-77).
- [ ] A test camera carrying, in order, a user's Video Delay (Async) named "Delay", a test filter of another type named "Camera Sync Delay", and a `gpu_delay` stand-in: read finds no plugin filter; write creates one after all three; after write and remove the three are unchanged in settings, enabled state and order (ALIGN-42, ALIGN-43, ALIGN-79).
- [ ] Two plugin filters on one camera: the first in chain order is read and written (ALIGN-42).
- [ ] The Sync Offset is written and read back to the nanosecond, including −200,000,000 (ALIGN-44, ALIGN-77).
- [ ] A forgotten UUID: `resolves` false, reads return none, writes return false.
- [ ] `on_air()` returns whatever the injected function returns (ALIGN-45; the frontend query itself is wired in T20).
- [ ] The file is added to the plugin's sources and to the integration target. No other filter, property or source is touched by any operation (ALIGN-46's port half).
- [ ] Before the push, `src/obs/obs-settings-port.cpp` passes `g++ 13 -std=c++17 -fsyntax-only -Wall -Wextra -Werror` against OBS 30.0.2's headers in a scratch directory.
- [ ] CI gate passes on all three platforms, with at least 6 new integration cases passing in the Ubuntu job.

**Tests**: integration
**Gate**: ci

**Commit**: `:sparkles: feat(alignment): write the plugin filter and sync offset through libobs`

---

#### T19: Apply and Undo against real OBS objects

**What**: Integration cases that run `Alignment` over `ObsSettingsPort` against test sources, including a source removed between the reads and the writes.
**Where**: `tests/integration/alignment.cpp`
**Depends on**: T18, T11
**Reuses**: `Alignment`, `ObsSettingsPort`, a `SettingsPort` decorator that removes a source before a chosen write; a fake `MeasuringControl`
**Requirement**: ALIGN-77 (Apply and Undo), ALIGN-78, ALIGN-80

**Done when**:

- [ ] Vector 1 against two test cameras and a test mic: Cam B gets a plugin filter at 60 ms, the mic 100,000,000 ns, Cam A no filter; Undo returns all three to their starting state (ALIGN-77).
- [ ] Cam B removed by the decorator after the reads and before its write: Apply reports ApplyFailed, Cam A and the mic hold their starting values, and no plugin filter Apply created remains on any source (ALIGN-78).
- [ ] The user's filters of T18's camera are unchanged after the Apply and the Undo.
- [ ] CI gate passes, with the new cases passing in the Ubuntu job; under T17's skip, the rig run of ALIGN-80 is recorded in `validation.md` instead.

**Tests**: integration
**Gate**: ci

**Commit**: `:white_check_mark: test(alignment): apply and undo against libobs`

---

#### T20: Wire Apply and Undo into the controller

**What**: The controller owns an `ObsSettingsPort` (with the frontend's streaming and recording query) and an `Alignment`, adapts the session as `MeasuringControl`, builds the request, answers the question through the view sink, passes `alignment_.status()` to `build_view`, and tells `Alignment` about Reset and collection switches.
**Where**: `src/controller.cpp` (with `src/controller.hpp`)
**Depends on**: T18, T11, T16, T2
**Reuses**: `camera_list`, `obs_get_video_info`, `confirmation_question`, `confirmation_detail`; `obs_frontend_streaming_active`, `obs_frontend_recording_active`
**Requirement**: ALIGN-17 (wired), ALIGN-28 (wired), ALIGN-32 (collection switch wired), ALIGN-45 (the frontend query), ALIGN-57 and ALIGN-58 (Reset wired)

**Done when**:

- [ ] `apply()` and `undo()` build the request from the ticked cameras in `camera_list` order with their names, the mic, `session_.snapshot()` and the canvas rate, call `alignment_`, and refresh.
- [ ] The `Confirm` passed asks `sink_->confirm(question, detail)` and answers no when there is no sink.
- [ ] `on_air` is `obs_frontend_streaming_active() || obs_frontend_recording_active()`.
- [ ] `on_collection_changing()` calls `alignment_.collection_changing()`; `reset()` calls `alignment_.reset()`.
- [ ] `ViewSink` gains `confirm(question, detail) -> bool`. No rule is added here.
- [ ] CI gate passes on all three platforms. The rig check is T22 and T23.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(alignment): wire apply and undo into the controller`

---

#### T21: Add Apply and Undo to the dock

**What**: Apply and Undo buttons after Reset, enabled from the view, forwarding their clicks, and `confirm()` as a modal `QMessageBox` titled with the dock's title, with "Apply" and "Cancel" buttons.
**Where**: `src/camera-sync-dock.cpp`
**Depends on**: T20
**Reuses**: the existing button wiring and `text()` lookup in `src/camera-sync-dock.cpp`
**Requirement**: ALIGN-64

**Done when**:

- [ ] The buttons read "Apply" and "Undo" from the locale and follow `apply_enabled` and `undo_enabled`.
- [ ] `confirm()` shows the question and its detail line (none when empty), returns true only for "Apply", and Cancel is the default button.
- [ ] CI gate passes on all three platforms. The rig check is T22 and T23.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(alignment): add apply and undo to the dock`

---

### Phase 7 tasks: Rig

#### T22: Align the rig's cameras and diff the scene collection

**What**: Three measure-and-apply runs on the rig in OBS 30.0.0 and the latest stable, with scene-collection diffs before Apply, after Apply and after Undo, and the plugin removed afterwards.
**Where**: `.specs/features/alignment/validation.md` (the rig record)
**Depends on**: T21, T19
**Reuses**: the TDD's rig protocol, steps 1 to 4
**Requirement**: ALIGN-65, ALIGN-66, ALIGN-67, ALIGN-68, ALIGN-46 (the diff)

**Done when**:

- [ ] Two cameras with clearly different lags, one with a Render Delay: each run, after Apply and re-measuring, every camera reads within one frame; three runs in a row in each OBS version (ALIGN-65).
- [ ] The collection saved after Apply differs from the one before only in the plugin filters and the mic's sync offset; after Undo it equals the one before Apply (ALIGN-66, ALIGN-67, ALIGN-46).
- [ ] With the plugin removed and OBS restarted, the collection loads with the filters and the offset in place, and Sync Dock reads each camera within one frame on Program (ALIGN-68).
- [ ] The OBS versions, canvas resolution and frame rate, and devices are recorded.

**Tests**: none
**Gate**: rig

**Commit**: `:memo: docs(alignment): record the rig alignment runs`

---

#### T23: Walk every Alignment row on the rig

**What**: Interactive UAT of every row of the design's Alignment table except "a write fails partway", and Apply's duration with four cameras.
**Where**: `.specs/features/alignment/validation.md` (the UAT record)
**Depends on**: T22
**Reuses**: tlc-spec-driven's interactive UAT
**Requirement**: ALIGN-69, ALIGN-81

**Done when**:

- [ ] Each row shows the spec's text in the dock, including the on-air question while recording, "Waiting for {camera}", "Already aligned", the limit message (forced with a user filter of 20,000 ms), "Aligned" with check marks, Undo's restored values, and Undo disabled after a restart (ALIGN-69).
- [ ] With four cameras and the mic, `apply.result` shows `duration_ms` under 100 (ALIGN-81).
- [ ] Every issue found becomes a fix task with its severity inferred, as the skill's UAT says.

**Tests**: none
**Gate**: rig

**Commit**: `:memo: docs(alignment): record the rig walkthrough of every alignment row`

---

## Phase Execution Map

```
Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6 → Phase 7

Phase 1:  T1, T2
Phase 2:  T3 → T4
Phase 3:  T5, T6
Phase 4:  T7 → T8 → T9 → T10 → T11
Phase 5:  T12 → T13 → T14 → T15
          T12 → T16
Phase 6:  T17 → T18 → T19
          T18 → T20 → T21
Phase 7:  T22 → T23
```

Execution is strictly sequential, in task-number order. The 23 tasks pack into about four batches of whole phases: phases 1 to 3 (6 tasks), phase 4 (5), phase 5 (5), phase 6 (5), and phase 7 (2) on the rig, which is not delegated.

---

## Task Granularity Check

`validate_tasks.py` passes with 0 errors. It warns on every `Tests: none` (T20 to T23); each matches a matrix layer whose test is the CI build or the rig. It also warns wherever a task names several files; each such task is one deliverable with its header, or a test harness with its build hook.

| Task | Scope | Status |
| ---- | ----- | ------ |
| T1: Types and port interface | Two headers of declarations | ⚠️ Cohesive: the port's signatures use the types |
| T2: Session hooks | One method and one snapshot field | ✅ Granular |
| T3: precheck | One function | ✅ Granular |
| T4: compute_plan | One function | ✅ Granular |
| T5: write_steps | One function, plus the fake port its tests need | ✅ Granular |
| T6: Log lines | A family of formatting functions in one file | ⚠️ Cohesive: one format, two prefixes |
| T7: Apply refusals | One method's first half | ✅ Granular |
| T8: Apply writes | One method's second half | ✅ Granular |
| T9: On-air question | One branch of one method, plus `collection_changing` | ✅ Granular |
| T10: Undo | One method | ✅ Granular |
| T11: State lifetimes | One method (`reset`) and sequence tests | ✅ Granular |
| T12: Strings | One key list and its locale file | ✅ Granular |
| T13: Button states | Two fields and one message | ✅ Granular |
| T14: Result messages | One message family | ✅ Granular |
| T15: Check marks | One row rule and one message | ✅ Granular |
| T16: Question text | Two functions | ✅ Granular |
| T17: Integration harness | One test executable, its build hook and the CI step that runs it | ⚠️ Cohesive: the harness cannot run without the hook and the step |
| T18: ObsSettingsPort | One class | ✅ Granular |
| T19: Integration Apply and Undo | One test file | ✅ Granular |
| T20: Controller wiring | One class's changes | ✅ Granular |
| T21: Dock buttons | One widget's changes | ✅ Granular |
| T22: Rig runs | One record | ✅ Granular |
| T23: Rig walkthrough | One record | ✅ Granular |

---

## Diagram-Definition Cross-Check

| Task | Depends On (task body) | Diagram Shows | Status |
| ---- | ---------------------- | ------------- | ------ |
| T1 | None | Phase 1 start | ✅ Match |
| T2 | None | Phase 1 start | ✅ Match |
| T3 | T1 (phase 1) | Phase 2 start | ✅ Match |
| T4 | T3 | T3 → T4 | ✅ Match |
| T5 | T1 (phase 1) | Phase 3 start | ✅ Match |
| T6 | T1 (phase 1) | Phase 3 start | ✅ Match |
| T7 | T4 (phase 2), T5, T6 (phase 3) | Phase 4 start | ✅ Match |
| T8 | T7 | T7 → T8 | ✅ Match |
| T9 | T8 | T8 → T9 | ✅ Match |
| T10 | T9 | T9 → T10 | ✅ Match |
| T11 | T10 | T10 → T11 | ✅ Match |
| T12 | None | Phase 5 start | ✅ Match |
| T13 | T12 | T12 → T13 | ✅ Match |
| T14 | T13 | T13 → T14 | ✅ Match |
| T15 | T14, T2 (phase 1) | T14 → T15 | ✅ Match |
| T16 | T12 | T12 → T16 | ✅ Match |
| T17 | None | Phase 6 start | ✅ Match |
| T18 | T17, T1 (phase 1) | T17 → T18 | ✅ Match |
| T19 | T18, T11 (phase 4) | T18 → T19 | ✅ Match |
| T20 | T18, T11 (phase 4), T16 (phase 5), T2 (phase 1) | T18 → T20 | ✅ Match |
| T21 | T20 | T20 → T21 | ✅ Match |
| T22 | T21, T19 | Phase 7 start | ✅ Match |
| T23 | T22 | T22 → T23 | ✅ Match |

No task depends on a later phase.

---

## Test Co-location Validation

| Task | Code Layer Created/Modified | Matrix Requires | Task Says | Status |
| ---- | --------------------------- | --------------- | --------- | ------ |
| T1 | Core logic (`core/alignment/`) | unit | unit | ✅ OK |
| T2 | Core logic (session hooks) | unit | unit | ✅ OK |
| T3 | Core logic | unit | unit | ✅ OK |
| T4 | Core logic | unit | unit | ✅ OK |
| T5 | Core logic | unit | unit | ✅ OK |
| T6 | Core logic (`core/log-lines.*`) | unit | unit | ✅ OK |
| T7 | Core logic | unit | unit | ✅ OK |
| T8 | Core logic | unit | unit | ✅ OK |
| T9 | Core logic | unit | unit | ✅ OK |
| T10 | Core logic | unit | unit | ✅ OK |
| T11 | Core logic | unit | unit | ✅ OK |
| T12 | Core logic (`core/viewmodel/`) | unit | unit | ✅ OK |
| T13 | Core logic | unit | unit | ✅ OK |
| T14 | Core logic | unit | unit | ✅ OK |
| T15 | Core logic | unit | unit | ✅ OK |
| T16 | Core logic | unit | unit | ✅ OK |
| T17 | Integration harness, CMake wiring, CI workflow | integration (highest of integration, build) | integration | ✅ OK |
| T18 | Settings port's OBS implementation | integration | integration | ✅ OK |
| T19 | Integration tests | integration | integration | ✅ OK |
| T20 | Plugin adapters (controller) | build + UAT | none (ci, then rig in T22, T23) | ✅ OK |
| T21 | Dock | build + UAT | none (ci, then rig in T22, T23) | ✅ OK |
| T22 | Rig record | build + UAT (rig) | none (rig) | ✅ OK |
| T23 | Rig record | build + UAT (rig) | none (rig) | ✅ OK |

---

## Requirement Coverage

| Requirement | Tasks |
| ----------- | ----- |
| ALIGN-01, ALIGN-02 | T3 |
| ALIGN-03 | T1, T4 |
| ALIGN-04 to ALIGN-10 | T4 |
| ALIGN-11 to ALIGN-16 | T7 |
| ALIGN-17 | T9, T20 |
| ALIGN-18, ALIGN-19 | T9 |
| ALIGN-20 to ALIGN-23 | T8 |
| ALIGN-24 | T5 |
| ALIGN-25, ALIGN-26 | T5, T8 |
| ALIGN-27, ALIGN-29, ALIGN-30 | T8 |
| ALIGN-28 | T2, T8, T20 |
| ALIGN-31, ALIGN-33 to ALIGN-37, ALIGN-40 | T10 |
| ALIGN-32 | T9, T11, T20 |
| ALIGN-38, ALIGN-39 | T5, T10 |
| ALIGN-41 | T1, T18 |
| ALIGN-42 | T1, T18 |
| ALIGN-43, ALIGN-44 | T18 |
| ALIGN-45 | T18, T20 |
| ALIGN-46 | T18, T22 |
| ALIGN-47 to ALIGN-49 | T13 |
| ALIGN-50 to ALIGN-56 | T14 |
| ALIGN-57 | T11, T14, T20 |
| ALIGN-58 | T2, T11, T15, T20 |
| ALIGN-59 | T15 |
| ALIGN-60 | T14, T15 |
| ALIGN-61, ALIGN-62 | T16 |
| ALIGN-63 | T12 |
| ALIGN-64 | T21 |
| ALIGN-65 to ALIGN-68 | T22 |
| ALIGN-69, ALIGN-81 | T23 |
| ALIGN-70 to ALIGN-73 | T6, T8 |
| ALIGN-74 | T6, T7, T9 |
| ALIGN-75 | T6, T10 |
| ALIGN-76 | T6 |
| ALIGN-77 | T17, T18, T19 |
| ALIGN-78 | T19 |
| ALIGN-79 | T18 |
| ALIGN-80 | T17, T19 |

**Coverage:** 81 requirements, 81 mapped to tasks, 0 unmapped.
