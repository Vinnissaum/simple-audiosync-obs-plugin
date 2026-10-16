# Reading Tasks

## Execution Protocol (MANDATORY -- do not skip)

Implement these tasks with the `tlc-spec-driven` skill: **activate it by name and follow its Execute flow and Critical Rules.** Do not search for skill files by filesystem path. The skill is the source of truth for the full flow (per-task cycle, sub-agent delegation, adequacy review, Verifier, discrimination sensor).

**If the skill cannot be activated, STOP and tell the user - do not proceed without it.**

---

**Spec**: `.specs/features/reading/spec.md` (READ-01 to READ-98)
**Design**: `.specs/features/reading/design.md`
**Status**: In progress. T1 to T33 done and pushed; T34 to T37 wait for the rig.

**Prerequisites before Execute:**

- `export PATH=$HOME/.local/bin:$PATH` in every shell: CMake and Ninja live there on the WSL machine. gcc 13 is installed.
- T9 only: network access, Python 3 with Pillow and qrcode (Sync Dock's `tool/requirements.txt`), and `git`, to run `tests/fixtures/generate.sh`. The WSL machine could not reach the network on 2026-10-15. If T9 cannot run, T9 to T11 wait; every other task in phases 1, 2, 4 and 6 does not depend on them.
- Every `ci` gate: push access, and either `gh` authenticated or the maintainer reporting each run, as in Foundation.
- T24 and T26: nothing local. `gcovr` and Clang are installed by the CI job. No tool is added to the WSL machine.
- Phase 9 (T34 to T37): the rig, with OBS 30.0.0 and the latest stable, two cameras of different lags (one with a Render Delay), a mic, a phone, and Sync Dock installed.

**Commit messages** follow `CLAUDE.md` (gitmoji shortcode, Conventional Commits, scope `reading`). The skill's `check_commit.py` does not know the gitmoji prefix, so it runs on the message with the leading `:shortcode: ` removed.

**Execution order is task-number order.** Dependencies on earlier phases are listed in each task's `Depends on` and drawn only in the cross-check table.

**Tools for every task:** MCP none, Skill none. Ask before changing that.

---

## Test Coverage Matrix

> Generated from the repository's tests and guidelines. Guidelines found: `CLAUDE.md` (Tests, Build), `docs/tdd/multi-camera-sync.md` (Testing Strategy: 90% core line coverage, a fuzz test for the QR parser, headless integration tests for Alignment), `CMakePresets.json` (the `core` preset), `.github/workflows/build-project.yaml` (the `core-build` job). Samples read: `tests/core/load-line.cpp`, `tests/detector/{audio-detector,video-detector,quirc}.cpp`, `tests/cmake/*.cmake`.

| Code Layer | Required Test Type | Coverage Expectation | Location Pattern | Run Command |
| ---------- | ------------------ | -------------------- | ---------------- | ----------- |
| Core logic (`core/reading/`, `core/selection/`, `core/viewmodel/`, `core/log-lines.*`) | unit (doctest through CTest) | Every spec AC the task carries, 1:1, with exact expected values and strings; every listed edge case; a boundary on each side of every threshold (F, half a wrap, 256, five samples, 2 s, 5 s); at least 90% of lines across `core/` excluding `core/detector/` (gate in T24) | `tests/reading/*.cpp`, `tests/selection/*.cpp`, `tests/viewmodel/*.cpp`, `tests/core/*.cpp` | quick gate |
| Vendored detector (`core/detector/`) | unit | Fixes assert the changed behavior; fixture tests assert indices exactly and marker times within 1 ms | `tests/detector/*.cpp` | quick gate |
| Test fixtures (`tests/fixtures/`) | unit (the test that reads them) | Regenerable with the committed script; small (under 1 MB added) | `tests/fixtures/` | quick gate |
| Fuzz harness (`tests/fuzz/`) | unit (corpus replay in CTest) plus a timed libFuzzer run in CI | Boundary corpus that fails if any range check is removed | `tests/fuzz/` | quick gate, then ci gate |
| Plugin adapters, controller and dock (`src/`) | build + UAT | Builds and packages on three platforms; rules stay in the core; every behavior walked on the rig in phase 9 | - | ci gate, then rig gate |
| Build spec, presets, CI workflows | build | The named CI jobs pass; a negative check wherever an AC names a failure | - | ci gate |

## Gate Check Commands

> Generated from the `core` preset and the existing CI. Confirm before Execute.

| Gate Level | When to Use | Command |
| ---------- | ----------- | ------- |
| Quick | Tasks that change the core, its tests or fixtures | `export PATH=$HOME/.local/bin:$PATH && cmake --preset core && cmake --build --preset core && ctest --preset core --output-on-failure` |
| CI | Tasks that change `src/`, the presets, CMake wiring or the workflows | Run the quick gate, push the task's commit, and wait: Check Formatting, Build for Windows, Build for macOS, Build for Ubuntu and the core-only job all pass. After T24 the core-only job also enforces 90% coverage; after T26 it also runs the 60 s fuzz |
| Rig | T34 to T37 | Install the CI packages into OBS 30.0.0 and the latest stable on the rig and walk the task's requirements through interactive UAT |

Every task keeps the earlier gates green. A new test count never drops: record the `ctest --preset core -N` total before and after.

---

## Execution Plan

Phases run in order, and tasks in a phase run in order.

### Phase 1: Detector fix and shared vocabulary

```
T1
T2
```

### Phase 2: Reading estimator

```
T3 → T4 → T5 → T6
```

### Phase 3: Pipelines and fixtures

```
T7
T8
T7 → T9 → T10 → T11
T8 → T10
```

### Phase 4: Selection rules and log lines

```
T12
T13
```

### Phase 5: Session

```
T14 → T15 → T16 → T17 → T18 → T19
```

### Phase 6: View-model

```
T20 → T21 → T22 → T23
```

### Phase 7: CI quality gates

```
T24 → T26
T25 → T26
```

### Phase 8: Plugin adapters, controller and dock

```
T27 → T32
T28 → T32
T29 → T31
T30 → T31
T31 → T32 → T33
```

### Phase 9: Rig

```
T34 → T35 → T36
T35 → T37
```

---

## Task Breakdown

### Phase 1 tasks: Detector fix and shared vocabulary

#### T1: Make QR payload decoding transactional and quiet

**What**: `QrPayload::decode` parses into a local copy and assigns the object only when the range check passes, and its four range-check messages log at Debug.
**Where**: `core/detector/qr-payload.hpp`
**Depends on**: None (Foundation is done)
**Reuses**: the existing `QrPayload`, `Logger`, `LogLevel::Debug`; the notice and provenance lines stay
**Requirement**: READ-23

**Done when**:

- [x] A payload with `f=9`, `f=32001`, `c=0`, `c` above `f`, `q=0`, `q=1001`, `i=256` or no `i` returns false and leaves every field of an already-valid `QrPayload`, including `valid`, unchanged. The fresh copy is default-constructed, not a copy of the current payload, so a payload with no `i` keeps the default `i` and fails its range check.
- [x] A payload with a malformed key (`ff=1`, `=1`) returns false with no state change. It also logs `payload: malformed parameter` at Debug, without the payload's text (READ-23, READ-89).
- [x] A valid payload still decodes to the same values as before (`tests/detector/video-detector.cpp` existing cases pass).
- [x] The existing "rejects a value out of range and logs which one" case now expects `LogLevel::Debug`, and a new case asserts a rejected payload logs nothing at Warning.
- [x] Added doctest cases: "qr payload keeps a valid payload when a later one is rejected" (one section per rejected value above), "qr payload logs a rejected value at debug level". No existing case is deleted or loosened. Follow-up (Verifier gap 1, mutant M51): two detector cases show `qr-frame-f9.pgm`, a QR frame with f=9 that `generate.py` now writes, followed by a flash. Neither a fresh detector nor one past a valid cycle's window emits a marker. In the second case only the `continue` after a rejected decode keeps the kept index from opening a new window. Test count: 160.
- [x] Quick gate passes. Test count: ran 26, which is 24 plus 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:bug: fix(reading): keep the last valid qr payload when a later one is rejected`

---

#### T2: Add the shared types and the pattern arithmetic

**What**: The core vocabulary (`SourceId`, `CameraState`) and the pattern functions: `PatternParams` with its half-wrap length, `round_half_away` and `frame_interval_ms`.
**Where**: `core/reading/pattern.cpp` (with `core/reading/pattern.hpp` and `core/reading/types.hpp`)
**Depends on**: None
**Reuses**: `QrPayload` from `core/detector/qr-payload.hpp`; the SPDX header rule
**Requirement**: READ-01, READ-12, READ-16 (the functions; their callers arrive in T3, T4 and T14)

**Done when**:

- [x] `PatternParams::half_wrap_ns()` equals `index_max × 3 × q_ms × 1,000,000 ÷ 2`: 25,344,000,000 ns for q_ms 66 and index_max 256, and 19,200,000,000 ns for q_ms 100 and index_max 128. The spec's "25.6 s" uses the unrounded q of 66.7 ms; the test uses the integer `q_ms` the payload carries.
- [x] `PatternParams::from(QrPayload)` copies f, c, q_ms and index_max, and `==` compares all four.
- [x] `round_half_away`: 141.5 → 142, −141.5 → −142, 141.49 → 141, 0.5 → 1, −0.5 → −1, 0 → 0, −0.49 → 0.
- [x] `frame_interval_ms`: (30, 1) → 33.333…, (60, 1) → 16.667…, (60000, 1001) → 16.683, (30000, 1001) → 33.367, each within 0.001.
- [x] `tests/CMakeLists.txt` and `core/CMakeLists.txt` list the new files. Added doctest cases: 4, in `tests/reading/pattern.cpp`.
- [x] Quick gate passes. Test count: ran 30, which is 26 plus 4.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): add the pattern arithmetic and shared types`

---

### Phase 2 tasks: Reading estimator

#### T3: Pair markers into samples and keep each camera's window

**What**: `ReadingEstimator` forms samples from video and audio markers, keeps each camera's five most recent samples, and reports Looking or Settling. Stability arrives in T4; until then a full window stays Settling.
**Where**: `core/reading/estimator.cpp` (with `core/reading/estimator.hpp`)
**Depends on**: T2
**Reuses**: `PatternParams`, `round_half_away`, `CameraState`
**Requirement**: READ-01, READ-02, READ-03, READ-04, READ-05, READ-06, READ-09, READ-10, READ-11 (the fewer-than-five part)

**Done when**:

- [x] Interfaces as in the design: `set_pattern`, `add_camera`, `remove_camera`, `clear_camera`, `clear`, `on_video_marker`, `on_audio_marker`, `snapshot`. Not thread-safe yet (T5). The two marker entry points return nothing until T4 gives them its stable-change results.
- [x] Video at 1.142 s and audio at 1.000 s with the same index give a sample of +142; the sign flips with the order of the times (READ-01).
- [x] Different indexes form no sample; times exactly half a wrap apart form none, one nanosecond under form one (READ-02).
- [x] Two audio markers with the video marker's index: the one nearest in time pairs (READ-03).
- [x] Audio before video and video before audio give the same sample (READ-04).
- [x] Two cameras fed the same audio get separate windows; removing one leaves the other's reading (READ-05). An audio marker stays in its list after it pairs and pairs once with each camera; the design's "a paired marker leaves its list" holds for video markers only (a `SPEC_DEVIATION` note in `estimator.hpp`), and a repeated video marker forms no second sample.
- [x] 257 unpaired video markers on one camera drop the oldest; 257 audio markers drop the oldest; a video marker whose audio was dropped forms no sample (READ-06).
- [x] No sample → Looking; first sample → Settling with `live_ms` rounded; four samples → Settling; a sixth sample older than all five in the window is dropped; a newer one pushes out the oldest by video time (READ-09, READ-10, READ-11). `live_ms` is the newest sample by video time, so arrival order never changes it.
- [x] `clear()` returns every camera to Looking; `clear_camera` affects one; both leave cameras registered. A marker for an unregistered camera is ignored.
- [x] Added doctest cases: at least 10, in `tests/reading/estimator.cpp`. Quick gate passes. Test count: ran 45, which is 30 plus 15.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): pair video and audio markers into samples`

---

#### T4: Decide stability and the reading

**What**: A full window within one frame is Stable, with the median rounded as the reading; a Stable camera is never demoted.
**Where**: `core/reading/estimator.cpp` (modify)
**Depends on**: T3
**Reuses**: `round_half_away`, `frame_interval_ms`
**Requirement**: READ-11, READ-12, READ-13, READ-14, READ-15, READ-16 (the estimator's input)

**Done when**:

- [x] `set_frame_interval_ms(double)` exists; with F = 33.0 a window spanning 33.0 is Stable and one spanning 33.01 is Settling (the boundary is inclusive, READ-12). F is 0 until it is set, so nothing turns Stable before the session sets it at Start.
- [x] Five samples within F make the camera Stable; the reading is the median rounded half away from zero: medians 141.5 → 142 and −141.5 → −142.
- [x] A Stable camera whose next five samples span more than F stays Stable with the old reading; one that gets no more samples stays Stable (READ-13, READ-14).
- [x] A later window within F replaces the reading with its median (READ-15).
- [x] A Settling camera that has never been stable shows `live_ms` as the latest sample rounded (READ-11).
- [x] `on_video_marker` and `on_audio_marker` return which cameras became Stable or changed reading, with the window spread (maximum minus minimum, rounded) for the `reading.stable` log line in T19. `on_video_marker` returns a `SampleResult` (`formed_sample`, `stable_changed`, `reading_ms`, `spread_ms`; the design's struct plus the reading the log line needs), and `on_audio_marker` a `std::vector<StableChange>` (camera, `reading_ms`, `spread_ms`).
- [x] Added doctest cases: at least 8, covering each bullet. Quick gate passes. Test count: ran 54, which is 45 plus 9.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): decide when a camera's reading is stable`

---

#### T5: Make the estimator safe for several threads

**What**: One mutex guards the estimator, and a snapshot is copied under it.
**Where**: `core/reading/estimator.cpp` (modify)
**Depends on**: T4
**Reuses**: `std::mutex`; the Foundation test layout
**Requirement**: READ-07, READ-08

**Done when**:

- [x] Four camera threads push 1,000 video markers each while one audio thread pushes the matching audio markers, in lock-step batches of 100 so no list reaches its cap; at the end every camera's total formed samples is 1,000 and its reading equals the injected offset (READ-07). The estimator exposes a cumulative `samples_formed` per camera for this. No video list reaches its cap. The audio list keeps paired markers (T3), so it fills to 256 from the third batch on, but it drops only markers from two batches back, which no camera still needs.
- [x] While four writers run, 10,000 snapshots each satisfy: Looking ⇒ 0 samples in the window; Stable ⇒ 5 samples and a reading; no row mixes a state with another instant's count (READ-08). Each writer drives one camera through Looking, Settling and Stable. The reader takes at least 10,000 snapshots, keeps going until it has seen all three states, and also checks Settling ⇒ 1 to 5 samples and samples ≤ `samples_formed`.
- [x] The two tests also pass once built with `-fsanitize=thread` with g++ 13 on WSL (a one-time local run, recorded in the commit body).
- [x] Added doctest cases: 2. Quick gate passes. Test count: ran 56, which is 54 plus 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): guard the estimator for marker threads`

---

#### T6: Count markers received and missed per camera

**What**: The estimator counts each camera's video markers received and the indexes skipped between consecutive ones.
**Where**: `core/reading/estimator.cpp` (modify)
**Depends on**: T5
**Reuses**: the pattern's `index_max`
**Requirement**: READ-98

**Done when**:

- [x] Indexes 3, 4, 7 give received 3, missed 2.
- [x] 254, 255, 0, 1 give missed 0 (wrap); 255 then 1 gives missed 1.
- [x] A repeated index adds to received and not to missed.
- [x] With `index_max` 16, indexes 14 then 1 give missed 2.
- [x] `clear()` and `clear_camera` zero the counters; `reset_counters()` zeroes them and keeps readings (for Start, used in T14). Each also forgets the last index, so the first marker afterwards counts no gap.
- [x] The snapshot carries `received` and `missed`. Added doctest cases: 4. Quick gate passes. Test count: ran 60, which is 56 plus 4.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): count the markers each camera received and missed`

---

### Phase 3 tasks: Pipelines and fixtures

#### T7: Add the camera pipeline

**What**: `CameraPipeline` wraps one `VideoDetector`, takes frames through `FrameSink`, restarts the detector on a size or format change and stamps the time of its last frame.
**Where**: `core/reading/camera-pipeline.cpp` (with `core/reading/camera-pipeline.hpp`)
**Depends on**: T1
**Reuses**: `VideoDetector`, `VideoFrame`, `LumaFormat`, the `Clock` type declared here and reused by T14
**Requirement**: READ-57 (the frame stamp), READ-69 (one pipeline per camera)

**Done when**:

- [x] `FrameSink::on_frame(const VideoFrame &, width, height, LumaFormat)` is the only entry; the first frame calls `detector.start`. `Clock` and `FrameSink` are declared in `camera-pipeline.hpp`; `qr_data()` and `last_frame_ns()` only read.
- [x] Feeding `tests/fixtures/qr-frame.pgm` makes the detector's `qr_data()` valid with index 37; feeding it again after a frame of another size restarts the detector without a crash and still decodes. A format change (Luma8 to Green32) restarts it too.
- [x] A frame with width 0 or a null pointer is dropped with a Warning, and `last_frame_ns` does not move. A height of 0 is dropped the same way. The spec gives no text; the line names the width and height.
- [x] `last_frame_ns()` starts at the clock reading at construction and moves to the clock reading on each accepted frame (a fake clock).
- [x] Two pipelines built together share no state: a frame to one leaves the other's `qr_data()` invalid.
- [x] Added doctest cases: 5, in `tests/reading/camera-pipeline.cpp`. Quick gate passes. Test count: ran 65, which is 60 plus 5.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): add the per-camera detection pipeline`

---

#### T8: Add the mic pipeline

**What**: `MicPipeline` wraps the `AudioDetector` behind `AudioSink`, starts it at the mic's sample rate and passes the pattern through.
**Where**: `core/reading/mic-pipeline.cpp` (with `core/reading/mic-pipeline.hpp`)
**Depends on**: None
**Reuses**: `AudioDetector`, `AudioBlock`, the audio fixture `audio-48k-mono-s16le.pcm` and its reader in `tests/detector/audio-detector.cpp`
**Requirement**: READ-19

**Done when**:

- [x] The 2 s fixture fed before `set_pattern` yields no marker (READ-19); fed after `set_pattern(884, 2, 66)` it yields the 10 indices 0 to 9 in order. `AudioSink` is declared in `mic-pipeline.hpp`.
- [x] `set_pattern` called from a second thread while blocks are processed does not race (a thread test, run once under `-fsanitize=thread`). The run is recorded in the commit body.
- [x] A stereo block (two planes) decodes the same as the mono block of the same samples: the same indices at the same times.
- [x] The fixture reader moves from `tests/detector/audio-detector.cpp` into a shared `tests/support/fixtures.hpp` helper; the existing detector tests still pass unchanged in meaning. `tests/detector/fixtures.hpp` moved there too, so there is one helper, and `tests/` is on the test include path.
- [x] Added doctest cases: 4, in `tests/reading/mic-pipeline.cpp`. Quick gate passes. Test count: ran 69, which is 65 plus 4.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): add the mic detection pipeline`

---

#### T9: Generate the pattern's video cycles and time its markers

**What**: Extend the fixture generator to write the frames of marker cycles 0 to 3, assemble them into a timeline in tests, and check the video decoder's markers.
**Where**: `tests/fixtures/generate.py` (modify), plus the new files under `tests/fixtures/video-cycles/` and `tests/support/pattern-timeline.hpp`
**Depends on**: T7
**Reuses**: Sync Dock's `tool/videogen.py` at `e4ad294` through the existing `generate.sh`; `CameraPipeline`
**Requirement**: READ-20

**Done when**:

- [x] `generate.py` writes, for cycles 0 to 3 at q=4, f=884, c=2, 60 fps, 320×180, every distinct frame once as a PGM (deduplicated by content) and one text file listing the frame sequence by name. The added files are under 1 MB in total. `generate.sh` and the script's header describe them. Six distinct frames (`video-cycles/frame-00.pgm` to `frame-05.pgm`) and `sequence.txt` with 48 names: 346,314 bytes. The frames carry the shipped pattern's wrap `I=256`.
- [x] `tests/support/pattern-timeline.hpp` rebuilds the timestamped frame sequence from those files at 60 fps with a start offset argument. It also plays the sequence into a `FrameSink`.
- [x] Feeding the 4 cycles through a `CameraPipeline` gives 4 video markers with indices 0, 1, 2, 3 in order, and consecutive marker times 200 ms ± 1 ms apart (READ-20: the generated cycle is 3q frames). Each marker is also within 1 ms of its generated time, the start of frame 2q of its cycle (measured: 0 ms off).
- [x] Added doctest cases: 2, in `tests/detector/video-cycles.cpp`. Quick gate passes. Test count: ran 71, which is 69 plus 2.
- [x] **If the generator cannot run on this machine** (no network, Pillow or qrcode), stop at this task, record why in `tasks.md`, and continue with phases 4 to 8 while T9 to T11 wait. It ran on 2026-10-16: the network was up, and Pillow 12.3.0 and qrcode 8.2 came from PyPI wheels in a scratch virtualenv outside the repository. With them the three existing fixtures regenerate byte for byte.

**Tests**: unit
**Gate**: quick

**Commit**: `:white_check_mark: test(reading): cover the pattern's video cycles with generated frames`

---

#### T10: Check the video and audio fixtures are in sync

**What**: A test that feeds the T9 video cycles and the audio fixture, started at the same time, and asserts each index's two marker times agree.
**Where**: `tests/detector/pattern-sync.cpp`
**Depends on**: T8, T9
**Reuses**: `CameraPipeline`, `MicPipeline`, `pattern-timeline.hpp`
**Requirement**: READ-21

**Done when**:

- [x] For indices 0 to 3, the video marker time and the audio marker time differ by at most 1 ms (READ-21: marker times within 1 ms of the generated time, anchored on the generator's sync). The mic pipeline takes the pattern from the first video marker's payload, as the session will. Follow-up (Verifier, READ-21 anchor): a second case checks every audio marker of the clip, indices 0 to 9, for its generated index and against its generated time, the start of frame 2q of its cycle. Measured: each is +0.054 ms off. Test count: 162.
- [x] **If any index differs by more than 1 ms**, do not widen the bound. Record the measured bias per index in this file and stop: READ-20, READ-21 and READ-22 need a user decision (amend the spec, or fix the detector). Measured video minus audio: −0.054 ms at each of indices 0, 1, 2 and 3 (video 133.333 ms, audio 133.388 ms into each cycle), so no index is over the bound.
- [x] Added doctest cases: 1. Quick gate passes. Test count: ran 72, which is 71 plus 1.

**Tests**: unit
**Gate**: quick

**Commit**: `:white_check_mark: test(reading): check the fixtures' video and audio markers agree to 1 ms`

---

#### T11: Read two cameras' offsets through the estimator

**What**: An end-to-end unit test: two cameras whose video trails the shared audio by 100 ms and 40 ms, through two `CameraPipeline`s, the `MicPipeline` and the estimator.
**Where**: `tests/reading/estimator-replay.cpp`
**Depends on**: T6, T10
**Reuses**: `pattern-timeline.hpp` with its start offset, `ReadingEstimator`, both pipelines
**Requirement**: READ-22

**Done when**:

- [x] Camera A's timeline is offset +100 ms and camera B's +40 ms against the same audio; after the 4 cycles, A's `live_ms` is 100 ± 1 and B's is 40 ± 1 (READ-22), each camera has 4 samples and is Settling, and neither window holds the other's samples. Measured: A 100 and B 40 (samples of 99.946 and 39.946 ms). The test plays the session's part by hand: the first video marker's payload sets the pattern of the estimator and the mic. Follow-up (Verifier, READ-22 anchor): the case also checks each of the four samples as it forms, not only the latest. Measured: A 100 and B 40 each time.
- [x] A third run with a camera that never sees the video leaves it Looking while A and B keep their samples. That camera's pipeline gets grey frames at the same times.
- [x] Added doctest cases: 2. Quick gate passes. Test count: ran 74, which is 72 plus 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:white_check_mark: test(reading): read two cameras' offsets from the pattern fixtures`

---

### Phase 4 tasks: Selection rules and log lines

#### T12: Add the selection rules

**What**: The catalog's rules as pure functions: the sorted lists, the initial choice and the saved-selection mapping.
**Where**: `core/selection/selection-policy.cpp` (with `core/selection/selection-policy.hpp`)
**Depends on**: None
**Reuses**: `SourceId`
**Requirement**: READ-49 (sorting), READ-50, READ-51, READ-52, READ-53, READ-54 (the struct), READ-60

**Done when**:

- [x] `camera_list` returns async-video sources and `mic_list` audio sources, each sorted by name ignoring case ("cam b" before "Cam C") (READ-49). Case is folded for ASCII letters only; names equal but for case fall back to the exact name, then the UUID.
- [x] With no saved selection, every async-video source with `in_program` is ticked, including the ones the adapter marked from nested scenes and groups (the adapter sets the flag; this test sets it) (READ-50).
- [x] With no saved selection, the mic is chosen only if exactly one source has audio; zero or two leave it unset; a camera with its own audio counts toward the two (READ-51). When that one source is a camera the same call ticks, the camera wins and the mic stays unset, so the two lists stay disjoint (READ-60). The spec does not say which wins; this needs the maintainer's confirmation.
- [x] A saved selection of version 1 ticks only the UUIDs that exist, preselects nothing, and a version 1 selection with no surviving UUID still suppresses preselection (READ-52).
- [x] Version 0, 2 and an absent selection all preselect (READ-53).
- [x] `mic_list` leaves out ticked cameras and `camera_list` leaves out the chosen mic (READ-60).
- [x] `to_saved(Choice)` returns version 1 with the camera UUIDs and the mic or null (READ-54).
- [x] Added doctest cases: at least 9, in `tests/selection/selection-policy.cpp`. Quick gate passes. Added 11. Test count: ran 85, which is 74 plus 11.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): add the camera and mic selection rules`

---

#### T13: Add the log line formats

**What**: Pure formatters for `measure.start`, `measure.stop`, `measure.reset` and `reading.stable`, and the quoting of source names.
**Where**: `core/log-lines.cpp` (with `core/log-lines.hpp`)
**Depends on**: None
**Reuses**: the pure-formatter pattern of `core/load-line.cpp`
**Requirement**: READ-84, READ-85, READ-86, READ-88

**Done when**:

- [x] `measure_start(3, "Shure MV7", 60, 1)` returns `measure.start cameras=3 mic="Shure MV7" canvas_fps=60`; fps is `fps_num ÷ fps_den` with up to two decimals and trailing zeros trimmed (30 → `30`, 60000/1001 → `59.94`, 30000/1001 → `29.97`). The spec says `<fps>` without a format; this is the task's decision and goes in the code comment. The two decimals are rounded, not cut: 24000/1001 gives `23.98`, as OBS names the rate, and 25/2 gives `12.5`.
- [x] `measure_stop()` and `measure_reset()` return `measure.stop` and `measure.reset`.
- [x] `reading_stable("Cam B", 142, 9)` returns `reading.stable camera="Cam B" reading_ms=+142 spread_ms=9`; −60 gives `reading_ms=-60` and 0 gives `reading_ms=0`. The sign is ASCII because the log is for machines, not the dock (a code comment).
- [x] A name containing `"` is written `\"`; a name containing a backslash is written `\\` (READ-88). `quote(name)` is public for the session's other lines.
- [x] The functions return the text without the `[camera-sync] ` prefix, which the plugin's logger adds.
- [x] Added doctest cases: 4, in `tests/core/log-lines.cpp`. Quick gate passes. Test count: ran 89, which is 85 plus 4. A doctest case name must hold no `"` or backslash: `doctest_discover_tests` writes it into CTest's list unescaped, which drops that case and breaks the next one.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): format the measuring log lines`

---

### Phase 5 tasks: Session

#### T14: Add the session's lifecycle and tap requests

**What**: `Session` with its ports (`Tap`, `TapFactory`, `Clock` reused from T7) and the Idle, Measuring and Stopped state machine that requests and releases taps.
**Where**: `core/reading/session.cpp` (with `core/reading/session.hpp`)
**Depends on**: T6, T7, T8
**Reuses**: `ReadingEstimator`, `CameraPipeline`, `MicPipeline`, `frame_interval_ms`
**Requirement**: READ-24, READ-28, READ-29, READ-30, READ-31, READ-16, READ-67, READ-68

**Done when**:

- [x] A fake `TapFactory` records creations and releases and can return a null tap for chosen sources. It also keeps the sink each tap was given, for the later tasks.
- [x] `set_selection` and `start(FrameRate)`: with two cameras and a mic, Start enters Measuring and requests 3 taps; the estimator's F is `frame_interval_ms(fps_num, fps_den)` (READ-16, READ-24); `reset_counters` is called. F shows only in a Stable window, so the marker entry points arrive here, adopting the first marker's pattern; T15 tests the adoption and adds the rest. F is checked at 60 and 59.94 fps, with five samples spanning just under F and just over it.
- [x] Start with no camera or no mic returns `NothingChosen`, requests nothing and leaves the state (READ-30). Checked from Idle and from Stopped.
- [x] A null camera tap is skipped with a Warning naming it, the other camera and the mic are tapped (READ-67). A null mic tap returns `MicTapFailed`, releases the taps already made, stays out of Measuring and sets `mic_tap_failed` (READ-68). The spec gives no warning text: it is `tap.failed camera="<name>"`, or `tap.failed mic="<name>"`. The names come from a `name_of` callback, a constructor argument from here on (a `SPEC_DEVIATION` note in `session.hpp`: the design's `SessionConfig` is gone, since `start` takes the frame rate). The mic's tap is made first, as the design says, so a failed mic leaves no camera tap to release; the next Start that gets a mic tap clears `mic_tap_failed`.
- [x] `collection_changing` releases every tap, clears readings and the selection and goes Idle; `shutdown` releases every tap (READ-28, READ-29). The collection change also forgets the pattern. `shutdown` leaves Measuring for Stopped, and the destructor calls it.
- [x] `stop()` and `reset()` exist as lifecycle-only for now: Stop in Idle or Stopped and Reset in Idle change nothing (READ-31). Their reading semantics are T15. Markers are read only while Measuring.
- [x] Taps are destroyed after the session lock is released (a test destroys a fake tap whose destructor tries to take the lock through a callback and does not deadlock). Every fake tap's destructor runs a delivery on another thread that takes the session's lock, and fails the case if it cannot finish within 2 s, so every test checks every release. The dedicated case covers Stop, a collection change and exit. Checked once by destroying Stop's taps under the lock: three failures, no hang.
- [x] Added doctest cases: at least 9, in `tests/reading/session.cpp`. Quick gate passes. Added 10. Test count: ran 99, which is 89 plus 10.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): add the measuring session and its tap lifecycle`

---

#### T15: Route markers and keep the session's pattern

**What**: The session's marker entry points adopt the first pattern, ignore a different one, and Stop, Start and Reset keep or clear readings as the spec says.
**Where**: `core/reading/session.cpp` (modify)
**Depends on**: T14
**Reuses**: `PatternParams::from`, `MicPipeline::set_pattern`, `ReadingEstimator`
**Requirement**: READ-17, READ-18, READ-25, READ-26, READ-27

**Done when**:

- [x] `on_video_marker` on a fresh session adopts the marker's pattern: the mic pipeline receives `set_pattern(f, c, q_ms)` once and the estimator receives the params (READ-17). Checked through the real `MicPipeline`: the audio fixture decodes and pairs once the first marker's pattern reaches the mic, and a later marker of another pattern does not replace it. The estimator's params are checked with `index_max` 16: indexes 14 then 1 miss 2, and markers half a wrap (1,584 ms) apart do not pair while 1,583 ms do.
- [x] A marker whose f, c, q or index_max differs (one test each) forms no sample (READ-18). One section each; the ignored marker is not counted as received either.
- [x] Stop keeps readings and the pattern and releases every tap; Start from Stopped requests taps again and every kept reading and state is unchanged (READ-25, READ-26). Start hands a kept pattern to the new mic pipeline, so the audio fixture decodes after Stop and Start.
- [x] Reset clears samples, markers and the pattern and reports each measured camera Looking; the next marker may then adopt a different pattern (READ-27). Reset leaves the session Measuring or Stopped, as the TDD's session diagram has it. Until a camera reads a pattern again the session takes no audio marker: after Reset the mic may still decode the cleared pattern, and READ-19 allows no audio marker without one.
- [x] The pattern survives the removal of the camera that supplied it (an edge case) until Reset. Tested by unticking that camera; the pattern belongs to the session, not to a camera.
- [x] Added doctest cases: at least 8. Quick gate passes. Added 8. Test count: ran 107, which is 99 plus 8.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): follow one pattern per session`

---

#### T16: Follow selection changes while measuring

**What**: `set_selection` diffs the new selection against the old and adds, releases or clears accordingly; the mic can change or disappear.
**Where**: `core/reading/session.cpp` (modify)
**Depends on**: T15
**Reuses**: `TapFactory`, `ReadingEstimator::clear_camera`
**Requirement**: READ-32, READ-33, READ-34, READ-35

**Done when**:

- [x] Ticking a camera while Measuring requests one tap and reports it Looking; while Stopped it only joins the selection (READ-32). Nothing else is asked for again, and the cameras already measured keep their readings.
- [x] Unticking a camera while Measuring or Stopped releases its tap if it has one, drops its reading, and leaves every other camera's tap and reading as they were (READ-33). Ticked again, the camera starts over at Looking, so the reading was dropped, not hidden.
- [x] Choosing another mic clears every reading and the pattern, and while Measuring replaces the mic tap and reports every camera Looking (READ-34). The cameras keep their taps, and the new mic's tap listens for the next pattern a camera reads (checked with the audio fixture). The spec does not say what follows while Measuring when the new mic's tap cannot be made, or when no mic is chosen any more: measuring ends as it does for a removed mic, with every tap released and the session Idle; a tap that cannot be made also warns and sets `mic_tap_failed` (READ-68). The mic's pipeline and tap are now held together, like a camera's.
- [x] `on_mic_removed` releases every tap, clears readings and the pattern and enters Idle (READ-35). It also clears the chosen mic, so the dock asks for one (the design's error table); the cameras stay ticked. Choosing a mic afterwards clears the pattern too, so the test sees it gone either way.
- [x] Added doctest cases: at least 6. Quick gate passes. Added 7. Test count: ran 114, which is 107 plus 7.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): follow camera and mic changes while measuring`

---

#### T17: Detect a mic that hears nothing

**What**: The session's no-sound timer and the `no_pattern_sound` flag in its snapshot.
**Where**: `core/reading/session.cpp` (modify)
**Depends on**: T16
**Reuses**: the injected `Clock`
**Requirement**: READ-42, READ-43 (the session half)

**Done when**:

- [x] With a fake clock: the first video marker at t = 0 and no audio marker leaves `no_pattern_sound` false at 4.999 s and true at 5.0 s while video markers keep arriving (READ-42). Only markers the session reads count: its own pattern's, while Measuring. The flag is worked out in `snapshot()` from the clock, so `tick()` plays no part in it.
- [x] An audio marker at any time clears it and restarts the wait for the next video marker; video markers stopping for 5 s clears it; Stop, Reset, Start and leaving Measuring clear it (READ-43). Choosing another mic also restarts the wait, so one mic's silence is never reported under the next mic's name; the spec does not say. Stop restarts the wait, and every way back to Idle ends with a mic being chosen, which restarts it too, so Start's own restart cannot be told apart in a test; it stays, as the spec's rule. Follow-up (Verifier gap 2, mutant E2): `shutdown()` leaves Measuring without a restart, so a Start after it does tell Start's own restart apart, and a case checks it. Test count: 161. Follow-up (Verifier gap 2, mutant F6): a case checks where the wait starts over when video markers resume, at the design's 5 s or more. A marker 4.999 s after the last leaves the message on; one exactly 5 s after it clears the message and starts a new 5 s wait. Test count: 164.
- [x] A session that is Stopped or Idle never reports it. Checked Idle, Stopped, and Idle after a collection change or a removed mic, with markers still arriving.
- [x] A mic that was heard and then muted: audio marker at t = 0, video markers continuing, no audio marker: true at 5 s after the first video marker that followed it. False at 5.199 s, true at 5.2 s.
- [x] Added doctest cases: at least 6. Quick gate passes. Added 6. Test count: ran 120, which is 114 plus 6.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): flag a mic that hears no pattern sound`

---

#### T18: React to OBS source changes

**What**: The session's handlers for a removed, renamed or silent camera, a camera's filter change and the mic's Sync Offset change, driven by `tick()`.
**Where**: `core/reading/session.cpp` (modify)
**Depends on**: T17
**Reuses**: `CameraPipeline::last_frame_ns`, `ReadingEstimator::clear_camera`
**Requirement**: READ-55, READ-56, READ-57, READ-58, READ-59

**Done when**:

- [x] `on_camera_removed` and `on_camera_renamed` drop the reading, release the tap and remove the camera from the selection; the session reports the camera as no longer ticked (READ-55, READ-56). Both take one path, in any state, and release the tap only if the camera has one. A removed camera that brought the pattern leaves it to the others until Reset (the spec's edge case, checked here through a removal too). A renamed camera ticked again starts at Looking.
- [x] `tick()` with a camera silent for 1.999 s keeps it; at 2.0 s it drops its reading, releases its tap and unticks it, and a camera that sees no pattern but delivers frames is untouched (READ-57). Only cameras with a tap are checked; their pipeline stamps each frame with the clock. The test is last frame + 2 s ≤ now, because a tap's thread can stamp a frame just after `tick` reads the clock. A camera that delivers a frame every 500 ms for 10 s and never sees the pattern stays ticked and Looking. Follow-up (Verifier gap 1, mutant F5): a case shows the untick itself. After the 2 s drop, ticking the camera again gives it a Looking row and a new tap, and Stop then Start taps only the mic and the other camera. Test count: 163.
- [x] `tick()` does nothing while Stopped or Idle. No camera has a tap outside Measuring, so there is nothing to drop; the test checks that the selection and a kept reading stay.
- [x] `on_camera_filters_changed(id)` clears that camera's reading and reports it Looking; other cameras keep theirs (READ-58). In any state; the camera keeps its tap.
- [x] `on_mic_sync_offset_changed()` clears every reading and reports every camera Looking, and keeps the pattern (READ-59). The audio markers go too, since each was stamped with the old offset.
- [x] Added doctest cases: at least 7. Quick gate passes. Added 8. Test count: ran 128, which is 120 plus 8.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): drop readings when a source changes`

---

#### T19: Log the session's events

**What**: The session logs start, stop, reset, stable readings, warnings and Debug markers through `LogLines`.
**Where**: `core/reading/session.cpp` (modify)
**Depends on**: T18, T13
**Reuses**: `core/log-lines.hpp`, `Logger`
**Requirement**: READ-84, READ-85, READ-86, READ-87, READ-89

**Done when**:

- [x] Start logs the `measure.start` text at Info with the number of ticked cameras, the mic name and the canvas fps; Stop and Reset log `measure.stop` and `measure.reset` at Info (READ-84, READ-85). The session gets names from a `name_of(SourceId)` callback it is constructed with. `name_of` has been a constructor argument since T14. Start logs only when it starts measuring, and Stop and Reset only when they apply.
- [x] The estimator's stable-change results log `reading.stable` at Info with the reading and spread (READ-86). Both paths: a video marker that completes its pair, and an audio marker that completes one or several. A source's name is read when its tap is made, on the UI thread, because the line is written on a tap's thread.
- [x] Every video and audio marker logs at Debug with camera or mic, index and time, each rejected or foreign-pattern payload logs at Debug with its parameters, and a camera's skipped frames (from `Tap::skipped_frames`) log at Debug at most once per 5 s per camera (READ-87). The spec gives no Debug format, so these follow the key=value form of `LogLines`: `marker.video camera="<name>" index=<i> time_ns=<t> f=<f> c=<c> q_ms=<q> index_max=<I>`, `marker.audio mic="<name>" index=<i> time_ns=<t>`, `marker.ignored` with the same fields for another pattern's marker and for an audio marker heard with no pattern set, and `frames.skipped camera="<name>" skipped=<total>`, written when the count has grown, at most once per 5 s. A rejected payload is logged at Debug by the camera's detector (T1's "qr payload logs a rejected value at debug level") through the logger the session hands each pipeline; the session test checks that wiring through the pipeline's dropped-frame warning. A marker from a camera with no tap is dropped: only a released tap's late delivery can bring one.
- [x] A full scenario's Info and Warning lines all match the allowlist of formats in `LogLines` plus the tap warnings, and no line carries samples, frame data or a name that was not passed in (READ-89). The scenario writes each allowed format at least once, with a camera whose name holds a quote and a backslash. The Warning formats are the two `tap.failed` lines and the pipeline's dropped-frame line.
- [x] Added doctest cases: at least 6. Quick gate passes. Added 6. Test count: ran 134, which is 128 plus 6. The 45 session cases, and all 126 doctest cases, also pass once built with `-fsanitize=thread` (g++ 13.3 on WSL, under `setarch -R`) with no report.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): log measuring events`

---

### Phase 6 tasks: View-model

#### T20: Add the dock's strings and a locale parity check

**What**: The list of locale keys the view-model uses, their en-US values, a placeholder helper, and a test that the two agree.
**Where**: `core/viewmodel/strings.hpp`
**Depends on**: T14
**Reuses**: `data/locale/en-US.ini`, the doctest fixture-path define pattern
**Requirement**: READ-48

**Done when**:

- [x] `data/locale/en-US.ini` holds every key in the design's table with the exact values (U+2212 never appears in the ini; the code formats the sign). The ini keeps `CameraSync.Dock.Title` and gains the 15 keys of the table. `strings.hpp` names each key as a constant in `camera_sync::strings` and lists them all in `all_keys`.
- [x] A `CAMERA_SYNC_LOCALE_FILE` compile definition points the tests at that file.
- [x] A test parses the ini and asserts every key in `strings.hpp` is present and every `CameraSync.*` key in the ini except `CameraSync.Dock.Title` is in `strings.hpp` (READ-48). A second case checks the 15 values against the design's table and that no value holds U+2212.
- [x] `fill("{value} ms - settling", {{"value", "+142"}})` returns `+142 ms - settling`; an unknown placeholder is left as written; a value containing `{` is not re-expanded. `fill` takes a `std::vector<Placeholder>` of name and value pairs.
- [x] Added doctest cases: 3, in `tests/viewmodel/strings.cpp`. Quick gate passes. Test count: ran 137, which is 134 plus 3.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): list the dock's locale strings`

---

#### T21: Build the camera rows

**What**: `build_view` produces the checklist rows and their status text from a session snapshot.
**Where**: `core/viewmodel/view-model.cpp` (with `core/viewmodel/view-model.hpp`)
**Depends on**: T20, T4, T12
**Reuses**: `SessionSnapshot`, `camera_list`, `fill`
**Requirement**: READ-37, READ-38, READ-39, READ-40, READ-41, READ-44

**Done when**:

- [x] `format_signed_ms`: 142 → `+142`, −60 → `−60` with U+2212 (bytes E2 88 92), 0 → `0`.
- [x] Looking → "Looking for the pattern"; Settling with live 142 → "+142 ms - settling"; Stable 142 → "+142 ms - video late"; Stable −60 → "−60 ms - video early"; Stable 0 → "0 ms - in sync" (READ-37 to READ-41, exact strings). A Settling row shows `live_ms` and a Stable row `reading_ms`, never the other. The spec gives no text for a Settling live value of 0 or below: it follows `format_signed_ms`, so "0 ms - settling" and "−60 ms - settling" (a spec-precision gap, READ-38).
- [x] A camera unticked or dropped has no status text (READ-44); rows are in name order and the chosen mic is not a row. A ticked camera gets a status only when the session snapshot holds a row for it; an unticked one gets none even if the snapshot still does.
- [x] Added doctest cases: at least 7, in `tests/viewmodel/view-model.cpp`. Quick gate passes. Added 9. Test count: ran 146, which is 137 plus 9. `build_view` takes the design's `bool details` already, unused until T23, so T22 and T23 do not change its callers; `DockView` holds the camera rows only until T22.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): show each camera's state in the dock view`

---

#### T22: Build the buttons and the message line

**What**: Start/Stop and Reset state, the message line precedence, and the mic list.
**Where**: `core/viewmodel/view-model.cpp` (modify)
**Depends on**: T21, T17
**Reuses**: `mic_list`, `SessionSnapshot::no_pattern_sound`, `mic_tap_failed`
**Requirement**: READ-36, READ-42, READ-43, READ-45, READ-46, READ-47

**Done when**:

- [x] No camera ticked or no mic chosen: Start disabled and the message "Pick at least one camera and the mic" (READ-36). Both cases are tested; "ticked" and "chosen" come from the `Choice`.
- [x] At least one camera and a mic, Idle or Stopped: Start enabled (READ-47). Measuring: the button reads "Stop"; otherwise "Start" (READ-45). While Measuring the button stays enabled, with or without a ticked camera, so a session can always be stopped; the spec says nothing about it (a spec-precision gap, READ-47).
- [x] Reset is disabled in Idle and enabled in Measuring and Stopped (READ-46).
- [x] `no_pattern_sound` with mic "Shure MV7" gives "No pattern sound from Shure MV7 - bring the phone closer or unmute it"; a false flag gives no such message (READ-42, READ-43). The mic's name is the snapshot's mic, or the choice's when the snapshot has none, looked up in the catalog.
- [x] `mic_tap_failed` gives "Could not listen to Shure MV7". Precedence when several apply: pick-prompt, then mic tap failure, then no-sound.
- [x] The mic list leaves out ticked cameras. `DockView` gains `mics` (`MicView`: id, name, chosen), `message`, `start_enabled`, `reset_enabled` and `start_label`.
- [x] Added doctest cases: at least 8. Quick gate passes. Added 9. Test count: ran 155, which is 146 plus 9.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): show the buttons and messages in the dock view`

---

#### T23: Show the details toggle's counts

**What**: With details on, each ticked camera's row adds "received {n}, missed {m}".
**Where**: `core/viewmodel/view-model.cpp` (modify)
**Depends on**: T22, T6
**Reuses**: `CameraRow::received`, `CameraRow::missed`
**Requirement**: READ-97

**Done when**:

- [x] Details on, a camera with received 12 and missed 2 has the detail text "received 12, missed 2" in its own `CameraRowView` field, next to its unchanged status text; the dock shows both (T33). The field is `details`, and `DockView::show_details` carries the toggle. A camera that is not ticked has an empty `details`, as it has an empty status.
- [x] Details off: the detail field is empty.
- [x] Added doctest cases: 2. Quick gate passes. Test count: ran 157, which is 155 plus 2.

**Tests**: unit
**Gate**: quick

**Commit**: `:sparkles: feat(reading): add marker counts to the dock view`

---

### Phase 7 tasks: CI quality gates

#### T24: Gate the core's line coverage at 90%

**What**: A coverage build option and a `gcovr` step in the core-only CI job that fails below 90% of the lines of `core/`, excluding `core/detector/`.
**Where**: `.github/workflows/build-project.yaml`
**Depends on**: T23
**Reuses**: the `core-build` job (line 74); `core/CMakeLists.txt`; `CMakePresets.json`
**Requirement**: READ-90

**Done when**:

- [x] `CAMERA_SYNC_COVERAGE` adds `--coverage` to the core and tests; a `core-coverage` preset (or a flag on the `core` preset) enables it; `CMakeLists.txt`, `core/CMakeLists.txt` and `CMakePresets.json` are updated. The preset is a configure, build and test preset of that name (Debug, `build_core_coverage`), and `tests/CMakeLists.txt` also changed: under the option the tests link the core whole (`$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`). gcovr reads only the counts a linked object writes at exit, so a core source that no test reaches would otherwise vanish from the report instead of counting as uncovered (removing `tests/reading/session.cpp` from the tests without this left the total at 97.19%).
- [x] The core job installs `gcovr` (Ubuntu 24.04's package, with the build tools) and runs it with `--filter 'core/' --exclude 'core/detector/' --fail-under-line 90`, after building and testing the `core-coverage` preset. The step also posts the measured percentage as a notice annotation, so a run shows its number without the job log.
- [x] The measured value is recorded in this file. Measured on 2026-10-16 with gcc 13 `gcov` JSON on the WSL machine (gcovr is not installed there, and the count is the same: a line is covered when any translation unit ran it): 637 of 654 lines, 97.40%, over `core/` without `core/detector/`. Above 90%, so no tests were added. The CI run's notice gives gcovr's own figure.
- [x] Verified once that removing a core test file turns the coverage red. Done in a scratch copy of the tree on the WSL machine, not on a pushed scratch branch (a branch push was outside the authorisation for this batch): `tests/reading/session.cpp` removed from `tests/CMakeLists.txt`, 112 tests pass, coverage 381 of 652 lines, 58.44%, under 90%. The same removal on a scratch branch is still open if the maintainer wants it.
- [x] CI gate passes with the coverage step green. Quick gate: ran 157, unchanged (no test added).

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the five bullets map to `CMakeLists.txt` (option), `core/CMakeLists.txt` and `tests/CMakeLists.txt` (instrumentation), `CMakePresets.json` (`core-coverage`), `.github/workflows/build-project.yaml` (gcovr step), the measured 97.40% and the 58.44% negative run; no assertion exists to cite, the gate is the build. Check C: no test was added or changed, so nothing is unclaimed. Verdict: criteria covered by the build and the measured runs; the scratch-branch variant is open.

**Tests**: none
**Gate**: ci

**Commit**: `:construction_worker: ci: fail the core job below 90% line coverage`

---

#### T25: Fuzz the QR payload parser and replay a corpus in CTest

**What**: A fuzz harness for `QrPayload::decode` with a boundary corpus, built as a libFuzzer target when Clang is asked for and as a plain corpus replay in every build.
**Where**: `tests/fuzz/qr-payload-fuzz.cpp`
**Depends on**: T1
**Reuses**: `QrPayload`; the CTest layout in `tests/CMakeLists.txt`
**Requirement**: READ-92

**Done when**:

- [x] `LLVMFuzzerTestOneInput` copies the input, appends a NUL, calls `decode`, and aborts when it returns true with a value outside f 10 to 32,000, c 1 to f, q 1 to 1,000 or i 0 to 255. It also aborts when `valid` is false on a true return, and prints the values first. The function is in `qr-payload-fuzz.cpp`; `qr-payload-replay.cpp` is the replay's `main`.
- [x] `tests/fuzz/corpus/` holds boundary seeds: f=9, 10, 32000, 32001; c=0, 1, f, f+1; q=0, 1, 1000, 1001; i=-1, 0, 255, 256; an empty input; 4,096 `a` bytes; a missing key; repeated keys; a negative number; a 40-digit number. 23 files, 7,191 bytes with the sources, named for what they hold (`f-32001`, `c-f-plus-1`, `i-minus-1`, `empty`, `a-4096`, `missing-i`, `repeated-f-valid-then-invalid`, ...). Each is `f=884,c=2,q=66,i=0` with one value changed. A repeated key is tested in both orders.
- [x] A `qr-payload-replay` executable runs every corpus file through the same function and is registered as the CTest test `fuzz_corpus_replay`; it builds with any compiler. The replay fails when the corpus directory holds no file, so a wrong path cannot pass.
- [x] With `CAMERA_SYNC_FUZZ=ON` and Clang, `qr-payload-fuzz` builds with `-fsanitize=fuzzer,address`; the option is off by default. Not built on the WSL machine, which has no Clang and gets none; T26's CI job is its first build. The option stops the configure with a message on any other compiler.
- [x] Verified locally once: deleting the `f` upper-bound check in `QrPayload::check` makes `fuzz_corpus_replay` fail (reverted before committing). Done for each of the seven range checks (f lower, f upper, c lower, c upper, q lower, q upper, i): each removal aborts the replay (`Subprocess aborted`). `core/detector/qr-payload.hpp` was restored and is not in the commit.
- [x] Added CTest tests: 1. Quick gate passes. Test count: ran 158, which is 157 plus 1. The core-coverage total stays 97.40% (the replay adds no core line).

**Test Adequacy:** Check A: criterion 1 `tests/fuzz/qr-payload-fuzz.cpp:14-16` (copy, NUL) and `:19-27` (`if (!payload.decode(text.data(), nullptr))`, the range test, `std::abort()`), shown to fire by the seven removals above; criterion 2 `tests/fuzz/corpus/` (one file per listed boundary); criterion 3 `tests/CMakeLists.txt:52-56` (`add_test(NAME fuzz_corpus_replay COMMAND qr-payload-replay ...)`), run passing in the 158; criterion 4 `tests/CMakeLists.txt:60-70` (not built here, see its bullet); criterion 5 the seven removals. Check B: the replay's assertion is an abort on an accepted out-of-range value, and each removal makes it fire, so it is not a no-error tautology. Check C: the one CTest test maps to READ-92. Check D: the layout of `tests/CMakeLists.txt` and the SPDX header rule. Verdict: all criteria covered except criterion 4, whose build waits for T26's Clang job. Follow-up: the macOS CI step builds only `camera-sync-tests` and then runs CTest, so the replay was never built there and `fuzz_corpus_replay` failed with exit code 8; `camera-sync-tests` now depends on `qr-payload-replay`.

**Tests**: unit
**Gate**: quick

**Commit**: `:white_check_mark: test(reading): fuzz the qr payload parser`

---

#### T26: Run the fuzz target in CI

**What**: A step in the Ubuntu core job that builds the libFuzzer target with Clang and runs it for 60 s on the corpus.
**Where**: `.github/workflows/build-project.yaml`
**Depends on**: T24, T25
**Reuses**: the core job; `CAMERA_SYNC_FUZZ`
**Requirement**: READ-91

**Done when**:

- [x] The job installs `clang`, configures a second build directory with `CC=clang CXX=clang++ -DCAMERA_SYNC_CORE_ONLY=ON -DCAMERA_SYNC_FUZZ=ON`, and runs `qr-payload-fuzz -max_total_time=60 tests/fuzz/corpus`. The step is `Fuzz QR Payload Parser` in the `core-build` job, after the coverage step. It also installs `libclang-rt-18-dev` (the libFuzzer runtime, a separate package on Ubuntu 24.04), builds `build_fuzz` as RelWithDebInfo, builds only the `qr-payload-fuzz` target and adds `-timeout=10`.
- [x] A crash, a hang or an out-of-range acceptance fails the job. The step is a plain command, so libFuzzer's non-zero exit (a crash, `-timeout=10` expiring, or the target's `std::abort()`) fails it.
- [ ] Verified once on a scratch branch that is never merged: the same broken range check as in T25 turns this step red. **Open:** the batch's authorisation covered pushes to `main` only, and the WSL machine has no Clang. The corpus replay's response to each broken check is recorded in T25; the fuzz step's own response waits for a scratch-branch run.
- [x] CI gate passes with the fuzz step green. No test added: the `ctest --preset core -N` total stays 158. Check A: the bullets map to the workflow step above and the CI run (recorded in the batch report). Check C: no test was added or changed. Verdict: criteria covered by the CI step; the scratch-branch variant is open.

**Tests**: none
**Gate**: ci

**Commit**: `:construction_worker: ci: fuzz the qr payload parser for 60 seconds`

---

### Phase 8 tasks: Plugin adapters, controller and dock

#### T27: List OBS sources for the catalog

**What**: `ObsCatalog::list()` turns OBS sources into `SourceInfo`, and a recursive scan marks the sources inside the current Program scene.
**Where**: `src/obs/obs-catalog.cpp` (with `src/obs/obs-catalog.hpp`)
**Depends on**: T12
**Reuses**: `SourceInfo`; `obs_enum_sources`, `obs_source_get_output_flags`, `obs_source_get_uuid`, `obs_frontend_get_current_scene`, `obs_scene_enum_items`
**Requirement**: READ-49, READ-50 (the scene scan)

**Done when**:

- [x] `async_video` is set from `OBS_SOURCE_ASYNC_VIDEO` and `audio` from `OBS_SOURCE_AUDIO`; `id` is the UUID; every reference released. `async_video` needs both bits of `OBS_SOURCE_ASYNC_VIDEO`. `obs_enum_sources` lends each source for the length of the call, so the only reference taken is the Program scene's from `obs_frontend_get_current_scene`, and it is released. Every source `obs_enum_sources` lists is returned; in OBS 30.0.2 that is the inputs and the groups, and a group has neither flag.
- [x] `in_program` is true for sources in the Program scene, in a nested scene and in a group, with a guard against a scene that contains itself. Each item's source is walked through `obs_group_or_scene_from_source`, and a set of the scenes already walked ends a loop. `obs_frontend_get_current_scene` returns Program in studio mode too (OBS 30.0.2, `UI/api-interface.cpp`).
- [x] The file is added to the plugin's sources in `CMakeLists.txt`; no rule is added here (the policy functions decide). `ObsCatalog` has the one method, `list()`, and no state.
- [x] CI gate passes on all three platforms. The rig check is in T34. Quick gate: ran 158, unchanged (no test added). Before the push, `src/obs/obs-catalog.cpp` was checked with `g++ 13 -std=c++17 -fsyntax-only -Wall -Wextra -Werror` against the libobs and frontend headers of OBS 30.0.2's source archive (its SHA-256 matches `buildspec.json`), in a scratch directory outside the repository.

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/obs/obs-catalog.cpp` (`list_source`, `scan_scene`, `scan_item`) and `CMakeLists.txt`; the gate is the CI build, and the behavior is walked on the rig in T34. Check C: no test was added or changed. Verdict: criteria covered by the build; the rig check is open.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): list cameras and mics from OBS`

---

#### T28: Save and load the selection with the scene collection

**What**: `SelectionStore` maps `SavedSelection` to the collection's module data on save and back on load.
**Where**: `src/obs/selection-store.cpp` (with `src/obs/selection-store.hpp`)
**Depends on**: T12
**Reuses**: `obs_frontend_add_save_callback`, `obs_data_t` and arrays
**Requirement**: READ-54, READ-53 (the load side)

**Done when**:

- [x] On save it writes `version` 1, `cameras` as an array of objects that each hold one camera's `uuid`, and `mic` as a string, left out when no mic is chosen (READ-54 as amended on 2026-10-16: OBS 30's `obs_data` holds arrays of objects only and writes no null). The key is `camera-sync`, a constant, not the build's name. What to save comes from a `Current` callback the controller gives the store (T32), asked at each save: on a collection switch OBS 30.0.2 fires the changing event, then saves the old collection, then loads the new one, so the answer has to stay the old collection's until the new one loads.
- [x] On load it returns a `SavedSelection`, or `nullopt` when the key is absent or malformed, or when OBS passes no module data at all (a null `save_data`, which OBS 30.0 passes for a collection with no `modules` object); the version field is passed through for `initial_choice` to judge. `saved()` holds what the last load found, for the controller to read at the finished-loading and collection-changed events. Malformed means the key is not an object. A missing or non-numeric `version` reads as 0, and one outside an `int` as 0 rather than wrapping, which `initial_choice` treats as no saved selection; a camera with no `uuid` and an empty `mic` are skipped.
- [x] No rule beyond the field copy; the file is added to `CMakeLists.txt`. The destructor removes the save callback, so the controller destroys the store before the frontend goes, at the exit event at the latest (after it, OBS 30.0.2 logs an error for the removal). With four sources the `target_sources` call no longer fits in 120 columns, so it takes gersemi's expanded form.
- [x] CI gate passes on all three platforms. Quick gate: ran 158, unchanged (no test added). Checked before the push with the same `g++ -fsyntax-only` run as T27 against the OBS 30.0.2 headers.

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/obs/selection-store.cpp` (`write`, `read`, `on_save`) and `CMakeLists.txt`; the gate is the CI build, and the round trip (tick, save, restart, same ticks) is walked on the rig in T34. Check C: no test was added or changed; the core's `SavedSelection` and `to_saved` hold no JSON, so their T12 tests stand. Verdict: criteria covered by the build; the rig check is open.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): store the selection in the scene collection`

---

#### T29: Tap the mic's audio

**What**: `MicTap` delivers the mic's own samples, with the Sync Offset added to their time, to an `AudioSink`.
**Where**: `src/obs/mic-tap.cpp` (with `src/obs/mic-tap.hpp`)
**Depends on**: T14
**Reuses**: `AudioSink`, `AudioBlock`; `obs_source_add_audio_capture_callback`, `obs_source_get_sync_offset`
**Requirement**: READ-62, READ-66, READ-68 (the failure path)

**Done when**:

- [x] The callback builds an `AudioBlock` from `audio_data` with `timestamp = max(0, timestamp + sync_offset_ns)`, reading the offset on every block. The sum is taken as a signed 64-bit value and held at 0 below it. In OBS 30.0.2 the capture callback gets the source's audio after resampling (planar float at OBS's rate, so planes 0 and 1 go to the block, plane 1 null for mono) but with the device's timestamp, before the timing adjustment and the Sync Offset the mix adds (`obs-source.c`, `source_output_audio_data`).
- [x] A muted block is sent as zeros of the same length and time. The zeros live in a buffer the tap reuses on the audio thread; a stereo block gets the zeros in both planes.
- [x] The tap holds an `obs_weak_source_t`; a removed source makes the callback return without a call, and the destructor removes the callback and waits for one in flight. libobs calls the capture callbacks under `audio_cb_mutex`, which `obs_source_remove_audio_capture_callback` takes, so the removal waits. A source that is already destroyed took its callback list with it, and the destructor only releases the weak reference. `skipped_frames()` is 0: the callback hands over every block.
- [x] Construction returns null when the source cannot be resolved. `MicTap::create(uuid, sink)`, through `obs_get_source_by_uuid`; the strong reference is released once the callback is added.
- [x] CI gate passes on all three platforms. The timestamp claim is checked in T35. Quick gate: ran 158, unchanged (no test added). Checked before the push with the T27 `g++ -fsyntax-only` run, and with `-Wconversion` for the narrowing that MSVC's `/W3 /WX` rejects. With five sources the `target_sources` list takes gersemi's one-per-line form.

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/obs/mic-tap.cpp` (`create`, `on_audio`, `~MicTap`) and `CMakeLists.txt`; the gate is the CI build, and the Sync Offset claim is compared with Sync Dock on the rig in T35. Check C: no test was added or changed. Verdict: criteria covered by the build; the rig check is open.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): tap the mic's audio with its sync offset`

---

#### T30: Tap a camera through a private view

**What**: `CameraTap` renders the camera into a private view with its own video mix and delivers NV12 luma frames to a `FrameSink`.
**Where**: `src/obs/camera-tap.cpp` (with `src/obs/camera-tap.hpp`)
**Depends on**: T14
**Reuses**: `FrameSink`, `VideoFrame`, `LumaFormat::Luma8`; `obs_view_create`, `obs_view_set_source`, `obs_view_add2`, `video_output_connect`
**Requirement**: READ-61, READ-63, READ-64, READ-66, READ-67 (the failure path), READ-69

**Done when**:

- [x] The view holds only the camera on channel 0; the mix runs at the canvas frame rate, 640×360 with aspect kept, NV12; the frame time passed on is the mix frame's `timestamp`. A view draws its channel at the source's own size, not scaled to the canvas, so the mix's base is the camera's size after its filters, or the canvas's when the camera has no size yet; its output is that size fitted into 640×360 with the aspect kept, never scaled up, and each side even for NV12. The mix copies the canvas's settings (colour space, range, scale type), and `obs_view_add2` gives every mix the main mix's frame rate anyway. The timestamp is the one every mix's frame carries for its render-loop tick.
- [x] The tap adds no filter, scene or property, and does not change the camera's active state; the destructor disconnects (waiting for a callback in flight), removes the view and destroys it. `obs_view_set_source` activates the camera as an `AUX_VIEW`, which raises its show count only (shown, not active). The private output is not part of the scene collection. The destructor stops the output and waits until `obs_output_active` is false (libobs ends the capture on its own thread, and that thread's `video_output_disconnect` waits for a frame in flight), releases it, then removes and destroys the view, which releases the camera.
- [x] `skipped_frames()` counts timestamp gaps over 1.5 frame intervals. Each such gap adds the frames missing from it (the gap in frame intervals, rounded, minus 1), so the count is in frames, as `Tap` says.
- [x] Construction returns null when the weak reference, the view or the mix cannot be made. Also when no source has the UUID, the canvas has no video info, or the output cannot be created or started; a partly built tap is torn down by its destructor.
- [x] CI gate passes on all three platforms. **Spike-dependent**: the three claims in the design (inactive camera shown without activating, timestamp equal to Program's, mix at canvas fps) are checked in T35; if one fails, only this file changes. Quick gate: ran 158, unchanged (no test added). Checked before the push with the T27 `g++ -fsyntax-only` run and `-Wconversion` against the OBS 30.0.2 headers.

**SPEC_DEVIATION (in `camera-tap.hpp`, decided by the maintainer on 2026-10-16):** the frames reach the tap through a private raw `obs_output` bound to the mix's video, not through `video_output_connect`. In OBS 30.0.2 a mix renders and hands out raw frames only while its `raw_active` count is up, and only the internal `start_raw_video` raises it (outputs and encoders reach it; `video_output_connect` does not), so the design's connection would never get a frame. The output type, `camera_sync_camera_tap`, is registered once in `obs_module_load` (one line in `src/plugin-main.cpp`, outside this task's file list, approved). `design.md`'s Tap B integration row and `CameraTap` paragraph are amended in this commit.

**Forwarding rule added for READ-57:** the mix renders every tick, whether or not the camera has a picture, so a frame goes on to the pipeline only while the camera reports a size. A removed camera, or one whose device stopped and cleared its frame, reports none, and its pipeline's last frame time stops moving, so the session's 2 s rule can drop it.

**Risks for phase 9 (rig):**
- The output path itself: frames arrive at the canvas rate, carry Program's timestamp for the same tick, and the camera is shown without being activated (T35, with T30's three claims).
- Cost: each camera adds a render at its own size, a scale to at most 640×360, an NV12 conversion and a readback every frame (T36, READ-93, READ-94).
- Size: the base is the camera's size at Start. A camera with no size yet uses the canvas's, and a camera that changes resolution while measured keeps the first size, so its picture is cropped or padded until Stop and Start (T35).
- A device that freezes on its last frame without clearing it still reports a size, so the tap keeps delivering and READ-57 does not drop it (T34, the unplugged-camera row).
- OBS logs three Info lines per camera at each Stop for the output (`Output 'camera-sync camera tap': stopping`, `Total frames output`, `Total drawn frames`); they hold no user data, but check them against READ-89 in the log (T34).
- In OBS 30 other plugins' main render callbacks draw into every mix, so into each private mix too (T35).
- While measuring, OBS counts video as active, so the video settings cannot be changed until Stop (T34).

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/obs/camera-tap.cpp` (`create`, `on_frame`, `~CameraTap`, the output callbacks), `src/plugin-main.cpp` (the registration) and `CMakeLists.txt`; the gate is the CI build, and the claims above are walked on the rig. Check C: no test was added or changed. Verdict: criteria covered by the build; the rig checks are open.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): tap each camera through a private video mix`

---

#### T31: Provide the taps to the session

**What**: `ObsTapFactory` implements `TapFactory` with the two taps and the mic's sample rate.
**Where**: `src/obs/obs-tap-factory.cpp` (with `src/obs/obs-tap-factory.hpp`)
**Depends on**: T29, T30
**Reuses**: `MicTap`, `CameraTap`, `audio_output_get_sample_rate(obs_get_audio())`
**Requirement**: READ-61, READ-62

**Done when**:

- [x] `create_camera_tap` and `create_mic_tap` return the taps, or null when construction fails; `mic_sample_rate()` returns the audio output's rate. They hand back `CameraTap::create` and `MicTap::create`, each null when its tap cannot be made. The rate is `audio_output_get_sample_rate(obs_get_audio())`: the mic tap's samples arrive after OBS has resampled them to its own rate (T29).
- [x] The file is added to `CMakeLists.txt`. CI gate passes on all three platforms. Quick gate: ran 158, unchanged (no test added). Checked before the push with the T27 `g++ -fsyntax-only` run against the OBS 30.0.2 headers.

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/obs/obs-tap-factory.cpp` and `CMakeLists.txt`; the gate is the CI build. Check C: no test was added or changed. Verdict: criteria covered by the build.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): create taps for the session`

---

#### T32: Connect OBS to the session

**What**: `Controller` owns the session and the adapters, marshals every OBS callback to the UI thread, and refreshes a view sink every 100 ms.
**Where**: `src/controller.cpp` (with `src/controller.hpp`)
**Depends on**: T19, T23, T27, T28, T31
**Reuses**: `Session`, `build_view`, `initial_choice`, `to_saved`, `ObsCatalog`, `SelectionStore`, `ObsTapFactory`; `obs_frontend_add_event_callback`; QTimer
**Requirement**: READ-28, READ-29, READ-55, READ-56, READ-58, READ-59, READ-96

**Done when**:

- [x] Frontend events: collection changing → `collection_changing`; finished loading → load the saved selection, `initial_choice`, `set_selection`; exit → `shutdown`, before `obs_module_unload` releases anything. The collection-changed event loads the selection too: OBS 30.0.2 sends finished loading only at startup, and a switch is changing, save of the old collection, load, changed. So at the changing event the controller keeps its choice for that save, and has the store forget what the old collection loaded with, since a new or duplicated collection is not loaded and saved nothing (two lines added to `SelectionStore`: `forget()`). At exit the controller stops its timer, disconnects every signal, shuts the session down, destroys the store and removes its event callback while the frontend still exists. OBS 30.0.2 sends exit after it has removed every source and checked for leaked ones; by then the taps are gone, because `source_remove` reaches the controller through queued calls that OBS runs during that clear, and a removed mic releases every tap.
- [x] Signals `source_remove`, `source_rename`, per-camera `filter_add`/`filter_remove` and the mic's `audio_sync` are connected for the selection and disconnected when it changes; each reaches the session through a queued call on the UI thread. The two global signals are connected for the controller's life, up to exit, and act only on the ticked cameras and the chosen mic: a removed or renamed camera leaves the choice and the session drops it (READ-55, READ-56), a removed mic ends measuring (READ-35). The per-source signals are rewired after every change of the choice and hold weak references, so a watched source can still be destroyed. Each handler queues a lambda through `QMetaObject::invokeMethod` on a `QObject` the controller owns, so it runs on the UI thread and is dropped if the controller is gone. No class uses `Q_OBJECT`, and the local variables avoid Qt's `signals` keyword.
- [x] A 100 ms `QTimer` calls `session.tick()`, builds the view and hands it to a `ViewSink` interface that the dock implements in T33. The choice follows the session after each tick, since the session unticks a camera that delivered no frame for 2 s on its own (READ-57). The view is built only while a sink is set and its widget is alive (a `QPointer`): OBS deletes the main window, and the dock in it, after unloading the plugin, so the dock never calls the controller from its destructor. Start reads the canvas frame rate with `obs_get_video_info` each time (READ-16).
- [x] The logger passes through `obs_log` with the `[camera-sync] ` prefix. `obs_log` adds the prefix; Error, Warning, Info and Debug map to OBS's levels, so Debug lines show only when OBS logs verbosely. The clock is `os_gettime_ns`, and the session's `name_of` reads a source's name by UUID.
- [x] The file is added to `CMakeLists.txt`. CI gate passes on all three platforms. The controller is created in `obs_module_load`, after the camera tap's output type is registered, and deleted in `obs_module_unload` (`src/plugin-main.cpp`, outside this task's file list, so that it runs). Until T33 the one-line dock stays, and the controller has no sink. Quick gate: ran 158, unchanged (no test added). Checked before the push with `g++ 13 -fsyntax-only -Wall -Wextra -Werror` and `-Wconversion` against the OBS 30.0.2 headers and the Qt 6.4.2 headers of Ubuntu 24.04's `qt6-base-dev` (unpacked into a scratch directory, not installed); the check caught the `signals` macro.

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/controller.cpp` (`ObsCallbacks`, `on_collection_changing`, `load_selection`, `on_exit`, `on_source_removed`, `on_source_renamed`, `watch_selection`, `refresh`), `src/obs/selection-store.hpp` (`forget`), `src/plugin-main.cpp` and `CMakeLists.txt`; the gate is the CI build, and every event is walked on the rig in T34. Check C: no test was added or changed; the rules the controller calls are the core's, under their own tests. Verdict: criteria covered by the build; the rig checks are open.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): drive the session from OBS events`

---

#### T33: Replace the one-line dock with the measuring dock

**What**: The Qt dock renders `DockView` and forwards clicks to the controller; it replaces Foundation's label.
**Where**: `src/camera-sync-dock.cpp`
**Depends on**: T32, T22
**Reuses**: `camera_sync_register_dock`, `obs_frontend_add_dock_by_id`, `obs_module_text`, `ViewSink`
**Requirement**: READ-70, READ-71, READ-72, READ-73, READ-74, READ-75

**Done when**:

- [x] The dock shows a camera checklist (checkbox, name, status, and the detail text from T23 when on), a mic picker, a link "Pattern files" to `https://github.com/norihiro/obs-audio-video-sync-dock/blob/main/README.md` opened in the system browser, Start/Stop, Reset, a Details check box and a wrapped message line, and no Apply or Undo. The checklist is a `QTreeWidget` with three columns (ticked name, status, details), the details column hidden while Details is off; the picker a `QComboBox`, with no entry chosen while the view chooses none; the link a rich-text `QLabel` with `setOpenExternalLinks`; the message line hidden while the view has no message.
- [x] Ticking, choosing and pressing only call the controller; `render` fills widgets from the view with signals blocked and updates items in place. Each click calls one controller method (`set_camera_ticked`, `choose_mic`, `start_or_stop`, `reset`, `set_details`). The rows and the mic entries are rebuilt only when their sources or order change, so a tick or an open list under the cursor is not rebuilt every 100 ms. The dock is the controller's `ViewSink`, handed over with its widget, which the controller watches through a `QPointer` (T32); `camera_sync_register_dock` now takes the controller.
- [x] Every user-visible string comes from `obs_module_text` with the keys from T20; the dock title key is unchanged. The view carries the row texts, the button label and the message; the fixed labels (Mic, Pattern files, Details, Reset) come from their keys in `strings.hpp`. The title is still `CameraSync.Dock.Title`.
- [x] The Foundation label and its "Camera Sync 0.1.0" text are gone; `data/locale/en-US.ini` is complete. The ini already held every key since T20, so it is unchanged.
- [x] CI gate passes on all three platforms, with the packages attached. Quick gate: ran 158, unchanged (no test added). Checked before the push with the T32 `g++ -fsyntax-only` and `-Wconversion` runs against the OBS 30.0.2 and Qt 6.4.2 headers. The packages and the rig walk of the dock (READ-70 to READ-76) are T34's.

**Test Adequacy (no tests added, Check B and D vacuous):** Check A: the bullets map to `src/camera-sync-dock.cpp` (`CameraSyncDock`, `render`, `render_cameras`, `render_mics`, `camera_sync_register_dock`), `src/camera-sync-dock.hpp` and `src/plugin-main.cpp`; the gate is the CI build, and the dock is compared with READ-70 on the rig in T34. Check C: no test was added or changed; every text and state the dock shows is the view-model's, under its T21 to T23 tests. Verdict: criteria covered by the build; the rig check is open.

**Tests**: none
**Gate**: ci

**Commit**: `:sparkles: feat(reading): add the measuring dock`

---

### Phase 9 tasks: Rig

#### T34: Rig session 1: load, controls and every Reading row

**What**: Install the CI packages in OBS 30.0.0 and the latest stable and walk the dock's rows and controls.
**Where**: `.specs/features/reading/validation.md` (rig record)
**Depends on**: T33, T26
**Reuses**: the Foundation T16 procedure; the design's Reading state table
**Requirement**: READ-76, READ-77, READ-78, READ-81, READ-82, READ-27, READ-35, READ-55, READ-56, READ-57, READ-66, READ-67

**Done when**:

- [ ] OBS 30.0.0 and the latest stable (version recorded) list "Camera Sync" under Docks and open the dock with the controls of T33 (READ-76).
- [ ] After a Start, Stop and Reset and closing OBS, each OBS's last "Number of memory leaks" count equals its count without the plugin (READ-77).
- [ ] Both cameras reach Stable in the same measuring pass (READ-78); hiding the phone from one camera leaves it "Looking for the pattern" while the other measures (READ-81).
- [ ] Each Reading row of the design's table is exercised once in each OBS and the dock shows the row's text (READ-82): pick prompt, looking, no sound, settling, stable late and early, pattern lost keeps the value, camera removed, camera renamed, camera unplugged, Stop, Reset, collection switch.
- [ ] Evidence recorded: platform, both OBS versions, devices, canvas size and fps, and a screenshot or log excerpt per row. Any failure becomes a fix task before T35.

**Tests**: none
**Gate**: rig

**Commit**: `:memo: docs(reading): record the rig walk of every reading row`

---

#### T35: Rig session 2: do the readings match Program

**What**: Compare the plugin's readings with Sync Dock's for each camera on Program, with a Render Delay on one camera, and check what measuring leaves behind.
**Where**: `.specs/features/reading/validation.md` (rig record)
**Depends on**: T34
**Reuses**: the TDD's rig protocol
**Requirement**: READ-61, READ-62, READ-63, READ-64, READ-65, READ-69, READ-79, READ-80

**Done when**:

- [ ] For each camera put on Program with Sync Dock running, the plugin's stable reading equals minus Sync Dock's latency within one frame, in three runs in a row, with one camera carrying a Render Delay and the other a Video Delay (Async) filter (READ-61, READ-79).
- [ ] The mic's reading changes by the amount of a Sync Offset set by hand (READ-62).
- [ ] A camera in no scene reaches a reading (READ-63).
- [ ] The scene collection saved while measuring differs from the one saved while idle only in the plugin's module data, shown by a diff (READ-64, READ-65).
- [ ] With the phone in view and audible, each camera is Stable within 5 s (READ-80); one camera slowed on purpose does not delay the other's rows (READ-69).
- [ ] Debug logs of the runs are saved with the OBS version, canvas, devices and phone distances. A failed claim of the three in T30 is a fix task confined to `src/obs/`, and T35 reruns.

**Tests**: none
**Gate**: rig

**Commit**: `:memo: docs(reading): record the rig comparison with sync dock`

---

#### T36: Rig session 3: performance

**What**: Measure CPU, dropped frames and memory with up to four cameras.
**Where**: `.specs/features/reading/validation.md` (rig record)
**Depends on**: T35
**Reuses**: the TDD's Performance Requirements
**Requirement**: READ-93, READ-94, READ-95, READ-96

**Done when**:

- [ ] At most 5% of one core per measured camera at 60 fps (READ-93).
- [ ] With four cameras on a 1080p canvas at 30 and at 60 fps, "frames missed due to rendering lag" over five minutes of measuring does not exceed five minutes without (READ-94).
- [ ] Over 30 minutes the plugin's memory does not grow (READ-95).
- [ ] The dock refreshes about ten times a second and a profile shows no marker detection on the UI thread (READ-96).
- [ ] A figure over its bound becomes a fix task or a recorded spec amendment, not a silent pass.

**Tests**: none
**Gate**: rig

**Commit**: `:memo: docs(reading): record the rig performance figures`

---

#### T37: Replay a rig run's markers through the estimator

**What**: Commit one rig run's Debug marker log as a fixture and a test that replays it.
**Where**: `tests/reading/rig-replay.cpp`
**Depends on**: T35, T6
**Reuses**: the Debug marker lines from T19; `ReadingEstimator`
**Requirement**: READ-83

**Done when**:

- [ ] The marker log, with the steps that produced it, is committed under `tests/fixtures/rig/`.
- [ ] The test parses the log, feeds the estimator and asserts the states and readings the run showed on the dock (values copied from the run's record).
- [ ] Added doctest cases: 1. Quick gate passes.

**Tests**: unit
**Gate**: quick

**Commit**: `:white_check_mark: test(reading): replay a rig run through the estimator`

---

## Phase Execution Map

```
Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6 → Phase 7 → Phase 8 → Phase 9

Phase 1:  T1, T2
Phase 2:  T3 → T4 → T5 → T6
Phase 3:  T7, T8, T9 → T10 → T11
Phase 4:  T12, T13
Phase 5:  T14 → T15 → T16 → T17 → T18 → T19
Phase 6:  T20 → T21 → T22 → T23
Phase 7:  T24, T25 → T26
Phase 8:  T27, T28, T29, T30 → T31 → T32 → T33
Phase 9:  T34 → T35 → T36, T37
```

Execution is strictly sequential, in task-number order. The 37 tasks pack into about five batches of whole phases: phases 1 and 2 (6 tasks), 3 and 4 (7), 5 (6), 6 and 7 (7), 8 (7), and 9 (4) on the rig, which is not delegated.

---

## Task Granularity Check

`validate_tasks.py` warns on every `Tests: none`. Each `none` matches a matrix layer whose test is the CI build or the rig. Where a task names several files it is one deliverable with its header.

| Task | Scope | Status |
| ---- | ----- | ------ |
| T1: QrPayload transactional | One function and its tests | ✅ Granular |
| T2: Pattern arithmetic | Three small functions and a header | ⚠️ Cohesive: none is useful alone |
| T3: Pairing and windows | One class, pairing only | ⚠️ One large unit; stability is split off into T4 |
| T4: Stability | One rule | ✅ Granular |
| T5: Thread safety | One lock | ✅ Granular |
| T6: Counters | One feature | ✅ Granular |
| T7: Camera pipeline | One class | ✅ Granular |
| T8: Mic pipeline | One class and a shared test helper | ✅ Granular |
| T9: Video fixtures | One generator change, its files and one test | ⚠️ Fixtures and test are inseparable |
| T10: Sync check | One test | ✅ Granular |
| T11: Offsets replay | One test | ✅ Granular |
| T12: Selection rules | One module of pure functions | ⚠️ Several functions, one file |
| T13: Log lines | One module | ✅ Granular |
| T14 to T19: Session | One class in six steps, each one behavior group | ✅ Granular |
| T20 to T23: View-model | One module in four steps | ✅ Granular |
| T24: Coverage gate | One CI step and its build switch | ⚠️ Cohesive |
| T25: Fuzz harness | One harness and its corpus | ✅ Granular |
| T26: Fuzz in CI | One CI step | ✅ Granular |
| T27 to T31: Adapters | One adapter each | ✅ Granular |
| T32: Controller | One glue class | ⚠️ Several wires, one object |
| T33: Dock | One widget | ✅ Granular |
| T34 to T36: Rig sessions | One rig visit each | ✅ Granular |
| T37: Rig replay | One test and its fixture | ✅ Granular |

---

## Diagram-Definition Cross-Check

| Task | Depends On (task body) | Diagram Shows | Status |
| ---- | ---------------------- | ------------- | ------ |
| T1 | None | Standalone | ✅ Match |
| T2 | None | Standalone | ✅ Match |
| T3 | T2 (Phase 1) | Start of Phase 2 | ✅ Match |
| T4 | T3 | T3 → T4 | ✅ Match |
| T5 | T4 | T4 → T5 | ✅ Match |
| T6 | T5 | T5 → T6 | ✅ Match |
| T7 | T1 (Phase 1) | Standalone in Phase 3 | ✅ Match |
| T8 | None | Standalone in Phase 3 | ✅ Match |
| T9 | T7 | T7 → T9 | ✅ Match |
| T10 | T8, T9 | T8 → T10, T9 → T10 | ✅ Match |
| T11 | T6 (Phase 2), T10 | T10 → T11 | ✅ Match |
| T12 | None | Standalone | ✅ Match |
| T13 | None | Standalone | ✅ Match |
| T14 | T6 (Phase 2), T7, T8 (Phase 3) | Start of Phase 5 | ✅ Match |
| T15 | T14 | T14 → T15 | ✅ Match |
| T16 | T15 | T15 → T16 | ✅ Match |
| T17 | T16 | T16 → T17 | ✅ Match |
| T18 | T17 | T17 → T18 | ✅ Match |
| T19 | T18, T13 (Phase 4) | T18 → T19 | ✅ Match |
| T20 | T14 (Phase 5) | Start of Phase 6 | ✅ Match |
| T21 | T20, T4 (Phase 2), T12 (Phase 4) | T20 → T21 | ✅ Match |
| T22 | T21, T17 (Phase 5) | T21 → T22 | ✅ Match |
| T23 | T22, T6 (Phase 2) | T22 → T23 | ✅ Match |
| T24 | T23 (Phase 6) | T24 → T26 | ✅ Match |
| T25 | T1 (Phase 1) | T25 → T26 | ✅ Match |
| T26 | T24, T25 | T24 → T26, T25 → T26 | ✅ Match |
| T27 | T12 (Phase 4) | T27 → T32 | ✅ Match |
| T28 | T12 (Phase 4) | T28 → T32 | ✅ Match |
| T29 | T14 (Phase 5) | T29 → T31 | ✅ Match |
| T30 | T14 (Phase 5) | T30 → T31 | ✅ Match |
| T31 | T29, T30 | T29 → T31, T30 → T31 | ✅ Match |
| T32 | T19, T23 (earlier phases), T27, T28, T31 | T27 → T32, T28 → T32, T31 → T32 | ✅ Match |
| T33 | T32, T22 (Phase 6) | T32 → T33 | ✅ Match |
| T34 | T33 (Phase 8), T26 (Phase 7) | T34 → T35 | ✅ Match |
| T35 | T34 | T34 → T35 | ✅ Match |
| T36 | T35 | T35 → T36 | ✅ Match |
| T37 | T35, T6 (Phase 2) | T35 → T37 | ✅ Match |

---

## Test Co-location Validation

| Task | Code Layer Created/Modified | Matrix Requires | Task Says | Status |
| ---- | --------------------------- | --------------- | --------- | ------ |
| T1: QrPayload | Vendored detector | unit | unit | ✅ OK |
| T2: Pattern | Core logic | unit | unit | ✅ OK |
| T3 to T6: Estimator | Core logic | unit | unit | ✅ OK |
| T7, T8: Pipelines | Core logic | unit | unit | ✅ OK |
| T9: Video fixtures | Test fixtures | unit | unit | ✅ OK |
| T10, T11: Sync and offsets | Test fixtures, core logic | unit | unit | ✅ OK |
| T12, T13: Selection, log lines | Core logic | unit | unit | ✅ OK |
| T14 to T19: Session | Core logic | unit | unit | ✅ OK |
| T20 to T23: View-model | Core logic | unit | unit | ✅ OK |
| T24: Coverage gate | CI workflows | build | none (ci gate) | ✅ OK |
| T25: Fuzz harness | Fuzz harness | unit | unit | ✅ OK |
| T26: Fuzz in CI | CI workflows | build | none (ci gate) | ✅ OK |
| T27 to T33: Adapters, controller, dock | Plugin (`src/`) | build + UAT | none (ci gate; UAT in T34 to T36) | ✅ OK |
| T34 to T36: Rig | Plugin (`src/`) | UAT | none (rig gate; the rig is the test) | ✅ OK |
| T37: Rig replay | Core logic | unit | unit | ✅ OK |

T27 to T33 defer their UAT to the rig phase so the rig is needed three times, not once per task. That is the matrix's own plan for the plugin layer, as in Foundation: each task carries the CI build gate, and every rule in them lives in the core under unit tests.

---

## Requirement Coverage

| Requirement | Tasks |
| ----------- | ----- |
| READ-01, READ-02, READ-03, READ-04, READ-05, READ-06 | T3 |
| READ-07, READ-08 | T5 |
| READ-09, READ-10 | T3 |
| READ-11 | T3, T4 |
| READ-12, READ-13, READ-14, READ-15 | T4 |
| READ-16 | T2, T4, T14 |
| READ-17, READ-18 | T15 |
| READ-19 | T8 |
| READ-20 | T9 |
| READ-21 | T10 |
| READ-22 | T11 |
| READ-23 | T1 |
| READ-24, READ-28, READ-29, READ-30, READ-31 | T14 |
| READ-25, READ-26, READ-27 | T15 |
| READ-32, READ-33, READ-34, READ-35 | T16 |
| READ-36, READ-45, READ-46, READ-47 | T22 |
| READ-37, READ-38, READ-39, READ-40, READ-41, READ-44 | T21 |
| READ-42, READ-43 | T17, T22 |
| READ-48 | T20 |
| READ-49, READ-50, READ-51, READ-52, READ-53, READ-60 | T12, T27 |
| READ-54 | T12, T28 |
| READ-55, READ-56, READ-57, READ-58, READ-59 | T18, T32 |
| READ-61, READ-62, READ-63, READ-64, READ-65, READ-66 | T29, T30, T31, T35 |
| READ-67, READ-68 | T14, T29, T30 |
| READ-69 | T30, T35 |
| READ-70, READ-71, READ-72, READ-73, READ-74, READ-75 | T33 |
| READ-76, READ-77, READ-78, READ-81, READ-82 | T34 |
| READ-79, READ-80 | T35 |
| READ-83 | T37 |
| READ-84, READ-85, READ-86 | T13, T19 |
| READ-87, READ-89 | T19 |
| READ-88 | T13 |
| READ-90 | T24 |
| READ-91 | T26 |
| READ-92 | T25 |
| READ-93, READ-94, READ-95 | T36 |
| READ-96 | T32, T36 |
| READ-97 | T23 |
| READ-98 | T6 |

All 98 requirements map to at least one task.
