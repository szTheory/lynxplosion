---
phase: "06"
slug: "audio-and-playable-checkpoint"
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-12"
---

# Phase 6 — Validation Strategy

Coverage reconciliation against final qualified source `bc943eecc7a498b4405a150961c036e6d911e60e`.
Independent assertion-level audit and repair re-reviews found no remaining
scoped automation gap. All24 planned task obligations have recorded passing
evidence; the complete native/GNU/hosted canonical inventories account for246
rows with244 passes and only two declared SERCTL skips. See06-QUALIFICATION.md
for exact identities, failed-attempt retention and limits. This establishes the
scoped software contract, not physical/model accuracy or global EVD-02 closure.

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

Commands remain task-specific. Evidence reports summarize the observed scoped runs
at their owning task commits. Final source also passes the complete inventory
in all three required environments. CTest selectors use `--no-tests=error`; exact
registration and sorted inventory are enforced. Lifecycle, audio-boundary and
fixture-symlink obligations are closed separately below.

| Task | Requirements | Automated command | Threat Ref | Evidence |
|---|---|---|---|---|
| 06-01-1 | AUD-02, HB-03, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_tracer\|frame_tracer\|snapshot_ppm\|silence_wav)$'` | T-06-01 | PASS at Plan 01 task commit: 4/4 on AppleClang and GNU; positive tracer/audio/image/WAV assertions and four checker corruption controls. See 06-01-SUMMARY.md. |
| 06-01-2 | AUD-02, HB-03, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(api\.(header_selfcontained\|abi_symbols\|snapshot_roundtrip\|snapshot_refusal\|snapshot_continue\|timer_snapshot_controls\|video_snapshot\|video_snapshot_refusal)\|cpu\.vectors\.(lynx1\|lynx2\|none)\|diag\.(result_symbol\|result_controls)\|det\.(golden\|golden\.self_test\|video_paths\|video_sensitivity)\|timer\.fault\.done\|video\.fault\.dma_cost_zero)$'` | T-06-02 | PASS at Plan 01 task commit: 19/19 on AppleClang and GNU; ABI/raw-reader/copied-core regressions passed. Final combined qualification is recorded in06-QUALIFICATION.md. See 06-01-SUMMARY.md. |
| 06-02-1 | EVD-06 | `sh scripts/build-suites.sh --out build/check/scratch/phase06-suite-build && sh scripts/check-manifests.sh` | T-06-03 | PASS after repair: builder/manifest command plus `hygiene.manifests` and `hygiene.manifests.self_test` passed in the 14-case native/GNU follow-up. New controls reject listed outside-file and directory links, unlisted/dangling links, and symlinked fixture/manifests roots or manifest files before reading external bytes. See `scripts/check-manifests.sh:61–105,188–205,223–226`, `tests/gates/manifests_controls.sh:338–403`, and 06-REVIEW-FIX.md. |
| 06-02-2 | EVD-06 | `sh scripts/generate-suite-oracle.sh --source build/pins/lynx-tests --include-audio --out build/check/scratch/phase06-audio-oracle.txt && sh tests/gates/audio-script-controls.sh && shellcheck scripts/build-suites.sh scripts/generate-suite-oracle.sh tests/gates/audio-script-controls.sh` | T-06-04 | PASS on macOS/GNU. Extended oracle emitted 18 audio + 18 audio2 rows (including audio2 row 9 alternative 30\|34); default remains CPU/timer-only. The explicit `--include-audio` is required to match the extended oracle behavior. See 06-02-SUMMARY.md. |
| 06-03-1 | AUD-01, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(registers\|lfsr\|integrator\|counter)$'` | T-06-05 | PASS on AppleClang/GNU; all four channels' registers, LFSR, counters, signed integration/clipping passed with independent expected values. See 06-03-SUMMARY.md. |
| 06-03-2 | AUD-01, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(stereo\|mix\|registers\|integrator)$'` | T-06-06 | PASS on AppleClang/GNU; all eight mute routes and exact wide signed mix limits/cancellation passed. See 06-03-SUMMARY.md. |
| 06-04-1 | AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(links\|ring_zero)\|api\.(timer_links\|timer_unavailable\|timer_snapshot_controls\|video_snapshot)\|diag\.result_controls\|cpu\.vectors\.(none\|lynx1\|lynx2))$'` | T-06-07 | PASS in scoped AppleClang/GNU runs; all ring edges including A3→T1 and zero-reload bounded debt/snapshot continuation have exact assertions. Plan 04 also records its earlier broad CTest failure (18 failures, 2 skips) and focused follow-up repairs; no broad rerun is claimed. See 06-04-SUMMARY.md. |
| 06-04-2 | AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(cadence\|order\|ring_zero)\|api\.(scheduler_order\|scheduler_watch\|scheduler_limits\|frame_edges))$'` | T-06-08 | PASS in scoped AppleClang/GNU runs; post-review `audio.order` also proves a CPU CTLA enable at tick 400 has no same-tick natural clock and first clocks at 416, including capacity retry, watched resume, explicit CTLB strobe, and snapshot/restore suffix equality. All 14 affected native/GNU tests passed; see 06-REVIEW-FIX.md and `tests/audio/test_audio.c:845–1110`. |
| 06-05-1 | AUD-02, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(span_refusal\|span_alias)$'` | T-06-09 | PASS 2/2 AppleClang/GNU: full-range pointer/null/alignment/wrap/overlap/capacity refusal preserves machine, result, descriptor and sentinels; one-past-end adjacency is accepted. See 06-05-SUMMARY.md. |
| 06-05-2 | AUD-02, CON-01, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(capacity\|stops\|span_refusal)\|api\.(frame_stops\|frame_refusal\|input_refusal\|reset_power_on\|load_refusal_unchanged))$'` | T-06-10 | PASS after repair: `audio.stops` now starts lifecycle transitions from public A/V progress with nonzero PCM, ordinal 1, channel CLOCK events, and deferred ring debt. Accepted reset/load match fresh state; deinit refusal preserves machine/result/descriptors/spans; same-allocation reinit uses only new spans. `audio.stops` passed in the focused 14-case native/GNU batch. See `tests/audio/test_audio.c:2510–2725` and 06-REVIEW-FIX.md. |
| 06-06-1 | DET-05, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(snapshot\|snapshot_refusal)\|api\.(snapshot_refusal\|snapshot_roundtrip\|timer_snapshot_controls\|video_snapshot_refusal))$'` | T-06-11 | PASS: audio snapshot/refusal selector passed on AppleClang and GNU; format-7 audio state, validation-before-apply and exact-build compatibility are covered by fieldwise snapshot and mismatch controls. See 06-06-SUMMARY.md and snapshot-audio.md. |
| 06-06-2 | DET-05, AUD-02 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(det\.(audio_paths\|audio_sensitivity\|golden\|golden\.self_test\|video_paths\|video_sensitivity)\|api\.header_selfcontained)$'` | T-06-12 | PASS 7/7 AppleClang/GNU at Plan 06 task commit; full/sliced/watched/interleaved/two-instance/restore streams and empty/single/equal-tick suffixes use the source-built av_probe fixture. See 06-06-SUMMARY.md and tests/det/test_det.c. |
| 06-07-1 | HB-03, DET-05, CON-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_edges\|av_restore\|av_tracer\|frame_tracer\|snapshot_ppm\|silence_wav)$'` | T-06-13 | PASS: scoped Clang set passed 11/11; core plus in-memory host checkpoint, edge and suffix variants are exercised. See 06-07-SUMMARY.md. |
| 06-07-2 | HB-03, DET-05, CON-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_replay\|av_response_controls\|av_tracer\|frame_tracer\|snapshot_ppm\|silence_wav)$'` | T-06-14 | PASS on AppleClang/GNU: replay and response controls passed; bundle checker validates exact file set, applied-input/progress semantics and all stream hashes/extents. Same-core comparisons are reported as repeatability only. See 06-07-SUMMARY.md. |
| 06-08-1 | CON-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_controls\|replay_controls\|refusal_controls)$'` | T-06-15 | PASS in final scoped native/GNU runs: malformed inputs, in-memory checkpoint identity/offset/prefix and refusal sentinels pass; the plan expressly creates no persisted checkpoint file interface. See 06-08-SUMMARY.md. |
| 06-08-2 | CON-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_publication_controls\|av_replay\|frame_publication_controls)$'` | T-06-16 | PASS: normal independent processes raced for one destination; one winner/one refusal, write/close/interruption/foreign-marker/inode/symlink sentinels all passed in final native/GNU selectors. See 06-08-SUMMARY.md. |
| 06-09-1 | EVD-06, AUD-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(audio\|audio2)\.observation\.(lynx1\|lynx2)$'` | T-06-17 | PASS: fresh audio/audio2 guests on lynx1 and lynx2 each retained 18 bytes and 18 source rows, matching unchanged alternatives and independent model evidence. See 06-09-SUMMARY.md. |
| 06-09-2 | EVD-06, AUD-01 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(audio\.controls\|audio2\.controls\|audio\.source_controls\|selection_controls\|cpu\.(lynx1\|lynx2)\|page_mode\.(observation\.(lynx1\|lynx2)\|controls)\|timers\.(observation\.(lynx1\|lynx2)\|strict_comparison_controls\|video_revisit\|video_revisit_controls))$'` | T-06-18 | PASS: audio/audio2 24/26 named negative controls, source controls and exact five-suite selection passed; only the two inherited page-mode SERCTL observations skipped. Timer row 14 remains legacy 0C/source 0D and video-policy1 0F/source 0D. Selection control is separately listed here because the plan selector omitted it although the summary records it as run. See 06-09-SUMMARY.md. |
| 06-10-1 | EVD-05, AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.fault\.(lfsr_tap\|integrator_clip\|link_a1\|link_a2\|link_a3\|ring_return)$'` | T-06-19 | PASS: six private production-source mutants built successfully, exited 1 at only their intended assertions, with clean before/after brackets and identity checks; positive source cases passed 7/7 on both builds. See 06-10-SUMMARY.md. |
| 06-10-2 | EVD-05, AUD-01, AUD-02, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.fault\.(stereo_left\|stereo_right\|cadence_399\|snapshot_phase)$'` | T-06-20 | PASS: four additional actual mutants built and failed only exact assertions; restored-suffix mutant proved snapshot restore before comparison; clean brackets/identities and positive source cases passed 10/10 on AppleClang/GNU. See 06-10-SUMMARY.md. |
| 06-11-1 | PERF-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_benchmark\|av_replay)$'` | T-06-21 | PASS AppleClang/GNU: five fresh, same-build runs completed 1,248,256 guest ticks (1,253,985 is only the window bound) with 22 frames, 3,120 PCM frames, 1,706 events, 12 inputs and 13 identical stream digests. Clock brackets only public advancement calls. See 06-11-SUMMARY.md and docs/performance.md. |
| 06-11-2 | PERF-01, HB-03, DET-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_benchmark_controls\|av_benchmark)$'` | T-06-22 | PASS AppleClang/GNU: 28 malformed benchmark-report mutations rejected at named parser diagnostics; real no-run guest refused before output. Timing spans validated for public API advance, encoding, assembly, publication and replay total. CI timing is recorded in06-QUALIFICATION.md. See 06-11-SUMMARY.md. |
| 06-12-1 | AUD-01, AUD-02, HB-03, DET-05, EVD-06, PERF-01, CON-01, EVD-05 | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(audio\.(registers\|lfsr\|integrator\|counter\|stereo\|mix\|links\|ring_zero\|cadence\|order\|span_refusal\|span_alias\|capacity\|stops\|snapshot\|snapshot_refusal)\|det\.(audio_paths\|audio_sensitivity)\|consumer\.(av_replay\|av_response_controls)\|core\.(no_float\|includes\|nm_undefined\|nm_globals)\|api\.(header_selfcontained\|abi_symbols))$'` | T-06-23 | PASS: exact26-case group passed; independent54-file review iterations2–4 clean, all repaired seams pass focused checks and final canonical inventories. See06-REVIEW.md and06-QUALIFICATION.md. |
| 06-12-2 | AUD-01, AUD-02, HB-03, DET-05, EVD-06, PERF-01, CON-01, EVD-05 | `sh scripts/check.sh` | T-06-24 | PASS: canonical native/GNU gates and exact-source hosted run43448772929;246 rows accounted once,244 passes,two declared SERCTL skips,no failures. Timing and retained limits in06-QUALIFICATION.md. |

Plan12's planned focused group and independent review are complete. The final
native/GNU canonical gates and hosted CI pass on the exact source recorded in
06-QUALIFICATION.md; they include prior-phase regression tests. No duplicate
broad gate is required for final report prose. Historical scoped results retain
their own source/build identities.

## Assertion-level evidence map

The following is the source-level check behind the scoped result rows above;
registration alone is not counted as coverage.

| Requirement/seam | Actual assertion evidence | Scoped proof |
|---|---|---|
| AUD-01 channel state | `tests/audio/test_audio.c`: `case_registers` asserts `audio.registers.mapped-read`, `mapped-value`, `peek-pure`, and `opcode-peek-route`; `case_lfsr` asserts tap maps/channel output; `case_counter` and `case_integrator` assert reload/DONE and signed output/clipping; `case_links` checks each channel link including A3→T1. | 06-03/04 and private mutants in 06-10; exact selectors in task rows. |
| AUD-01 stereo and mix | `case_stereo` asserts independent per-channel left/right mute routes and that muting preserves LFSR/event evolution; `case_mix` asserts positive/negative extrema and cancellation using exact signed results. | 06-03 and stereo-side mutants in 06-10. |
| AUD-02 cadence, events and bounded ring | `case_cadence` asserts origin 400/ordinal 0 and half-open 399/400/401 plus 799/800/801; `case_order` checks sample-before-CPU, exact-target debt, stable same-tick events and resume; `case_ring_zero` checks zero-capacity links, deferred deadline, debt conservation, high-word carry/overflow refusal and snapshot continuation. | 06-04 focused selectors; named cadence/link/ring mutants 06-10. |
| AUD-02 public span refusal/ownership | `check_av_refusal_unchanged` asserts status and unchanged machine, descriptor, result and payload bytes; `case_span_refusal`/`case_span_alias` exercise invalid, overlapping and adjacent spans; `case_capacity` proves capacity frontier/resume and separate live-instance spans. `check_audio_lifecycle` now exercises reset/load/deinit/reinit after public A/V progress. | 06-05 task proof plus post-review `audio.stops` in the 14-case native/GNU batch. |
| DET-05 audio streams/checkpoint | `tests/det/test_det.c:2768–2929` (`case_audio_paths`) compares whole/coarse/one-tick/irregular/watched/interleaved streams and guest state, requires nonempty input audio/events, checks input response, sample/event suffixes and restored/deferred-ring suffixes. `case_audio_sensitivity` independently changes PCM/event/input/frame/progress records. | 06-06 focused det selector and 06-07 consumer replay selectors. |
| HB-03 / CON-01 consumer response and artifacts | `tests/consumer/av_driver.cmake` `consumer.av_replay` checks sequential and interleaved output; `consumer.av_response_controls` rejects no-run, over-bound, silent/stale/truncated WAV and frame/event mutations. `tests/consumer/check_artifacts.c` validates the expected 13-file bundle, source/input identity, applied-input/progress semantics, output extents and per-stream hashes. | 06-01/07 plus refusal and bundle-control selectors in 06-08; no persisted host-checkpoint file API is claimed. |
| EVD-06 source observations and fixture scope | `suite.audio.observation.lynx1/lynx2` and `suite.audio2.observation.lynx1/lynx2` compare all 18 source rows to the explicit alternatives; suite/oracle/source/selection negatives remain registered. `tests/gates/manifests_controls.sh` now rejects listed outside links, linked directories, unlisted/dangling links and symlinked roots/manifests before outside bytes are read. | 06-02/09 scoped observations and post-review `hygiene.manifests(.self_test)` passed; the two page-mode SERCTL observations remain the only documented skips. |
| EVD-05 semantic faults | `tests/audio/test_audio.c` fault cases and their private production-source variants assert each named intended behavior; each mutant must build, exit 1 at its one expected assertion, and pass clean before/after identity brackets. | 10 fault selectors passed on the scoped AppleClang/GNU runs (06-10). |
| PERF-01 equal-work report | `tests/consumer/av_driver.cmake` validates five fresh runs, shared build/input/environment/stream identity, equal actual work and digests, separate API/encoding/assembly/publication/total durations; controls reject 28 malformed reports and a real no-run guest. | 06-11 benchmark and benchmark-controls selectors; CI timing is recorded in06-QUALIFICATION.md. |

## Post-review automated gap closure

The three issues from the initial coverage/code review are now closed by
behavioral tests and the scoped follow-up batch. Their former failures were
software defects or missing assertions; none is a human-only acceptance item.

| Initial gap | Current source/test proof | Observed follow-up result |
|---|---|---|
| Active A/V lifecycle through reset, accepted reload, deinit refusal and same-allocation reinit (CONTEXT D-06/D-09; Plan 06-05-2) | `tests/audio/test_audio.c:2510–2725` `check_audio_lifecycle` drives public AV with public guest channel writes and a T1 strobe. Assertions `audio.lifecycle.active-nonzero-sample`, `active-events`, `active-clock`, `active-ring-debt`, `reset-fresh-byte-equivalence`, `load-fresh-byte-equivalence`, `deinit-av-refusal`, `deinit-instance-unchanged`, `deinit-result-unchanged`, `deinit-descriptor-unchanged`, `deinit-caller-spans-unchanged`, `same-allocation-fresh-equivalence`, and `old-span-not-retained` verify nondefault sample ordinal/event/debt state, reset/load clearing, transactional deinit refusal, and new-span ownership. | `audio.stops` passed in the 14-case native and GNU/Linux batch. |
| Natural clock at the same tick as CPU enable of an inactive channel (AUD-01/AUD-02; D-01/D-04) | `core/machine.c` marks successful CPU device writes as applied at the grant tick. `tests/audio/test_audio.c:845–1110` adds `check_cpu_enable_edge`, capacity-retry, watch-stop, and CTLB-strobe cases. `audio.order.cpu-enable.boundary-no-work` and `first-clock-416` prove CTLA enable at tick 400 does not produce a natural clock until 416; restore event/PCM equality, output-capacity retry, watched continuation, and explicit strobe remain checked. | `audio.order` passed in the 14-case native and GNU/Linux batch. The fix report records the pre-fix assertion failing with observed 400 versus expected 416. |
| Fixture manifest symlink escape and inventory omission (EVD-06) | `scripts/check-manifests.sh:61–105,188–205,223–226` rejects symlinks before manifest reads and checks entry paths/inventory. `tests/gates/manifests_controls.sh:338–403` includes listed outside-file and directory links, unlisted and dangling links, plus symlinked fixtures root/manifests directory/manifest file; outside sentinels must remain unchanged and external manifest bytes must not appear in output. | `hygiene.manifests` and `hygiene.manifests.self_test` passed in the focused batch. The fix report records that the listed outside-file symlink succeeded under the old gate and is now refused. |

The recorded focused batch passed 14/14 cases on native macOS and 14/14 on the
existing GNU/Linux container at this source HEAD: `audio.order`,
`audio.capacity`, `audio.stops`, `audio.snapshot`, `audio.snapshot_refusal`,
`det.audio_paths`, `det.audio_sensitivity`, four Lynx I/II audio/audio2 suite
observations, `suite.timers.pending_controls`, `hygiene.manifests`, and
`hygiene.manifests.self_test`. ShellCheck 0.11.0 and 0.9.0 plus `sh -n` passed
for the modified shell scripts. Durable details are in 06-REVIEW-FIX.md and
the native/GNU logs recorded there. This is scoped evidence, not the exact
Plan 12 focused group or final canonical/CI qualification.


## Wave 0 Requirements

- [x] Add the thin A/V tracer's exact test registrations and inventory entries.
- [x] Build unchanged audio/audio2 source through owned wrappers; validate
  source-derived oracle extents and audio2's existing alternative before guest runs.
- [x] Add new audio tests and private semantic controls in their owning plans.
- [x] Update all raw snapshot-layout consumers and explicit copied-core source
  lists with the audio structure change, retaining historical assertions.
- [x] Fill the scoped per-task/threat/command map from owning-plan evidence;
  final-source and Plan12 evidence are complete in06-QUALIFICATION.md.

Existing test infrastructure is sufficient; no framework installation is needed.

## Manual-Only Verifications

No human-only verification is used to close an automatable software behavior.
The lifecycle, divider-edge and fixture-symlink cases have automated repairs
and focused passing evidence. Physical hardware, subjective listening, release,
licence, and O1 model-baseline decisions retain their owner boundaries and are
separate from this phase's software acceptance evidence.

## Validation Sign-Off

- [x] Every task has runnable automated verification or a Wave 0 dependency.
- [x] No three consecutive tasks lack automated verification.
- [x] Lifecycle and review-triggered regression cases are registered and pass in the focused repair batch.
- [x] No watch-mode or desktop-focus requirement.
- [x] Feedback latency and slow cases measured in owning-plan summaries.
- [x] Independent coverage audit and final Plan12 evidence reconcile status and Nyquist compliance.

**Approval:** independently audited scoped coverage; final native/GNU/CI gates pass. No human software UAT remains.

## Planning shape, dependencies and source coverage audit

Discovery level 0: completed current RESEARCH/PATTERNS and concrete existing source seams provide sufficient evidence; no new library or external service is selected. No additional research fanout or framework installation. Graph context was unavailable because graphify is disabled; no graph claims are used. Agent-specific skill query returned no additional skills. Calibration: factor1, sample_count0, confidence low; estimates are raw context projections, not measured model usage.

There are 12 plans, 24 tasks and 11 waves. Wave1 contains 06-01 (production tracer plus immediate layout integration repairs) and 06-02 (source-only wrapper/oracle construction). They own disjoint source paths; 06-02 uses separate invocation-owned scratch and no shared core configure/build/registration. Wave2 06-03 consumes both; subsequent shared core/registry/inventory edits are sequential through 06-12 in wave11. No same-wave file overlap is permitted. Needs/creates are concrete: 01 creates public A/V spans and guest/consumer path; 02 creates unchanged source guests/oracles; 03 adds complete channel/stereo observations; 04 completes cyclic links/order; 05 adds refusal/frontier behavior; 06 adds full candidate/continuation proof; 07 adds full replay/checkpoint; 08 adds publication safety; 09 consumes source+device for exact suite observations; 10 consumes named behavior assertions for actual semantic mutants; 11 consumes full replay for equal-output timing; 12 consumes all reviewed evidence for final gates.

The tracer and its immediate structural repair are explicit bounded integration exceptions to the five-file heuristic, approved by the orchestrator. They list all touched paths, preserve a single channel/A-edge path, and front-load raw API/CPU-vector/diagnostic reader, canonical digest and both timer/video source-list repairs. Subsequent tasks implement concrete usable expansions or refusal/evidence refinements. Metadata registrations/inventories accompany their feature rather than becoming foundation-only slices.

| Source | Item | Coverage | Plans |
|---|---|---|---|
| GOAL | Public API input-responsive A/V checkpoint on one guest timeline | COVERED | 01, 04–08 |
| GOAL | Success1 four channels/stereo, captured audio/audio2 and named fault | COVERED | 03, 04, 09, 10 |
| GOAL | Success2 rational integer PCM/events and forbidden imports | COVERED | 01, 03–06, 10 |
| GOAL | Success3 real input-responsive PPM/WAV, separate repeatability streams | COVERED | 01, 06–08 |
| GOAL | Success4 all five pinned tier-A suite observations/exact skips | COVERED | 02, 09 |
| GOAL | Success5 named equal-output throughput, no optimization | COVERED | 11 |
| REQ | AUD-01 | COVERED | 03, 04, 09, 10; post-review inactive-channel divider-edge test |
| REQ | AUD-02 | COVERED | 01, 03–06, 10; post-review inactive-channel divider-edge test |
| REQ | HB-03 | COVERED | 01, 07, 08 |
| REQ | DET-05 | COVERED | 01, 04–08, 10–11 |
| REQ | EVD-06 | COVERED | 02, 09; post-review fixture symlink controls and gate |
| REQ | PERF-01 | COVERED | 11 |
| REQ | CON-01 final real A/V consumer | COVERED | 01, 05, 07, 08 |
| REQ | EVD-05 final audio faults | COVERED | 10 |
| RESEARCH | Core-device ownership, all channel registers/taps/status ambiguity | COVERED | 01, 03; inactive-audio divider edge has behavioral coverage after review |
| RESEARCH | Shared cyclic graph, finite work and serialized pulse debt | COVERED | 01, 04, 06 |
| RESEARCH | All mapped access paths including pure opcode peek | COVERED | 01, 03 |
| RESEARCH | Explicit lattice/mix/order precision, half-open targets | COVERED | 01, 03–05; exact inactive-channel enable ordering asserted after review |
| RESEARCH | Caller spans, transactional refusal and explicit partial frontier | COVERED | 01, 05 |
| RESEARCH | Reset/load/unload A/V lifecycle with nondefault audio state | COVERED | 05; public-progress reset/load/deinit/reinit assertions added and passed |
| RESEARCH | Immutable identity, fieldwise candidate and canonical digest | COVERED | 01, 06 |
| RESEARCH | All raw-layout readers and copied timer/video build lists early | COVERED | 01; repeated on any structural change |
| RESEARCH | Public-only replay, real independent WAV/raw/PPM checker | COVERED | 01, 07 |
| RESEARCH | Complete host cursor/prefix ownership and exclusive publication | COVERED | 07, 08 |
| RESEARCH | Unchanged pinned assembly wrappers and 18-row C oracles | COVERED | 02, 09 |
| RESEARCH | Fixture-manifest path containment and complete symlink-aware inventory | COVERED | 02; listed/unlisted/dangling/root/manifest symlink controls passed |
| RESEARCH | Native tool identity local, source/build recipe portable | COVERED | 01, 02, 09 |
| RESEARCH | Actual semantic mutants, clean brackets, no production switches | COVERED | 10 |
| RESEARCH | Five equal-output fresh benchmark samples/monotonic host time | COVERED | 11 |
| RESEARCH | GCC/Clang and ShellCheck0.9/0.11 early, final exact HEAD gate | COVERED | 01, 02, 09, 12; final native/GNU/CI exact-source qualification |
| RESEARCH | Security memory/input/capacity/state/publication controls | COVERED | 05, 06, 08, 12; threats in every plan |
| RESEARCH | Physical reset/link/mix/stereo unknowns remain unqualified | COVERED | 01, 04, 09, 12 |
| CONTEXT | D-01 documented four digital channels and their natural-clock enable boundary | COVERED | 01, 03; post-review `audio.order` asserts first natural clock at 416 after tick-400 enable |
| CONTEXT | D-02 complete bounded ring/deferred work | COVERED | 04, 06 |
| CONTEXT | D-03 40000/1 stereoS16, separate events/integer mix | COVERED | 01, 03–05 |
| CONTEXT | D-04 origin/order/half-open target, including inactive-audio enable at divider edge | COVERED | 01, 04, 05; post-review `audio.order` boundary, retry, watch, strobe and restore assertions |
| CONTEXT | D-05 none/legacy default/video optin/strict observations | COVERED | 01, 04, 09, 12 |
| CONTEXT | D-06 bounded caller spans/no retained FIFO, including reset/deinit ownership edges | COVERED | 01, 05, 07, 08; lifecycle refusal and same-allocation reinit span assertions |
| CONTEXT | D-07 identity/snapshot/digest and host checkpoint separation | COVERED | 01, 06, 07 |
| CONTEXT | D-08 preserved old tracers/separate original complete bundle | COVERED | 01, 07, 08 |
| CONTEXT | D-09 every continuation/session/boundary/failure path, including reset/load/unload | COVERED | 04–08; lifecycle subcase added to `audio.stops` and passed |
| CONTEXT | D-10 unchanged audio assembly/oracles, no CRT | COVERED | 02, 09 |
| CONTEXT | D-11 both fresh profiles/model/return/progress/bounds | COVERED | 02, 09 |
| CONTEXT | D-12 all stereo bits and actual named semantic faults | COVERED | 03, 10 |
| CONTEXT | D-13 exact five suites, two SERCTL skips and strict timer limits | COVERED | 09, 12 |
| CONTEXT | D-14 thin tracer/early source/oracle/structural/portability | COVERED | 01, 02, 04, 09 |
| CONTEXT | D-15 five-run fixed digest baseline/no optimization | COVERED | 11 |
| CONTEXT | D-16 reuse distinct proof/final environment gates/exact CI | COVERED | 11, 12; final native/GNU canonical gates and exact-source CI pass |
| CONTEXT | D-17 existing authority and owner/rights/model stops | COVERED | 01, 02, 12 |

All source items have an owning plan and the three previously open automated
seams now have scoped passing evidence. `COVERED` here means the scoped
requirement has behavioral test evidence from its owning task; it does not mean
hardware accuracy is established. Final software qualification is separately
recorded in06-QUALIFICATION.md. CONTEXT deferred ideas, later milestone work and
RESEARCH out-of-scope items are excluded: host resampling/filter/live
presentation, UART/SERCTL unblock, physical qualification, shared LibFuturo
integration, optimization and release. Global EVD-02 remains open; other
completed requirement records are reconciled through the phase lifecycle.

## Spec-less edge/prohibition fallback

No SPEC exists; the enabled fallback uses the orchestrator's deterministic
11-item raw edge inventory. Six classified edges have explicit predicates.
Five unclassified rows remain flagged assumptions in that planning inventory;
they are not a direction to substitute human review for automatable behavior.
Their scoped software evidence is mapped below. The three initial audit/review
gaps have dedicated automated assertions in the closure section above; they are
not new entries fabricated into the 11-item inventory.

| Requirement/category | Status | Exact predicate/evidence target |
|---|---|---|
| AUD-02 boundary | COVERED | `audio.cadence` asserts 399/400/401 and 799/800/801 half-open targets, ordinal 0 at tick 400, and one-time samples; post-review `audio.order` asserts the tick-400 enable write does not cause a natural clock until tick 416 and checks deferred continuation. |
| AUD-02 precision | COVERED | `audio.mix` asserts exact signed wide-sum extrema/cancellation; cadence checks sample ordinal/tick limits. |
| CON-01 concurrency | COVERED | `consumer.av_publication_controls` launches normal independent publishers and asserts exactly one complete winner; loser cannot overwrite/delete; existing per-instance tests assert isolated state. |
| EVD-05 adjacency | COVERED | `audio.fault.cadence_399` makes adjacent sample/target assertions independently fail at their intended assertion. |
| EVD-05 empty | COVERED | `audio_paths` asserts empty/single suffixes and exact record extents; named mutants cannot pass on a missing record. |
| EVD-05 ordering | COVERED | `audio.order` checks equal-tick sequence; `audio.fault.snapshot_phase` checks restored continuation before exact suffix comparison. |
| AUD-01 unclassified | COVERED for scoped software behavior; assumption flag retained | `audio.registers`, `lfsr`, `counter`, `integrator`, `links`, `stereo`, and `mix`, plus fresh source-suite observations, named faults, and the post-review inactive-channel divider-edge test. No hardware-accuracy inference. |
| HB-03 unclassified | COVERED for scoped software behavior; assumption flag retained | Source-built `av_probe` input-response assertions require non-silent PCM and changed guest progress/frames; consumer replay checker validates complete output. |
| DET-05 unclassified | COVERED for scoped software behavior; assumption flag retained | `audio_paths`/`audio_sensitivity` compare separate streams and continuation; consumer bundle hashes streams independently. Same-core agreement is repeatability only. |
| EVD-06 unclassified | COVERED for scoped software behavior; assumption flag retained | Four audio/audio2 lynx1/lynx2 runs, 18 expected source rows per profile, controls, provenance, exact selection, and post-review fixture symlink containment controls. Page-mode-only skips remain visible. |
| PERF-01 unclassified | COVERED for scoped software behavior; assumption flag retained | Five equal-work/digest runs, public-advance-only host monotonic timing plus encoding/assembly/publication/total intervals, parser mutations and real no-run refusal. |

No-silent-drop arithmetic remains 11 raw edge items = 6 explicitly classified
predicates + 5 surfaced flagged assumptions. The coverage labels above refer to
observed automation; the unresolved assumption flags are retained as provenance,
not as additional human-only acceptance criteria. The audit's three initial
automation obligations (lifecycle coverage, natural-clock enable behavior,
manifest symlink containment) are closed by the assertion map above, independently
of the preserved planning inventory.

Prohibition recall considered per requirement: silent/missing data, wrong channel,
inaccurate claims, unduly broad model claims, copyrighted assets, emulator-source
exposure, physical testing, live focus-taking output, resampling/optimization
scope creep, and release/shared-host authority. Per-requirement emphasis was
AUD-01 hardware/implementation exposure; AUD-02 hidden analog/host-rate claims;
HB-03 misleading playable/rights claims; DET-05 repeatability presented as
accuracy; EVD-06 suite/model pass without O1; PERF-01 misleading unmatched
throughput; CON-01 unapproved distribution/content; EVD-05 false mutant
detection. Precision dropped normal buffer/arithmetic/empty/order/import
correctness into ordinary tests/review rather than duplicating them as values
prohibitions. General memory/path controls are mapped to threat controls, but
the discovered fixture-symlink containment case is closed by the repaired gate
and its seeded controls above.

Three bespoke kept statements were projected using the installed canonical
projectProhibitions serializer into 06-12 must_haves.prohibitions, initially with
unresolved status and no wired check descriptor; Plan12 now records explicit
verified security judgments for each statement: no physical/suite-qualification misrepresentation; no unapproved
assets/implementation-source exposure; no silent extension of
physical/release/distribution/shared-host authority. These remain scope and
authority boundaries; none replaces a test for an automatable software case.
No-silent-drop arithmetic: 3 kept = 3 authored descriptorless items.

## Capability contribution application

Read and applied active planner contributions in array order:
1. API coverage: input scope detector supplied detected:false. This phase extends its own native C ABI and integrates no external service; no fictitious endpoint matrix.
2. Assumption delta: supplied detected:false; no extra choice gate. Immutable audio policy extends the existing per-instance identity, and old frame/snapshot consumer modes remain explicit separate modes under D-08.
3. Schema gate: no ORM/database pattern or migration exists; snapshot format changes use the fieldwise serializer, not a database push.
4. Security: ASVS1/block-high applied; T-06-01…T-06-24 are unique, with repeated reserved T-06-SC only. Every plan has dispositions and specific mitigation/verification mapping.

The lexical page-mode UI match has no frontend evidence and is nonblocking: headless C/file consumer work needs no UI-SPEC or interactive UI task. No AI work is inferred. Any later API detector lexical positive must be evaluated against this native-only scope and recorded as a reasoned no-external declaration, not a fabricated integration.

## Audit boundary

Independent coverage inspection, targeted re-reviews and final exact-source
qualification close the scoped automation obligations. Status is validated,
Nyquist compliance true, and wave0 complete from observed evidence. No RED was
fabricated for an already-green coverage case. The six stale temporal assertions
and inherited clean mutant baseline were repaired without changing production
or source oracles. The frame replay matrix retains its complete input/profile/
handedness and assertion scope while avoiding repeated sensitivity interleaving.

All24 task rows are green at their scoped commits and final registered inventory.
No human-only software UAT remains. O1, rights and physical evidence remain owner
boundaries; strict timer disagreement and the two SERCTL skips remain visible
limits. Independent phase-goal verification is the next lifecycle gate.
