---
phase: "05"
slug: "display-raw-video-and-input"
status: in-progress
nyquist_compliant: false
wave_0_complete: true
created: "2026-11-11"
---

# Phase 5 — Validation Strategy

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | C17 CTest with existing ASan/UBSan and CMake/shell controls |
| Config | CMakePresets.json, tests/tests.cmake, tests/inventory/fast.txt |
| Existing quick command | `ctest --preset check --no-tests=error -R '^api.advance_budget$'` |
| Full command | `sh scripts/check.sh` |
| Runtime | Baseline hosted ~109s; focused groups target <60s; new source fault batches measured separately |

## Sampling Rate

Run each task's owning assertions after its last relevant edit, then reuse completed focused evidence. Each wave runs the affected groups; final source gets one full local/macOS, one self-contained Linux and exact-head hosted gate. Do not repeat the unchanged full gate after prose-only edits. Missing CTest matches fail with `--no-tests=error`.

## Per-Task Verification Map

Complete planned24-task map below; independent checker reviews before execution.

Task format is phase-plan-task. SC is T-05-SC; numeric threats are unique phase-wide. Commands define the owning checks, and the final column records their current execution status. Each named new test is authored and registered before the same task's verification; later users require its completed predecessor.

| Task | Wave | Requirements | Threat refs | Exact registered tests / proof | Command | Dependency | Exists now | Status |
|---|---|---|---|---|---|---|---|---|
| 05-01-1 | 1 | VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02, CON-01 | T-05-01, T-05-02, T-05-03, SC | consumer.frame_tracer | C011 | Phase4 + pinned tools | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-01-SUMMARY: tracer before/after task commit |
| 05-01-2 | 1 | VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02, CON-01 | T-05-01, T-05-02, T-05-03, SC | consumer.frame_tracer; consumer.public_api; consumer.snapshot_ppm; consumer.silence_wav; api.header_selfcontained | C012 | 05-01-1 | mixed old/header + tracer01-1 | passed per 05-01-SUMMARY: all 5 selected cases |
| 05-02-1 | 2 | VID-01, VID-03, VID-04, VID-05 | T-05-04, T-05-05, T-05-06, SC | api.video_cadence; api.frame_edges; api.scheduler_order; api.scheduler_limits | C021 | 05-01 | new cases created/registered in owning task before verify; inherited cases exist | passed with combined six-case C021/C022 run; 6/6 in 0.19 s on 2026-11-11 |
| 05-02-2 | 2 | VID-01, VID-03, VID-04, VID-05 | T-05-04, T-05-05, T-05-06, SC | api.video_geometry; api.palette_boundary | C022 | 05-02-1 | new cases created/registered in owning task before verify; inherited cases exist | passed with combined six-case C021/C022 run; 6/6 in 0.19 s on 2026-11-11 |
| 05-03-1 | 3 | VID-02, EVD-05 | T-05-07, T-05-08, T-05-09, SC | api.video_bus; api.video_bus_edges; api.request_hold; api.request_wake; api.request_boundary | C031 | 05-02 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-03-SUMMARY: 5/5 |
| 05-03-2 | 3 | VID-02, EVD-05 | T-05-07, T-05-08, T-05-09, SC | api.video_page_mode; api.video_bus; api.cost_model; api.cost_model.self_test | C032 | 05-03-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-03-SUMMARY: expanded related set 13/13 |
| 05-04-1 | 4 | VID-04, VID-05, API-06 | T-05-10, T-05-11, T-05-12, SC | api.frame_refusal; api.input_refusal | C041 | 05-03 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-04-SUMMARY: 2/2 |
| 05-04-2 | 4 | VID-04, VID-05, API-06 | T-05-10, T-05-11, T-05-12, SC | api.frame_stops; api.video_reset_load; api.frame_edges; api.reset_power_on; api.load_refusal_unchanged | C042 | 05-04-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-04-SUMMARY: 5/5 after correcting a test-only sentinel expectation |
| 05-05-1 | 5 | INP-01, API-06, HB-02 | T-05-13, T-05-14, T-05-15, SC | api.input_bits; api.input_orientation; api.input_refusal | C051 | 05-04 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-05-SUMMARY: 3/3 |
| 05-05-2 | 5 | INP-01, API-06, HB-02 | T-05-13, T-05-14, T-05-15, SC | api.input_edges; api.input_orientation; api.video_bus_edges | C052 | 05-05-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-05-SUMMARY: 3/3 |
| 05-06-1 | 6 | VID-04, VID-05, INP-01, API-06 | T-05-16, T-05-17, T-05-18, SC | api.video_snapshot; api.video_snapshot_refusal; api.snapshot_refusal; api.timer_snapshot_controls | C061 | 05-05 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-06-SUMMARY: owning command passed |
| 05-06-2 | 6 | VID-04, VID-05, INP-01, API-06 | T-05-16, T-05-17, T-05-18, SC | det.video_paths; det.video_sensitivity; det.golden; det.golden.self_test; api.header_selfcontained | C062 | 05-06-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-06-SUMMARY: final focused set 6/6; 17 affected tests also passed |
| 05-07-1 | 7 | API-06, INP-01, HB-02, CON-01, VID-04, VID-05 | T-05-19, T-05-20, T-05-21, SC | consumer.replay_edges; consumer.replay_restore; consumer.frame_tracer | C071 | 05-06 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-07-SUMMARY: first exact command selected 3 cases |
| 05-07-2 | 7 | API-06, INP-01, HB-02, CON-01, VID-04, VID-05 | T-05-19, T-05-20, T-05-21, SC | consumer.frame_replay; consumer.frame_tracer; consumer.snapshot_ppm; consumer.silence_wav | C072 | 05-07-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-07-SUMMARY: second exact command selected 4 cases |
| 05-08-1 | 8 | HB-02, CON-01, API-06, EVD-05 | T-05-22, T-05-23, T-05-24, SC | consumer.replay_controls; consumer.frame_publication_controls; consumer.refusal_controls | C081 | 05-07 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-08-SUMMARY: exact gate passed |
| 05-08-2 | 8 | HB-02, CON-01, API-06, EVD-05 | T-05-22, T-05-23, T-05-24, SC | consumer.frame_response_controls; consumer.frame_replay | C082 | 05-08-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-08-SUMMARY: exact gate passed |
| 05-09-1 | 9 | VID-02, VID-03, INP-01, EVD-05 | T-05-25, T-05-26, T-05-27, SC | video.fault.dma_cost_zero; video.fault.dma_request_omitted; video.fault.palette_boundary | C091 | 05-08 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-09-SUMMARY: all 3 video mutants |
| 05-09-2 | 9 | VID-02, VID-03, INP-01, EVD-05 | T-05-25, T-05-26, T-05-27, SC | input.fault.a_b_swapped; input.fault.option1_option2_swapped; input.fault.pause_register; input.fault.orientation; input.fault.edge_late | C092 | 05-09-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-09-SUMMARY: all 5 input mutants |
| 05-10-1 | 10 | VID-02, EVD-02 | T-05-28, T-05-29, T-05-30, SC | source builder/oracle/manifests/hygiene (no CTest claim) | C101 | 05-09 | existing scripts extended here | passed per 05-10-SUMMARY: source build, oracle, manifests and hygiene |
| 05-10-2 | 10 | VID-02, EVD-02 | T-05-28, T-05-29, T-05-30, SC | suite.page_mode.observation.lynx1; suite.page_mode.observation.lynx2; suite.page_mode.controls; suite.oracle_controls; suite.symbol_controls | C102 | 05-10-1 | new cases created/registered in owning task before verify; inherited cases exist | passed: 3 tests passed; 2 profile observations skipped at SERCTL $FD8C before any result |
| 05-11-1 | 11 | EVD-02, VID-02 | T-05-31, T-05-32, T-05-33, SC | suite.timers.video_revisit; suite.timers.observation.lynx1; suite.timers.observation.lynx2; suite.timers.strict_comparison_controls; suite.timers.cascade_model | C111 | 05-10 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-11-SUMMARY: all 5 tests; strict index-14 mismatch retained |
| 05-11-2 | 11 | EVD-02, VID-02 | T-05-31, T-05-32, T-05-33, SC | suite.timers.video_revisit_controls; suite.timers.pending_controls; suite.timers.result_controls; suite.timers.model_controls | C112 | 05-11-1 | new cases created/registered in owning task before verify; inherited cases exist | passed per 05-11-SUMMARY: all 4 controls |
| 05-12-1 | 12 | VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02, CON-01, EVD-02, EVD-05 | T-05-34, T-05-35, T-05-36, SC | consumer.frame_tracer; consumer.frame_replay; consumer.frame_response_controls; consumer.replay_controls; consumer.frame_publication_controls; api.frame_refusal; api.video_snapshot_refusal; suite.page_mode.controls; suite.timers.video_revisit_controls; claim/manifest/hygiene checks | C121 | 05-11 | existing after01–11; reviews before final gate | passed C121 focused gate: 9/9 tests; claim, manifests (11 manifests/22 files), tree hygiene, and post-write diff check passed; parent reviews/fixes still pending |
| 05-12-2 | 12 | VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02, CON-01, EVD-02, EVD-05 | T-05-34, T-05-35, T-05-36, SC | entire exact tests/inventory/fast.txt; JUnit/no-skip/artifact/history; macOS/Linux/exact-head CI records | C122 | 05-12-1 | existing after01–11; reviews before final gate | passed at reviewed/pushed/CI head `d3fb002`: macOS, self-contained Ubuntu 24.04 and hosted Ubuntu CI each accounted for 202 rows (200 passed, two declared page-mode skips, zero failures); JUnit, artifact scan, history hygiene passed; exact source fingerprint and retained timer/page-mode limits in `05-QUALIFICATION.md` |

## Wave 0 Requirements

- No new test is claimed present now. Its owner first authors a failing semantic case/driver, exact dispatcher/CTest registration and sorted inventory before configure/build/verify. Later consumers require that completed plan. Missing case or compile failure never counts as semantic RED.
- Every registration updates fast.txt and the existing repeatability claim's inventory binding in the same task because cmake/Tests.cmake checks equality at configure. These shared sidecars force sequential waves; there is no deferred-inventory waiver.
- The original public-only tracer is first. Its single atomic integration exception and other small registration/serialization/provenance sidecars are documented in01 and05-SOURCE-AUDIT.md; no disconnected foundation task.
- Source/symbol/oracle identities stay exact. Page-mode runtime blockers produce an exact declared skip and missing evidence, never a green suite. Stale declarations fail. New timer environments cannot inherit the old pending exception.
- Mutants require a unique changed source anchor, successful build, exact intended semantic/timing diagnostic and fresh positives. Crashes, sanitizer, timeout, missing output or generic nonzero do not prove detection.
- Apps/tests remain headless/backgrounded, with finite guest/frame/record limits and current-invocation contained/nonce-owned scratch. No orphan watch loop. Clean only verified successful invocation outputs; failed evidence stays attributable.
- Buffer/snapshot cases compare whole-machine/all-output sentinels and canaries, full capacity spans and metadata. All17 probe items are recorded in05-SOURCE-AUDIT.md (9 explicit predicates,8 flagged classifier assumptions); no unclassified backstop invented.
- Feedback times are recorded for C121 and C122. Routine groups target<60s; source mutation/suite batches use measured finite bounds. One final canonical gate per reviewed-source environment; no duplicate broad rerun or irrelevant 2GB vector rerun was made.

## Exact command registry

Copied from canonical PLAN bodies after path/test-name correction. Every command assumes checkout root and has the immediate fails_when in its owning plan. This is a mapping, not a second schedule.

### C011 — 05-01-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.frame_tracer$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C012 — 05-01-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(consumer\.(frame_tracer|public_api|snapshot_ppm|silence_wav)|api\.header_selfcontained)
    <fails_when>Nonzero exit, no matching tests, timeout, sanitizer finding, or any named semantic assertion fails. A negative-control wrapper is green only after the child built successfully, exited with its exact expected code and emitted the intended diagnostic with fresh passing brackets.</fails_when>
  </verify>
  <done>The standalone header and public-only tracer pass, old snapshot/silence behavior remains evidenced, and documented packet sizes/policy identities match the implementation.</done>
</task>
</tasks>

<threat_model>
ASVS1; critical/high findings block completion. Existing caller serialization applies; no network/database/frontend/AI surface is added.

## Trust Boundaries
| Boundary | Protection |
|---|---|
| Caller/guest bytes to machine | Validate spans, policy and reachable state before mutation |
| Guest observations to host evidence | Independent semantic assertions, exact source/config identity |
| Test scratch to checkout | Invocation-owned paths; no binary assets committed |

## STRIDE Threat Register
| Threat ID | Category | Component | Severity | Disposition | Mitigation Plan |
|---|---|---|---|---|---|
| T-05-01 | Tampering | native buffer→machine | high | mitigate | preflight all spans before mutation; frame_driver sentinel and public-only checks |
| T-05-02 | Repudiation | homebrew→frame evidence | medium | mitigate | independent known pen/palette/progress assertions and pinned original source recipe |
| T-05-03 | Denial of service | timer→frame scheduler | high | mitigate | finite event cursors, positive bus costs and half-open budget; no second clock |
| T-05-SC | Tampering | pinned toolchain/source | high | mitigate | Reuse exact SHA256 pin/build-record/notice gates; no package install or new dependency. Unknown package requires legitimacy audit and blocking human gate before install. |
</threat_model>

<verification>
Run owning focused commands during implementation, headless/backgrounded; record exact selected CTest IDs and feedback time. The task authors/registers its named cases and inventory before verification: that is its RED/scaffold step, not a missing-test green. All later consumers require the predecessor plan. Do not claim RED where existing behavior already passes. Every registration includes same-task sorted fast inventory and claim-hash rebinding because configure enforces inventory equality. Shared inventory/claim sidecars force sequential waves; this measured coupling is preferred over unneeded test-inventory architecture. Preserve strict source comparison failures separately from successful observation/control wrappers. No firmware/game downloads, physical tests, LibFuturo edits or publication actions.
</verification>

<success_criteria>
- An original source-built tier-A guest produces two independently checked raw frames and changes a known pen in response to a recorded A press through the public-only consumer.
- The tracer keeps a single scheduler and an instance-owned bounded accumulator; snapshot format 5 carries its real state.
- The native packet has at most 8160 pen bytes, 3264 raw palette bytes and 102 uint64 line ticks; no host presentation enters the core.
- All authored high-severity mitigations have current evidence; no owner gate is silently closed.
</success_criteria>

<output>
Create `.planning/phases/05-display-raw-video-and-input/05-01-SUMMARY.md` when done. Report blockers with exact source/build/environment identity; keep EVD-02, recurring EVD-05 and CON-01 partial wherever remaining obligations continue.
</output>

```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C021 — 05-02-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_cadence|frame_edges|scheduler_order|scheduler_limits)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C022 — 05-02-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_geometry|palette_boundary)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C031 — 05-03-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_bus|video_bus_edges|request_hold|request_wake|request_boundary)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C032 — 05-03-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_page_mode|video_bus|cost_model|cost_model\.self_test)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C041 — 05-04-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(frame_refusal|input_refusal)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C042 — 05-04-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(frame_stops|video_reset_load|frame_edges|reset_power_on|load_refusal_unchanged)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C051 — 05-05-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(input_bits|input_orientation|input_refusal)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C052 — 05-05-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(input_edges|input_orientation|video_bus_edges)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C061 — 05-06-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_snapshot|video_snapshot_refusal|snapshot_refusal|timer_snapshot_controls)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C062 — 05-06-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(det\.(video_paths|video_sensitivity|golden|golden\.self_test)|api\.header_selfcontained)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C071 — 05-07-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(replay_edges|replay_restore|frame_tracer)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C072 — 05-07-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(frame_replay|frame_tracer|snapshot_ppm|silence_wav)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C081 — 05-08-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(replay_controls|frame_publication_controls|refusal_controls)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C082 — 05-08-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(frame_response_controls|frame_replay)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C091 — 05-09-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^video\.fault\.(dma_cost_zero|dma_request_omitted|palette_boundary)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C092 — 05-09-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^input\.fault\.(a_b_swapped|option1_option2_swapped|pause_register|orientation|edge_late)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C101 — 05-10-1

```sh
sh scripts/build-page-mode.sh --out build/check/scratch/phase05-build && sh scripts/generate-page-mode-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/phase05-page-oracle.txt && sh scripts/check-manifests.sh && sh scripts/hygiene.sh --tree
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C102 — 05-10-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(page_mode\.(observation\.(lynx1|lynx2)|controls)|oracle_controls|symbol_controls)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C111 — 05-11-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(video_revisit|observation\.(lynx1|lynx2)|strict_comparison_controls|cascade_model)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C112 — 05-11-2

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(video_revisit_controls|pending_controls|result_controls|model_controls)$'
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C121 — 05-12-1

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(consumer\.(frame_tracer|frame_replay|frame_response_controls|replay_controls|frame_publication_controls)|api\.(frame_refusal|video_snapshot_refusal)|suite\.(page_mode\.controls|timers\.video_revisit_controls))$' && sh scripts/check-claims.sh && sh scripts/check-manifests.sh && sh scripts/hygiene.sh --tree
```

Failure: nonzero exit, missing test match, timeout, sanitizer or wrong intended child assertion; controls require successful build/exact code and diagnostic/fresh clean brackets.

### C122 — 05-12-2

```sh
sh scripts/check.sh
```

Failure: Nonzero exit, missing/extra/duplicate JUnit rows, undeclared or stale skips, artifact/history violation, or incomplete final-source macOS/Linux/hosted evidence. This local command alone does not satisfy the separate required self-contained Linux and exact-head CI records.



## Manual-Only Verifications

None for scoped software behavior. Physical calibration, baseline adoption, licence/rights and shared LibFuturo changes remain their explicit external owner boundaries; they are not sign-offs for repeatability.

## Validation Sign-Off

- [x] Every actual task maps to an automated command, intended proof and dependency.
- [x] No three consecutive tasks lack planned automated feedback.
- [x] Each new test is created/registered before its owning verification; later users depend on that producer.
- [ ] Planned tests are implemented and meaningful semantic failures observed.
- [ ] No watch-mode/background orphan process.
- [x] Actual feedback times recorded and broad verification reused appropriately.
- [ ] nyquist_compliant true only after final coverage audit.

Wave 0 and the C122 final-source qualification are complete. Nyquist compliance and phase-wide requirement status remain under root's final coverage/security closeout; this Task 2 record does not advance the roadmap or close phase-wide requirements.
