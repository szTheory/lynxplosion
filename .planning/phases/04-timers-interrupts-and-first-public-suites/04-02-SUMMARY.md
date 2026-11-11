---
phase: 04-timers-interrupts-and-first-public-suites
plan: "02"
subsystem: core-timing
tags: [mikey, timers, scheduler, interrupts, ctest, tdd]
requires:
  - phase: 04-timers-interrupts-and-first-public-suites
    provides: pinned-suite tracer and public test harness from plan 01
provides:
  - Instance-owned timer 0 through CPU, bus, mapped inspection, and bounded scheduler
  - Eight standalone timer blocks with shared fixed-zero phase and explicit source/ordering policy
  - Timer tracer, standalone, and phase test modes with exact inventory and claim binding
affects: [04-03 linked timers, 04-04 interrupt/sleep integration, 04-06 snapshots]
actuals:
  tokens: 12644
  tasks: 2
  commits: 5
commits: 5
plan_head_before: 4d2a7c5a2c785cfbe4e5571cd357a7e818e97ea0
plan_head_after: a6c06e542f0803d0c4170e3279f134de55703c89
tech-stack:
  added: []
  patterns: [bounded next-event scan, instance-owned timer bank, mapped pure peek]
key-files:
  created:
    - core/timer.c
    - docs/timers-interrupts.md
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-02-TASK1-RED.json
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-02-TASK2-RED.json
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-02-PHASE-RED.json
  modified:
    - core/internal.h
    - core/machine.c
    - CMakeLists.txt
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
key-decisions:
  - "Use timer-phase-fixed-zero-1 as a deterministic software phase policy; it is not hardware calibration."
  - "Order timer-bank events before a same-tick CPU bus action and its poll/completion with timer-cpu-poll-order-1."
  - "Keep linked timers, CTLB lower observation bits, and timer snapshot continuation explicitly incomplete."
requirements-completed: []
coverage:
  - id: D1
    description: "A strict project-authored BLL guest programs timer 0, observes mapped DONE and pending state, and stores its predicted result."
    verification:
      - kind: integration
        ref: "api.timer_tracer; build/check/tests/lynx-test-api timer_tracer"
        status: pass
    human_judgment: false
  - id: D2
    description: "All eight standalone timer blocks and seven source dividers follow the declared shared-phase policy, with bounded edge and refusal behavior."
    verification:
      - kind: unit
        ref: "api.timer_standalone; api.timer_phase; targeted CTest run"
        status: pass
    human_judgment: false
duration: 25min
completed: 2026-11-10
status: complete
---

# Phase 4 Plan 02: Timers and Bounded Scheduling Summary

**An instance-owned eight-counter timer bank now advances on the machine tick scheduler, with a real timer-0 guest trace and explicit phase, tie-order, and evidence limits.**

## Performance

- **Duration:** 25 minutes
- **Started:** 2026-11-10T23:25:13-05:00
- **Completed:** 2026-11-10T23:50:29-05:00
- **Tasks:** 2
- **Files modified:** 12

## Accomplishments

- Added a bounded `(tick, priority, sequence)` scheduler path for timer events and CPU bus actions. Timer events run before same-tick CPU actions; a held CPU preserves its request while timers progress.
- Added all eight mapped timer register blocks, seven standalone divider periods, backup/reload, one-shot DONE, reset-DONE, pending status, timer-4's UART exception, and pure mapped peeks.
- Added an original strict BLL timer-0 guest tracer and direct standalone/phase coverage. Registered `api.timer_tracer`, `api.timer_standalone`, and `api.timer_phase`, refreshed the exact inventory and claim hash, and documented the current model and limits.

## Task Commits

1. **Task 1 RED: timer guest tracer** — `32fd42e` (`test`)
2. **Task 1 GREEN: timer-0 scheduler path** — `a77a178` (`feat`)
3. **Task 2 RED: standalone timer coverage** — `5bde66b` (`test`)
4. **Task 2 RED: phase and boundary cases** — `acdc144` (`test`)
5. **Task 2 GREEN: eight standalone counters and contract** — `a6c06e5` (`feat`)

The plan metadata commit is recorded in the executor completion report.

## Files Created/Modified

- `core/timer.c` — timer bank initialization, event selection/application, mapped reads/writes, and pending bits.
- `core/internal.h`, `core/machine.c`, `CMakeLists.txt` — instance ownership, bounded scheduler integration, and source registration.
- `tests/api/test_api.c`, `tests/api/tests.cmake`, `tests/inventory/fast.txt` — guest and bank tests with exact permanent test IDs.
- `docs/timers-interrupts.md`, `docs/claims/skeleton-repeatability.claim` — source/policy distinctions, deferred behavior, and inventory-bound claim.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-02-{TASK1-RED,TASK2-RED,PHASE-RED}.json` — semantic RED evidence for the TDD tasks.

## Decisions Made

- Reset, init, and load use a deterministic fixed-zero timer phase (`timer-phase-fixed-zero-1`). This is software policy, not a physical reset-phase measurement.
- Timer-bank work wins a same-tick tie against a CPU bus action; that action's IRQ poll and completion follow (`timer-cpu-poll-order-1`). Timer index supplies stable bank-event ordering.
- A source enable gates creation of a new pending timer latch. Clearing enable leaves an existing pending bit set. This is project policy and is not hardware qualification.
- The timer bank is not represented as snapshot-complete. Snapshot continuation, timer linking, and CTLB last-clock/borrow observations remain future work.

## TDD Gate Compliance

- Task 1 RED was a real guest assertion: the new trace expected the watched timer register access but received the existing unavailable result (`5` instead of `2`). The saved TAP evidence passed the semantic RED classifier.
- Task 2 RED showed timer 1's mapped register block unavailable; the phase test independently showed the mapped timer bank was not yet implemented. Both saved TAP artifacts passed the semantic RED classifier.
- GREEN evidence: `cmake --preset check`; `cmake --build --preset check`; direct `timer_tracer`, `timer_standalone`, and `timer_phase` modes; all 12 selected targeted CTests passed. The three registered timer cases are included in that run.

## Verification

- `ctest --test-dir build/check --output-on-failure -R '^(api\.(advance_budget|advance_slicing|map_matrix|memory_peek_pure|watch_post_write|watch_controls|reset_power_on|bll_tracer|timer_(tracer|standalone|phase))|det\.two_runs|core\.sha256)$'` — 12/12 passed.
- `sh scripts/hygiene.sh --tree` — passed.
- `sh scripts/check-claims.sh --inventory-sha256` — passed; the staged inventory digest matched the claim's `inventory_sha256` exactly.
- `git diff --check` and `git diff --cached --check` — passed.

These checks establish the implemented deterministic software behavior. They do not establish Lynx I/II profile compatibility, physical hardware timing, firmware boot, or a pinned public suite result. The global TIM-01, TIM-02, and TIM-03 requirements remain open while later Phase 4 plans add device integration, links, IRQ evidence, and timer snapshot support.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Keep refused CPU requests state-preserving**
- **Found during:** Task 1 scheduler/refusal regression checks
- **Issue:** Advancing the timer event cursor before a CPU bus action was accepted could mutate machine state on an unavailable access.
- **Fix:** Move cursor marking after the successful bus action while retaining the posted request on a held CPU.
- **Files modified:** `core/machine.c`
- **Verification:** Refusal postcondition and bounded scheduler regressions passed in the targeted CTest run.
- **Committed in:** `a77a178`

**Total deviations:** 1 auto-fixed (Rule 1)
**Impact on plan:** The fix preserves the existing refusal contract and stays within the planned scheduler behavior.

## Known Stubs

| File | Line | Incomplete behavior | Planned follow-up |
|---|---:|---|---|
| `docs/timers-interrupts.md` | 58 | CTLB bits 2–0 are not modeled and read as zero. | Later timer semantics plan. |
| `docs/timers-interrupts.md` | 90 | Timer bank and event cursor are absent from snapshots until Phase 4 Plan 06. | Phase 4 Plan 06. |

## Issues Encountered

The existing pure mapped-memory test expected FD00 to remain unavailable. Its expectations were updated to verify the new timer view while keeping raw backing RAM separate. No changes were needed to the underlying raw-memory API.

## User Setup Required

None. No external configuration or content is required.

## Next Phase Readiness

Plan 03 can build linked timer behavior on the permanent timer bank and scheduler. Plan 06 remains responsible for timer snapshot continuation. No user action is needed before the next authorized Phase 4 plan.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary file was created at the planned path.
- All five task commits are ancestors of the current plan head.
- Build, targeted tests, hygiene, claim binding, and diff checks passed.
