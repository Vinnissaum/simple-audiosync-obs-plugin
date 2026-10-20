# Alignment Context

**Gathered:** 2026-10-20
**Spec:** `.specs/features/alignment/spec.md`
**Status:** Ready for design

---

## Feature Boundary

Apply turns every ticked camera's stable reading into an alignment, a delay on each camera's "Camera Sync Delay" filter and a Sync Offset for the mic, and writes it into OBS's own settings all or nothing. Undo puts back the values from before the last Apply, for the current OBS session. The dock offers both buttons, asks first while on air, and says what happened. The arithmetic, the refusals, the limits and the write order come from the design's Alignment slice, ADR-002 to ADR-008 and the TDD; this file records only the decisions those documents left open.

---

## Implementation Decisions

### Showing the result

- After a successful Apply the message line shows one summary built from the plan: "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms", every ticked camera's new delay in checklist order, then the mic's new Sync Offset.
- After a successful Undo it shows the restored values the same way: "Undone - Cam A 0 ms, Cam B 0 ms, Shure MV7 0 ms".
- The summary stays until another message of higher precedence needs the line, or until the next Apply, Undo, Reset or collection switch.
- Rows go back to "Looking for the pattern" as the readings are cleared. No delay or Sync Offset is shown on the rows, and nothing is read from OBS to draw the dock.

### Aligned and Already aligned

- Apply stays enabled while every ticked camera is stable. Pressed while every reading is within one frame, it writes nothing and the message line shows "Already aligned".
- After a successful Apply, once every ticked camera is stable again within one frame, the message line shows "Aligned" and each row's status gets a check mark: "✓ +4 ms - video late".
- That lasts until the next Apply, Undo, Reset or collection switch.

### On-air confirmation

- While streaming or recording, Apply reads the current values and computes the plan first (nothing written), then asks.
- {n} counts only the cameras whose delay changes, and the mic is named only when its Sync Offset changes: "Shift 2 cameras and the mic while on air?", "Shift 2 cameras while on air?".
- When the plan changes nothing there is no question; that case is "Already aligned", which is decided before anything is read.
- The question adds one sentence: "Cameras whose delay changes pause until their new delay fills."
- This orders the question after the reads, where the design's sequence diagram draws it before them. No write happens before the answer, so "cancelling writes nothing" holds and no Key decision changes.

### Measuring after Undo

- Undo clears every reading, since they are stale once values change (ADR-003), and leaves the session as it was: still measuring if it was, stopped if it was.
- A cleared camera shows "Looking for the pattern", as after Reset. (The preview during discussion said "rows empty" for a stopped session; the code shows "Looking for the pattern" for a cleared camera in any state, and the spec follows the code.)

### Agent's Discretion

None. Every area had a recommended option accepted.

### Declined / Undiscussed Gray Areas → Assumptions

Every gray area offered was discussed. The smaller choices that fell out of the four decisions are logged as assumptions in the spec: the wording for one camera and for the mic alone, the order the message line ranks its messages in, a check mark per row before every row has one, what happens when values change while the question is open, and whether Undo asks while on air.

---

## Specific References

- The previews accepted during discussion:

  ```
  Cam A    [x] Looking for the pattern
  Cam B    [x] Looking for the pattern
  Mic: Shure MV7

  [Stop] [Reset] [Apply] [Undo]
  Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms
  ```

  ```
  Shift 2 cameras and the mic while on air?
  Cameras whose delay changes pause until
  their new delay fills.

            [Cancel]  [Apply]
  ```

  ```
  Re-measured after an Apply:
    Cam A  [x] ✓ +4 ms - video late
    Cam B  [x] ✓ 0 ms - in sync
    Aligned
  ```

---

## Deferred Ideas

- Showing each camera's current plugin delay and the mic's Sync Offset on the rows at all times, read from OBS (offered as an option and not chosen).
