---
phase: 04-timers-interrupts-and-first-public-suites
plan: "10"
subsystem: testing
tags: [cpu, timers, completion, no-run, ctest, stale-records]
requires:
  - phase: 04-09
    provides: D-10 pending timer observation gate and immutable two-profile timer captures
provides:
  - True held-before-instruction no-run controls for CPU and timer suites under both profiles
  - Actual guest completion faults and immutable captured-record identity controls
  - Exact required-suite/control selection validation and inventory binding
affects: [04-11, 04-12, timer-suite-evidence]
actuals:
  tokens: 19284
  tasks: 2
  commits: 2
plan_head_before: 19ccde9a960487a78b3c361a12c74a3c9757d9b4
plan_head_after: dfa78843d17370e73476927051d548cfa02fb6af
tech-stack:
  added: []
  patterns: [invocation-owned immutable profile captures, guest completion validation, exact CTest inventory checks]
key-files:
  created: []
  modified:
    - tests/suites/test_public_suites.c
    - tests/suites/run.cmake
    - tests/suites/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/public-suites.md
key-decisions:
  - "Keep D-10 limited to timer index 14 at observed 0C versus unchanged source 0D; global EVD-02 remains open."
  - "Prove same-invocation stale outputs are ignored through fresh-machine execution and the guest watch path; hashes alone cannot distinguish identical deterministic captures."
patterns-established:
  - "Captured parser faults use per-invocation immutable profile baselines, private mutations, positive revalidation and baseline SHA checks."
  - "Actual guest faults remain separately bracketed by fresh positives and must emit their exact refusal before downstream evidence is accepted."
requirements-completed: [EVD-03, EVD-04, EVD-05]
coverage:
  - id: D1
    description: "Both suites are held before their first instruction, advance held guest time, and fail through the normal absent-execution validator."
    requirement: EVD-03
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^suite\\.(cpu\\.(no_run)|timers\\.(no_run))$'"
        status: pass
    human_judgment: false
  - id: D2
    description: "Eight actual guest completion faults cover sentinel omission and bounded guest time for both suites and profiles."
    requirement: EVD-05
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^suite\\.completion_controls$'"
        status: pass
    human_judgment: false
  - id: D3
    description: "Captured records reject zero, missing, stale and cross-profile evidence, and required/control selections are exact and nonempty."
    requirement: EVD-04
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^suite\\.(cpu\\.(model_controls|bound_controls)|timers\\.result_controls|selection_controls)$'"
        status: pass
    human_judgment: false
duration: 46min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 10: No-run and stale completion controls Summary

**CPU and timer suites now prove held no-run refusal, actual guest completion faults, and identity-checked captured-record controls for both profiles.**

## Performance

- **Duration:** 46 min
- **Started:** 2026-11-11 (session start was after the Plan 09 state checkpoint)
- **Completed:** 2026-11-11
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Held each real suite before its first wrapper instruction, advanced guest time through the minimum, proved CPU/request/RAM stayed unchanged with no guest write, and required the normal completion validator's exact absent-execution refusal.
- Added eight actual guest completion-fault cases: omitted post-return sentinel and a 256-tick guest-time limit across CPU/timers and Lynx I/II. Each starts from hash-verified prior positive output files, proves the fresh-machine/watch path clears them and refuses completion, then verifies a fresh positive again.
- Added immutable two-profile CPU capture batching, per-case baseline hash checks, exact zero/progress/sentinel/model/count/time/object/source identity refusals, and copied Lynx II evidence refused in a Lynx I case. Timer copied raw evidence is refused at its earlier `timer raw record identity mismatch` boundary.
- Registered `suite.completion_controls` and `suite.selection_controls`, bound them in the sorted fast inventory, refreshed the claim hash, and documented the control contract.

## Task Commits

1. **Task 1: Hold each real suite before its first instruction and reject completion after minimum time** — `8cfe11e` (`test(04-10): prove public suites reject no-run execution`)
2. **Task 2: Reject missing, stale and zero execution evidence at the correct boundary** — `dfa7884` (`test(04-10): reject stale and incomplete suite evidence`)

The GSD summary/state/roadmap updates are recorded in the final documentation commit for this plan.

## Files Created/Modified

- `tests/suites/test_public_suites.c` — true held no-run setup, shared completion validation, bounded guest controls and artifact identity validation.
- `tests/suites/run.cmake` — invocation-owned fault builds, actual completion controls, immutable data-only capture batches and exact selection cases.
- `tests/suites/tests.cmake` — registered the two new controls.
- `tests/inventory/fast.txt` — added the matching required suite cases.
- `docs/claims/skeleton-repeatability.claim` — refreshed its inventory binding.
- `docs/public-suites.md` — documented no-run, completion, stale-output and parser-control limits.

## Decisions Made

- Preserved D-10: all 16 timer source comparisons remain present, with only index 14 pending as actual `0c` versus unchanged expected `0d`. The strict timer command continues to exit 1 for that exact known mismatch. Global EVD-02, O1/profile qualification and hardware accuracy remain open; default profile remains `none`.
- Same-profile deterministic content hashes cannot establish that a capture is fresh. The actual completion controls instead seed prior valid output files and prove the new machine/watch path clears those files and requires fresh guest completion. Cross-profile copies are rejected by their exact raw or record identity checks.

## Deviations from Plan

### Narrow recipe choice

The fault wrapper is assembled and linked directly in `tests/suites/run.cmake` using the already pinned ca65/ld65 binaries verified against `.lynx-build-files`. No selector was added to `scripts/build-suites.sh`; that keeps the normal recipe and outputs unchanged. The scratch fault is verified to change only the five-byte sentinel store, preserve linked labels and size, and leave pinned upstream `tests.s` unchanged. This implementation choice was approved by the parent executor.

## Issues Encountered

The copied timer profile case was first asserted at the later run-record identity layer, while validation correctly refused earlier at raw-row identity. The test now requires the exact earlier diagnostic, `timer raw record identity mismatch`; the final timer result-control run passes.

## Verification

- `cmake --preset check && cmake --build --preset check` — passed.
- Targeted CTest gate covering both CPU profiles, CPU model/bound/no-run controls, timer result/no-run controls, completion controls and selection controls — **9/9 passed**.
- `sh scripts/check-claims.sh`, `sh scripts/hygiene.sh --tree` and `git diff --check` — passed.
- Plan 1 tracer ran after commit `8cfe11e`; its focused no-run tests passed 2/2. No full phase, Linux or exact-head CI run was attempted here; those remain in Plan 12.

## User Setup Required

None.

## Next Phase Readiness

Plan 11 can proceed. Plan 12 remains responsible for the final local/Linux/exact-head CI qualification. The strict timer source disagreement remains visible and is carried to Phase 5; no user action is needed for this approved D-10 work.

## Self-Check: PASSED

- Summary file exists at the planned path.
- Both task commits are ancestors of the current plan head.
- The existing untracked `.planning/state.json` was not staged or deleted.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*
