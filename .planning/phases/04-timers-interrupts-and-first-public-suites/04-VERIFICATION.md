---
phase: 04-timers-interrupts-and-first-public-suites
verified: 2026-11-11T22:51:41Z
status: passed
score: 49/49 must-haves verified
covered_files:
  - .planning/debug/evidence/timer-suite-cascade-count.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-01-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-01-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-02-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-02-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-03-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-03-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-04-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-04-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-05-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-05-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-06-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-06-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-07-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-07-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-08-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-08-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-09-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-09-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-10-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-10-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-11-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-11-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-12-PLAN.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-12-SUMMARY.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-CONTEXT.md
  - .planning/phases/04-timers-interrupts-and-first-public-suites/04-QUALIFICATION.md
  - CMakeLists.txt
  - README.md
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - core/timer.c
  - docs/claims/skeleton-repeatability.claim
  - docs/memory-startup.md
  - docs/policy/exposure-record.md
  - docs/provenance.md
  - docs/public-suites.md
  - docs/timer-source-index14-pending.json
  - docs/timers-interrupts.md
  - fixtures/manifests/cpu-tests.manifest
  - fixtures/manifests/timer-tests.manifest
  - fixtures/suites/NOTICE
  - fixtures/suites/bll.cfg
  - fixtures/suites/cpu-wrapper.s
  - fixtures/suites/timers-wrapper.s
  - include/lynxplosion.h
  - scripts/build-suites.sh
  - scripts/check-manifests.sh
  - scripts/generate-suite-oracle.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/diag/test_diag.c
  - tests/diag/tests.cmake
  - tests/inventory/fast.txt
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
  - tests/tests.cmake
  - tests/timers/fault_controls.cmake
covered_digest: "v4:sha256:14fd4c974f703192d5f95a1eed07501afe4b0b1e43b19f299671009ce3d54f7e"
behavior_unverified: 0
overrides_applied: 0
deferred:
  - truth: "Global EVD-02 remains partial for the unchanged timer source result at index 14."
    addressed_in: "Phase 5"
    evidence: "Phase 5 requirements and success criterion 2 explicitly revisit the unchanged offset-14 timer oracle."
  - truth: "Global EVD-05 still requires the DMA device-fault slice."
    addressed_in: "Phase 5"
    evidence: "Phase 5 requirement mapping names the EVD-05 DMA fault slice."
  - truth: "Global EVD-05 still requires the final audio device-fault slice."
    addressed_in: "Phase 6"
    evidence: "Phase 6 requirement mapping names the EVD-05 final device-fault closure."
---

# Phase 4: Timers, Interrupts and First Public Suites Verification Report

**Phase Goal:** As a developer, I want to advance Mikey timers and interrupts on one master-tick scheduler and run the pinned cpu and timers suites headlessly, so that I can inspect bounded, reproducible per-profile guest results without firmware.

**Verified:** 2026-11-11T22:51:41Z
**Status:** passed
**Re-verification:** No. This is the first canonical whole-phase verification; the separately named pending-gate audit remains unchanged.

## User Flow Coverage

User story: “As a developer, I want to advance Mikey timers and interrupts on one master-tick scheduler and run the pinned cpu and timers suites headlessly, so that I can inspect bounded, reproducible per-profile guest results without firmware.”

| Step | Expected | Evidence | Status |
|---|---|---|---|
| Run the pinned cpu and timers suites headlessly | The original pinned sources are linked into tier-A images, loaded through the public BLL path and executed without firmware. | scripts/build-suites.sh; fixtures/suites/cpu-wrapper.s; fixtures/suites/timers-wrapper.s; tests/suites/run.cmake | ✓ |
| Select and inspect each CPU profile | Each fresh run checks the guest-observed model against explicit lynx1 or lynx2 selection and reports raw result bytes and guest time. | tests/suites/test_public_suites.c; docs/public-suites.md; 04-QUALIFICATION.md | ✓ |
| Inspect timer results and their source comparison | All 16 meaningful timer rows are retained with bounded progress and completion evidence; 15 agree, while only the owner-approved index-14 0C-versus-0D row remains pending on both profiles. | docs/timer-source-index14-pending.json; .planning/debug/evidence/timer-suite-cascade-count.md; 04-QUALIFICATION.md | ✓ |
| Use the results within their evidence limits | The observations are documented as reproducible software evidence, with fixed-zero phase and flat5/timer-before-CPU policy distinct from hardware calibration, profile qualification or firmware boot. | README.md; docs/public-suites.md; docs/timers-interrupts.md; docs/memory-startup.md | ✓ |
| Outcome: inspect bounded, reproducible per-profile guest results without firmware | Source-built guest runs produce bounded per-profile records and source comparisons without claiming hardware accuracy. | 04-QUALIFICATION.md records exact-source qualification; source identity and result acceptance are checked in tests/suites/test_public_suites.c | ✓ |

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | Timers and CPU bus work advance on one guest-tick scheduler with a total order; timer state survives snapshots with matching observations. | ✓ VERIFIED | core/machine.c:454-539 orders timer work before eligible same-tick CPU work; core/timer.c:183-253 computes and applies shared-phase events; core/snapshot.c:296-302, 453-482 serializes, validates and restores timer/hold state. Named tests api.scheduler_order and api.timer_snapshot_controls passed. |
| 2 | Timer pending state is separate from enables, CPU IRQ sampling follows the implemented profile path, and the cpu/timers suites produce bounded source-compared guest observations. | ✓ VERIFIED | core/machine.c:536 passes the pending IRQ line to CPU completion; tests/api/tests.cmake registers api.irq_pending and api.irq_poll; tests/suites/test_public_suites.c validates symbols, profile probes, results, progress and bounds. The D-10 row is the sole explicit pending comparison. |
| 3 | Both suites reject a held no-run case and reject absent or mismatched model evidence. | ✓ VERIFIED | tests/suites/test_public_suites.c:2389-2430 holds before the first instruction and checks unchanged CPU/RAM while guest time advances; CTest suite.cpu.no_run, suite.timers.no_run and suite.cpu.model_controls are registered and present in the 163-test exact-source gate. |
| 4 | Named faults fail at their intended timer or ordering assertions. | ✓ VERIFIED | tests/timers/fault_controls.cmake maps five isolated mutations to exact assertion IDs and checks a clean positive before and after; tests/api/tests.cmake registers all five inventory controls. The named timer.fault.done control also passed as a verifier spot-check. |
| 5 | The private bus-hold/request model preserves withheld CPU state while timers progress, without claiming guest-visible Suzy sleep. | ✓ VERIFIED | core/internal.h and core/machine.c:348-353 define the private hold seam; core/machine.c:475-539 advances timer events while the CPU grant is held. docs/memory-startup.md:74-76 and docs/timers-interrupts.md:127-132 keep FD91 unavailable and distinguish the seam from Suzy ownership or CPUSLEEP. |

**Score:** 49/49 truths verified (five roadmap success criteria plus 44 distinct plan truths; repeated plan statements were counted once).

### Plan Must-Have Truths

| Plan | Distinct truth coverage | Status | Evidence |
|---|---|---|---|
| 04-01 | Pinned, unchanged suite sources build as tier-A BLLs; actual symbols and first unavailable access are observed; missing or empty execution evidence refuses. | ✓ VERIFIED | scripts/build-suites.sh; tests/diag/test_diag.c; fixture manifests and NOTICE; final source qualification. |
| 04-02 | Instance-owned clock and ordered events; exclusive target and budget behavior; zero-budget/held idle behavior; shared divider phase. | ✓ VERIFIED | core/machine.c:454-539; core/timer.c:183-229; registered api.scheduler_limits and api.scheduler_order; targeted scheduler-order run passed. |
| 04-03 | DONE/reset-DONE, borrow/reload and supported links follow the transition policy; unsupported enabled links refuse; hardware phase remains unclaimed. | ✓ VERIFIED | core/timer.c; docs/timers-interrupts.md transition table; api.timer_done_borrow, api.timer_links and timer fault registrations. |
| 04-04 | IRQ pending/enable separation, acknowledgements, POLL timing and equal-tick ordering are exercised. | ✓ VERIFIED | core/machine.c:504-539; core/timer.c:224-253; api.irq_poll and api.scheduler_order targeted runs passed. |
| 04-05 | Instance isolation, retained requests, wake state, real FD91 refusal and non-underflowing CPU state are implemented. | ✓ VERIFIED | core/internal.h; core/machine.c:87-100, 348-353, 475-539; docs/memory-startup.md:66-76; request and boundary tests are in the exact inventory. |
| 04-06 | Timer/IRQ/hold state round-trips; forged snapshots refuse before apply; watched, held and restored continuations remain equivalent. | ✓ VERIFIED | core/snapshot.c:188-222, 296-302, 453-482; tests/det/test_det.c; api.timer_snapshot_controls targeted run passed. |
| 04-07 | Oracle/symbol grammar, extents, overlap, counts, time bounds, integer arithmetic and fresh-instance identities are validated. | ✓ VERIFIED | tests/suites/test_public_suites.c:422-755, 1108-1208; scripts/generate-suite-oracle.sh; suite.oracle_controls and suite.symbol_controls are registered. |
| 04-08 | Guest-observed model and source tuple are compared to explicit profiles; 18 CPU expectations are checked; wrong-profile evidence fails. | ✓ VERIFIED | tests/suites/test_public_suites.c; fixtures/suites/cpu-wrapper.s; suite.cpu.lynx1 and suite.cpu.lynx2 targeted runs passed. |
| 04-09 | The unchanged timers source runs with its own guest model probe; 16 results in an 18-byte region are preserved; the exact D-10 comparison is independently bounded. | ✓ VERIFIED | tests/suites/test_public_suites.c:1236-1305, 1526-1679, 1743 onward; docs/timer-source-index14-pending.json; both timer observation tests passed. |
| 04-10 | CPU and timer no-run cases preserve state and fail completion; empty controls, stale results, wrong models and pending-row misclassification are guarded. | ✓ VERIFIED | tests/suites/test_public_suites.c:2389-2430 and completion/record validators; suite.cpu.no_run, suite.timers.no_run and both positive suite runs passed. |
| 04-11 | Five named-fault mutations fail their intended assertions, with clean cases before and after; test/inventory drift is rejected. | ✓ VERIFIED | tests/timers/fault_controls.cmake; tests/api/tests.cmake; tests/inventory/fast.txt; timer.fault.done targeted run passed and the exact-source gate covers all five. |
| 04-12 | Exact-source qualification, reviews/fixes and evidence boundaries are recorded; D-10 remains the sole approved source comparison exception. | ✓ VERIFIED | 04-QUALIFICATION.md, 04-REVIEW.md, 04-REVIEW-FIX.md, 04-REVIEW-DISPOSITION.md, 04-VALIDATION.md; product/script source is unchanged after baabaee. |

### Prohibition Verification

The plans repeat two negative guards. Both guards are upheld by the current implementation and user-facing evidence. This verifies the guard wording and enforcement evidence only; it does not decide O1, hardware calibration, or any rights or release gate.

| Must-not claim | Status | Evidence |
|---|---|---|
| Do not present source-oracle agreement as a Lynx profile pass or hardware-accuracy claim while O1 is open. | ✓ VERIFIED (negative guard) | README.md:16-21; docs/public-suites.md:3,18,31; docs/timers-interrupts.md:166; 04-QUALIFICATION.md:11-18,65. They label the observations as software evidence and leave O1/profile qualification unresolved. |
| Do not present fixed-zero phase, timer-before-CPU order, flat5 cost or private hold as calibrated hardware, active Suzy sleep, or firmware/cartridge boot evidence. | ✓ VERIFIED (negative guard) | docs/timers-interrupts.md:21-24,127-132,212-232,274; docs/memory-startup.md:32-36,74-76; 04-QUALIFICATION.md:12-18. FD91 stays unavailable and the private hold is explicitly not CPUSLEEP or Suzy ownership. |

### Deferred Items

These are remaining global requirements with explicit later-phase ownership, not Phase 4 gaps.

| Item | Addressed in | Evidence |
|---|---|---|
| EVD-02 strict source agreement for timer result 14 | Phase 5 | Roadmap Phase 5 success criterion 2 explicitly revisits the unchanged timer oracle and preserves disagreement if evidence does not justify agreement. |
| EVD-05 DMA fault slice | Phase 5 | Roadmap Phase 5 requirement mapping names the DMA fault slice. |
| EVD-05 final audio fault slice | Phase 6 | Roadmap Phase 6 requirement mapping names final device-fault closure. |

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| core/timer.c; core/internal.h | Instance-owned bank, shared phase, timer links, DONE, borrow, IRQ pending and event state | ✓ VERIFIED | Substantive timer engine and per-instance state; used by the scheduler and snapshot code. |
| core/machine.c | Master-tick scheduler, ordered timer/CPU events, IRQ sampling and private held-request progress | ✓ VERIFIED | Calls timer candidate/apply operations, enforces event order, checks bus grant availability and passes pending IRQ into CPU completion. |
| core/snapshot.c | Canonical timer and request state serialization plus refusal-before-apply validation | ✓ VERIFIED | Timer state is serialized, validated into candidates, then restored; host watch state remains outside guest snapshot identity. |
| tests/api/test_api.c; tests/api/tests.cmake; tests/det/test_det.c; tests/det/tests.cmake | Behavioral coverage for timer, IRQ, ordering, hold, snapshot and continuation behavior | ✓ VERIFIED | Named tests are registered and exercised by the exact-source qualification; targeted transition tests passed. |
| scripts/build-suites.sh; scripts/generate-suite-oracle.sh; fixtures/suites/*; fixtures/manifests/* | Pinned source build, wrapper ABI, source identities, notices and oracle generation | ✓ VERIFIED | Build and parser check exact identities; manifests and notice cover the source fixtures. |
| tests/suites/test_public_suites.c; tests/suites/run.cmake | Real guest execution, symbol-based reads, model check, completion and bounded result validation | ✓ VERIFIED | Loads source-built BLLs into fresh machines, bounds symbols before reads, derives expectations from pinned source and rejects missing execution evidence. |
| tests/suites/tests.cmake; tests/inventory/fast.txt; tests/timers/fault_controls.cmake | Required suite/control registrations and named-fault inventory | ✓ VERIFIED | Both suite profiles, no-run/model controls and five timer faults are registered and inventoried. |
| docs/timer-source-index14-pending.json; .planning/debug/evidence/timer-suite-cascade-count.md | Exact D-10 pending row tied to source, environment, build recipe, wrapper and independent count proof | ✓ VERIFIED | Runtime descriptor validation checks the exact index, profiles, hashes, policies, raw value and proof identity. |
| README.md; docs/public-suites.md; docs/timers-interrupts.md; docs/memory-startup.md | Accurate software-only results and preserved owner limits | ✓ VERIFIED | They disclose the mismatch and avoid hardware/profile, firmware, Suzy and rights claims. |

### Key Link Verification

| From | To | Via | Status | Details |
|---|---|---|---|---|
| Timer bank | Master scheduler | timer_next/timer_apply before same-tick CPU grant | ✓ WIRED | core/machine.c:475-505 directly schedules and applies timer events. |
| Timer pending bits | CPU completion | Nonzero pending produces IRQ line independently of timer enable state | ✓ WIRED | core/machine.c:536; api.irq_pending and api.irq_poll. |
| Timer bank | Snapshot round-trip | Serialize, validate candidate, restore phase/pending/event cursor/hold | ✓ WIRED | core/snapshot.c:296-302,453-482; api.timer_snapshot_controls. |
| Pinned suite source | Built BLL and symbol-based guest results | Build script links original source; test harness validates source/object identity and linked ranges before reading RAM | ✓ WIRED | scripts/build-suites.sh; tests/suites/test_public_suites.c:1108-1208. |
| Guest profile probe | Selected profile assertion | Guest writes probe bytes; harness decodes and compares them to explicit selection | ✓ WIRED | tests/suites/test_public_suites.c:1526-1607; both CPU profile tests passed. |
| No-run hold | Suite completion validator | Hold before first instruction, advance bounded time, then reject absent guest completion | ✓ WIRED | tests/suites/test_public_suites.c:2389-2430; suite.cpu.no_run and suite.timers.no_run passed. |
| Timer observation | D-10 pending descriptor | Compare complete raw result with source oracle and validate the single bound exception against exact identities | ✓ WIRED | tests/suites/test_public_suites.c:1236-1305,1526-1679,1743 onward. |
| Fault descriptor | Intended failing assertion | Apply one source mutation, require exit 1 at named assertion, then require clean positive again | ✓ WIRED | tests/timers/fault_controls.cmake. |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|---|---|---|---|---|
| tests/suites/test_public_suites.c | g_results bytes, progress, model probe, completion sentinel, guest ticks | Executed guest code in the freshly loaded pinned BLL; addresses come from validated linker symbols | Yes | ✓ FLOWING |
| CPU oracle comparison | Expected bytes and count | Generator parses pinned main.c/tests.s, revision and source identities | Yes | ✓ FLOWING |
| Timer oracle and pending row | 16 source expectations and actual bytes | Pinned source, actual guest RAM observations, validated build record and descriptor-bound proof | Yes; exactly one approved row remains 0D expected versus 0C actual | ✓ FLOWING |
| Timer/snapshot state | Counter, phase, pending and continuation digest | Live per-instance machine state and restored snapshot candidate | Yes | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| Scheduler total order | ctest --test-dir build/check --output-on-failure -R '^api.scheduler_order$' | 1/1 passed in 0.15s | ✓ PASS |
| CPU IRQ poll boundary | ctest --test-dir build/check --output-on-failure -R '^api.irq_poll$' | 1/1 passed in 0.13s | ✓ PASS |
| Timer snapshot continuation/refusal | ctest --test-dir build/check --output-on-failure -R '^api.timer_snapshot_controls$' | 1/1 passed in 0.49s | ✓ PASS |
| CPU suite, lynx1 | ctest --test-dir build/check --output-on-failure -R '^suite.cpu.lynx1$' | 1/1 passed in 1.62s | ✓ PASS |
| CPU suite, lynx2 | ctest --test-dir build/check --output-on-failure -R '^suite.cpu.lynx2$' | 1/1 passed in 1.40s | ✓ PASS |
| Timers suite observation, lynx1 | ctest --test-dir build/check --output-on-failure -R '^suite.timers.observation.lynx1$' | 1/1 passed in 1.58s | ✓ PASS |
| Timers suite observation, lynx2 | ctest --test-dir build/check --output-on-failure -R '^suite.timers.observation.lynx2$' | 1/1 passed in 1.40s | ✓ PASS |
| CPU no-run control | ctest --test-dir build/check --output-on-failure -R '^suite.cpu.no_run$' | 1/1 passed in 1.50s | ✓ PASS |
| Timers no-run control | ctest --test-dir build/check --output-on-failure -R '^suite.timers.no_run$' | 1/1 passed in 2.76s | ✓ PASS |
| Timer DONE fault control | ctest --test-dir build/check --output-on-failure -R '^timer\\.fault\\.done$' | 1/1 passed in 2.16s | ✓ PASS |
| Exact-source final qualification | Recorded sh scripts/check.sh on baabaee in 04-QUALIFICATION.md | All 163 tests passed with exact JUnit accounting, zero skips and artifact/history checks | ✓ PASS |

The model-control replay exceeded the verifier's 10-second single-check limit, so that replay is not counted as a spot-check result. The exact-source final qualification includes the registered model control and reports the complete 163-test gate passing. No full gate was rerun here.

### Probe Execution

No phase-declared or conventional scripts/*/tests/probe-*.sh probe was found. No separate probe execution was applicable.

### Requirements Coverage

| Requirement | Source plan(s) | Status | Evidence |
|---|---|---|---|
| TIM-01 | 04-02, 04-04, 04-05, 04-06, 04-11, 04-12 | SATISFIED | One ordered master-tick scheduler and event-order tests; snapshot/path controls passed. |
| TIM-02 | 04-02, 04-03, 04-06, 04-09, 04-11, 04-12 | SATISFIED | Shared-phase timer bank, supported link graph, DONE/borrow semantics and named fault controls. Phase policy is software-only. |
| TIM-03 | 04-02 through 04-06, 04-09, 04-11, 04-12 | SATISFIED | Pending/enable separation and CPU poll behavior have direct registered tests. |
| TIM-04 | 04-05, 04-06, 04-10, 04-12 | SATISFIED for Phase 4 slice | Private hold/request/wake seam is implemented and tested. The global checkbox remains root-owned; this report does not edit REQUIREMENTS.md. |
| EVD-02 | 04-01, 04-07 through 04-12 | SATISFIED for owner-approved Phase 4 observation/control slice; globally PARTIAL | Pinned suites, symbol-based reads, completion/bounds/count checks and all16 timer comparisons are present. Only index14 expected 0D versus actual 0C on both profiles remains pending under D-10; strict comparison still exits 1. |
| EVD-03 | 04-10, 04-12 | SATISFIED | Both CPU and timer no-run controls passed. |
| EVD-04 | 04-01, 04-07 through 04-10, 04-12 | SATISFIED for Phase 4 probe/control slice | Guest-observed model is compared with explicit selected profile and wrong-model controls are registered. The broader taxonomy remains unresolved. |
| EVD-05 | 04-04, 04-05, 04-08 through 04-12 | SATISFIED for timer/IRQ slice; globally PARTIAL | Five named timer faults cover this phase. DMA and audio fault obligations remain open in later phases. |

All eight requirement IDs declared by the 12 plans appear in REQUIREMENTS.md. No additional Phase 4 requirement mapping is orphaned.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| — | — | None found in phase implementation artifacts. No TBD, FIXME, XXX, TODO or HACK markers. Search matches for NULL returns are error/initialization paths; “placeholder” occurrences explicitly label the flat5/audio policy and are not empty implementations. | — | No stub or incomplete user-visible path found. |

### Human Verification Required

None. No visual interface, external service, physical hardware or performance-feel claim is required for this software slice. O1, hardware calibration, firmware rights, physical testing, publication and release gates remain unresolved as directed; this report does not reopen or waive them.

### Gaps Summary

No blocking Phase 4 gap found. The scheduler, timer/IRQ behavior, snapshots, private request hold, pinned guest execution, bounded per-profile observations, suite controls and negative claim guards are present and wired. The sole D-10 timer source mismatch is the roadmap's explicit owner-approved exception for this phase. Global EVD-02 and EVD-05 remain partial and are not marked complete.

---

_Verified: 2026-11-11T22:51:41Z_
_Verifier: the agent (gsd-verifier)_
