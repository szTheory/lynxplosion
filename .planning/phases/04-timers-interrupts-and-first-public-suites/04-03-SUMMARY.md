---
phase: 04-timers-interrupts-and-first-public-suites
plan: "03"
subsystem: timers
tags: [C17, Mikey, timers, CTLB, interrupts, CTest]

# Dependency graph
requires:
  - phase: 04-02
    provides: shared-tick timer bank, mapped guest timer seam, deterministic event scheduler
provides:
  - guest-visible CTLB DONE, borrow, reset-DONE and owned timer-link transitions
  - explicit bounded link graph and refusal behavior for unsupported incoming sources
  - four permanent guest-level timer checks with exact fast-inventory binding
affects: [phase-04, snapshots, timer-verification]

# Actuals (#2632), measured from the realized task diff (chars/4).
actuals:
  tokens: 12623
  tasks: 2
  commits: 3
plan_head_before: 7a16307ad649c2069e85a71d4efebc8a48a3e799
plan_head_after: 5e713424ea1f2c5a760cd08a10116be6e93f1dbc

# Tech tracking
tech-stack:
  added: []
  patterns:
    - guest-programmed register tests observe mapped results and pending state
    - bounded iterative timer-link drain follows a source-labeled transition table

key-files:
  created:
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-03-tdd-red.json
  modified:
    - core/timer.c
    - core/machine.c
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/timers-interrupts.md
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "CTLB bit 1 is modeled as a one-event borrow-in strobe and bit 3 sets or clears DONE; pinned guest behavior is comparison evidence, not hardware qualification."
  - "Only the source-declared 0→2→4 and 1→3→5→7 link edges are enabled; unsupported timer 0, timer 1/audio 3, and timer 6 inputs refuse before mutation."
  - "CTLB low status-bit lifetime remains an explicit software policy until the next timer event because the reviewed primary notes do not define stable read lifetime."

patterns-established:
  - "Separate source observations, deterministic model policy, and inference in the timer transition table."
  - "Test unavailable guest bus actions by asserting the refusal point, consumed prefix, pending request, unchanged action, and retry behavior."

# These are phase-level requirements and remain open pending the remaining Phase 4 work.
requirements-completed: []

coverage:
  - id: D1
    description: "Guest-visible DONE, reset-DONE, borrow and bounded owned timer-link transitions"
    requirement: TIM-02
    verification:
      - kind: integration
        ref: "api.timer_done_borrow, api.timer_link_tracer, api.timer_links"
        status: pass
    human_judgment: false
  - id: D2
    description: "Unsupported timer, audio, Mikey and Suzy guest actions refuse without applying the invalid write"
    requirement: TIM-02
    verification:
      - kind: integration
        ref: "api.timer_unavailable"
        status: pass
    human_judgment: false
  - id: D3
    description: "Timer pending observations remain distinct from enable state and timer-4 UART readiness"
    requirement: TIM-03
    verification:
      - kind: integration
        ref: "api.timer_links, api.timer_phase, api.timer_tracer"
        status: pass
    human_judgment: false

# Metrics
duration: 38min
completed: 2026-11-11
status: complete
---

# Phase 04 Plan 03: CTLB Borrow, DONE and Owned Timer Links Summary

**Guest-written CTLB controls now drive a bounded, source-labeled timer borrow/link model with explicit refusals for unavailable inputs.**

## Performance

- **Duration:** 38 min
- **Started:** 2026-11-11T04:58:06Z
- **Completed:** 2026-11-11T05:36:39Z
- **Tasks:** 2
- **Files modified:** 8 task files, including the TDD RED evidence record

## Accomplishments

- Implemented CTLB DONE set/clear, borrow-in strobe, held reset-DONE, one-shot/reload behavior, and bounded same-tick propagation through the owned link graph.
- Added direct guest cases for the pinned `$0A` borrow result and a real 3→5 linked timer event colliding with a CPU access. The task-1 RED test failed at its intended mapped count and CTLB assertions before the implementation, then passed afterward.
- Expanded permanent guest coverage across every owned edge, both full chains, simultaneous roots, reload/count boundaries, CTLB clear before/on/after expiry, unsupported incoming links, arbitrary unimplemented Mikey/Suzy writes, refusal retries, and atomic mapped-range refusal.
- Refreshed the exact fast-inventory hash binding and documented source facts separately from model policy and inference.

## Task Commits

1. **Task 1 RED: CTLB borrow transition** - `b3a4be9` (`test`)
2. **Task 1 GREEN: CTLB borrow and linked transitions** - `aab4f3c` (`feat`)
3. **Task 2 link and unavailable-boundary coverage** - `5e71342` (`test`)

The plan metadata commit is recorded after the roadmap and state updates.

## Files Created/Modified

- `core/timer.c` - guest CTLB transitions, source-to-link mapping, bounded borrow drain, and refusal preflight.
- `core/machine.c` - records timer event ordering before a same-tick CPU bus action.
- `tests/api/test_api.c` - guest-driven borrow, link, timing-boundary, and unavailable-device checks.
- `tests/api/tests.cmake`, `tests/inventory/fast.txt`, and `docs/claims/skeleton-repeatability.claim` - permanent IDs and exact inventory binding.
- `docs/timers-interrupts.md` - source observations, policy/inference table, behavior, and verification limits.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-03-tdd-red.json` - normalized evidence for the intended failing guest assertion.

## Decisions Made

- The `$0A` CTLB write follows the pinned guest test's expected count `$80→$7F` and DONE `$08` result. This establishes software comparison behavior only.
- Links are limited to the declared `0→2→4` and `1→3→5→7` graphs. Timer 0, timer 1's absent audio-3 predecessor, and timer 6 cannot be enabled as incoming link timers.
- Source facts establish that pending status can be polled independently of its mask. Under D-04, enable gates new latches and any nonzero pending byte drives the modeled IRQ line; the exact CPU sampling point remains scheduler policy.
- Guest tick zero is the fixed divider phase, and timer work precedes a same-tick CPU action. Both are named deterministic model policies, not physical calibration.
- CTLB LAST CLOCK/BORROW IN/BORROW OUT names are source-backed, while their stable read lifetime is not defined by the reviewed primary notes; the current model exposes the latest scheduled input until the next timer event.
- `timers2` remains supporting source evidence only and outside the required suite set. Timer snapshots remain incomplete and are scheduled for later snapshot work.

## Deviations from Plan

### Task classification mismatch

Task 2 is marked `tdd=true`, but its files are test, registration, inventory, and documentation only. Its new guest cases passed against Task 1's committed core behavior on the first run, so there was no behavior gap to demonstrate as RED. The tests were retained, the exact result was recorded, and no failure was manufactured.

### Scope boundary

The plan's `TIM-02` and `TIM-03` requirements remain unchecked globally because they span the rest of Phase 4 and retain unresolved baseline/qualification limits. This plan's guest checks establish bounded software behavior only; they do not close those phase-level requirements.

**Total deviations:** 2 documented scope/classification notes. No unplanned production behavior was added in Task 2.

## Verification

Before and after Task 2, the affected checks passed headlessly:

- `cmake --preset check`
- `cmake --build --preset check`
- `ctest --preset check --no-tests=error --output-on-failure -R '^api\.(timer_(done_borrow|link_tracer|links|unavailable)|timer_(phase|standalone|tracer))$'` — all 7 selected tests passed.
- `sh scripts/check-claims.sh`
- `sh scripts/hygiene.sh --tree`
- `git diff --check`

The initial RED ran the named guest test to its watch point and failed only on the expected mapped assertions: count `128 != 127` and CTLB `0 != 8`. Build and fixture setup succeeded. After implementation, the tracer cases and final affected regression set passed.

## Issues Encountered

None. The test-only task classification mismatch is documented above.

## User Setup Required

None - no external service configuration is required.

## Next Phase Readiness

Plan 04-04 may proceed. `TIM-02`/`TIM-03` remain phase-level open; baseline decision O1 remains unresolved, CTLB status lifetime is still model policy, no physical hardware or named-profile qualification was performed, and timer state is not yet included in snapshots. No firmware, game image, or copyrighted fixture was used.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*

## Self-Check: PASSED

- Summary file exists at the required phase path.
- Task commits `b3a4be9`, `aab4f3c`, and `5e71342` are ancestors of the current plan head.
- `git diff --check` passed.
