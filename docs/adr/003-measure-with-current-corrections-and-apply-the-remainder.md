# ADR-003: Measure With Current Corrections in Place and Apply the Remainder

- **Date**: 2026-10-11
- **Status**: Accepted
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: measurement, alignment

## Context and Problem Statement

Users will measure more than once: to check the result right after Apply, after changing a camera, or months later. Some will already have corrections in place, either from an earlier Apply or set by hand from forum advice. A **reading**, meaning one camera's offset to the mic, has to mean one thing, and Apply has to do the right thing with it no matter what is already configured.

Audio Video Sync Dock measures "the video and audio that are about to be encoded", that is, Program with everything already applied. Its numbers are the natural reference for checking this plugin's readings.

## Decision Drivers

- Measuring right after Apply should read about zero, so verification is just measuring again
- Applying twice must converge, not double the correction
- Corrections the user set by hand must be respected, not overwritten
- Readings must be comparable with Sync Dock's numbers for the camera on air

## Considered Options

- **A. A reading is what Program would show for that camera with every current correction in place; Apply adds the remainder to the current values**
- **B. Measure each device's raw lag, bypassing current corrections; Apply replaces the values with absolute ones**
- **C. Reset all corrections to zero, measure, then write absolute values**

## Decision Outcome

Chosen: **A, readings include current corrections and Apply adds the remainder.** It verifies itself, converges when repeated, and never clobbers what the user set.

### Positive Consequences

- Verification is free: re-measure after Apply, and every camera should read within one frame
- Apply is safe to repeat; a second Apply only adds what is still off
- Hand-set filters and offsets stay in place and are accounted for automatically
- Readings mean the same as Sync Dock's for the camera on air, which is how the Reading spike checks them

### Negative Consequences

- **The measuring tap is constrained.** It must sit after every filter on the camera and after the mic's Sync Offset. Where exactly a per-camera tap can do that is the Reading spike's open question ([ADR-007](./007-measure-all-cameras-in-one-pass.md))
- A reading goes stale the moment someone edits a value by hand, so Apply clears readings after writing and measuring starts again
- The plugin never learns a device's raw latency, only what is left to correct. That is enough for alignment but less useful for diagnosing a bad camera

## Pros and Cons of the Options

### A. Include current corrections, add the remainder ✅ Chosen

- ✅ Self-verifying, convergent, respects existing settings
- ❌ Ties the tap location to "what Program would show"

### B. Raw lag, absolute values

- ✅ Reports each device's true latency
- ❌ Apply would overwrite or double-count corrections the user set by hand
- ❌ Re-measuring after Apply reads the same raw lag again, so a reading cannot confirm the result

### C. Reset, then measure

- ✅ Simple arithmetic
- ❌ Wipes the user's settings before measuring, and on air that is a visible jump
- ❌ A reset that fails partway leaves the setup worse than before

## Links

- [Design: Key decision 3](../../.design/multi-camera-sync.md#key-decisions) and the [Reading slice](../../.design/multi-camera-sync.md#reading)
- [ADR-007](./007-measure-all-cameras-in-one-pass.md): where the per-camera tap sits
- [ADR-008](./008-make-apply-all-or-nothing-and-undoable.md): what Apply does with the remainder
- [Audio Video Sync Dock issue #13](https://github.com/norihiro/obs-audio-video-sync-dock/issues/13): the maintainer on what Sync Dock measures
