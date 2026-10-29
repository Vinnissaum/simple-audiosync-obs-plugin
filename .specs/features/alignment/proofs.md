# alignment proofs

**HEAD**: bcd271b
**Tree**: clean
**Result**: 96 pass, 22 fail; mutations 3 killed, 0 not

| Check | Proof | Ran | Passed | Result |
| --- | --- | --- | --- | --- |
| C1 | `ctest --preset core -R "^precheck waits for the first camera in order that is not stable$" --no-tests=error` | count unknown | - | pass |
| C2 | `ctest --preset core -R "^precheck finds every reading within one frame already aligned$" --no-tests=error` | count unknown | - | pass |
| C3 | `ctest --preset core -R "^plan takes the effective delay of an absent, enabled or disabled filter$" --no-tests=` | count unknown | - | pass |
| C4 | `ctest --preset core -R "^plan rounds the sync offset half away from zero before taking the delta$" --no-tests=` | count unknown | - | pass |
| C5 | `ctest --preset core -R "^plan gives a single camera the whole delta$" --no-tests=error` | count unknown | - | pass |
| C6 | `ctest --preset core -R "^plan never gives a negative delay or sync offset over a grid of inputs$" --no-tests=e` | count unknown | - | pass |
| C7 | `ctest --preset core -R "^plan refuses the first source over 20000 ms and accepts exactly 20000$" --no-tests=er` | count unknown | - | pass |
| C8 | `ctest --preset core -R "^plan matches the seven computed test vectors$" --no-tests=error` | count unknown | - | pass |
| C8 | `ctest --preset core -R "^precheck finds the eighth test vector already aligned$" --no-tests=error` | count unknown | - | pass |
| C9 | `ctest --preset core -R "^plan marks which cameras and whether the mic change$" --no-tests=error` | count unknown | - | pass |
| C10 | `ctest --preset core -R "^plan leaves a camera without a filter unchanged when its new delay is 0$" --no-tests=` | count unknown | - | pass |
| C10 | `ctest --preset core -R "^apply writes only the changed values with the cameras in order before the mic$" --no-` | count unknown | - | pass |
| C11 | `ctest --preset core -R "^apply with no mic, no camera or a camera not stable makes no port call$" --no-tests=e` | count unknown | - | pass |
| C12 | `ctest --preset core -R "^apply when already aligned makes no port call and reports it$" --no-tests=error` | count unknown | - | pass |
| C13 | `ctest --preset core -R "^apply refuses with nothing written when a camera or the mic is gone$" --no-tests=erro` | count unknown | - | pass |
| C14 | `ctest --preset core -R "^apply refuses with nothing written when the delay filter is not available$" --no-test` | count unknown | - | pass |
| C15 | `ctest --preset core -R "^apply reads every filter and the sync offset once before the first write$" --no-tests` | count unknown | - | pass |
| C16 | `ctest --preset core -R "^apply over the limit writes nothing and names the source$" --no-tests=error` | count unknown | - | pass |
| C17 | `ctest --preset core -R "^apply asks only on air after the reads and before any write$" --no-tests=error` | count unknown | - | pass |
| C18 | `ctest --preset core -R "^apply cancelled on air writes nothing and keeps the last result and undo$" --no-tests` | count unknown | - | pass |
| C19 | `ctest --preset core -R "^apply after a yes refuses when a value changed or a source went while asking$" --no-t` | count unknown | - | pass |
| C19 | `ctest --preset core -R "^apply after a yes refuses when the collection switched while asking$" --no-tests=erro` | count unknown | - | pass |
| C19 | `ctest --preset core -R "^apply after a yes writes the plan the user saw when only a reading changed$" --no-tes` | count unknown | - | pass |
| C20 | `ctest --preset core -R "^apply writes only the changed values with the cameras in order before the mic$" --no-` | count unknown | - | pass |
| C21 | `ctest --preset core -R "^apply creates the plugin filter enabled with the new delay on a camera without one$" ` | count unknown | - | pass |
| C21 | `walk - CI: the Ubuntu job's integration case "settings port creates the plugin filter last in the chain and up` | - | - | **no Walked line** |
| C22 | `ctest --preset core -R "^apply sets an existing plugin filter and enables it without removing it$" --no-tests=` | count unknown | - | pass |
| C23 | `ctest --preset core -R "^apply writes the mic's new sync offset in nanoseconds$" --no-tests=error` | count unknown | - | pass |
| C24 | `ctest --preset core -R "^write steps reads each value back right after writing it$" --no-tests=error` | count unknown | - | pass |
| C24 | `ctest --preset core -R "^write steps fails a step the port refuses or reads back different$" --no-tests=error` | count unknown | - | pass |
| C25 | `ctest --preset core -R "^write steps puts every value back in reverse order after a failure at each position$"` | count unknown | - | pass |
| C25 | `ctest --preset core -R "^write steps removes a created filter whose delay reads back wrong$" --no-tests=error` | count unknown | - | pass |
| C25 | `ctest --preset core -R "^apply that fails a write at each position puts every value back and reports failed$" ` | count unknown | - | pass |
| C26 | `ctest --preset core -R "^write steps carries on putting back past a failed restore write$" --no-tests=error` | count unknown | - | pass |
| C26 | `ctest --preset core -R "^apply whose restore fails reports restore failed and drops undo$" --no-tests=error` | count unknown | - | pass |
| C27 | `ctest --preset core -R "^apply that lands keeps the restore point for undo and reports the new values$" --no-t` | count unknown | - | pass |
| C28 | `ctest --preset core -R "^apply that lands clears the readings and starts a stopped session at the request rate` | count unknown | - | pass |
| C29 | `ctest --preset core -R "^apply that fails after a write clears the readings and starts nothing$" --no-tests=er` | count unknown | - | pass |
| C29 | `ctest --preset core -R "^apply refused before any write leaves the readings alone$" --no-tests=error` | count unknown | - | pass |
| C30 | `ctest --preset core -R "^a failed apply with a clean restore keeps the earlier undo and aligned flag$" --no-te` | count unknown | - | pass |
| C31 | `ctest --preset core -R "^undo with no snapshot makes no port call$" --no-tests=error` | count unknown | - | pass |
| C32 | `ctest --preset core -R "^the undo snapshot is dropped by a collection switch and replaced by a later apply$" -` | count unknown | - | pass |
| C32 | `ctest --preset core -R "^reset keeps the undo snapshot and drops the aligned flag and last result$" --no-tests` | count unknown | - | pass |
| C33 | `ctest --preset core -R "^undo refuses and keeps the snapshot when a source is gone or the delay filter is not ` | count unknown | - | pass |
| C34 | `ctest --preset core -R "^undo reads each snapshot filter and the sync offset before writing$" --no-tests=error` | count unknown | - | pass |
| C35 | `ctest --preset core -R "^undo on air asks with the counts it would change and a cancel keeps the snapshot$" --` | count unknown | - | pass |
| C36 | `ctest --preset core -R "^undo restores the sync offset to the nanosecond and removes a filter apply created$" ` | count unknown | - | pass |
| C36 | `ctest --preset core -R "^undo recreates a filter deleted by hand and leaves one the user removed or renamed$" ` | count unknown | - | pass |
| C37 | `ctest --preset core -R "^undo writes only the values that differ and overwrites hand edits$" --no-tests=error` | count unknown | - | pass |
| C38 | `ctest --preset core -R "^undo that fails a write at each position puts back the values from after apply$" --no` | count unknown | - | pass |
| C39 | `ctest --preset core -R "^undo whose restore fails reports restore failed and drops the snapshot$" --no-tests=e` | count unknown | - | pass |
| C40 | `ctest --preset core -R "^undo that lands drops the snapshot and clears the readings without starting$" --no-te` | count unknown | - | pass |
| C90 | `ctest --preset core -R "^undo counts the duration from the answer and not the request$" --no-tests=error` | count unknown | - | pass |
| C91 | `ctest --preset core -R "^undo on air with nothing to change asks nothing and lands$" --no-tests=error` | count unknown | - | pass |
| C41 | `test "$(grep -cE '^\s*virtual .*\) = 0;' core/alignment/settings-port.hpp)" -eq 8` | count unknown | - | pass |
| C41 | `walk - CI: the Ubuntu job's integration case "settings port answers an unknown uuid with nothing" passes` | - | - | **no Walked line** |
| C42 | `ctest --preset core -R "^the recognised filter names hold only camera sync delay$" --no-tests=error` | count unknown | - | pass |
| C42 | `walk - CI: the Ubuntu job's integration case "settings port takes the first plugin filter in chain order" pass` | - | - | **no Walked line** |
| C43 | `walk - CI: the Ubuntu job's integration case "settings port leaves the user's filters unchanged and in order" ` | - | - | **no Walked line** |
| C44 | `walk - CI: the Ubuntu job's integration case "settings port writes delay_ms and the sync offset exactly" passe` | - | - | **no Walked line** |
| C45 | `walk - CI: the Ubuntu job's integration case "settings port reports on air from the injected query" passes` | - | - | **no Walked line** |
| C45 | `walk - rig: Apply asks while streaming and while a recording is paused, and does not ask with only the virtual` | - | - | **no Walked line** |
| C46 | `walk - rig: the scene-collection diff of C66` | - | - | **no Walked line** |
| C47 | `ctest --preset core -R "^apply is enabled only with a mic and every ticked camera stable$" --no-tests=error` | count unknown | - | pass |
| C48 | `ctest --preset core -R "^the message line waits for the first camera in checklist order that is not stable$" -` | count unknown | - | pass |
| C49 | `ctest --preset core -R "^undo is enabled exactly while an undo snapshot exists$" --no-tests=error` | count unknown | - | pass |
| C50 | `ctest --preset core -R "^the message line reads already aligned$" --no-tests=error` | count unknown | - | pass |
| C51 | `ctest --preset core -R "^the message line names the camera or the mic over the limit$" --no-tests=error` | count unknown | - | pass |
| C52 | `ctest --preset core -R "^the message line reads apply failed nothing changed$" --no-tests=error` | count unknown | - | pass |
| C53 | `ctest --preset core -R "^the message line reads apply failed check the log after a failed restore$" --no-tests` | count unknown | - | pass |
| C54 | `ctest --preset core -R "^the message line lists every applied value in order$" --no-tests=error` | count unknown | - | pass |
| C55 | `ctest --preset core -R "^the message line lists every undone value with a true minus sign$" --no-tests=error` | count unknown | - | pass |
| C56 | `ctest --preset core -R "^the message line words a failed undo and a failed undo restore$" --no-tests=error` | count unknown | - | pass |
| C57 | `ctest --preset core -R "^the last result lasts until the next result, reset or collection switch$" --no-tests=` | count unknown | - | pass |
| C57 | `ctest --preset core -R "^the message line shows no result before any apply or undo$" --no-tests=error` | count unknown | - | pass |
| C58 | `ctest --preset core -R "^a ticked camera within one frame after an apply gets a check mark$" --no-tests=error` | count unknown | - | pass |
| C59 | `ctest --preset core -R "^the message line reads aligned once every ticked camera is marked$" --no-tests=error` | count unknown | - | pass |
| C60 | `ctest --preset core -R "^the message line ranks its messages$" --no-tests=error` | count unknown | - | pass |
| C61 | `ctest --preset core -R "^the on-air question words each count$" --no-tests=error` | count unknown | - | pass |
| C62 | `ctest --preset core -R "^the on-air question adds the pause line only when a camera changes$" --no-tests=error` | count unknown | - | pass |
| C63 | `ctest --preset core -R "^locale file and the view-model's keys list the same strings$" --no-tests=error` | count unknown | - | pass |
| C63 | `ctest --preset core -R "^locale file holds the en-US text of every alignment string$" --no-tests=error` | count unknown | - | pass |
| C64 | `walk - CI: Build for Windows, Build for macOS and Build for Ubuntu pass with the dock's Apply and Undo` | - | - | **no Walked line** |
| C64 | `walk - rig: click Apply and Undo in the dock, off air and while recording, and read the message line` | - | - | **no Walked line** |
| C65 | `walk - rig: three measure-and-apply runs per OBS version, readings recorded in validation.md` | - | - | **no Walked line** |
| C66 | `walk - rig: diff the collection JSON saved before and after Apply` | - | - | **no Walked line** |
| C67 | `walk - rig: diff the collection JSON saved before Apply and after Undo` | - | - | **no Walked line** |
| C68 | `walk - rig: remove the plugin, restart OBS, and read each camera within one frame with Sync Dock on Program` | - | - | **no Walked line** |
| C69 | `walk - rig: interactive UAT of each Alignment row, recorded in validation.md` | - | - | **no Walked line** |
| C70 | `ctest --preset core -R "^alignment_plan lists each camera and the mic with old and new values$" --no-tests=err` | count unknown | - | pass |
| C70 | `ctest --preset core -R "^apply logs the plan before the first write and the result after the last$" --no-tests` | count unknown | - | pass |
| C71 | `ctest --preset core -R "^alignment_ok counts the written and skipped values and the duration$" --no-tests=erro` | count unknown | - | pass |
| C71 | `ctest --preset core -R "^apply counts the duration from the answer and not the request$" --no-tests=error` | count unknown | - | pass |
| C72 | `ctest --preset core -R "^alignment_failed names the failed step and whether it was restored$" --no-tests=error` | count unknown | - | pass |
| C72 | `ctest --preset core -R "^apply that fails a write at each position puts every value back and reports failed$" ` | count unknown | - | pass |
| C73 | `ctest --preset core -R "^restore_failed lines give the intended and actual values$" --no-tests=error` | count unknown | - | pass |
| C73 | `ctest --preset core -R "^apply whose restore fails reports restore failed and drops undo$" --no-tests=error` | count unknown | - | pass |
| C74 | `ctest --preset core -R "^alignment_refused and alignment_event give each reason and event$" --no-tests=error` | count unknown | - | pass |
| C74 | `ctest --preset core -R "^apply logs each refusal at warning and each event at info$" --no-tests=error` | count unknown | - | pass |
| C75 | `ctest --preset core -R "^the alignment lines take the undo prefix$" --no-tests=error` | count unknown | - | pass |
| C75 | `ctest --preset core -R "^undo logs its plan and result with the undo prefix$" --no-tests=error` | count unknown | - | pass |
| C76 | `ctest --preset core -R "^the alignment lines escape quotes in source names$" --no-tests=error` | count unknown | - | pass |
| C77 | `walk - CI: the Ubuntu job's integration cases "libobs starts with video delay async registered" and "apply and` | - | - | **no Walked line** |
| C78 | `walk - CI: the Ubuntu job's integration case "apply restores every value when a camera is removed before its w` | - | - | **no Walked line** |
| C79 | `walk - CI: the Ubuntu job's integration case "apply and undo leave the user's filters unchanged and in order" ` | - | - | **no Walked line** |
| C80 | `walk - CI: the Ubuntu job's log shows the integration cases ran, or the skip reason and the rig run in validat` | - | - | **no Walked line** |
| C81 | `walk - rig: the apply.result line of a four-camera Apply in the OBS log` | - | - | **no Walked line** |
| C82 | `ctest --preset core -R "^session clear_readings keeps the pattern and the session state$" --no-tests=error` | count unknown | - | pass |
| C83 | `ctest --preset core -R "^session snapshot reports the frame interval read at the last start$" --no-tests=error` | count unknown | - | pass |
| C84 | `ctest --preset core -R "^session clears every reading when the mic's sync offset changes and keeps the pattern` | count unknown | - | pass |
| C85 | `ctest --preset core --no-tests=error` | count unknown | - | pass |
| C85 | `ctest --preset core -R "^check_notices_repository$" --no-tests=error` | count unknown | - | pass |
| C86 | `walk - CI: the core-only job's Check Core Line Coverage step passes and prints the line total` | - | - | **no Walked line** |
| C87 | `walk - CI: the Check Formatting job passes` | - | - | **no Walked line** |
| C88 | `ctest --preset core -R "^filter state takes its effective delay and compares every field$" --no-tests=error` | count unknown | - | pass |
| C89 | `ctest --preset core -R "^the last result lasts until the next result, reset or collection switch$" --no-tests=` | count unknown | - | pass |
| Build | `cmake --preset core` | - | - | pass |
| Build | `cmake --build --preset core` | - | - | pass |

## Mutations

| Mutation | Location | Killed |
| --- | --- | --- |
| C23: Apply writes the mic as the ns found plus delta ms, not s' x 1,000,000 | `core/alignment/alignment.cpp:133` | yes |
| C36/C37: Undo's diff compares a filter's presence and delay but not its enabled state | `core/alignment/alignment.cpp:202` | yes |
| C24: a camera's read-back compares the effective delay, not the state written | `core/alignment/write-steps.cpp:25` | yes |
