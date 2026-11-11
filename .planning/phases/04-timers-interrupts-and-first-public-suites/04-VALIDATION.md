---
phase: "04"
slug: timers-interrupts-and-first-public-suites
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 4 — Validation Strategy

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing C17 executables, CTest, CMake/POSIX shell controls; no new framework |
| Configuration | CMakePresets.json, tests/tests.cmake, exact tests/inventory/fast.txt and bound claim |
| Build/targeted command | Configure/build check preset, then nonempty CTest selection for the owning new behavior; planner assigns exact registered IDs |
| Full regression/qualification command | `sh scripts/check.sh` (also workflow.test_command and CI command) |
| Current measured baseline | Phase3 source: macOS full89s; Linux full53s; closeout CI single job42s with check37s. Phase4 fixture/build/runtime cost not yet measured. |

## Sampling Rate

After each meaningful task, run its exact direct mode or targeted registered CTests with `--no-tests=error`. After an integration wave or source concern, run affected cross-module regressions. Run the full script at phase integration and after review fixes that change source, then the self-contained Linux and exact-head CI gates. Reuse one full local run for the regression gate and final local qualification. This applies the owner's targeted-during-implementation/full-at-integration efficiency policy rather than duplicating history/rights scans after every small plan. Preserve every distinct negative control.

## Requirement Verification Map (planned)

| Requirement | Required behavior and independent controls | Status |
|---|---|---|
| TIM-01 | Total order, stable tie sequence, target boundaries, no overshoot, slicing, idle jumps and arithmetic/sequence limits | Tests to be registered by owning plan |
| TIM-02 | Shared phase retained across writes; counters/reload/DONE/reset-DONE/borrow; supported cascades; explicit unavailable links; forged snapshot and named faults | Tests to be registered |
| TIM-03 | Pending separate from source enables, INTSET/INTRST, Timer4 UART exception, profile POLL before/at/after edge, I masks acceptance | Tests to be registered |
| TIM-04 | Held posted request unchanged, timers keep progressing, wake without double bus effects; no WAI/STP or fabricated Suzy | Tests to be registered |
| EVD-02 | Exact pin/notice/build identity, strict BLL/link symbols, source-generated 18 CPU/16 meaningful timer expectations, alternatives/counts, independent sentinel/progress/time/model | Tests to be registered |
| EVD-03 | CPU truly prevented from executing the first test instruction; both suites rejected despite zero-like results | Tests to be registered |
| EVD-04 | Guest-executed per-suite model evidence; wrong selected profile rejected; no copied host value | Tests to be registered |
| EVD-05 | Named DONE/reset-DONE/order/IRQ/completion/model/count faults each fail intended assertions with positive controls | Tests to be registered |

## Per-Task Verification Map

All rows are **pending execution**. Named IDs are acceptance obligations, not evidence until they are implemented, registered, inventoried and run. New code/test modes are Wave0 work inside the owning leading behavior task; CTest registration, fast inventory and claim hash land in that same implementation commit.

| Task | Wave | Requirements | Threat refs | Expected secure behavior | Type | Exact automated command | Source/scaffold | Status |
|---|---|---|---|---|---|---|---|---|
| 04-01-T1 | 1 | EVD-02, EVD-04 | T-04-01, T-04-02, T-04-03 | T-04-SC; Both exact suites build and publicly load, labels bound the actual objects, source hashes are unchanged, and a bounded guest prefix exposes its actual first unsupported request without raw-RAM substitution. | End-to-end tracer; suite.build | `sh scripts/build-suites.sh --out build/check/scratch/phase04-build && cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-diag suite_build build/check/scratch/phase04-build` | Wave0 original wrappers/build modes; existing diagnostic executable | pending |
| 04-01-T2 | 1 | EVD-02, EVD-04 | T-04-01, T-04-02, T-04-03 | T-04-SC; Metadata covers every new fixture input, exact notice identity and startup/rights limits; no upstream archive, source copy or BLL binary is committed. | Behavior/control expansion; hygiene.manifests, hygiene.tree | `sh scripts/check-manifests.sh && sh scripts/hygiene.sh --tree` | Wave0 original wrappers/build modes; existing diagnostic executable | pending |
| 04-01-T3 | 1 | EVD-02, EVD-04 | T-04-01, T-04-02, T-04-03 | T-04-SC; Both registered tests exercise cases, malformed inputs fail their own checks, and inventory/claim binding remains exact. | Behavior/control expansion; suite.build, suite.build_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(build\|build_controls)$'` | Wave0 original wrappers/build modes; existing diagnostic executable | pending |
| 04-02-T1 | 2 | TIM-01, TIM-02, TIM-03 | T-04-04, T-04-05, T-04-06 | T-04-SC; The original guest programs timer0, observes expiry/pending through the real mapped path and stores the expected RAM result; exclusive-target and zero-budget assertions pass. | End-to-end tracer; api.timer_tracer | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_tracer` | Wave0 timer module/direct modes; existing API executable | pending |
| 04-02-T2 | 2 | TIM-01, TIM-02, TIM-03 | T-04-04, T-04-05, T-04-06 | T-04-SC; Every timer/divider is exercised, shared phase survives each write class and unsupported routes do not read backing RAM. Registered tests are nonempty and source/policy labels describe the behavior actually implemented. | Behavior/control expansion; api.timer_standalone, api.timer_phase, api.timer_tracer | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_standalone && build/check/tests/lynx-test-api timer_phase` | Wave0 timer module/direct modes; existing API executable | pending |
| 04-03-T1 | 3 | TIM-02, TIM-03 | T-04-07, T-04-08, T-04-09 | T-04-SC; Guest-written CTLB/CTLA values exercise held reset-DONE and count gating, and the linked pair yields the independent event/count trace. | End-to-end tracer; api.timer_done_borrow, api.timer_link_tracer | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_done_borrow && build/check/tests/lynx-test-api timer_link_tracer` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-03-T2 | 3 | TIM-02, TIM-03 | T-04-07, T-04-08, T-04-09 | T-04-SC; All supported links and unsupported incoming modes are exercised, no raw-RAM substitutes exist, and each edge case has a nonzero assertion inventory. | Behavior/control expansion; api.timer_done_borrow, api.timer_link_tracer, api.timer_links, api.timer_unavailable | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.timer_(done_borrow\|link_tracer\|links\|unavailable)$'` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-04-T1 | 4 | TIM-01, TIM-03, EVD-05 | T-04-10, T-04-11, T-04-12 | T-04-SC; Both profiles take the intended original IRQ handler; pending is independent of later enables and before/at/after POLL observations match the declared policy. | End-to-end tracer; api.irq_pending, api.irq_poll | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api irq_pending && build/check/tests/lynx-test-api irq_poll` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-04-T2 | 4 | TIM-01, TIM-03, EVD-05 | T-04-10, T-04-11, T-04-12 | T-04-SC; Five registered cases prove pending/POLL and total-order/watch/limit behavior, including meaningful same-tick event observations. | Behavior/control expansion; api.irq_pending, api.irq_poll, api.scheduler_order, api.scheduler_limits, api.scheduler_watch | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(irq_pending\|irq_poll\|scheduler_order\|scheduler_limits\|scheduler_watch)$'` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-05-T1 | 5 | TIM-04, TIM-01, TIM-03 | T-04-13, T-04-14, T-04-15 | T-04-SC; One original store remains pending during real timer progress and completes exactly once after release; both independent instances remain isolated. Long-held idle requests report zero remaining cost without underflow; an in-flight request reports its actual residual cost until expiration. | End-to-end tracer; api.request_hold | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api request_hold` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-05-T2 | 5 | TIM-04, TIM-01, TIM-03 | T-04-13, T-04-14, T-04-15 | T-04-SC; Held/wake/cycle boundaries pass their independent assertions, and unsupported guest sleep is explicit. | Behavior/control expansion; api.request_hold, api.request_wake, api.request_boundary | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.request_(hold\|wake\|boundary)$'` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-06-T1 | 6 | TIM-01, TIM-02, TIM-03, TIM-04 | T-04-16, T-04-17, T-04-18 | T-04-SC; The next format resumes a real timer/IRQ/request path; forged/old formats refuse atomically and the inherited 255-ready gap is closed. Restored held/in-flight readiness and public remaining-cost observations follow the canonical normalization and exact reachable residual bound. | End-to-end tracer; api.timer_snapshot, api.snapshot_refusal | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_snapshot && build/check/tests/lynx-test-api snapshot_refusal` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-06-T2 | 6 | TIM-01, TIM-02, TIM-03, TIM-04 | T-04-16, T-04-17, T-04-18 | T-04-SC; Common-tick paths match each independent observation, hostile restores preserve whole instances, and any golden edit has a specific representation cause. Exact registrations execute all snapshot paths and documented fields/bounds match the serializer. | Behavior/control expansion; api.timer_snapshot_controls, det.timer_paths, det.golden, det.golden.self_test, api.timer_snapshot | `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api timer_snapshot_controls && build/check/tests/lynx-test-det timer_paths && ctest --preset check --no-tests=error --output-on-failure -R '^det\.golden(\.self_test)?$'` | Existing core/API/determinism sources; new named behavior modes | pending |
| 04-07-T1 | 7 | EVD-02, EVD-04 | T-04-19, T-04-20, T-04-21 | T-04-SC; A real CPU/lynx1 suite completes with exact source/symbol/model/count/time provenance and comparison; unsupported or disagreeing evidence cannot be marked complete. | End-to-end tracer; suite.cpu.lynx1, suite.oracle_tracer | `sh scripts/build-suites.sh --out build/check/scratch/phase04-cpu && sh scripts/generate-suite-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/phase04-cpu/oracle.tsv && cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-suites run cpu lynx1 build/check/scratch/phase04-cpu build/check/scratch/phase04-cpu/oracle.tsv` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-07-T2 | 7 | EVD-02, EVD-04 | T-04-19, T-04-20, T-04-21 | T-04-SC; Every invalid input fails its specific assertion before dependent RAM reads, with a nonempty fault inventory and fresh positives. | Behavior/control expansion; suite.oracle_tracer, suite.oracle_controls, suite.symbol_controls, suite.cpu.lynx1 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(oracle_tracer\|oracle_controls\|symbol_controls\|cpu\.lynx1)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-08-T1 | 8 | EVD-02, EVD-04, EVD-05, TIM-03 | T-04-22, T-04-23 | T-04-SC; Both explicitly selected CPU profiles really complete, derive the observed model independently and compare the complete source-generated result set. | End-to-end tracer; suite.cpu.lynx1, suite.cpu.lynx2 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.cpu\.(lynx1\|lynx2)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-08-T2 | 8 | EVD-02, EVD-04, EVD-05, TIM-03 | T-04-22, T-04-23 | T-04-SC; Each seeded error reaches its intended validator failure with exact status/message and positive controls restore clean behavior. | Behavior/control expansion; suite.cpu.model_controls, suite.cpu.bound_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.cpu\.(model_controls\|bound_controls)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-09-T1 | 9 | EVD-02, EVD-04, EVD-05, TIM-02, TIM-03 | T-04-24, T-04-25 | T-04-SC; Both timer runs truly finish with independently observed model and source-generated 16-result comparisons; unused buffer bytes never count as tests. | End-to-end tracer; suite.timers.lynx1, suite.timers.lynx2 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(lynx1\|lynx2)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-09-T2 | 9 | EVD-02, EVD-04, EVD-05, TIM-02, TIM-03 | T-04-24, T-04-25 | T-04-SC; Wrong model, bad 16/18 extents and invalid alternatives fail their intended assertions; generated valid alternatives remain intact. | Behavior/control expansion; suite.timers.model_controls, suite.timers.result_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(model_controls\|result_controls)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-10-T1 | 10 | EVD-03, EVD-02, EVD-04, EVD-05, TIM-04 | T-04-26, T-04-27, T-04-28 | T-04-SC; Both real suites fail completion despite time reaching minimum and oracle-like initial RAM, with proof no test instruction ran. | End-to-end tracer; suite.cpu.no_run, suite.timers.no_run | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(cpu\|timers)\.no_run$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-10-T2 | 10 | EVD-03, EVD-02, EVD-04, EVD-05, TIM-04 | T-04-26, T-04-27, T-04-28 | T-04-SC; Each missing/stale/zero/premature execution fact fails its intended assertion and no empty case inventory can appear green. | Behavior/control expansion; suite.completion_controls, suite.selection_controls | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(completion_controls\|selection_controls)$'` | Wave0 generator/suite executable/modes; existing pinned toolchain | pending |
| 04-11-T1 | 11 | EVD-05, TIM-01, TIM-02, TIM-03 | T-04-29, T-04-30, T-04-31 | T-04-SC; Both successfully built semantic mutants fail their intended independent assertion, and fresh clean positives pass before/after each. | End-to-end tracer; timer.fault.done, timer.fault.reset_done | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^timer\.fault\.(done\|reset_done)$'` | Wave0 isolated fault driver; existing core/API guest cases | pending |
| 04-11-T2 | 11 | EVD-05, TIM-01, TIM-02, TIM-03 | T-04-29, T-04-30, T-04-31 | T-04-SC; Each phase/order/IRQ mutant builds, triggers its specific independent test and leaves clean source/tests passing afterward. | Behavior/control expansion; timer.fault.phase, timer.fault.order, timer.fault.irq | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^timer\.fault\.(phase\|order\|irq)$'` | Wave0 isolated fault driver; existing core/API guest cases | pending |
| 04-12-T1 | 12 | TIM-01, TIM-02, TIM-03, TIM-04, EVD-02, EVD-03, EVD-04, EVD-05 | T-04-32, T-04-33, T-04-34 | T-04-SC; The canonical full local command accounts for every exact test and artifact; docs reproduce actual observations and preserve all owner stops. | End-to-end tracer; tests/inventory/fast.txt (entire exact inventory) | `sh scripts/check.sh` | Existing full check; new qualification record | pending |
| 04-12-T2 | 12 | TIM-01, TIM-02, TIM-03, TIM-04, EVD-02, EVD-03, EVD-04, EVD-05 | T-04-32, T-04-33, T-04-34 | T-04-SC; Accepted fixes are tested, a self-contained Ubuntu24.04 arm64 run and exact reviewed-head CI are green with full inventory, and every remaining owner/evidence gate is explicit. | Behavior/control expansion; tests/inventory/fast.txt (entire exact inventory) | `sh scripts/check.sh` | Existing full check; new qualification record | pending |

Every command has an immediate fails_when sibling in its PLAN. Direct modes must report nonzero cases; CTest uses --no-tests=error. Full-gate rows refer to every exact ID in tests/inventory/fast.txt rather than an invented aggregate test ID. Final local/Ubuntu24.04 arm64/exact-head CI obligations remain separate from a command merely returning zero.

## Coverage Flags

- All 18 explicit edge resolutions are planned as must_haves.truths, with tests owned by the corresponding tasks.
- TIM-02 and EVD-04 are unclassified, unresolved flagged assumptions; transition/probe tests are concrete, and software checks do not auto-resolve hardware or taxonomy uncertainty.
- Both recalled prohibitions remain unresolved/flagged-unverified in must_haves.prohibitions, with no fabricated wired-check descriptor.
- Readiness is canonical: expired priced-cycle readiness normalizes to guest time; ownership hold stays separate and the public CPU_STATE remaining-cost byte cannot underflow. Held/in-flight restore checks are in Plans05/06.
- Full four-source audit and dependency graph are embedded in 04-12-PLAN.md. No source item is silently deferred.

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
