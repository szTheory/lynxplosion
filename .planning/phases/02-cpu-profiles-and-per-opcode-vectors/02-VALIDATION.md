---
phase: "2"
slug: "cpu-profiles-and-per-opcode-vectors"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-09"
---

# Phase 2 — Validation Strategy

> Per-phase validation contract. Source: the Validation Architecture section of 02-RESEARCH.md.
> Goal (`.planning/AUTONOMY.md`): zero human UAT; every requirement has an automated test and a
> named negative control. Items open by design (O1 baseline, hardware accuracy, Lynx cycle timing)
> are recorded as unverified in the inventory headers and profile document, never as passed.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake 3.25+), C test executables with `tests/check.h`, CMake `-P` scripts, POSIX sh controls; exact-inventory gate and JUnit completeness |
| **Config file** | `CMakePresets.json` (`check`: Debug, ASan+UBSan, `jobs: 4`), `tests/tests.cmake` includes |
| **Quick run command** | `ctest --preset check -R '^(cpu\|core\|api)\.'` (about 5 to 10 s) |
| **Full suite command** | `sh scripts/check.sh` |
| **Not in check** | `sh scripts/vectors-full.sh run` (network, about 2 GB): Phase 2 verification, every pin bump, before any claim |
| **Estimated runtime** | check.sh ~47 s local / ~27 s CI today; Phase 2 adds about 4 to 6 s |

---

## Sampling Rate

- **After every task commit:** `ctest --preset check -R '^(cpu|core|api)\.'`
- **After every plan:** `sh scripts/check.sh` (the last task of each plan)
- **Before `/gsd-verify-work`:** check.sh green, `scripts/vectors-full.sh run` green for both profiles (all 10,000 vectors per file), results recorded in the verification report, same golden digest on Linux (container or CI)
- **Max feedback latency:** 60 seconds locally

---

## Requirement-Level Verification Map (task IDs filled in by the planner, 2026-11-09)

| Requirement | Test IDs (plan-task) | Negative control (plan-task) | Status |
|-------------|----------------------|------------------------------|--------|
| CPU-01 | `cpu.vectors.lynx1`, `cpu.vectors.lynx2`, `cpu.vectors.none` (02-04-T2; each compares `tests/inventory/vectors/<id>.txt` byte for byte, then asserts); full lane for none, lynx1, lynx2 (02-05-T3 script, run at 02-11-T2) | `cpu.vectors.no_step`, `.corrupt_expect`, `.malformed`, `.inventory_drift` (includes the none cases), `.sample_identity`, `.mutant_profile` (02-05-T1); `policy.vectors_pin.self_test` (02-05-T2); reader malformed self-test (02-04-T1) | ✅ green |
| CPU-02 | `cpu.tables` (02-01-T2), `api.init_refusal` (02-09-T2), `api.profile_select`, `cpu.profile_divergence` (02-10-T1), `det.profile_agreement` (02-10-T3), `cpu.scenarios.lynx1/lynx2` bit-op scenarios (02-06-T2) | `cpu.tables.self_test` (02-01-T3); `cpu.vectors.mutant_profile` (02-05-T1); `cpu.scenarios.wrong_profile.self_test` (02-06-T2) | ✅ green |
| CPU-03 | `cpu.cycles` incl. cost tables (02-01-T2, 02-08-T1), `core.cpu_includes` (02-01-T3), `fixtures.cpu_asm` (02-07-T2), `api.advance_budget` (02-09-T2), `api.cost_model`, `api.advance_slicing`, `api.seam_agreement.none`, `.lynx1`, `.lynx2` (02-10-T2) | `api.cost_model.self_test` (persistent constant-cost control, 02-10-T2); `core.cpu_includes.self_test` (02-01-T3); `fixtures.cpu_asm.self_test` (02-07-T2); `<=` loop mutation failing `api.advance_budget` is one-time evidence recorded in 02-09-SUMMARY (02-09-T2), not a persistent test | ✅ green |
| CPU-04 | `cpu.cycles` interrupt/BRK/reset checks (02-01-T3), `cpu.scenarios.lynx1`, `cpu.scenarios.lynx2` with counts per source tag (02-06-T1/T2); WAI/STP evidence = absence in both tables (`cpu.tables`) plus the Test 8 replica | `cpu.scenarios.test8.self_test` (`$CB` as polling op fails, 02-06-T2) | ✅ green |
| CPU-05 | `cpu.vectors.wrong_profile`, `cpu.vectors.wrong_profile_reverse` (02-05-T1; failing set = exactly the 32 `$x7/$xF` opcodes); `api.profile_select` (02-10-T1) | the controls are the evidence; `cpu.vectors.no_step` (02-05-T1) shows they exercise the CPU | ✅ green |
| Phase 1 properties | `det.two_runs`, `det.two_instances`, `det.golden`, `det.sensitivity`, `det.sensitivity_state`, `api.init_refusal`, `api.load_refusal_unchanged`, `api.snapshot_roundtrip`, `api.snapshot_refusal`, `api.advance_budget` (02-09-T2/T3), `core.*`, `api.abi_symbols`, `hygiene.*`, `docs.*`, `policy.*`, tier vocabulary cases (02-02-T1) | existing controls, plus UNDEFINED stop leaves the buffer byte-identical (02-09-T2); `det.sensitivity_state` flips every new CPU field and the cost model (02-10-T3); golden unchanged across the neutral 02-08 | ✅ green |

*Status: ✅ green · ✅ green · ❌ red · ⚠️ flaky*

---

> Plan-check W-4 accepted (iteration 2): the last task of each plan runs the full `sh scripts/check.sh` by design (VALIDATION sampling: per plan); per-task verification elsewhere uses the quick slice.

---

## Wave 0 Requirements

- [x] `tests/cpu/` (reader, runner, scenarios, helper sh), `tests/inventory/vectors/*.txt`
- [x] `tests/cmake/cpu_includes.cmake`, `tests/cmake/cpu_fixtures.cmake`
- [x] `fixtures/vectors/**`, `fixtures/cpu/**`, `scripts/vectors-full.sh`
- [x] no test framework install is needed

---

## Manual-Only Verifications

All phase behaviors have automated verification. The full vector lane is a script run (network) at phase verification, not a human step.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 60s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-11-10

## Validation Audit 2026-11-10

| Metric | Count |
|---|---|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
