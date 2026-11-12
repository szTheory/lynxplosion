---
phase: 05-display-raw-video-and-input
verified: 2026-11-12T18:24:19Z
status: passed
score: 44/44 must-haves verified
covered_files:
  - .planning/phases/05-display-raw-video-and-input/05-01-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-01-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-02-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-02-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-03-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-03-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-04-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-04-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-05-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-05-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-06-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-06-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-07-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-07-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-08-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-08-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-09-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-09-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-10-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-10-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-11-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-11-SUMMARY.md
  - .planning/phases/05-display-raw-video-and-input/05-12-PLAN.md
  - .planning/phases/05-display-raw-video-and-input/05-12-SUMMARY.md
  - CMakeLists.txt
  - README.md
  - consumer/main.c
  - core/bus.c
  - core/bus.h
  - core/input.c
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - core/timer.c
  - core/video.c
  - docs/bus-video-policy.md
  - docs/claims/skeleton-repeatability.claim
  - docs/input-replay.md
  - docs/public-suites.md
  - docs/snapshot-video.md
  - docs/timer-video-revisit.md
  - docs/video-faults.md
  - docs/video-input.md
  - fixtures/diagnostic/input_probe.cfg
  - fixtures/diagnostic/input_probe.s
  - fixtures/diagnostic/input_probe_tracer.s
  - fixtures/manifests/input-probe.manifest
  - fixtures/manifests/page-mode-tests.manifest
  - fixtures/suites/page-mode-wrapper.s
  - include/lynxplosion.h
  - runner/bundle.c
  - runner/bundle.h
  - scripts/build-page-mode.sh
  - scripts/check-manifests.sh
  - scripts/generate-page-mode-oracle.sh
  - scripts/validate-kv.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/cmake/abi.cmake
  - tests/cmake/no_skip.cmake
  - tests/consumer/check_artifacts.c
  - tests/consumer/frame_driver.cmake
  - tests/consumer/tests.cmake
  - tests/cpu/test_cpu_vectors.c
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/det/two_runs.cmake
  - tests/diag/test_diag.c
  - tests/gates/manifests_controls.sh
  - tests/gates/page-mode-script-controls.sh
  - tests/golden/skeleton.digest
  - tests/header/header.c
  - tests/inventory/fast.txt
  - tests/inventory/skips.txt
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
  - tests/timers/fault_controls.cmake
  - tests/video/fault_controls.cmake
covered_digest: "v4:sha256:5b3bf741b04f97df3070b19f78ee93ab1ed1fccab5850c13e428d9b7d24be021"
behavior_unverified: 0
overrides_applied: 0
---

# Phase 5: Display, Raw Video and Input Verification Report

**Phase Goal:** As a public API host, I want to receive timer-generated raw frames from an original homebrew responding to a recorded logical input timeline, so that I can render and reproduce its input-responsive frames.

**Verified:** 2026-11-12T18:24:19Z
**Status:** passed
**Re-verification:** No — initial verification

## User Flow Coverage

| Step | Expected | Evidence in codebase | Status |
|---|---|---|---|
| Host selects the original homebrew and supplies a recorded logical input timeline | The public consumer validates the bounded timeline and guest identity before creating output. | consumer/main.c:379-448,1380-1407; original direct-load fixture fixtures/diagnostic/input_probe.s and its manifest | ✓ VERIFIED |
| Timeline edges reach the guest at their exact guest ticks | The scheduler advances to each edge, sets the instance input, and the guest samples JOYSTICK/SWITCHES at its actual bus grant. | consumer/main.c:2038-2103; core/machine.c:609-646; core/input.c:20-58; api.input_edges, api.input_bits, api.input_orientation | ✓ VERIFIED |
| Guest timers complete frames and the API returns raw observations | Frame completion is Timer 2 driven; the frame API returns pens, per-row palettes, line ticks, and boundary metadata. | core/machine.c:578-603,739-780; core/video.c:196-260; include/lynxplosion.h:236-304; api.video_cadence, api.video_geometry | ✓ VERIFIED |
| Consumer renders the guest's input-responsive frames | PPM output is generated from raw pens and palette bytes; the independent checker checks expected pens and rendered pixels. | consumer/main.c:1174-1206,1835-1880,2211-2222; tests/consumer/check_artifacts.c; consumer.frame_replay, consumer.frame_response_controls | ✓ VERIFIED |
| The same recording reproduces its raw frames | Fresh and interleaved runs compare complete raw and rendered digests; the manifest labels this repeatability, not hardware accuracy. | consumer/main.c:2200-2220; tests/consumer/frame_driver.cmake; 05-QUALIFICATION.md exact-source run record | ✓ VERIFIED |

## Goal Achievement

### Observable Truths

The five ROADMAP success criteria and all plan must_haves.truths were checked against source, registered assertions, and the recorded exact-source qualification. The plan truth that repeats the roadmap strict timer-disagreement clause is counted with that roadmap truth, giving 44 unique truths.

| # | Truth | Status | Evidence |
|---:|---|---|---|
| 1 | Timer 0/2 settings control guest-tick line/frame timing; frame completion is documented and no fixed host 60 Hz drives it. | ✓ VERIFIED | core/video.c:214-249, core/machine.c:578-588; api.video_cadence asserts 50/60/75 Hz presets, altered settings, and short frames. |
| 2 | DMA consumes bus ticks and closes page streams; page-mode outcomes and the exact timer disagreement remain visible; the named DMA-cost fault fails its target assertion. | ✓ VERIFIED | core/machine.c:594-604,627-664; api.video_bus, api.video_dma_cost_fault, suite.page_mode.observation.*, suite.timers.video_revisit; docs/timer-video-revisit.md. The two page-mode skips and timer mismatches are retained below. |
| 3 | Raw frames expose per-line 16-entry palette bytes, pen indices, and guest timing, separate from host presentation. | ✓ VERIFIED | include/lynxplosion.h:236-301; core/video.c:196-212,252-260; consumer.frame_replay, api.video_geometry, api.video_palette_fault; independent artifact checks. |
| 4 | The nine logical input levels map to guest joystick/switch reads at guest sample time, without controller discovery. | ✓ VERIFIED | include/lynxplosion.h:216-234; core/input.c:9-58; core/machine.c:609-646; api.input_bits, api.input_orientation, api.input_edges. |
| 5 | The original homebrew responds to the recorded timeline, the consumer renders its frames, and the same recording yields the same labelled raw digest. | ✓ VERIFIED | fixtures/diagnostic/input_probe.s; consumer/main.c:1835-1880,2038-2222; tests/consumer/check_artifacts.c; consumer.frame_replay, consumer.replay_edges. |
| 6 | The public consumer observes independently checked baseline and A-pressed frames with the expected raw-pen change. | ✓ VERIFIED | consumer/main.c:1438-1533; response checker and consumer.frame_response_controls assert released 0x11 and pressed 0x22 results. |
| 7 | Video scheduler state is instance-owned and snapshot format 5 restores the continuation needed for the next frame. | ✓ VERIFIED | core/internal.h, core/snapshot.c; api.video_snapshot, api.video_reset_load. |
| 8 | Frame buffers have bounded declared capacities, validated disjoint spans, fixed raw pen/palette/tick extents, and refusal paths preserve machine and outputs. | ✓ VERIFIED | include/lynxplosion.h:236-301; core/machine.c:739-780; api.frame_refusal, api.video_snapshot_refusal. |
| 9 | Guest-configured Timer 0/2 presets, including 50/60/75 Hz and short frames, produce the asserted guest ticks. | ✓ VERIFIED | api.video_cadence assertions at tests/api/test_api.c:6898-7014; core/video.c:214-249. |
| 10 | Captures include exactly visible rows 0–101, three blank rows are explicit in the contract, and row 103 is not captured. | ✓ VERIFIED | include/lynxplosion.h:236-243,285-292; api.video_geometry and frame boundary assertions. |
| 11 | Each row carries the palette latched at its boundary, its guest line-start tick, and the asserted display-address traversal. | ✓ VERIFIED | core/video.c:196-212,236-248; api.video_geometry, api.video_palette_fault. |
| 12 | Video, refresh, and CPU grants follow the documented priority; blocked CPU requests remain pending. | ✓ VERIFIED | core/machine.c:550-665; api.video_bus, api.video_bus_edges. |
| 13 | Only eligible same-page opcode fetches receive the four-tick continuation; data accesses and other grants close the stream. | ✓ VERIFIED | core/machine.c:627-655; core/bus.c; api.video_page_mode, with page-mode source observations separately skipped where the guest reaches unsupported SERCTL. |
| 14 | LEGACY_FLAT5 remains the default; the new frame policy is explicitly unverified software policy. | ✓ VERIFIED | include/lynxplosion.h:268-275; docs/bus-video-policy.md, docs/video-input.md; qualification labels policy and profile limits. |
| 15 | Invalid frame descriptors, spans, overlaps, and unavailable policy refuse without changing guest state or caller outputs. | ✓ VERIFIED | core/machine.c:739-780; api.frame_refusal, api.input_refusal. |
| 16 | Frame API stops preserve outputs until a complete frame; only the FRAME result publishes the captured arrays. | ✓ VERIFIED | core/video.c:252-260; core/machine.c:739-780; api.video_geometry and frame-edge assertions. |
| 17 | Accepted reset/load clears logical input and capture state; rejected loads preserve it. | ✓ VERIFIED | core/machine.c, core/video.c; api.video_reset_load, api.video_snapshot_refusal. |
| 18 | All nine input levels, including independent action/options and Pause, are sampled correctly at the actual grant. | ✓ VERIFIED | core/input.c:30-58; api.input_bits, api.input_edges; input fault controls. |
| 19 | Guest SPRSYS Lefthand state alone selects directional bit orientation; logical host masks remain unchanged. | ✓ VERIFIED | core/input.c:39-50; api.input_orientation, api.input_fault_orientation. |
| 20 | MAPCTL selects the documented device/RAM routes; unsupported Suzy access is refused. | ✓ VERIFIED | core/machine.c, core/bus.c, core/input.c:20-25; api.input_refusal, api.video_bus. |
| 21 | Snapshot format 5 restores ongoing video/input state and reproduces the following frame. | ✓ VERIFIED | core/snapshot.c; api.video_snapshot, api.video_snapshot_refusal; runner/bundle.c state digest integration. |
| 22 | Rehashed but invalid snapshots are refused transactionally without changing machine, watches, or outputs. | ✓ VERIFIED | core/snapshot.c; api.video_snapshot_refusal, api.state_digest. |
| 23 | Canonical guest-state identity includes relevant video/input state but excludes host pointers, scripts, and watch configuration. | ✓ VERIFIED | core/snapshot.c, runner/bundle.c; api.state_digest. |
| 24 | Timeline grammar, strict event ordering, mask bounds, and canonical input hashing are checked before publishing output. | ✓ VERIFIED | consumer/main.c:379-448,1380-1407,2200-2222; consumer.replay_controls, consumer.frame_publication_controls. |
| 25 | Early frame returns do not shift later timeline edges; replay/restore cursors resume at the exact guest tick. | ✓ VERIFIED | consumer/main.c:2038-2115; consumer.replay_edges, consumer.replay_restore. |
| 26 | Original-homebrew replay checks guest progress and sampled input independently of host frame boundaries. | ✓ VERIFIED | fixtures/diagnostic/input_probe.s; consumer/main.c:2038-2222; consumer.frame_replay, consumer.replay_edges. |
| 27 | Malformed timeline, labels, paths, or fixture identities cannot publish or replace a valid output directory. | ✓ VERIFIED | consumer/main.c validation/staging paths; consumer.frame_publication_controls, consumer.replay_controls. |
| 28 | Missing guest progress or released-input response fails the intended semantic assertion after a successful build. | ✓ VERIFIED | consumer.frame_driver.cmake; consumer.frame_response_controls; recorded positive brackets in the current inventory. |
| 29 | PPM/publication failures are distinguished from guest-semantic failures. | ✓ VERIFIED | consumer/main.c publication path; consumer.frame_publication_controls, consumer.replay_controls. |
| 30 | Three video and five input semantic mutants fail their specific independent assertions. | ✓ VERIFIED | tests/video/fault_controls.cmake, tests/api/test_api.c; recorded api.video_dma_cost_fault, api.video_dma_request_fault, api.video_palette_fault, and five input-fault cases. |
| 31 | Clean positive brackets surround the DMA/palette/input fault controls. | ✓ VERIFIED | The named fault-control registrations in tests/api/tests.cmake and final C122 qualification. |
| 32 | Phase 5 covers the video/input slice of recurring EVD-05 while leaving audio fault coverage open. | ✓ VERIFIED | docs/video-faults.md; 05-QUALIFICATION.md preserves the Phase 6 audio obligation. |
| 33 | Input samples are checked immediately before, at, and after the edge, including a frame tie and delayed DMA/refresh grant. | ✓ VERIFIED | tests/api/test_api.c:8714-8805; api.input_edges. |
| 34 | The page-mode suite is built from unchanged pinned source with independently recorded source, tool, layout, and oracle identities. | ✓ VERIFIED | scripts/build-page-mode.sh, scripts/generate-page-mode-oracle.sh, tests/suites/test_public_suites.c, docs/public-suites.md. |
| 35 | Each page-mode profile either produces a bounded raw result or a precise visible blocker; O1 alone does not turn a skip into agreement. | ✓ VERIFIED | tests/suites/run.cmake, tests/suites/tests.cmake:44-47, docs/public-suites.md, 05-QUALIFICATION.md. Both current profiles stop at the same unsupported request before results. |
| 36 | A page-mode skip remains missing observation evidence and is never counted as source agreement or hardware qualification. | ✓ VERIFIED | Inventory/JUnit skip controls and explicit qualification status; no raw result is reported for either skipped profile. |
| 37 | Legacy D-10 remains the exact no-DMA, counter-zero, flat-five-tick row-14 observation 0c against source 0d. | ✓ VERIFIED | docs/timer-video-revisit.md; suite.timers.observation.*; no source expectation was altered. |
| 38 | The VIDEO_POLICY1 revisit uses separately frozen preconditions and does not inherit the legacy exception. | ✓ VERIFIED | docs/timer-video-revisit.md; suite.timers.video_revisit, timer record identity/control cases. |
| 39 | Timer records preserve result extents, boundary ranges, +/-1 tick cases, and reject invalid observation inputs before recording. | ✓ VERIFIED | tests/suites/test_public_suites.c, tests/suites/run.cmake; suite.timers.result_controls, suite.timers.video_revisit_controls. |
| 40 | Oracle records preserve exact canonical bytes and complete alternative sets; checked parsing prevents integer overflow. | ✓ VERIFIED | tests/suites/test_public_suites.c and timer record/parser controls in tests/suites/run.cmake. |
| 41 | Uninterrupted, sliced, watched, and restored runs agree on common ticks/results; one machine's calls are serialized. | ✓ VERIFIED | Timer and video API assertions; api.video_snapshot, api.input_edges, suite.timers.video_revisit_controls; public contract in include/lynxplosion.h:300-301. |
| 42 | Exact inventory/JUnit, semantic, security, and review evidence is bound to the reviewed implementation source. | ✓ VERIFIED | 05-CLOSEOUT.md, 05-QUALIFICATION.md, 05-REVIEW-RESOLUTION.md; the 54 product/test/script/fixture/doc paths other than README are byte-identical to qualification, and README has only later status prose (SHA-256 629a416602d47780bee8fdfeb13b64ce9b5e1b9c3c30fab23846f76c2c91a510). |
| 43 | Local macOS/Linux gates use the canonical command, and hosted CI checked the exact pushed reviewed source commit. | ✓ VERIFIED | 05-QUALIFICATION.md: sh scripts/check.sh on macOS and independent Ubuntu 24.04, plus hosted run 43268267738 for d3fb0024fe6a732b05f9b42122a18c110bed8ae2. Current HEAD is documentation/evidence-only follow-up; no later-head hosted run is claimed. |
| 44 | Raw source agreement, repeatability, partial recurring requirements, and owner/hardware limits are recorded distinctly. | ✓ VERIFIED | README.md, docs/claims/skeleton-repeatability.claim, 05-QUALIFICATION.md; exact mismatches and skips remain explicit. |

**Score:** 44/44 truths verified (0 present, behavior-unverified).

### Required Artifacts

verify.artifacts returned **33/33 declared artifact references present and substantive** across all 12 plans. I then checked their wiring at call sites. Primary paths include:

| Artifact group | Expected | Status | Details |
|---|---|---|---|
| include/lynxplosion.h, core/machine.c, core/video.c, core/timer.c | Public input/frame contract and timer-owned frame scheduler | ✓ VERIFIED | Public descriptors and stop contract connect to timer→video dispatch and frame-copy path. |
| core/bus.c, core/input.c | Timed guest bus behavior and logical input readback | ✓ VERIFIED | CPU bus grant calls the mapped input read path using the actual grant sample tick. |
| core/snapshot.c, runner/bundle.c | Format-5 continuation and canonical state identity | ✓ VERIFIED | Snapshot validates before apply; runner consumes canonical guest-state digest. |
| consumer/main.c, fixtures/diagnostic/input_probe.s, fixtures/manifests/input-probe.manifest | Public consumer and original source-built input-responsive guest | ✓ VERIFIED | Validated input timeline drives guest input; frame data and trace manifest are generated. |
| tests/api/test_api.c, tests/consumer/check_artifacts.c, tests/consumer/frame_driver.cmake | Independent behavioral assertions and artifact checks | ✓ VERIFIED | Tests inspect raw values, timing, guest response, render output, refusal paths, and replay equality. |
| tests/suites/test_public_suites.c, tests/suites/run.cmake, scripts/build-page-mode.sh, scripts/generate-page-mode-oracle.sh | Pinned source build, strict oracle, and honest blockers | ✓ VERIFIED | Profile-specific missing observations are represented as exact declared skips. |

### Key Link Verification

| From | To | Via | Status | Details |
|---|---|---|---|---|
| Timer bank | Video scheduler | machine_advance applies the timer borrow mask at each guest event | ✓ WIRED | Timer 2 underflow signals frame completion; timer 0 underflow advances line state. |
| Video/refresh grants | CPU bus stream | Scheduler priority and page latch reset | ✓ WIRED | Video and refresh work precede CPU and close page continuation; pending CPU request is retained. |
| CPU mapped input read | Logical input state | bus_access samples at actual grant tick | ✓ WIRED | Guest sees the latest level after any DMA/refresh hold. |
| Public frame API | Caller raw buffers | Descriptor validation, then lynx__video_copy_frame on FRAME | ✓ WIRED | Non-FRAME leaves buffers unchanged; no presentation transform is in the core. |
| Snapshot | Video/input continuation | Format-5 encode, validate, apply | ✓ WIRED | Invalid rehashed states refuse without partial application. |
| State digest | Runner evidence | runner/bundle.c calls lynx_state_digest | ✓ WIRED | Host pointers and runner scripts do not enter canonical guest identity. |
| Input timeline | Guest observation | parse_input_timeline → exact tick advancement → lynx_set_input → guest bus sample | ✓ WIRED | Frame boundaries do not move the later timeline event. |
| Fixture and consumer | Raw/PPM artifact checker | Source-built original guest → public API consumer → independent checker | ✓ WIRED | Checker asserts expected raw pen and RGB output, timing, palette, and transcript fields. |
| Pinned page-mode source/oracle | Suite runner | Build/oracle identity passed into suite parser and runtime record validation | ✓ WIRED | A blocker record is validated as missing observation, not agreement. |
| Canonical gate | Exact inventory and JUnit | scripts/check.sh runs registered inventory and rejects undeclared/stale skips | ✓ WIRED | Qualification accounted for all rows without silently converting the two skips. |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|---|---|---|---|---|
| Raw frame | pens, palettes, line_ticks | Guest display registers, RAM DMA, timer boundaries in core/video.c | Yes; copied to caller-owned arrays on a frame event | ✓ FLOWING |
| Logical input | input_mask and mapped register byte | Parsed timeline event at exact guest tick; core/input.c maps JOYSTICK/SWITCHES | Yes; read by the guest at its CPU grant | ✓ FLOWING |
| Rendered frame | PPM pixel bytes | Raw pen nibbles plus captured row palettes in consumer/main.c | Yes; independently checked against expected pixels | ✓ FLOWING |
| Repeatability | raw_sha256 | Complete raw frame bytes from each consumer run | Yes; fresh/interleaved replay comparisons require equal digests | ✓ FLOWING |

### Behavioral Spot-Checks

No duplicate broad regression was run. I reused the exact-source qualification and read the named assertion bodies for the behavior-dependent transitions.

| Behavior | Evidence | Result | Status |
|---|---|---|---|
| Timer cadence, frame stops, visible/blank geometry, raw palette/timing | api.video_cadence, api.video_geometry assertions; final qualified gate | 50/60/75 settings and boundaries asserted | ✓ PASS |
| DMA ordering, bus price, page-stream closure, named fault | api.video_bus, api.video_bus_edges, api.video_dma_cost_fault; final qualified gate | Intended assertions and fault controls passed | ✓ PASS |
| Logical input mapping and grant-time edge behavior | api.input_bits, api.input_orientation, api.input_edges; final qualified gate | Nine masks and edge/sample transitions asserted | ✓ PASS |
| Snapshot continuation and refusal atomicity | api.video_snapshot, api.video_snapshot_refusal; final qualified gate | Restored results agree; hostile snapshots refuse transactionally | ✓ PASS |
| Original homebrew replay, rendering, same-recording digest, and malformed-output controls | consumer.frame_replay, consumer.frame_response_controls, consumer.replay_edges, consumer.frame_publication_controls; final qualified gate | Independent expected output plus replay equality asserted | ✓ PASS |
| Whole current registered suite | 05-QUALIFICATION.md, source commit d3fb0024fe6a732b05f9b42122a18c110bed8ae2 | macOS, self-contained Ubuntu 24.04, and hosted CI each accounted for 202 rows: 200 passed, 2 exact declared skips, 0 failed | ✓ PASS |

### Probe Execution

No plan declares a probe, and there are no scripts/*/tests/probe-*.sh files. Probe execution is not applicable.

### Requirements Coverage

| Requirement | Source plan | Description | Status | Evidence |
|---|---|---|---|---|
| VID-01 | 05-01, 05-02 | Programmable timer-driven line/frame timing | ✓ SATISFIED for Phase 5 | api.video_cadence; scheduler ties frame stop to Timer 2. |
| VID-02 | 05-03, 05-09, 05-10, 05-11 | DMA arbitration, page-mode behavior, and timer revisit | ✓ SATISFIED for scoped software criteria | Bus/fault assertions pass; two profile page-mode raw observations remain exact declared skips; timer row-14 mismatch remains open. |
| VID-03 | 05-01, 05-02, 05-09 | Raw pens, per-line palette, and timing metadata | ✓ SATISFIED for Phase 5 software contract | Public buffers and independent consumer checks. |
| VID-04 | 05-01, 05-04, 05-06 | Caller-owned raw frame API | ✓ SATISFIED | Capacity, overlap, error, and stop-output assertions. |
| VID-05 | 05-04, 05-06 | Frame output and continuation/refusal behavior | ✓ SATISFIED | Frame boundary, reset/load, snapshot, and refusal tests. |
| INP-01 | 05-05, 05-07, 05-08 | Logical input mapping and replay timeline | ✓ SATISFIED for Phase 5 software contract | Nine-button mapping, exact guest-tick delivery, input response, and parser controls. |
| API-06 | 05-04, 05-05, 05-06 | Public input and frame API behavior | ✓ SATISFIED | Public header and caller-based tests. |
| HB-02 | 05-07, 05-08 | Original homebrew responds to recorded input | ✓ SATISFIED | Original fixture, public consumer, independent raw/PPM checker. |
| CON-01 | 05-01, 05-12 | Consumer integration | PARTIAL globally | Phase 5 real-frame video/input slice is present; audio integration remains Phase 6. |
| EVD-02 | 05-10, 05-11, 05-12 | Timer agreement evidence | OPEN globally | Phase 5 revisit is complete, but strict source agreement remains unresolved as detailed below. |
| EVD-05 | 05-09, 05-12 | Semantic fault controls | PARTIAL globally | Phase 5 DMA/palette/input slice is verified; audio fault slice remains Phase 6. |

### Global Evidence and Scope Limits

These are visible open obligations, not missing Phase 5 user-flow work:

| Item | Current evidence and disposition |
|---|---|
| Legacy timer result | LEGACY_FLAT5 remains the default. With no DMA and zero counter, D-10 row 14 is observed as 0c against pinned source 0d. |
| VIDEO_POLICY1 timer result | There is no pending descriptor. Strict row 14 is 0f against source 0d; the other 15/16 rows agree. The strict mismatch remains a failure. |
| Page-mode observations | suite.page_mode.observation.lynx1 and .lynx2 stop on unsupported $06 write to $FD8C before any result. Raw observations are missing; no profile agreement/qualification is claimed. |
| Recurring evidence | EVD-02 remains open. CON-01 and EVD-05 remain partial for their Phase 6 audio obligations. |
| Accuracy/authority | Repeatability and software observation are not hardware calibration. O1/O5/O6/O9, firmware/commercial rights, physical tests, shared LibFuturo, release, and broader publication stops remain open. |

Plan 05-12's two prohibitions were checked as negative constraints: current claims distinguish repeatability from calibrated accuracy and no named-profile suite pass is claimed; the native frame/input slice adds no authority for media, physical tests, shared-library work, release, or broader publication. The existing owner stops remain in force without a new human UAT request.

### Decision Coverage

The decision coverage check reports **12/12 trackable CONTEXT.md decisions honored** by the shipped artifacts.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---:|---|---|---|
| — | — | No unresolved TBD, FIXME, or XXX debt markers in the scoped source/evidence paths | — | No blocker found. |
| consumer/main.c | 682, 1162 | Explicit placeholder silence for Phase 6 audio | Info | Audio output is deliberately outside this video/input goal and labelled as such; it does not feed raw frames. |

### Human Verification Required

None for the Phase 5 software flow. Hardware accuracy, media rights, physical testing, and owner authority are open project gates, not unverified software transitions for this phase.

### Gaps Summary

None for the Phase 5 goal. The exact page-mode skips, remaining strict timer disagreement, and partial Phase 6 audio obligations remain open and visible; they are not represented as agreement or global completion.

---

_Verified: 2026-11-12T18:24:19Z_
_Verifier: the agent (gsd-verifier)_
