# Alignment checks

Mutations: 3
Build: `cmake --preset core`; `cmake --build --preset core`
Plan: `.specs/features/alignment/plan.md`

Every command runs from the repository root with `export PATH=$HOME/.local/bin:$PATH` first, as
`CLAUDE.md` says for the WSL machine. A `walk - CI` proof is closed by the CI run that the Handoff's
walk line names (`gh` is not installed here, so the maintainer reports the run); a `walk - rig` proof
by the maintainer's rig session recorded there. That line is written only once both have run.

## Intent

89 checks in 10 slices · 2 one-way doors · 1 open, of which 0 block

## Checks

### S1 - The calculator turns readings into an alignment

**C1** - Precheck refuses with "waiting" and the index of the first camera in order that is not Stable, before looking at any reading: Stable, Settling, Stable → 1; Looking, Settling → 0; a Settling camera within ±F → waiting (AC 1)
Proof: `ctest --preset core -R "^precheck waits for the first camera in order that is not stable$" --no-tests=error`

**C2** - Precheck returns "already aligned" exactly when every reading lies in −F ≤ r ≤ F: at F 33.333 (+20, −30) and (+33, −33) are aligned, (+34, 0) and (0, −34) are not; at F 33.0, +33 is aligned and +34 is not (AC 2)
Proof: `ctest --preset core -R "^precheck finds every reading within one frame already aligned$" --no-tests=error`

**C3** - The plan takes a camera's effective delay as 0 for an absent or disabled filter and the delay for an enabled one: r (+100, +40) with a disabled 60 ms filter on the first camera gives Δ 100, d′ (0, 60) and the first camera unchanged; the same filter enabled gives Δ 40 (AC 3)
Proof: `ctest --preset core -R "^plan takes the effective delay of an absent, enabled or disabled filter$" --no-tests=error`

**C4** - The plan rounds the Sync Offset half away from zero from ns before taking Δ: −200,400,000 ns → −200 ms, −200,500,000 → −201, −200,600,000 → −201, each visible in Δ and s′ (AC 4)
Proof: `ctest --preset core -R "^plan rounds the sync offset half away from zero before taking the delta$" --no-tests=error`

**C5** - The plan gives d′ = d + Δ − r and s′ = s + Δ: one camera at +75 with s 0 → Δ 75, d′ 0, s′ 75 (AC 5)
Proof: `ctest --preset core -R "^plan gives a single camera the whole delta$" --no-tests=error`

**C6** - No new delay and no new Sync Offset is negative, over a grid of readings from −1,000 to +1,000 ms in steps of 37 with d in {0, 200} and s in {−950, 0, 300}, each grid point's result also satisfying r + (d′ − d) − (s′ − s) = 0 (AC 6)
Proof: `ctest --preset core -R "^plan never gives a negative delay or sync offset over a grid of inputs$" --no-tests=error`

**C7** - The plan refuses a new value above 20,000 ms and names the first such source, cameras in order then the mic; exactly 20,000 is accepted on a camera and on the mic; 20,001 is refused on each; with two cameras over, the first is named (AC 7)
Proof: `ctest --preset core -R "^plan refuses the first source over 20000 ms and accepts exactly 20000$" --no-tests=error`

**C8** - The seven computed TDD vectors at F = 33 give exactly their Δ, d′ and s′, and the eighth, r (+20, −30), is already aligned (AC 8)
Proof: `ctest --preset core -R "^plan matches the seven computed test vectors$" --no-tests=error`
Proof: `ctest --preset core -R "^precheck finds the eighth test vector already aligned$" --no-tests=error`

**C9** - A camera is changed when d′ differs from its effective delay and the mic when s′ differs from s: the first vector marks the first camera unchanged and the second camera and the mic changed; the second vector marks the mic unchanged (AC 9)
Proof: `ctest --preset core -R "^plan marks which cameras and whether the mic change$" --no-tests=error`

**C10** - A camera with no plugin filter whose new delay is 0 is unchanged, and Apply makes no port call for it (AC 10)
Proof: `ctest --preset core -R "^plan leaves a camera without a filter unchanged when its new delay is 0$" --no-tests=error`
Proof: `ctest --preset core -R "^apply writes only the changed values with the cameras in order before the mic$" --no-tests=error`

### S2 - Apply writes the alignment all or nothing

**C11** - Apply with no mic, with no camera, or with a ticked camera not Stable makes no port call at all, returns "waiting" naming the first such camera in request order, and does not keep "waiting" as the last result (AC 11)
Proof: `ctest --preset core -R "^apply with no mic, no camera or a camera not stable makes no port call$" --no-tests=error`

**C12** - Apply with every reading within ±F makes no port call, returns and keeps "already aligned", and leaves a hand-set −200 ms offset as it was (AC 12)
Proof: `ctest --preset core -R "^apply when already aligned makes no port call and reports it$" --no-tests=error`

**C13** - Apply with a ticked camera or the mic gone makes no write call and returns "failed" (AC 13)
Proof: `ctest --preset core -R "^apply refuses with nothing written when a camera or the mic is gone$" --no-tests=error`

**C14** - Apply with Video Delay (Async) unavailable makes no write call and returns "failed", both for a plan that creates a filter and for one that only changes an existing plugin filter (AC 14)
Proof: `ctest --preset core -R "^apply refuses with nothing written when the delay filter is not available$" --no-tests=error`

**C15** - Apply reads each ticked camera's plugin filter and the mic's offset exactly once, in request order, all before the first write, and computes the plan from those values (AC 15)
Proof: `ctest --preset core -R "^apply reads every filter and the sync offset once before the first write$" --no-tests=error`

**C16** - Apply whose plan is over the limit makes no write call and returns "over the limit" with the named camera's or mic's name (AC 16)
Proof: `ctest --preset core -R "^apply over the limit writes nothing and names the source$" --no-tests=error`

**C17** - Apply asks only while the port reports on air, once, after the reads and before any write: the first vector asks with 1 camera and the mic, the second with 2 cameras and no mic; off air it never asks (AC 17)
Proof: `ctest --preset core -R "^apply asks only on air after the reads and before any write$" --no-tests=error`

**C18** - A cancelled question makes no write call, returns "cancelled", and leaves the last result and the Undo snapshot as they were (AC 18)
Proof: `ctest --preset core -R "^apply cancelled on air writes nothing and keeps the last result and undo$" --no-tests=error`

**C19** - After a yes, Apply resolves and reads again and makes no write call, returning "failed", when a camera's delay or the mic's offset changed or a source went while asking, or when the collection switched while asking with every value the same; a changed reading with OBS's values unchanged writes the plan the user saw (AC 19)
Proof: `ctest --preset core -R "^apply after a yes refuses when a value changed or a source went while asking$" --no-tests=error`
Proof: `ctest --preset core -R "^apply after a yes refuses when the collection switched while asking$" --no-tests=error`
Proof: `ctest --preset core -R "^apply after a yes writes the plan the user saw when only a reading changed$" --no-tests=error`

**C20** - Apply writes only the changed values, the cameras in checklist order then the mic: the first vector writes the second camera's filter and then the mic, and nothing for the first camera (AC 20)
Proof: `ctest --preset core -R "^apply writes only the changed values with the cameras in order before the mic$" --no-tests=error`

**C21** - A changed camera with no plugin filter gets `write_filter` with the new delay and enabled; in OBS that creates a Video Delay (Async) named "Camera Sync Delay" last in the camera's chain (AC 21)
Proof: `ctest --preset core -R "^apply creates the plugin filter enabled with the new delay on a camera without one$" --no-tests=error`
Proof: walk - CI: the Ubuntu job's integration case "settings port creates the plugin filter last in the chain and updates it in place" passes

**C22** - A changed camera with a plugin filter gets its delay set and enabled, never removed: enabled 200 → 80 enabled; enabled 200 → 0 enabled with no `remove_filter`; disabled 60 → 60 enabled (AC 22)
Proof: `ctest --preset core -R "^apply sets an existing plugin filter and enables it without removing it$" --no-tests=error`

**C23** - A changed mic gets `write_sync_offset_ns(s′ × 1,000,000)`: the first vector writes 100,000,000 (AC 23)
Proof: `ctest --preset core -R "^apply writes the mic's new sync offset in nanoseconds$" --no-tests=error`

**C24** - Each write is read back right after it, and a write is failed when the port refuses it or the value read back differs: three steps that land call write, read, write, read, write, read in step order (AC 24)
Proof: `ctest --preset core -R "^write steps reads each value back right after writing it$" --no-tests=error`
Proof: `ctest --preset core -R "^write steps fails a step the port refuses or reads back different$" --no-tests=error`

**C25** - On a failed write nothing more is written, every value changed is put back in reverse order of writing, the failed step included, every filter created is removed, and Apply returns "failed": a failure at each of the three write positions and at each of the three read-backs leaves every fake value as found and no created filter (AC 25)
Proof: `ctest --preset core -R "^write steps puts every value back in reverse order after a failure at each position$" --no-tests=error`
Proof: `ctest --preset core -R "^write steps removes a created filter whose delay reads back wrong$" --no-tests=error`
Proof: `ctest --preset core -R "^apply that fails a write at each position puts every value back and reports failed$" --no-tests=error`

**C26** - A failed write while putting back does not stop the rest being put back; Apply then returns "restore failed" and drops the Undo snapshot (AC 26)
Proof: `ctest --preset core -R "^write steps carries on putting back past a failed restore write$" --no-tests=error`
Proof: `ctest --preset core -R "^apply whose restore fails reports restore failed and drops undo$" --no-tests=error`

**C27** - An Apply that lands keeps the restore point as the Undo snapshot (every ticked camera's filter as found, the mic's ns exactly), replaces an earlier one, and returns "applied" with each ticked camera's new delay in order then the mic's new offset (AC 27)
Proof: `ctest --preset core -R "^apply that lands keeps the restore point for undo and reports the new values$" --no-tests=error`

**C28** - An Apply that lands clears every reading and keeps the pattern; a Measuring session is not started again, and a Stopped one is started at the request's rate after the clear (AC 28)
Proof: `ctest --preset core -R "^apply that lands clears the readings and starts a stopped session at the request rate$" --no-tests=error`

**C29** - An Apply that fails after a write clears the readings and starts nothing; a refusal before any write neither clears nor starts (AC 29)
Proof: `ctest --preset core -R "^apply that fails after a write clears the readings and starts nothing$" --no-tests=error`
Proof: `ctest --preset core -R "^apply refused before any write leaves the readings alone$" --no-tests=error`

**C30** - Apply, then an Apply that fails with a clean restore, keeps the first Apply's Undo snapshot and the aligned flag (AC 30)
Proof: `ctest --preset core -R "^a failed apply with a clean restore keeps the earlier undo and aligned flag$" --no-tests=error`

### S3 - Undo puts back what was there

**C31** - Undo with no snapshot makes no port call, before any Apply and on the second of two Undos (AC 31)
Proof: `ctest --preset core -R "^undo with no snapshot makes no port call$" --no-tests=error`

**C32** - The snapshot lives only in `Alignment`: a collection switch drops it, a second Apply replaces it so Undo restores the values from before the second Apply, a successful Undo drops it, and Reset keeps it (AC 32)
Proof: `ctest --preset core -R "^the undo snapshot is dropped by a collection switch and replaced by a later apply$" --no-tests=error`
Proof: `ctest --preset core -R "^reset keeps the undo snapshot and drops the aligned flag and last result$" --no-tests=error`

**C33** - Undo with a snapshot source gone, or Video Delay (Async) unavailable, makes no write call, returns "failed" and keeps the snapshot (AC 33)
Proof: `ctest --preset core -R "^undo refuses and keeps the snapshot when a source is gone or the delay filter is not available$" --no-tests=error`

**C34** - Undo reads each snapshot camera's filter and the mic's offset once, before the first write (AC 34)
Proof: `ctest --preset core -R "^undo reads each snapshot filter and the sync offset before writing$" --no-tests=error`

**C35** - On air, Undo asks with the counts of the values it would change (one camera and the mic after an Apply that changed those), and a cancel writes nothing and keeps the snapshot (AC 35)
Proof: `ctest --preset core -R "^undo on air asks with the counts it would change and a cancel keeps the snapshot$" --no-tests=error`

**C36** - Undo puts the mic back to the snapshot's ns exactly (−200,000,000 after the hand-set negative vector), removes a filter Apply created, creates again at 120 ms disabled a pre-existing filter deleted by hand, and gives no step for a created filter the user deleted or renamed (AC 36)
Proof: `ctest --preset core -R "^undo restores the sync offset to the nanosecond and removes a filter apply created$" --no-tests=error`
Proof: `ctest --preset core -R "^undo recreates a filter deleted by hand and leaves one the user removed or renamed$" --no-tests=error`

**C37** - Undo writes only the values that differ from what it reads now (two steps after an Apply that changed one camera and the mic) and writes the snapshot's value over a hand edit (AC 37)
Proof: `ctest --preset core -R "^undo writes only the values that differ and overwrites hand edits$" --no-tests=error`

**C38** - An Undo write failing at each position leaves the values from after Apply, returns "failed" and keeps the snapshot (AC 38)
Proof: `ctest --preset core -R "^undo that fails a write at each position puts back the values from after apply$" --no-tests=error`

**C39** - A failed write while Undo puts back still puts back the rest, returns "restore failed" and drops the snapshot (AC 39)
Proof: `ctest --preset core -R "^undo whose restore fails reports restore failed and drops the snapshot$" --no-tests=error`

**C40** - An Undo that lands drops the snapshot and the aligned flag, clears the readings, never starts the session (Measuring stays Measuring, Stopped stays Stopped), and returns "undone" with each snapshot camera's restored delay in Apply's order, 0 for a removed filter, then the mic's offset rounded half away from zero (AC 40)
Proof: `ctest --preset core -R "^undo that lands drops the snapshot and clears the readings without starting$" --no-tests=error`

### S4 - The settings port touches only the plugin's filter and the mic's Sync Offset

**C41** - `SettingsPort` declares exactly eight pure virtual operations, and the OBS port answers a UUID nobody has with no resolve, empty reads and failed writes (AC 41)
Proof: `test "$(grep -cE '^\s*virtual .*\) = 0;' core/alignment/settings-port.hpp)" -eq 8`
Proof: walk - CI: the Ubuntu job's integration case "settings port answers an unknown uuid with nothing" passes

**C42** - `recognised_filter_names` holds exactly "Camera Sync Delay"; in OBS the port takes the first `async_delay_filter` with a recognised name in chain order, and finds none when only another type carries the name (AC 42)
Proof: `ctest --preset core -R "^the recognised filter names hold only camera sync delay$" --no-tests=error`
Proof: walk - CI: the Ubuntu job's integration case "settings port takes the first plugin filter in chain order" passes

**C43** - A camera carrying, in order, a Video Delay (Async) named "Delay", another filter type named "Camera Sync Delay" and a Render Delay keeps all three unchanged in settings, enabled state and order after a port write and remove, with the plugin filter created after them (AC 43)
Proof: walk - CI: the Ubuntu job's integration case "settings port leaves the user's filters unchanged and in order" passes

**C44** - The port writes `delay_ms` 0 and 20,000 and reads them back, and writes and reads back the Sync Offset to the nanosecond, −200,000,000 included (AC 44)
Proof: walk - CI: the Ubuntu job's integration case "settings port writes delay_ms and the sync offset exactly" passes

**C45** - The port's `on_air` returns the injected query; on the rig, streaming and a paused recording are on air, and the virtual camera alone and the replay buffer alone are not (AC 45)
Proof: walk - CI: the Ubuntu job's integration case "settings port reports on air from the injected query" passes
Proof: walk - rig: Apply asks while streaming and while a recording is paused, and does not ask with only the virtual camera or only the replay buffer running

**C46** - Apply and Undo leave every other filter, scene, audio track, mic property and source untouched: the scene collection saved before and after Apply differs only in the plugin filters and the mic's sync offset (AC 46)
Proof: walk - rig: the scene-collection diff of C66

### S5 - The dock offers Apply and Undo and says what happened

**C47** - Apply is enabled exactly with a mic chosen, at least one camera ticked and every ticked camera Stable (AC 47)
Proof: `ctest --preset core -R "^apply is enabled only with a mic and every ticked camera stable$" --no-tests=error`

**C48** - With a ticked camera not Stable, Apply is disabled and the message line reads "Waiting for {camera}" with the first in checklist order ignoring case ("Waiting for cam b" for "Brio" Stable, "cam b" Settling, "Cam C" Looking), unless "No pattern sound from {mic} - ..." applies (AC 48)
Proof: `ctest --preset core -R "^the message line waits for the first camera in checklist order that is not stable$" --no-tests=error`

**C49** - Undo is enabled exactly while an Undo snapshot exists (AC 49)
Proof: `ctest --preset core -R "^undo is enabled exactly while an undo snapshot exists$" --no-tests=error`

**C50** - "already aligned" reads "Already aligned" (AC 50)
Proof: `ctest --preset core -R "^the message line reads already aligned$" --no-tests=error`

**C51** - "over the limit" reads "Cam B needs more delay than OBS allows", and "Shure MV7 needs more delay than OBS allows" when the mic is named (AC 51)
Proof: `ctest --preset core -R "^the message line names the camera or the mic over the limit$" --no-tests=error`

**C52** - Apply "failed" reads "Apply failed - nothing changed" (AC 52)
Proof: `ctest --preset core -R "^the message line reads apply failed nothing changed$" --no-tests=error`

**C53** - Apply "restore failed" reads "Apply failed - check the log" (AC 53)
Proof: `ctest --preset core -R "^the message line reads apply failed check the log after a failed restore$" --no-tests=error`

**C54** - "applied" reads "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms" for those values (AC 54)
Proof: `ctest --preset core -R "^the message line lists every applied value in order$" --no-tests=error`

**C55** - "undone" reads "Undone - Cam A 0 ms, Cam B 0 ms, Shure MV7 −200 ms" with U+2212, and a value is written "60", "0", "−200" (AC 55)
Proof: `ctest --preset core -R "^the message line lists every undone value with a true minus sign$" --no-tests=error`

**C56** - Undo "failed" reads "Undo failed - nothing changed" and "restore failed" reads "Undo failed - check the log" (AC 56)
Proof: `ctest --preset core -R "^the message line words a failed undo and a failed undo restore$" --no-tests=error`

**C57** - The last result is that of the last Apply or Undo that was not "waiting" or "cancelled", and Reset and a collection switch clear it; with no last result the message line shows none of them (AC 57)
Proof: `ctest --preset core -R "^the last result lasts until the next result, reset or collection switch$" --no-tests=error`
Proof: `ctest --preset core -R "^the message line shows no result before any apply or undo$" --no-tests=error`

**C58** - While the aligned flag is set, a ticked Stable camera with −F ≤ r ≤ F reads "✓ " before its status ("✓ +4 ms - video late", "✓ 0 ms - in sync"); +34 at F 33.333 and a Settling camera get no mark, and nothing is marked without the flag (AC 58)
Proof: `ctest --preset core -R "^a ticked camera within one frame after an apply gets a check mark$" --no-tests=error`

**C59** - The message line reads "Aligned" while every ticked camera has the mark, and the last result or "Waiting for ..." when one does not (AC 59)
Proof: `ctest --preset core -R "^the message line reads aligned once every ticked camera is marked$" --no-tests=error`

**C60** - The message line shows the highest of its six messages: each adjacent pair shown together gives the higher one (AC 60)
Proof: `ctest --preset core -R "^the message line ranks its messages$" --no-tests=error`

**C61** - The on-air question reads each of its five wordings for 3 cameras and the mic, 1 and the mic, 2 cameras, 1 camera, and the mic alone (AC 61)
Proof: `ctest --preset core -R "^the on-air question words each count$" --no-tests=error`

**C62** - The question's second line is "Cameras whose delay changes pause until their new delay fills." when a camera changes and empty for the mic alone (AC 62)
Proof: `ctest --preset core -R "^the on-air question adds the pause line only when a camera changes$" --no-tests=error`

**C63** - Every key the view-model uses is in `en-US.ini` and every `en-US.ini` key but the dock title is listed, the new keys included (AC 63)
Proof: `ctest --preset core -R "^locale file and the view-model's keys list the same strings$" --no-tests=error`
Proof: `ctest --preset core -R "^locale file holds the en-US text of every alignment string$" --no-tests=error`

**C64** - The dock shows "Apply" and "Undo" after Reset, enabled from the view; a click runs Apply or Undo and the message line shows the result; while recording, Apply opens a modal question with "Apply" and "Cancel", and "Cancel" writes nothing (AC 64)
Proof: walk - CI: Build for Windows, Build for macOS and Build for Ubuntu pass with the dock's Apply and Undo
Proof: walk - rig: click Apply and Undo in the dock, off air and while recording, and read the message line

### S6 - The rig confirms the alignment

**C65** - Two cameras with clearly different lags, one with a Render Delay, and the mic: after Apply every camera re-measures within one frame, three runs in a row, in OBS 30.0.0 and the latest stable (AC 65)
Proof: walk - rig: three measure-and-apply runs per OBS version, readings recorded in `validation.md`

**C66** - The scene collection saved before and after Apply differs only in each changed camera's plugin filter and the mic's sync offset (AC 66)
Proof: walk - rig: diff the collection JSON saved before and after Apply

**C67** - The scene collection saved after Undo equals the one saved before Apply (AC 67)
Proof: walk - rig: diff the collection JSON saved before Apply and after Undo

**C68** - With the plugin removed after an Apply and OBS restarted, the collection loads with every plugin filter and the mic's offset in place and working (AC 68)
Proof: walk - rig: remove the plugin, restart OBS, and read each camera within one frame with Sync Dock on Program

**C69** - Every row of the design's Alignment table but "a write fails partway" shows this plan's text in the dock, the on-air question while recording included (AC 69)
Proof: walk - rig: interactive UAT of each Alignment row, recorded in `validation.md`

### S7 - Apply and Undo are visible in the OBS log

**C70** - The plan line reads `apply.plan camera="Cam A" delay_ms=none->0 camera="Cam B" delay_ms=60(off)->60 mic="Shure MV7" sync_offset_ms=0->100`, and Apply logs it at Info before its first write (AC 70)
Proof: `ctest --preset core -R "^alignment_plan lists each camera and the mic with old and new values$" --no-tests=error`
Proof: `ctest --preset core -R "^apply logs the plan before the first write and the result after the last$" --no-tests=error`

**C71** - The ok line reads `apply.result ok written=2 skipped=1 duration_ms=4`, and its duration runs from the answer, not the request: 3 s inside the question and 4 ms of writes log `duration_ms=4` (AC 71)
Proof: `ctest --preset core -R "^alignment_ok counts the written and skipped values and the duration$" --no-tests=error`
Proof: `ctest --preset core -R "^apply counts the duration from the answer and not the request$" --no-tests=error`

**C72** - The failed line reads `apply.result failed step=camera:"Cam B" restored=yes` or `step=mic:"Shure MV7" restored=no`, logged at Warning (AC 72)
Proof: `ctest --preset core -R "^alignment_failed names the failed step and whether it was restored$" --no-tests=error`
Proof: `ctest --preset core -R "^apply that fails a write at each position puts every value back and reports failed$" --no-tests=error`

**C73** - The restore lines read `restore.failed camera="Cam B" intended_ms=0 actual_ms=60`, `intended_ms=none` for a filter that should be gone, and `restore.failed mic="Shure MV7" intended_ns=-50000000 actual_ns=0`, one per value at Warning (AC 73)
Proof: `ctest --preset core -R "^restore_failed lines give the intended and actual values$" --no-tests=error`
Proof: `ctest --preset core -R "^apply whose restore fails reports restore failed and drops undo$" --no-tests=error`

**C74** - Each refusal logs at Warning `apply.refused reason=waiting camera="Cam A"`, `reason=source_gone source="<uuid>"`, `reason=no_delay_filter`, `reason=over_limit source="Cam B" value_ms=20001` or `reason=changed`; `apply.already_aligned` and `apply.cancelled` log at Info (AC 74)
Proof: `ctest --preset core -R "^alignment_refused and alignment_event give each reason and event$" --no-tests=error`
Proof: `ctest --preset core -R "^apply logs each refusal at warning and each event at info$" --no-tests=error`

**C75** - Undo's lines read the same with `undo.` in place of `apply.`, with the snapshot's values as the new ones and `undo.result ok ...` on success (AC 75)
Proof: `ctest --preset core -R "^the alignment lines take the undo prefix$" --no-tests=error`
Proof: `ctest --preset core -R "^undo logs its plan and result with the undo prefix$" --no-tests=error`

**C76** - A source named `Cam "B"` is written `"Cam \"B\""` in every line that names it (AC 76)
Proof: `ctest --preset core -R "^the alignment lines escape quotes in source names$" --no-tests=error`

### S8 - Apply and Undo run against real OBS objects in CI

**C77** - The Ubuntu job starts libobs headless with OBS 30.0.2's own `async_delay_filter` compiled in, and the first vector applied to two test cameras and a test mic gives the second camera a plugin filter at 60 ms and the mic 100,000,000 ns and the first camera nothing; Undo puts all three back (AC 77)
Proof: walk - CI: the Ubuntu job's integration cases "libobs starts with video delay async registered" and "apply and undo the first vector against libobs" pass

**C78** - A test camera removed between Apply's reads and writes leaves every other value as it started and no plugin filter Apply created on any source (AC 78)
Proof: walk - CI: the Ubuntu job's integration case "apply restores every value when a camera is removed before its write" passes

**C79** - The user's Video Delay (Async), the other filter type with a recognised name and the Render Delay on a test camera are unchanged and in order after Apply and after Undo (AC 79)
Proof: walk - CI: the Ubuntu job's integration case "apply and undo leave the user's filters unchanged and in order" passes

**C80** - If libobs cannot start on the runner the integration cases are skipped with the reason logged, the job stays green, and C77 to C79 run on the rig instead, recorded in `validation.md` (AC 80)
Proof: walk - CI: the Ubuntu job's log shows the integration cases ran, or the skip reason and the rig run in `validation.md`

### S9 - Apply is quick

**C81** - With four cameras and the mic ticked on the rig, `apply.result` logs `duration_ms` under 100 (AC 81)
Proof: walk - rig: the `apply.result` line of a four-camera Apply in the OBS log

### S10 - What the plan's changes underneath owe

**C82** - `Session::clear_readings()` returns every camera to Looking with 0 samples, keeps the pattern so the next pair of markers forms a sample at once, and leaves Measuring and Stopped as they were (AC 28, AC 40)
Proof: `ctest --preset core -R "^session clear_readings keeps the pattern and the session state$" --no-tests=error`

**C83** - `SessionSnapshot::frame_interval_ms` is 0 before any Start, 33.333 after Start at 30/1 and 16.683 at 60000/1001 (each within 0.001), and keeps its value through Stop (AC 58)
Proof: `ctest --preset core -R "^session snapshot reports the frame interval read at the last start$" --no-tests=error`

**C84** - Reading's sync offset handler still clears every reading and keeps the pattern after it calls `clear_readings()` (AC 28)
Proof: `ctest --preset core -R "^session clears every reading when the mic's sync offset changes and keeps the pattern$" --no-tests=error`

**C85** - Every one of the 164 Reading and Foundation tests still passes, `build_view`'s callers unchanged, and every file the feature adds starts with the SPDX line (AC 63)
Proof: `ctest --preset core --no-tests=error`
Proof: `ctest --preset core -R "^check_notices_repository$" --no-tests=error`

**C86** - The core-only CI job's line coverage of `core/` without `core/detector/` stays at 90% or more (AC 8)
Proof: walk - CI: the core-only job's Check Core Line Coverage step passes and prints the line total

**C87** - CI's Check Formatting passes on every file the feature adds or changes under `src/`, `core/`, `tests/` and the CMake files (AC 63)
Proof: walk - CI: the Check Formatting job passes

**C88** - `FilterState::effective_ms()` is the delay only when present and enabled, and `==` compares present, delay and enabled (AC 3)
Proof: `ctest --preset core -R "^filter state takes its effective delay and compares every field$" --no-tests=error`

**C89** - Every `Outcome` that is not "waiting" or "cancelled" becomes the last result, and the view-model reads only `AlignmentStatus`, so the dock needs no `Alignment` object (AC 57)
Proof: `ctest --preset core -R "^the last result lasts until the next result, reset or collection switch$" --no-tests=error`

## Coverage

| Set (size) | Member -> proof | Unproven |
| --- | --- | --- |
| TDD test vectors (8) | C8, table-driven over the 7 computed · already aligned C8 | - |
| precheck outcomes (3) | waiting C1 · already aligned C2 · ready C2 | - |
| effective delay by filter (3) | absent C3 · enabled C3 · disabled C3 | - |
| Sync Offset rounding, AC 4 (3 examples) | −200,400,000 C4 · −200,500,000 C4 · −200,600,000 C4 | - |
| 20,000 ms limit, AC 7 (5 examples) | camera 20,000 C7 · camera 20,001 C7 · mic 20,000 C7 · mic 20,001 C7 · two over C7 | - |
| outcome kinds (10) | `Waiting` C11 · `AlreadyAligned` C12 · `OverLimit` C16 · `Cancelled` C18 · `ApplyFailed` C13 · `ApplyRestoreFailed` C26 · `Applied` C27 · `UndoFailed` C33 · `UndoRestoreFailed` C39 · `Undone` C40 | - |
| Apply refusals before any write (5) | no mic C11 · no camera C11 · not Stable C11 · source gone C13 · filter unavailable C14 | - |
| Apply refusals after the question (4) | value changed C19 · source gone C19 · collection switched C19 · cancelled C18 | - |
| write and read-back positions (6) | write 1 C25 · write 2 C25 · write 3 C25 · read-back 1 C25 · read-back 2 C25 · read-back 3 C25 | - |
| Undo write positions (3) | write 1 C38 · write 2 C38 · write 3 C38 | - |
| a plugin filter's write cases, AC 22 (3 examples) | 200 → 80 C22 · 200 → 0 C22 · disabled 60 → 60 C22 | - |
| Undo's camera cases, AC 36 (4) | created and present → removed C36 · created and gone → no step C36 · pre-existing and deleted → created again C36 · hand-edited → overwritten C37 | - |
| `SettingsPort` operations (8) | `resolves` C13 · `delay_filter_available` C14 · `read_filter` C15 · `write_filter` C21 · `remove_filter` C36 · `read_sync_offset_ns` C15 · `write_sync_offset_ns` C23 · `on_air` C17 | - |
| the OBS port's operations, in the Ubuntu job (8) | `resolves` C41 · `delay_filter_available` C77 · `read_filter` C42 · `write_filter` C21, C44 · `remove_filter` C43 · `read_sync_offset_ns` C44 · `write_sync_offset_ns` C44 · `on_air` C45 | - |
| Undo snapshot lifetime (6) | collection switch C32 · later Apply C32 · Undo lands C40 · failed restore C26, C39 · Reset keeps it C32 · OBS exit C32, held only in `Alignment` | - |
| aligned flag lifetime (5) | set by Apply C27 · Undo C40 · Reset C32 · collection switch C32 · failed restore C26 | - |
| session state after a success (4) | Apply from Measuring C28 · Apply from Stopped C28 · Undo in Measuring C40 · Undo in Stopped C40 | - |
| message line precedence (6) | pick C60 · could not listen C60 · no pattern sound C60 · Aligned C60 · last result C60 · waiting C60 | - |
| result messages (8) | already aligned C50 · over the limit C51 · Apply failed C52 · Apply restore failed C53 · applied C54 · undone C55 · Undo failed C56 · Undo restore failed C56 | - |
| question wordings (5) | C61, table-driven over all 5 | - |
| log lines (10) | plan C70 · ok C71 · failed C72 · restore failed camera C73 · restore failed mic C73 · refused ×5 reasons C74 · already aligned C74 · cancelled C74 · undo prefix C75 · quoting C76 | - |
| startup config: `Alignment` and its port (2 assemblies) | the plugin's `Controller` C64 · the integration test harness C77 | - |
| Landing doors (2) | door 1 C21, C42, C43, C68 · door 2 C77, C80 | - |
| rig criteria (6) | C65 · C66 · C67 · C68 · C69 · C81 | - |

- Claims about a set of examples name a table-driven proof whose cases are those examples: C2, C6, C7, C8, C22, C25, C38, C61
- No other check claims more than the cases its proof exercises

## Swept

- validation: C7, C44
- failure modes: C25, C26, C38, C39, C78
- idempotency: C12, C31, C37, C8 (the third vector adds only the remainder)
- authorization: n/a - no network, no account and no caller but the user at the dock; the write boundary inside OBS is C41 to C46
- concurrency: C19 - values, sources and the collection change while the question's nested event loop runs; every write runs in one call on the UI thread
- data lifecycle: C32, C57, C40
- dependency failure: C14, C33, C24 (a write OBS refuses or rewrites is caught by the read-back), C80
- state transitions: C11, C28, C31, C40, C49
- observability: C70 to C76

## Handoff

- **Settled mid-build:** none. The plan was approved with "approved, keep going"; the two questions
  asked with it, pushing to `origin/main` for CI and raising `Mutations` to 5, were not answered, so
  the build stopped at local commits and `Mutations` stays 3. 2026-10-28, user: "yes, push" - pushed
  `6c1142f..de0f614` to `origin/main` for CI. CI then failed Check Formatting on seven files and
  Build for Windows on Reading's "estimator snapshots show each camera as it was at one instant while
  four threads push markers", whose files this feature does not touch; it passed 300 runs in a row on
  WSL. User, asked to push the formatting fix: "ok, keep going".
- **Dispatched:** none. The Verifier waits until the feature is held whole: the `walk - CI` proofs
  need a push and the `walk - rig` proofs need the rig.
- **Boundary:** every check with a `ctest` or command proof is green at `09a7b8f`: C1-C20, C21's
  first proof, C22-C40, C41's first proof, C42's first proof, C47-C63, C70-C76, C82-C85, C88, C89.
  Open: the `walk - CI` proofs of C21, C41, C42, C43, C44, C45, C64, C77-C80, C86 and C87, and the
  `walk - rig` proofs of C45, C46, C64, C65-C69 and C81.
- **Decided in build:** "Waiting for {camera}" names only a ticked camera the session has a row for,
  so a camera the session dropped is named nowhere (READ-44) and Reading's message-line tests stay
  unchanged. Undo with no snapshot returns "waiting" and is not remembered. A failed Undo clears the
  readings as a failed Apply does. "Undone" gives a restored disabled filter's delay. Putting a value
  back skips one that already reads as found or whose source is gone, so a camera removed mid-Apply
  is no restore failure. `request_for` builds Apply's request in the core, so the controller holds no
  ordering rule.
- **Abandoned:** none.
- **Not run here:** `src/obs/obs-settings-port.cpp` and every file of `tests/integration/` pass
  `g++ -std=c++17 -fsyntax-only -Wall -Wextra -Werror` against OBS 30.0.2's headers (the archive
  `buildspec.json` pins, hash checked, unpacked in the session's scratchpad), and OBS's
  `async-delay-filter.c` passes `gcc -fsyntax-only`; nothing links or runs them before CI. The
  controller and the dock are not compiled here at all: there are no Qt headers. clang-format 19.1.1
  now runs from Ubuntu's packages unpacked in the scratchpad (no sudo), and every C and C++ file in
  `src/`, `core/` and `tests/` outside `core/detector/` passes it; gersemi is still matched by hand.
  CI's Check Formatting is C87.
