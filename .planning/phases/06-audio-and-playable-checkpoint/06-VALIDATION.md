---
phase: "06"
slug: "audio-and-playable-checkpoint"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-12"
---

# Phase 6 — Validation Strategy

Planning contract only: no audio runtime result is claimed. The planner fills
the exact per-task map from its final plans; execution records actual evidence.

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing C17 CMake/CTest and POSIX shell harnesses |
| Config | CMakeLists.txt, CMakePresets.json, tests/inventory/fast.txt |
| Focused command | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '<exact planned selector>'` |
| Canonical gate | `sh scripts/check.sh` |
| Runtime | Measure new cases; Phase 5 final hosted CTest was about 87 seconds, not a Phase 6 estimate |

## Sampling Rate

- After each task, run the exact semantic tests named by that task. Missing
  registration or an empty selector is failure, not skipped evidence.
- At structural changes, include existing API/CPU-vector/diagnostic snapshot
  consumers and timer/video copied-core fault builds immediately.
- Run focused GNU GCC/Clang and ShellCheck 0.9/0.11 portability checks early.
- After review and required fixes, run one final canonical gate per required
  environment and hosted CI on exact pushed HEAD. Reuse it as regression
  evidence; do not duplicate broad gates on unchanged source/prose changes.
- Max feedback target for focused cases: 30 seconds where practical; measure
  slow semantic fault cases rather than dropping distinct controls.

## Per-Task Verification Map

Exact finalized plan mapping below. These are executable acceptance commands, not claimed results. Each new case/standalone control is created in its owning task before verification; all CTest selectors use the grounded check preset and fail on an empty selection. The native ABI/buffer boundary uses a focused ASVS1 memory-safety threat map; ordinary correctness, float/import and provenance checks remain distinct.

| Task | Requirements | Automated command | Threat Ref | Evidence |
|---|---|---|---|---|
| 06-01-1 | AUD-02, HB-03, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_tracer|frame_tracer|snapshot_ppm|silence_wav)$'` | T-06-01 | Pending execution |
| 06-01-2 | AUD-02, HB-03, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(api\.(header_selfcontained|abi_symbols|snapshot_roundtrip|snapshot_refusal|snapshot_continue|timer_snapshot_controls|video_snapshot|video_snapshot_refusal)|cpu\.vectors\.(lynx1|lynx2|none)|diag\.(result_symbol|result_controls)|det\.(golden|golden\.self_test|video_paths|video_sensitivity)|timer\.fault\.done|video\.fault\.dma_cost_zero)$'` | T-06-02 | Pending execution |
| 06-02-1 | EVD-06 | `sh scripts/build-suites.sh --out build/check/scratch/phase06-suite-build && sh scripts/check-manifests.sh` | T-06-03 | Pending execution |
| 06-02-2 | EVD-06 | `sh scripts/generate-suite-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/phase06-audio-oracle.txt && sh tests/gates/audio-script-controls.sh && shellcheck scripts/build-suites.sh scripts/generate-suite-oracle.sh tests/gates/audio-script-controls.sh` | T-06-04 | Pending execution |
| 06-03-1 | AUD-01, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(registers|lfsr|integrator|counter)$'` | T-06-05 | Pending execution |
| 06-03-2 | AUD-01, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(stereo|mix|registers|integrator)$'` | T-06-06 | Pending execution |
| 06-04-1 | AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(links|ring_zero)|api\.(timer_links|timer_unavailable|timer_snapshot_controls|video_snapshot)|diag\.result_controls|cpu\.vectors\.(none|lynx1|lynx2))$'` | T-06-07 | Pending execution |
| 06-04-2 | AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(cadence|order|ring_zero)|api\.(scheduler_order|scheduler_watch|scheduler_limits|frame_edges))$'` | T-06-08 | Pending execution |
| 06-05-1 | AUD-02, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(span_refusal|span_alias)$'` | T-06-09 | Pending execution |
| 06-05-2 | AUD-02, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(capacity|stops|span_refusal)|api\.(frame_stops|frame_refusal|input_refusal|reset_power_on|load_refusal_unchanged))$'` | T-06-10 | Pending execution |
| 06-06-1 | DET-05, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(snapshot|snapshot_refusal)|api\.(snapshot_refusal|snapshot_roundtrip|timer_snapshot_controls|video_snapshot_refusal))$'` | T-06-11 | Pending execution |
| 06-06-2 | DET-05, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(det\.(audio_paths|audio_sensitivity|golden|golden\.self_test|video_paths|video_sensitivity)|api\.header_selfcontained)$'` | T-06-12 | Pending execution |
| 06-07-1 | HB-03, DET-05, CON-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_edges|av_restore|av_tracer|frame_tracer|snapshot_ppm|silence_wav)$'` | T-06-13 | Pending execution |
| 06-07-2 | HB-03, DET-05, CON-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_replay|av_response_controls|av_tracer|frame_tracer|snapshot_ppm|silence_wav)$'` | T-06-14 | Pending execution |
| 06-08-1 | CON-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_controls|replay_controls|refusal_controls)$'` | T-06-15 | Pending execution |
| 06-08-2 | CON-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_publication_controls|av_replay|frame_publication_controls)$'` | T-06-16 | Pending execution |
| 06-09-1 | EVD-06, AUD-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(audio|audio2)\.observation\.(lynx1|lynx2)$'` | T-06-17 | Pending execution |
| 06-09-2 | EVD-06, AUD-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(audio\.controls|audio2\.controls|audio\.source_controls|cpu\.(lynx1|lynx2)|page_mode\.(observation\.(lynx1|lynx2)|controls)|timers\.(observation\.(lynx1|lynx2)|strict_comparison_controls|video_revisit|video_revisit_controls))$'` | T-06-18 | Pending execution |
| 06-10-1 | EVD-05, AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.fault\.(lfsr_tap|integrator_clip|link_a1|link_a2|link_a3|ring_return)$'` | T-06-19 | Pending execution |
| 06-10-2 | EVD-05, AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.fault\.(stereo_left|stereo_right|cadence_399|snapshot_phase)$'` | T-06-20 | Pending execution |
| 06-11-1 | PERF-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_benchmark|av_replay)$'` | T-06-21 | Pending execution |
| 06-11-2 | PERF-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_benchmark_controls|av_benchmark)$'` | T-06-22 | Pending execution |
| 06-12-1 | AUD-01, AUD-02, HB-03, DET-05, EVD-06, PERF-01, CON-01, EVD-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(registers|lfsr|integrator|counter|stereo|mix|links|ring_zero|cadence|order|span_refusal|span_alias|capacity|stops|snapshot|snapshot_refusal)|det\.(audio_paths|audio_sensitivity)|consumer\.(av_replay|av_response_controls)|core\.(no_float|includes|nm_undefined|nm_globals)|api\.(header_selfcontained|abi_symbols))$'` | T-06-23 | Pending execution |
| 06-12-2 | AUD-01, AUD-02, HB-03, DET-05, EVD-06, PERF-01, CON-01, EVD-05 | `sh scripts/check.sh` | T-06-24 | Pending execution |

Plan12 Task2 additionally runs the identical canonical gate in the existing Linux clone, then checks successful ci.yml completion at the exact pushed source HEAD; both automated commands and their observable failure signals are in 06-12-PLAN.md. Do not run another broad gate for final report prose.


## Wave 0 Requirements

- [ ] Add the thin A/V tracer's exact test registrations and inventory entries.
- [ ] Build unchanged audio/audio2 source through owned wrappers; validate
  source-derived oracle extents and audio2's existing alternative before guest runs.
- [ ] Add new audio tests and private semantic controls in their owning plans.
- [ ] Update all raw snapshot-layout consumers and explicit copied-core source
  lists with the audio structure change, retaining historical assertions.
- [ ] Fill the final per-task/threat/command map after planning.

Existing test infrastructure is sufficient; no framework installation is needed.

## Manual-Only Verifications

All scoped software behaviors have automated verification. Physical hardware,
subjective listening, release, licence and model-baseline decisions retain their
existing owner boundaries and are not software acceptance tasks in this phase.

## Validation Sign-Off

- [ ] Every task has runnable automated verification or a Wave 0 dependency.
- [ ] No three consecutive tasks lack automated verification.
- [ ] Missing cases are registered before they can count as evidence.
- [ ] No watch-mode or desktop-focus requirement.
- [ ] Feedback latency and slow cases measured.
- [ ] Final validation audit sets status and Nyquist compliance from evidence.

**Approval:** pending execution and independent validation audit.

## Planning shape, dependencies and source coverage audit

Discovery level 0: completed current RESEARCH/PATTERNS and concrete existing source seams provide sufficient evidence; no new library or external service is selected. No additional research fanout or framework installation. Graph context was unavailable because graphify is disabled; no graph claims are used. Agent-specific skill query returned no additional skills. Calibration: factor1, sample_count0, confidence low; estimates are raw context projections, not measured model usage.

There are 12 plans, 24 tasks and 11 waves. Wave1 contains 06-01 (production tracer plus immediate layout integration repairs) and 06-02 (source-only wrapper/oracle construction). They own disjoint source paths; 06-02 uses separate invocation-owned scratch and no shared core configure/build/registration. Wave2 06-03 consumes both; subsequent shared core/registry/inventory edits are sequential through 06-12 in wave11. No same-wave file overlap is permitted. Needs/creates are concrete: 01 creates public A/V spans and guest/consumer path; 02 creates unchanged source guests/oracles; 03 adds complete channel/stereo observations; 04 completes cyclic links/order; 05 adds refusal/frontier behavior; 06 adds full candidate/continuation proof; 07 adds full replay/checkpoint; 08 adds publication safety; 09 consumes source+device for exact suite observations; 10 consumes named behavior assertions for actual semantic mutants; 11 consumes full replay for equal-output timing; 12 consumes all reviewed evidence for final gates.

The tracer and its immediate structural repair are explicit bounded integration exceptions to the five-file heuristic, approved by the orchestrator. They list all touched paths, preserve a single channel/A-edge path, and front-load raw API/CPU-vector/diagnostic reader, canonical digest and both timer/video source-list repairs. Subsequent tasks implement concrete usable expansions or refusal/evidence refinements. Metadata registrations/inventories accompany their feature rather than becoming foundation-only slices.

| Source | Item | Coverage | Plans |
|---|---|---|---|
| GOAL | Public API input-responsive A/V checkpoint on one guest timeline | COVERED | 01, 04–08 |
| GOAL | Success1 four channels/stereo, captured audio/audio2 and named fault | COVERED | 03, 04, 09, 10 |
| GOAL | Success2 rational integer PCM/events and forbidden imports | COVERED | 01, 03–06, 12 |
| GOAL | Success3 real input-responsive PPM/WAV, separate repeatability streams | COVERED | 01, 06–08 |
| GOAL | Success4 all five pinned tier-A suite observations/exact skips | COVERED | 02, 09, 12 |
| GOAL | Success5 named equal-output throughput, no optimization | COVERED | 11, 12 |
| REQ | AUD-01 | COVERED | 03, 04, 09, 10, 12 |
| REQ | AUD-02 | COVERED | 01, 03–06, 10, 12 |
| REQ | HB-03 | COVERED | 01, 07, 08, 11, 12 |
| REQ | DET-05 | COVERED | 01, 04–08, 10–12 |
| REQ | EVD-06 | COVERED | 02, 09, 12 |
| REQ | PERF-01 | COVERED | 11, 12 |
| REQ | CON-01 final real A/V consumer | COVERED | 01, 05, 07, 08, 12 |
| REQ | EVD-05 final audio faults | COVERED | 10, 12 |
| RESEARCH | Core-device ownership, all channel registers/taps/status ambiguity | COVERED | 01, 03 |
| RESEARCH | Shared cyclic graph, finite work and serialized pulse debt | COVERED | 01, 04, 06 |
| RESEARCH | All mapped access paths including pure opcode peek | COVERED | 01, 03 |
| RESEARCH | Explicit lattice/mix/order precision, half-open targets | COVERED | 01, 03–05 |
| RESEARCH | Caller spans, transactional refusal and explicit partial frontier | COVERED | 01, 05 |
| RESEARCH | Immutable identity, fieldwise candidate and canonical digest | COVERED | 01, 06 |
| RESEARCH | All raw-layout readers and copied timer/video build lists early | COVERED | 01; repeated on any structural change |
| RESEARCH | Public-only replay, real independent WAV/raw/PPM checker | COVERED | 01, 07 |
| RESEARCH | Complete host cursor/prefix ownership and exclusive publication | COVERED | 07, 08 |
| RESEARCH | Unchanged pinned assembly wrappers and 18-row C oracles | COVERED | 02, 09 |
| RESEARCH | Native tool identity local, source/build recipe portable | COVERED | 01, 02, 09 |
| RESEARCH | Actual semantic mutants, clean brackets, no production switches | COVERED | 10 |
| RESEARCH | Five equal-output fresh benchmark samples/monotonic host time | COVERED | 11 |
| RESEARCH | GCC/Clang and ShellCheck0.9/0.11 early, final exact HEAD gate | COVERED | 01, 02, 09, 12 |
| RESEARCH | Security memory/input/capacity/state/publication controls | COVERED | 05, 06, 08, 12; threats in every plan |
| RESEARCH | Physical reset/link/mix/stereo unknowns remain unqualified | COVERED | 01, 04, 09, 12 |
| CONTEXT | D-01 documented four digital channels | COVERED | 01, 03 |
| CONTEXT | D-02 complete bounded ring/deferred work | COVERED | 04, 06 |
| CONTEXT | D-03 40000/1 stereoS16, separate events/integer mix | COVERED | 01, 03–05 |
| CONTEXT | D-04 origin/order/half-open target | COVERED | 01, 04, 05 |
| CONTEXT | D-05 none/legacy default/video optin/strict observations | COVERED | 01, 04, 09, 12 |
| CONTEXT | D-06 bounded caller spans/no retained FIFO | COVERED | 01, 05, 07, 08 |
| CONTEXT | D-07 identity/snapshot/digest and host checkpoint separation | COVERED | 01, 06, 07 |
| CONTEXT | D-08 preserved old tracers/separate original complete bundle | COVERED | 01, 07, 08 |
| CONTEXT | D-09 every continuation/session/boundary/failure path | COVERED | 04–08 |
| CONTEXT | D-10 unchanged audio assembly/oracles, no CRT | COVERED | 02, 09 |
| CONTEXT | D-11 both fresh profiles/model/return/progress/bounds | COVERED | 02, 09 |
| CONTEXT | D-12 all stereo bits and actual named semantic faults | COVERED | 03, 10 |
| CONTEXT | D-13 exact five suites, two SERCTL skips and strict timer limits | COVERED | 09, 12 |
| CONTEXT | D-14 thin tracer/early source/oracle/structural/portability | COVERED | 01, 02, 04, 09 |
| CONTEXT | D-15 five-run fixed digest baseline/no optimization | COVERED | 11 |
| CONTEXT | D-16 reuse distinct proof/final environment gates/exact CI | COVERED | 11, 12 |
| CONTEXT | D-17 existing authority and owner/rights/model stops | COVERED | 01, 02, 12 |

No source item is missing from the plan set. CONTEXT deferred ideas, later milestone work and RESEARCH out-of-scope items are excluded: host resampling/filter/live presentation, UART/SERCTL unblock, physical qualification, shared LibFuturo integration, optimization and release. Requirements remain globally unchecked during planning.

## Spec-less edge/prohibition fallback

No SPEC exists; the enabled fallback uses the orchestrator's deterministic 11-item raw edge inventory. Six defensible classified edges have explicit must-have truths below. Five unclassified rows remain unresolved flagged assumptions; they are not dismissed, auto-resolved, converted into invented generic backstops, or confused with missing source coverage.

| Requirement/category | Status | Exact predicate/evidence target |
|---|---|---|
| AUD-02 boundary | Resolved, explicit | 06-04 truths and audio.cadence/order:399/400/401, target work pending, ordinal0 at400, once-only continuation |
| AUD-02 precision | Resolved, explicit | 06-03 truth and audio.mix: wide signed sum×64 exact -32768/+32512, no rounding/wrap; 06-04 checked ordinal/tick limits |
| CON-01 concurrency | Resolved, explicit | 06-08 truth and consumer.av_publication_controls: exactly one complete winner, loser cannot overwrite/delete; per-instance caller serialization |
| EVD-05 adjacency | Resolved, explicit | 06-10 truth and audio.fault.cadence_399: adjacent sample/target edges remain distinct and intended exact assertion fails |
| EVD-05 empty | Resolved, explicit | 06-10 truth; empty/single suffix and record extents explicit; missing mutant/expected record cannot pass |
| EVD-05 ordering | Resolved, explicit | 06-10 truth and snapshot_phase fault: equal-tick stable sequence and complete suffix assertion |
| AUD-01 unclassified | Unresolved, flagged assumption | Deterministic taxonomy gave no shape; manual software review of 03/04/10 remains required, not automatic backstop |
| HB-03 unclassified | Unresolved, flagged assumption | Taxonomy gave no shape; manually review real guest/image/non-silent response evidence in07 |
| DET-05 unclassified | Unresolved, flagged assumption | Taxonomy gave no shape; manually review full stream/cursor continuation evidence in06/07 |
| EVD-06 unclassified | Unresolved, flagged assumption | Taxonomy gave no shape; manually review fresh source observation/inventory controls in09 |
| PERF-01 unclassified | Unresolved, flagged assumption | Taxonomy gave no shape; manually review equal-work/digest/clock boundaries in11 |

No-silent-drop arithmetic:11 raw edge items =6 authored explicit predicates +5 surfaced unresolved assumptions; zero fabricated backstops.

Prohibition recall considered per requirement: silent/missing data, wrong channel, inaccurate claims, unduly broad model claims, copyrighted assets, emulator-source exposure, physical testing, live focus-taking output, resampling/optimization scope creep, and release/shared-host authority. Per-requirement emphasis was AUD-01 hardware/implementation exposure; AUD-02 hidden analog/host-rate claims; HB-03 misleading playable/rights claims; DET-05 repeatability presented as accuracy; EVD-06 suite/model pass without O1; PERF-01 misleading unmatched throughput; CON-01 unapproved distribution/content; EVD-05 false mutant detection. Precision dropped normal buffer/arithmetic/empty/order/import correctness into ordinary tests/review rather than duplicating them as values prohibitions. Canon memory/path safety is covered by the threat review and secure-phase, not minted as bespoke prohibitions.

Three bespoke kept statements were projected using the installed canonical projectProhibitions serializer into 06-12 must_haves.prohibitions, with unresolved status and flagged-unverified rationale, and no wired check descriptor: no physical/suite-qualification misrepresentation; no unapproved assets/implementation-source exposure; no silent extension of physical/release/distribution/shared-host authority. Review must adjudicate these explicitly from actual artifacts; they cannot silently pass. No-silent-drop arithmetic:3 kept =3 authored descriptorless items.

## Capability contribution application

Read and applied active planner contributions in array order:
1. API coverage: input scope detector supplied detected:false. This phase extends its own native C ABI and integrates no external service; no fictitious endpoint matrix.
2. Assumption delta: supplied detected:false; no extra choice gate. Immutable audio policy extends the existing per-instance identity, and old frame/snapshot consumer modes remain explicit separate modes under D-08.
3. Schema gate: no ORM/database pattern or migration exists; snapshot format changes use the fieldwise serializer, not a database push.
4. Security: ASVS1/block-high applied; T-06-01…T-06-24 are unique, with repeated reserved T-06-SC only. Every plan has dispositions and specific mitigation/verification mapping.

The lexical page-mode UI match has no frontend evidence and is nonblocking: headless C/file consumer work needs no UI-SPEC or interactive UI task. No AI work is inferred. Any later API detector lexical positive must be evaluated against this native-only scope and recorded as a reasoned no-external declaration, not a fabricated integration.

## Planned structural validation

The planner runs GSD frontmatter/structure checks plus static path/decision/wave/task/failing-direction audits only. Runtime evidence remains pending. Estimates are12 calibrated raw projections with factor1 and low confidence from zero samples. Parent owns checked artifact commit and subsequent execution dispatch; this planner does not build, run tests, start containers, install packages, mutate state/config, commit or push.
