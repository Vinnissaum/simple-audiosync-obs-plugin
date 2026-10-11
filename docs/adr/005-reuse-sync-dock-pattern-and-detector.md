# ADR-005: Reuse Audio Video Sync Dock's Pattern and Detector

- **Date**: 2026-10-11
- **Status**: Proposed. It is accepted when the detector spike passes (see Decision Outcome)
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: measurement, reuse

## Context and Problem Statement

Measuring is the hard part of this plugin, and it is where users of existing tools complain. Of five user reports in Audio Video Sync Dock's support thread, four are about measuring and none about applying. Klaps was forked specifically because detection failed in echoey rooms.

Sync Dock's **pattern** is a test video played on a phone: QR-numbered frames with sound pulses that carry the same numbers. It has the field record of 19,161 downloads, measures on OBS's own frame and audio timestamps, and is GPL-2.0-or-later. Its detector is written for Program, though:

- all its state lives in one per-instance structure;
- it tracks one pattern at a time;
- its audio decoding needs parameters read from the video's QR codes.

Klaps reworked detection for echo (chirp and DTMF audio, low-flash markers). It needs OBS 31 or later, analyses through paired encoders rather than raw buffers, has 7 GitHub stars, and its licence was not confirmed in this research.

## Decision Drivers

- The lowest-risk route to a detector that works
- Users who already own Sync Dock's pattern should need nothing new
- Detection must run once per camera ([ADR-007](./007-measure-all-cameras-in-one-pass.md)), on timestamps compatible with [ADR-003](./003-measure-with-current-corrections-and-apply-the-remainder.md)
- Detection must work in an ordinary room with the phone near the mic

## Considered Options

- **A. Reuse Sync Dock's pattern and detector, reworked to run once per camera**
- **B. Adopt Klaps' echo-tolerant pattern and detection**
- **C. Design our own pattern and detector**

## Decision Outcome

Proposed: **A, reuse Sync Dock's pattern and detector.**

**Accept when** the detector spike in the [Reading slice](../../.design/multi-camera-sync.md#reading) passes: a stable reading with the phone within arm's reach of the mic, in an ordinary untreated room, three runs at each of two phone distances. **If it fails**, a new ADR adopts B or C and supersedes this one, before the Alignment slice starts.

### Positive Consequences

- The pattern exists, and many users already have it
- The detector has been proven on thousands of setups, and its timestamp model fits ADR-003
- Far less new code than either alternative

### Negative Consequences

- The detector needs reworking: per-camera state, plus one audio decoder for the mic, fed QR parameters by whichever camera decodes them first
- **It inherits Sync Dock's weakness in echoey rooms.** Multi-camera rigs (halls, event spaces) may be exactly where that bites, which is why this ADR waits on the spike
- The plugin is tied to Sync Dock's pattern format. Changing it later means new media and new instructions for users
- GPL obligations apply to the reused code ([ADR-006](./006-license-the-plugin-gpl-2-0-or-later.md)), and each release ships pattern files generated with Sync Dock's tool

## Pros and Cons of the Options

### A. Reuse Sync Dock's ✅ Proposed

- ✅ Proven and familiar; least new code
- ❌ Echo weakness; needs reworking to run per camera

### B. Klaps' detection

- ✅ Built for real, echoey rooms
- ❌ Analyses through encoders, which is further from per-camera taps than Sync Dock's raw buffers
- ❌ OBS 31 or later, a small user base, and the licence is unconfirmed

### C. Our own

- ✅ Designed for per-camera use from the start
- ❌ The most work, aimed at the part users already find hardest
- ❌ A new pattern every user must obtain

## Links

- [Design: Key decision 5](../../.design/multi-camera-sync.md#key-decisions) and [Shape](../../.design/multi-camera-sync.md#shape) (the echo-tolerant alternative)
- [Audio Video Sync Dock](https://github.com/norihiro/obs-audio-video-sync-dock), its [forum resource](https://obsproject.com/forum/resources/audio-video-sync-dock.2028/) and [support thread](https://obsproject.com/forum/threads/audio-video-sync-dock.181514/)
- [Klaps](https://github.com/matiaspl/klaps)
