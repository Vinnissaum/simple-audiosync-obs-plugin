# Alignment Specification

> Phase 3 of the [technical design](../../../docs/tdd/multi-camera-sync.md#implementation-plan): Apply writes the corrections into OBS's own settings in one step, and Undo puts back what was there. It implements the design's [Alignment slice](../../../.design/multi-camera-sync.md#alignment), whose state table is the source of these criteria, and the TDD's [Alignment](../../../docs/tdd/multi-camera-sync.md#alignment) arithmetic and Apply steps. The design, the [ADRs](../../../docs/adr/README.md) and the TDD win over this spec where they disagree. Size: Large. The user decisions behind four gray areas are in [`context.md`](context.md).
>
> **Written before the rig, by decision.** Reading's rig tasks (T34 to T37) and both spikes are still open. Alignment's rules take readings as input and do not depend on where the taps sit, so the calculator, Apply, Undo and the view-model are specified and gated on WSL and CI now. Story 6 is the rig's.

## Problem Statement

Reading gives every ticked camera a stable reading against the mic, but nothing acts on it. The user still works out each camera's delay and the mic's Sync Offset by hand, types them into OBS's dialogs, and gets it wrong in the ways the forum threads show: Render Delay instead of Video Delay (Async), a negative offset where a delay was needed, one camera fixed and the others left out. Alignment closes the loop: one Apply writes every correction where OBS keeps it, or writes nothing, and one Undo takes it back.

## Goals

- [ ] One measure-and-apply pass leaves every ticked camera within one frame of the mic on the rig, three runs in a row.
- [ ] Apply writes every correction or none: a failure at any write position leaves every value as it was, in the fake-port tests and in the integration tests.
- [ ] Apply and Undo touch nothing but the plugin's own filter on each ticked camera and the mic's Sync Offset: a scene collection saved before and after Apply differs only there.
- [ ] Every row of the design's Alignment table and every test vector in the TDD runs as an OBS-free unit test on the WSL machine and in CI.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
| ------- | ------ |
| Showing each camera's current delay and the mic's Sync Offset on the rows at all times | Offered during discussion and not chosen; the message line shows the result of each Apply and Undo (`context.md`) |
| Undo of more than the last Apply, Undo after an OBS restart, Redo | Design Alignment default 1 |
| Adjusting cameras that are not ticked | Apply writes only the ticked cameras and the mic (see Assumptions) |
| Removing the plugin's filter when its delay falls to 0 | TDD: it is set to 0 and kept. Only Undo, or a failed Apply's restore, removes a filter Apply created |
| Shrinking a user's own filter, or adding or editing Render Delay | Key decisions 2 and 4 |
| Noticing a hand edit of a filter's delay or enabled state while readings are kept | Reading observes only filter add and remove and Sync Offset changes; Apply clears the readings it makes stale itself |
| The final product name, and so the final filter name | Release (TDD Open question 14); the recognised-names list absorbs a rename |
| Pattern files, packaging, the forum page | Release phase |
| The spikes, and Reading's rig tasks T34 to T37 | Reading's |
| Locales other than en-US | As in Reading |
| Drift, more than one mic, measuring without the pattern, screen, window and game captures, a camera's own mic as the reference | Design Boundary |

---

## Assumptions & Open Questions

Facts below were checked on 2026-10-20 against the repository at `f1cd39a`, the design (confirmed 2026-10-11) and the TDD. Rows marked `y` were confirmed by the maintainer; rows marked `n` are the agent's defaults, taken from the TDD where it names one, and stay open to override until the spec is confirmed.

| Assumption / decision | Chosen default | Rationale | Confirmed? |
| --------------------- | -------------- | --------- | ---------- |
| Which phase is next | Alignment (phase 3), specified while Reading's rig tasks and the spikes are open | The maintainer asked to start Alignment on 2026-10-20 after Reading's T1 to T33 passed validation | y |
| What the dock shows after Apply and Undo | A summary on the message line built from the plan or the snapshot: "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms", "Undone - ..." | Discussed (`context.md`, Showing the result) | y |
| "Already aligned" and "Aligned" | Two cases: "Already aligned" is the result of pressing Apply with every reading within one frame; "Aligned", with a check mark on each row, appears after a successful Apply once every ticked camera is stable again within one frame | Discussed (`context.md`, Aligned and Already aligned) | y |
| When the on-air question is asked and what it counts | After the reads and the plan; {n} counts the cameras whose delay changes; the mic is named only when its offset changes | Discussed (`context.md`, On-air confirmation). It moves the question after the reads in the design's sequence diagram; no write happens before the answer | y |
| The question's second sentence (TDD Open question 11) | "Cameras whose delay changes pause until their new delay fills.", shown when at least one camera changes | Accepted in the discussion's preview; TDD finding 7 | y |
| What Undo does to measuring | Clears every reading, keeps the pattern, leaves the session's state as it was | Discussed (`context.md`, Measuring after Undo) | y |
| Requirement IDs | `ALIGN-NN`, one per criterion, in story order. The TDD's suggested ALIGN-01 to ALIGN-12 are design-row labels; the traceability table maps them | As Reading did with READ-NN | n |
| The frame tolerance F | The frame interval the session read at its last Start (READ-16): `1000 × fps_den ÷ fps_num` ms. A reading r is within one frame when −F ≤ r ≤ F | The TDD's arithmetic step 2; the same F Reading uses for stability | n |
| "Checklist order" | Alphabetical by source name, ignoring case, as the camera checklist is (READ-49) | The order the user sees, so "Waiting for {camera}" names the first row that is waiting | n |
| The Sync Offset in the arithmetic | s is the mic's Sync Offset in ms, rounded half away from zero from its nanoseconds. Apply writes the new offset as whole ms (`(s + Δ) × 1,000,000` ns). Undo restores the nanoseconds read before Apply exactly | OBS's dialog sets whole ms; the TDD asks Undo to restore "to the nanosecond". An unchanged offset is not written, so a sub-ms offset survives an Apply that leaves the mic alone | n |
| A disabled plugin filter | Its effective delay is 0, because a disabled filter delays nothing and the reading shows that. When the plan gives the camera a non-zero delay, Apply sets the delay and enables the filter; Undo puts back both the delay and the disabled state | The filter is the plugin's own (Key decision 4). Writing a delay into a disabled filter would never converge | n |
| Which filter is the plugin's | The first filter on the camera, in filter order, whose type is Video Delay (Async) (`async_delay_filter`) and whose name is on the recognised-names list. The list holds "Camera Sync Delay" | TDD "The plugin's filter" and ADR-004. A recognised name on another filter type is the user's | n |
| What a change during the on-air question does | After the user confirms, Apply resolves and reads every value again. If a source is gone or a value differs from the first read, it writes nothing and shows "Apply failed - nothing changed" | The question waits on the user, and OBS keeps running meanwhile. Writing a plan computed from values that have since changed would break Key decision 3 | n |
| Which readings Apply uses | The readings when Apply is pressed. A reading that changes while the question is open does not change the plan | The question is about the plan the user saw | n |
| Readings after a failed Apply | If Apply wrote anything before it failed, every reading is cleared and the session's state is kept. A failure before the first write leaves the readings as they were | Apply's own writes fire the filter and Sync Offset signals Reading obeys (READ-58, READ-59), so some readings would drop anyway; clearing all makes the result the same whichever write failed | n |
| The pattern after Apply or Undo | Kept. Only the readings are cleared | The same phone keeps playing; Reset is how the user drops the pattern (READ-27) | n |
| The Undo snapshot after a failed Apply | A failed Apply whose restore succeeds keeps the earlier Apply's Undo snapshot. A failed restore drops it | After a clean restore the setup is the one the earlier snapshot was taken from; after a failed restore it is not | n |
| Undo while on air | Asks first, with the same wording rules as Apply | Undo pauses cameras the same way (TDD finding 7), and design default 2 asks before an on-air change | n |
| Undo when a filter changed by hand since Apply | A filter Apply created and the user deleted is not there to remove, which is no failure. A filter Apply created and the user renamed is no longer the plugin's and is left alone. A filter that existed before Apply and was deleted since is created again with its snapshot delay and enabled state | ADR-008: Undo restores the earlier values exactly and overwrites hand edits | n |
| A refused Undo | A source that does not resolve refuses Undo with nothing written and keeps the snapshot, so Undo can be tried again | A source may be back after a scene change; the snapshot is dropped only by the events the TDD lists | n |
| Cameras that are not ticked | Not read or written, even when Apply changes the mic's Sync Offset, which moves a camera aligned earlier and unticked since | Apply aligns the cameras the user ticked. Selections persist per collection (READ-54), so a camera aligned earlier is usually still ticked | n |
| The question for one camera and for the mic alone | Separate en-US strings: "Shift 1 camera and the mic while on air?", "Shift 1 camera while on air?", "Shift the mic while on air?" | OBS's locale files have no plural rules; separate keys keep each sentence whole | n |
| How long an Apply or Undo message stays | Until the next Apply or Undo request, Reset or collection switch, below the messages that outrank it | The result stays readable while measuring resumes | n |
| The message line's precedence | Highest first: "Pick at least one camera and the mic", "Could not listen to {mic}", "No pattern sound from {mic} - ...", "Aligned", the last Apply or Undo result, "Waiting for {camera}" | A problem the user must fix outranks a result; "Aligned" replaces the "Applied" summary it confirms; "Waiting for {camera}" is the most common and least urgent | n |
| Check marks before every row has one | After a successful Apply, each row gets its check mark as soon as that camera is stable within one frame; "Aligned" appears once every ticked row has one | The design shows "each camera ticked" in the Aligned state; marking rows one by one shows progress | n |
| The check mark | "✓ " (U+2713 and a space) before the row's status | The design's "ticked" next to the existing status text | n |
| On air (TDD Open question 12) | Streaming active, or recording active including paused. The virtual camera and the replay buffer alone are not on air | The design names streaming and recording; a paused recording resumes into the same file | n |
| Failed restore and failed Undo text (TDD Open question 8) | "Apply failed - check the log" when a restore fails; "Undo failed - nothing changed" when Undo fails and its restore holds; "Undo failed - check the log" when Undo's restore fails | The TDD's proposals, with the last one following the same pattern | n |
| A plugin filter for a new delay of 0 (TDD Open question 10) | Not created. An existing one is set to 0 and kept | TDD default | n |
| Video Delay (Async) not available (obs-filters not loaded) | Apply and Undo refuse before any write, whether or not they need a new filter | TDD Error handling. An existing filter of an unloaded type delays nothing, so writing it would be a false success | n |
| Integration tests (amended 2026-10-20, Design) | A Linux CI job with headless libobs, OBS 30.0.2's Video Delay (Async) compiled from its source into the test, and a test async source. If libobs cannot start headless on CI, the same tests run on the rig and are recorded in `validation.md` | The TDD's Testing Strategy, with its own fallback. The Ubuntu build compiles libobs with plugins off, so obs-filters as a module would need every plugin's dependencies; the filter's own source needs only libobs | y |
| Log lines | The formats in story 7, in the key=value style of Reading's lines | The TDD's Monitoring section gives `apply.plan`, `apply.result` and `undo.result`; the rest follow it | n |
| Confirmed lessons | None exist (`.specs/lessons.json` holds candidates only, which are not loaded) | Skill rule: load confirmed lessons only | y |

**Open questions:** none - all resolved or logged above (required before the spec is confirmed).

---

## User Stories

### Story 1 · P1: The calculator turns readings into an alignment ⭐ MVP

**User Story**: As a multi-camera user, I want my readings turned into a delay for each camera and a Sync Offset for the mic, so that every camera lines up with the mic with the least latency added and no negative value.

**Why P1**: Every other story writes, counts or shows what the calculator decides. Key decisions 2 and 3 live here.

*Design rows: a selected camera has no stable reading; every camera already within one frame; a correction past OBS's limits; re-measured after Apply, a camera still off. Core, OBS-free, pure: the same inputs always give the same output.*

**Acceptance Criteria**:

1. IF any ticked camera is not Stable THEN the calculator SHALL refuse with "waiting" and name the first such camera in checklist order. **ALIGN-01**
2. WHEN every ticked camera is Stable and every reading r satisfies −F ≤ r ≤ F THEN the calculator SHALL return "already aligned" and no plan. **ALIGN-02**
3. The calculator SHALL take each camera's current delay dᵢ as its effective delay: the plugin filter's delay when the filter is present and enabled, and 0 when it is absent or disabled. **ALIGN-03**
4. The calculator SHALL set Δ to the largest of every (rᵢ − dᵢ) and −s, where s is the mic's Sync Offset in ms rounded half away from zero from its nanoseconds. **ALIGN-04**
5. The calculator SHALL give each ticked camera the new delay dᵢ′ = dᵢ + Δ − rᵢ and the mic the new Sync Offset s′ = s + Δ. **ALIGN-05**
6. The calculator SHALL produce no negative new delay and no negative new Sync Offset. **ALIGN-06**
7. IF any new delay or the new Sync Offset is above 20,000 ms THEN the calculator SHALL refuse with "over the limit" and name the first such source, the cameras in checklist order and then the mic; a value of exactly 20,000 ms SHALL be accepted. **ALIGN-07**
8. WHEN the inputs are any of the TDD's eight test vectors at F = 33 ms THEN the calculator SHALL return exactly that row's Δ and new values, or "already aligned" for the last row. **ALIGN-08**
9. The calculator SHALL mark a camera as changed when its new delay differs from its effective delay, and the mic as changed when s′ differs from s. **ALIGN-09**
10. WHEN a camera has no plugin filter and its new delay is 0 THEN the plan SHALL not create a filter for it. **ALIGN-10**

**Independent Test**: Unit-test the calculator with the TDD's eight vectors, a single camera, a disabled filter carrying 60 ms, a Sync Offset of −200,400,000 ns, new values of exactly 20,000 and 20,001 ms on a camera and on the mic, and a Settling camera between two Stable ones.

---

### Story 2 · P1: Apply writes the alignment all or nothing ⭐ MVP

**User Story**: As a multi-camera user, I want one Apply to write every correction into OBS, or none if anything goes wrong, so that my cameras never disagree with each other on air because of a half-applied alignment.

**Why P1**: Key decisions 1, 4 and 7. A half-applied alignment is worse than none (design).

*Design rows: a selected camera has no stable reading; every camera already within one frame; streaming or recording is active; Apply succeeds; a write fails partway. Core over the settings port (story 4), tested against a fake port that can fail any chosen read or write.*

**Acceptance Criteria**:

1. IF Apply is requested with no mic chosen, no camera ticked, or a ticked camera that is not Stable THEN Apply SHALL resolve, read and write nothing. **ALIGN-11**
2. WHEN Apply is requested and the calculator returns "already aligned" THEN Apply SHALL resolve, read and write nothing, and report "already aligned". **ALIGN-12**
3. IF a ticked camera or the mic does not resolve by its UUID THEN Apply SHALL write nothing and report "failed". **ALIGN-13**
4. IF Video Delay (Async) is not available in OBS THEN Apply SHALL write nothing and report "failed". **ALIGN-14**
5. WHEN every source resolves THEN Apply SHALL read each ticked camera's plugin filter (absent, or present with its delay and enabled state) and the mic's Sync Offset in nanoseconds, compute the plan from them, and keep them as the restore point. **ALIGN-15**
6. IF the calculator refuses with "over the limit" THEN Apply SHALL write nothing and report "over the limit" with the named source. **ALIGN-16**
7. WHILE streaming or recording is active, WHEN the plan is ready THEN Apply SHALL ask for confirmation with the plan's counts (story 5) before writing anything. **ALIGN-17**
8. IF the confirmation is cancelled THEN Apply SHALL write nothing and report "cancelled". **ALIGN-18**
9. WHEN the confirmation is accepted THEN Apply SHALL resolve and read every value again, and IF a source is gone or any value differs from the first read THEN it SHALL write nothing and report "failed". **ALIGN-19**
10. WHEN Apply writes THEN it SHALL write only the values the plan marks as changed, the ticked cameras first in checklist order and then the mic. **ALIGN-20**
11. WHEN a changed camera has no plugin filter THEN Apply SHALL create a Video Delay (Async) filter named "Camera Sync Delay" at the end of that camera's filter list, enabled, with the new delay. **ALIGN-21**
12. WHEN a changed camera has a plugin filter THEN Apply SHALL set its delay to the new delay, including 0, and enable it, and SHALL not remove it. **ALIGN-22**
13. WHEN the mic is changed THEN Apply SHALL set its Sync Offset to s′ × 1,000,000 ns. **ALIGN-23**
14. Apply SHALL read back each value right after writing it, and SHALL treat a write as failed when the port reports a failure or the value read back differs from the value written. **ALIGN-24**
15. IF a write fails THEN Apply SHALL write nothing more from the plan, put back every value it changed from the restore point in reverse order of writing, remove every filter it created, and report "failed". **ALIGN-25**
16. IF a write made while putting values back fails THEN Apply SHALL still put back the remaining values, report "restore failed", and drop the Undo snapshot. **ALIGN-26**
17. WHEN every write lands THEN Apply SHALL keep the restore point as the Undo snapshot, replacing any earlier one, and report "applied" with the plan. **ALIGN-27**
18. WHEN every write lands THEN Apply SHALL clear every reading, keep the pattern, and leave the session Measuring, starting it with the current canvas frame rate if it was Stopped. **ALIGN-28**
19. IF Apply fails after writing at least one value THEN it SHALL clear every reading and keep the pattern and the session's state; a refusal or failure before the first write SHALL leave every reading as it was. **ALIGN-29**
20. IF Apply reports "failed" THEN the Undo snapshot of an earlier Apply SHALL stay as it was. **ALIGN-30**

**Independent Test**: Drive Apply in a unit test against a fake port and a fake session: every refusal leaves the fake's call log empty of writes (and of reads for ALIGN-11 and ALIGN-12); the first vector writes Cam B, then the mic, and skips Cam A; a failure injected at each write position, and at each read-back, leaves every fake value as it was and no created filter; a failure injected into a restore write still restores the rest and drops Undo; on air, a cancel writes nothing and a value changed while the question is open writes nothing.

---

### Story 3 · P1: Undo puts back what was there ⭐ MVP

**User Story**: As a multi-camera user, I want one Undo to put back exactly the values from before my last Apply, so that I can try an alignment and take it back without remembering what was there.

**Why P1**: Key decision 7: Apply can be undone. ADR-008 makes Undo all-or-nothing too.

*Design rows: Undo; Undo after OBS was restarted. Core over the settings port, on the same resolve, write, verify and restore path as Apply.*

**Acceptance Criteria**:

1. IF Undo is requested with no Undo snapshot THEN Undo SHALL resolve, read and write nothing. **ALIGN-31**
2. The Undo snapshot SHALL live in memory only, and SHALL be dropped when the scene collection changes, when OBS exits, when a later Apply replaces it, and when Undo succeeds. **ALIGN-32**
3. IF a source in the snapshot does not resolve, or Video Delay (Async) is not available, THEN Undo SHALL write nothing, report "failed" and keep the snapshot. **ALIGN-33**
4. WHEN every source in the snapshot resolves THEN Undo SHALL read each snapshot camera's plugin filter and the mic's Sync Offset as its restore point. **ALIGN-34**
5. WHILE streaming or recording is active, WHEN Undo is ready to write THEN it SHALL ask for confirmation with the counts of the values it would change (story 5); a cancel SHALL write nothing and keep the snapshot. **ALIGN-35**
6. WHEN Undo writes THEN, for each camera in the snapshot in checklist order, it SHALL remove the plugin filter if Apply created it and it is still there, or set the filter's delay and enabled state to the snapshot's, creating the filter at the end of the camera's filter list if it is gone; and then it SHALL set the mic's Sync Offset to the snapshot's nanoseconds exactly. **ALIGN-36**
7. Undo SHALL write only the values that differ from its restore point, and SHALL write the snapshot's values even where they were changed by hand after Apply. **ALIGN-37**
8. Undo SHALL read back each value right after writing it, and IF a write fails THEN it SHALL put back every value it changed from its restore point in reverse order, remove any filter it created, report "failed" and keep the snapshot. **ALIGN-38**
9. IF a write made while putting values back fails THEN Undo SHALL still put back the remaining values, report "restore failed" and drop the snapshot. **ALIGN-39**
10. WHEN every write lands THEN Undo SHALL drop the snapshot, clear every reading, keep the pattern and the session's state, and report "undone" with the restored values. **ALIGN-40**

**Independent Test**: Apply the hand-set negative Sync Offset vector (s = −200,000,000 ns) and a vector that creates a filter, then Undo: the fake port holds −200,000,000 ns and no filter. Undo after a hand edit restores the snapshot. A failure at each Undo write position leaves the post-Apply values. A collection switch, a second Apply and a successful Undo each leave no snapshot.

---

### Story 4 · P1: The settings port touches only the plugin's filter and the mic's Sync Offset ⭐ MVP

**User Story**: As a multi-camera user, I want the plugin to change nothing in OBS but its own filter on each camera and the mic's Sync Offset, so that the filters and settings I built stay exactly as I left them.

**Why P1**: Key decision 4, made structural: the port has no operation that reaches anything else (TDD Internal contracts).

*Design table "What Apply writes". The contract is core; the OBS implementation is an adapter, gated by the CI build, the integration tests (story 8) and the rig (story 6).*

**Acceptance Criteria**:

1. The settings port SHALL offer only these operations: resolve a source by UUID; read a camera's plugin filter; write a camera's plugin filter's delay and enabled state, creating the filter if it is absent; remove a camera's plugin filter; read the mic's Sync Offset; write the mic's Sync Offset; say whether streaming or recording is active; say whether Video Delay (Async) is available. **ALIGN-41**
2. The port SHALL take as a camera's plugin filter the first filter on it, in filter order, whose type is Video Delay (Async) (`async_delay_filter`) and whose name is on the recognised-names list, which holds "Camera Sync Delay". **ALIGN-42**
3. The port SHALL never edit, reorder or remove a filter that is not the camera's plugin filter, including a filter with a recognised name and another type, and a Video Delay (Async) filter with another name. **ALIGN-43**
4. The port SHALL write the plugin filter's delay as its `delay_ms` setting in whole milliseconds from 0 to 20,000, and the mic's Sync Offset in nanoseconds. **ALIGN-44**
5. The port SHALL report on air WHILE streaming is active or recording is active, including a paused recording, and SHALL not report on air for the virtual camera or the replay buffer alone. **ALIGN-45**
6. The port SHALL never add or edit Render Delay, and SHALL never change a scene, an audio track, any property of the mic other than its Sync Offset, or any source other than the ticked cameras' plugin filters and the mic. **ALIGN-46**

**Independent Test**: In the integration tests (story 8): a test source with a user's own Video Delay (Async) filter named "Delay", a Color Correction filter named "Camera Sync Delay" and a Render Delay; Apply and Undo; every one of them is unchanged and in the same order, and the plugin filter is created after them. The fake port used by stories 2 and 3 implements exactly the operations of ALIGN-41.

---

### Story 5 · P1: The dock offers Apply and Undo and says what happened ⭐ MVP

**User Story**: As a multi-camera user, I want Apply and Undo in the dock, a clear question before anything changes on air, and a line that says what was written, so that I always know what the plugin did to my setup.

**Why P1**: The design's "Dock shows" column for every Alignment row. The rules stay in the view-model, where they are tested.

*Design rows: every "Dock shows" cell of the Alignment table, and the four decisions in `context.md`. View-model in the core; all text from the locale file; the Qt dock renders it.*

**Acceptance Criteria**:

1. WHILE a mic is chosen, at least one camera is ticked and every ticked camera is Stable, the view-model SHALL enable Apply. **ALIGN-47**
2. WHILE a mic is chosen and a ticked camera is not Stable, the view-model SHALL disable Apply and, when no message of higher precedence applies, show "Waiting for {camera}" on the message line with the first such camera's name in checklist order. **ALIGN-48**
3. The view-model SHALL enable Undo while an Undo snapshot exists and disable it otherwise. **ALIGN-49**
4. WHEN Apply reports "already aligned" THEN the message line SHALL read "Already aligned". **ALIGN-50**
5. WHEN Apply reports "over the limit" THEN the message line SHALL read "{camera} needs more delay than OBS allows" with the named source's name, which is the mic's name when the mic is the source. **ALIGN-51**
6. WHEN Apply reports "failed" THEN the message line SHALL read "Apply failed - nothing changed". **ALIGN-52**
7. WHEN Apply reports "restore failed" THEN the message line SHALL read "Apply failed - check the log". **ALIGN-53**
8. WHEN Apply reports "applied" THEN the message line SHALL read "Applied - " followed by each ticked camera's new delay in checklist order and then the mic's new Sync Offset, each as "{name} {value} ms", separated by ", " (for example "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms"). **ALIGN-54**
9. WHEN Undo reports "undone" THEN the message line SHALL read "Undone - " followed by each snapshot camera's restored delay in checklist order, 0 for a removed filter, and then the mic's restored Sync Offset rounded half away from zero to whole ms, in the same form; a negative value SHALL carry a leading "−" (U+2212) and a non-negative one no sign. **ALIGN-55**
10. WHEN Undo reports "failed" THEN the message line SHALL read "Undo failed - nothing changed", and WHEN it reports "restore failed" THEN "Undo failed - check the log". **ALIGN-56**
11. The view-model SHALL keep the last Apply or Undo message until the next Apply or Undo request, Reset or collection switch, and a cancelled question SHALL leave the previous message as it was. **ALIGN-57**
12. WHILE the last successful Apply has not been followed by an Undo, a Reset, a collection switch or a failed restore, a ticked camera that is Stable with −F ≤ reading ≤ F SHALL show its status prefixed with "✓ " (U+2713 and a space), for example "✓ +4 ms - video late". **ALIGN-58**
13. WHILE every ticked camera shows the check mark, the message line SHALL read "Aligned". **ALIGN-59**
14. The message line SHALL show the highest of: "Pick at least one camera and the mic", "Could not listen to {mic}", "No pattern sound from {mic} - ...", "Aligned", the last Apply or Undo message, "Waiting for {camera}". **ALIGN-60**
15. The on-air question SHALL count {n} as the cameras whose value changes, and SHALL read "Shift {n} cameras and the mic while on air?" for n ≥ 2 with the mic changed, "Shift 1 camera and the mic while on air?" for one camera with the mic changed, "Shift {n} cameras while on air?" for n ≥ 2 with the mic unchanged, "Shift 1 camera while on air?" for one camera with the mic unchanged, and "Shift the mic while on air?" for no camera with the mic changed. **ALIGN-61**
16. WHEN n is 1 or more THEN the on-air question SHALL add a second line, "Cameras whose delay changes pause until their new delay fills." **ALIGN-62**
17. The view-model SHALL resolve every string this story adds through a key in the en-US locale file, and every key it uses SHALL exist there (extends READ-48). **ALIGN-63**
18. The dock SHALL show an Apply button and an Undo button, labelled "Apply" and "Undo", next to Start/Stop and Reset, forward their clicks, and show the on-air question as a modal dialog with "Apply" and "Cancel" buttons. **ALIGN-64**

**Independent Test**: Unit-test the view-model over snapshots and Apply and Undo results: one case per Alignment row asserting the exact message, the button states and the row text; the five question wordings and the second line; a precedence case for each adjacent pair in ALIGN-60; a locale check over the new keys. Build the dock in CI and open it on the rig.

---

### Story 6 · P1: The rig confirms the alignment ⭐ MVP

**User Story**: As the maintainer, I want Apply and Undo run on the real rig in the oldest and newest supported OBS, so that I know every camera ends within one frame of the mic and nothing else in OBS changed.

**Why P1**: The design's Success line is a rig measurement; the unit tests cannot prove that OBS applies what the port writes.

*Design rows: re-measured after Apply, every camera within one frame; re-measured after Apply, a camera still off. Needs Reading's rig tasks first.*

**Acceptance Criteria**:

1. WHEN two cameras with clearly different lags, one carrying a Render Delay, and the mic are measured to Stable on the rig and Apply is pressed THEN every camera SHALL re-measure within one frame of the mic, in each of three runs in a row, in OBS 30.0.0 and in the latest stable OBS. **ALIGN-65**
2. WHEN the scene collection is saved before and after Apply THEN the two files SHALL differ only in each changed camera's plugin filter (added, or its `delay_ms` and enabled state) and the mic's sync offset. **ALIGN-66**
3. WHEN Undo is pressed after Apply THEN the scene collection saved afterwards SHALL equal the one saved before Apply. **ALIGN-67**
4. WHEN the plugin is removed after an Apply and OBS is restarted THEN the scene collection SHALL load with every plugin filter and the mic's Sync Offset in place and working (Key decision 1). **ALIGN-68**
5. WHEN every row of the design's Alignment table except "a write fails partway" is walked on the rig through interactive UAT THEN the dock SHALL show the text this spec gives for it, including the on-air question while recording. **ALIGN-69**

**Independent Test**: The rig protocol in the TDD's Testing Strategy, steps 1 to 4, plus a scene-collection diff before Apply, after Apply and after Undo, recorded in `validation.md` with the OBS versions, canvas resolution and frame rate, and devices.

---

### Story 7 · P2: Apply and Undo are visible in the OBS log

**User Story**: As the maintainer answering a forum report, I want every plan, result and refusal in the log, so that I can tell from a user's log what Apply wrote and how to put it back by hand.

**Why P2**: Support and the manual way back after a crash depend on it (ADR-008); no user flow does.

*TDD Monitoring & Observability. Formatting in the core, through the injected logger, as Reading's lines are.*

**Acceptance Criteria**:

1. WHEN Apply is about to write THEN it SHALL log at Info `apply.plan`, then for each ticked camera `camera="<name>" delay_ms=<old>-><new>`, with `none` as the old value for an absent filter and `<value>(off)` for a disabled one, then `mic="<name>" sync_offset_ms=<old>-><new>`. **ALIGN-70**
2. WHEN every write lands THEN Apply SHALL log at Info `apply.result ok written=<n> skipped=<m> duration_ms=<d>`, where n counts the values written, m the values left unchanged, and d the whole milliseconds from the request (after any question) to the result. **ALIGN-71**
3. WHEN a write fails THEN Apply SHALL log at Warning `apply.result failed step=camera:"<name>"` or `step=mic:"<name>"` for the failed write, followed by `restored=yes` or `restored=no`. **ALIGN-72**
4. WHEN a write made while putting values back fails THEN Apply SHALL log at Warning `restore.failed camera="<name>" intended_ms=<value> actual_ms=<value>` or `restore.failed mic="<name>" intended_ns=<value> actual_ns=<value>`, with `none` for an absent filter. **ALIGN-73**
5. WHEN Apply refuses THEN it SHALL log at Warning `apply.refused reason=waiting camera="<name>"`, `reason=source_gone source="<uuid>"`, `reason=no_delay_filter`, `reason=over_limit source="<name>" value_ms=<value>` or `reason=changed`, and SHALL log at Info `apply.already_aligned` and `apply.cancelled`. **ALIGN-74**
6. Undo SHALL log the same lines with the prefix `undo.` in place of `apply.`, its plan's new values being the snapshot's, and `undo.result ok ...` on success. **ALIGN-75**
7. Every source name in these lines SHALL be written with each `"` as `\"`, as Reading's lines do (READ-88). **ALIGN-76**

**Independent Test**: Unit-test the lines with a fake logger over the first vector, a failure with restored=yes, a failed restore, each refusal, and a camera named `Cam "B"`.

---

### Story 8 · P2: Apply and Undo run against real OBS objects in CI

**User Story**: As the maintainer, I want Apply and Undo tested against a real libobs, so that the settings port's OBS implementation is proven without waiting for the rig.

**Why P2**: The fake port proves the logic; only libobs proves the filter and Sync Offset calls. The rig covers it if CI cannot.

*TDD Testing Strategy, Integration row. An adapter test, Linux only.*

**Acceptance Criteria**:

1. A Linux CI job SHALL start libobs headless with OBS 30.0.2's Video Delay (Async) registered, compiled from `plugins/obs-filters/async-delay-filter.c` of the OBS sources the build downloads, and run Apply and Undo against a test async source and a test audio source: create, find, update and remove the plugin filter; read and write the Sync Offset; check each against the value set. **ALIGN-77**
2. WHEN the test source is removed between the reads and the writes of an Apply THEN the integration test SHALL see every value on the remaining sources restored and no plugin filter left that Apply created. **ALIGN-78**
3. The integration test SHALL check that a user's own Video Delay (Async) filter, a filter of another type with a recognised name, and a Render Delay on the test source are unchanged and in the same order after Apply and after Undo. **ALIGN-79**
4. IF libobs cannot start headless on CI THEN the same checks SHALL run on the rig and be recorded in `validation.md`. **ALIGN-80**

**Independent Test**: The CI job's log; or, under ALIGN-80, the rig run in `validation.md`.

---

### Story 9 · P2: Apply is quick

**User Story**: As a multi-camera user, I want Apply to finish at once, so that nothing visibly hangs while it writes.

**Why P2**: TDD Performance Requirements; it affects no rule.

**Acceptance Criteria**:

1. WHEN Apply writes a plan for four cameras and the mic on the rig THEN the `duration_ms` of its `apply.result` line SHALL be under 100. **ALIGN-81**

**Independent Test**: The rig log of story 6's runs with four cameras ticked.

---

## Edge Cases

- WHEN only one camera is ticked THEN Apply works as for several: Δ = max(r − d, −s) (story 1).
- WHEN a ticked camera carries a plugin filter from an earlier session THEN its delay is read as dᵢ like any other (story 1); only the Undo snapshot is lost across a restart.
- IF the user renamed the plugin filter THEN it is the user's filter, measured in the reading, and the next Apply creates a new plugin filter if the camera needs one (ALIGN-42, ALIGN-43).
- IF a filter of another type carries the name "Camera Sync Delay" THEN it is the user's, and Apply creates the plugin filter after it under the same name (ALIGN-42, ALIGN-21).
- IF a reading changes while the on-air question is open THEN the plan the user saw is the one written, provided the OBS values did not change (ALIGN-19).
- IF the scene collection switches while the on-air question is open THEN the sources no longer resolve after the answer and Apply writes nothing (ALIGN-19).
- WHEN Apply is pressed with every reading within one frame but the mic's Sync Offset hand-set negative THEN nothing is written ("Already aligned" is decided first, TDD step 2), and the negative offset stays.
- WHEN the mic is the slowest path THEN the cameras are delayed to it and the mic's Sync Offset does not change (vector 2), so the question names no mic.
- WHEN the plugin filter is disabled and the plan gives the camera 0 THEN nothing is written for it: a disabled filter already delays nothing (ALIGN-03, ALIGN-09).
- IF a camera aligned by an earlier Apply is unticked and Apply then changes the mic's Sync Offset THEN that camera is left out of sync; the plugin does not touch it (Assumptions).
- WHEN Apply is pressed again right after a successful Apply THEN Apply is disabled, because every reading was cleared and no camera is Stable yet (ALIGN-28, ALIGN-47).
- WHEN Undo is pressed twice THEN the second press does nothing: the first dropped the snapshot (ALIGN-40, ALIGN-31).

Implicit-requirement sweep, Large scope:

| Dimension | Covered by |
| --------- | ---------- |
| Input validation & bounds | Story 1 (20,000 ms limit on cameras and the mic, nothing negative, whole ms), story 4 (the `delay_ms` range) |
| Failure / partial-failure states | Story 2 (a gone source, a failed write or read-back, restore in reverse, a failed restore), story 3 (the same for Undo), story 8 (a source removed mid-Apply) |
| Idempotency / retry / duplicate handling | Story 1 (Apply again adds only the remainder, vector 3; "already aligned" writes nothing), story 2 (unchanged values are not written), story 3 (Undo runs once; a refused Undo keeps its snapshot to retry) |
| Auth boundaries & rate limits | N/A because the plugin has no network, no account and no caller other than the user. The write boundary inside OBS is story 4 |
| Concurrency / ordering | Story 2 (cameras then mic; restore in reverse; values read again after the question), story 9 (one quick call). Thread placement is the TDD's (every write on the UI thread) and is left to Design |
| Data lifecycle / expiry | Story 3 (the snapshot lives in memory until a collection switch, exit, a later Apply or a successful Undo), story 5 (how long a message and the check marks last) |
| Observability | Story 7 |
| External-dependency failure | Story 2 and story 3 (Video Delay (Async) not available; a write OBS refuses or ignores, caught by the read-back) |
| State-transition integrity | Story 2 (the refusals, measuring resumed only on success), story 3 (Undo only with a snapshot), story 5 (Apply and Undo enabled only when they can run) |

---

## Requirement Traceability

| Requirement ID | Story | Phase | Status |
| -------------- | ----- | ----- | ------ |
| ALIGN-01 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-02 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-03 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-04 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-05 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-06 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-07 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-08 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-09 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-10 | Story 1, P1: The calculator turns readings into an alignment | - | Pending |
| ALIGN-11 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-12 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-13 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-14 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-15 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-16 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-17 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-18 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-19 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-20 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-21 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-22 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-23 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-24 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-25 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-26 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-27 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-28 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-29 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-30 | Story 2, P1: Apply writes the alignment all or nothing | - | Pending |
| ALIGN-31 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-32 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-33 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-34 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-35 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-36 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-37 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-38 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-39 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-40 | Story 3, P1: Undo puts back what was there | - | Pending |
| ALIGN-41 | Story 4, P1: The settings port touches only the plugin's filter and the mic's Sync Offset | - | Pending |
| ALIGN-42 | Story 4, P1: The settings port touches only the plugin's filter and the mic's Sync Offset | - | Pending |
| ALIGN-43 | Story 4, P1: The settings port touches only the plugin's filter and the mic's Sync Offset | - | Pending |
| ALIGN-44 | Story 4, P1: The settings port touches only the plugin's filter and the mic's Sync Offset | - | Pending |
| ALIGN-45 | Story 4, P1: The settings port touches only the plugin's filter and the mic's Sync Offset | - | Pending |
| ALIGN-46 | Story 4, P1: The settings port touches only the plugin's filter and the mic's Sync Offset | - | Pending |
| ALIGN-47 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-48 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-49 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-50 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-51 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-52 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-53 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-54 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-55 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-56 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-57 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-58 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-59 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-60 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-61 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-62 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-63 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-64 | Story 5, P1: The dock offers Apply and Undo and says what happened | - | Pending |
| ALIGN-65 | Story 6, P1: The rig confirms the alignment | - | Pending |
| ALIGN-66 | Story 6, P1: The rig confirms the alignment | - | Pending |
| ALIGN-67 | Story 6, P1: The rig confirms the alignment | - | Pending |
| ALIGN-68 | Story 6, P1: The rig confirms the alignment | - | Pending |
| ALIGN-69 | Story 6, P1: The rig confirms the alignment | - | Pending |
| ALIGN-70 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-71 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-72 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-73 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-74 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-75 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-76 | Story 7, P2: Apply and Undo are visible in the OBS log | - | Pending |
| ALIGN-77 | Story 8, P2: Apply and Undo run against real OBS objects in CI | - | Pending |
| ALIGN-78 | Story 8, P2: Apply and Undo run against real OBS objects in CI | - | Pending |
| ALIGN-79 | Story 8, P2: Apply and Undo run against real OBS objects in CI | - | Pending |
| ALIGN-80 | Story 8, P2: Apply and Undo run against real OBS objects in CI | - | Pending |
| ALIGN-81 | Story 9, P2: Apply is quick | - | Pending |

**ID format:** `ALIGN-NN`. The TDD's suggested ALIGN-01 to ALIGN-12 name design rows: no stable reading (ALIGN-01, ALIGN-11, ALIGN-48), already aligned (ALIGN-02, ALIGN-12, ALIGN-50), on air (ALIGN-17, ALIGN-18, ALIGN-61, ALIGN-62), past OBS's limits (ALIGN-07, ALIGN-16, ALIGN-51), Apply succeeds (ALIGN-27, ALIGN-28, ALIGN-54), a write fails partway (ALIGN-25, ALIGN-26, ALIGN-52, ALIGN-53), re-measured within one frame (ALIGN-58, ALIGN-59, ALIGN-65), re-measured still off (ALIGN-08, vector 3), Undo (ALIGN-36 to ALIGN-40, ALIGN-55), Undo after a restart (ALIGN-32, ALIGN-49), one plugin filter per camera found by name (ALIGN-42, ALIGN-43), the mic's Sync Offset never negative (ALIGN-06).

**Coverage:** 81 total, 0 mapped to tasks yet, 81 unmapped until Tasks.

---

## Success Criteria

- [ ] On the rig, after one measure-and-apply pass, every ticked camera reads within one frame of the mic, three runs in a row, in OBS 30.0.0 and the latest stable.
- [ ] Every row of the design's Alignment table and all eight TDD test vectors have a unit test on WSL and in CI.
- [ ] A failure injected at every write position of Apply and of Undo leaves every value as it was, with no filter left behind.
- [ ] The scene collection saved after Apply differs from the one saved before only in the plugin filters and the mic's sync offset, and after Undo it equals the one saved before Apply.
- [ ] `ctest --preset core` passes on WSL with the new tests, and the core job's coverage gate holds at 90%.
