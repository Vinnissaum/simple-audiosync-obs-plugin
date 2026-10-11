# ADR-001: Store the Alignment in OBS's Native Settings

- **Date**: 2026-10-11
- **Status**: Accepted
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: alignment, obs-integration, portability

## Context and Problem Statement

The plugin computes an **alignment**: how much to delay each camera and the mic so they line up. That alignment has to live somewhere OBS applies it on every frame. OBS already ships two controls that can hold it:

- the Video Delay (Async) filter, 0 to 20,000 ms on any async video source;
- each audio source's Sync Offset in Advanced Audio Properties, -950 to 20,000 ms.

Where the alignment lives decides what happens to a user's setup when the plugin is disabled or uninstalled, or when the scene collection opens on another machine. It is the one choice in the design that cannot be walked back once users' scene collections depend on it.

## Decision Drivers

- A setup must stay aligned without the plugin: disabled, uninstalled, or the scene collection moved to another machine
- Users must be able to see and adjust every value in OBS's own dialogs
- Every delay the alignment needs fits within the native controls' ranges (ADR-002)
- Changing the storage later must not strand existing scene collections

## Considered Options

- **A. Native controls: a Video Delay (Async) filter per camera and the mic's Sync Offset**
- **B. A delay filter type of the plugin's own**
- **C. Plugin-held state, applied at runtime by the plugin itself**
- **D. Render Delay on the cameras**

## Decision Outcome

Chosen: **A, the native controls**. It is the only option where the alignment outlives the plugin and stays legible in OBS's own UI, and its ranges cover every correction the design makes.

### Positive Consequences

- Uninstalling the plugin, or opening the scene collection where it is not installed, leaves the setup aligned
- Every value can be seen and edited in the filter list and Advanced Audio Properties, so support can work from screenshots of OBS
- The plugin has no frame-buffering code of its own; OBS's filter does the delaying

### Negative Consequences

- The plugin is bound to the native limits. Apply refuses a correction above 20,000 ms rather than inventing a way around it
- Video Delay (Async) holds frames in memory, so a long delay on a high-resolution camera costs RAM the plugin cannot reduce
- Video Delay (Async) works only on async sources, which is why screen, window and game captures are out of scope
- Users can edit the values by hand. The design absorbs that by measuring with corrections in place (ADR-003)

## Pros and Cons of the Options

### A. Native controls ✅ Chosen

- ✅ Survives the plugin's absence; scene collections stay portable
- ✅ Visible and adjustable without the plugin
- ❌ Limited to what the native controls can express

### B. A delay filter of the plugin's own

- ✅ Full control over buffering and ranges
- ❌ Scene collections reference a filter type other machines may not have, and without the plugin the correction stops working
- ❌ Moving away from it later strands every setup built on it, which is the one-way door this decision exists to avoid

### C. Plugin-held runtime state

- ✅ Nothing is written into the scene collection
- ❌ The alignment disappears whenever the plugin is not loaded
- ❌ Reimplements buffering that OBS already does

### D. Render Delay on the cameras

- ✅ A native control that every OBS user has
- ❌ Audio Video Sync Dock's own instructions recommend Video Delay (Async) for cameras and advise against Render Delay
- ❌ OBS forum threads show users struggling with it on cameras

## Links

- [Design: Key decision 1](../../.design/multi-camera-sync.md#key-decisions) and the [Alignment slice](../../.design/multi-camera-sync.md#alignment)
- [ADR-002](./002-correct-only-by-delaying-to-the-slowest-path.md): every correction is a delay
- [ADR-004](./004-edit-only-the-plugin-filter-and-the-mic-sync-offset.md): what the plugin may edit inside these controls
- OBS source: [Video Delay (Async)](https://github.com/obsproject/obs-studio/blob/master/plugins/obs-filters/async-delay-filter.c) and [Advanced Audio Properties @ 30.2.0](https://github.com/obsproject/obs-studio/blob/30.2.0/UI/adv-audio-control.cpp)
- [Audio Video Sync Dock README](https://github.com/norihiro/obs-audio-video-sync-dock): its guidance on Video Delay (Async) versus Render Delay
