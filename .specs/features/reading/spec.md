# Reading Specification

> Phase 2 of the [technical design](../../../docs/tdd/multi-camera-sync.md#implementation-plan): every selected camera's reading against the mic, measured at once in one dock. It implements the design's [Reading slice](../../../.design/multi-camera-sync.md#reading), whose state table is the source of these criteria. The design, the [ADRs](../../../docs/adr/README.md) and the TDD win over this spec where they disagree. Size: Complex.
>
> **Written ahead of the spikes, by decision.** The TDD holds this spec until the rig spikes settle ADR-005 and ADR-007. The maintainer chose to specify now, on the TDD's defaults: Tap B works, Sync Dock's detector holds, and all cameras are measured in one pass. Stories 3 and 7 carry that bet and are marked. If a spike fails, the Assumptions table says what is reworked.

## Problem Statement

Foundation left the repository with an OBS-free core, Sync Dock's detector vendored, and a dock that shows one line of text. Nothing measures a camera. Users with several cameras and one mic still run the clap-and-editor loop, because the only tool that measures (Sync Dock) reads Program, so one camera at a time. Alignment cannot start until the plugin can say, for every selected camera at once, by how many milliseconds its video trails or leads the mic, and can say when that number is trustworthy.

## Goals

- [ ] With two or more cameras and one mic selected and the pattern playing on a phone, every camera that sees the pattern reaches a stable reading in one measuring pass, and each reading is within one frame of minus Sync Dock's latency for that camera.
- [ ] Every rule of the design's Reading table (pairing, stability, camera states, session, what the dock shows, the remembered selection) runs in OBS-free unit tests on the WSL machine and in CI.
- [ ] Measuring changes nothing in the user's OBS: no filter, scene, source property or Program output is added or edited, and the scene collection saved while measuring is the one the user built.
- [ ] A camera that cannot see the pattern, or a mic that hears nothing, tells the user so in the dock instead of staying silent.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
| ------- | ------ |
| Calculator, settings port, Apply, Undo, the Apply and Undo buttons, "Already aligned", "Aligned" | Alignment phase. Reading ends at a stable reading per camera |
| Clearing readings after Apply, and resuming measuring after Apply | Alignment phase; the session offers the clear operation Reset uses |
| Spike 1 and spike 2, and recording ADR outcomes | Rig experiments outside tlc-spec-driven. This spec assumes both pass |
| An echo-tolerant pattern and detector | Only if spike 2 fails; it then needs its own ADR and spec |
| Measuring Program one camera at a time, with a guided walk-through | Only if spike 1 fails (ADR-007's option B); it then replaces stories 6 and 7 |
| Choosing between Tap B and a filter tap, Tap B's canvas size, thread layout | Design phase (TDD Open questions 1 and 15) |
| Pattern hosting, generation in CI, the Windows installer, signing, Flatpak, the forum page | Release phase |
| Detecting enable, disable or settings changes of a camera's filters | Only filter add and remove are observed (see Assumptions) |
| Detecting a canvas frame-rate change while measuring | Not signalled to the plugin; Stop then Start reads it again |
| Locales other than en-US | The locale file has en-US first; translations follow the release |
| Drift, more than one mic, measuring without the pattern, screen, window and game captures, a camera's own mic as the reference, single-camera users as the target | Design Boundary |

---

## Assumptions & Open Questions

Facts below were checked on 2026-10-15 against the repository at `55c0270`, the design (confirmed 2026-10-11) and the TDD. Rows marked `y` were confirmed by the maintainer; rows marked `n` are the agent's defaults, taken from the TDD where it names one, and stay open to override until the spec is confirmed.

| Assumption / decision | Chosen default | Rationale | Confirmed? |
| --------------------- | -------------- | --------- | ---------- |
| Which phase is "next" | Reading (phase 2), specified before the spikes run | Foundation's task T16 and both spikes need the rig. Everything in Reading except the taps and the rig runs is gate-able on WSL | y |
| What the spikes are assumed to decide | ADR-005 holds (Sync Dock's detector, reworked) and ADR-007 holds (Tap B, all cameras at once) | The TDD's defaults for Open questions 1 and 2 | y |
| If spike 1 fails (ADR-007 superseded) | Stories 6 and 7 and the session's multi-camera Start are rewritten as a guided Program walk-through; stories 1, 2, 4, 5 and 8 keep their rules, since they take markers from whatever tap feeds them | The estimator, states and view-model do not care where a camera's markers come from | n |
| If spike 2 fails (ADR-005 superseded) | Story 3 and its detector fixtures are rewritten for the new detector; the estimator's input contract (video marker, audio marker) stays | The estimator pairs markers; only the code that produces them changes | n |
| Requirement IDs | `READ-NN`, one per criterion, in story order. The TDD's suggested IDs READ-01 to READ-10 are design-row labels; the traceability table maps each story to its rows | Foundation used one ID per criterion; finer IDs trace to tests | n |
| Window of "the last five samples" | The five most recent by video marker time | Samples arrive nearly in time order; this removes any dependence on arrival order | n |
| The frame tolerance F | `1000 × fps_den ÷ fps_num` ms from OBS's canvas frame rate, read at each Start (33.333 ms at 30 fps, 16.667 ms at 60 fps). A span of exactly F is within one frame | The design says "within one frame"; the TDD gives F as the canvas frame interval | n |
| Rounding of a reading | The median of the five samples, to the nearest whole ms, halves away from zero (141.5 gives 142, −141.5 gives −142) | The TDD says "rounded to whole milliseconds" without a rule for halves | n |
| The pairing limit | Less than half a wrap apart, where a wrap lasts `index_max × 3 × q_ms` (the pattern's marker cycle is 3q frames): 25.6 s for q = 4 at 60 fps and index_max 256 | The TDD's Reading arithmetic, with its cadence made explicit. It exceeds OBS's 20,000 ms limit | n |
| Capacity of each marker list | 256 markers, one full wrap, dropping the oldest. The audio list must hold up to 20 s of markers (about 100) because a delayed camera's marker arrives that much after the mic's | The TDD says queues are bounded; a wrap-sized bound covers the largest reading Apply can write | n |
| Which pattern the session follows | The parameters (f, c, q, index_max) of the first valid QR payload after Start on a fresh session or after Reset. They survive Stop and the removal of the camera that supplied them, and are cleared by Reset or a collection switch | TDD detector table: "one pattern per session", "ignored until Reset" | n |
| Mic removed, or a different mic chosen | Every reading and the pattern are cleared. A removed mic ends the session in Idle | A reading is relative to one mic; keeping it after a change would pair a camera with a mic it was not measured against. The TDD stops measuring on removal and says nothing on readings | n |
| Mic and cameras are disjoint | The mic picker leaves out ticked cameras, and the camera checklist leaves out the chosen mic | TDD Open question 5 gives the first half; the second keeps a source from being both | n |
| What counts as an "audio input" for preselecting the mic | Every source in the scene collection that has an audio output, including cameras with their own audio. The mic is preselected only if exactly one exists | The design's wording is "exactly one audio input exists"; a wrong preselection is worse than none. Typical OBS setups have Desktop Audio and a mic, so they see no preselection | n |
| When preselection runs | On the first dock open or collection load that has no saved selection. A saved selection of a known version always wins, even if none of its sources resolve any more. Later Program scene changes do not reselect | TDD Persisted state: "with no saved selection, the dock preselects" | n |
| Order of the lists | Alphabetical by source name, ignoring case | Stable rows under the user's cursor; the design is silent | n |
| Rename (TDD Open question 3), and what "leaves the list" means | The reading is dropped, the camera is unticked, and it stays in the checklist under its new name. "Leaves the list" in the design means it leaves the measured rows, which show only ticked cameras | The design: a renamed camera's reading is dropped; the TDD says it reappears under its new name unselected | n |
| Deactivated (TDD Open question 4) | While measuring, a ticked camera that delivers no frames for 2 s is deactivated: reading dropped, tap released, unticked. It stays listed. A pattern that is out of view is not deactivation, since frames still flow | A device that stops delivering is what "deactivate when not showing" and an unplugged camera look like to the plugin. Stopped sessions have no taps and detect nothing | n |
| Correction changed by hand (TDD Open question 6) | Observed: a camera's filter added or removed, and the mic's Sync Offset changing. A Sync Offset change drops every reading; a filter change drops that camera's. Enabling, disabling or editing a filter is not observed; the next stable window replaces the stale reading if the pattern is still playing | The TDD's integration points list those signals; observing more is a Design question | n |
| No pattern sound (TDD Open question 7) | Shown while measuring once 5 s have passed since the first video marker that followed the last audio marker (or Start) with no audio marker in between, and video markers are still arriving; rows keep "Looking for the pattern" meanwhile. It clears when an audio marker arrives, when no video marker arrived in the last 5 s, or when measuring stops | The TDD says "after 5 s of video markers with no audio marker". Counting from the first unanswered video marker gives the mic its 5 s to decode, covers a mic that was heard and then muted, and goes quiet when the phone stops | n |
| A stable reading is not demoted (TDD Open question 9) | An unsettled stretch or a lost pattern leaves the reading and its row unchanged; a newer window within F replaces the value | The TDD default and Key decision 6 | n |
| Strings the design does not give | A zero reading shows "0 ms - in sync". A settling row shows "{live value} ms - settling", for example "+142 ms - settling". A mic tap that cannot be created shows "Could not listen to {mic}". Details show "received {n}, missed {m}" | The design names the late, early, looking, no-sound and pick strings; these are the missing cases | n |
| The sign character | "−" is U+2212 in "−60 ms - video early", as the design writes it; "+" is ASCII | Matches the design's text exactly | n |
| A malformed QR payload logs at debug level | The vendored range check logs at Warning today; this phase changes it to Debug | TDD Error handling: "Ignored, logged at debug level". At five markers a second a noisy picture would flood the log at Warning | n |
| The pattern link | Points to Sync Dock's README (`https://github.com/norihiro/obs-audio-video-sync-dock/blob/main/README.md`), which the TDD cites for the pattern files, until Release hosts the files | Release owns the files and the forum page | n |
| Fuzz run | A libFuzzer target on the Ubuntu core job, 60 s per CI run, with a seeded corpus replayed through CTest everywhere else | Foundation deferred the fuzz test to this phase. libFuzzer needs Clang; the core preset uses the default compiler | n |
| Coverage gate | 90% of lines in `core/`, excluding `core/detector/` (Sync Dock's code, covered by the detector fixtures instead) | The TDD sets 90% for the core; Foundation deferred the gate to this phase. Upstream code the project did not write is judged by its outputs | n |
| Foundation's T16 | Stays open. The dock this phase builds replaces the one-line dock, so FND-22 no longer describes it; the rig run in story 9 loads the plugin in OBS 30.0.0 and the latest stable and covers FND-19, FND-20 and FND-24 for the new dock | One OBS machine session serves both | n |
| Confirmed lessons | None exist (`.specs/lessons.json` holds four candidates, which are not loaded) | Skill rule: load confirmed lessons only | y |
| The saved selection's JSON shape (amended 2026-10-16) | `{"version": 1, "cameras": [{"uuid": <UUID>}, ...], "mic": <UUID>}`, with `mic` left out when none is chosen; still version 1, since nothing has shipped | OBS 30's `obs_data`, which writes the scene collection, holds arrays of objects only and writes no null, so the first shape (an array of UUID strings and a null mic) cannot be saved | y |

**Open questions:** none - all resolved or logged above (required before the spec is confirmed).

---

## User Stories

### Story 1 · P1: A camera's reading comes from pairing its markers with the mic's ⭐ MVP

**User Story**: As a multi-camera user, I want each camera's offset to the mic computed from the pattern's markers on that camera and on the mic, so the number means "this camera's video trails the mic by this many milliseconds".

**Why P1**: Every other story feeds or displays this number. Alignment's arithmetic takes it as input.

*Design rows: Reading arithmetic (TDD). Core only; runs on WSL.*

**Acceptance Criteria**:

1. WHEN a video marker of a camera and an audio marker carry the same index and their times differ by less than half a wrap THEN the estimator SHALL form a sample equal to the video marker's time minus the audio marker's time in ms, positive when video is late (video at 1.142 s and audio at 1.000 s give +142). **READ-01**
2. IF a video marker and an audio marker carry different indexes, or lie half a wrap or more apart, THEN the estimator SHALL NOT form a sample from them. **READ-02**
3. WHEN more than one audio marker carries the index of a video marker THEN the estimator SHALL pair the video marker with the audio marker nearest to it in time. **READ-03**
4. WHEN a pair's audio marker arrives before its video marker, or after it, THEN the estimator SHALL form the same sample. **READ-04**
5. The estimator SHALL keep one sample window per camera and pair every camera's video markers with the one list of audio markers, so a sample of one camera never enters another camera's window. **READ-05**
6. WHEN a marker list holds more than 256 markers THEN the estimator SHALL drop the oldest. **READ-06**
7. WHEN several threads push markers at the same time THEN the estimator SHALL record every marker pushed. **READ-07**
8. WHEN the estimator is asked for a snapshot THEN it SHALL return, for every camera, the state, latest sample, reading and sample count as they were at one instant. **READ-08**

**Independent Test**: Feed hand-built markers through the estimator in a unit test: the +142 example, a different-index pair, a pair 25.6 s apart, a wrapped index, the audio-first and video-first orders, and four threads pushing 1,000 markers each.

---

### Story 2 · P1: A camera's state follows its samples ⭐ MVP

**User Story**: As a multi-camera user, I want each camera to show whether it is still looking for the pattern, settling, or stable, so I know when its number can be trusted.

**Why P1**: Alignment refuses to write until every selected camera is stable (Key decision 7).

*Design rows: no camera has seen the pattern; markers disagree; last five markers agree; a stable camera loses the pattern. Core only.*

**Acceptance Criteria**:

1. WHILE a camera has no sample the estimator SHALL report it as Looking. **READ-09**
2. WHEN a camera that has no sample receives its first sample THEN the estimator SHALL report it as Settling, with that sample, rounded to a whole ms, as its live value. **READ-10**
3. WHILE a camera has never been stable and has fewer than five samples, or its last five samples span more than F, the estimator SHALL report it as Settling with its latest sample, rounded to a whole ms, as the live value. **READ-11**
4. WHEN a camera's last five samples span at most F (maximum minus minimum, F included) THEN the estimator SHALL report it as Stable with the median of those five, rounded to the nearest whole ms with halves away from zero, as its reading. **READ-12**
5. WHEN a Stable camera's last five samples span more than F THEN the estimator SHALL keep it Stable with its reading unchanged. **READ-13**
6. WHEN a Stable camera stops receiving samples, for example because the phone no longer faces it, THEN the estimator SHALL keep it Stable with its reading unchanged. **READ-14**
7. WHEN a Stable camera's last five samples again span at most F THEN the estimator SHALL replace its reading with the median of the new five. **READ-15**
8. The estimator SHALL take F as `1000 × fps_den ÷ fps_num` ms from OBS's canvas frame rate when the session starts. **READ-16**

**Independent Test**: Unit-test the state machine with sample sequences at 30 fps (F = 33.333): four samples then Settling; five within 33.3 then Stable at their median; a sixth sample 40 ms away keeps Stable; five new samples within F replace the reading; samples 141.5 and −141.5 round to 142 and −142; a span of exactly F is stable.

---

### Story 3 · P1: One pattern, read by a detector per camera and one for the mic ⭐ MVP

**User Story**: As a multi-camera user, I want the pattern's QR codes and sound pulses decoded for each camera and for the mic on their own, so all cameras are measured against the same mic at once.

**Why P1**: ADR-005's rework: Sync Dock's detector runs once on Program; this plugin needs a video half per camera and one audio half.

*Depends on ADR-005 (Proposed until spike 2). Design rows: the mic hears nothing; a second pattern. Core only.*

**Acceptance Criteria**:

1. WHEN the first camera after Start on a fresh session, or after Reset, decodes a valid QR payload THEN the session SHALL set the audio decoder's pattern (f, c, q) and the estimator's index wrap from that payload. **READ-17**
2. WHILE the session's pattern is set, IF a camera decodes a payload whose f, c, q or index_max differs from the session's THEN the session SHALL ignore that payload's markers. **READ-18**
3. WHILE the session's pattern is not set the audio decoder SHALL emit no audio marker. **READ-19**
4. WHEN a generated pattern clip with a known video-to-audio offset is fed to one camera's video decoder THEN every marker in the clip SHALL decode with the index it was generated with and a time within 1 ms of the generated time. **READ-20**
5. WHEN the same clip's audio is fed to the audio decoder after the pattern is set THEN every audio marker in the clip SHALL decode with the index it was generated with and a time within 1 ms of the generated time. **READ-21**
6. WHEN clips whose video trails the shared audio by 100 ms and by 40 ms are fed to two cameras' decoders THEN the estimator SHALL form samples of +100 ms ± 1 ms for the first camera and +40 ms ± 1 ms for the second. **READ-22**
7. IF a QR payload is malformed or has a value out of range (f outside 10 to 32,000, c outside 1 to f, q outside 1 to 1,000 ms, i outside 0 to 255) THEN the video decoder SHALL ignore it, log it at debug level, emit no marker and change no state. **READ-23**

**Independent Test**: Replay the committed fixtures from `tests/fixtures/` through two video decoders and one audio decoder with injected offsets of 100 ms and 40 ms, and assert the samples; feed a payload with `f=9`, `c=0`, `q=1001` and `i=256` one at a time and assert no marker and a debug log line.

---

### Story 4 · P1: A measuring session starts, stops and resets ⭐ MVP

**User Story**: As a multi-camera user, I want to start measuring, stop without losing what I measured, and reset when I want to begin again, so I can measure a camera, move the phone, and measure the next one.

**Why P1**: Key decision 6: readings persist until Apply, Reset or a collection switch, so a camera that cannot see the phone is measured later.

*Design row: Stop, Reset, or scene collection switched. Core logic; taps and OBS events are adapters (story 7).*

**Acceptance Criteria**:

1. WHEN Start is requested in Idle or Stopped with at least one camera and the mic chosen THEN the session SHALL enter Measuring and request one tap for each chosen camera and one for the mic. **READ-24**
2. WHEN Stop is requested while Measuring THEN the session SHALL enter Stopped, release every tap and keep every reading and the pattern. **READ-25**
3. WHEN Start is requested in Stopped THEN the session SHALL enter Measuring with every kept reading and its camera state unchanged. **READ-26**
4. WHEN Reset is requested in Measuring or Stopped THEN the session SHALL clear every reading, sample, marker and the pattern, and report every measured camera as Looking. **READ-27**
5. WHEN the scene collection is changing THEN the session SHALL release every tap, clear every reading and the pattern, enter Idle and load the new collection's selection. **READ-28**
6. WHEN OBS exits THEN the session SHALL release every tap before libobs shuts down. **READ-29**
7. IF Start is requested with no camera or no mic chosen THEN the session SHALL stay as it is. **READ-30**
8. IF Stop is requested in Idle or Stopped, or Reset in Idle, THEN the session SHALL stay as it is. **READ-31**
9. WHEN a camera is ticked while Measuring THEN the session SHALL request a tap for it and report it as Looking. **READ-32**
10. WHEN a camera is unticked while Measuring or Stopped THEN the session SHALL release its tap if it has one and drop its reading, and leave every other camera's tap and reading unchanged. **READ-33**
11. WHEN the chosen mic changes THEN the session SHALL clear every reading and the pattern, and while Measuring replace the mic's tap and report every camera as Looking. **READ-34**
12. WHEN the chosen mic is removed THEN the session SHALL release every tap, clear every reading and the pattern, and enter Idle. **READ-35**

**Independent Test**: Drive the session in a unit test with fake taps that record requests and releases: Start, markers to Stable, Stop (readings kept, taps released), Start (readings kept), Reset (Looking), collection switch (Idle, nothing kept), untick one of two cameras, change the mic, remove the mic.

---

### Story 5 · P1: The dock tells the user the state of every camera ⭐ MVP

**User Story**: As a multi-camera user, I want each camera's row to say what it is doing and each problem to be written in the dock, so a camera that never got a reading is never a silent failure.

**Why P1**: The complaint that dominates Sync Dock's support thread is a camera that never got a reading (design Success, "Going wrong").

*Design rows: every "Dock shows" cell of the Reading table. View-model in the core; all text from the locale file.*

**Acceptance Criteria**:

1. IF no camera or no mic is chosen THEN the view-model SHALL disable Start and show "Pick at least one camera and the mic" on the message line. **READ-36**
2. WHILE a camera is Looking its row SHALL read "Looking for the pattern". **READ-37**
3. WHILE a camera is Settling its row SHALL read its live value with an explicit sign, then " ms - settling" (for example "+142 ms - settling"). **READ-38**
4. WHILE a camera is Stable with a reading above 0 its row SHALL read the reading with a leading "+" and " ms - video late" (for example "+142 ms - video late"). **READ-39**
5. WHILE a camera is Stable with a reading below 0 its row SHALL read the reading with a leading "−" (U+2212) and " ms - video early" (for example "−60 ms - video early"). **READ-40**
6. WHILE a camera is Stable with a reading of 0 its row SHALL read "0 ms - in sync". **READ-41**
7. WHILE the session is Measuring, IF 5 s or more have passed since the first video marker (of any camera) that followed the last audio marker or Start, no audio marker has arrived in that time, and a video marker arrived within the last 5 s THEN the message line SHALL read "No pattern sound from {mic} - bring the phone closer or unmute it" with the mic's name. **READ-42**
8. WHEN an audio marker arrives, or 5 s pass with no video marker, or the session leaves Measuring THEN the message line SHALL stop showing the no-sound message, and the wait starts over from the next video marker. **READ-43**
9. WHEN a camera is unticked or dropped (removed, renamed or deactivated) THEN the view-model SHALL show no reading or status for it. **READ-44**
10. The view-model SHALL label the Start/Stop button "Stop" while the session is Measuring and "Start" otherwise. **READ-45**
11. The view-model SHALL disable Reset while the session is Idle and enable it while the session is Measuring or Stopped. **READ-46**
12. WHILE the session is Idle or Stopped and at least one camera and the mic are chosen the view-model SHALL enable Start. **READ-47**
13. The view-model SHALL resolve every user-visible string through a key in the en-US locale file, and every key it uses SHALL exist there. **READ-48**

**Independent Test**: Unit-test the view-model over snapshots: one snapshot per row of the Reading table, asserting the exact string, the button states and the message line; a test that reads `data/locale/en-US.ini` and the view-model's key list and compares them.

---

### Story 6 · P1: The dock lists the cameras and mics and remembers the choice ⭐ MVP

**User Story**: As a multi-camera user, I want the dock to offer the right sources, preselect the obvious ones, and remember my choice for each scene collection, so I tick nothing twice.

**Why P1**: Without a list there is nothing to start. The selection is the plugin's only persisted state (design, "What Apply writes").

*Design rows: Dock opened; a selected camera is removed, renamed or deactivated; the selection remembered. Selection parsing and the lookup rules are core; the source catalog is an adapter.*

**Acceptance Criteria**:

1. WHEN the dock opens THEN the source catalog SHALL list, sorted by name ignoring case, every source with asynchronous video (webcams, capture cards, media and NDI sources) as a camera, and every source with an audio output as a mic candidate. **READ-49**
2. WHEN the dock opens or a collection finishes loading with no saved selection THEN the catalog SHALL tick every camera in the current Program scene, counting cameras inside nested scenes and groups. **READ-50**
3. WHEN the dock opens or a collection finishes loading with no saved selection and exactly one source in the collection has an audio output THEN the catalog SHALL choose that source as the mic, unless it is a ticked camera (READ-60 keeps a ticked camera off the mic list), in which case the mic is left unset. **READ-51**
4. WHEN a saved selection of version 1 exists THEN the catalog SHALL tick and choose exactly the sources whose UUIDs it names that still exist, and SHALL NOT preselect anything. **READ-52**
5. IF a saved selection's `version` is not 1 THEN the catalog SHALL treat the collection as having no saved selection. **READ-53**
6. WHEN OBS saves the scene collection THEN the selection store SHALL write the module data `{"version": 1, "cameras": [{"uuid": <UUID>}, ...], "mic": <UUID>}` for the chosen cameras and mic, leaving out `mic` when no mic is chosen. **READ-54**
7. WHEN a ticked camera is removed from OBS THEN the session SHALL drop its reading and release its tap, and the catalog SHALL remove it from the list and from the selection. **READ-55**
8. WHEN a ticked camera is renamed THEN the session SHALL drop its reading and release its tap, and the catalog SHALL show it under its new name, unticked. **READ-56**
9. WHILE the session is Measuring, IF a ticked camera delivers no frames for 2 s THEN the session SHALL drop its reading, release its tap and untick it, and the catalog SHALL keep it listed. **READ-57**
10. WHEN a filter is added to or removed from a ticked camera THEN the session SHALL drop that camera's reading and report it as Looking. **READ-58**
11. WHEN the Sync Offset of the chosen mic changes THEN the session SHALL drop every reading and report every camera as Looking. **READ-59**
12. The mic picker SHALL NOT list a ticked camera, and the camera checklist SHALL NOT list the chosen mic. **READ-60**

**Independent Test**: Unit-test the selection rules in the core with fake source lists (Program scene with a nested group, one audio input, two audio inputs, a saved selection with a vanished UUID, version 2, a rename, a removal). On the rig, tick two cameras, save the collection, restart OBS and see the same ticks.

---

### Story 7 · P1: Every selected camera is measured at once, without touching the user's setup ⭐ MVP

**User Story**: As a multi-camera user, I want all my selected cameras measured together while one phone plays the pattern, and I want OBS left exactly as I built it, so I can measure at any time and keep the setup I have.

**Why P1**: Key decision 6 is the promise the design's Success line is written against, and Key decisions 1 and 4 forbid measuring from inside the user's setup.

*Depends on ADR-007 (Proposed until spike 1). Design rows: every Reading row's cause; all of it adapter code, gated by the CI build and by the rig (story 9).*

**Acceptance Criteria**:

1. WHILE the session is Measuring, each ticked camera's tap SHALL deliver that camera's frames to its own video decoder, stamped with the time Program would show the frame if that camera were on air with every current filter in place, including Video Delay (Async) and Render Delay. **READ-61**
2. WHILE the session is Measuring, the mic's tap SHALL deliver the chosen mic's own samples, and no other source's, to the audio decoder, stamped with the mic's device time plus the mic's current Sync Offset. **READ-62**
3. WHILE the session is Measuring, the taps SHALL measure a ticked camera that is in no scene or is not on air. **READ-63**
4. The taps SHALL NOT add, remove, edit or reorder any filter, scene, source property, audio track or Program output. **READ-64**
5. WHEN OBS saves the scene collection while the session is Measuring THEN the saved collection SHALL differ from the collection saved with the session Idle only in the plugin's module data. **READ-65**
6. WHEN a measured source is removed THEN its tap SHALL end without a crash or a dangling reference. **READ-66**
7. IF a camera's tap cannot be created THEN the session SHALL measure the other cameras, leave that camera Looking and log a warning naming it. **READ-67**
8. IF the mic's tap cannot be created THEN the session SHALL NOT enter Measuring, SHALL release every tap it created, log a warning and show "Could not listen to {mic}" on the message line. **READ-68**
9. IF one camera's decoding falls behind THEN the session SHALL skip that camera's own frames and leave every other camera's frames and markers unaffected. **READ-69**

**Independent Test**: On the rig, with two cameras of different lags and one carrying a Render Delay: save the scene collection before and during measuring and diff them; start measuring with one camera off-air and see it read; unplug a camera and see its tap end.

---

### Story 8 · P1: The dock renders the session and forwards clicks ⭐ MVP

**User Story**: As a multi-camera user, I want one dock with everything I need to measure and nothing I do not, so I can find the controls without a manual.

**Why P1**: It is the only surface the user touches. It stays thin so every rule lives in the core where it is tested.

*Design: Dock (TDD). Replaces Foundation's one-line dock.*

**Acceptance Criteria**:

1. The dock SHALL show the camera checklist with each camera's status text, the mic picker, a link to the pattern, Start/Stop, Reset and the message line, and SHALL NOT show Apply or Undo. **READ-70**
2. WHEN the user ticks or unticks a camera, or chooses a mic THEN the dock SHALL pass the change to the session. **READ-71**
3. WHEN the user presses Start, Stop or Reset THEN the dock SHALL pass that request to the session. **READ-72**
4. WHEN the user follows the pattern link THEN the dock SHALL open `https://github.com/norihiro/obs-audio-video-sync-dock/blob/main/README.md` in the system browser. **READ-73**
5. The dock SHALL repaint from a view-model snapshot, and SHALL NOT compute any label, status or button state itself. **READ-74**
6. The dock's title SHALL come from the plugin's en-US locale file. **READ-75**

**Independent Test**: Build the dock in CI; on the rig, open it and compare it with the list above. The view-model tests (story 5) cover every string and state.

---

### Story 9 · P1: The rig confirms the readings ⭐ MVP

**User Story**: As the maintainer, I want every Reading row walked on the real rig in the oldest and newest supported OBS, so I know the readings equal what Program shows and the dock behaves as the design says.

**Why P1**: The design's success line is a rig measurement, and the unit tests cannot prove the taps.

*Rig session: two cameras with clearly different lags (a phone webcam app is the second), one carrying a Render Delay, one mic, Sync Dock installed, a phone playing the pattern. Depends on ADR-005 and ADR-007.*

**Acceptance Criteria**:

1. WHEN OBS 30.0.0 and the latest stable OBS each start with the plugin installed THEN the Docks menu SHALL list "Camera Sync" and the dock SHALL open with the controls of story 8. **READ-76**
2. WHEN OBS 30.0.0 and the latest stable OBS each close after a Start, Stop and Reset with the plugin installed THEN each OBS's last "Number of memory leaks" count SHALL equal its count without the plugin. **READ-77**
3. WHEN the rig runs the protocol with both cameras ticked THEN both cameras SHALL reach Stable in the same measuring pass. **READ-78**
4. WHEN each camera in turn is put on Program with Sync Dock running THEN the plugin's stable reading for that camera SHALL equal minus Sync Dock's latency within one frame, in three runs in a row. **READ-79**
5. WHEN the phone is in view of a camera and audible to the mic THEN the camera SHALL reach Stable within 5 s. **READ-80**
6. WHEN the phone is hidden from one camera THEN that camera SHALL read "Looking for the pattern" and the other camera SHALL keep measuring. **READ-81**
7. WHEN a Reading row of the design's state table is exercised on the rig, in OBS 30.0.0 and in the latest stable, THEN the dock SHALL show the text of that row's "Dock shows" cell. **READ-82**
8. WHEN a rig run completes THEN its debug marker log SHALL be committed under `tests/fixtures/` with the steps that produced it, and a unit test SHALL replay it through the estimator and assert the readings the run showed. **READ-83**

**Independent Test**: The rig protocol in the TDD's Testing Strategy, through tlc-spec-driven's interactive UAT, recorded in `validation.md` with the OBS versions, canvas resolution and frame rate, devices and phone distances.

---

### Story 10 · P2: Measuring is visible in the OBS log

**User Story**: As the maintainer answering a forum report, I want one searchable log line per event, so I can tell from a user's log which camera never got a reading and why.

**Why P2**: Support depends on it, but no user flow does.

**Acceptance Criteria**:

1. WHEN measuring starts THEN the plugin SHALL log at info level `[camera-sync] measure.start cameras=<n> mic="<name>" canvas_fps=<fps>`. **READ-84**
2. WHEN the session stops or resets THEN the plugin SHALL log at info level `[camera-sync] measure.stop` or `[camera-sync] measure.reset`. **READ-85**
3. WHEN a camera becomes Stable, or its stable reading changes, THEN the plugin SHALL log at info level `[camera-sync] reading.stable camera="<name>" reading_ms=<signed integer> spread_ms=<integer>`, where the spread is the maximum minus the minimum of the window, rounded. **READ-86**
4. WHERE OBS logs verbosely, the plugin SHALL log at debug level every marker with its camera or mic, index and time, every QR payload's parameters, and the frames each camera skipped. **READ-87**
5. The plugin SHALL write a double quote inside a logged source name as `\"`. **READ-88**
6. The plugin SHALL log no audio, image or user data beyond the source names OBS already logs. **READ-89**

**Independent Test**: Unit-test the formatting with a fake logger, including a name that contains a quote; read the log of a rig run.

---

### Story 11 · P2: The core's quality gates run in CI

**User Story**: As the maintainer, I want the QR parser fuzzed and the core's coverage measured in CI, so untrusted camera input cannot crash OBS and the arithmetic the later phases rely on is exercised.

**Why P2**: Foundation deferred both to this phase. Neither blocks a feature.

**Acceptance Criteria**:

1. The core job SHALL fail IF line coverage of `core/`, excluding `core/detector/`, is below 90%. **READ-90**
2. WHEN the Ubuntu core job runs THEN it SHALL run a libFuzzer target against the QR payload parser for 60 s and fail on any crash, hang or accepted out-of-range value. **READ-91**
3. WHEN CTest runs on any platform THEN it SHALL replay the fuzzer's seed corpus through the parser and fail on any crash or accepted out-of-range value. **READ-92**

**Independent Test**: Break a range check on a branch and see the corpus replay and the fuzz run fail; delete a core test and see the coverage gate fail.

---

### Story 12 · P2: Measuring stays inside the performance budget

**User Story**: As a multi-camera user, I want measuring to leave Program smooth, so I can measure during setup without dropping frames.

**Why P2**: A measuring dock that drops frames would not be used, but the readings themselves do not depend on it.

*Measured on the rig with OBS's stats and a process monitor.*

**Acceptance Criteria**:

1. WHILE measuring, the plugin SHALL use at most 5% of one core per measured camera at 60 fps. **READ-93**
2. WHILE measuring four cameras on a 1080p canvas at 30 fps and at 60 fps, OBS's "frames missed due to rendering lag" count over five minutes SHALL NOT exceed its count over five minutes without measuring. **READ-94**
3. WHILE a session runs for 30 minutes the plugin's memory SHALL NOT grow. **READ-95**
4. The dock SHALL refresh about ten times a second, and the plugin SHALL run no marker detection on the UI thread. **READ-96**

**Independent Test**: The performance table of the TDD, on the rig.

---

### Story 13 · P3: Details show how well each camera sees the pattern

**User Story**: As a support person reading a user's screenshot, I want each camera's markers received and missed, so I can tell a camera that sees little of the pattern from one that sees none.

**Why P3**: The TDD marks it P3; Sync Dock shows the same as a "missed" percentage.

**Acceptance Criteria**:

1. WHEN the user turns on the details toggle THEN each camera's row SHALL add "received {n}, missed {m}" for the markers since Start or Reset. **READ-97**
2. The view-model SHALL count a camera's missed markers as the indexes skipped between each two consecutive markers it received, counting across the wrap. **READ-98**

**Independent Test**: Unit-test the counts over a marker sequence with gaps and a wrap at 255 to 0.

---

## Edge Cases

- IF the camera that supplied the session's pattern is removed THEN the session SHALL keep the pattern until Reset, so the other cameras keep measuring against it.
- IF every camera loses the pattern THEN every Stable camera keeps its reading and every Looking camera stays Looking (stories 2 and 5).
- IF two cameras show the pattern at different frame rates or from different phones THEN the second pattern's markers are ignored until Reset (story 3).
- IF the canvas frame rate changes while measuring THEN F stays as read at Start; Stop then Start reads the new rate (Out of Scope).
- IF the user ticks a camera that is the chosen mic THEN the checklist does not offer it (story 6).
- IF the saved selection names a source that is gone THEN it is ignored and the rest of the selection loads (story 6).
- WHEN Start is pressed twice quickly THEN the second press is Stop, since the button toggles (story 5).

Implicit-requirement sweep, Complex scope:

| Dimension | Covered by |
| --------- | ---------- |
| Input validation & bounds | Story 3 (QR range checks), story 1 (list capacity), story 11 (fuzz) |
| Failure / partial-failure states | Story 7 (tap failures, removed source), story 4 (mic removed), story 6 (deactivated) |
| Idempotency / retry / duplicate handling | Story 4 (repeated Stop and Reset do nothing). Duplicate markers: N/A because the detector emits one marker per flash and the pairing keeps the nearest, so a repeat cannot add a second sample for the same index |
| Auth boundaries & rate limits | N/A because the plugin has no network, no account and no caller other than the user |
| Concurrency / ordering | Story 1 (threads, order of arrival, one-instant snapshot), story 7 (a slow camera), story 12 (no detection on the UI thread) |
| Data lifecycle / expiry | Story 4 (readings live until Reset, a collection switch or exit), story 6 (selection per collection) |
| Observability | Story 10 |
| External-dependency failure | Story 7 (tap creation), story 6 (a camera that stops delivering) |
| State-transition integrity | Story 4 (invalid requests change nothing), story 2 (the camera states) |

---

## Requirement Traceability

| Requirement ID | Story | Phase | Status |
| -------------- | ----- | ----- | ------ |
| READ-01 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T2, T3) |
| READ-02 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T3) |
| READ-03 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T3) |
| READ-04 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T3) |
| READ-05 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T3) |
| READ-06 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T3) |
| READ-07 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T5) |
| READ-08 | Story 1, P1: A camera's reading comes from pairing its markers with the mic's | Execute | Done (T5) |
| READ-09 | Story 2, P1: A camera's state follows its samples | Execute | Done (T3) |
| READ-10 | Story 2, P1: A camera's state follows its samples | Execute | Done (T3) |
| READ-11 | Story 2, P1: A camera's state follows its samples | Execute | Done (T3, T4) |
| READ-12 | Story 2, P1: A camera's state follows its samples | Execute | Done (T2, T4) |
| READ-13 | Story 2, P1: A camera's state follows its samples | Execute | Done (T4) |
| READ-14 | Story 2, P1: A camera's state follows its samples | Execute | Done (T4) |
| READ-15 | Story 2, P1: A camera's state follows its samples | Execute | Done (T4) |
| READ-16 | Story 2, P1: A camera's state follows its samples | Execute | Done (T2, T4, T14) |
| READ-17 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T15) |
| READ-18 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T15) |
| READ-19 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T8) |
| READ-20 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T9) |
| READ-21 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T10) |
| READ-22 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T11) |
| READ-23 | Story 3, P1: One pattern, read by a detector per camera and one for the mic | Execute | Done (T1) |
| READ-24 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T14) |
| READ-25 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T15) |
| READ-26 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T15) |
| READ-27 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T15); rig check pending (T34) |
| READ-28 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T14, T32) |
| READ-29 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T14, T32) |
| READ-30 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T14) |
| READ-31 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T14) |
| READ-32 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T16) |
| READ-33 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T16) |
| READ-34 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T16) |
| READ-35 | Story 4, P1: A measuring session starts, stops and resets | Execute | Done (T16); rig check pending (T34) |
| READ-36 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T22) |
| READ-37 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T21) |
| READ-38 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T21) |
| READ-39 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T21) |
| READ-40 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T21) |
| READ-41 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T21) |
| READ-42 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T17, T22) |
| READ-43 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T17, T22) |
| READ-44 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T21) |
| READ-45 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T22) |
| READ-46 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T22) |
| READ-47 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T22) |
| READ-48 | Story 5, P1: The dock tells the user the state of every camera | Execute | Done (T20) |
| READ-49 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12, T27) |
| READ-50 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12, T27) |
| READ-51 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12) |
| READ-52 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12) |
| READ-53 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12, T28) |
| READ-54 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12, T28) |
| READ-55 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T18, T32); rig check pending (T34) |
| READ-56 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T18, T32); rig check pending (T34) |
| READ-57 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T7, T18); rig check pending (T34) |
| READ-58 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T18, T32) |
| READ-59 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T18, T32) |
| READ-60 | Story 6, P1: The dock lists the cameras and mics and remembers the choice | Execute | Done (T12) |
| READ-61 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Built (T30, T31); rig check pending (T35) |
| READ-62 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Built (T29, T31); rig check pending (T35) |
| READ-63 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Built (T30); rig check pending (T35) |
| READ-64 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Built (T30); rig check pending (T35) |
| READ-65 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Pending rig (T35) |
| READ-66 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Built (T29, T30); rig check pending (T34) |
| READ-67 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Done (T14, T30); rig check pending (T34) |
| READ-68 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Done (T14, T29) |
| READ-69 | Story 7, P1: Every selected camera is measured at once, without touching the user's setup | Execute | Done (T7, T30); rig check pending (T35) |
| READ-70 | Story 8, P1: The dock renders the session and forwards clicks | Execute | Built (T33) |
| READ-71 | Story 8, P1: The dock renders the session and forwards clicks | Execute | Built (T33) |
| READ-72 | Story 8, P1: The dock renders the session and forwards clicks | Execute | Built (T33) |
| READ-73 | Story 8, P1: The dock renders the session and forwards clicks | Execute | Built (T33) |
| READ-74 | Story 8, P1: The dock renders the session and forwards clicks | Execute | Built (T33) |
| READ-75 | Story 8, P1: The dock renders the session and forwards clicks | Execute | Built (T33) |
| READ-76 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T34) |
| READ-77 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T34) |
| READ-78 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T34) |
| READ-79 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T35) |
| READ-80 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T35) |
| READ-81 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T34) |
| READ-82 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T34) |
| READ-83 | Story 9, P1: The rig confirms the readings | Execute | Pending rig (T37) |
| READ-84 | Story 10, P2: Measuring is visible in the OBS log | Execute | Done (T13, T19) |
| READ-85 | Story 10, P2: Measuring is visible in the OBS log | Execute | Done (T13, T19) |
| READ-86 | Story 10, P2: Measuring is visible in the OBS log | Execute | Done (T13, T19) |
| READ-87 | Story 10, P2: Measuring is visible in the OBS log | Execute | Done (T19) |
| READ-88 | Story 10, P2: Measuring is visible in the OBS log | Execute | Done (T13) |
| READ-89 | Story 10, P2: Measuring is visible in the OBS log | Execute | Done (T19) |
| READ-90 | Story 11, P2: The core's quality gates run in CI | Execute | Done (T24) |
| READ-91 | Story 11, P2: The core's quality gates run in CI | Execute | Done (T26) |
| READ-92 | Story 11, P2: The core's quality gates run in CI | Execute | Done (T25) |
| READ-93 | Story 12, P2: Measuring stays inside the performance budget | Execute | Pending rig (T36) |
| READ-94 | Story 12, P2: Measuring stays inside the performance budget | Execute | Pending rig (T36) |
| READ-95 | Story 12, P2: Measuring stays inside the performance budget | Execute | Pending rig (T36) |
| READ-96 | Story 12, P2: Measuring stays inside the performance budget | Execute | Built (T32); rig check pending (T36) |
| READ-97 | Story 13, P3: Details show how well each camera sees the pattern | Execute | Done (T23) |
| READ-98 | Story 13, P3: Details show how well each camera sees the pattern | Execute | Done (T6) |

**ID format:** `READ-NN`. The TDD's suggested READ-01 to READ-10 name design rows: dock opened (story 6), start without camera or mic (story 5), no pattern seen (stories 2 and 5), no pattern sound (stories 3 and 5), settling (stories 2 and 5), stable (stories 2 and 5), pattern lost (story 2), removed, renamed or deactivated (story 6), Stop, Reset, collection switch (story 4), the remembered selection (story 6).

**Coverage:** 98 total, 0 mapped to tasks yet, 98 unmapped until Tasks.

---

## Success Criteria

- [ ] Two cameras of different lags, ticked together, both reach Stable in one measuring pass on the rig, and each reading equals minus Sync Dock's latency for that camera within one frame, three runs in a row.
- [ ] Every row of the design's Reading table has a unit test on WSL and in CI, and the rig run shows the same text in OBS 30.0.0 and the latest stable.
- [ ] The scene collection saved while measuring differs from the one saved while idle only in the plugin's module data.
- [ ] A camera hidden from the phone reads "Looking for the pattern" and a muted mic produces "No pattern sound from {mic} - bring the phone closer or unmute it" within 5 s.
- [ ] `ctest --preset core` passes on WSL with the new tests, and the core job's coverage gate holds at 90%.
