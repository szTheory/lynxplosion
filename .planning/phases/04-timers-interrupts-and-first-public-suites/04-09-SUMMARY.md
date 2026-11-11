---
phase: 04-timers-interrupts-and-first-public-suites
plan: 09
subsystem: testing
tags: [timers, lynx-tests, CMake, CTest, source-oracle]

requires:
  - phase: 04
    provides: Timer scheduler, interrupt behavior, linked guest-suite harness and source-generated oracle
provides:
  - Fresh Lynx1 and Lynx2 timer observations with independent guest model evidence and a linked 64-phase cascade proof
  - Strict 16-row comparison retained with only D-10 index14 pending, plus named model, result, descriptor and companion controls
affects: [04-10, 04-11, 04-12, phase-05-timer-source-agreement]

actuals:
  tokens: 36693
  tasks: 2
  commits: 2
commits: 2
plan_head_before: 4139498581c7aba5813b56f165cfbb51cbcd1e6c
plan_head_after: 42bf6e5639478a766380302abb2af85163eda03e

tech-stack:
  added: []
  patterns:
    - Immutable two-profile observation baselines with isolated per-fault data copies
    - Source-bound timer comparison descriptor and independently derived linked-code proof

key-files:
  created:
    - docs/timer-source-index14-pending.json
    - fixtures/suites/timers-wrapper.s
  modified:
    - tests/suites/test_public_suites.c
    - tests/suites/run.cmake
    - tests/suites/tests.cmake
    - tests/inventory/fast.txt
    - docs/public-suites.md
    - docs/claims/skeleton-repeatability.claim
    - fixtures/manifests/timer-tests.manifest
    - scripts/build-suites.sh
    - scripts/generate-suite-oracle.sh
    - tests/diag/test_diag.c
    - tests/diag/tests.cmake

key-decisions:
  - "Keep the owner-approved D-10 comparison pending at source index14: raw 0c versus unchanged source 0d on both profiles; global EVD-02 and O1 remain open."
  - "Derive the 12-count result from linked Test6 instruction paths and all 64 first-edge delays; this is software-model evidence, not hardware qualification."
  - "For data-only controls, capture fresh two-profile observations once per CTest invocation, then validate immutable baselines around private-copy faults; execution/model faults still use fresh guest runs."

patterns-established:
  - "Every timer observation retains raw values separately from the source comparison and labels profile qualification unresolved."
  - "Captured comparator alternatives are labelled as corpus mutations, not guest executions."

requirements-completed: []
coverage:
  - id: D1
    description: "Both timer profiles complete the unchanged guest suite with 16 retained results, 15 source agreements, the exact pending row, and a linked-code 64-phase count proof."
    verification:
      - kind: integration
        ref: "suite.timers.observation.lynx1, suite.timers.observation.lynx2, suite.timers.cascade_model"
        status: pass
      - kind: integration
        ref: "suite.timers.strict_comparison_controls"
        status: pass
    human_judgment: false
  - id: D2
    description: "Timer model, alternatives, result shape, completion metadata, descriptor, companion and proof faults reach their named refusals while the approved pending comparison remains bounded."
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(model_controls|result_controls|pending_controls)$'"
        status: pass
    human_judgment: false

duration: 74 min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 9: Timer observations and pending comparison controls Summary

**The pinned timer guest now produces identity-bound Lynx1/Lynx2 observations, an independent 64-phase count proof, and fail-closed controls around the one D-10 source mismatch.**

## Performance

- **Duration:** about 74 minutes
- **Started:** 2026-11-11 09:32:23 EST
- **Completed:** 2026-11-11 10:46 EST
- **Tasks:** 2
- **Files modified:** 15

## Accomplishments

- Ran the unchanged pinned timer suite on fresh Lynx1 and Lynx2 instances. Each observation retains all 16 meaningful bytes in the 18-byte extent, with 15 source agreements and only index14 pending (`0c` observed, `0d` expected). The strict comparison still exits 1 with its exact mismatch diagnostic; no oracle, timing, phase, layout or initial-counter change was made.
- Added an executed A1/A2 guest model probe and source-bound timer records. The linked Test6 verifier checks setup bytes, opcodes, operands, addressing modes, branch targets and page geometry, derives 14/24-cycle paths and 70/120-tick intervals, and enumerates 12 acknowledgments for each of 64 first-edge delays. This matches the observed count under the declared software policy and does not establish hardware accuracy or profile qualification.
- Added exact model, accepted-alternative, result-shape, record, pending-descriptor, companion, stale-identity and proof controls. Controls require named exit-1 diagnostics and preserve strict source evidence.
- Preserved the seven existing timer integration edits and repaired the build-control fixture copy and diagnostic model-prefix checks. The five related build/oracle/symbol integration checks and the targeted ShellCheck selection passed.

## Task Commits

1. **Task 1: Qualify complete timer observations and the exact pending comparison on both profiles** - `92d167e` (`feat`)
2. **Task 2: Prove timer model, extent and alternative-set controls** - `42bf6e5` (`test`)

## Timer Control Inventory

| CTest | Fault cases | Coverage |
| --- | ---: | --- |
| `suite.timers.model_controls` | 1 actual guest fault | Lynx1 guest with the opposite Lynx2 expectation; fresh Lynx1/Lynx2 observations bracket the run. |
| `suite.timers.result_controls` | 48 captured-data cases | 9 comparator cases (all 5 accepted alternative members and 4 adjacent outside-set rejections); 26 record-field cases; 3 time cases; 1 all-zero result block; 2 index14 replacements (`0b`, `0d`); 1 second mismatch; 6 raw/comparison shape or summary faults. |
| `suite.timers.pending_controls` | 55 captured-data cases | 33 descriptor schema/type/value/identity cases; 20 companion schema/type/value/hash/self-reference cases; 2 missing/corrupt proof cases. |

The 103 captured-data cases use private copies. At the start of each control-test invocation, fresh observations, proof and companions are captured for both profiles. Both immutable baseline records are positively validated and their listed SHA-256 identities checked before and after every individual mutation. Each data-control batch ends with fresh profile observations. The alternative-set cases validate a separately labelled captured comparator corpus; they do not represent new guest runs. The combined Task 2 gate passed all three controls.

The first unbatched result-control run took 188.20 seconds because it repeated guest execution for each data mutation. After the approved batching adjustment, the same control passed in 61.49 seconds. The pending matrix passed in 37.73 seconds; the combined CTest wall time was 30.88 seconds with the configured parallel test preset.

## Decisions Made

- D-10 remains a single explicitly pending software-comparison row. All other 15 source comparisons must agree, and exact source agreement remains a Phase 5 item. The overall profile remains `none`; O1, global EVD-02, hardware accuracy, firmware, rights, publication and release conclusions are unchanged.
- The count proof is grounded in pinned source and actual linked bytes under the current fixed-zero/flat5/no-DMA policy. It does not calibrate timing or imply that startup, display DMA or counter preconditions match physical hardware.

## Deviations from Plan

### Approved verification-efficiency adjustment

For captured data/parser faults, each CTest invocation captures actual observations for both profiles once, preserves immutable hash-checked baselines, and validates them positively before and after every private-copy fault. Actual wrong-profile execution retains fresh guest observations before and after. This removed repeated unchanged guest setup without reducing the individual mutation cases or changing D-10 acceptance.

### Auto-fixed harness fixture issue

The descriptor duplicate-key control originally appended an extra `version` field, so the parser correctly reported an unknown key before reaching duplicate-key handling. The fixture now duplicates a declared `schema` key directly and verifies the exact duplicate-key diagnostic. The pending-control matrix passed afterward.

**Total deviations:** 1 approved verification-efficiency adjustment and 1 harness fixture correction. No production timer behavior or source expectation changed.

## Issues Encountered

- A removed final raw row reached the parser's exact `timer raw record shape mismatch` diagnostic rather than a generic count/byte error. The control now expects that named shape diagnostic; the result-control CTest passes.
- The initial duplicate-key fixture masked its intended branch as described above; corrected and rerun successfully.

## User Setup Required

None. All work uses the existing pinned source and local headless harness.

## Next Phase Readiness

Plan 9 is complete and Plan 10 can proceed. The strict index14 mismatch is deliberately still failing against source, both profiles remain unqualified, and Phase 5 must resolve the original startup, counter and bus/video preconditions before any broader timer agreement claim. No user action is needed for this handoff.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary file exists; Task 1 and Task 2 commits are ancestors of HEAD; and `plan_head_after` matches the measured Task 2 commit.
