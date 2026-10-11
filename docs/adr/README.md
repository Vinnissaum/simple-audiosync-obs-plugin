# Architecture Decision Records

Durable records of the architectural choices behind the multi-camera sync plugin. Each records what was decided, what the alternatives were, and what each choice costs, so a future reader does not have to reconstruct the reasoning.

ADRs are **immutable**. A decision that changes is not edited: a new ADR supersedes it, and the old one is marked `Superseded by ADR-NNN`. A Proposed ADR becomes Accepted when its acceptance condition is met.

The format is [MADR](https://adr.github.io/madr/), and files are named `NNN-kebab-case-title.md`.

## Index

| ADR | Decision | Status | Tags |
| --- | --- | --- | --- |
| [001](./001-store-the-alignment-in-obs-native-settings.md) | Store the alignment in OBS's native settings | **Accepted** | alignment, obs-integration, portability |
| [002](./002-correct-only-by-delaying-to-the-slowest-path.md) | Correct only by delaying, aligned to the slowest path | **Accepted** | alignment, latency |
| [003](./003-measure-with-current-corrections-and-apply-the-remainder.md) | Measure with current corrections in place and apply the remainder | **Accepted** | measurement, alignment |
| [004](./004-edit-only-the-plugin-filter-and-the-mic-sync-offset.md) | Edit only the plugin's own filter and the mic's Sync Offset | **Accepted** | alignment, obs-integration, safety |
| [005](./005-reuse-sync-dock-pattern-and-detector.md) | Reuse Audio Video Sync Dock's pattern and detector | **Proposed** | measurement, reuse |
| [006](./006-license-the-plugin-gpl-2-0-or-later.md) | License the plugin GPL-2.0-or-later | **Accepted** | licensing |
| [007](./007-measure-all-cameras-in-one-pass.md) | Measure all selected cameras in one pass | **Proposed** | measurement, ux |
| [008](./008-make-apply-all-or-nothing-and-undoable.md) | Make Apply all-or-nothing and undoable | **Accepted** | alignment, safety, ux |

All eight come from the Key decisions of the [multi-camera sync design](../../.design/multi-camera-sync.md#key-decisions), confirmed by Vinicius Braz on 2026-10-11. "Accepted" means the decision is settled, not that it is implemented: no code exists yet.

## Proposed until a spike settles them

| ADR | Accepted when | If not |
| --- | --- | --- |
| 005 | The detector holds a stable reading with the phone within arm's reach of the mic in an ordinary untreated room, three runs at each of two distances | A new ADR adopts an echo-tolerant pattern and detector and supersedes 005, before the Alignment slice starts |
| 007 | Two cameras with different lags each read within one frame of Sync Dock's own reading for that camera, three runs in a row | A new ADR measures Program one camera at a time and supersedes 007; the design's "one pass" becomes "one session" |

Both spikes need the test rig, which does not exist yet: an OBS machine with two cameras that lag differently, a mic, and a phone to play the pattern.

## Relationship to the design

| Design Key decision | ADR |
| --- | --- |
| 1: the alignment lives in OBS's own settings | 001 |
| 2: every correction is a delay, aligned to the slowest path | 002 |
| 3: readings include current corrections; Apply adds the remainder | 003 |
| 4: the plugin edits only its own filter and the mic's Sync Offset | 004 |
| 5: Sync Dock's pattern and detector, so GPL-2.0-or-later | 005, 006 |
| 6: all cameras measured in one pass | 007 |
| 7: Apply is all-or-nothing and undoable | 008 |

Key decision 5 is split in two because only its first half waits on a spike. The licence holds either way, since libobs alone requires a GPL-compatible licence.

Some choices in the design are deliberately not recorded here: the Undo depth, Apply while on air, the filter name, macOS signing, Flatpak, the minimum OBS version and the product name. They are reversible defaults, and they live in the design's slices until one of them hardens into a decision.
