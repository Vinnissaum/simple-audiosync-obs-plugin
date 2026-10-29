# Rig checklist

One rig session for everything that needs OBS, cameras, a mic and the phone: Foundation's load check
(T16), spike 1 (ADR-005), Reading's rig tasks (T34 to T37, with spike 2, ADR-007) and Alignment's
rig checks (C45, C46, C64, C65 to C69, C81). Do the parts in order: a failed spike stops the session,
because what comes after builds on it.

Fill this file in on the rig machine as you go: tick `[x]`, write the value after each `→`, and add
a note to anything that surprised you. Then commit it or paste it back; the results are transcribed
into each feature's validation record and Alignment's walk line from here.

## 0. Set up

### The rig

- Platform and OS version → 
- OBS 30.0.0 installed (a portable copy is fine) → 
- Latest stable OBS, version → 
- Canvas resolution and frame rate (Settings → Video), the same in both → 
- F, one frame, in ms (1000 ÷ fps; 33.3 at 30 fps, 16.7 at 60) → 
- Camera A: device and connection → 
- Camera B, with clearly more or less lag than A (a phone webcam app works) → 
- Cameras C and D, only for parts 6 and 7 (four cameras) → 
- Mic: device → 
- Phone model, and the pattern file played (from Sync Dock's README, at the canvas rate or twice
  it) → 
- Audio Video Sync Dock installed in both OBS versions, version → 

### The plugin

- [ ] The CI packages from the latest green run on `main`, run 40796979774 at `39d7015` (GitHub →
  Actions → that run → Artifacts): the Windows .zip, the macOS .pkg or the Ubuntu .deb.
- [ ] Installed in both OBS versions as `CLAUDE.md` → "Install and load into OBS" says.
- [ ] Each OBS starts with `--verbose` (a shortcut flag on Windows, the command line elsewhere), so
  the marker lines reach the log at Debug level.

### Leak baseline (needed by part 1)

- [ ] With the plugin removed, start OBS 30.0.0, close it, and note the last
  "Number of memory leaks" line in its log → 
- [ ] The same for the latest stable → 

## 1. The plugin loads (Foundation T16; READ-76, READ-77)

In OBS 30.0.0, then in the latest stable, with the plugin installed:

| Check | OBS 30.0.0 | Latest stable |
| --- | --- | --- |
| Docks lists "Camera Sync" and the dock opens | [ ] | [ ] |
| The dock shows the camera checklist, the mic picker, "Pattern files", Start, Reset, Apply, Undo and Details | [ ] | [ ] |
| The log has exactly one `[camera-sync] load ...` line with `obs_built=30.0.2` and this OBS's version as `obs_running` | → | → |
| After Start, Stop, Reset and closing OBS, the last "Number of memory leaks" equals the baseline above | → | → |

## 2. Spike 1: the detector in an ordinary room (ADR-005)

The phone plays the pattern within arm's reach of the mic, in the room the rig is in (no treatment).
Both cameras ticked, the mic chosen, Start.

| Run | Phone to mic distance | Both cameras Stable? | Camera A reading | Camera B reading |
| --- | --- | --- | --- | --- |
| 1 | near: → | [ ] | → | → |
| 2 | near | [ ] | → | → |
| 3 | near | [ ] | → | → |
| 4 | far, still arm's reach: → | [ ] | → | → |
| 5 | far | [ ] | → | → |
| 6 | far | [ ] | → | → |

- [ ] Every run reached Stable and held it (ADR-005 is then accepted). If not: stop here, note what
  the dock showed, and bring it back - a new ADR replaces the pattern before the rest is worth
  running.

## 3. Every Reading row in the dock (T34; READ-78, READ-81, READ-82)

Do each row in OBS 30.0.0 and again in the latest stable. Take a screenshot or a log excerpt per row.

| Do this | The dock shows | 30.0.0 | Latest |
| --- | --- | --- | --- |
| Open the dock in a new collection the plugin never saved, with the cameras on Program and one audio input | cameras in name order, those on Program ticked; the mic picked if it is the only audio input | [ ] | [ ] |
| Untick every camera, or clear the mic | Start disabled, "Pick at least one camera and the mic" | [ ] | [ ] |
| Tick both, choose the mic, Start, phone hidden from every camera | each row "Looking for the pattern" | [ ] | [ ] |
| Show the phone to the cameras, mute the phone | "No pattern sound from {mic} - bring the phone closer or unmute it" after about 5 s | [ ] | [ ] |
| Unmute; move the phone while a row settles | "{value} ms - settling" with a live value | [ ] | [ ] |
| Hold still | each row "+{n} ms - video late", "−{n} ms - video early" or "0 ms - in sync" | [ ] | [ ] |
| Both cameras Stable in the same pass (READ-78) | both rows Stable without a second Start | [ ] | [ ] |
| Reset, then hide the phone from camera B only (READ-81) | B "Looking for the pattern"; A keeps measuring | [ ] | [ ] |
| Hide the phone from a Stable camera | its value stays, unchanged | [ ] | [ ] |
| Remove camera B from OBS | B leaves the list | [ ] | [ ] |
| Rename camera A | A is unticked and stays in the list under its new name | [ ] | [ ] |
| Unplug or deactivate a ticked camera | after about 2 s it is unticked | [ ] | [ ] |
| Stop | the readings stay | [ ] | [ ] |
| Reset | every row "Looking for the pattern" | [ ] | [ ] |
| Switch the scene collection | the readings are gone; the new collection's selection loads | [ ] | [ ] |

While Idle with cameras and the mic chosen, the message line reads "Waiting for {camera}" (Alignment
criterion 48 applies before Start too). Does that read well before anyone pressed Start? → 

## 4. Readings against Sync Dock (T35, spike 2 ADR-007; READ-61 to READ-65, READ-69, READ-79, READ-80)

Camera A carries a Render Delay filter (any value, for example 100 ms); camera B carries a Video
Delay (Async) filter of your own named "Delay" (for example 60 ms). Neither is "Camera Sync Delay".

For each camera in turn: put it alone on Program with Sync Dock running, note Sync Dock's latency,
then read this plugin's Stable reading. The reading must equal minus Sync Dock's value within F.

| Run | A: Sync Dock | A: plugin | within F? | B: Sync Dock | B: plugin | within F? |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | → | → | [ ] | → | → | [ ] |
| 2 | → | → | [ ] | → | → | [ ] |
| 3 | → | → | [ ] | → | → | [ ] |

- [ ] Three runs in a row within F for both cameras (ADR-007 is then accepted). If not: stop and
  bring it back.
- [ ] Set the mic's Sync Offset by hand to +100 ms (Advanced Audio Properties): both readings move
  by 100 ms → before: ____ after: ____ (READ-62). Put it back to 0.
- [ ] A ticked camera in no scene reaches a reading (READ-63).
- [ ] Export the scene collection while Idle, Start, export it again while measuring, and diff the
  two: only the plugin's module data differs (READ-64, READ-65) → 
- [ ] With the phone in view and audible, each camera Stable within 5 s → A: ____ s B: ____ s
  (READ-80).
- [ ] Slow one camera on purpose (its highest resolution and frame rate, or a heavy filter): the
  other camera's row keeps updating (READ-69).
- [ ] Keep the OBS log (with `--verbose`) of one full run here, and write down the steps of that
  run; it becomes T37's replay fixture → log file: 

## 5. Alignment (C45, C46, C64, C65 to C69)

Start from part 4's setup: camera A with its Render Delay, camera B with no filter of yours (remove
"Delay"), the mic's Sync Offset at 0. Both cameras ticked, the mic chosen.

### Three measure-and-apply runs per OBS version (C65)

Each run: measure until both rows are Stable, Apply, wait for both rows to be Stable again, read
them, then Undo to come back to the start.

| OBS | Run | Readings before (A, B) | Message after Apply | Readings after (A, B) | both within F, "Aligned" and "✓ " on each row? |
| --- | --- | --- | --- | --- | --- |
| 30.0.0 | 1 | → | → | → | [ ] |
| 30.0.0 | 2 | → | → | → | [ ] |
| 30.0.0 | 3 | → | → | → | [ ] |
| latest | 1 | → | → | → | [ ] |
| latest | 2 | → | → | → | [ ] |
| latest | 3 | → | → | → | [ ] |

### The scene collection before, after Apply and after Undo (C46, C66, C67)

In one run: Scene Collection → Export before Apply, after Apply, and after Undo, then diff them.

- [ ] Before against after Apply: only each changed camera's filter "Camera Sync Delay" (added, or
  its `delay_ms` and enabled state) and the mic's `sync` value differ. Anything else that differs → 
- [ ] Before against after Undo: no difference → 

### Every Alignment row in the dock (C64, C69, C45)

Do each row in OBS 30.0.0 and in the latest stable. Click Apply and Undo in the dock itself.

| Do this | The dock shows | 30.0.0 | Latest |
| --- | --- | --- | --- |
| A ticked camera not Stable (hide the phone from it after a Reset) | Apply disabled, "Waiting for {that camera}" | [ ] | [ ] |
| Apply with the readings apart | "Applied - {camera} {n} ms, {camera} {n} ms, {mic} {n} ms"; every row back to "Looking for the pattern" | [ ] | [ ] |
| Measure again until both are Stable | each row "✓ " before its status, the message "Aligned" | [ ] | [ ] |
| Reset (it drops the check marks), measure until both are Stable, Apply | "Already aligned"; nothing written ("Aligned" would outrank it without the Reset) | [ ] | [ ] |
| Add a Render Delay of 100 ms to camera B, Reset, measure, Apply, measure again | Apply adds only the remainder; "Aligned" again. Remove that Render Delay afterwards | [ ] | [ ] |
| Undo | "Undone - {camera} {n} ms, {camera} {n} ms, {mic} {n} ms"; the values back as before Apply | [ ] | [ ] |
| Undo again | Undo is disabled | [ ] | [ ] |
| Put a filter of your own, Video Delay (Async) at 20,000 ms, on camera A; Reset; measure; Apply | "{camera B} needs more delay than OBS allows"; nothing written. Remove that filter afterwards | [ ] | [ ] |
| Apply once, close and reopen OBS | Undo disabled | [ ] | [ ] |
| Start recording; Apply with the readings apart | a question "Shift {n} cameras and the mic while on air?" (or its one-camera or mic-only form) with "Cameras whose delay changes pause until their new delay fills.", buttons Apply and Cancel, Cancel the default | [ ] | [ ] |
| In that question, Cancel | nothing changes; the message before stays | [ ] | [ ] |
| In that question, Apply | the alignment is written, as off air | [ ] | [ ] |
| Pause the recording; Apply | it asks too (C45) | [ ] | [ ] |
| Stream (a local RTMP server such as MediaMTX, or a private test stream); Apply | it asks (C45) | [ ] | [ ] |
| Stop both; start only the virtual camera; Apply | no question (C45) | [ ] | [ ] |
| Only the replay buffer running (enable it in Output settings); Apply | no question (C45) | [ ] | [ ] |
| Undo while recording | the same question for what Undo changes | [ ] | [ ] |

### The plugin removed after an Apply (C68)

- [ ] Apply in the latest stable, close OBS, remove the plugin's files, start OBS again.
- [ ] Each camera still has "Camera Sync Delay" with its value, and the mic its Sync Offset → 
- [ ] With Sync Dock, each camera on Program in turn reads within F of 0 → A: ____ B: ____
- [ ] Reinstall the plugin.

## 6. Performance with four cameras (T36; READ-93 to READ-96; C81)

Four cameras ticked on a 1080p canvas.

- [ ] CPU at 60 fps, per measured camera, at most 5% of one core → 
- [ ] "Frames missed due to rendering lag" over five minutes measuring against five minutes not
  measuring, at 30 fps → ____ / ____, at 60 fps → ____ / ____ (measuring must not exceed not
  measuring)
- [ ] The plugin's memory over 30 minutes measuring: start ____ end ____ (no growth)
- [ ] The dock updates about ten times a second; a profile shows no marker detection on OBS's UI
  thread → 
- [ ] Apply with all four cameras and the mic changing: the log's
  `[camera-sync] apply.result ok ... duration_ms=<d>` has d under 100 → d = ____ (C81)

## 7. What to bring back

- [ ] This file, filled in.
- [ ] Both OBS logs of part 1 (load lines and leak counts).
- [ ] The verbose log and steps from part 4 for T37's replay fixture.
- [ ] The three exported collections of part 5 and the two diffs.
- [ ] Screenshots, or log excerpts, of the rows in parts 3 and 5.
- [ ] Anything that failed, with what the dock showed.
