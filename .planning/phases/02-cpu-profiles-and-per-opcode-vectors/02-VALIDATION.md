---
phase: "2"
slug: "cpu-profiles-and-per-opcode-vectors"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: draft
nyquist_compliant: false
wave_0_complete: false
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

## Requirement-Level Verification Map (task IDs are filled in by the planner)

| Requirement | Test IDs | Negative control | Status |
|-------------|----------|------------------|--------|
| CPU-01 | `cpu.vectors.lynx1`, `cpu.vectors.lynx2` (each compares `tests/inventory/vectors/<id>.txt` byte for byte, then asserts); full lane at verification | `cpu.vectors.no_step`, `.corrupt_expect`, `.malformed`, `.inventory_drift`, `.sample_identity`, `.mutant_profile`, `policy.vectors_pin.self_test` | ⬜ pending |
| CPU-02 | `cpu.tables`, `api.profile_select`, `api.init_refusal`, `cpu.profile_divergence`, `det.profile_agreement` | `cpu.tables.self_test`; `cpu.vectors.mutant_profile` | ⬜ pending |
| CPU-03 | `cpu.cycles`, `api.cost_model`, `api.advance_slicing`, `api.advance_budget`, `core.cpu_includes` | constant-cost fault fails `api.cost_model`; `core.cpu_includes.self_test`; `<=` mutation fails `api.advance_budget` | ⬜ pending |
| CPU-04 | `cpu.scenarios.lynx1`, `cpu.scenarios.lynx2` (counts per source tag) | `cpu.scenarios.test8.self_test` (`$CB` as polling op fails) | ⬜ pending |
| CPU-05 | `cpu.vectors.wrong_profile`, `cpu.vectors.wrong_profile_reverse` (failing set = exactly the 32 `$x7/$xF` opcodes) | the controls are the evidence; `cpu.vectors.no_step` shows they exercise the CPU | ⬜ pending |
| Phase 1 properties | `det.two_runs`, `det.two_instances`, `det.golden`, `det.sensitivity*`, `api.init_refusal`, `api.load_refusal_unchanged`, `api.snapshot_roundtrip`, `api.snapshot_refusal`, `api.advance_budget`, `core.*`, `api.abi_symbols`, `hygiene.*`, `docs.*`, `policy.*` | existing controls, plus UNDEFINED stop leaves the buffer byte-identical; `det.sensitivity_state` flips every new CPU field | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/cpu/` (reader, runner, scenarios, helper sh), `tests/inventory/vectors/*.txt`
- [ ] `tests/cmake/cpu_includes.cmake`, `tests/cmake/cpu_fixtures.cmake`
- [ ] `fixtures/vectors/**`, `fixtures/cpu/**`, `scripts/vectors-full.sh`
- [ ] no test framework install is needed

---

## Manual-Only Verifications

All phase behaviors have automated verification. The full vector lane is a script run (network) at phase verification, not a human step.

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
