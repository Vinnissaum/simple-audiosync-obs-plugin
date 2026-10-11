# ADR-006: License the Plugin GPL-2.0-or-later

- **Date**: 2026-10-11
- **Status**: Accepted
- **Deciders**: Vinicius Braz (@Vinnissaum, maintainer)
- **Tags**: licensing

## Context and Problem Statement

The plugin links libobs, which is GPL-2.0-or-later. A plugin linked against it is generally treated as a derivative work, so it needs a GPL-compatible licence. [ADR-005](./005-reuse-sync-dock-pattern-and-detector.md) also proposes reusing Audio Video Sync Dock's code, which is GPL-2.0-or-later as well. A licence is easy to choose now and hard to change later: once other people contribute, relicensing needs every contributor's consent.

## Decision Drivers

- Compatibility with libobs and with any reused GPL code
- Keep the option of combining GPL-3.0 code later
- Match the plugins it sits beside on the OBS forum

## Considered Options

- **A. GPL-2.0-or-later**
- **B. GPL-2.0-only**
- **C. A permissive licence (MIT, BSD)**

## Decision Outcome

Chosen: **A, GPL-2.0-or-later**, the same terms as libobs and Sync Dock. The choice stands even if ADR-005 is superseded, because libobs alone already requires a GPL-compatible licence.

### Positive Consequences

- Reusing Sync Dock's code needs no licence analysis beyond keeping its notices
- Users and contributors get the same terms as OBS itself
- GPL-3.0 code can still be combined later, with the combined work under version 3

### Negative Consequences

- Anyone distributing modified builds must publish the source
- Reused files must keep their upstream copyright notices and attribution. `CLAUDE.md` makes that a working rule
- Nobody can reuse this code under permissive terms
- Once there are outside contributors, changing the licence needs their consent

## Pros and Cons of the Options

### A. GPL-2.0-or-later ✅ Chosen

- ✅ Matches libobs and Sync Dock exactly; leaves version 3 open
- ❌ Copyleft obligations for anyone who redistributes

### B. GPL-2.0-only

- ✅ Compatible with libobs and the reused code
- ❌ Cannot be combined with GPL-3.0-only code

### C. Permissive

- ✅ The widest reuse by others
- ❌ Not possible while reusing GPL code, and doubtful for a plugin that links libobs

## Links

- [Design: Key decision 5](../../.design/multi-camera-sync.md#key-decisions)
- [ADR-005](./005-reuse-sync-dock-pattern-and-detector.md): the code reuse this licence covers
- [libobs `obs.h`](https://github.com/obsproject/obs-studio/blob/master/libobs/obs.h): GPL-2.0-or-later header
- [Audio Video Sync Dock](https://github.com/norihiro/obs-audio-video-sync-dock): GPL-2.0-or-later
