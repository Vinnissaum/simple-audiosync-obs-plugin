# ADR-008: Make Apply All-or-Nothing and Undoable

- **Date**: 2026-10-11
- **Status**: Accepted
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: alignment, safety, ux

## Context and Problem Statement

Apply writes several values into a live setup: one filter per camera and the mic's Sync Offset (ADR-001, ADR-004). The values only make sense together. A camera delayed without the matching mic offset is worse off than before, and on air the cameras would disagree with each other. Users may also apply while streaming or recording, where a wrong result is seen at once and has to be recoverable at once.

## Decision Drivers

- Never leave a half-applied alignment
- One-click recovery from a bad Apply
- No surprise changes on air

## Considered Options

- **A. All-or-nothing: Apply needs a stable reading for every selected camera, writes every value or restores them all, and keeps what it replaced for Undo**
- **B. Apply each camera as soon as its reading stabilises**
- **C. No Undo, relying on OBS's dialogs to revert by hand**

## Decision Outcome

Chosen: **A, all-or-nothing with Undo.** If a write fails partway, for example because a source vanished, every value read before writing is restored. Two of the design's Alignment defaults are reversible and not part of this decision: how far back Undo reaches (currently one Apply, for the current session) and whether Apply asks first while on air (currently yes).

### Positive Consequences

- The setup is always either the old alignment or the new one
- Undo is one click and restores exactly what was there
- On-air changes are deliberate

### Negative Consequences

- **One camera that never stabilises blocks Apply for all of them.** The user has to deselect it to go ahead
- **"All-or-nothing" is best effort.** OBS settings writes are not transactional, so a failure is handled by restoring the values read beforehand, but a crash in the middle of the writes could still leave a partial state
- Undo overwrites any hand edits made after Apply, because it restores the earlier values exactly
- With the current default, Undo does not survive an OBS restart, though every value stays visible in OBS's own dialogs

## Pros and Cons of the Options

### A. All-or-nothing with Undo ✅ Chosen

- ✅ No half-applied states in normal operation; one-click recovery
- ❌ One unstable camera blocks the rest; atomicity is best effort

### B. Per-camera, as readings stabilise

- ✅ Progress without waiting for the slowest reading
- ❌ The alignment depends on every camera at once (ADR-002), so applying one camera before the others are known can be wrong, and is wrong whenever an unmeasured camera turns out to be the slowest
- ❌ The cameras disagree on air until the last one lands

### C. No Undo

- ✅ No state to keep
- ❌ Recovery means finding and retyping several values in different dialogs, possibly on air

## Links

- [Design: Key decision 7](../../.design/multi-camera-sync.md#key-decisions) and the [Alignment slice](../../.design/multi-camera-sync.md#alignment) (Undo and on-air defaults)
- [ADR-001](./001-store-the-alignment-in-obs-native-settings.md), [ADR-002](./002-correct-only-by-delaying-to-the-slowest-path.md), [ADR-004](./004-edit-only-the-plugin-filter-and-the-mic-sync-offset.md)
