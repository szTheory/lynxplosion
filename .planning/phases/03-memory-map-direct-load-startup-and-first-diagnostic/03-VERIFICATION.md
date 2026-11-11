---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
verified: 2026-11-11T00:59:24Z
status: passed
score: 46/46 must-haves verified
covered_files:
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-01-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-01-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-02-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-02-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-03-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-03-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-04-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-04-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-05-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-05-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-06-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-06-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-07-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-07-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-08-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-08-SUMMARY.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-09-PLAN.md
  - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-09-SUMMARY.md
  - CMakeLists.txt
  - README.md
  - consumer/main.c
  - core/bus.c
  - core/bus.h
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - docs/claims/skeleton-repeatability.claim
  - docs/cpu-profiles.md
  - docs/memory-startup.md
  - docs/policy/exposure-record.md
  - docs/provenance.md
  - fixtures/diagnostic/bll.cfg
  - fixtures/diagnostic/first.hex
  - fixtures/diagnostic/first.s
  - fixtures/manifests/first-diagnostic.manifest
  - include/lynxplosion.h
  - runner/bundle.c
  - runner/bundle.h
  - runner/main.c
  - scripts/check.sh
  - scripts/scan-binaries.sh
  - scripts/validate-kv.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/consumer/check_artifacts.c
  - tests/consumer/tests.cmake
  - tests/cpu/test_cpu_vectors.c
  - tests/det/bll.cmake
  - tests/det/golden.cmake
  - tests/det/golden_self_test.cmake
  - tests/det/manifest_controls.sh
  - tests/det/manifest_fields.cmake
  - tests/det/runner_cli.cmake
  - tests/det/sensitivity.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/det/two_runs.cmake
  - tests/diag/test_diag.c
  - tests/diag/tests.cmake
  - tests/golden/skeleton.digest
  - tests/header/header.c
  - tests/hygiene/scan_control.sh
  - tests/inventory/fast.txt
  - tests/tests.cmake
covered_digest: "v4:sha256:834cb389dc8580f05e324431d27b80192130856c99eb76a9b618a9ed1d9ec188"
behavior_unverified: 0
overrides_applied: 0
---

# Phase 3: Memory Map, Direct-Load Startup and First Diagnostic Verification

**Phase Goal:** As a developer, I want to run an original diagnostic through rights-clean startup tier A on the real memory map and read its RAM pass or fail result and labelled viewable snapshot, so that I can inspect reproducible guest progress through the public API without firmware.
**Verified:** 2026-11-11T00:59:24Z
**Status:** passed
**Re-verification:** No — initial verification; the earlier MVP guard stopped before a report was created.

## User Flow Coverage

| Step | Expected | Evidence | Status |
|---|---|---|---|
| Build the diagnostic | Project-authored assembly and linker configuration build as the pinned BS93 object with matching linked symbols. | [scripts/check.sh](../../../scripts/check.sh#L352), [first.s](../../../fixtures/diagnostic/first.s#L18), [fixture manifest](../../../fixtures/manifests/first-diagnostic.manifest#L1); `fixture.bll_header` passed. | ✓ |
| Start through tier A | Public BLL load validates and copies the payload, starts at its load address, records startup A, and has no firmware option. | [lynx_load_bll](../../../core/machine.c#L224), [runner CLI](../../../runner/main.c#L221), [runner controls](../../../tests/det/runner_cli.cmake#L60); `api.bll_tracer`, `api.bll_load`, `api.bll_alias`, and `runner.cli` passed. | ✓ |
| Run and inspect the result | Guest code selects its MAPCTL windows, writes the symbol-addressed result block and packed pixels; the harness accepts PASS and rejects the broken/no-run cases. | [diagnostic guest](../../../fixtures/diagnostic/first.s#L29), [symbol parser and result oracle](../../../tests/diag/test_diag.c#L137), [result validation](../../../tests/diag/test_diag.c#L561); `diag.result_controls` passed. | ✓ |
| Check map and continuation | MAPCTL routing keeps raw RAM distinct; the watch stops after the committed store and restored diagnostic state matches uninterrupted state. | [decoder](../../../core/bus.c#L33), [watch completion](../../../core/machine.c#L439), [snapshot restore](../../../core/snapshot.c#L237); map, watch and diagnostic-restore tests passed. | ✓ |
| Produce a viewable snapshot | Independent public-API consumer reads linked RAM data and publishes a P3 `SNAPSHOT` plus explicitly placeholder silence WAV and metadata. | [public consumer target](../../../CMakeLists.txt#L87), [consumer validation and output](../../../consumer/main.c#L369), [artifact parser](../../../tests/consumer/check_artifacts.c#L403); all four consumer tests passed. | ✓ |
| Outcome: inspect reproducible progress without firmware | The generated output is labelled with guest tick, startup A, selected profile, full object hash and diagnostic palette; no boot, frame or hardware-accuracy claim is made. | [consumer metadata](../../../consumer/main.c#L426), [startup and evidence limits](../../../docs/memory-startup.md#L93); `consumer.snapshot_ppm`, `consumer.silence_wav`, and `consumer.public_api` passed. | ✓ |

The flow is a CLI/API software contract, not an interactive UI. The P3 output is independently parsed as a viewable image format; no manual UAT is needed for this phase.

## Goal Achievement

### Observable Truths

| # | Roadmap success criterion | Status | Evidence |
|---|---|---|---|
| 1 | Original pinned-toolchain diagnostic starts from tier A, reports PASS by linked result symbol, and its deliberately broken form reports FAIL. | ✓ VERIFIED | `fixture.bll_header` and `diag.result_controls` passed; guest source writes `LXD3`, progress/error/done and pixels; the harness resolves strict `-Ln` symbols and checks PASS, error, no-run and timeout controls. |
| 2 | MAPCTL CPU view decodes the four windows and keeps raw RAM inspection separate. | ✓ VERIFIED | `memory.map_matrix`, its self-test, `api.memory_peek_pure`, and `memory.rom_unavailable` (including controls) passed; `core/bus.c` routes all 16 low-nibble values and handles FFF8/FFF9 before windows. This verifies the specified software decoder/refusal contract, not device or hardware behavior. |
| 3 | Manifests and claims record startup A; firmware cannot be supplied. | ✓ VERIFIED | `det.manifest_fields` and `runner.cli` passed. Public content loading is BLL-only; runner rejects `--firmware`, and the consumer exposes no firmware input. |
| 4 | Result-block watch stops after its write without changing guest digest; diagnostic restore continuation matches uninterrupted state. | ✓ VERIFIED | `api.watch_post_write`, `api.snapshot_continue`, `det.two_instances`, and `det.diagnostic_restore` passed. Watch data stays outside serialized guest state and digests; snapshot restore validates before applying and clearing host watch state. |
| 5 | Public-only consumer emits a labelled, tick-stamped viewable snapshot and WAV writer; audio remains silence in this phase. | ✓ VERIFIED | `consumer.public_api`, `consumer.snapshot_ppm`, `consumer.silence_wav`, and `consumer.refusal_controls` passed. Independent parser accepts exact 8×8 P3 pixels/metadata and valid 800-sample zero PCM WAV. |

**Score:** 46/46 must-have truths verified (5 roadmap criteria plus 41 plan-specific truths; repeated plan details were checked individually). **Behavior-unverified:** 0.

### Plan Must-Haves

| Plan | Must-have truths checked | Status | Evidence |
|---|---|---|---|
| 03-01 | Strict BLL load and CPU start; full-object identity; MAPCTL reset state; snapshot v3 identity; validate-before-mutation refusal order. | ✓ VERIFIED | `api.bll_tracer`, `api.bll_identity`, `api.snapshot_roundtrip`, `api.snapshot_refusal`; [loader](../../../core/machine.c#L224), [snapshot identity checks](../../../core/snapshot.c#L271). |
| 03-02 | Malformed/overlapping input refusal; runner startup metadata; per-object identity; copied input ownership and header-sensitive identity. | ✓ VERIFIED | `api.bll_load`, `api.bll_alias`, `api.bll_identity`, `runner.cli`, `det.manifest_fields`; [bundle identity source](../../../runner/bundle.c#L206). |
| 03-03 | Normal loading is BLL-only; flat vector adapter stays private; manifest tags are strict and unloaded runs cannot publish success. | ✓ VERIFIED | `runner.cli`, `det.manifest_fields`, CPU vector seam tests; [private adapter](../../../core/machine.c#L279), [public header](../../../include/lynxplosion.h#L147). |
| 03-04 | All MAPCTL windows/special addresses route as specified; raw/mapped reads are pure and distinct; unsupported routes refuse; bit 7 and placeholder timing are explicit. | ✓ VERIFIED | `memory.map_matrix`, `api.memory_peek_pure`, `memory.rom_unavailable`, `det.golden`; [decoder](../../../core/bus.c#L33), [mapped read preflight](../../../core/machine.c#L545). |
| 03-05 | Watch returns after committed write with bounded ticks/no replay; named controls exist; watcher state is per-instance and bad calls preserve state. | ✓ VERIFIED | `api.watch_post_write`, `api.watch_controls`, `api.watch_independent` and map/refusal controls; [watch return path](../../../core/machine.c#L443). |
| 03-06 | Watch fields do not affect snapshots/digests; uninterrupted/sliced/watched/restored runs agree; rearm/isolation/clear-on-success and preserve-on-refusal hold. | ✓ VERIFIED | `api.snapshot_continue`, `api.snapshot_refusal`, `det.two_instances`, `det.diagnostic_restore`; restore validates then clears host observation at [snapshot.c](../../../core/snapshot.c#L347). |
| 03-07 | Original source-built diagnostic reports semantic PASS/FAIL; strict symbol parser rejects invalid symbols; no-run/broken/timeout/corrupt controls fail independently; fixture provenance is recorded. | ✓ VERIFIED | `fixture.bll_header`, `diag.result_symbol`, `diag.result_controls`; [source](../../../fixtures/diagnostic/first.s#L29), [manifest](../../../fixtures/manifests/first-diagnostic.manifest#L1). |
| 03-08 | Runner and harness agree; independent consumer uses linked symbols/public API; exact snapshot and placeholder WAV metadata are checked; refusals/bounds are enforced. | ✓ VERIFIED | `diag.result_controls` and all consumer tests; [consumer validates before output](../../../consumer/main.c#L807). `tests/diag/runner_diagnostic.cmake` is generated in the build tree by [tests/diag/tests.cmake](../../../tests/diag/tests.cmake#L9) and is the driver CTest invokes. The artifact helper’s root-path miss was manually resolved to this generated, substantive, wired artifact. |
| 03-09 | Consumer validates before atomic no-replace publication; failures/interruption/conflicts preserve existing outputs; exact inventory and source qualification remain aligned. | ✓ VERIFIED | `consumer.refusal_controls`, independent artifact parser, claim/inventory gate. Recorded macOS and isolated Ubuntu checks each passed 120/120 with no skips; source-head CI passed. |

The artifact query passed 78/79 declared paths directly. Its only source-root miss was the generated diagnostic driver described above; checking the actual build output and CTest invocation confirms it exists, is substantive and wired. All other declared artifacts exist and pass their path/content checks.

Plan 03-09 prohibitions were also checked as negative constraints. No output or documentation presents tier-A repeatability as BIOS boot, cartridge compatibility, hardware accuracy, or an adopted baseline: the emitted artifact is explicitly labelled `SNAPSHOT`, and project documentation states the limits. The diagnostic fixture is project-authored and its manifest restricts use to original test content; provenance/exposure records and the BLL-only interface show no firmware, commercial game, or emulator implementation source used for this diagnostic. No licence, release, or additional rights decision is inferred by these checks.

### Required Artifacts and Key Links

| Area | Status | Evidence |
|---|---|---|
| BLL API, identity, MAPCTL bus, watch and snapshot | ✓ VERIFIED | Public API and core implementation are present; targeted loader, route, watch and continuation tests passed. |
| Diagnostic fixture, manifest, labels and oracle | ✓ VERIFIED | Pinned source/config build and linked-symbol parser are wired into diagnostic CTests; PASS and negative controls passed. |
| Runner and startup manifest | ✓ VERIFIED | `runner/main.c` calls `lynx_load_bll`; `runner/bundle.c` sources startup and full-object hash from public identity; manifest tests passed. |
| Independent consumer and generated driver | ✓ VERIFIED | `lynx-consume` links only `lynx_core`; the CMake-generated diagnostic script runs runner, API consumer and artifact-parser checks. |
| Inventory, claim, local/Linux/CI qualification | ✓ VERIFIED | Claim hash matches exact inventory; retained platform checks passed 120/120 with zero skips; source-head CI had one required check and passed. |

All 24 declared plan key links were manually traced because their `from`/`to` entries are logical symbols, not file paths accepted by the generic link query. The traced paths cover BLL payload validation → CPU start, guest request → decoder/CPU completion, identity → runner manifest/snapshot, private vector seam → test-only adapter, decoder → mapped inspection, committed write → watch event, guest-only state → digest/snapshot, pinned toolchain → fixture/labels, result symbols → public RAM reads, and validated consumer outputs → exclusive publication. CTest exercised those paths, including the generated diagnostic driver.

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|---|---|---|---|---|
| Diagnostic result | `diag_result` | Guest instructions in `fixtures/diagnostic/first.s`, read by linked symbol through public RAM API. | Yes; progress/error/done bytes are guest writes. | ✓ FLOWING |
| Snapshot pixels | `diag_pixels` | Guest copies the source-authored pixel table into RAM; consumer reads the linked extent and checks independent expected values. | Yes; consumer output is derived from runtime guest RAM. | ✓ FLOWING |
| Runner manifest | startup, hash, entry and payload identity | `lynx_get_identity` after strict BLL load. | Yes; fields are derived from the loaded core identity. | ✓ FLOWING |
| Snapshot image | `snapshot.ppm` and metadata | Consumer encodes validated guest pixels, tick and loaded identity. | Yes; output is not a hardcoded image. | ✓ FLOWING |
| WAV | `silence.wav` | Consumer’s fixed zero-sample PCM writer, intentionally pending Phase 6. | Static silence by the Phase 3 contract; labelled placeholder and checked for format/length/content. | ✓ EXPECTED PLACEHOLDER |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| BLL/header and diagnostic PASS/FAIL path | `ctest --preset check -R '^(fixture\.bll_header|diag\.result_controls)$' --no-tests=error --output-on-failure` | Exit 0; 2/2 passed. | ✓ PASS |
| MAPCTL, watch, snapshot and restore | `ctest --preset check -R '^(memory\.map_matrix|memory\.map_matrix\.self_test|memory\.rom_unavailable|memory\.rom_unavailable\.self_test|api\.memory_peek_pure|api\.watch_post_write|api\.snapshot_continue|det\.diagnostic_restore|det\.two_instances)$' --no-tests=error --output-on-failure` | Exit 0; 9/9 passed. | ✓ PASS |
| Public consumer, snapshot, WAV and refusal publication | `ctest --preset check -R '^(consumer\.public_api|consumer\.snapshot_ppm|consumer\.silence_wav|consumer\.refusal_controls)$' --no-tests=error --output-on-failure` | Exit 0; 4/4 passed. | ✓ PASS |
| BLL refusal, startup manifest and runner contract | `ctest --preset check -R '^(api\.bll_tracer|api\.bll_load|api\.bll_alias|det\.manifest_fields|runner\.cli)$' --no-tests=error --output-on-failure` | Exit 0; 5/5 passed. | ✓ PASS |
| Independent P3/WAV validator controls | `build/check/tests/lynx-check-artifacts self-test` | Exit 0; accepted valid artifacts and rejected pixel, WAV-size and metadata mutations. | ✓ PASS |

No full suite was rerun; the phase’s existing macOS/Linux full-gate and exact-source-head CI evidence was reused. No app or foreground UI was launched.

### Requirements Coverage

| Requirement | Phase 3 status | Evidence |
|---|---|---|
| MEM-01 | ✓ SATISFIED for the specified route/view contract | All MAPCTL values/boundaries, raw/mapped separation and unavailable-target controls passed. This is not device emulation or hardware qualification. |
| MEM-02 | ✓ SATISFIED | Strict BLL tier-A load, payload/entry identity, sourced or power-on state policy, ownership/refusal tests and actual guest execution passed. |
| MEM-03 | ✓ SATISFIED | Runner and consumer metadata plus claim carry startup A; negative manifest-tag controls passed. |
| MEM-04 | ✓ SATISFIED | BLL-only public loader and CLI; tests reject firmware option and unsupported container input. No firmware is included. |
| EVD-01 | ✓ SATISFIED | Original project-authored diagnostic reports by linked RAM block; result, symbol, pixel and failure controls passed. |
| HB-01 | ✓ SATISFIED | Independent consumer and parser verify the tick-stamped, labelled P3 snapshot. It is not a rendered Lynx frame. |
| API-08 | ✓ SATISFIED | Post-write watch and controls pass; watch stop follows the store and CPU completion. |
| DET-03 | ✓ SATISFIED for Phase 3 scope | API and diagnostic restore tests match uninterrupted guest observations/digests at common ticks. Same-core repeatability is not hardware accuracy. |
| CON-01 | ✓ SATISFIED for the Phase 3 slice | Public-only consumer emits P3 diagnostic snapshot and valid placeholder-silence WAV. Actual frames/audio remain later-phase work. |

No requirement checkbox or owner decision was changed. The fixture manifest identifies project-authored source, test-only allowed use and all-rights-reserved status; this verification grants no licence or redistribution right. The repository and output labels do not claim BIOS boot, cartridge compatibility, hardware accuracy, or an adopted Lynx baseline.

### Anti-Patterns and Test Quality

| Check | Result |
|---|---|
| Debt markers | No unreferenced `TBD`, `FIXME` or `XXX` marker. |
| Stub markers | Flat5 timing and zero-sample WAV are explicitly labelled placeholders with later-phase scope; neither is presented as implemented hardware behavior. Other empty returns are error/refusal paths, not user-visible stubs. |
| Disabled linked tests | None found. |
| Circular expected values | None found for the verified behavior: diagnostic results/pixels are checked against explicit test-owned values; PPM/WAV formats are parsed independently; digest equality is only used for the repeatability claim. |
| Assertion strength | Value/behavior assertions cover map values, result fields, symbols, pixels, ticks, digests, refusal state preservation and output bytes. |

### Decision Coverage

The canonical decision-coverage query returned “No trackable decisions in CONTEXT.md” and skipped because the phase context has no machine-readable `<decisions>` block. The implementation evidence above still follows its recorded implementation constraints; this skip does not affect the verdict.

### Human Verification Required

N/A — this is a CLI/core software phase with no interactive UI. Runtime behaviors and output-format obligations are covered by named tests and the independent artifact parser. Physical hardware, firmware/rights authority, baseline selection, project licensing, and release decisions remain explicitly outside this phase and are not inferred here.

### Gaps Summary

No Phase 3 goal gap remains. Unsupported Suzy/Mikey registers and absent ROM stop explicitly; their behavior, MAPCTL electrical/readback details, flat5 timing, display DMA, real audio, hardware accuracy, and rights/owner decisions are not established by this phase. The PPM is a diagnostic RAM snapshot and the WAV is placeholder silence, as required by the phase boundary.

---

_Verified: 2026-11-11T00:59:24Z_  
_Verifier: the agent (gsd-verifier)_
