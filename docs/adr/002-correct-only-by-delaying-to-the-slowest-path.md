# ADR-002: Correct Only by Delaying, Aligned to the Slowest Path

- **Date**: 2026-10-11
- **Status**: Accepted
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: alignment, latency

## Context and Problem Statement

Every camera reaches OBS with its own lag, and so does the mic. To line them up, some paths have to wait for others. OBS can delay video (Video Delay (Async)) and delay audio (Sync Offset), but it cannot make video arrive earlier, and a negative Sync Offset stops at -950 ms. Wireless, Bluetooth and network mics can be later than every camera, so "just offset the mic" does not always exist as a correction.

On the OBS forum, users already reach for one workaround by hand: delay every faster camera to match the slowest, then offset the mic. The OBS Offset Calculator instead aligns everything to a master source the user picks, and compensates for negative values internally.

## Decision Drivers

- A correction must always exist within OBS's native ranges (ADR-001)
- Every millisecond of delay is added latency on stream and buffered frames in memory, so the total should be as small as possible
- One rule users can predict, matching the workaround they already know

## Considered Options

- **A. Delays only: align every camera and the mic to the slowest path, with the least total added delay**
- **B. Align to a master source the user chooses**
- **C. Fix late audio with a negative Sync Offset**

## Decision Outcome

Chosen: **A, delays only, aligned to the slowest path.** Faster cameras are delayed to match the slowest camera, and the mic is delayed to match them. When the mic is the slowest, the cameras are delayed to it and the mic gets no negative offset. No value Apply writes is negative.

### Positive Consequences

- Every case has a correction within the native ranges, including audio that is later than all the video
- Late audio needs no special path; it is just another case of "slowest wins"
- Users recognise the rule from the forum workaround

### Negative Consequences

- **The whole show inherits the slowest device's latency.** One slow camera (a phone over Wi-Fi, say) delays every other camera and the mic, and the plugin cannot make that camera faster. The fix is for the user to replace it
- More delay means more frames held in memory by each camera's filter
- When a user's own filter over-delays a camera, the plugin delays everything else to match instead of shrinking that filter (ADR-004), which adds latency the user could remove by hand

## Pros and Cons of the Options

### A. Delays only, slowest path ✅ Chosen

- ✅ Always solvable, with the least added latency
- ✅ No decision for the user to get wrong
- ❌ Total latency is set by the slowest device

### B. Master source

- ✅ Familiar from the Offset Calculator
- ❌ A master that is not the slowest needs negative corrections. Making those work means shifting everything else, which is option A with an extra choice for the user to get wrong
- ❌ A video master would need video moved earlier, which OBS cannot do

### C. Negative Sync Offset for late audio

- ✅ No video delay needed when the audio is only slightly late
- ❌ Bounded at -950 ms; network audio can exceed that
- ❌ Two correction directions to reason about instead of one

## Links

- [Design: Key decision 2](../../.design/multi-camera-sync.md#key-decisions)
- [ADR-001](./001-store-the-alignment-in-obs-native-settings.md): the native controls and their ranges
- [ADR-004](./004-edit-only-the-plugin-filter-and-the-mic-sync-offset.md): why the user's own filters are never shrunk
- [OBS Offset Calculator](https://obsproject.com/forum/resources/obs-offset-calculator.2209/): the master-source model
- OBS forum: [multiple cameras, different delay times](https://obsproject.com/forum/threads/multiple-cameras-different-delay-times.120646/), the slowest-camera workaround
