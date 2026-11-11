---
phase: "04-timers-interrupts-and-first-public-suites"
plan: "06"
subsystem: testing
tags: [mikey-timers, snapshots, deterministic-tests, refusal-validation]
requires:
  - phase: "04-05"
    provides: [CPU request ownership, normalized readiness, held request behavior]
provides:
  - Format 4 timer, IRQ, hold, and current-tick event continuation
  - Atomic validation controls and common-tick timer path comparisons
  - Documented format fields, readiness bounds, observer policy, and golden decision
affects: [phase-04-timers, snapshots, diagnostics, determinism]
actuals:
  tokens: 16214
  tasks: 2
  commits: 3
tech-stack:
  added: []
  patterns: [serialize canonical state only, validate before apply, compare independent common-tick observations]
key-files:
  created:
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-06-task1-red-evidence.json
  modified:
    - core/internal.h
    - core/machine.c
    - core/snapshot.c
    - include/lynxplosion.h
    - tests/api/test_api.c
    - tests/det/test_det.c
    - tests/diag/test_diag.c
    - docs/timers-interrupts.md
key-decisions:
  - "Persist only timer records, pending IRQ, phase policy, the current-tick event-applied bit, and bus hold; derive wake, request, divider phase, and future candidates."
  - "Normalize historical event timestamps away as guest time advances; fixed timer ordinals 0–7 leave no dynamic sequence counter or exhaustion state."
  - "Keep tests/golden/skeleton.digest unchanged because the existing golden and sensitivity controls pass and the digest excludes snapshot bytes."
patterns-established:
  - "Snapshot restore validates the complete candidate before any machine write and clears host watch state only after successful apply."
  - "Compare RAM, mapped timer/status, CPU registers, request, time, and canonical snapshots independently across scheduler paths."
requirements-completed: []
metrics:
  duration: "52m"
  completed: "2026-11-11"
status: complete
commits: 3
plan_head_before: 4efd1ce9829526a563bed9df6aa1bc178852fde3
plan_head_after: 35fab3be1adec9de0e3bf200203bfdbf7c6f9544
---

# Phase 4 Plan 6: Timer and Snapshot Continuation Summary

**Format 4 restores active timer and held-request state with reachable validation, and coarse, sliced, watched, and restored runs agree at common ticks.**

## Performance

- **Duration:** about 52 minutes
- **Started:** 2026-11-11T07:02:54Z
- **Completed:** 2026-11-11T07:54:51Z
- **Tasks:** 2/2
- **Implementation files modified:** 14, including the Task 1 RED evidence record

## Accomplishments

- Added the version 4 snapshot payload for canonical timer records, pending IRQ, phase policy, the current-tick event-applied bit, and bus hold. Restore checks the whole candidate before mutation, recomputes wake and CPU request state, and clears watches only after success.
- Replaced the inherited `ready_at` allowance of 255 with reachable flat5/test_skew residuals of 4/13 ticks. Controls retain positive in-flight and legal time-headroom states while rejecting over-bound, stale, and hostile rehashed values.
- Added `api.timer_snapshot_controls` and `det.timer_paths`. Both CPU profiles compare coarse, one-tick, irregular, watched, and restored continuations using raw RAM, mapped timer/status, registers, request, guest time, and serialized guest state.
- Updated API and diagnostic format assumptions, documented the canonical fields and host-watch contract, and refreshed the exact test inventory claim. No golden digest edit was made.

## Task Commits

1. **Task 1 RED: live timer snapshot continuation test** — `f1e3471` (`test`)
2. **Task 1 GREEN: validated timer and hold serialization** — `6126a5d` (`feat`)
3. **Task 2: timer snapshot continuation paths** — `35fab3b` (`test`)

The plan ledger measured three task commits from `4efd1ce9829526a563bed9df6aa1bc178852fde3` through `35fab3be1adec9de0e3bf200203bfdbf7c6f9544`.

## Verification Evidence

| Check | Result |
|---|---|
| `cmake --preset check && cmake --build --preset check` | Passed before the final consumer-assertion fix; affected API/diagnostic/determinism targets rebuilt successfully afterward. |
| Task 1 direct API modes: `timer_snapshot`, `snapshot_refusal`, `snapshot_roundtrip` | Passed; the RED failure evidence is preserved in the created JSON record. |
| `build/check/tests/lynx-test-api timer_snapshot_controls` | Passed on lynx1 and lynx2. |
| `build/check/tests/lynx-test-det timer_paths` | Passed on lynx1 and lynx2 at common tick 257; watched grant 80 observed pending IRQ `0x07` after the linked cascade. |
| `ctest --preset check --no-tests=error --output-on-failure -R '^det\.golden(\.self_test)?$'` | 2/2 passed; existing golden digest stayed unchanged. |
| First full CTest run | 136/140 passed. Four failures were stale format-3 consumer assertions: `api.bll_identity`, `diag.result_controls`, `det.diagnostic_restore`, and `det.diagnostic_restore.self_test`. |
| Targeted retest after consumer fixes | 9/9 passed, including all four failed IDs and snapshot refusal, roundtrip, timer snapshot, controls, and determinism path tests. |
| `sh scripts/hygiene.sh --tree`; `sh scripts/check-claims.sh`; `git diff --check` | Passed. |
| `sh scripts/check.sh` | Stopped at shellcheck before its workflow/CTest step: installed ShellCheck 0.11.0 reports SC1007 at five pre-existing `CDPATH= cd` assignments in unchanged `scripts/build-suites.sh`. No successful canonical full-gate claim is made; Phase 12 owns that correction. |

## Files Created/Modified

- `core/snapshot.c`, `core/internal.h`, `core/machine.c` — format 4 serialization, full candidate validation, and normalization of historical event cursor timestamps.
- `include/lynxplosion.h`, `tests/api/test_api.c`, `tests/diag/test_diag.c` — public format description and updated API/diagnostic consumer checks.
- `tests/det/test_det.c`, `tests/api/tests.cmake`, `tests/det/tests.cmake`, `tests/inventory/fast.txt`, `docs/claims/skeleton-repeatability.claim` — common-tick modes and exact permanent registration.
- `docs/memory-startup.md`, `docs/timers-interrupts.md` — snapshot and readiness contract.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-06-task1-red-evidence.json` — exact intended RED evidence for Task 1.

## Decisions Made

- Serialize the current-tick event-applied bit because an unavailable CPU request can stop after timer work at that same tick; reconstruct its timestamp from guest time and normalize it away once time advances.
- Do not serialize wake, CPU request, divider phase, deadline queue, or historical cursor timestamp. Wake follows pending IRQ; request and candidates are recomputed; fixed timer ordinals are bounded 0–7 with no sequence counter.
- Keep the existing golden digest. `det.golden` and its sensitivity control pass; the snapshot representation is not part of that digest.
- Leave TIM-01 through TIM-04 globally open because they span later Phase 4 suite and qualification plans. No `requirements.mark-complete` update was made.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Canonical cursor normalization]**
- **Found during:** Task 1
- **Issue:** `last_event_tick` was derivable history but the runtime retained old timestamps after guest time moved beyond the event.
- **Fix:** Normalize the cursor in `core/machine.c`; snapshots store only whether the current tick's timer event was already applied.
- **Verification:** Same-tick unavailable-refusal continuation and repeated-request tests pass; snapshots no longer depend on stale cursor history.
- **Committed in:** `6126a5d`

**2. [Rule 1 - Stale format consumers]**
- **Found during:** Task 2 full CTest
- **Issue:** API and diagnostic tests expected version 3 and a 65,568-byte payload after the format advanced.
- **Fix:** Updated the API version expectation and diagnostic payload size/version checks to format 4 (65,604-byte payload).
- **Files modified:** `tests/api/test_api.c`, `tests/diag/test_diag.c`
- **Verification:** All four initially failing IDs passed in the 9/9 targeted retest.
- **Committed in:** `35fab3b`

**3. [Rule 3 - Inventory ordering]**
- **Found during:** Task 1 registration
- **Issue:** CMake rejected the first inventory update because the new test row was not in exact lexical order.
- **Fix:** Sorted the registered test row and refreshed the claim hash.
- **Verification:** CMake configure and `scripts/check-claims.sh` passed.
- **Committed in:** `f1e3471` and `6126a5d` (inventory/claim updates were atomic with each task's registrations).

**Total deviations:** 3 auto-fixed. The changes remained within snapshot correctness, consumer compatibility, and required test wiring.

## Issues Encountered

- The first `timer_snapshot_controls` assertion guessed return tick 81. Deriving the guest schedule gives SEI 2 cycles, LDA 2, two absolute stores 4 each, and LDA 2; the CTLA write is granted at tick 85 and the watch returns at tick 86. The test now asserts that derived value and passes.
- The initial full CTest run exposed the four stale consumer assumptions listed above. They were fixed and all four passed in the targeted retest; the remaining full local qualification is reserved for Phase 12.
- The canonical check reached ShellCheck and stopped on SC1007 in the unchanged suite-build script. This is recorded for Phase 12 in `deferred-items.md` and remains an open blocker to claiming the canonical full gate green.

## User Setup Required

None.

## Next Phase Readiness

Plan 06 is complete and committed. Later Phase 4 plans remain responsible for suite evidence and final qualification; Phase 12 owns the narrow ShellCheck 0.11.0 cleanup and successful canonical check. Project baseline and hardware-accuracy flags remain unresolved. No user action is needed for this plan.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary exists, and commits `f1e3471`, `6126a5d`, and `35fab3b` are ancestors of HEAD.
