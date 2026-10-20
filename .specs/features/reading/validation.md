# Reading Validation

**Date**: 2026-10-20
**Spec**: `.specs/features/reading/spec.md` (READ-01 to READ-98)
**Diff range**: `cece348..8fab0aa`. Iteration 2 is `7156f1f..8fab0aa` (4 commits): `f578985` (READ-57 untick), `13178fc` (READ-42/43 restart boundary), `8fab0aa` (spec clarifications for READ-43 and READ-51, traceability table, task notes). It changes only `tests/reading/session.cpp`, `spec.md` and `tasks.md`; no file under `core/`, `src/`, `data/` or `.github/` changed (`git diff 7156f1f..HEAD --stat -- core src` is empty).
**Verifier**: standalone fresh-eyes pass in the orchestrating session (no sub-agent was spawned, as the user did not ask for one). It did not write the iteration 2 fixes or the iteration 0 and 1 reports.
**Scope**: tasks T1 to T33 only. The rig tasks T34 to T37, and every AC whose proof needs OBS, the rig or a CI run, are NOT covered by this verdict; they are listed under "Rig, UAT and CI-only ACs" with the code that implements them.

**History**: iteration 0 (at `91dca3d`, 158 tests) was FAIL: M51 and E2 survived. Iteration 1 (at `7156f1f`, 162 tests) closed those and found F5 and F6. Iteration 2 (this report, at `8fab0aa`, 164 tests) closes F5 and F6.

## Validation: Reading (T1 to T33) - PASS ✅

The gate is green at 164 of 164. F5 (`core/reading/session.cpp:228`, the 2 s rule calls `drop_camera` instead of `untick`) is killed by `tests/reading/session.cpp:1196` ("session unticks a camera that delivers no frame for 2 s"). F6 (`core/reading/session.cpp:256`, `>=` to `>`) is killed by "session starts the wait for pattern sound over when video markers resume 5 s after the last". Three fresh mutants around the same rules are killed. The two spec clarifications in `8fab0aa` close SP3 and SP10. Open, and outside this verdict: T26's red run of the fuzz CI step (`tasks.md:751`, READ-91, CI only) and the rig tasks T34 to T37.

---

## Task Completion

| Task | Status | Notes |
| ---- | ------ | ----- |
| T1 | ✅ Done | Iteration 1 adds the detector-level proof: M51 is killed by `tests/detector/video-detector.cpp:256` |
| T2 | ✅ Done | Half wrap uses the payload's integer q (25.344 s, not the spec's 25.6 s), noted in the task (SP9) |
| T3 | ✅ Done | `SPEC_DEVIATION` in `core/reading/estimator.hpp:90`: audio markers stay listed and pair once per camera |
| T4 to T8 | ✅ Done | - |
| T9 | ✅ Done | Iteration 1 adds `qr-frame-f9.pgm` (57,615 bytes); `git diff 91dca3d..7156f1f -- tests/fixtures` adds files only, so the three older fixtures and `video-cycles/` are unchanged. The byte-for-byte regeneration claim was not re-run here (it needs Pillow and qrcode; nothing is installed) |
| T10 | ✅ Done | Iteration 1 adds the per-marker check against the generated time (READ-21) |
| T11 | ✅ Done | Iteration 1 checks all four samples of each camera (READ-22) |
| T12 | ✅ Done | Camera-wins rule now stated in READ-51 (SP3 closed) |
| T13 | ✅ Done | - |
| T14 | ✅ Done | `SPEC_DEVIATION` in `core/reading/session.hpp:71`: constructor takes `name_of`, `start` takes the frame rate |
| T15, T16 | ✅ Done | - |
| T17 | ✅ Done | Start's own restart tested (E2 killed). Iteration 2 adds the restart boundary when video resumes at 5 s (F6 killed) |
| T18 | ✅ Done | Iteration 2 adds the untick proof (F5 killed) |
| T19 to T23 | ✅ Done | - |
| T24 | ✅ Done | Coverage 637 of 654 lines (97.40%) was reproduced at `91dca3d`; no core file changed since and tests only grew. CI run not re-read (no `gh`) |
| T25 | ✅ Done | M49 (f upper bound removed) aborts `fuzz_corpus_replay` (iteration 0) |
| T26 | ⚠️ Partial | Unchanged open box in `tasks.md:751`: the fuzz step's red run on a broken range check was never made |
| T27 to T33 | ✅ Done (build) | CI-gated adapters and dock; behaviour waits for the rig (T34, T35). `SPEC_DEVIATION` in `src/obs/camera-tap.hpp:24`, maintainer-approved 2026-10-16 |
| T34 to T37 | Not in scope | Rig tasks, not started |

---

## Spec-Anchored Acceptance Criteria

Paths are under `tests/` unless written otherwise. "PASS" means the asserted value is the spec-defined outcome. Rows re-derived in iteration 1 are marked **(re-derived)**; the others are carried from iteration 0 with their citations re-checked at `7156f1f` (lines in `tests/reading/session.cpp` after line 1009 moved down by 19, lines in `tests/detector/video-detector.cpp` by 5).

### Story 1: pairing (estimator)

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-01 | V 1.142 s, A 1.000 s, same index → +142; positive when video is late | `tests/reading/estimator.cpp:100` `row(estimator, "cam").live_ms == 142`; `:107` `live_ms == -142` | ✅ PASS (M53 killed) |
| READ-02 | Different index, or half a wrap or more apart → no sample | `tests/reading/estimator.cpp:122` `samples == 0`; `:139` `row(estimator, "half").samples == 0` at exactly half a wrap, `:140` `"under"` `samples == 1`; `tests/reading/session.cpp:525-528` index_max 16: 1,584 ms Looking, 1,583 ms Settling | ✅ PASS (M01, F1 killed) |
| READ-03 | Several audio markers of the index → nearest | `tests/reading/estimator.cpp:165`, `:172`, `:179` `live_ms == 142`, `:181` | ✅ PASS (M02 killed) |
| READ-04 | Audio first or video first → same sample | `tests/reading/estimator.cpp:200-201` | ✅ PASS (M03 killed) |
| READ-05 | One window per camera; no cross-camera sample | `tests/reading/estimator.cpp:222-227`, `:230-234` | ✅ PASS |
| READ-06 | More than 256 markers → oldest dropped | `tests/reading/estimator.cpp:274`, `:278-279` video; `:294`, `:298-299` audio | ✅ PASS (M07, M08, F10 killed) |
| READ-07 | Concurrent pushes all recorded | `tests/reading/estimator.cpp:747-749` `samples_formed == flashes`, 4 threads | ✅ PASS |
| READ-08 | Snapshot is one instant | `tests/reading/estimator.cpp:826-830` `mixed == 0` | ✅ PASS |

### Story 2: camera state (estimator)

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-09 | No sample → Looking | `tests/reading/estimator.cpp:308-309`, `:313` | ✅ PASS |
| READ-10 | First sample → Settling, live = sample rounded | `tests/reading/estimator.cpp:318` `live_ms == 142` (141.5), `:323` `-142`, `:325-326` | ✅ PASS (F3 killed) |
| READ-11 | Never stable and < 5 samples or span > F → Settling with latest rounded | `tests/reading/estimator.cpp:340-342`; `:598-599` | ✅ PASS (M40, R22a killed) |
| READ-12 | Span ≤ F, F included → Stable at median, halves away from zero | `tests/reading/estimator.cpp:470-472` span 33.0 Stable at 120, 33.01 Settling; `:502-504` `reading_ms == expected` (±142) | ✅ PASS (M04, M05, M06, F2, F4 killed) |
| READ-13 | Stable, later span > F → reading unchanged | `tests/reading/estimator.cpp:542-543` | ✅ PASS (M09 killed) |
| READ-14 | Stable, samples stop → unchanged | `tests/reading/estimator.cpp:560-561` | ✅ PASS |
| READ-15 | Stable, new window within F → median of new five | `tests/reading/estimator.cpp:578`, `:581-582` | ✅ PASS (F4 killed) |
| READ-16 | F = 1000 × fps_den ÷ fps_num at Start | `tests/reading/session.cpp:304-307`; `tests/reading/pattern.cpp:56-59` | ✅ PASS (M28 killed) |

### Story 3: one pattern, decoders

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-17 | First valid payload sets the mic's (f, c, q) and the estimator's wrap | `tests/reading/session.cpp:504-507`; `:521` `missed == 2` with index_max 16; `:525-528` | ✅ PASS (F11 killed) |
| READ-18 | Payload with another f, c, q or index_max → markers ignored | `tests/reading/session.cpp:558-560` four subcases | ✅ PASS (M23 killed) |
| READ-19 | No pattern → no audio marker | `tests/reading/mic-pipeline.cpp:56` `found.empty()`; `tests/reading/session.cpp:676-678` | ✅ PASS |
| READ-20 | Each video marker: generated index, time within 1 ms of generated | `tests/detector/video-cycles.cpp:45` `markers[k].qr_data.index == k`, `:48` `distance(markers[k].timestamp, generated_ns) <= one_ms` | ✅ PASS |
| READ-21 **(re-derived)** | Each audio marker of the clip: generated index, time within 1 ms of the generated time | `tests/detector/pattern-sync.cpp:84` `REQUIRE(audio.size() == generated.size())` (10, from `audio-indices.txt`, the generator's own list); `:87` `audio[k].index == generated[k]`; `:88` `generated_ns = pattern_frame_ns(0, k * pattern_cycle_frames + pattern_marker_frame)` (frame 2q of cycle k, the generator's sound start, independent of any decoder); `:91` `std::abs(error_ns) <= 1000000` | ✅ PASS. Every marker, indices 0 to 9, against the generator's time, not the video decoder's. R21 (index 5 stamped 2 ms late) is killed by `:91` alone, with `error_ns := 2054399` |
| READ-22 **(re-derived)** | Offsets 100 and 40 ms → samples +100 ± 1 and +40 ± 1 | `tests/reading/estimator-replay.cpp:151` `REQUIRE(samples_a.size() == 4)`, `:153` `std::abs(sample - 100) <= 1` for every sample; `:155`, `:157` `std::abs(sample - 40) <= 1`; `:139`, `:145` `samples == 4` (own window only) | ✅ PASS. All four samples, recorded as each forms (`:71-78`). R22a (second sample 3 ms off) and R22b (first sample 3 ms off) are each killed by `:153` and `:157`, which the latest-only check at `:141` could not see. Note: the bound is on the rounded live value, so a raw sample is bounded to ±1.5 ms here; the 1 ms precision of the markers that form it is asserted directly by READ-20 and READ-21 |
| READ-23 **(re-derived)** | Malformed or out-of-range payload: ignored, Debug log, no marker, no state change | Detector level: `tests/detector/video-detector.cpp:255` `shown.logged == LogLines(qr_frames, {LogLevel::Debug, "f: out of range: 9"})`, `:256` `shown.markers.size() == 1` (no marker after a valid cycle's window), `:258-260` `payload.valid`, `payload.f == 884`, `payload.index == 0`; fresh detector `:234-236` `markers.empty()`, `CHECK_FALSE(qr_data().valid)`. Payload level: `:152-159` every field unchanged in 10 subcases (f=9, f=32001, c=0, c above f, q=0, q=1001, i=256, no i, two malformed keys), `:178` `levels == {LogLevel::Debug}`, `:98` | ✅ PASS (M51 killed by `:256`, `CHECK( 2 == 1 )`). The fresh-detector case passes under M51 (an invalid payload never arms the window), so the second case carries the proof. `qr-frame-f9.pgm` is shown to carry f=9 by the logged message itself |

### Story 4: session lifecycle

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-24 | Start → Measuring, one tap each | `tests/reading/session.cpp:266-269` `requests == {"mic:mic","camera:cam-a","camera:cam-b"}` | ✅ PASS |
| READ-25 | Stop → Stopped, taps released, readings and pattern kept | `tests/reading/session.cpp:581-588`, `:615-616` | ✅ PASS (M11, M12 killed) |
| READ-26 | Start from Stopped → readings unchanged | `tests/reading/session.cpp:603-612`, `:622-623` | ✅ PASS |
| READ-27 | Reset → cleared, Looking | `tests/reading/session.cpp:649-654`, `:660-661`, `:686-688` | ✅ PASS (M13 killed) |
| READ-28 | Collection changing → taps released, cleared, Idle | `tests/reading/session.cpp:410-413`, `:418-422`. Loading the new selection is `src/controller.cpp:219-225`, rig-only | ✅ PASS (core half; M27 killed) |
| READ-29 | Exit → every tap released | `tests/reading/session.cpp:434` `h.factory.live.empty()`; ordering is `src/controller.cpp:227-236`, rig-only | ✅ PASS (core half) |
| READ-30 | Start with nothing chosen → unchanged | `tests/reading/session.cpp:356-359` | ✅ PASS (M15 killed) |
| READ-31 | Stop in Idle/Stopped, Reset in Idle → unchanged | `tests/reading/session.cpp:445-448`, `:456-460`; no log line `:1333-1335` | ✅ PASS (M14 killed) |
| READ-32 | Tick while Measuring → tap, Looking | `tests/reading/session.cpp:725-732`, `:744-748` | ✅ PASS (M25 killed) |
| READ-33 | Untick → its tap and reading go, others unchanged | `tests/reading/session.cpp:773-782` | ✅ PASS |
| READ-34 | Other mic → cleared; Measuring: mic tap replaced, Looking | `tests/reading/session.cpp:798-804`, `:812-813`, `:827-830` | ✅ PASS (M24 killed) |
| READ-35 | Mic removed → released, cleared, Idle | `tests/reading/session.cpp:884-889`, `:895-896` | ✅ PASS (E1 equivalent) |

### Story 5: view-model

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-36 | No camera or no mic → Start disabled, "Pick at least one camera and the mic" | `tests/viewmodel/view-model.cpp:180-181`, `:188-189` | ✅ PASS (SP2 for Measuring) |
| READ-37 | Looking → "Looking for the pattern" | `tests/viewmodel/view-model.cpp:85` | ✅ PASS |
| READ-38 | Settling → signed live + " ms - settling" | `tests/viewmodel/view-model.cpp:90` `"+142 ms - settling"` | ✅ PASS (M40 killed; SP1) |
| READ-39 | Stable > 0 → "+142 ms - video late" | `tests/viewmodel/view-model.cpp:99` | ✅ PASS |
| READ-40 | Stable < 0 → "−60 ms - video early", U+2212 | `tests/viewmodel/view-model.cpp:104`; `:79-80` | ✅ PASS (M38, F12 killed) |
| READ-41 | Stable 0 → "0 ms - in sync" | `tests/viewmodel/view-model.cpp:109` | ✅ PASS (M39 killed) |
| READ-42 **(iteration 2)** | 5 s since the first video marker after the last audio marker or Start, video within 5 s → "No pattern sound from {mic} - ..." | `tests/reading/session.cpp:932-933` false at 4,999, true at 5,000; Start's own count: `:1047` `CHECK_FALSE(no_sound_at(h, 5000))` after `shutdown()` at 4.6 s and `start` at 4.7 s, `:1049-1050` false at 9,999 and true at 10,000; restart at 5 s: `:981` `CHECK(no_sound_at(h, 9799))` for a marker 4.999 s after the last, `:986` `CHECK_FALSE(no_sound_at(h, 9800))` for one exactly 5 s after, `:988-989` false at 14,799 and true at 14,800; `tests/viewmodel/view-model.cpp:227-228` exact text | ✅ PASS (M18, M21, E2, F6, F15 killed). F6 is killed by the 5 s subcase alone, F15 (never starts over) by both new cases |
| READ-43 **(iteration 2)** | Cleared by an audio marker, 5 s without video, or leaving Measuring (Start restarts); the wait starts over from the next video marker | `tests/reading/session.cpp:946`, `:948-949`; `:960-961` true at 9,799, false at 9,800; the restart: `:969-991`; `:1047` Start; `tests/viewmodel/view-model.cpp:231` | ✅ PASS (M19, M20, M22, E2, F6, F15, F16 killed). The spec now states the restart (`spec.md:193`) |
| READ-44 | Unticked or dropped → no reading or status | `tests/viewmodel/view-model.cpp:122-123`, `:134`, `:301` | ✅ PASS (M42 killed) |
| READ-45 | "Stop" while Measuring, "Start" otherwise | `tests/viewmodel/view-model.cpp:198`, `:206` | ✅ PASS (M43 killed) |
| READ-46 | Reset off in Idle, on otherwise | `tests/viewmodel/view-model.cpp:218-220` | ✅ PASS (M41 killed) |
| READ-47 | Idle/Stopped with choice → Start enabled | `tests/viewmodel/view-model.cpp:197` | ✅ PASS |
| READ-48 | Every string through an en-US key; every key present | `tests/viewmodel/strings.cpp:45`, `:56`, `:85` | ✅ PASS |

### Story 6: selection (core half)

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-49 | Async video as cameras, audio as mics, sorted ignoring case | `tests/selection/selection-policy.cpp:64`, `:75`; flags `src/obs/obs-catalog.cpp:63-64`, rig-only | ✅ PASS (core half; M36 killed) |
| READ-50 | No saved selection → tick Program's cameras | `tests/selection/selection-policy.cpp:83`; nested scan `src/obs/obs-catalog.cpp:24-44`, rig-only | ✅ PASS (core half; M34 killed) |
| READ-51 | Exactly one audio source → the mic | `tests/selection/selection-policy.cpp:93`, `:112` | ✅ PASS (M31 killed; SP3) |
| READ-52 | Version 1 → only surviving UUIDs, no preselection | `tests/selection/selection-policy.cpp:123-124`, `:137-138`, `:143-144` | ✅ PASS (F7 killed) |
| READ-53 | Version ≠ 1 → as no saved selection | `tests/selection/selection-policy.cpp:168-169` | ✅ PASS (M33 killed) |
| READ-54 | Module data shape, mic left out when none | `tests/selection/selection-policy.cpp:206-208`, `:213-215`; JSON `src/obs/selection-store.cpp:39-63`, rig-only | ✅ PASS (core half) |
| READ-55 | Removed ticked camera → reading and tap dropped | `tests/reading/session.cpp:1129-1133` | ✅ PASS (core half) |
| READ-56 | Renamed → dropped, unticked | `tests/reading/session.cpp:1159-1162`; unticked shown by ticking it again: `:1165-1167` Looking, 0 samples | ✅ PASS (core half) |
| READ-57 **(iteration 2)** | No frame for 2 s while Measuring → reading dropped, tap released, **unticked**, catalog keeps it listed | Dropped and released: `tests/reading/session.cpp:1203-1208` kept at 1,999, `CHECK_FALSE(h.has_row("cam-a"))` and `h.factory.live == {"mic:mic","camera:cam-b"}` at 2,000. Unticked: `:1220` "session unticks a camera that delivers no frame for 2 s": ticking it again gives a Looking row, 0 samples and a fourth `camera:cam-a` request; Stop then Start requests only `mic:mic` and `camera:cam-b` and shows no `cam-a` row; stamps `tests/reading/camera-pipeline.cpp:147`, `:152`, `:156` | ✅ PASS (M16, M17, M52, F5, F13 killed). F5 is killed by the new case only |
| READ-58 | Filter added or removed → that camera Looking | `tests/reading/session.cpp:1254-1258` | ✅ PASS (M29 killed) |
| READ-59 | Mic Sync Offset changed → every camera Looking | `tests/reading/session.cpp:1272-1275`, `:1279` | ✅ PASS (M30 killed) |
| READ-60 | Mic picker omits ticked cameras; checklist omits the mic | `tests/selection/selection-policy.cpp:181-184`, `:197-198`; `tests/viewmodel/view-model.cpp:145-148`, `:265-270` | ✅ PASS (M32, M35, F8 killed) |

### Story 7, core half; stories 10, 11 and 13

| AC | Spec-defined outcome | `file:line` + assertion | Result |
| -- | -------------------- | ----------------------- | ------ |
| READ-67 | Camera tap fails → others measured, Looking, warning naming it | `tests/reading/session.cpp:371-372`, `:375-377` | ✅ PASS (SP4) |
| READ-68 | Mic tap fails → not Measuring, released, warning, "Could not listen to {mic}" | `tests/reading/session.cpp:387-391`; `tests/viewmodel/view-model.cpp:239` | ✅ PASS |
| READ-84 | `measure.start cameras=<n> mic="<name>" canvas_fps=<fps>` at Info | `tests/reading/session.cpp:1315-1316`; `tests/core/log-lines.cpp:16`, `:19-23` | ✅ PASS (M46, M47 killed; SP5) |
| READ-85 | `measure.stop`, `measure.reset` at Info | `tests/reading/session.cpp:1333-1335`; `tests/core/log-lines.cpp:28-29` | ✅ PASS |
| READ-86 | `reading.stable camera=".." reading_ms=<signed> spread_ms=<int>` | `tests/reading/session.cpp:1367-1370`; `tests/core/log-lines.cpp:34-36`; `tests/reading/estimator.cpp:627-628` | ✅ PASS (M45, M48, F2 killed) |
| READ-87 | Debug: markers, payload parameters, skipped frames | `tests/reading/session.cpp:1386-1391`, `:1426-1428`; verbose gating `src/controller.cpp:33-34`, rig-only | ✅ PASS (SP6) |
| READ-88 | `"` in a name written `\"` | `tests/core/log-lines.cpp:42-46`; `tests/reading/session.cpp:1501-1503` | ✅ PASS (M44 killed) |
| READ-89 | No audio, image or user data beyond names | `tests/reading/session.cpp:1477`, `:1479`, `:1497` | ✅ PASS |
| READ-92 | CTest replays the seed corpus; fails on crash or accepted out-of-range value | `tests/CMakeLists.txt:59` `add_test(NAME fuzz_corpus_replay ...)`; `tests/fuzz/qr-payload-fuzz.cpp:27` `std::abort()` | ✅ PASS (M49 aborts the replay) |
| READ-97 | Details on → "received {n}, missed {m}" | `tests/viewmodel/view-model.cpp:298`, `:310` | ✅ PASS (F9 killed) |
| READ-98 | Missed = indexes skipped, across the wrap | `tests/reading/estimator.cpp:846-849`, `:883-884`, `:896-901` | ✅ PASS (M10, F14 killed) |

### Edge cases

- [x] Pattern kept after its camera leaves, until Reset: `tests/reading/session.cpp:703-706`, `:711-713`, `:1145-1148`.
- [x] Every camera loses the pattern: Stable kept (`tests/reading/estimator.cpp:560-561`), Looking stays Looking (`tests/reading/estimator-replay.cpp:173`).
- [x] Second pattern ignored until Reset: `tests/reading/session.cpp:558-560`, `:686-688`.
- [ ] Canvas rate change while measuring keeps F: no test (out of scope by the spec; F is set only in `start`).
- [x] Mic is not offered as a camera: `tests/selection/selection-policy.cpp:183`.
- [x] Saved selection names a gone source: `tests/selection/selection-policy.cpp:123-124`.
- [x] Start pressed twice: button toggles (`tests/viewmodel/view-model.cpp:206`; the toggle is `src/controller.cpp:184-195`).

### Rig, UAT and CI-only ACs (not judged here)

| AC | Why | Implementation seen |
| -- | --- | ------------------- |
| READ-61, READ-63 to READ-66 | Tap behaviour against libobs; rig T35 | `src/obs/camera-tap.cpp`, `src/obs/mic-tap.cpp` |
| READ-62 | Sync Offset added to mic time; rig T35 | `src/obs/mic-tap.cpp:146` |
| READ-69 | Skipping a slow camera's frames; rig T35 | `src/obs/camera-tap.cpp:172-175`; one pipeline per camera unit-tested at `tests/reading/camera-pipeline.cpp:170-173` |
| READ-70 to READ-75 | Qt dock; UAT T34 | `src/camera-sync-dock.cpp` |
| READ-76 to READ-83 | Story 9, rig T34 to T37 | - |
| READ-90 | CI coverage gate | `.github/workflows/build-project.yaml:113-115`; 97.40% reproduced at `91dca3d` |
| READ-91 | CI fuzz run | `.github/workflows/build-project.yaml:121-131`; red-run check open (T26) |
| READ-93 to READ-96 | Performance on the rig, T36 | 100 ms timer at `src/controller.cpp:147` |

Boundary check (iteration 0, unchanged since no core file changed): `core/` includes no libobs, frontend or Qt header.

### Spec-precision gaps

| # | AC | What the spec leaves open | What the code does |
| - | -- | ------------------------- | ------------------ |
| SP1 | READ-38 | Settling text for a live value of 0 or below | "0 ms - settling", "−60 ms - settling" (`tests/viewmodel/view-model.cpp:93-94`) |
| SP2 | READ-36, READ-47 | Start/Stop while Measuring with nothing chosen | Enabled, "Stop" (`tests/viewmodel/view-model.cpp:212-213`) |
| SP3 | READ-51, READ-60 | Closed in `8fab0aa`: `spec.md:216` says a ticked camera is not the mic | The camera wins, the mic is left unset (`tests/selection/selection-policy.cpp:187-198`) |
| SP4 | READ-67, READ-68 | Warning text | `tap.failed camera="<name>"`, `tap.failed mic="<name>"` |
| SP5 | READ-84 | Format of `<fps>` | Two decimals, rounded, trailing zeros trimmed |
| SP6 | READ-87 | Debug line formats | `marker.video`, `marker.audio`, `marker.ignored`, `frames.skipped` key=value lines |
| SP7 | READ-43 | Whether another mic restarts the no-sound wait | It does (`tests/reading/session.cpp:1001`) |
| SP8 | READ-34 | Measuring with a new mic whose tap fails, or with no mic | Idle, every tap released (`tests/reading/session.cpp:863-864`) |
| SP9 | READ-01/02 | "25.6 s" assumes q = 66.7 ms; the payload carries whole ms | 25.344 s from q_ms 66 (`tests/reading/pattern.cpp:18`) |
| SP10 | READ-42, READ-43 | Closed in `8fab0aa`: `spec.md:193` says the wait starts over from the next video marker | The code starts it over at the next video marker once the markers have stopped for ≥ 5 s (`core/reading/session.cpp:256`); both sides of the boundary are tested (`tests/reading/session.cpp:969-991`) |

**Status**: ✅ All unit-testable ACs covered. 8 spec-precision gaps stay open (SP1, SP2, SP4 to SP9); SP3 and SP10 are closed by the spec. Line citations in the other rows that point into `tests/reading/session.cpp` after line 966 were taken at `7156f1f` and now sit 24 lines lower (55 after line 1219); the gate at `8fab0aa` shows each cited case still passes.

---

## Discrimination Sensor

### Iteration 2 (this report, at `8fab0aa`)

Run in a temporary `git worktree` at `<scratchpad>/wt` (detached at `8fab0aa`, `deps/quirc` copied in). The unmutated worktree passed 164 of 164 first. Each mutant was applied alone by exact string replacement (the script asserts the target text occurs once) to `core/reading/session.cpp`, then `cmake --build --preset core` and `ctest --preset core`, and the file was restored from `git show HEAD:` before the next. A first attempt restored with `git checkout` inside the worktree, which fails there (the copied `deps/quirc/.git` points outside it), so its results were discarded and all five mutants were rerun this way. The worktree was removed with `git worktree remove --force`; `git status --porcelain` of the real tree matched the pre-sensor baseline (`M .specs/LESSONS.md`, `M .specs/lessons.json`, `?? .specs/features/reading/validation.md`, before this report was rewritten), and `git stash list` is empty.

| Mutation | File:line | Description | Killed? (failing test) |
| -------- | --------- | ----------- | ---------------------- |
| F5 | `core/reading/session.cpp:228` | 2 s rule drops tap and reading but does not untick (`untick` → `drop_camera`) | ✅ Killed, only by "session unticks a camera that delivers no frame for 2 s" (`tests/reading/session.cpp:1220`) |
| F6 | `core/reading/session.cpp:256` | Wait starts over only after more than 5 s without video (`>=` → `>`) | ✅ Killed, only by "session starts the wait for pattern sound over when video markers resume 5 s after the last" (`:969`) |
| F13 | `core/reading/session.cpp:224` | Camera deactivated only after more than 2 s (`<=` → `<`) | ✅ Killed by "session drops a camera that delivers no frame for 2 s" and the new untick case |
| F15 | `core/reading/session.cpp:256` | Wait never starts over when video resumes (`now - last_video_ns_ >= no_sound_ns` removed) | ✅ Killed by "... when the video markers stop for 5 s" and the new restart case |
| F16 | `core/reading/session.cpp:356` | No-sound message ignores whether video arrived within 5 s | ✅ Killed by "session clears no pattern sound when the video markers stop for 5 s" |
| F6b | `core/reading/session.cpp:256` | Threshold `no_sound_ns - 1` ns | Equivalent: the tests' clock moves in whole milliseconds, so no gap lands within 1 ns of 5 s (the 4.999 s subcase is 1,000,000 ns short). Not counted |

**Sensor depth**: P0-full (5 non-equivalent mutants, all aimed at the rules the two fixes cover; the iteration 1 kills carry over because no file under `core/`, `src/` or `data/` changed in `7156f1f..8fab0aa`).
**Result**: 5 of 5 killed - PASS

### Iteration 1 (at `7156f1f`)

Run in a temporary `git worktree` at `/tmp/claude-1000/-home-vinicius-braz-projects-simple-audiosync-obs-plugin/b62d1231-fbcf-4856-82ee-74b17ec848aa/scratchpad/wt` (detached at `7156f1f`), with `deps/quirc` copied in from the real tree. The unmutated worktree passed 162 of 162 first. Each mutant was applied alone by exact string replacement, then `cmake --build --preset core` and `ctest --preset core`, then `git checkout -- <file>` in the worktree. The two survivors were rerun with two scratch-only probe cases appended to the worktree's `tests/reading/session.cpp`; both probes pass on HEAD. The worktree was removed with `git worktree remove --force`. `git status --porcelain` of the real tree was the same before and after (only this report and the two lessons files); no stash was made (`git stash list` is empty).

| Mutation | File:line | Description | Killed? (failing assertion) |
| -------- | --------- | ----------- | --------------------------- |
| M51 | `core/detector/video-detector.cpp:226` | Decode result ignored: a rejected QR code re-arms the marker window | ✅ Killed, only by `tests/detector/video-detector.cpp:256` `CHECK( 2 == 1 )` |
| E2 | `core/reading/session.cpp:111` | Start does not restart the no-sound wait | ✅ Killed, only by `tests/reading/session.cpp:1023` and `:1025` |
| R21 | `core/detector/audio-detector.cpp:110` | Audio marker of index 5 stamped 2 ms late | ✅ Killed, only by `tests/detector/pattern-sync.cpp:91` (`k := 5`, `error_ns := 2054399`) |
| R22a | `core/reading/estimator.cpp:190` | Each camera's second sample 3 ms off (a middle sample of the replay's four) | ✅ Killed (11 tests), the replay by `tests/reading/estimator-replay.cpp:153` and `:157` |
| R22b | `core/detector/audio-detector.cpp:110` | Audio marker of index 0 stamped 3 ms early: only the replay's first sample is off | ✅ Killed (3 tests), the replay by `tests/reading/estimator-replay.cpp:153` and `:157` |
| F1 | `core/reading/pattern.cpp:20` | Half wrap counted from index_max − 1 | ✅ Killed (3 tests) |
| F2 | `core/reading/estimator.cpp:210` | Window span ignores its largest sample | ✅ Killed (7 tests) |
| F3 | `core/reading/pattern.cpp:25` | Rounding truncates toward zero | ✅ Killed (6 tests) |
| F4 | `core/reading/estimator.cpp:214` | Reading is the fourth sorted sample, not the median | ✅ Killed (6 tests) |
| F5 | `core/reading/session.cpp:228` | 2 s rule drops tap and reading but does not untick (`untick` → `drop_camera`) | ❌ Survived (0 of 162), **not equivalent**: probe (deactivate cam A, then tick it again → row and tap; or Stop, Start → only `mic:mic`, `camera:cam-b` requested) passes on HEAD and fails under F5 → fix task 1 |
| F6 | `core/reading/session.cpp:256` | Wait starts over only after more than 5 s without video (`>=` → `>`) | ❌ Survived (0 of 162), **not equivalent**: probe (markers to 4.8 s, next at exactly 9.8 s → no message at 9.8 s, none at 14.799 s, message at 14.8 s) passes on HEAD and fails under F6 → fix task 2 |
| F7 | `core/selection/selection-policy.cpp:45` | Saved mic chosen even when its source is gone | ✅ Killed (1 test) |
| F8 | `core/selection/selection-policy.cpp:85` | Mic picker lists ticked cameras | ✅ Killed (2 tests) |
| F9 | `core/viewmodel/view-model.cpp:100` | Details swap received and missed | ✅ Killed (1 test) |
| F10 | `core/reading/estimator.cpp:155` | Audio list not capped at 256 | ✅ Killed (1 test) |
| F11 | `core/reading/session.cpp:243` | First payload's pattern not handed to the estimator | ✅ Killed (27 tests) |
| F12 | `core/viewmodel/view-model.cpp:32` | Stable negative reading reads "video late" | ✅ Killed (1 test) |
| F13 | `core/reading/session.cpp:18` | Deactivation after 1.9 s without a frame | ✅ Killed (1 test) |
| F14 | `core/reading/estimator.cpp:93` | A repeated index counts index_max − 1 missed | ✅ Killed (1 test) |

**Sensor depth**: P0-full (19 mutants: the four targeted at the iteration 0 gaps and R22b, plus 14 fresh ones across the half wrap, F, rounding, the median, the 2 s and 5 s rules, selection and view-model strings).
**Outcome**: 17 of 19 killed, 2 survived (F5, F6), neither equivalent - FAIL at the time (both killed in iteration 2)

### Iteration 0 (at `91dca3d`, summary)

56 mutants (M01 to M53, E1 to E3): 52 of 54 non-equivalent killed; M51 and E2 survived (both killed now); E1 (`on_mic_removed` keeps the pattern) and E3 (`tick()` outside Measuring) equivalent. The other iteration 0 kills carry over: no production file changed in `91dca3d..7156f1f`, and the only test lines removed are the audio feed moved into `hear_clip` (`tests/detector/pattern-sync.cpp:28-37`), with no assertion removed or loosened.

---

## Code Quality

| Principle | Status |
| --------- | ------ |
| Minimum code | ✅ The fixes add tests, one fixture and its generator lines only |
| Surgical changes | ✅ `7156f1f..8fab0aa` touches only `tests/reading/session.cpp` (+55), `spec.md` and `tasks.md`; `91dca3d..7156f1f` touches no file outside `tests/` and `tasks.md` notes; one refactor (`hear_clip`) shared by two cases |
| No scope creep | ✅ |
| Matches patterns | ✅ SPDX headers (`check_notices_repository` passes); generator documents the new fixture in its header and in `generate.sh` |
| Spec-anchored outcome check | ✅ READ-21 and READ-22 now anchored on the generator's values and on every sample |
| Per-layer Coverage Expectation | ✅ READ-57's untick is asserted from both sides (re-tick, Stop then Start); both sides of the no-sound wait's restart boundary are asserted |
| Every test maps to a requirement | ✅ Each new case names its AC (READ-21, READ-22, READ-23, READ-42, READ-43, READ-57) |
| Documented guidelines followed | ✅ `CLAUDE.md`, `tasks.md` Test Coverage Matrix, commit convention |
| Core boundary | ✅ Unchanged since iteration 0 |

---

## Gate Check

- **Gate command**: `export PATH=$HOME/.local/bin:$PATH && cmake --preset core && cmake --build --preset core && ctest --preset core --output-on-failure`, on the real tree at `8fab0aa`
- **Result**: 164 passed, 0 failed, 0 skipped (exit 0; the build log has no warning, and warnings are errors)
- **Test count before feature**: 24 at `cece348`
- **Test count after feature**: 164 (`ctest --preset core -N`); iteration 1 had 162, and iteration 2 adds 2: "session unticks a camera that delivers no frame for 2 s" and "session starts the wait for pattern sound over when video markers resume 5 s after the last". Iteration 0 had 158, and the iteration 1 fixes added 4: "video detector emits no marker for a rejected qr code on a fresh detector", "... after a valid marker cycle", "session counts the wait for pattern sound from start", "pattern audio fixture decodes every marker with its generated index within 1 ms of its generated time"
- **Delta**: +140
- **Skipped tests**: none
- **Removed or weakened tests**: none. Over `cece348..7156f1f` the only removed assertion is T1's `CHECK(level == LogLevel::Warning)`, replaced by Debug as the spec requires (`tests/detector/video-detector.cpp:98`). Over `91dca3d..8fab0aa` no assertion is removed (`git diff 7156f1f..HEAD -- tests` is 55 added lines)
- **Fixtures**: `git diff --stat 91dca3d..7156f1f -- tests/fixtures` shows `generate.py` (+11), `generate.sh` (+3 −2) and the new `qr-frame-f9.pgm`; `qr-frame.pgm`, `audio-48k-mono-s16le.pcm`, `audio-indices.txt` and `video-cycles/` are unchanged
- **CI**: not re-read (no `gh`); iteration 0 recorded the two CI-only gate failures and their fixes (`6c9896c`, `0b43200`)

---

## Fix Plans

Both fixes below were made in iteration 2 (`f578985`, `13178fc`) and verified above. They stay for the record.

### Fix 1 (done, `f578985`): Prove the 2 s rule unticks the camera in the session (READ-57) - Major

- **Root cause**: `tests/reading/session.cpp:1170-1194` checks only the row and the tap. The session's selection, which decides what Start taps and what `set_selection` treats as newly ticked, is not observed, so F5 (`drop_camera` instead of `untick` at `core/reading/session.cpp:228`) passes. In the plugin this would leave a deactivated camera that the user ticks again unmeasured while measuring, and tapped again at the next Start.
- **Fix task**: in `tests/reading/session.cpp`, after cam A is dropped at 2.0 s: ticking it again with `set_selection({"cam-a", "cam-b"}, "mic")` gives it a Looking row and a live `camera:cam-a` tap; and, in a second subcase, Stop then Start requests only `mic:mic` and `camera:cam-b`. The rename case at `:1164-1167` is the pattern to follow. Done when F5 fails the case.

### Fix 2 (done, `13178fc`): Test where the no-sound wait starts over (READ-42, READ-43) - Minor

- **Root cause**: the restart when video markers resume is tested after a 7.2 s gap only (`tests/reading/session.cpp:964-966`), so its ≥ 5 s boundary (`core/reading/session.cpp:256`, from `.specs/features/reading/design.md:169`) is not pinned.
- **Fix task**: video markers every 200 ms to 4.8 s, the next at exactly 9.8 s, then every 200 ms: no message at 9.8 s, none at 14.799 s, the message at 14.8 s. Done when F6 fails the case. SP10 was closed by the maintainer in `8fab0aa`, which states the restart in READ-43.

---

## Requirement Traceability Update

`8fab0aa` already updated the traceability table in `spec.md`; this table is the Verifier's view of it:

| Requirement | Previous Status | New Status |
| ----------- | --------------- | ---------- |
| READ-01 to READ-56 (core halves), READ-58 to READ-60, READ-67, READ-68, READ-84 to READ-89, READ-92, READ-97, READ-98 | Pending | ✅ Verified (unit) |
| READ-57 | Pending | ✅ Verified (unit; rig check pending, T34) |
| READ-61 to READ-66, READ-69 to READ-83, READ-90, READ-91, READ-93 to READ-96 | Pending | Pending rig, UAT or CI |

---

## Summary

**Overall**: ✅ Ready (for T1 to T33)

**Spec-anchored check**: 71 of 71 unit-testable ACs match the spec outcome; 8 spec-precision gaps stay open (SP1, SP2, SP4 to SP9), SP3 and SP10 are closed by the spec.
**Sensor**: iteration 2, 5 non-equivalent mutants, 5 killed (F5 and F6, the iteration 1 survivors, among them).
**Gate**: 164 passed, 0 failed.

**What works**: everything iteration 1 reported, plus: a camera silent for 2 s is dropped, released and unticked, and ticking it again or a Stop then Start treats it as unticked (F5 killed); the no-sound wait starts over exactly when the video markers have stopped for 5 s, not at 4.999 s and not later (F6, F15 killed).

**Not covered by this verdict**: T26's open box (the red run of the fuzz CI step on a broken range check, `tasks.md:751`; READ-91, CI only), the CI run itself (not re-read, no `gh`), and the rig tasks T34 to T37 (READ-61 to READ-83, READ-93 to READ-96).

**Next steps**: do the T26 red run when convenient, then rig tasks T34 to T37, then start the `alignment` feature.
