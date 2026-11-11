---
phase: 04-timers-interrupts-and-first-public-suites
plan: "11"
subsystem: testing
tags: [mikey, timers, irq, ctest, isolated-mutants, api]
requires:
  - phase: 04-10
    provides: [D-10 pending timer observation gate, fresh suite completion controls]
provides:
  - Five isolated semantic timer/scheduler/IRQ faults caught by exact API assertions
  - Per-CTest source/build identities and clean API positives before and after each mutant
  - Exact timer fault inventory and claim binding
affects: [phase-05-timer-agreement, phase-05-device-fault-evidence, phase-06-device-fault-evidence]
actuals:
  tokens: 7002
  tasks: 2
  commits: 2
commits: 2
plan_head_before: de7c3a9a50cb111fc67b91931c248cfe56b52162
plan_head_after: 86a8d038e9f45fa92b7fa2c3973030f0aea6c48d
tech-stack:
  added: []
  patterns:
    - Per-CTest source copies with unique anchors, owned scratch, source/build hashes, and exact assertion matching
key-files:
  created:
    - tests/timers/fault_controls.cmake
  modified:
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/timers-interrupts.md
    - docs/claims/skeleton-repeatability.claim
key-decisions:
  - Five independent semantic faults use only copied source variants; production core has no test fault flags.
  - D-10 keeps timer result offset 14 pending at expected 0D versus observed 0C; global EVD-02 remains open for Phase 5.
  - EVD-05 records this Phase 4 slice only and remains open across Phases 5 and 6.
requirements-completed: [TIM-01, TIM-02, TIM-03]
requirement-slices:
  EVD-05: "Phase 4 timer/IRQ slice complete; recurring device-fault evidence remains open in Phases 5 and 6."
  EVD-02: "Phase 4 observation/control slice complete; exact index-14 source agreement remains pending in Phase 5."
coverage:
  - id: D1
    description: "Five built semantic mutants fail their exact independent API assertion and pass a clean API case before and after."
    requirement: EVD-05
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^timer\\.fault\\.(done|irq|order|phase|reset_done)$'"
        status: pass
    human_judgment: false
  - id: D2
    description: "CPU full-source, timer observation and strict-mismatch controls remain green around the mutant batches."
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^(suite\\.cpu\\.lynx1|suite\\.cpu\\.lynx2|suite\\.timers\\.observation\\.lynx1|suite\\.timers\\.observation\\.lynx2|suite\\.timers\\.strict_comparison_controls)$'"
        status: pass
    human_judgment: false
duration: 30min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 11: Timer, order and IRQ fault sensitivity Summary

**Five isolated timer, scheduler and IRQ mutants now fail their exact independent API assertions while clean timer/CPU observations preserve the approved D-10 boundary.**

## Performance

- **Duration:** 30 min (30 min 17 sec elapsed)
- **Started:** 2026-11-11T16:46:21Z
- **Completed:** 2026-11-11T17:16:38Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Added two DONE controls and three phase/order/IRQ controls. Each uses a single unique source anchor in a per-CTest source copy, then requires configure, compile and link to succeed before the API case can prove detection.
- Every mutant exits 1 at its exact named assertion. Its clean API case passes before and after the mutant. Timeout, sanitizer output, build failure, a different assertion, or zero test cases cannot count as detection.
- Registered `timer.fault.done`, `timer.fault.reset_done`, `timer.fault.phase`, `timer.fault.order`, and `timer.fault.irq` in the fast inventory and refreshed the claim hash in the same task commits.
- The full committed five-control CTest selection passed 5/5. The CPU full-source tests, both timer observation tests, and strict-mismatch controls passed 5/5 after the final mutant run.
- Kept all 16 timer comparisons and the unchanged expected source row. Both timer observations remain 15/16; index 14 is expected `0D`, observed `0C`, and pending under D-10.

## Fault → Assertion Inventory

| CTest ID | Fault applied to copied source | Required API assertion | Result |
| --- | --- | --- | --- |
| `timer.fault.done` | Remove DONE gating from timer advancement | `timer.done-gates-counting` | Built; exit 1 at intended assertion; clean API passed before/after. |
| `timer.fault.reset_done` | Remove held RESET-DONE behavior during timer events | `timer.reset-done-held-clear` | Built; exit 1 at intended assertion; clean API passed before/after. |
| `timer.fault.phase` | Replace shared divider phase with zero | `timer.phase.shared-divider-edge` | Built; exit 1 at intended assertion; clean API passed before/after. |
| `timer.fault.order` | Reverse same-tick CPU/timer priority | `timer.order.timer-before-cpu-prefix` | Built; exit 1 at intended assertion; clean API passed before/after. |
| `timer.fault.irq` | Gate the latched pending IRQ line on the current timer enable | `timer.irq.pending-independent-of-enable` | Built; exit 1 at intended assertion; clean API passed before/after. |

Per-fault clean/mutated source hashes, API executable hashes and configure/build elapsed bounds are recorded in [timers-interrupts.md](../../../docs/timers-interrupts.md#plan-04-11-task-1-done-fault-sensitivity). The driver removes its ownership-checked scratch directory after every invocation. Live `core/timer.c` and `core/machine.c` hashes stayed at their clean values, and the default core library/API test executable hashes were identical before and after the three Task 2 mutant runs.

## Task Commits

1. **Task 1: Remove DONE gating and pulse reset-DONE in isolated builds and catch each fault** — `d876857` (`test(04-11): prove DONE and reset-DONE fault sensitivity`)
2. **Task 2: Catch independent shared-phase, same-tick order and pending-mask faults** — `86a8d03` (`test(04-11): prove phase order and IRQ fault sensitivity`)

The final documentation/state/roadmap commit is recorded by the executor after its self-check and state updates.

## Verification

- `cmake --preset check` and `cmake --build --preset check` — passed before the task checks and after the task commits.
- `ctest --preset check --no-tests=error --output-on-failure -R '^timer\\.fault\\.(done|irq|order|phase|reset_done)$'` — passed 5/5 after both tasks.
- CPU full-source ×2, timer observation ×2, and strict-mismatch control — passed 5/5 before/after each task batch; final post-mutant run passed in 2.69 s.
- `sh scripts/check-claims.sh`, `sh scripts/hygiene.sh --tree`, and `git diff --check` — passed.
- No firmware, commercial ROM, physical hardware, full 2 GB vector lane, Linux run, or CI run was used. Plan 12 owns final local/Linux/exact-head qualification.

## Deviations and Issues

- The first detached shell build wrapper exited before producing its log or exit marker. Its recorded PID was already gone and the log was empty; the managed headless `exec_command` session then built the clean API target successfully. No duplicate build remained active.
- The first broad clean selection also ran `suite.timers.pending_controls`, a 55-fault negative matrix that took 44.16 s (the selection took 49.96 s total). After parent review identified that matrix as fault evidence rather than a clean positive, it was not repeated per mutant. Later positive brackets ran the five CPU/full-source, timer observation and strict-mismatch controls only (2.28 s and 2.05 s around Task 2; 2.69 s in final verification). No intended fault or positive observation was dropped.
- EVD-05's Phase 4 slice is recorded here, but the recurring global requirement remains open for Phases 5 and 6. Global EVD-02 remains open for the exact index-14 agreement in Phase 5. No source oracle, timer phase, bus cost, profile, or wrapper layout was changed to manufacture agreement.

## User Setup Required

None.

## Next Plan Readiness

Plan 12 is next and owns final Phase 4 verification, review, and local/Linux/exact-head CI evidence. EVD-02 and EVD-05 remain open as described above; no user action is needed to continue Plan 12.

## Self-Check: PASSED

- Summary file exists at the planned path.
- Both task commits are ancestors of the current plan head.
- The pre-existing untracked `.planning/state.json` remains unstaged and untouched by Git operations.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*
