---
phase: "1"
slug: "evidence-harness-and-walking-skeleton"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-09"
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Goal (`.planning/AUTONOMY.md`): zero human UAT. Every gate has a negative control that must fail.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake ≥3.25, preset schema 6), plain C test executables with a small project-authored check header, sh/CMake script tests |
| **Config file** | `CMakePresets.json`, `CMakeLists.txt`, `tests/inventory/fast.txt`, `tests/inventory/skips.txt` (Wave 0 creates) |
| **Quick run command** | `ctest --preset check -R '<slice>'` (after first configure) plus `shellcheck -s sh scripts/*.sh` |
| **Full suite command** | `scripts/check.sh` |
| **Estimated runtime** | ~30–60 seconds locally on a warm build; the cold cc65 build adds ~5 s (dev host). CI time is measured in-phase. |

---

## Sampling Rate

- **After every task commit:** Run the quick slice for the touched area (the task's `<automated>` command).
- **After every plan wave:** Run `scripts/check.sh` (each plan is its own wave; its last task runs it).
- **Before `/gsd-verify-work`:** `scripts/check.sh` green locally AND the same commit green on GitHub Actions (`gh run view`), wall times recorded.
- **Max feedback latency:** 60 seconds locally.

---

## Per-Task Verification Map

Task IDs are `<plan>-T<n>` (for example `01-04-T1` is plan 01-04, Task 1). Per-task commands use the touched slice (`cmake --preset check && cmake --build --preset check && ctest --preset check -R '<slice>'`); the last task of each plan also runs `scripts/check.sh` as the plan-end (wave-end) full check.

| Requirement | Task ID(s) | Test Type | Automated Command | Negative control | Status |
|-------------|------------|-----------|-------------------|------------------|--------|
| FND-01 | 01-01-T1, 01-01-T2, 01-01-T3, 01-04-T2, 01-11-T2, 01-11-T3, 01-13-T2 | e2e | `scripts/check.sh`; CI runs the same; run selected by `headSha == HEAD`, `gh run watch` green | controls below prove the failure paths; `noTestsAction: error` | ⬜ pending |
| FND-02 | 01-09-T1, 01-09-T2 | script | `policy.pins`, `policy.no_network`, `fetch_pin.mismatch.self_test` | wrong digest ⇒ exit 1, DEST absent; bad pins rows rejected; network tool in tests ⇒ reject | ⬜ pending |
| FND-03 | 01-10-T1, 01-10-T2 | script | `hygiene.manifests` | missing/unknown/duplicate key, bad sha256 or tier, duplicate identity, uncovered fixture ⇒ reject | ⬜ pending |
| FND-04 | 01-09-T2, 01-10-T2, 01-11-T1, 01-11-T3 | script | `hygiene.firmware_scan` + post-workflow `artifacts` step | seeded 512 B NUL and no-NUL files in every zone; unmanifested binary; unlisted or modified pin-zone binary; file added or changed after the cc65 build list; build list with a mismatched pin digest; toolchain magic outside build output paths ⇒ reject; removed ⇒ pass | ⬜ pending |
| FND-05 | 01-05-T1, 01-05-T2 | script + nm | `core.includes`, `core.no_float`, `core.nm_undefined`, `core.nm_globals`, `api.abi_symbols` | stdio include, float, mutable static, `time()`/`malloc()` fixtures ⇒ reject | ⬜ pending |
| FND-06 | 01-01-T2, 01-04-T1, 01-04-T2 | CMake script | configure-time inventory equality + `inventory.junit` + `policy.no_skip` | missing, extra, duplicate, skipped, undeclared skip, self-skip property ⇒ reject | ⬜ pending |
| FND-07 | 01-12-T1, 01-13-T1 | script | `docs.policy`, `docs.provenance` | exposure row removed / banner missing / adapted file dropped from provenance ⇒ reject | ⬜ pending |
| FND-08 | 01-12-T2 | script | `docs.claim_template` | missing field, stale inventory hash, Lynx profile ⇒ reject | ⬜ pending |
| FND-09 | 01-10-T2, 01-13-T2 | check.sh step + ctest | cc65 wall time printed; `toolchain.probe` hash matches its manifest; CI log lines | wrong expected hash ⇒ reject | ⬜ pending |
| API-01 | 01-01-T1, 01-05-T2, 01-06-T1 | C test | `api.skeleton`, `api.instances_independent`, `core.nm_globals` | mutation of one leaves the other byte-identical | ⬜ pending |
| API-02 | 01-06-T1 | C test | `api.load_refusal_unchanged` | each refusal case + one accepted load showing mutation is detectable | ⬜ pending |
| API-03 | 01-01-T1, 01-06-T2 | C test | `api.skeleton`, `api.advance_budget` | boundary budgets (0, 1, huge, `UINT64_MAX`); image ending at `0x10000` | ⬜ pending |
| API-04 | 01-02-T1, 01-03-T2, 01-06-T2 | C test + script | `api.identity`, `api.reset_power_on`, `runner.cli` | different seed ⇒ different RAM; default path is fixed-0x00 | ⬜ pending |
| API-05 | 01-01-T1, 01-06-T1 | C test | `api.inspect_pure` | whole-instance `memcmp` + guest time unchanged | ⬜ pending |
| API-07 | 01-07-T1, 01-07-T2 | C test | `api.snapshot_roundtrip`, `api.snapshot_refusal` | every header field flip, truncation, trailing bytes ⇒ refuse, state unchanged | ⬜ pending |
| DET-01 | 01-03-T1, 01-08-T1, 01-08-T2 | e2e | `det.two_runs`, `det.golden` | `det.sensitivity`, `det.sensitivity_state` (seed/content/budget/register/time/RAM bit ⇒ digest differs) | ⬜ pending |
| DET-02 | 01-08-T1, 01-08-T2 | C test | `det.two_instances` (whole-bundle equality) | same sensitivity controls | ⬜ pending |
| DET-04 | 01-02-T1, 01-02-T2, 01-03-T1, 01-08-T3 | script | `api.identity`, `api.build_id`, `det.manifest_fields` | missing key, timestamp or absolute path ⇒ reject | ⬜ pending |
| (cross-compiler) | 01-08-T1, 01-08-T3 | e2e | `det.golden` vs `tests/golden/skeleton.digest` | edited golden ⇒ fail | ⬜ pending |
| (PII hygiene) | 01-11-T2 | script | `hygiene.tree`, `hygiene.pii.self_test`, `history` step | runtime-constructed fake token, home path, non-noreply email ⇒ reject | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `CMakeLists.txt`, `CMakePresets.json`, test-registration CMake that equals the inventory
- [ ] `tests/cmake/*.cmake` helpers (adapted from owner precedent muhtari, MIT)
- [ ] `scripts/{check.sh,fetch-pin.sh,hygiene.sh,…}`, `pins.txt`
- [ ] `tests/inventory/fast.txt`, `tests/inventory/skips.txt`, C check header
- [ ] `include/lynxplosion.h`, `core/*`, `runner/*`
- [ ] `.github/workflows/ci.yml`, `README.md`, `docs/policy/clean-room.md`, claim template, manifests

---

## Manual-Only Verifications

All phase behaviors have automated verification. The only external evidence is a GitHub Actions run, which is read with `gh run view` (automated, no human step).

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
