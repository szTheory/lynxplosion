---
phase: 05-display-raw-video-and-input
plan: "02"
subsystem: video-input
tags: [c, cmake, ctest, raw-video, timer-cadence, palette]

requires:
  - phase: 04-timers-interrupts-and-first-public-suites
    provides: Guest-tick timer scheduler, half-open budgets and same-tick event ordering
  - phase: 05-01
    provides: Public frame API, bounded raw capture buffers and VIDEO_POLICY1 timing identity
provides:
  - Guest Timer0/2-driven cadence checks for source presets and a short frame
  - Raw geometry, row palette snapshots, line ticks and aligned normal-mode address traversal
  - Omission and clearing of unfinished DMA rows while retaining consumed guest work
affects: [05-display-raw-video-and-input, 06-audio-and-playable-checkpoint]

# Measured over the plan task diff through ce34795: 36,564 diff characters / 4.
actuals:
  tokens: 9141
  tasks: 2
  commits: 4
commits: 4
plan_head_before: 14a6ac333794a334d97939e8bd5e2c7d4068028b
plan_head_after: f8b6527c56b7d7f1b11ff958ae46d0448233dd4d

tech-stack:
  added: []
  patterns:
    - Timer borrow notifications define row and frame progress under guest time
    - Bounded output arrays are checked with row-tail canaries and semantic byte assertions
    - Palette and first-edge timing policies are documented separately from hardware claims

key-files:
  created:
    - .planning/phases/05-display-raw-video-and-input/05-02-task1-red.json
    - .planning/phases/05-display-raw-video-and-input/05-02-task2-red.json
  modified:
    - core/video.c
    - docs/video-input.md
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "An unfinished row is omitted and its unused capture slot is cleared; already granted DMA reads and guest time remain committed."
  - "The normal-mode effective display address masks its low two bits at the third-blank transfer; guest backup bytes remain unchanged."
  - "Only DISPCTL DMA-enable bit 0 is supported by this raw-capture subset; flip, color and two-bit mode writes are refused atomically."
  - "Per-row palettes are sampled at the Timer 0 row-start event after timer work and before a same-tick CPU store; this remains software policy."

patterns-established:
  - "A short programmed Timer0/2 period exercises partial-frame geometry without inventing a fixed host refresh rate."
  - "Geometry checks compare raw RAM bytes and asymmetric palette bytes directly, including address wrap and CPU device overlays."

requirements-completed: []
requirements-partial: [VID-01, VID-03, VID-04, VID-05]

coverage:
  - id: D1
    description: "Timer 0/2 settings determine measured steady frame periods and half-open frame return edges for the source presets and a short cadence."
    requirement: VID-01
    verification:
      - kind: unit
        ref: "api.video_cadence; api.frame_edges; api.scheduler_order; api.scheduler_limits"
        status: pass
    human_judgment: false
  - id: D2
    description: "Normal-mode raw capture reports 0, 1 and 102 actual rows, masks the display address at the third blank edge, wraps through 16-bit RAM, and preserves caller canaries."
    requirement: VID-03
    verification:
      - kind: unit
        ref: "api.video_geometry"
        status: pass
    human_judgment: false
  - id: D3
    description: "All 16 asymmetric raw palette entries are checked at selected row boundaries with explicit palette and pen-byte ordering."
    requirement: VID-04
    verification:
      - kind: unit
        ref: "api.palette_boundary; api.video_geometry"
        status: pass
    human_judgment: false
  - id: D4
    description: "Timer 2 defines the guest-tick frame-complete boundary and incomplete rows are omitted from published geometry."
    requirement: VID-05
    verification:
      - kind: unit
        ref: "api.video_cadence; api.frame_edges; api.video_geometry"
        status: pass
    human_judgment: false

duration: "about 35 minutes recorded from the first task commit through closeout; initial setup was not timed"
completed: 2026-11-11
status: complete
---

# Phase 5 Plan 02: Programmable Frames, Geometry and Palette Summary

**Timer-driven raw frames now honor guest cadence, omit incomplete DMA rows and expose aligned normal-mode geometry with per-row palette observations.**

## Performance

- **Duration:** About 35 minutes from the first recorded task commit through closeout; earlier setup was not timed.
- **Started:** 2026-11-11T20:52:57-05:00 (first recorded task commit; plan setup began earlier)
- **Completed:** 2026-11-11T21:28:11-05:00 (metadata closeout window)
- **Tasks:** 2
- **Files touched by plan tasks:** 8 (six source/test/contract sidecars and two TDD evidence records)

## Accomplishments

- Extended API checks across the documented 50/60/75 Timer 0 presets and a short Timer 0/2 period. Measured reload periods follow guest timer settings; exact-target frame events are deferred, zero budget does no work, and a due frame is returned only once.
- Fixed unfinished-row handling at row rollover and frame end. Incomplete rows no longer increase `row_count`, and their pen, palette and tick slots are cleared while already granted DMA reads and guest time remain committed.
- Added geometry checks for zero, one and 102 actual rows; row 0, row 1 and row 101; raw high/low pen nibbles; 16-bit address wrap; underlying RAM beneath CPU device overlays; third-blank address transfer; and caller canaries after the 102-row capacity.
- Masked the effective normal-mode display address down to a four-byte boundary at the documented third-blank transfer. The raw backup register is retained. Added full asymmetric 16-entry palette snapshots at row boundaries and documented their software ordering.
- Kept `LEGACY_FLAT5` as the default and left timer phase, grant costs and palette sampling labelled as software policy. `DISPCTL` accepts only DMA-enable bit 0; unsupported flip, color and two-bit mode writes leave the selected value unchanged. The primary [Epyx Display chapter](https://www.monlynx.de/lynx/lynx5.html) describes bottom-up traversal in flip mode; this plan explicitly refuses that mode and makes no flipped-output claim.
- Updated the sorted fast-test inventory and rebound the existing claim hash without changing its profile or scope.

## Task Commits

1. **Task 1 tests (RED):** `bc6ad61` — `test(05-02): add failing tests for programmable video cadence`
2. **Task 1 implementation (GREEN):** `3f4a781` — `fix(05-02): omit incomplete DMA rows from published frames`
3. **Task 2 geometry/palette tests and address fix:** `ce34795` — `fix(05-02): mask display addresses and verify raw frame geometry`

Task 2's baseline geometry failure and its fix landed in one commit after the owner resolved the normal-main commit approval interruption. The pre-fix run and semantic assertions are recorded in `05-02-task2-red.json`; no separate RED commit is claimed. The palette case passed against baseline behavior before the alignment fix and is not described as a RED. The separate owner-authority and blocker-resolution record is commit `f8b6527`.

**Measured task-plan commits:** 4 commits from `plan_head_before` through `plan_head_after`: the three task/test commits above and the parent-owned authority/blocker-resolution commit. The final summary/progress metadata commit is separate.

## Verification

- Fresh headless `cmake --preset check` and `cmake --build --preset check` passed after the source commits.
- Combined owning CTest selection `api.video_cadence`, `api.frame_edges`, `api.scheduler_order`, `api.scheduler_limits`, `api.video_geometry`, and `api.palette_boundary` passed 6/6 in 0.19 seconds.
- `api.video_geometry` baseline RED failed on the unmasked third-blank address and the resulting normal-row/RAM-overlay observations. After the fix, it checks aligned traversal across the full 16-bit address space, exact row ticks and all caller guards.
- `api.palette_boundary` passed on baseline and after the fix. It compares all 16 raw green and blue/red bytes for asymmetric values before and after selected row boundaries; no host RGB conversion is used.
- Both RED artifacts passed `gsd_run check tdd-red-evidence`: Task 1's short-frame assertion observed `row_count=2`, `flags=1`, and `pen0=171` at ticks 80–160 instead of omitting the unfinished rows; Task 2's geometry case failed four address/traversal assertions before masking.
- `sh scripts/hygiene.sh --tree`, `sh scripts/scan-binaries.sh --source --root .`, `sh scripts/check-claims.sh`, `LC_ALL=C sort -c tests/inventory/fast.txt`, and `git diff --check` passed before the task commits.

## TDD Gate Compliance

- **Task 1:** `api.video_cadence` produced a semantic RED for incomplete-row publication. `discard_incomplete_row()` fixed rollover and frame-end behavior; cadence, edge and scheduler checks passed.
- **Task 2:** `api.video_geometry` produced a semantic RED because `backup_addr` was used without masking at ordinal 2. The effective counter now uses `backup_addr & 0xFFFC`. The palette case was already green and remains a positive regression test.
- Task 2's tests and green fix are combined in `ce34795` because the normal commit action was paused for owner authority. This summary does not invent a separate post-hoc RED commit.

## Files Created/Modified

- `core/video.c` — discard and clear unfinished rows; apply aligned effective display address; stable palette/address mutation anchors.
- `docs/video-input.md` — partial-row behavior, address traversal, unsupported display modes and row palette policy.
- `tests/api/test_api.c` — cadence, frame-edge, geometry, canary and palette-boundary cases.
- `tests/api/tests.cmake`, `tests/inventory/fast.txt` and `docs/claims/skeleton-repeatability.claim` — exact registration and claim binding.
- `05-02-task1-red.json` and `05-02-task2-red.json` — semantic RED records; the second record is part of the Task 2 commit.

## Decisions Made

- Keep frame/line timing derived from guest timers; the fixed-zero first-divider phase is an unverified policy separate from steady reload-period assertions.
- Count only completed rows. On an interrupted row, clear that row's unused observation slot without undoing DMA grants or guest time.
- Mask the display address only when the backup transfers at the third blank edge, preserving the guest backup value.
- Refuse display modes whose LCD traversal/encoding is not implemented. Normal raw capture is tested; flipped traversal remains unsupported and unclaimed.
- Snapshot palette bytes at the Timer 0 row-start event after timer work and before same-tick CPU work. This ordering is software policy, not hardware latch qualification.

## Deviations from Plan

No product-scope deviation. Task 2's RED evidence and green implementation share one commit because the owner paused the normal main-branch commit pending an explicit decision; after approval, the normal scoped commit and hooks succeeded. The baseline-green palette test was not presented as a RED. The approval history and exact original rejection are preserved in [05-02-COMMIT-BLOCKER.md](./05-02-COMMIT-BLOCKER.md).

## Issues Encountered

The automatic approval review initially rejected a normal task-scoped commit on `main`. The owner then explicitly authorized “Commit checked changes on main”; three task/test commits landed with normal hooks and no branch/configuration bypass. The historical blocker is resolved. This permission does not authorize release, publication, rights-bearing content or physical testing.

## Known Stubs

None introduced. Flip, color and two-bit display modes are explicit atomic refusals; the raw-capture subset does not silently emulate those LCD modes.

## Threat Flags

None. The changes add no new network endpoint, authorization path, filesystem access pattern or external trust boundary; address arithmetic is bounded by the 16-bit guest address type and covered through wrap.

## Phase-Level Obligations and Boundaries

- `VID-01`, `VID-03`, `VID-04` and `VID-05` are delivered in this plan but remain phase-partial until the phase's full coverage and root closeout. `VID-02`, `INP-01`, `API-06`, `HB-02`, `CON-01`, `EVD-02` and recurring `EVD-05` remain open for their owning plans.
- The strict timer-suite offset-14 comparison remains source `0D` versus observed `0C`; this plan does not tune timer policy or change the source/recipe/descriptor.
- No page-mode suite result, Lynx I/II baseline, hardware timing qualification, firmware/game content use, LibFuturo change or release claim is made. O1, O5, O6, O9/HOST-02 and physical-test stops remain in force.
- The pre-existing untracked `.planning/state.json` was not staged or modified.

## Next Phase Readiness

Plan 05-02 is complete; Plan 05-03 may proceed under the existing roadmap when the root orchestrator dispatches it. No later-plan implementation was started here.

## Self-Check: PASSED

The summary exists, and commits `bc6ad61`, `3f4a781`, `ce34795` and `f8b6527`
are ancestors of HEAD. The persisted plan ledger measures four commits from
`14a6ac333794a334d97939e8bd5e2c7d4068028b` through
`f8b6527c56b7d7f1b11ff958ae46d0448233dd4d`, matching `actuals.commits`.
No summary stub or skipped owning verification was introduced.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-11*
