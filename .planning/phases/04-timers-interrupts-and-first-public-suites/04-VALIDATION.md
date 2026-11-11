---
phase: "04"
slug: timers-interrupts-and-first-public-suites
status: complete
audit_status: complete
qualification_status: qualified
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-10"
---

# Phase 4 — Validation Strategy and Prequalification Audit

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing C17 executables, CTest, CMake/POSIX shell controls; no new framework |
| Configuration | CMakePresets.json, tests/tests.cmake, exact tests/inventory/fast.txt and bound claim |
| Build/targeted command | Configure/build check preset, then nonempty CTest selection for the owning new behavior; planner assigns exact registered IDs |
| Full regression/qualification command | `sh scripts/check.sh` (also workflow.test_command and CI command) |
| Current measured baseline | Phase3 source: macOS full89s; Linux full53s; closeout CI single job42s with check37s. Phase4 timer result-control batching was measured at 188.20s before and 61.49s after the approved data-only batching adjustment; the final full-gate cost remains pending. |

## Sampling Rate

After each meaningful task, run its exact direct mode or targeted registered CTests with `--no-tests=error`. After an integration wave or source concern, run affected cross-module regressions. Run targeted integration checks first, then return to root for full-phase code/security/coverage/UI-applicability reviews and the independent pending-gate audit/verifier. After accepted fixes, run the final full script once on the reviewed tree, then self-contained Linux and exact-head CI gates. Reuse one full local run for the regression gate and final local qualification. This applies the owner's targeted-during-implementation/full-at-integration efficiency policy rather than duplicating history/rights scans after every small plan. Preserve every distinct negative control.

## Requirement Verification Map (prequalification audit)

| Requirement | Required behavior and independent controls | Status |
|---|---|---|
| TIM-01 | Total order, stable tie sequence, target boundaries, no overshoot, slicing, idle jumps and arithmetic/sequence limits | Targeted evidence recorded in Plans02,04,05,06,11; final phase qualification pending |
| TIM-02 | Shared phase retained across writes; counters/reload/DONE/reset-DONE/borrow; supported cascades; explicit unavailable links; forged snapshot and named faults | Targeted evidence recorded in Plans02,03,06,11; hardware/taxonomy assumptions remain unresolved |
| TIM-03 | Pending separate from source enables, INTSET/INTRST, Timer4 UART exception, profile POLL before/at/after edge, I masks acceptance | Targeted evidence recorded in Plans02–06,08,11; profile qualification remains unresolved under O1 |
| TIM-04 | Held posted request unchanged, timers keep progressing, wake without double bus effects; no WAI/STP or fabricated Suzy | Targeted request/snapshot evidence recorded in Plans05–06; final phase qualification pending |
| EVD-02 | Exact pin/notice/build identity, strict BLL/link symbols, all18 CPU/all16 timer comparisons, alternatives/counts and independent sentinel/progress/time/model. D-10 observation/control slice requires15 timer agreements and exact bound index14 pending0C versus source0D; strict CLI still fails. | Global agreement OPEN; pending index14 carried to Phase5, never completed by Phase4 summary |
| EVD-03 | CPU truly prevented from executing the first test instruction; both suites rejected despite zero-like results | Four actual held no-run runs and exact absent-execution refusals are recorded; final phase qualification pending |
| EVD-04 | Guest-executed per-suite model evidence; wrong selected profile rejected; no copied host value | CPU and timer guest probes plus opposite-profile controls are recorded; O1/taxonomy is still unresolved |
| EVD-05 | Named DONE/reset-DONE/order/IRQ/completion/model/count faults each fail intended assertions with positive controls | Phase4 controls are recorded; recurring DMA/audio obligations keep global EVD-05 open |

## Per-Task Verification Map

The commands remain the plan acceptance commands. The status column now reflects observed evidence from Plans01–11 and the Plan 12 Task1 ledger; it does not claim final phase qualification. Detailed current registrations, control inventories, evidence boundaries, and ownership are in [04-COVERAGE-AUDIT.md](04-COVERAGE-AUDIT.md). New code/test modes are Wave0 work inside the owning leading behavior task; CTest registration, fast inventory and claim hash land in that same implementation commit.

| Task | Wave | Requirements | Threat refs | Expected secure behavior | Type | Exact automated command | Source/scaffold | Status |
|---|---|---|---|---|---|---|---|---|
| 04-01-T1 | 1 | EVD-02, EVD-04 | T-04-01, T-04-02, T-04-03 | T-04-SC; Both exact suites build and publicly load, labels bound the actual objects, source hashes are unchanged, and a bounded guest prefix exposes its actual first unsupported request without raw-RAM substitution. | End-to-end tracer; suite.build | `sh scripts/build-suites.sh --out build/check/scratch/phase04-build && cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-diag suite_build build/check/scratch/phase04-build` | Wave0 original wrappers/build modes; existing diagnostic executable | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-01-T2 | 1 | EVD-02, EVD-04 | T-04-01, T-04-02, T-04-03 | T-04-SC; Metadata covers every new fixture input, exact notice identity and startup/rights limits; no upstream archive, source copy or BLL binary is committed. | Behavior/control expansion; hygiene.manifests, hygiene.tree | `sh scripts/check-manifests.sh && sh scripts/hygiene.sh --tree` | Wave0 original wrappers/build modes; existing diagnostic executable | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-01-T3 | 1 | EVD-02, EVD-04 | T-04-01, T-04-02, T-04-03 | T-04-SC; Both registered tests exercise cases, malformed inputs fail their own checks, and inventory/claim binding remains exact. | Behavior/control expansion; suite.build, suite.build_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(build\|build_controls)$'` | Wave0 original wrappers/build modes; existing diagnostic executable | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-02-T1 | 2 | TIM-01, TIM-02, TIM-03 | T-04-04, T-04-05, T-04-06 | T-04-SC; The original guest programs timer0, observes expiry/pending through the real mapped path and stores the expected RAM result; exclusive-target and zero-budget assertions pass. | End-to-end tracer; api.timer_tracer | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_tracer` | Wave0 timer module/direct modes; existing API executable | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-02-T2 | 2 | TIM-01, TIM-02, TIM-03 | T-04-04, T-04-05, T-04-06 | T-04-SC; Every timer/divider is exercised, shared phase survives each write class and unsupported routes do not read backing RAM. Registered tests are nonempty and source/policy labels describe the behavior actually implemented. | Behavior/control expansion; api.timer_standalone, api.timer_phase, api.timer_tracer | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_standalone && build/check/tests/lynx-test-api timer_phase` | Wave0 timer module/direct modes; existing API executable | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-03-T1 | 3 | TIM-02, TIM-03 | T-04-07, T-04-08, T-04-09 | T-04-SC; Guest-written CTLB/CTLA values exercise held reset-DONE and count gating, and the linked pair yields the independent event/count trace. | End-to-end tracer; api.timer_done_borrow, api.timer_link_tracer | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_done_borrow && build/check/tests/lynx-test-api timer_link_tracer` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-03-T2 | 3 | TIM-02, TIM-03 | T-04-07, T-04-08, T-04-09 | T-04-SC; All supported links and unsupported incoming modes are exercised, no raw-RAM substitutes exist, and each edge case has a nonzero assertion inventory. | Behavior/control expansion; api.timer_done_borrow, api.timer_link_tracer, api.timer_links, api.timer_unavailable | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.timer_(done_borrow\|link_tracer\|links\|unavailable)$'` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-04-T1 | 4 | TIM-01, TIM-03, EVD-05 | T-04-10, T-04-11, T-04-12 | T-04-SC; Both profiles take the intended original IRQ handler; pending is independent of later enables and before/at/after POLL observations match the declared policy. | End-to-end tracer; api.irq_pending, api.irq_poll | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api irq_pending && build/check/tests/lynx-test-api irq_poll` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-04-T2 | 4 | TIM-01, TIM-03, EVD-05 | T-04-10, T-04-11, T-04-12 | T-04-SC; Five registered cases prove pending/POLL and total-order/watch/limit behavior, including meaningful same-tick event observations. | Behavior/control expansion; api.irq_pending, api.irq_poll, api.scheduler_order, api.scheduler_limits, api.scheduler_watch | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(irq_pending\|irq_poll\|scheduler_order\|scheduler_limits\|scheduler_watch)$'` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-05-T1 | 5 | TIM-04, TIM-01, TIM-03 | T-04-13, T-04-14, T-04-15 | T-04-SC; One original store remains pending during real timer progress and completes exactly once after release; both independent instances remain isolated. Long-held idle requests report zero remaining cost without underflow; an in-flight request reports its actual residual cost until expiration. | End-to-end tracer; api.request_hold | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api request_hold` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-05-T2 | 5 | TIM-04, TIM-01, TIM-03 | T-04-13, T-04-14, T-04-15 | T-04-SC; Held/wake/cycle boundaries pass their independent assertions, and unsupported guest sleep is explicit. | Behavior/control expansion; api.request_hold, api.request_wake, api.request_boundary | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.request_(hold\|wake\|boundary)$'` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-06-T1 | 6 | TIM-01, TIM-02, TIM-03, TIM-04 | T-04-16, T-04-17, T-04-18 | T-04-SC; The next format resumes a real timer/IRQ/request path; forged/old formats refuse atomically and the inherited 255-ready gap is closed. Restored held/in-flight readiness and public remaining-cost observations follow the canonical normalization and exact reachable residual bound. | End-to-end tracer; api.timer_snapshot, api.snapshot_refusal | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_snapshot && build/check/tests/lynx-test-api snapshot_refusal` | Existing core/API/determinism sources; new named behavior modes | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-06-T2 | 6 | TIM-01, TIM-02, TIM-03, TIM-04 | T-04-16, T-04-17, T-04-18 | T-04-SC; Common-tick paths match each independent observation, hostile restores preserve whole instances, and any golden edit has a specific representation cause. Exact registrations execute all snapshot paths and documented fields/bounds match the serializer. | Behavior/control expansion; api.timer_snapshot_controls, det.timer_paths, det.golden, det.golden.self_test, api.timer_snapshot | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_snapshot_controls && build/check/tests/lynx-test-det timer_paths && ctest --preset check --no-tests=error --output-on-failure -R '^det\.golden(\.self_test)?$'` | Existing core/API/determinism sources; new named behavior modes | Targeted retest passed after 4 stale format-consumer fixes; full canonical gate remains pending |
| 04-07-T1 | 7 | EVD-02, EVD-04 | T-04-19, T-04-20, T-04-21 | T-04-SC; A real CPU/lynx1 suite completes with exact source/symbol/model/count/time provenance and comparison; unsupported or disagreeing evidence cannot be marked complete. | End-to-end tracer; suite.cpu.lynx1, suite.oracle_tracer | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(cpu\.lynx1\|oracle_tracer)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-07-T2 | 7 | EVD-02, EVD-04 | T-04-19, T-04-20, T-04-21 | T-04-SC; Every invalid input fails its specific assertion before dependent RAM reads, with a nonempty fault inventory and fresh positives. | Behavior/control expansion; suite.oracle_tracer, suite.oracle_controls, suite.symbol_controls, suite.cpu.lynx1 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(oracle_tracer\|oracle_controls\|symbol_controls\|cpu\.lynx1)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-08-T1 | 8 | EVD-02, EVD-04, EVD-05, TIM-03 | T-04-22, T-04-23 | T-04-SC; Both explicitly selected CPU profiles really complete, derive the observed model independently and compare the complete source-generated result set. | End-to-end tracer; suite.cpu.lynx1, suite.cpu.lynx2 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.cpu\.(lynx1\|lynx2)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-08-T2 | 8 | EVD-02, EVD-04, EVD-05, TIM-03 | T-04-22, T-04-23 | T-04-SC; Each seeded error reaches its intended validator failure with exact status/message and positive controls restore clean behavior. | Behavior/control expansion; suite.cpu.model_controls, suite.cpu.bound_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.cpu\.(model_controls\|bound_controls)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-09-T1 | 9 | EVD-02, EVD-04, EVD-05, TIM-02, TIM-03 | T-04-24, T-04-25 | T-04-SC; Both profiles have complete model/progress/sentinel/bounds/count/identity observations; all16 timer rows are compared,15 agree and only bound index14 actual0C/source0D is pending. Independent64-phase model predicts12. Strict CLI still returns exact exit 1/diagnostic/row. | End-to-end observation; suite.timers.observation.lynx1, suite.timers.observation.lynx2, suite.timers.strict_comparison_controls, suite.timers.cascade_model | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(build\|build_controls\|oracle_tracer\|oracle_controls\|symbol_controls\|timers\.(observation\.(lynx1\|lynx2)\|strict_comparison_controls\|cascade_model))$'` | Preserve/integrate seven current edits; separate observe/strict modes, exact descriptor and proof; atomic registrations/inventory/claim | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-09-T2 | 9 | EVD-02, EVD-04, EVD-05, TIM-02, TIM-03 | T-04-24, T-04-25 | T-04-SC; Actual model mismatch and shape/alternatives faults reach exact earlier refusals; changed pending value/descriptor/environment/proof, second mismatch or independent execution error never passes. | Behavior controls; suite.timers.model_controls, suite.timers.result_controls, suite.timers.pending_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(model_controls\|result_controls\|pending_controls)$'` | Existing guest/model behavior; normal verified controls, no fabricated product RED | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-10-T1 | 10 | EVD-03, EVD-02, EVD-04, EVD-05, TIM-04 | T-04-26, T-04-27, T-04-28 | T-04-SC; Both real suites fail completion despite time reaching minimum and oracle-like initial RAM, with proof no test instruction ran. | End-to-end tracer; suite.cpu.no_run, suite.timers.no_run | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(cpu\|timers)\.no_run$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-10-T2 | 10 | EVD-03, EVD-02, EVD-04, EVD-05, TIM-04 | T-04-26, T-04-27, T-04-28 | T-04-SC; Each missing/stale/zero/premature execution fact fails its intended assertion and no empty case inventory can appear green. | Behavior/control expansion; suite.completion_controls, suite.selection_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(completion_controls\|selection_controls)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-11-T1 | 11 | EVD-05, TIM-01, TIM-02, TIM-03 | T-04-29, T-04-30, T-04-31 | T-04-SC; Both successfully built semantic mutants fail their intended independent assertion, and fresh clean positives pass before/after each. | End-to-end tracer; timer.fault.done, timer.fault.reset_done | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^timer\.fault\.(done\|reset_done)$'` | Wave0 isolated fault driver; existing core/API guest cases | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-11-T2 | 11 | EVD-05, TIM-01, TIM-02, TIM-03 | T-04-29, T-04-30, T-04-31 | T-04-SC; Each phase/order/IRQ mutant builds, triggers its specific independent test and leaves clean source/tests passing afterward. | Behavior/control expansion; timer.fault.phase, timer.fault.order, timer.fault.irq | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^timer\.fault\.(phase\|order\|irq)$'` | Wave0 isolated fault driver; existing core/API guest cases | Targeted evidence recorded as passing; final phase qualification remains pending |
| 04-12-T1 | 12 | TIM-01, TIM-02, TIM-03, TIM-04, EVD-02, EVD-03, EVD-04, EVD-05 | T-04-32, T-04-33, T-04-34 | T-04-SC; Actual pending-gate implementation/records/inventory are independently audited; all16 rows and strict failure/global EVD-02 stay explicit. Executor returns to root for reviews/fixes before final qualification/push. | Targeted pending-gate audit; exact Plan 09 timer controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(observation\.(lynx1\|lynx2)\|strict_comparison_controls\|cascade_model\|model_controls\|result_controls\|pending_controls)$'` | Existing implemented controls; root pending verifier/code/security/coverage/UI-applicability checkpoint | Task1 targeted evidence recorded: 7/7 at source `0c4ff91`; parent reviews and independent verifier pending |
| 04-12-T2 | 12 | TIM-01, TIM-02, TIM-03, TIM-04, EVD-02, EVD-03, EVD-04, EVD-05 | T-04-32, T-04-33, T-04-34 | T-04-SC; Root reviews/verifier/fixes precede final full macOS/Linux-arm64/exact-head CI. Final source passes approved D-10 observation/control gate; strict index14/global EVD-02 remain open. | Final full qualification; exact tests/inventory/fast.txt | `sh scripts/check.sh` | Root resumes after all accepted fixes and targeted retests; one final local gate then self-contained Linux/exact-head CI | PLANNED/PENDING final qualification; not a product test gap or human-only item |

Every command has an immediate fails_when sibling in its PLAN. Direct modes must report nonzero cases; CTest uses --no-tests=error. Full-gate rows refer to every exact ID in tests/inventory/fast.txt rather than an invented aggregate test ID. Final local/Ubuntu 24.04 arm64/exact-head CI obligations remain separate from a command merely returning zero.

## Audit outcome — 2026-11-11

- All 25 tasks map to registered behavioral checks, named negative controls, or the explicit final qualification obligation. See [04-COVERAGE-AUDIT.md](04-COVERAGE-AUDIT.md) for the 25-row map and hard-path inventories.
- A headless `ctest -N` discovery found 49 relevant Phase4 CTest IDs. Each appears exactly once in `tests/inventory/fast.txt`; `sh scripts/check-claims.sh --inventory-sha256` passed with digest `4c96587c3303c64aaa14874597cc173288eaf155dce9edca465aa3c3893034`. These are inventory checks, not test executions.
- Plan 12 Task1 is complete and committed at HEAD `db7149e`; its ledger records the seven targeted timer integration tests passing at source `0c4ff91`. HEAD changes only README and documentation/qualification records. This audit did not rerun those tests.
- No current product behavior gap was found in source/test mapping. One actionable review finding remains: current source has 49 timer result-control cases (the Plan 09 historical evidence records 48) and 55 pending descriptor/companion/proof cases, but their batch guards only require `case_count >= 1`. Removing a mutation call could leave an aggregate CTest green. Root owns reconciling the 48/49 drift and adding an exact unique case-name manifest with missing/duplicate detection before final qualification; this is not manual-only and does not require a new owner decision.
- Plan 12 Task2 remains **PLANNED/PENDING** until root completes the full-phase code/security/coverage and UI-applicability reviews, accepted fixes/targeted retests, and independent pending-gate verification. It is cross-platform/exact-head qualification work, not a missing product test and not a human-only UAT item.

The audit leaves `nyquist_compliant: false`, `wave_0_complete: false`, and final qualification pending. It did not modify implementation or test files and preserved the untracked `.planning/state.json`.

## Coverage Flags

- All 18 explicit edge resolutions have targeted checks mapped in the completed Plans01–11; full-phase sign-off remains pending.
- TIM-02 and EVD-04 are unclassified, unresolved flagged assumptions; transition/probe tests are concrete, and software checks do not auto-resolve hardware or taxonomy uncertainty.
- Both recalled prohibitions remain unresolved/flagged-unverified in must_haves.prohibitions, with no fabricated wired-check descriptor.
- Readiness is canonical: expired priced-cycle readiness normalizes to guest time; ownership hold stays separate and the public CPU_STATE remaining-cost byte cannot underflow. Held/in-flight restore checks are in Plans05/06.
- Full four-source audit and dependency graph are embedded in 04-12-PLAN.md. No source item is silently deferred.

## Approved D-10 pending-comparison contract

The successful timer check is complete **software observation/control evidence**, not source-suite agreement. Keep16 comparisons,15 required agreements and only source index14 expected0D/actual0C pending on both profiles. Strict run timers remains exit 1 with its exact diagnostic and complete comparison output. The separately registered strict-comparison control must prove that precise refusal; generic nonzero or WILL_FAIL acceptance is invalid.

The structured pending descriptor and each per-run pending record bind original source/tool/wrapper/oracle/build identity, exact profiles and policies, flat5/no-DMA/zero-counter environment, all execution/model/count/time facts, and independent64-phase12-count proof derived from validated pinned instructions and actual linked BLL opcode/address/branch geometry, with cost_ticks5 separate from sample_offset_ticks0. Missing/stale descriptor, wrong raw14 value (including0B or0D), second mismatch or any independent failure fails the observation gate. Controls must prove named diagnostics precede pending classification. Include malformed JSON, unknown/duplicate/missing keys, missing/stale schema/version and wrong type/value controls with exact named failures. Keep the original flat key=value run record and a distinct JSON pending companion; finalize/hash raw/run/comparison/proof inputs first, with no self-hash or cyclic identity links. No skipping row14, acceptance of all mismatches, retuned phase/costs/layout/counter, oracle edit or Phase5 implementation is authorized.

Plans10–11 use observation-gate timer positives and full-source CPU positives. True no-run holds before the first instruction; each negative/mutant must reach its intended named failure, not the known pending comparison. Fixture/harness/fault controls over already implemented behavior are normal verified tasks with tdd=false. Actual new production bugs require genuine pre-fix behavior RED.

Plan 12 Task1 returns to root for full-phase code/security/coverage review, UI applicability, accepted fixes and independent pending-gate verifier. Final full macOS/Linux/exact-head CI follows that checkpoint, with no push/CI first. Source changes after qualification invalidate affected exact-source evidence.

Global EVD-02 stays open and carried to Phase5 even if Phase4 completes. O1 and the two existing prohibitions remain unresolved; unresolved verification is null, no invented check descriptor. No hardware, firmware, physical, licence, release or new publication authority is inferred.

## Wave 0 Requirements

- [ ] Original source-built suite wrapper/manifest/generator/labels and count controls, with no upstream binary/source import into the public tree.
- [ ] Minimal original guest timer/IRQ tracer plus bank/scheduler/CPU/snapshot targeted tests.
- [ ] Source-oracle/semantic sentinel/no-run/model-mismatch controls under isolated machine state.
- [ ] Ownership/refusal, overflow, same-tick watch prefix and restore equivalence controls.
- [ ] Registration/inventory and claim-hash updates, with explicit named out-of-scope suite entries.

## Manual-Only Verifications

None required for this software phase. Hardware measurements, exact phase/slot calibration, baseline/suite-pass claims, firmware/rights decisions, project licence/release and physical tests remain outside authority. Missing hardware evidence is visible as a limitation, not a manufactured software UAT approval.

## Validation Sign-Off

- [ ] Every task has a meaningful automated check or explicit Wave0 dependency.
- [ ] No three consecutive implementation tasks lack targeted feedback.
- [ ] All registered IDs match the fast inventory and claim hash, with visible skips/missing evidence.
- [ ] Same-tick, split/watch/restore and hostile input/state controls test independent observations.
- [ ] Bounds and costs measured from actual fixtures; no watch/foreground-app mode.
- [ ] Coverage auditor sets validated/nyquist_compliant only after actual evidence.

Approval: implementation/verification is covered by existing phase authority; this draft claims no passing Phase4 check.

## Post-review repair evidence

The original inventory warning above is resolved at source `27145c8`. The batches
now enforce exact ordered unique manifests (49 result and 61 pending cases),
with missing/duplicate self-controls. The pending inventory gained six distinct
output/identity cases; historical Plan 09=48, pre-fix result49/pending55 remain
dated observations. Parser, symbol aliases, source/object/label identities and
owned output boundaries were repaired and independently rechecked; see
04-REVIEW-FIX.md and 04-SECURITY.md. Root's six affected CTests and two CPU/tracer
acceptance tests pass. The Plan 07 Task 1 command now uses the registered owned-output
driver; the previous direct unmarked CLI call is no longer a valid acceptance
path. Independent pending-gate verification and Plan 12 Task 2 qualification remain
outstanding, so compliance/completion flags remain false.

## Final coverage qualification

All 25 plan tasks are executed and mapped; Plan 12 SUMMARY now records complete
macOS, self-contained Ubuntu arm64 and exact-head hosted qualification. The
full 163-test fast inventory ran exactly once each, no skips, with all existing
and added refusal/mutation/manifest controls. The scoped D-10 verifier passed,
all implementation review findings are fixed, and 49/61 exact subcase manifests
retain their missing/duplicate controls. No new behavior gap remains and no
human UAT is needed. The flags above refer to this final software-scope audit;
earlier prequalification/pending statements are dated history, not live gaps.

The final canonical local command also serves as the regression gate under
Plan 12's explicit one-run policy. Product/scripts did not change after source
7616f6d; hosted run 43104539428 repeats the whole gate at exact docs-only
head `356a920`. Global EVD-02's source agreement and recurring EVD-05 future-device
coverage remain open, independently of this phase's complete coverage.

## Final marker review supplement

WR-02 adds five malformed pin-marker forms plus two valid forms to the existing
oracle controls. The old production predicate fails the new trailing-data
control; the tightened predicate and six owning tests pass. No CTest IDs or
49/61 batch inventories change. Cross-platform qualification is renewed after
this source change; previous complete-gate rows remain valid historical results.

## Marker-fix qualification completed

Exact source `baabaee` passed the full 163-test gate on macOS, self-contained
Ubuntu arm64 and hosted run 43111187855, with no skips and complete JUnit,
artifact and history results. Two positive/five malformed marker controls are
inside the existing oracle-controls ID; registration and claim hashes are
unchanged. All 25 tasks remain covered, the 11 review findings are fixed, and no
human verification or unimplemented coverage gap remains. The current flags
reflect this final evidence; intermediate reopened qualification rows above
remain history. Global EVD-02/EVD-05 still retain their later-phase scopes.
