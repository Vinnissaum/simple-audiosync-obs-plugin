# ADR-007: Measure All Selected Cameras in One Pass

- **Date**: 2026-10-11
- **Status**: Proposed. It is accepted when the tap spike passes (see Decision Outcome)
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: measurement, ux

## Context and Problem Statement

What hurts multi-camera users is the loop: measure each camera separately, then work out how far to delay each one. Audio Video Sync Dock and Klaps both measure Program, so they can only measure the camera that is on air, one at a time. The design's success line promises one measure-and-apply pass with no video editor.

Keeping that promise needs a tap on each camera that reproduces what Program would show for it, with current corrections included (ADR-003). Whether such a tap exists is unproven. Reading the devices' own latency reports is no shortcut, because forum answers say drivers report latency unreliably.

## Decision Drivers

- The success line: one pass, every camera, no editor
- Each reading must equal what Program would show for that camera, within one frame
- Cameras that cannot all see the phone at once must still be measurable

## Considered Options

- **A. A tap per camera, with every camera measured at once and readings kept per camera until Apply or Reset**
- **B. Measure Program one camera at a time, as Sync Dock does, with the dock walking the user through the cameras**
- **C. Read each device's own latency report, with no pattern**

## Decision Outcome

Proposed: **A, every camera measured at once.**

**Accept when** the tap spike in the [Reading slice](../../.design/multi-camera-sync.md#reading) passes. Two cameras with different lags must each read within one frame of Sync Dock's own reading for that camera, three runs in a row. Two taps are candidates:

- a measuring filter at the end of each camera's filter chain;
- a private video mix per camera, measured the way Sync Dock measures Program.

The spike stops after three working days on the test rig. **If neither tap matches**, a new ADR adopts B and supersedes this one, and the design's "one pass" becomes "one session".

### Positive Consequences

- One phone, one pass, every camera, with no scene switching while measuring and Program left untouched
- Moving the phone to reach another camera keeps the readings already taken
- The dock can show every camera's state side by side

### Negative Consequences

- One detector per camera runs at the same time, so measuring costs CPU in proportion to the camera count, from QR decoding on every frame
- **Reproducing Program's timing at each camera is the plugin's main technical risk**, and nothing here retires it until the spike runs
- A private mix per camera, if that is the tap that works, costs GPU while measuring and may raise the minimum OBS version
- In rigs where the cameras face different ways, the user still has to carry the phone around. That is supported, but it is not a single gesture

## Pros and Cons of the Options

### A. Every camera at once ✅ Proposed

- ✅ Keeps the success line; the only option that removes the loop
- ❌ Unproven tap; measuring cost grows with the number of cameras

### B. Program, one camera at a time

- ✅ Proven, since it is exactly Sync Dock's model
- ❌ Program has to be switched between cameras while measuring, so it cannot run quietly before going live
- ❌ Keeps part of the loop the plugin exists to remove

### C. Device latency reports

- ✅ No phone and no pattern
- ❌ Forum answers say drivers report latency unreliably

## Links

- [Design: Key decision 6](../../.design/multi-camera-sync.md#key-decisions), [Success](../../.design/multi-camera-sync.md#success) and the [Reading slice](../../.design/multi-camera-sync.md#reading)
- [ADR-003](./003-measure-with-current-corrections-and-apply-the-remainder.md): what a reading must equal
- [ADR-005](./005-reuse-sync-dock-pattern-and-detector.md): the detector each tap feeds
- OBS forum: [a visual way to sync](https://obsproject.com/forum/threads/visual-way-to-sync-up-video-audio.149304/), on per-source measuring and driver-reported latency
