---
phase: "3"
slug: "memory-map-direct-load-startup-and-first-diagnostic"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 3 — Validation Strategy

> Initial contract seeded from the settled Phase 3 context and research. All Phase 3 behavior is pending; these are candidate test IDs, not registered tests.

## Test Infrastructure

| Property | Value |
|---|---|
| **Framework** | CTest, C test executables, CMake-script and POSIX shell controls; exact fast-inventory gate |
| **Config** | `CMakePresets.json`, `tests/tests.cmake`, `cmake/Tests.cmake`, `tests/inventory/fast.txt` |
| **Quick run** | `cmake --preset check && cmake --build --preset check && ctest --preset check -R '^(api|memory|diag|runner|consumer|det|fixture|docs)\.'` |
| **Full suite** | `scripts/check.sh` |
| **Estimated runtime** | Phase 2 baseline: about 47 s local / 27 s CI; Phase 3 delta must be measured after tests land. |
| **Max feedback latency** | Target <60 s locally; measure after the first phase test slice. |

## Sampling Rate

- **After each task commit:** Run its targeted CTest slice; each plan's final task runs `scripts/check.sh`.
- **After each plan/wave:** Full `scripts/check.sh`; then inspect measured CI critical path on the exact tested head.
- **Before phase verification:** Full local and CI gate green, review/security findings fixed, inventory and claim hash aligned.
- No phase tests have run; no behavior is marked green.

## Requirement-Level Verification Map

| Requirement | Candidate automated tests | Negative controls / expected refusal | File exists / status |
|---|---|---|---|
| MEM-01 | `memory.map_matrix`, `api.memory_peek_pure` | Wrong bit/window mutant; FFF8/FFF9 boundary mutant; raw RAM must remain visible while CPU mapping changes | ❌ New Phase 3 tests pending |
| MEM-02 | `api.bll_load`, `fixture.bll_header` | Bad marker/magic/endian/length, empty body, truncated/trailing, range wrap, machine-buffer overlap; full instance byte-identical on refusal | ❌ Pending |
| MEM-03 | `runner.startup_manifest`, `docs.claim_template` | Missing/wrong startup tier, stale claim inventory hash | ❌ Pending |
| MEM-04 | `api.firmware_absent`, `memory.rom_unavailable` | BLL API rejects unknown/cart/firmware-shaped input; mapped absent ROM stops explicitly, never RAM/zero BIOS | ❌ Pending |
| EVD-01 | `diag.result_symbol`, `diag.result_controls` | No-run, broken guest, bad magic/schema, zero progress, zero done, corrupt result, timeout | ❌ Pending |
| API-08 | `api.watch_post_write`, `api.watch_controls` | No-hit, just-outside range, rearm/double-write mutant; matching write must already be visible at stop | ❌ Pending |
| DET-03 | `api.snapshot_continue`, `det.diagnostic_restore` | Bad version/hash/identity/invalid state; refusal preserves full machine; split-budget restore mutant | ❌ Pending |
| HB-01 | `consumer.snapshot_ppm` | Empty/unexecuted pixel buffer or wrong expected pixel must fail; metadata says SNAPSHOT, not frame | ❌ Pending |
| CON-01 | `consumer.public_api`, `consumer.silence_wav` | Consumer cannot include internal headers; malformed PPM/WAV structure and nonzero “silence” samples fail | ❌ Pending |

*Candidate IDs must be registered in CTest and listed exactly in `tests/inventory/fast.txt` before they can count as coverage. Add each named-fault control to the inventory; update the existing claim's inventory hash in the same change.*

## Cross-Cutting Invariants

- **Load/refusal:** Seed a valid running machine before each bad-object case. Assert byte-identical machine state on refusal, including input overlap; mutate/free source bytes after success and verify object identity/state are unchanged.
- **Startup/identity:** Init, reset and successful load begin with MAPCTL zero; PC is the BLL payload address. Hash the complete BLL object, report payload address/length separately. The diagnostic's own CPU write sets MAPCTL to `$0C`. Header-only mutation must change full-object identity.
- **Map/inspection:** Exercise every one of the 16 low-nibble combinations and each boundary. Raw RAM inspection and CPU-mapped peek remain independent, side-effect-free observations.
- **Watch:** Hit only after the matching CPU write completes; check byte, CPU continuation, tick and rearm. Watch settings/hit reporting do not enter guest-state digest. A watched run continued to the same final tick matches an uninterrupted run.
- **Snapshots:** Include MAPCTL/startup/container identity and guest state; restore into fresh same-identity machine and compare public final observations with uninterrupted run. Invalid restores leave target unchanged.
- **Guest evidence:** Result address comes from `-Ln` symbols; independently check nonzero magic/done, schema, semantic progress, error field and guest-time bounds. Compare expected PPM pixels from fixture constants, not from result output.
- **Rights and outputs:** Original source/config/header recipe and fixture identity are manifested. Keep WAV under temporary output and clean it; never add an unmanifested binary fixture. No BIOS or game content.
- **Consumer boundary:** Build separate C target against only public API; it obtains result/pixels from raw guest RAM, emits P3 PPM with SNAPSHOT/tick/content/startup/profile/palette metadata and valid integer PCM silence WAV.

## Wave 0 Requirements

- [ ] Add test registration/inventory scaffolding for `memory.*`, `api.bll_*`, `api.watch_*`, `diag.*`, `consumer.*`, `runner.startup_manifest`; retain negative controls as registered tests.
- [ ] Add original diagnostic source, project linker config/header recipe, generated symbol output and a source/text fixture representation plus manifest/provenance fields. Do not commit a raw firmware-like binary.
- [ ] Extend API refusal/snapshot test helpers for whole-instance equality and snapshot field controls.
- [ ] Add independent consumer target and temporary-output WAV validator.
- [ ] Preserve existing CTest/inventory/rights gates; no framework installation is needed.

## Manual-Only Verifications

None planned. Repository behavior, PPM/WAV structure, CI and evidence packaging are automatable. Physical hardware and firmware checks are explicitly outside this phase and remain owner-gated; do not report them as phase evidence.

## Validation Sign-Off

- [ ] Every planned task has automated verification or a Wave 0 dependency.
- [ ] No three consecutive implementation tasks lack a targeted automated check.
- [ ] All Wave 0 gaps resolved before relying on the coverage map.
- [ ] No watch-mode commands.
- [ ] Local feedback latency measured and below the agreed bound.
- [ ] `nyquist_compliant: true` only after validation audit verifies full coverage.

**Approval:** pending


