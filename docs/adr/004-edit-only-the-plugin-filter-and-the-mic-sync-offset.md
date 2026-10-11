# ADR-004: Edit Only the Plugin's Own Filter and the Mic's Sync Offset

- **Date**: 2026-10-11
- **Status**: Accepted
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: alignment, obs-integration, safety

## Context and Problem Statement

Apply writes into the user's scene collection (ADR-001). The cameras it touches may already carry filters: a Video Delay (Async) a user added from forum advice, a Render Delay, colour correction, anything. The plugin has to decide what it is allowed to change, and it has to find its own work again on the next run.

Klaps, the only plugin that applies a fix today, infers its target: it writes the Sync Offset only when exactly one audio source is on Track 1. Forum threads show users putting corrections in the wrong place by hand.

## Decision Drivers

- Never destroy or rewrite a user's own settings
- Apply and Undo must be predictable: they touch a known, small set of values
- The plugin must reliably find what it created on its last run
- The mic is chosen by the user, never inferred

## Considered Options

- **A. One plugin-owned Video Delay (Async) filter per camera, found by name, plus the chosen mic's Sync Offset. Nothing else is touched**
- **B. Adopt whichever Video Delay (Async) filter the camera already has**
- **C. Replace existing delay filters with one consolidated filter**

## Decision Outcome

Chosen: **A, the plugin's own filter plus the mic's Sync Offset.** The filter is created on the first Apply and found by a fixed name. Its working name is "Camera Sync Delay"; the design's Alignment defaults say it follows the product name. Filters the user added are never edited, reordered or removed, and their effect shows up in the reading (ADR-003).

### Positive Consequences

- A user's own filters are never at risk
- Undo only has to restore the values the plugin wrote
- The plugin's filter is recognisable in the filter list, so a user can see what Apply did

### Negative Consequences

- A camera can carry two delay filters, the user's and the plugin's. That is clutter, though the result is still correct
- **Finding the filter by name is fragile.** If a user renames it, the next Apply creates a new one. The renamed filter then counts as a user filter and is measured, so the alignment stays correct, just cluttered
- **Renaming the product later must keep recognising the old filter name**, or existing setups gain a second filter on their next Apply
- When a user's own filter over-delays a camera, the plugin cannot shrink it and delays everything else instead (ADR-002)
- The mic's Sync Offset is shared, not owned. If the user set it by hand, Apply adds to it rather than resetting it (ADR-003)

## Pros and Cons of the Options

### A. Own filter plus the mic's Sync Offset ✅ Chosen

- ✅ Never edits user settings; small and predictable write set
- ❌ Possible duplicate delay filters; the filter is identified by its name

### B. Adopt an existing delay filter

- ✅ No extra filter in the list
- ❌ Rewrites a value the user set for their own reasons
- ❌ Ambiguous when a camera has more than one delay filter

### C. Replace existing delay filters

- ✅ One clean filter per camera
- ❌ Deletes user settings; Undo would have to recreate whole filters

## Links

- [Design: Key decision 4](../../.design/multi-camera-sync.md#key-decisions), and the [Alignment slice](../../.design/multi-camera-sync.md#alignment) for the filter name default
- [ADR-001](./001-store-the-alignment-in-obs-native-settings.md), [ADR-002](./002-correct-only-by-delaying-to-the-slowest-path.md), [ADR-003](./003-measure-with-current-corrections-and-apply-the-remainder.md)
- [Klaps](https://github.com/matiaspl/klaps): Correct Sync Offset applies only with exactly one audio source on Track 1
