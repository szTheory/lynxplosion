---
phase: 04-timers-interrupts-and-first-public-suites
plan: "08"
subsystem: testing
tags: [cpu, ctest, source-oracle, profile-controls, guest-time]

requires:
  - phase: "04-07"
    provides: pinned suite build, source oracle, and initial CPU Lynx I execution path
provides:
  - Fresh zeroed and seeded CPU suite runs for explicit lynx1 and lynx2 inputs
  - Independent guest probe, source tuple, selected profile, raw result, and time-bound records
  - Permanent model mismatch, count, zero-result, and captured time-bound controls
affects: [phase-04-verification, phase-04-plan-09, phase-04-plan-12]

actuals:
  tokens: 13657.5
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - Fresh guest instances for zeroed and deterministic seeded RAM observations
    - Raw CPU observations stored separately from source-oracle comparisons
    - Captured evidence controls distinguished from actual guest execution controls

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
  - "Detected CPU model requires agreement among the guest RMB0 probe and marker, the source-defined result tuple, and the independently selected profile."
  - "CPU suite results remain raw software-oracle evidence; O1 and hardware qualification stay open, and the overall default remains none."
  - "Zeroed and seeded observations use independent machine instances; no public debug API or production fault flag was added."

patterns-established:
  - "A source comparison never replaces the raw guest result record."
  - "Negative controls require their named model, count, zero-block, or time-bound diagnostic and a clean positive run after each captured-record fault."

requirements-completed: []
commits: 2
plan_head_before: 6ad5222994eead4a815b0c0edc74d82027a3ecf5
plan_head_after: a8d2ca3ca70fd760b7b72cd912b610f369e1516b
duration: 30min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 8: CPU Model Evidence Summary

**The pinned CPU suite records both explicit CPU profiles from fresh zeroed and seeded memory, with independently matched guest probes, source tuples, and bounded completion evidence.**

## Performance

- **Duration:** 30 min
- **Started:** 2026-11-11T08:55:09Z
- **Completed:** 2026-11-11T09:25:13Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Registered `suite.cpu.lynx2` and expanded both profile tests to run from fresh zeroed and deterministic seeded RAM. Each run yielded exactly 18 raw CPU result bytes and a separate source comparison record.
- The observed CPU model is derived from the executed RMB0 probe plus the guest-written model marker, then cross-checked against the pinned source tuple. The selected host profile is an independent comparison input; missing and `none` suite selections are rejected.
- Recorded `lynx1` completion at 26,696 guest ticks (`01` probe, `a1` marker) and `lynx2` at 26,841 ticks (`00` probe, `a2` marker). Zeroed and seeded runs produced identical results per profile. Both are software-oracle matches only; they do not qualify hardware profiles.
- Added real opposite-selection execution coverage and captured-record controls for missing/corrupt probe data, probe/tuple disagreement, metadata and raw count drift, zero/unexecuted results, exact minimum/maximum time boundaries, and integer overflow.
- Added the four CPU test IDs to the exact fast inventory and refreshed its bound claim hash.

## Verification

- `cmake --preset check` — passed.
- `cmake --build --preset check` — passed.
- `ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(cpu\.(lynx1|lynx2|model_controls|bound_controls)|oracle_tracer|oracle_controls|symbol_controls)$'` — 7/7 passed.
- `sh scripts/hygiene.sh --tree` — passed.
- `sh scripts/check-claims.sh --inventory-sha256` — passed; the claim hash matches `tests/inventory/fast.txt`.

## Task Commits

1. **Task 1: Complete cpu/lynx2 and compare both profiles from fresh guest state** — `7f66c14` (`feat(04-08)`).
2. **Task 2: Reject model, count and completion-boundary spoofing with the intended assertions** — `a8d2ca3` (`test(04-08)`).

## Files Created/Modified

- `tests/suites/test_public_suites.c` — fresh-instance RAM setup, independent model derivation, complete records, and captured record validator.
- `tests/suites/run.cmake` — per-test scratch isolation and specific guest/parser control drivers.
- `tests/suites/tests.cmake` — Lynx II and named model/bound control registrations.
- `tests/inventory/fast.txt` and `docs/claims/skeleton-repeatability.claim` — exact test inventory and matching hash.
- `docs/public-suites.md` — source evidence, measured bounds, and limits.

## Decisions Made

- Keep observed model evidence independent from host profile selection and verify it against both the guest probe and the pinned source tuple.
- Keep raw results, source comparisons, completion fields, and hardware-accuracy status separate.
- Apply seeded RAM only through the private test harness; retain the public BLL/guest execution path and untouched upstream assembly.

## Deviations from Plan

### Auto-fixed issue

**1. [Rule 1 - Bug] Isolated the two profile CTests' scratch directories**
- **Found during:** Task 1 focused parallel CTest run.
- **Issue:** Both new profile tests used `mode=cpu` as their scratch directory, so concurrent runs removed or overwrote each other's invocation data.
- **Fix:** Keyed each scratch directory by its unique CTest ID.
- **Files modified:** `tests/suites/tests.cmake`.
- **Verification:** The exact Lynx I/Lynx II test pair passed after the change.

### TDD process classification

Both tasks carry `tdd="true"`, but project `workflow.tdd_mode` is disabled and the existing runner already executed the Lynx II path successfully before the edits. No valid semantic RED was available for the harness and evidence-record changes. I did not fabricate RED evidence or claim TDD compliance; permanent positive and fault controls were added and then verified.

**Total deviations:** 1 auto-fixed test-isolation bug and 1 documented TDD process deviation. No product behavior was changed.

## Issues Encountered

The first parallel profile test run exposed the shared scratch directory described above. After isolating the work directories, the profile tests and affected parser/fixture controls passed.

## User Setup Required

None.

## Next Phase Readiness

The CPU slice supplies bounded software evidence for both execution profiles. O1 remains open, so the records do not establish hardware accuracy, a Lynx baseline, or public profile qualification. Timer-suite work and phase-level requirements EVD-02, EVD-04, EVD-05, and TIM-03 remain open; no global requirement checkboxes were changed. The pre-existing `scripts/check.sh` ShellCheck issue remains assigned to Phase 12; this plan's targeted verification passed. No user action is needed.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary file exists, and task commits `7f66c14` and `a8d2ca3` are ancestors of HEAD.
