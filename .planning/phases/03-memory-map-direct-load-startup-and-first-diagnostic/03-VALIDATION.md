---
phase: "3"
slug: "memory-map-direct-load-startup-and-first-diagnostic"
status: validated
nyquist_compliant: false
wave_0_complete: true
created: "2026-11-10"
audited: "2026-11-10"
---

# Phase 3 — Validation Strategy

> **Coverage verdict: PARTIAL.** The in-scope behavior and its negative controls are automated and registered. One Plan 03-09 publication interruption assertion is missing; see Task 03-09 T1 below. This is a test-coverage result, separate from phase qualification: parent code/security review and exact-head CI are still pending.

## Test Infrastructure

| Property | Value |
|---|---|
| **Framework** | CTest, C test executables, CMake-script and POSIX shell controls; exact fast-inventory gate |
| **Config** | `CMakePresets.json`, `tests/tests.cmake`, `cmake/Tests.cmake`, `tests/inventory/fast.txt` |
| **Targeted command** | `cmake --build --preset check --target lynx-test-api lynx-test-diag lynx-consume lynx-consume-test lynx-check-artifacts lynx-run`, then the CTest command below |
| **Focused CTest command** | `ctest --preset check --no-tests=error --output-on-failure -R '^(api\.(bll_|snapshot_|watch_|memory_peek_pure|inspect_pure|seam_agreement\.)|memory\.|runner\.cli|fixture\.bll_header|diag\.|consumer\.|det\.(diagnostic_restore|manifest_fields|two_instances|two_runs|golden|sensitivity))'` |
| **Focused retest** | 41/41 passed on macOS arm64 after rebuilding the current API, diagnostic and consumer targets; 11.61 s CTest wall time. The large full-data CPU-vector lane was not run. |
| **Full suite / exact inventory** | `sh scripts/check.sh`; current CTest registers 120 tests, matching the 120 non-comment rows in `tests/inventory/fast.txt`. Inventory SHA-256 and claim `inventory_sha256`: `1e5eee703b7d5a8009c1cb05fd20625b228418d1eedbd92f18c53f6dc1625654`. |
| **Recorded local/Linux qualification** | Plan 03-09 checkpoint: 120/120, zero declared skips; macOS arm64 82 s total / 33.11 s CTest, Ubuntu 24.04 arm64 74 s total / 22.40 s CTest. These are checkpoint results; no exact-head CI result is recorded. |

## Requirement-Level Verification Map

| Requirement | Behavioral tests and negative controls | Coverage |
|---|---|---|
| MEM-01 | `memory.map_matrix`, `memory.map_matrix.self_test`, `api.memory_peek_pure`, `api.memory_peek_pure.self_test`; all 16 low-nibble values, window boundaries, special FFF8/FFF9 ordering, raw/mapped separation, and wrong-route mutants. | **COVERED** — software routing contract only; device behavior, readback policy and hardware accuracy remain unverified. |
| MEM-02 | `api.bll_tracer`, `api.bll_load`, `api.bll_alias`, `fixture.bll_header(.self_test)`; strict header/length/range, empty/truncated/trailing/wrapped inputs, overlap and unchanged-instance refusals, one-byte and maximum payload boundaries, full-object identity, and actual guest execution. | **COVERED** |
| MEM-03 | `det.manifest_fields(.self_test)`, `runner.cli`, `consumer.*`, `docs.claim_template`; startup A and identity fields, missing/empty/conflicting tag controls, canonical manifest and claim inventory binding. | **COVERED** |
| MEM-04 | `api.bll_tracer`, `api.bll_load`, `runner.cli`, `memory.rom_unavailable(.self_test)`; cart/unknown container refusal, no raw/firmware CLI path, unavailable ROM/vector read/write/fetch, and RAM-selected positive controls. | **COVERED** — proves supported-interface limits; it does not establish rights provenance for arbitrary caller-supplied bytes. |
| EVD-01 | `fixtures.cpu_asm`, `fixture.bll_header(.self_test)`, `diag.result_symbol`, `diag.result_controls`, `det.diagnostic_restore(.self_test)`; independent symbol/result/pixel checks plus no-run, broken guest, timeout, malformed/duplicate/range symbol, each result-field mutation and rehashed invalid snapshot controls. | **COVERED** — flat5 tick bounds are PLACEHOLDER evidence, not hardware timing. |
| API-08 | `api.watch_post_write`, `api.watch_controls(.self_test)`, `api.watch_independent`; committed store/tick/continuation, half-open budget, range edges, rearm, instance isolation, and stop-before-store/repeated-store mutants. | **COVERED** |
| DET-03 | `api.snapshot_continue(.self_test)`, `api.snapshot_refusal`, `det.diagnostic_restore(.self_test)`, `det.two_instances`; uninterrupted, sliced, watched and restored observations at common ticks; rehashed invalid-state refusal preserves state. | **COVERED** — equality establishes repeatability, not hardware accuracy. |
| HB-01 | `consumer.snapshot_ppm`, `consumer.public_api`, `consumer.refusal_controls`; independently parsed exact 8x8 P3 samples/palette and SNAPSHOT/tick/content/startup/profile metadata; empty or wrong pixel data refuses. | **COVERED** — diagnostic snapshot, not a frame or video-accuracy claim. |
| CON-01 | `consumer.public_api`, `consumer.snapshot_ppm`, `consumer.silence_wav`, `consumer.refusal_controls`; public-header boundary, independent PPM/WAV parser, exact silence samples/metadata, invalid-input refusal, existing-path preservation, injected write/close failures and concurrent publishers. | **PARTIAL** — Phase 3 delivers the documented snapshot/silence portion; real frame/audio streams are later-phase work. The interruption assertion in Task 03-09 T1 is also missing. |

## Per-Task Coverage Map

The map contains both tasks from each of the nine plans. “Covered” means the planned behavioral evidence exists and was either run in the focused retest or recorded in that plan/checkpoint. Overall qualification gates are listed separately below.

| Task | Requirements | Test evidence | Status |
|---|---|---|---|
| 03-01 T1 — strict BLL guest path | MEM-02, MEM-04 | `api.bll_tracer` | COVERED |
| 03-01 T2 — snapshot identity and latch | MEM-01, MEM-02, MEM-04, DET-03 | `api.snapshot_roundtrip`, `api.snapshot_refusal`, `api.bll_identity` | COVERED |
| 03-02 T1 — loader ownership/refusals | MEM-02, MEM-04 | `api.bll_load`, `api.bll_alias`, `api.bll_tracer` | COVERED |
| 03-02 T2 — runner startup metadata | MEM-03, MEM-04 | `runner.cli`, `det.manifest_fields(.self_test)`, `det.two_instances`, `det.sensitivity` | COVERED |
| 03-03 T1 — caller migration/private CPU seam | MEM-02, MEM-04, DET-03 | `api.seam_agreement.*`, `api.advance_*`, existing `cpu.vectors.*` inventory; full-data vector lane excluded | COVERED |
| 03-03 T2 — raw API removal/manifest controls | MEM-03, MEM-04 | `runner.cli`, `det.manifest_fields(.self_test)`, `det.two_runs`, `det.golden(.self_test)`, `det.sensitivity` | COVERED |
| 03-04 T1 — MAPCTL route matrix | MEM-01, MEM-04 | `memory.map_matrix(.self_test)` | COVERED |
| 03-04 T2 — pure inspection/unavailable policy | MEM-01, MEM-04 | `api.memory_peek_pure(.self_test)`, `api.inspect_pure`, `memory.rom_unavailable(.self_test)`, `det.golden(.self_test)` | COVERED |
| 03-05 T1 — permanent map/refusal controls | MEM-01, MEM-04 | `memory.map_matrix(.self_test)`, `api.memory_peek_pure(.self_test)`, `memory.rom_unavailable(.self_test)` | COVERED |
| 03-05 T2 — post-write watch | API-08 | `api.watch_post_write`, `api.watch_controls(.self_test)` | COVERED |
| 03-06 T1 — restore/watch isolation | API-08, DET-03 | `api.snapshot_continue(.self_test)`, `api.snapshot_refusal`, `det.two_instances` | COVERED |
| 03-06 T2 — persistent watch edges | API-08, DET-03 | `api.watch_post_write`, `api.watch_controls(.self_test)`, `api.watch_independent`, `api.snapshot_continue(.self_test)` | COVERED |
| 03-07 T1 — original diagnostic fixture | EVD-01, MEM-02, MEM-03 | `fixtures.cpu_asm`, `fixture.bll_header(.self_test)`, `hygiene.manifests(.self_test)` | COVERED |
| 03-07 T2 — result/symbol/continuation oracle | EVD-01, API-08, DET-03 | `diag.result_symbol`, `diag.result_controls`, `det.diagnostic_restore(.self_test)` | COVERED |
| 03-08 T1 — runner agreement/provenance | EVD-01, MEM-03, DET-03 | `diag.result_controls`, `fixture.bll_header(.self_test)`, `runner.cli`, `docs.provenance`, `policy.*` | COVERED |
| 03-08 T2 — public consumer output | HB-01, CON-01, MEM-03 | `consumer.public_api`, `consumer.snapshot_ppm`, `consumer.silence_wav`, `consumer.refusal_controls` | COVERED |
| 03-09 T1 — atomic consumer publication | HB-01, CON-01 | `consumer.refusal_controls` verifies conflicting publishers and write/close failures, but does not interrupt a process between staging and publish. | PARTIAL |
| 03-09 T2 — docs and delivery qualification | All Phase 3 requirements | Local and Ubuntu `sh scripts/check.sh` results in `03-09-CHECKPOINT.md`; exact-head CI is not recorded. | COVERED for test coverage; CI qualification pending |

### Missing assertion and recommended test

`consumer.refusal_controls` proves concurrent publishers yield one complete winner and one refusal, and injected write/close failures leave no final output path. There is no deterministic control that interrupts a consumer after it has created staging files but before the exclusive directory rename. Therefore the test does not establish the planned interruption assertion: **after process interruption, the final destination is absent, and any leftover private staging directory contains no `snapshot.manifest` success marker.**

Recommended test: add a test-only barrier/pause point after staged PPM/WAV creation and before manifest publication/rename. Have the parent test wait for that explicit readiness signal, terminate the child, then assert the final destination is absent and any owned leftover staging directory has no `snapshot.manifest`. Avoid a timing-only kill loop. This gap is routed to the parent; no production or test source was changed in this audit.

## Residual Assumptions and Prohibitions

The edge probe retains all 14 original observations: nine have explicit predicates and five remain flagged unclassified. Their relevant tests now run, but they remain flagged in the source classification because software observations cannot establish the missing hardware/rights premise.

| Probe item | Test evidence | Audit disposition |
|---|---|---|
| API-08 idempotency | `api.watch_controls(.self_test)` | Explicit behavior covered. |
| API-08 concurrency/instance isolation | `api.watch_independent` | Caller-serialized operations and interleaved independent instances covered. |
| DET-03 common-tick continuation | `api.snapshot_continue(.self_test)`, `det.diagnostic_restore(.self_test)` | Behavioral equality covered; not an accuracy claim. |
| MEM-01 route policy | `memory.map_matrix(.self_test)`, `memory.rom_unavailable(.self_test)` | Software route/refusal assertions covered; hardware readback/device policy stays unverified. |
| MEM-02 loader edges | `api.bll_load`, `api.bll_alias` | Empty, exact-end, overlap, refusal ordering and ownership controls covered. |
| MEM-03 startup tags | `det.manifest_fields(.self_test)` | Empty/conflict/order controls covered. |
| MEM-04 firmware boundary | `runner.cli`, `api.bll_load`, `memory.rom_unavailable(.self_test)` | No distinct firmware input path is exposed; rights/provenance of arbitrary caller bytes remains outside this proof. |
| EVD-01 result meaning | `diag.result_controls`, `diag.result_symbol` | No-run, broken, timeout, field corruption and symbol faults covered; physical accuracy is not. |
| HB-01 pixel/palette interpretation | `consumer.snapshot_ppm`, `consumer.refusal_controls` | Independent diagnostic pixel/output contract covered; no canonical video claim. |
| CON-01 publisher concurrency | `consumer.refusal_controls` | Competing publishers yield one complete output and one refusal; interruption remains the separate PARTIAL gap above. |

Two descriptor-less prohibitions in Plan 03-09 remain flagged-unverified: do not present tier-A repeatability as BIOS boot/cartridge compatibility/hardware accuracy/adopted model baseline; do not use firmware, commercial games or emulator implementation source to manufacture a diagnostic, licence or release authorization. Tests do not clear owner-gated rights, hardware or release decisions.

## Validation Sign-Off

- [x] Plans 03-01 through 03-09 have task-level automated checks; Task 03-09 T1 retains one explicit interruption-control gap.
- [x] The focused current retest is nonempty and the exact inventory remains 120 tests with zero declared skips in recorded full-gate runs.
- [x] The full-data CPU-vector lane was not run as part of this audit.
- [x] Local/macOS and Ubuntu full-gate evidence is recorded in the Plan 03-09 checkpoint.
- [ ] `nyquist_compliant: true` — withheld until the deterministic interruption assertion is added and passes.

## Phase Qualification (separate from coverage)

| Gate | Status |
|---|---|
| macOS local full check | Recorded pass: 120/120, zero declared skips. |
| Ubuntu 24.04 local full check | Recorded pass: 120/120, zero declared skips. |
| Parent code review | Pending. |
| Parent security review | Pending. |
| Exact-head CI | Pending; no CI run is recorded in the Plan 03-09 checkpoint. |
| Plan 03-09 summary | Intentionally pending parent review and exact-head CI; implementation, docs and local/Linux checks are recorded in `03-09-CHECKPOINT.md`. |

**Hardware and rights boundary:** no physical hardware, BIOS, commercial game content, release or publication qualification is included. MAPCTL/device policy, timing and snapshot/pixel repeatability are not hardware-accuracy evidence.

## Validation Audit 2026-11-10

| Metric | Count |
|---|---|
| Gaps found | 1 |
| Resolved | 0 |
| Escalated | 1 |
