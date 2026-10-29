# Alignment verification

**Verdict**: FAIL
**Mutations**: 3 - probed C23, C36/C37, C24
**Proofs**: `proofs.md` at 44c8e78 - 94 pass; the 22 `walk` rows (CI and rig) have no `**Walked:**` line yet
**Verifier**: independent sub-agent (author != verifier)

All three owed mutations survived, and not only their narrow proofs: an exploratory run of each against the
whole core suite (`ctest --preset core`) left them green too. Three more survivors from that exploratory run
are findings 4 to 6. The production code was right in every case: each finding is a missing test, and the
proposed test passes at `44c8e78`. Each proposed test was built against the core with its mutation injected
(through `mutate.py`) and fails there.

## Findings

| Finding | Evidence | Fix | Closed |
| --- | --- | --- | --- |
| 1. C23: Apply can write the mic as the ns found plus Δ ms instead of s′ × 1,000,000 and no test fails; with a sub-ms offset that writes a negative Sync Offset (AC 6, AC 23) | mutation 1 not killed, `core/alignment/alignment.cpp:133`; `tests/alignment/apply.cpp:399-407` runs with the mic at 0 ns, where both forms give 100,000,000. With the mic at −200,400,123 ns and r (+60, +10) the mutant writes −400,123 ns | in "apply writes the mic's new sync offset in nanoseconds" add: mic −200,400,123 ns, r (+60, +10) → last write `write_offset mic 0` and the mic reads 0 | `bcd271b` - C23 now kills mutation 1 (sub-ms offset case) |
| 2. C36/C37: Undo's diff can ignore a filter's enabled state and no test fails; a plugin filter the user had disabled, which Apply only enabled at the same delay, stays enabled after Undo (AC 36, AC 67, ADR-008) | mutation 2 not killed, `core/alignment/alignment.cpp:202`; no case in `tests/alignment/undo.cpp` has a camera whose filter differs from the snapshot only in `enabled` | Cam A disabled at 60, r (+40, +100): Apply writes `write_filter cam-a 60 on`; Undo writes `write_filter cam-a 60 off` then `write_offset mic 0`, and Cam A reads disabled 60 | `bcd271b` - C37 now kills mutation 2 (enabled-only subcase) |
| 3. C24: a camera's read-back can compare only the effective delay and no test fails; Undo re-creating a disabled filter that reads back absent, or at another delay, would count as landed (AC 24, AC 25) | mutation 3 not killed, `core/alignment/write-steps.cpp:25`; every misread in `tests/alignment/write-steps.cpp` and `tests/alignment/apply.cpp`/`undo.cpp` lands on an enabled or absent target, and `FakePort` (`tests/support/fake-port.hpp:86-90`) only shifts the delay | in the write-steps tests add a step with target disabled 120 and found absent, `misread_after = {0}` → `ok` false and the filter removed again; add a `FakePort` misread of `enabled` alone and assert that step fails too | `bcd271b` - C24 now kills mutation 3 (disabled target, delay and enabled misreads) |
| 4. C36/C37: Undo's mic diff can compare whole ms and no test fails, so a hand edit made after Apply that lands in the snapshot's ms is never put back to the nanosecond (AC 36: "the snapshot's nanoseconds exactly") | exploratory mutation `if (to_ms(found->sync_offset_ns) != to_ms(snapshot.sync_offset_ns))`, `core/alignment/alignment.cpp:211`, not killed by the whole core suite | mic −200,400,123 ns, r (+60, +10), Apply; set the mic to −200,000,000 by hand; Undo → the mic reads −200,400,123 | `bcd271b` - C36 (same-ms hand edit); the exploratory mutation is killed |
| 5. C75 (with C71): Undo's duration can run from the request instead of the answer and no test fails | exploratory mutation dropping `started = clock_();` after Undo's question, `core/alignment/alignment.cpp:226-227`, not killed by the whole core suite; `tests/alignment/undo.cpp:286-302` runs off air | Undo on air with 3 s spent in the question and 2 ms per write → `undo.result ok written=2 skipped=1 duration_ms=4` | `bcd271b` - C90 appended; the exploratory mutation is killed |
| 6. C35: Undo can open the on-air question with nothing to change and no test fails; the dock would show an empty question, since `confirmation_question({0, false})` is "" | exploratory mutation `if (port_.on_air())` in place of `if (!steps.empty() && port_.on_air())` in `undo`, `core/alignment/alignment.cpp:221`, not killed by the whole core suite | Apply the first vector, put Cam B's filter and the mic back by hand, go on air; Undo → nothing asked, nothing written, "undone", snapshot dropped | `bcd271b` - C91 appended; the exploratory mutation is killed |

## Mutations injected

| Mutation | Location | Killed |
| --- | --- | --- |
| C23: Apply writes the mic as the ns found plus delta ms, not s' x 1,000,000 | `core/alignment/alignment.cpp:133` | no |
| C36/C37: Undo's diff compares a filter's presence and delay but not its enabled state | `core/alignment/alignment.cpp:202` | no |
| C24: a camera's read-back compares the effective delay, not the state written | `core/alignment/write-steps.cpp:25` | no |

The exploratory run against the whole core suite also injected these mutations. Each is outside `mutations.json`:

| Mutation | Location | Killed |
| --- | --- | --- |
| Undo's mic diff in whole ms (finding 4) | `core/alignment/alignment.cpp:211` | no |
| Undo's duration from the request (finding 5) | `core/alignment/alignment.cpp:226` | no |
| Undo asks with nothing to change (finding 6) | `core/alignment/alignment.cpp:221` | no |
| `within_one_frame` lower bound `>=` to `>` | `core/alignment/calculator.cpp:27` | yes (C2's −33 at F 33.0) |

## Own examples

These were throwaway doctest cases, built outside the repository against `build_core`'s core library at `44c8e78`. All of them pass.

- The calculator on inputs no example names: three cameras, with an enabled 0, a disabled 40 and an enabled 1,000 filter, the mic at OBS's −950 ms floor and r (+10, −500, +300). The result is Δ 950, d′ (940, 1450, 1650) and s′ 0. In a second case, an existing plugin filter at 20,000 ms with a delta of +1 is refused, naming that camera at 20,001. In a third, an offset of −500,000 ns rounds to −1 ms and the mic ends at 0. At 25 fps (F 40.0), ±40 is aligned and −41 is not. An empty set of readings is not Waiting.
- Apply with four cameras when the mic is the slowest path: four writes in order, the mic left at 0, and `written=4 skipped=1`.
- Undo cases: putting back the disabled state of a filter that Apply only enabled, restoring the nanoseconds over a hand edit in the same ms, counting the duration from the answer while on air, and asking nothing while on air when there is nothing to change. These pass at HEAD. They became findings 2, 4, 5 and 6 because no committed test holds them.
- View-model cases: check marks at F 40.0 on both bounds, "Aligned" only while every ticked camera is marked, and "Waiting for Brio" for a camera ticked after Apply that is still Looking. Also the question for 12 cameras with and without the mic, and an "Undone" line with 20000 and −950 (U+2212).

## Not judged here

- C21, C41 to C46, C64 to C69, C77 to C81, C86 and C87 are `walk` proofs that need CI or the rig. `proofs.md` shows them red until the Handoff's `**Walked:**` line names the CI run and the rig session.
- `src/` and `tests/integration/` were read but not built: this machine has no libobs. The port takes the first `async_delay_filter` with a recognised name through `obs_source_enum_filters` and appends a new one with `obs_source_filter_add`. Both match door 1 as long as libobs 30.0.2 enumerates the chain from the first filter applied. The CI integration case behind C42 is what proves that.
