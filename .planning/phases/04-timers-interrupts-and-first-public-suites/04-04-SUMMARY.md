---
phase: "04-timers-interrupts-and-first-public-suites"
plan: "04"
subsystem: timing
tags: [Mikey, IRQ, scheduler, CPU profiles]
requires:
  - phase: "04-03"
    provides: BLL-backed machine execution, mapped bus routes, timer bank and bounded scheduler
provides:
  - Original guest IRQ latch and CPU POLL boundary observations under both explicit CPU profiles
  - Permanent scheduler ordering, watched-prefix and arithmetic-limit cases
  - Mapped Mikey versus unavailable-byte coverage in the MAPCTL boundary matrix
affects: ["04-06 snapshot serialization", "Phase 4 final verification"]
actuals:
  tokens: 11626
  tasks: 2
  commits: 3
plan_head_before: cbe9aa28caac2995cb15ca3d2edf9b48cf505fa7
plan_head_after: 0bdc78559ba3df9151ca6ceb53c0eeefe03ab126
tech-stack:
  added: []
  patterns: ["Independent expected event prefixes", "Private scheduler selection seam"]
key-files:
  created:
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-04-SUMMARY.md
    - .planning/phases/04-timers-interrupts-and-first-public-suites/deferred-items.md
  modified:
    - core/internal.h
    - core/machine.c
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/timers-interrupts.md
key-decisions:
  - "Keep existing already-green IRQ and scheduler behavior; test the real path without manufacturing RED evidence."
  - "Timer-before-CPU ordering is deterministic software policy, not hardware timing evidence."
  - "The scheduler has a bounded 0–7 timer ordinal range and no dynamic sequence counter; sequence exhaustion is structurally absent."
  - "Timer event-cursor snapshot serialization remains owned by Plan 06."
patterns-established:
  - "Capture exact guest tick, pending state, timer status, CPU request and watch return independently of shared hashes."
  - "Route mapped-device and unsupported-address controls separately in the MAPCTL matrix."
requirements-completed: []
coverage:
  - id: D1
    description: "Original guest verifies timer IRQ latch, source-enable/pending separation, acknowledgements and CPU POLL timing under both profiles."
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^api\\.(irq_pending|irq_poll)$'"
        status: pass
    human_judgment: false
  - id: D2
    description: "Scheduler ordering, timer/cascade prefix, watched CPU completion and arithmetic limits have exact expected observations."
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^api\\.(scheduler_order|scheduler_limits|scheduler_watch)$'"
        status: pass
    human_judgment: false
  - id: D3
    description: "MAPCTL matrix distinguishes implemented timer registers from unavailable Mikey bytes and RAM overlays."
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^memory\\.map_matrix(\\.self_test)?$'"
        status: pass
    human_judgment: false
duration: 46min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 4: IRQ Boundaries and Scheduler Prefix Summary

**Timer IRQ and scheduler tests capture exact pending, POLL, timer-prefix, watch-return and arithmetic behavior without claiming hardware timing accuracy.**

## Performance

- **Duration:** 46 min
- **Started:** 2026-11-11T05:41:26Z
- **Completed:** 2026-11-11T06:27:50Z
- **Tasks:** 2
- **Files modified:** 7 implementation and evidence files; two planning records created

## Accomplishments

- Added original BLL guest checks for mapped INTSET/INTRST, simultaneous pending bits, timer source enable versus pending state, IRQ stack frame and handler acknowledgement under `lynx1` and `lynx2`.
- Added independent timer/POLL and same-tick acknowledgement observations at ticks 75, 80 and 85, including the observed handler store at 150 and watch return at 151.
- Registered scheduler ordering, watch-prefix and arithmetic-limit cases. The timer-only trace compares ticks 16/32/48/64/80 across coarse, one-tick and irregular advancement; the watched CPU prefix records the grant at 80 and return at 81.
- Updated the MAPCTL matrix to recognize mapped Mikey timer registers while retaining negative controls at unsupported addresses. The final affected CTest set passed 16/16.

## Task Commits

1. **Task 1: Run an original timer IRQ handler across source-enable and POLL boundaries** — `c889340` (`test`).
2. **Task 2: Make total-order, watch-prefix and arithmetic limits permanent** — `d96f08d` (`test`).
3. **Evidence wording correction:** `0bdc785` (`docs`), clarifying the CPU-held timer prefix and default `none` profile for the watched CPU prefix.

The final plan metadata commit records this summary and the GSD state/roadmap updates.

## Files Created/Modified

- `core/internal.h`, `core/machine.c` — private event ordering seam shared by production selection and the permutation test.
- `tests/api/test_api.c`, `tests/api/tests.cmake`, `tests/inventory/fast.txt` — guest observations, scheduler cases and permanent test inventory.
- `docs/claims/skeleton-repeatability.claim`, `docs/timers-interrupts.md` — exact inventory hash and evidence boundaries.
- `deferred-items.md` — precise Plan 06 snapshot cursor follow-up.

## Decisions Made

- The tests found the existing IRQ latch and same-tick path already green, so no core behavior change or invented red result was needed. The plan’s task `tdd="true"` markers did not override the project’s disabled global TDD mode or the observed green baseline.
- The production scheduler coalesces each timer-bank tick into one candidate and has one CPU candidate. Stable timer source ordinals are bounded from 0 through 7; the implementation has no dynamic sequence counter, so sequence exhaustion is not an applicable runtime condition. The test permutes all eight fixed ordinals through the production comparator and checks the CPU priority after them.
- Pending-versus-enable behavior is source-backed; the selected fixed-zero divider phase, timer-before-CPU tie order and placeholder CPU timing remain software policy. No Lynx profile qualification or hardware accuracy claim is made.
- Timer snapshot continuation is incomplete until Plan 06 serializes the timer event cursor. This plan makes no snapshot-completion claim and does not mark phase-wide requirements complete.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Test integration] Updated stale MAPCTL matrix expectations for newly implemented timer registers**
- **Found during:** Final full check preset
- **Issue:** `memory.map_matrix` treated available timer registers at `$FD00` and the pending aliases as unavailable, while the same test needed to keep rejecting unsupported bytes in the Mikey window.
- **Fix:** Added a device-route case for implemented timer addresses and expanded the matrix around `$FD1F/$FD20`, `$FD7F–$FD82`, preserving overlay and unavailable-path assertions.
- **Files modified:** `tests/api/test_api.c`
- **Verification:** `memory.map_matrix` and its negative-control self-test passed in the final 16-test affected regression run.
- **Committed in:** `d96f08d`

**2. [Rule 1 - Test bug] Fixed candidate permutation compaction bounds**
- **Found during:** Task 2
- **Issue:** ASan found the test harness’s `memmove` length used the full candidate count instead of the remaining suffix after the selected item.
- **Fix:** Bound the moved suffix by `remaining_count - best - 1`.
- **Files modified:** `tests/api/test_api.c`
- **Verification:** The scheduler cases and final affected regression set passed after the fix.
- **Committed in:** `d96f08d`

**3. Added a private comparator test seam for actual selection-order permutations**
- **Found during:** Task 2
- **Issue:** The comparator was file-local, preventing the planned traversal-permutation proof from exercising production ordering logic.
- **Fix:** Moved its private event type and constants into `core/internal.h` and exposed the comparator only to internal code/tests. Ordering behavior and public API are unchanged.
- **Files modified:** `core/internal.h`, `core/machine.c`, `tests/api/test_api.c`
- **Verification:** Four traversal permutations passed, alongside the exact scheduler and watch-prefix checks.
- **Committed in:** `d96f08d`

**Total deviations:** 3 auto-fixed or test-seam adjustments. No scheduler behavior was tuned to expected values.

## Issues Encountered

The full check preset initially reported four failures: `api.snapshot_refusal`, `api.snapshot_roundtrip`, `memory.map_matrix`, and `memory.map_matrix.self_test`. The MAPCTL pair was fixed and both pass in the final targeted run. The two snapshot cases remain failing and are owned by Plan 06. Field-level comparison proved the timer arrays, pending byte and phase policy match; only `timer_bank.last_event_valid` and `timer_bank.last_event_tick` differ after restore. One roundtrip retained cursor `(valid=1, tick=4110)` instead of saved `(1, 2060)`. A cross-profile refusal control compared destination `(0, 0)` with source `(1, 495)`. CPU state, guest ticks, CPU-ready tick and host observation matched. See [deferred-items.md](./deferred-items.md). The full preset was not rerun after the MAPCTL correction; the final affected set passed 16/16, and the two snapshot failures were reproduced individually.

## User Setup Required

None.

## Next Phase Readiness

Plan 4 is complete and Plan 5 can proceed. Phase 4 final verification remains blocked until Plan 06 implements timer event-cursor snapshot serialization and `api.snapshot_refusal` plus `api.snapshot_roundtrip` pass. No user action is needed; the next executor should carry this blocker into Plan 06 and keep the compatibility and snapshot claims limited to the evidence recorded here.

## Self-Check: PASSED

The summary and deferred-items files exist, and all three plan commits are ancestors of the current `main` HEAD.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*
