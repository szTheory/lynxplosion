---
phase: "04-timers-interrupts-and-first-public-suites"
plan: "05"
subsystem: timing
tags: [CPU, bus ownership, timers, IRQ, deterministic scheduling]
requires:
  - phase: "04-04"
    provides: Shared-tick scheduler, timer IRQ state, and timer-before-CPU ordering
provides:
  - Private instance-owned CPU bus hold and IRQ wake observation seam
  - Normalized CPU priced-cycle readiness and accurate remaining-cost register
  - Permanently registered hold, wake-collision, and FD91 refusal cases
affects: ["04-06 snapshot serialization", "Phase 4 final verification"]
actuals:
  tokens: 8473
  tasks: 2
  commits: 3
commits: 3
plan_head_before: cd20a15e19896491673e4779dcb1b7f28de1fffc
plan_head_after: a128dc7d5358f080458ed26637bceae540570b49
tech-stack:
  added: []
  patterns:
    - Guest time normalizes expired CPU readiness while ownership hold remains separate
    - Timer pending and CPU I-mask acceptance remain independent observations
key-files:
  created:
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-05-red-evidence.json
  modified:
    - core/internal.h
    - core/machine.c
    - tests/api/test_api.c
    - include/lynxplosion.h
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/timers-interrupts.md
key-decisions:
  - "Keep bus ownership hold separate from priced-cycle readiness; when guest time passes cpu_ready_at, normalize it to the current tick and report zero remaining cycle cost."
  - "Derive wake_pending from timer pending independently of CPU I; CPU I continues to control interrupt acceptance at POLL."
  - "Keep FD91 unavailable without active Suzy ownership; the pinned public suite sources do not use CPUSLEEP."
  - "Plan 06 must define snapshot continuation or refusal for bus_hold and wake_pending, alongside timer event cursor serialization."
patterns-established:
  - "Hold a real posted BLL store, verify timer progress and byte-identical CPU state, then release and observe one committed write."
  - "Test timer expiry and CPU grant at the same tick under masked and unmasked I to separate wake from IRQ acceptance."
requirements-completed: []
coverage:
  - id: D1
    description: "A held BLL store remains unchanged during timer IRQ progress and is granted once after release."
    requirement: TIM-04
    verification:
      - kind: integration
        ref: "ctest: api.request_hold"
        status: pass
    human_judgment: false
  - id: D2
    description: "CPU readiness normalizes at expired cycle boundaries; the public remaining-cost byte excludes ownership hold."
    requirement: TIM-01
    verification:
      - kind: integration
        ref: "ctest: api.request_boundary"
        status: pass
    human_judgment: false
  - id: D3
    description: "Timer wake remains visible with CPU I set, while same-tick CPU IRQ acceptance follows the I mask."
    requirement: TIM-03
    verification:
      - kind: integration
        ref: "ctest: api.request_wake"
        status: pass
    human_judgment: false
  - id: D4
    description: "A real guest write to FD91 refuses with the request and hidden RAM unchanged on retry."
    requirement: TIM-04
    verification:
      - kind: integration
        ref: "ctest: api.request_boundary"
        status: pass
    human_judgment: false
duration: 25min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 5: CPU Ownership and Guest Sleep Boundary Summary

**The private bus-hold seam preserves real CPU requests while timers advance, reports only actual priced-cycle delay, and keeps unsupported FD91 sleep behavior explicit.**

## Performance

- **Duration:** 25 min
- **Started:** 2026-11-11T06:28:02Z
- **Completed:** 2026-11-11T06:52:58Z
- **Tasks:** 2
- **Files modified:** 9 implementation, test, documentation, and evidence files

## Accomplishments

- Added instance-owned `bus_hold` and `wake_pending` state with a private validating setter. A held grant preserves the posted request and CPU state while timer events and guest time continue.
- Normalized `cpu_ready_at` whenever guest time advances past the priced-cycle boundary. `LYNX_REG_CPU_STATE` now reports the remaining priced-cycle ticks without unsigned underflow or invented ownership delay.
- Registered `api.request_hold`, `api.request_wake`, and `api.request_boundary` with their exact inventory entries and refreshed the existing claim hash in the same commit.
- Confirmed from Plan 01's source inventory that neither pinned public suite references FD91. The test guest's FD91 write still refuses explicitly; no active Suzy, CPUSLEEP, WAI, or STP behavior is claimed.
- Recorded the actual Task 1 RED report in `04-05-red-evidence.json`. The test executed a real BLL store request, advanced timer 0 to pending while held, and failed because the public remaining-cost byte was `0xdc` at guest tick 95 instead of zero. The evidence classifier returned `RED_EVIDENCE_OK`.

## Task Commits

1. **Task 1 RED: capture held-request readiness failure** — `f3e944e` (`test`).
2. **Task 1 GREEN: separate ownership hold from priced readiness** — `228a8d4` (`feat`).
3. **Task 2: cover wake collision, boundaries, and FD91 refusal** — `a128dc7` (`test`).

The measured plan commit count is 3 from `plan_head_before` through `plan_head_after`.

## Verification

- `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api request_hold` — passed; the tracer feedback re-run also passed before Task 2.
- `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.request_(hold|wake|boundary)$'` — passed, 3/3.
- Targeted scheduler, IRQ, timer, watch, budget, and map regressions — passed, 16/16.
- `ctest --preset check --no-tests=error --output-on-failure -R '^cpu\.tables(\.self_test)?$'` — passed, 2/2; existing CB/DB NOP evidence remains green.
- `sh scripts/hygiene.sh --tree` — passed.
- `node "$HOME/.codex/gsd-core/bin/gsd-tools.cjs" check tdd-red-evidence .planning/phases/04-timers-interrupts-and-first-public-suites/04-05-red-evidence.json --raw` — returned `RED_EVIDENCE_OK` for the intended runtime assertion.

Task 2 expanded already implemented behavior and passed on its first behavioral run; no additional RED was fabricated. Its first compile attempt exposed unregistered local test functions, which were added to the dispatch table before verification.

## Files Created or Modified

- `core/internal.h`, `core/machine.c` — private hold/wake state, validation seam, time normalization, and safe remaining-cycle observation.
- `include/lynxplosion.h` — documented the CPU state remaining-cost byte.
- `tests/api/test_api.c`, `tests/api/tests.cmake`, `tests/inventory/fast.txt` — real held-store tracer, same-tick wake/grant collision, boundary/refusal tests, and exact registered inventory.
- `docs/claims/skeleton-repeatability.claim` — inventory hash matching the registered test list.
- `docs/timers-interrupts.md` — FD91 evidence boundary, scheduler ownership model, readiness semantics, and Plan 06 snapshot follow-up.
- `04-05-red-evidence.json` — classifier-accepted RED report with normalized repository paths.

## Decisions Made

- `cpu_ready_at` remains the end of a priced bus cycle; it does not encode ownership wait time. The separate hold gate prevents grant while readiness is normalized to guest time.
- `wake_pending` follows nonzero timer pending state independently of CPU I. The CPU still accepts IRQ only at its profile's poll boundary when I is clear.
- FD91 remains unavailable because the mapped Mikey byte has no implementation and no active Suzy owns the bus. The pinned suite sources do not require it.
- Snapshot continuation for `bus_hold` and `wake_pending` belongs to Plan 06; this plan does not claim held-request snapshot continuity.

## Deviations from Plan

None. No new dependency, public debug API, firmware, or hardware claim was added.

## Issues Encountered

Phase 4 still has the previously recorded snapshot blocker: `api.snapshot_refusal` and `api.snapshot_roundtrip` compare the complete machine state and fail because `timer_bank.last_event_valid` and `last_event_tick` are not serialized. Plan 06 owns that cursor. This plan also identified that `bus_hold` and `wake_pending` are currently absent from snapshots; Plan 06 must define and test their continuation or explicit refusal. The phase requirements remain pending until their full phase-level evidence is complete.

## User Setup Required

None.

## Next Phase Readiness

Plan 06 can proceed with the tested hold/wake seam. It must address timer event cursor serialization and held-request/wake snapshot continuation before phase-wide snapshot claims can pass. No user action is needed; no named Lynx profile qualification or working guest sleep is claimed.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary, RED evidence, and planned implementation files exist. Task commits
`f3e944e`, `228a8d4`, and `a128dc7` are ancestors of the current HEAD.
