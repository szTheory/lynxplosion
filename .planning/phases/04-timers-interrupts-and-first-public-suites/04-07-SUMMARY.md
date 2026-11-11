---
phase: "04-timers-interrupts-and-first-public-suites"
plan: "07"
subsystem: testing
tags: [6502, ld65, public-bll, source-oracle, ctest]
requires:
  - phase: "04-06"
    provides: [deterministic timer and CPU scheduling, bounded public BLL loading, guest watches]
provides:
  - Pinned-source CPU oracle generation and a completed bounded cpu/lynx1 guest tracer
  - Strict source, oracle, linked-symbol and integer-time negative controls
  - Public suite schema, bounds, provenance and evidence-limit documentation
affects: [phase-04-suites, test-inventory, evidence-claims]
actuals:
  tokens: 20153
  tasks: 2
  commits: 2
tech-stack:
  added: []
  patterns: [source-derived accepted-byte sets, validate symbol extents before RAM reads, invocation-owned suite scratch]
key-files:
  created:
    - scripts/generate-suite-oracle.sh
    - tests/suites/test_public_suites.c
    - tests/suites/tests.cmake
    - tests/suites/run.cmake
    - docs/public-suites.md
  modified:
    - scripts/build-suites.sh
    - scripts/check-manifests.sh
    - fixtures/suites/cpu-wrapper.s
    - fixtures/manifests/cpu-tests.manifest
    - tests/diag/test_diag.c
    - tests/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
key-decisions:
  - "Treat source-oracle agreement as raw software evidence; leave O1 and named-profile qualification unresolved."
  - "Derive a 256 guest-tick minimum from the wrapper's full zero-page clear and keep a finite 50,000,000 tick maximum."
  - "Determine the guest model from the instruction-written probe byte; host profile selection is an independent comparison input."
patterns-established:
  - "Fresh pinned source identity, build record, actual ld65 labels, guest execution and source comparison remain one fail-closed evidence chain."
  - "Negative controls require the intended parser diagnostic and exact nonzero status, then remove their invocation-owned scratch."
requirements-completed: []
metrics:
  duration: "about 20 minutes"
  completed: "2026-11-11"
status: complete
commits: 2
plan_head_before: 2b8d7e622d5c5a8620d39e1aafc6823a9d09dfe9
plan_head_after: 03a7c621bea224eec0a98b970a47cd7374969956
---

# Phase 4 Plan 7: Source-Bound Public CPU Suite Summary

**The pinned CPU source oracle now drives a completed public-API cpu/lynx1 run, with fail-closed source, oracle, symbol, and guest-time controls.**

## Performance

- **Duration:** about 20 minutes
- **Tasks:** 2/2
- **Implementation files changed:** 13
- **Task commits measured from plan ledger:** 2

## Accomplishments

- Generated a strict TSV oracle from the verified `lynx-tests` source and per-file identities. CPU has 18 meaningful expectations; timers retain 16 meaningful values in an 18-byte extent, including accepted alternatives.
- Completed the real cpu/lynx1 suite through public BLL loading and actual linked symbols. It reached its post-return sentinel at 26,696 guest ticks, reported progress `8`, probe byte `01` and guest model marker `a1`, and matched all 18 untouched CPU source expectations.
- Added exact-diagnostic controls for malformed and stale oracle records, source count/shape/token/extent changes, malformed/missing/duplicate/overlapping labels, extent overflow, and guest tick lower/upper/overflow boundaries.
- Bound `suite.cpu.lynx1`, `suite.oracle_tracer`, `suite.oracle_controls`, and `suite.symbol_controls` to the exact test inventory and refreshed the claim hash. Added the source schema, time-bound rationale, run instructions and evidence limits to `docs/public-suites.md`.

## Task Commits

1. **Task 1: Run cpu/lynx1 with source-generated expectations and independent completion/model evidence** — `0bd4ef6` (`feat`)
2. **Task 2: Reject malformed source, symbols and oracle shapes before observing guest results** — `03a7c62` (`test`)

The plan ledger records two task commits from `2b8d7e622d5c5a8620d39e1aafc6823a9d09dfe9` through `03a7c621bea224eec0a98b970a47cd7374969956`.

## Verification Evidence

| Check | Result |
|---|---|
| `cmake --preset check && cmake --build --preset check` | Passed. |
| Task 1 `ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(oracle_tracer|cpu\.lynx1)$'` | 2/2 passed; the fresh CPU tracer completed and matched 18/18 source expectations. |
| Task 2 `ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(oracle_tracer|oracle_controls|symbol_controls|cpu\.lynx1)$'` | 4/4 passed, including both positive runs and exact-failure controls. |
| `sh scripts/hygiene.sh --tree` | Passed. |
| `sh scripts/check-manifests.sh` | Passed; 9 manifests covered 18 fixture files. |
| `sh scripts/check-claims.sh` | Passed with inventory SHA-256 `f06a2ded61bb685f33fcd324aa09cb801c9305b0365c90db3a9765a40740dca1`. |
| `git diff --check` | Passed before both task commits and across the plan diff. |

The canonical full `scripts/check.sh` was not run. Its existing ShellCheck 0.11 SC1007 findings in `scripts/build-suites.sh` are already recorded for Phase 12. No successful full-gate claim is made here.

## Decisions Made

- The first CPU run is evidence of agreement with the pinned source oracle only. O1 remains open; this does not establish physical hardware accuracy or qualify a named CPU profile.
- The 256-tick floor is a conservative software bound from the wrapper's unavoidable 256-byte zero-page clear. The observed 26,696 ticks are measured guest time; no host clock supplies the bound.
- The source-level RMB0 probe writes the model observation byte inside the guest. The selected profile is checked independently against that observation and source tuple.
- EVD-02 and EVD-04 remain globally open because this plan does not complete the timer suite or settle the guest-model taxonomy. No requirements were marked globally complete.

## Deviations from Plan

- The plan's tasks carry `tdd="true"`, while the project's loaded GSD configuration has `workflow.tdd_mode=false`. No honest pre-change product RED was available for this harness-and-fixture slice, so no retroactive or fabricated RED evidence/commit was created. Positive completion and exact parser-failure controls were verified as planned.

## Issues Encountered

- The first symbol-control run assumed ld65 printed `_g_results` without a leading dot. Its actual row is `al HEX ._g_results`; the control matcher was corrected to accept the documented optional leading dot, and the final four-test run passed.

## User Setup Required

None.

## Next Phase Readiness

The source-bound CPU tracer and its parser controls are ready for later Phase 4 work. Timer-suite execution, O1, O5/O6 evidence, the guest-model taxonomy, and the Phase 12 canonical full-check gate remain unresolved. No user action is needed for this plan.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary exists, and task commits `0bd4ef6` and `03a7c62` are ancestors of HEAD.
