---
phase: 06-audio-and-playable-checkpoint
verified: 2026-11-13T15:38:52Z
status: passed
score: 45/45 must-haves verified
covered_files:
  - ".planning/phases/06-audio-and-playable-checkpoint/06-01-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-01-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-02-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-02-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-03-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-03-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-04-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-04-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-05-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-05-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-06-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-06-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-07-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-07-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-08-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-08-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-09-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-09-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-10-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-10-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-11-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-11-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-12-PLAN.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-12-SUMMARY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-QUALIFICATION.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-REVIEW-DISPOSITION.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-REVIEW-FIX.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-REVIEW.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-SECURITY.md"
  - ".planning/phases/06-audio-and-playable-checkpoint/06-VALIDATION.md"
  - "consumer/main.c"
  - "core/audio.c"
  - "core/internal.h"
  - "core/machine.c"
  - "core/snapshot.c"
  - "core/timer.c"
  - "docs/audio-faults.md"
  - "docs/audio.md"
  - "docs/av-replay.md"
  - "docs/claims/skeleton-repeatability.claim"
  - "docs/performance.md"
  - "docs/provenance.md"
  - "docs/public-suites.md"
  - "docs/snapshot-audio.md"
  - "docs/snapshot-video.md"
  - "fixtures/diagnostic/av_probe.cfg"
  - "fixtures/diagnostic/av_probe.s"
  - "fixtures/manifests/audio-tests.manifest"
  - "fixtures/manifests/audio2-tests.manifest"
  - "fixtures/manifests/av-probe.manifest"
  - "fixtures/suites/NOTICE"
  - "fixtures/suites/audio-wrapper.s"
  - "fixtures/suites/audio2-wrapper.s"
  - "include/lynxplosion.h"
  - "scripts/build-suites.sh"
  - "scripts/check-manifests.sh"
  - "scripts/generate-suite-oracle.sh"
  - "tests/api/test_api.c"
  - "tests/audio/fault_controls.cmake"
  - "tests/audio/test_audio.c"
  - "tests/audio/tests.cmake"
  - "tests/consumer/av_driver.cmake"
  - "tests/consumer/check_artifacts.c"
  - "tests/consumer/frame_driver.cmake"
  - "tests/consumer/tests.cmake"
  - "tests/cpu/test_cpu_vectors.c"
  - "tests/det/av_driver.cmake"
  - "tests/det/test_det.c"
  - "tests/det/tests.cmake"
  - "tests/diag/test_diag.c"
  - "tests/gates/audio-script-controls.sh"
  - "tests/gates/manifests_controls.sh"
  - "tests/golden/skeleton.digest"
  - "tests/inventory/fast.txt"
  - "tests/suites/run.cmake"
  - "tests/suites/test_public_suites.c"
  - "tests/suites/tests.cmake"
  - "tests/tests.cmake"
covered_digest: "v4:sha256:642e252c130114bfc39209b2d894b2042c7e6aeb889c95656121878f424d5d23"
behavior_unverified: 0
overrides_applied: 0
decision_coverage:
  honored: 17
  total: 17
  not_honored: []
---

# Phase 6: Audio and Playable Checkpoint Verification

**Phase Goal:** As a public API host, replay an original homebrew with input-responsive audio and video on one guest-tick timeline, reproduce an A/V checkpoint, and inspect the v1 public-suite inventory and throughput baseline without firmware.
**Verified:** 2026-11-13T15:38:52Z
**Status:** passed
**Re-verification:** No — initial verification

## User Flow Coverage

User story: As a public API host, replay an original homebrew with input-responsive audio and video on one guest-tick timeline, so that I can reproduce an A/V checkpoint and inspect its v1 public-suite inventory and throughput baseline without firmware.

| Step | Expected | Evidence | Status |
|---|---|---|---|
| Start a replay from the public host path | A project-authored, source-built guest and canonical input timeline run through the standalone public C API consumer. | consumer/main.c includes only lynxplosion.h for emulator access; CMake links lynx-consume to lynx_core. The source-built av_probe fixture and consumer.av_replay are in the final test inventory. | ✓ |
| Apply the recorded input edge | Guest progress, applied input, raw video and audio respond on the same guest-tick timeline. | consumer/main.c uses public input and A/V APIs; it refuses publication if released/pressed frame markers do not differ or if post-edge PCM is silent. consumer.av_replay and consumer.av_response_controls pass. | ✓ |
| Inspect the checkpoint bundle | A complete bundle contains raw and rendered frames, PCM/WAV, tick-stamped events, input/progress streams and a manifest binding identities and digests. | tests/consumer/av_driver.cmake runs the independent tests/consumer/check_artifacts.c validator on real output; consumer.av_replay and consumer.av_restore pass. The checkpoint host sidecar is in memory, paired with format-7 core state. | ✓ |
| Reproduce from the checkpoint | Restored output resumes after the owned prefix without replaying the edge or duplicating records. | consumer.av_restore, audio.snapshot, det.audio_paths and audio.order exercise restore, boundary, event and suffix behavior. | ✓ |
| Inspect the suite inventory and workload baseline | The host can inspect the exact five-suite inventory and a five-run digest-bound workload result without firmware. | docs/public-suites.md records suite observations and skips; docs/performance.md records the final-source five-run capture and separate CI costs. | ✓ |
| Story outcome | Reproduce an A/V checkpoint and inspect the public-suite inventory and throughput baseline without firmware. | The public consumer output is produced and independently checked by the registered integration cases; the source suite and benchmark rows below are covered by the exact-source qualification. | ✓ |

The phase is headless and its user flow is exercised by automated integration tests. There is no visual presentation, physical A/V device, external service, or subjective listening criterion in this goal.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | Four audio channels, their reload/LFSR/integrator behavior and stereo routes are emulated; authored tests cover channels 1–3 and stereo bits; source-suite results and a named fault control are captured. | ✓ VERIFIED | audio.registers, audio.counter, audio.lfsr, audio.integrator, audio.stereo, audio.mix, audio.links, four audio/audio2 profile observations, and ten audio.fault cases pass in the qualified inventory. |
| 2 | The host receives integer PCM on the exact 16 MHz / 400-tick lattice with tick-stamped events, and forbidden floating-point/filter/resampler imports remain absent from the core. | ✓ VERIFIED | core/audio.c fixes the sample interval at 400 ticks; audio.cadence, audio.order and core.no_float/core.includes pass. The direct audio.order spot-check also passed. |
| 3 | A source-built homebrew responds to recorded input with audio and video; the public consumer writes PPM/raw frames and WAV, preserves separate guest-tick streams and labels hashes as repeatability evidence. | ✓ VERIFIED | consumer.av_replay, consumer.av_edges, consumer.av_response_controls and consumer.av_restore pass; the independent checker validates the 13-stream bundle and response semantics. |
| 4 | The five source-built tier-A suites are run headlessly with results inventoried, and any unsupported suite observation is a visible skip with its blocker. | ✓ VERIFIED | Native, GNU and hosted canonical logs account for all 246 rows: 244 passed, two declared page-mode skips, zero failures. Audio/audio2 have four fresh profile observations with 18/18 rows each. |
| 5 | A fixed-input homebrew workload has a five-run throughput baseline with identical guest work and output digest; no optimization is made for this baseline. | ✓ VERIFIED | Final-source native capture records 1,248,256 guest ticks and an identical 13-stream digest across five runs; docs/performance.md separates API, encoding, assembly, publication and CI timing. |
| 6 | A source-built original guest responds to one recorded A edge with changed guest marker, raw frame and non-silent stereo WAV. | ✓ VERIFIED | consumer.av_replay plus the independent semantic checker validate guest markers, changed raw frames, post-edge PCM, events and WAV. |
| 7 | The first PCM frame is ordinal 0 at tick 400; device/link work precedes sampling, sampling precedes CPU grants, and target-tick work remains pending. | ✓ VERIFIED | audio.cadence and audio.order contain assertions for ordinals/ticks, the exclusive 400 boundary, CPU enable/write ordering and tick-416 first clock; audio.order passed independently. |
| 8 | The Phase 3 snapshot/silence path and Phase 5 input-only frame tracer remain separate working paths. | ✓ VERIFIED | consumer.public_api, consumer.frame_tracer and consumer.av_tracer remain separately registered and pass; av-replay is a separate mode. |
| 9 | Unchanged pinned audio/audio2 assembly can be built with project wrappers without executing the CRT main. | ✓ VERIFIED | scripts/build-suites.sh binds the source, wrapper, recipe and host-local tool records; suite.build and suite.audio.source_controls pass. |
| 10 | Both source-derived audio oracles contain exactly 18 rows, preserving audio2 index 9's 0x30-or-0x34 alternative. | ✓ VERIFIED | scripts/generate-suite-oracle.sh parses pinned source EXPECT arrays; tests/suites/test_public_suites.c checks extent and alternatives; suite.audio.source_controls passes. |
| 11 | Portable fixture identities bind source and recipe while each host validates its own native tool build record. | ✓ VERIFIED | Audio manifests, NOTICE, build record verification and suite.build_controls bind these identities; the native and GNU canonical gates pass. |
| 12 | All four mapped channels expose signed volume, reload/one-shot count, 12-bit LFSR, integrator and clipping behavior. | ✓ VERIFIED | audio.registers, audio.counter, audio.lfsr, audio.integrator and audio.mix make exact-value assertions and pass. |
| 13 | Each of eight MSTEREO bits independently mutes one channel/side without stopping channel evolution. | ✓ VERIFIED | audio.stereo plus audio.fault.stereo_left and audio.fault.stereo_right verify route independence and continued state progression. |
| 14 | Mapped inspection and opcode peek agree with real bus routing without advancing guest state. | ✓ VERIFIED | audio.registers checks mapped reads, opcode peek routing and unchanged instance bytes; the case passes. |
| 15 | Four routed signed-8 outputs are accumulated widely and scaled by 64 to the exact S16 bounds without rounding or overflow. | ✓ VERIFIED | audio.mix asserts the -32768 and +32512 endpoints and routed outputs; audio.fault.integrator_clip is a named negative control. |
| 16 | The T1→T3→T5→T7→A0→A1→A2→A3→T1 ring retains all pulses under a finite same-tick work bound. | ✓ VERIFIED | core/timer.c uses an eight-visit bounded worklist with retained debt; audio.links, audio.ring_zero and det.audio_paths pass. |
| 17 | A zero-reload ring cannot hang; pending work survives public stops and snapshots. | ✓ VERIFIED | audio.ring_zero, audio.capacity, audio.snapshot and det.audio_paths test zero reload, bounded stops, save/restore and continuation. |
| 18 | Samples/events match across whole and sliced calls at 399/400/401 and simultaneous CPU/device edges. | ✓ VERIFIED | audio.cadence, audio.order and det.audio_paths compare whole/sliced results, exclusive boundaries and common-tick event suffixes. |
| 19 | Invalid descriptors, aliases, alignment, ranges and arithmetic leave the machine, result and caller bytes unchanged. | ✓ VERIFIED | audio.span_refusal, audio.span_alias and API refusal cases check postconditions and sentinel bytes; these cases pass. |
| 20 | A capacity stop returns a complete ordered prefix and keeps the next record group pending for retry. | ✓ VERIFIED | audio.capacity, audio.stops and audio.order assert prefix, refusal tick, pending CPU work and exact retry output. |
| 21 | Caller-serialized machines and interleaved instances do not retain or share caller output buffers. | ✓ VERIFIED | audio.capacity checks replacement spans and independent instances; det.two_instances and consumer.av_replay cover interleaved instance state. |
| 22 | A refused format-7 restore preserves the instance and watch; a valid restore resumes audio state exactly. | ✓ VERIFIED | audio.snapshot_refusal validates unchanged state/watch on malformed blobs; audio.snapshot and api.snapshot_roundtrip exercise successful continuation. |
| 23 | Fresh, sliced, interleaved, watched/resumed and restored paths have identical complete stream suffixes at common ticks. | ✓ VERIFIED | det.audio_paths, consumer.av_replay, consumer.av_restore and audio.order compare state, frame, PCM and event outputs after continuation. |
| 24 | Snapshots around input edges, sample instants and deferred-ring stops neither drop nor duplicate output records. | ✓ VERIFIED | audio.snapshot, audio.order, det.audio_paths, consumer.av_edges and consumer.av_restore exercise pre/post edge and tick-400/deferred-ring boundaries. |
| 25 | A fixed input timeline changes guest markers, image evidence, channel events and non-silent PCM. | ✓ VERIFIED | consumer.av_replay checks all response streams; consumer.av_response_controls rejects silent, stale and malformed output. |
| 26 | The A/V manifest binds content/startup/profile/timeline/policies/build identity and every distinct output stream. | ✓ VERIFIED | consumer.av_replay hashes each real stream and checks manifest identity/extent through the independent checker. |
| 27 | Restoring host cursor/offset ownership and core state reproduces the exact suffix without replaying input or output prefix. | ✓ VERIFIED | consumer.av_restore checks sidecar identity, prefix and offsets; its resumed bundle matches the uninterrupted control. |
| 28 | Malformed timelines or checkpoints create no output and preserve caller-owned existing files. | ✓ VERIFIED | consumer.av_controls and consumer.av_restore cover malformed input/checkpoint refusals, absent output and unchanged sentinels. |
| 29 | Concurrent publication produces one complete exclusive bundle; refusals do not replace another owner's output. | ✓ VERIFIED | consumer.av_publication_controls races two independent normal consumers, checks exactly one winner, validates the bundle and preserves existing output. |
| 30 | Interrupted writes do not expose a partial published bundle or remove foreign staging. | ✓ VERIFIED | consumer.av_publication_controls injects write/close/abort failures and verifies owned cleanup plus preservation of foreign, markerless and replaced staging. |
| 31 | Fresh audio/audio2 guests complete under both named software profiles with independent model evidence and exactly 18 source rows each. | ✓ VERIFIED | Four native canonical observations report fresh completion and 18 raw/source-agreement rows; audio/audio2 control cases and GNU/CI inventories pass. |
| 32 | All five suite names appear once; only the exact two existing page-mode SERCTL skips remain. | ✓ VERIFIED | tests/inventory/fast.txt and suite.selection_controls enforce exact selection; only lynx1/lynx2 page-mode observations skip at FD8C. |
| 33 | Strict timer row 14 remains legacy 0x0C/source 0x0D and VIDEO_POLICY1 0x0F/source 0x0D, without new exceptions or hardware claims. | ✓ VERIFIED | docs/public-suites.md and tests/suites/run.cmake retain strict mismatches and the narrow existing descriptor; the reviewed diff adds no exception or hardware qualification. |
| 34 | Each private semantic mutant compiles and fails its intended assertion between clean positive brackets. | ✓ VERIFIED | Ten audio.fault tests use private source copies, exact assertion IDs, identity checks and clean brackets; every registered negative-control case passes. |
| 35 | Channel 1–3 link failures and the A3→T1 closing-edge fault are detectable independently of channel-0 arithmetic. | ✓ VERIFIED | audio.fault.link_a1, link_a2, link_a3 and ring_return target their own named link assertions and pass their fault-control protocol. |
| 36 | Left/right stereo, sample cadence and saved phase have independent fault detection without production switches. | ✓ VERIFIED | audio.fault.stereo_left, stereo_right, cadence_399 and snapshot_phase are distinct private mutants and all pass. |
| 37 | The excluded target tick remains pending and emits once on continuation; a cadence mutant fails the exact ordinal/tick assertion. | ✓ VERIFIED | audio.order, audio.cadence and audio.fault.cadence_399 assert this half-open boundary and named cadence failure. |
| 38 | Empty and one-record suffixes are compared explicitly; absent expectations or zero-mutant inventories cannot count as detection. | ✓ VERIFIED | audio.snapshot, audio.fault.snapshot_phase and det.audio_paths assert empty/single suffix cases; the fault harness rejects missing assertion and empty mutation results. |
| 39 | Equal-tick event sequences are stable and reordered/missing phase records fail the restored-suffix assertion. | ✓ VERIFIED | audio.order, audio.snapshot and audio.fault.snapshot_phase compare event sequence fields and exact restored suffixes. |
| 40 | Five same-build runs perform equal bounded work and produce identical per-stream/output digests. | ✓ VERIFIED | consumer.av_benchmark checks five fresh runs, all 13 stream identities, 1,248,256 guest ticks and final combined digest 4f8394b4587431b22180ca01d05a8631ada163fbebf77a18e5a95d4ac20759b4. |
| 41 | Monotonic host timing covers only in-memory public core advancement; encoding, publication and CI are separate measurements. | ✓ VERIFIED | consumer/main.c brackets lynx_advance_av calls with CLOCK_MONOTONIC; docs/performance.md and 06-QUALIFICATION.md keep host-output and CI timing distinct. |
| 42 | The baseline reports environment, median and range without a performance optimization. | ✓ VERIFIED | docs/performance.md reports the final native median/range and host/build identity; the final source adds no optimization for this baseline. |
| 43 | The reviewed exact source passes canonical headless checks on native AppleClang/ShellCheck and the existing GNU/ShellCheck environment. | ✓ VERIFIED | Private native and GNU gate logs each record exit 0 at source bc943ee; both account for the exact 246-row inventory. |
| 44 | Hosted CI qualifies the exact pushed source HEAD and records critical-path/duplicate-work inspection. | ✓ VERIFIED | CI run 43448772929 is completed/success for source bc943ee; 06-QUALIFICATION.md separates queue, setup, build, CTest, artifact and history costs. |
| 45 | Scoped software gaps are resolved while timer/source, SERCTL, O1, rights/model and release boundaries remain distinct. | ✓ VERIFIED | Final tests/reviews pass; docs retain the two SERCTL skips and strict timer disagreement. AGENTS.md and 06-SECURITY.md preserve owner stops and make no hardware or release claim. |

**Score:** 45/45 truths verified (0 present, behavior-unverified).

## Required Artifacts

The plan artifact query reports all declared artifacts present and substantive. The C/source links below were traced manually because the query's literal target-path heuristic does not resolve many C-to-header and symbol-level connections.

| Artifact | Plans | Status | Details |
|---|---|---|---|
| core/audio.c | 01, 03, 04 | ✓ VERIFIED | Four-channel arithmetic, mixer, sample lattice and event output are used by machine/timer scheduling. |
| docs/audio.md | 01, 04 | ✓ VERIFIED | Separates documented observations from versioned policy and records ring/deferred-work semantics. |
| fixtures/diagnostic/av_probe.s | 01 | ✓ VERIFIED | Project-authored input-responsive guest is built from its pinned manifest and used in consumer tests. |
| consumer/main.c | 01, 07, 08, 11 | ✓ VERIFIED | Public-only replay, checkpoint sidecar, stream generation and benchmark mode are wired to lynx_advance_av. |
| fixtures/suites/audio-wrapper.s | 02 | ✓ VERIFIED | Project wrapper for unchanged pinned audio assembly and source-built tier-A execution. |
| fixtures/suites/audio2-wrapper.s | 02 | ✓ VERIFIED | Project wrapper for unchanged pinned audio2 assembly and source-built tier-A execution. |
| scripts/generate-suite-oracle.sh | 02 | ✓ VERIFIED | Reads pinned source EXPECT arrays; enforces 18 rows and the audio2 row-9 alternative. |
| tests/audio/test_audio.c | 03, 05 | ✓ VERIFIED | Exact-value channel, route, mix, span, capacity, ordering and refusal assertions. |
| tests/audio/tests.cmake | 03, 10 | ✓ VERIFIED | Registers named positive cases and all ten exact semantic fault controls. |
| core/timer.c | 04 | ✓ VERIFIED | Bounded timer/audio ring uses audio transition functions and retains deferred debt. |
| include/lynxplosion.h | 05 | ✓ VERIFIED | Public PCM/event/frame span, advancement result and stop contract. |
| core/snapshot.c | 06 | ✓ VERIFIED | Format-7 audio fields are validated as a candidate before non-failing apply. |
| tests/det/test_det.c | 06 | ✓ VERIFIED | Complete stream, common-tick, snapshot continuation and sensitivity assertions. |
| docs/snapshot-audio.md | 06 | ✓ VERIFIED | Documents audio field table, immutable policy identity and continuation constraints. |
| tests/consumer/check_artifacts.c | 07 | ✓ VERIFIED | Independent semantic WAV/event/frame/response validator reads real produced output. |
| docs/av-replay.md | 07 | ✓ VERIFIED | Documents timeline, sidecar checkpoint, stream identities and bundle contract. |
| tests/consumer/av_driver.cmake | 08, 11 | ✓ VERIFIED | Drives malformed/refusal/race/restore controls and digest-bound benchmark checks. |
| tests/suites/test_public_suites.c | 09 | ✓ VERIFIED | Validates fresh suite observations, source rows, model/progress and exact selection evidence. |
| tests/suites/run.cmake | 09 | ✓ VERIFIED | Builds pinned guests/oracles, runs source observations and enforces strict comparison inventory. |
| docs/public-suites.md | 09 | ✓ VERIFIED | Lists the five-suite inventory, exact skips and unresolved timer-source mismatch. |
| tests/audio/fault_controls.cmake | 10 | ✓ VERIFIED | Builds private mutations and requires exact named failures with clean brackets. |
| docs/audio-faults.md | 10 | ✓ VERIFIED | Maps each audio mutant to its assertion and detection evidence. |
| 06-REVIEW.md | 12 | ✓ VERIFIED | Independent 54-path review report; later review iterations are clean. |
| 06-SECURITY.md | 12 | ✓ VERIFIED | ASVS1 threat review, 25 mitigations and three explicit prohibition judgments; no open threats. |
| 06-QUALIFICATION.md | 12 | ✓ VERIFIED | Exact-source native/GNU/hosted gate identities, inventory counts, evidence limits and CI timing. |
| docs/performance.md | 11, 12 | ✓ VERIFIED | Historical baseline remains separate from the final-source capture and CI costs. |

## Key Link Verification

All 24 declared key links were traced through real source references or the test evidence path. Several query results said “Target not referenced in source” because the checker looked for the destination path string rather than symbol calls/imports; manual evidence below confirms those links.

| Plan | From → To | Via | Status | Evidence |
|---|---|---|---|---|
| 06-01 | core/machine.c → core/audio.c | Mapped writes and scheduler feed caller sample/event spans | WIRED | machine.c initializes audio, routes mapped read/write, orders audio/link/sample events and captures audio spans. |
| 06-01 | consumer/main.c → include/lynxplosion.h | lynx_advance_av feeds WAV and raw frames | WIRED | main.c includes the public header only for emulator access and drives public A/V spans. |
| 06-02 | scripts/build-suites.sh → fixtures/suites/audio-wrapper.s | Pinned assembly include, linker layout and local tool verification | WIRED | Builder hashes and uses the project wrapper while validating source, recipe and tool record identities. |
| 06-02 | scripts/generate-suite-oracle.sh → build/pins/lynx-tests | Parse pinned main.c EXPECT arrays without CRT execution | WIRED | Generator requires the canonical pinned source tree and reads the source arrays; it never launches the C CRT main. |
| 06-03 | core/machine.c → core/audio.c | FD20–FD3F and FD50 routing across access paths | WIRED | Machine bus, mapped inspection, opcode peek and writes call audio availability/peek/write functions. |
| 06-03 | tests/audio/test_audio.c → include/lynxplosion.h | Guest-driven register and PCM/event assertions | WIRED | Test target uses public machine/API types and invokes lynx_advance_av; mapped internals are only used for internal-path controls. |
| 06-04 | core/timer.c → core/audio.c | One total-order worklist including A3→T1 | WIRED | timer.c calls audio clock roots and routes the eight-node ring through the same bounded worklist. |
| 06-04 | core/snapshot.c → core/internal.h | Pending pulse and same-tick cursor serialized fieldwise | WIRED | snapshot.c serializes audio fields and validates ring state through internal.h declarations. |
| 06-05 | include/lynxplosion.h → core/machine.c | lynx_advance_av preflight and commit frontier | WIRED | Header declares span/result contract; machine.c defines preflight, capacity checks, output commit and stop results. |
| 06-05 | core/machine.c → core/audio.c | Reserve capacity before device/sample/CPU mutation | WIRED | Scheduler stages audio transitions and events before committing, and checks sample/event capacity at their boundaries. |
| 06-06 | core/snapshot.c → core/audio.c | Candidate validation and non-failing apply | WIRED | Snapshot restore validates serialized audio/ring state before applying it to machine state. |
| 06-06 | tests/det/test_det.c → include/lynxplosion.h | Public spans and state digest compared at common tick | WIRED | Determinism test drives public spans and compares complete observations/state at common guest ticks. |
| 06-07 | consumer/main.c → include/lynxplosion.h | Only public API drives guest observations | WIRED | Emulator access in the consumer is through public calls; no private core symbols or fields are referenced. |
| 06-07 | tests/consumer/av_driver.cmake → tests/consumer/check_artifacts.c | Independent complete transcript/artifact checks | WIRED | Driver invokes the checker on real replay bundles and on specific private mutations. |
| 06-08 | consumer/main.c → tests/consumer/check_artifacts.c | Complete owned bundle acceptance | WIRED | Producer writes only its owned staging directory and checks publication identity; av_driver then runs the independent checker over the published bundle. The checker is intentionally test-only, not a production import. |
| 06-08 | tests/consumer/av_driver.cmake → consumer/main.c | Independent processes and interruption injection | WIRED | Driver launches normal/test consumers, races two publishers and injects write/close/abort failures. |
| 06-09 | tests/suites/run.cmake → scripts/build-suites.sh | Source-built guests and identities | WIRED | Suite driver invokes the builder and validates identities before running observations. |
| 06-09 | tests/suites/test_public_suites.c → scripts/generate-suite-oracle.sh | Exact source rows including audio2 alternative | WIRED | Driver generates the pinned oracle; reader validates row count, bytes, source identity and row-9 alternatives. |
| 06-10 | tests/audio/fault_controls.cmake → tests/audio/test_audio.c | Private mutation then exact intended assertion | WIRED | Fault harness copies and mutates source, builds a private test binary, checks the named assertion and brackets with clean runs. |
| 06-10 | tests/audio/fault_controls.cmake → core/snapshot.c | Serialized-phase loss changes restored suffix | WIRED | Snapshot-phase mutant alters private serialized-state handling and requires the exact continuation suffix failure. |
| 06-11 | consumer/main.c → include/lynxplosion.h | Clock only around in-memory public advancement | WIRED | Benchmark timer surrounds lynx_advance_av and reports host output work separately. |
| 06-11 | tests/consumer/av_driver.cmake → docs/performance.md | Five equal-output runs and exact source/build identity | WIRED | Driver checks run count/digests and docs records the corresponding final-source measurements and identities. |
| 06-12 | 06-VALIDATION.md → tests/inventory/fast.txt | Scoped proof and exact final inventory | WIRED | Validation coverage maps to registered tests; final CTest/JUnit accounts for each inventory row once. |
| 06-12 | .github/workflows/ci.yml → scripts/check.sh | Hosted gate on exact pushed source | WIRED | Workflow directly runs the canonical script; run 43448772929 completed successfully at the exact source HEAD. |

## Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|---|---|---|---|---|
| consumer/main.c A/V replay | guest tick, frame buffers, PCM, events | Source-built av_probe plus canonical input; public guest APIs and caller spans | Yes; checks changed markers, post-edge non-silent PCM/events and bounded progress before publication | ✓ FLOWING |
| tests/consumer/check_artifacts.c | WAV, PCM, event, raw/rendered frame and manifest bytes | Files produced by the normal consumer process | Yes; independently parses actual output, checks identities/extents and semantic response | ✓ FLOWING |
| tests/suites/run.cmake and test_public_suites.c | result bytes and comparison rows | Fresh guests built from pinned audio/audio2 assembly and source-derived EXPECT arrays | Yes; 18 raw and source rows per observation; two unrelated page-mode blockers remain explicit | ✓ FLOWING |
| consumer/main.c benchmark | timed guest ticks and stream hashes | Five fresh public A/V replays with fixed fixture/timeline | Yes; each run performs equal work and retains equal stream/output digests | ✓ FLOWING |

## Behavioral Spot-Checks

| Behavior | Command/evidence | Result | Status |
|---|---|---|---|
| Same-tick CPU/audio ordering and snapshot continuation | ctest --preset check --no-tests=error --output-on-failure -j1 -R '^audio.order$' | Exit 0; audio.order passed in 0.08 s. Assertions cover the exclusive tick-400 boundary, serialized continuation and the first channel clock at tick 416. | ✓ PASS |
| Full native canonical inventory and non-test gates | Existing private log for sh scripts/check.sh at source bc943ee | Exit 0; 246 rows accounted, 244 passed, two declared skips, zero failures; artifacts and full-history checks passed. | ✓ PASS |
| Full GNU canonical inventory and non-test gates | Existing private log for sh scripts/check.sh at source bc943ee | Exit 0; 246 rows accounted, 244 passed, two declared skips, zero failures. | ✓ PASS |
| Hosted exact-source CI | Existing run/job records and job log for run 43448772929 | Completed/success at source bc943ee; canonical command accounted for 246 rows. | ✓ PASS |
| Source suite observations and declared blocker | Native LastTest.log and gate.log | Audio/audio2 observations return with 18 raw rows and 18 source-agreement rows. Only page-mode lynx1/lynx2 skip at unsupported SERCTL write FD8C. | ✓ PASS |

The CTest spot-check was run once on the existing native check build. Its prior JUnit and LastTest logs were preserved before running it; the new test output and post-run logs were saved separately in private temporary storage. The broader canonical gates were not rerun for this report; their exact-source logs and CI records were inspected directly.

## Probe Execution

Not applicable. No phase plan or summary declares a probe script or PASS/stage-marker probe, and no conventional scripts/*/tests/probe-*.sh files are present.

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|---|---|---|---|---|
| AUD-01 | 06-01, 06-03, 06-09, 06-10, 06-12 | Four-channel digital behavior, timer links and stereo muting | SATISFIED | Authored value/stereo tests, four source observations, named faults and final inventory pass. |
| AUD-02 | 06-01, 06-03, 06-04, 06-05, 06-06, 06-10, 06-12 | Exact integer PCM/events, bounded ordering, spans and no core float/filter/resampler | SATISFIED | Cadence/order/ring/span/snapshot cases and forbidden-core checks pass. |
| HB-03 | 06-01, 06-07, 06-08, 06-11, 06-12 | Original homebrew produces input-responsive sound | SATISFIED | Real source-built fixture, response markers, post-edge PCM/events and independent bundle validation pass. |
| DET-05 | 06-01, 06-04, 06-05, 06-06, 06-07, 06-08, 06-10, 06-11, 06-12 | Separate progress/input/video/audio observations on one guest timeline; digests mean repeatability | SATISFIED | Complete stream comparison, snapshot continuation, sensitivity, manifest and five-run identity checks pass. |
| EVD-06 | 06-02, 06-09, 06-12 | Exact five-suite, source-built tier-A inventory with visible skips | SATISFIED | Final inventory is exact; audio/audio2 observations match all 18 rows; only the two named page-mode observations skip. |
| PERF-01 | 06-11, 06-12 | Five-run named workload baseline with constant output digest and no optimization | SATISFIED | Final-source native capture holds work and all 13 output streams constant and reports median/range separately from host and CI time. |
| CON-01 | 06-01, 06-05, 06-07, 06-08, 06-12 | Independent public-only consumer writes frames and audio WAV | SATISFIED | Consumer links only the public API; the independent validator checks actual PNG/PPM/raw, PCM and WAV output. |
| EVD-05 | 06-10, 06-12 | Each device phase has a named fault that fails its intended test | SATISFIED | Ten audio mutants build and fail their named assertion with clean brackets; existing timer/video/input faults remain in the canonical gate. |

All eight Phase 6 requirement IDs in the plans are accounted for. EVD-02 is not a Phase 6 plan requirement or a promised closure; its global strict timer-source mismatch remains open and visible in docs/public-suites.md.

## Test Quality Audit

| Test file | Linked requirement(s) | Active coverage | Circular expected values | Assertion strength | Verdict |
|---|---|---|---|---|---|
| tests/audio/test_audio.c | AUD-01, AUD-02, DET-05 | Active named cases for channels, routes, bounds, ordering, spans and restore | No; exact literals and state transitions are asserted | Value and behavioral | PASS |
| tests/audio/fault_controls.cmake | EVD-05, AUD-01, AUD-02 | Ten private semantic mutants with clean brackets | No; the mutant is not the source of its expected assertion | Behavioral negative control | PASS |
| tests/det/test_det.c and tests/det/av_driver.cmake | DET-05, AUD-02 | Complete stream/path/sensitivity comparisons | No; same-run comparisons are labelled repeatability, not an external correctness oracle | Multi-step behavioral | PASS |
| tests/consumer/av_driver.cmake | HB-03, CON-01, DET-05, PERF-01 | Real guest output, checkpoint, refusal/race controls and five-run baseline | No; mutated records are parser controls, not benchmark samples | Behavioral and value | PASS |
| tests/consumer/check_artifacts.c | HB-03, CON-01, DET-05 | Independently validates actual full output bundles and semantic mutations | No; reads consumer-produced files and has no consumer implementation import | Value and behavioral | PASS |
| tests/suites/test_public_suites.c and tests/suites/run.cmake | AUD-01, EVD-06 | Fresh suite observations, model/progress checks, exact selection and source controls | No; expectations come from pinned source EXPECT arrays | Value and behavioral comparison | PASS |
| scripts/generate-suite-oracle.sh | EVD-06 | Strict source parser; tests cover source shape and row alternatives | No; reads pinned C EXPECT source, not emulator execution | Value-level source comparison | PASS |

Disabled-test scan found no requirement test disabled as an implementation substitute. The only skips are the two explicitly allowed page-mode observations, with the unsupported SERCTL write and address recorded in the inventory/log. Test-file writes are controlled inputs, private mutations and output bundles; expected suite values are parsed from pinned source. No circular oracle pattern was found.

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---:|---|---|---|
| consumer/main.c | 719, 1199 | Legacy diagnostic mode names its intentional placeholder silence WAV | Info | This is the preserved Phase 3 snapshot/silence regression path. The separate av-replay mode emits real PCM/WAV and passes response checks. |
| include/lynxplosion.h | 11, 88 | Existing flat5 CPU model text calls its five-tick model a placeholder | Info | Inherited CPU-profile wording outside the new audio policy; does not feed the A/V path or its output. |

No unreferenced TBD/FIXME/XXX debt marker or empty active implementation was found in phase key files. Grep hits for XXXXXX are mkstemp/mkdtemp temporary-name templates, not debt markers. Return-NULL matches are optional/helper exits, not output stubs.

## Decision Coverage

All 17 trackable decisions in 06-CONTEXT.md are honored by shipped artifacts. The configured non-blocking decision-coverage query returned honored 17/17 with no missing decisions.

## Prohibition Judgments

The three plan prohibitions are satisfied against the owner policies in AGENTS.md and the scoped security review in 06-SECURITY.md:

| Prohibition | Status | Evidence |
|---|---|---|
| Do not present repeatability/source observations as hardware accuracy or an O1-qualified suite pass. | ✓ VERIFIED | Reports label repeatability/source observations separately; the unresolved model and physical qualification boundary remains explicit. |
| Do not use or redistribute unapproved firmware/commercial assets or emulator implementation source. | ✓ VERIFIED | Phase fixtures are project-authored or pinned notice-bearing tests; the security review records repaired containment and the existing asset/source boundaries. |
| Do not silently authorize physical testing, release, a new distribution channel or shared LibFuturo contract. | ✓ VERIFIED | The phase adds no such authority; AGENTS.md and security/qualification reports preserve those stops. |

## Human Verification Required

N/A. This headless public-API phase has no visual, external-service, physical-device or subjective listening criterion. Its full replay/checkpoint and lifecycle invariants are covered by active behavioral tests; owner rights, hardware and release decisions remain outside this phase.

## Gaps Summary

No Phase 6 roadmap truth, plan truth, required artifact, key link or Phase 6 requirement remains unverified. The exact source-qualified implementation at bc943ee is unchanged in the current HEAD successor; its diff contains planning metadata only. The final local and hosted evidence is tied to bc943ee, not claimed for an untested source change.

The two page-mode observations remain skipped because the source writes unsupported SERCTL address FD8C before producing results. Strict timer row 14 remains legacy 0x0C/source 0x0D and VIDEO_POLICY1 0x0F/source 0x0D, so global EVD-02 remains open. These are explicit qualification limits, not missing Phase 6 outputs. Same-core repeatability and source observation do not establish hardware accuracy, model qualification, O1, rights clearance or release authority.

---

_Verified: 2026-11-13T15:38:52Z_  
_Verifier: gsd-verifier_
