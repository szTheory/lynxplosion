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

- **After every task commit:** Run the quick slice for the touched area.
- **After every plan wave:** Run `scripts/check.sh`.
- **Before `/gsd-verify-work`:** `scripts/check.sh` green locally AND the same commit green on GitHub Actions (`gh run view`), wall times recorded.
- **Max feedback latency:** 60 seconds locally.

---

## Per-Task Verification Map

Requirement-level map (task IDs are filled in by the planner and the executor):

| Requirement | Test Type | Automated Command | Negative control | Status |
|-------------|-----------|-------------------|------------------|--------|
| FND-01 | e2e | `scripts/check.sh`; CI runs the same; `gh run view` green | controls below prove the failure paths | ⬜ pending |
| FND-02 | script | `policy.pins`, `fetch_pin.mismatch.self_test` | wrong digest ⇒ exit 1, DEST absent; bad pins rows rejected | ⬜ pending |
| FND-03 | script | `hygiene.manifests` | missing/unknown/duplicate key, bad sha256 or tier ⇒ reject | ⬜ pending |
| FND-04 | script | `hygiene.firmware_scan` + post-workflow artifact-zone step | seeded 512 B NUL and no-NUL files, unmanifested binary ⇒ reject; removed ⇒ pass | ⬜ pending |
| FND-05 | script + nm | `core.includes`, `core.no_float`, `core.nm_undefined`, `core.nm_globals`, `api.abi_symbols` | stdio include, float, mutable static, `time()`/`malloc()` fixtures ⇒ reject | ⬜ pending |
| FND-06 | CMake script | configure-time inventory equality + `inventory.junit` | missing, extra, duplicate, skipped, undeclared skip ⇒ reject | ⬜ pending |
| FND-07 | script | `docs.policy` | exposure row removed / banner missing ⇒ reject | ⬜ pending |
| FND-08 | script | `docs.claim_template` | missing field, stale inventory hash ⇒ reject | ⬜ pending |
| FND-09 | check.sh step + ctest | cc65 wall time printed; `toolchain.probe` hash matches its manifest | wrong expected hash ⇒ reject | ⬜ pending |
| API-01 | C test | `api.instances_independent` | mutation of one leaves the other byte-identical | ⬜ pending |
| API-02 | C test | `api.load_refusal_unchanged` | each refusal case + one accepted load showing mutation is detectable | ⬜ pending |
| API-03 | C test | `api.advance_budget` | boundary budgets (0, 1, huge, `UINT64_MAX`) | ⬜ pending |
| API-04 | C test | `api.reset_power_on` | different seed ⇒ different RAM | ⬜ pending |
| API-05 | C test | `api.inspect_pure` | whole-instance `memcmp` + guest time unchanged | ⬜ pending |
| API-07 | C test | `api.snapshot_roundtrip`, `api.snapshot_refusal` | every header field flip, truncation, trailing bytes ⇒ refuse, state unchanged | ⬜ pending |
| DET-01 | e2e | `det.two_runs` | `det.sensitivity` (seed/content/budget/state bit ⇒ digest differs) | ⬜ pending |
| DET-02 | C test | `det.two_instances` | same sensitivity controls | ⬜ pending |
| DET-04 | script | `det.manifest_fields` | missing key, timestamp or absolute path ⇒ reject | ⬜ pending |
| (cross-compiler) | e2e | `det.golden` vs `tests/golden/skeleton.digest` | edited golden ⇒ fail | ⬜ pending |
| (PII hygiene) | script | `hygiene.pii` | runtime-constructed fake token, home path, non-noreply email ⇒ reject | ⬜ pending |

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
