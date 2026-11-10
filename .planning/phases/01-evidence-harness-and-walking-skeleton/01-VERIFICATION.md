---
phase: 01-evidence-harness-and-walking-skeleton
verified: 2026-11-10T01:17:58Z
status: passed
score: 5/5 roadmap success criteria verified; 18/18 requirements satisfied
covered_files:
  - .github/workflows/ci.yml
  - .gitignore
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-01-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-01-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-02-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-02-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-03-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-03-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-04-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-04-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-05-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-05-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-06-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-06-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-07-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-07-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-08-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-08-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-09-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-09-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-10-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-10-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-11-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-11-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-12-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-12-SUMMARY.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-13-PLAN.md
  - .planning/phases/01-evidence-harness-and-walking-skeleton/01-13-SUMMARY.md
  - AGENTS.md
  - CMakeLists.txt
  - CMakePresets.json
  - README.md
  - cmake/BuildId.cmake
  - cmake/Tests.cmake
  - core/build_id.c.in
  - core/internal.h
  - core/machine.c
  - core/sha256.c
  - core/sha256.h
  - core/snapshot.c
  - docs/claims/claim.template
  - docs/claims/skeleton-repeatability.claim
  - docs/policy/clean-room.md
  - docs/policy/exposure-record.md
  - docs/provenance.md
  - fixtures/manifests/cc65-probe.manifest
  - fixtures/manifests/skeleton-image.manifest
  - fixtures/skeleton/skeleton-image.hex
  - fixtures/toolchain/probe.c
  - include/lynxplosion.h
  - pins.txt
  - runner/bundle.c
  - runner/bundle.h
  - runner/main.c
  - scripts/check-claims.sh
  - scripts/check-manifests.sh
  - scripts/check.sh
  - scripts/fetch-pin.sh
  - scripts/hygiene.sh
  - scripts/scan-binaries.sh
  - scripts/validate-kv.sh
  - tests/abi/global_fixture.c
  - tests/abi/libc_fixture.c
  - tests/abi/tests.cmake
  - tests/api/build_id_refresh.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/check.h
  - tests/cmake/abi.cmake
  - tests/cmake/check_junit.cmake
  - tests/cmake/core_includes.cmake
  - tests/cmake/docs_policy.cmake
  - tests/cmake/expect_fail.cmake
  - tests/cmake/inventory_controls.cmake
  - tests/cmake/inventory_equal.cmake
  - tests/cmake/no_float.cmake
  - tests/cmake/no_network.cmake
  - tests/cmake/no_skip.cmake
  - tests/cmake/pins.cmake
  - tests/cmake/pins_controls.cmake
  - tests/cmake/provenance.cmake
  - tests/cmake/toolchain_probe.cmake
  - tests/det/golden.cmake
  - tests/det/golden_self_test.cmake
  - tests/det/manifest_controls.sh
  - tests/det/manifest_fields.cmake
  - tests/det/runner_cli.cmake
  - tests/det/sensitivity.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/det/two_runs.cmake
  - tests/gates/check_controls.sh
  - tests/gates/claims_controls.sh
  - tests/gates/docs.cmake
  - tests/gates/fetch_pin_control.sh
  - tests/gates/inventory.cmake
  - tests/gates/manifests_controls.sh
  - tests/gates/rights.cmake
  - tests/gates/supply.cmake
  - tests/golden/skeleton.digest
  - tests/header/header.c
  - tests/hygiene/pii_control.sh
  - tests/hygiene/scan_control.sh
  - tests/inventory/fast.txt
  - tests/inventory/skips.txt
  - tests/tests.cmake
covered_digest: "v4:sha256:1d2a35fa089d331b97f241efbffdfa8be5c8360db5fe4d597934bd6d421c963e"
behavior_unverified: 0
overrides_applied: 0
flagged_prohibitions:
  - requirement_id: FND-07
    statement: "Implementers must never open GPL emulator source (Gearlynx or MAME or Holani or Beetle) while writing project code; any exposure is recorded in docs/policy/exposure-record.md and never silently dropped"
    plan_status: unresolved
    verification: null
    flag: "unverified-prohibition — human review recommended"
    judge_verdict: "non-authoritative: no contradicting evidence found (see Prohibitions section); not provable by automation"
advisory:
  - finding: "PROJECT.md 'Out of Scope' still lists 'Public publication, release or repository visibility change' (line 69) although the owner authorized the public repository (PROJECT.md Key Decisions line 186). Phase-transition document fix."
    category: other
    reason: "documentation drift, no behavioural effect; recorded by the orchestrator as an advisory item"
    evidence_status: "grep of .planning/PROJECT.md"
  - finding: "ROADMAP marks Phase 1 'Mode: mvp' but its Goal is not in 'As a [role], I want to [capability], so that [outcome].' form; `user-story.validate` returns valid=false. Verification proceeded on the goal-backward success criteria at the orchestrator's explicit request."
    category: other
    reason: "MVP-mode user-flow framing could not be applied; either reformat the goal via /gsd-mvp-phase or drop mode: mvp for this infrastructure phase"
    evidence_status: "gsd_run query user-story.validate output"
  - finding: "Local HEAD 88eb3c1 is two docs-only commits (ff167ca, 88eb3c1: 01-SECURITY.md and 01-VALIDATION.md) ahead of origin/main 8f71e6c. CI evidence is for 8f71e6c; the diff 8f71e6c..HEAD touches only two files under .planning/."
    category: other
    reason: "CI was not run for the two newest docs commits; they cannot affect scripts/check.sh results except the history/hygiene scans, which a local run of scripts/check.sh on HEAD passed"
    evidence_status: "git diff --stat 8f71e6c HEAD; local sh scripts/check.sh exit 0 on 88eb3c1"
  - finding: "scan-binaries prints 'N of exactly 512 bytes' counting text files of that size too (CI and dev host both report 1: the text file build/pins/cc65/libsrc/runtime/aslax7.s). The scan passes because the file is text; the wording could be read as a finding."
    category: other
    reason: "INFO only; the finding condition (non-text 512-byte file) was independently seeded and fails closed"
    evidence_status: "find build -size 512c; scripts/scan-binaries.sh output"
---

# Phase 1: Evidence Harness and Walking Skeleton Verification Report

**Phase Goal:** A developer runs one command that builds the core, enforces the rights and determinism boundaries, and proves that a skeleton machine is repeatable and refuses bad input without side effects.
**Verified:** 2026-11-10T01:17:58Z
**Status:** passed (with 1 flagged prohibition, FND-07, pending owner decision O6)
**Re-verification:** No, initial verification

All evidence below was produced in this verification run, not copied from SUMMARY files. SUMMARY claims were used only to know where to look.

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria, the contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `scripts/check.sh` fetches pins by SHA-256, builds, lints, runs the exact inventory, fails on hash mismatch / missing / extra test / undeclared skip, and is the PR CI gate | VERIFIED | Independent run `sh scripts/check.sh`: exit 0, "check_junit: 57 tests accounted for once each (0 declared skips)", `check: total 28s`. `tests/inventory/fast.txt` has 57 rows. CI run 42733279755 (headSha 8f71e6c, success) log shows the same steps from a clean runner: pins "verified d3e4bfa5..., 4288 files listed", cc65 built in 9s, probe, shellcheck, workflow, junit 57 accounted, artifacts, history. `.github/workflows/ci.yml` has exactly one `run:` step, `scripts/check.sh`. Seeded here: fetch-pin with a zeroed digest -> "sha256 mismatch", exit 1, destination never created; real digest -> verified. Inventory with a row removed -> "registered but not listed"; extra row -> "listed but not registered" (configure fails). JUnit with an undeclared `<skipped/>` -> exit 1; JUnit with a test removed -> exit 1. |
| 2 | Seeded 512-byte firmware-like file, unmanifested binary, forbidden core import each fail closed and pass once removed; every manifest records the ten required fields | VERIFIED | In an isolated clone: 512 NUL-byte file and 512 no-NUL-byte file in source zone -> "firmware-like 512-byte binary" rc 1, then rc 0 after removal; 303-byte unmanifested binary -> "unmanifested binary" rc 1 then 0; in the artifact zone an unlisted binary in `build/other/` and a 512-byte file in the CMake binary dir -> rc 1 each, then rc 0. Core: `#include <time.h>` -> `core_includes` rc 1; `static double` -> `no_float` rc 1; a writable global plus an undefined clock symbol -> `core.nm_undefined` and `core.nm_globals` Failed, restored -> Passed. Same-line email/home-path leak -> `hygiene.sh --tree` rc 1 (review fix CR-01 confirmed). Both manifests in `fixtures/manifests/` carry source, revision, recipe, sha256, licence, notices, allowed_use, startup_tier, profile, evidence_tier; `shasum` of `skeleton-image.hex` equals the manifest sha256; `hygiene.manifests` and `.self_test` pass in the full run. |
| 3 | Two fresh runs and two in-process instances give identical bundle digests; run manifest records the required fields; reset returns to declared power-on | VERIFIED | Two separate `lynx-run` invocations: `digests.txt` byte-identical (`cmp`), bundle digest bc45c7c6... both times. `run.manifest` contains profile = none, startup_tier = A, content_sha256, content_load_addr, input_digest (= SHA-256 of empty input), tick_budget, ticks_consumed, stop_reason, core_build, ram_policy = zero, ram_seed = 0x0000000000000000, evidence_tier = R. Changing policy/seed changes `obs.ram.hex` digest (not vacuous). Golden workload (splitmix64, seed 0123456789abcdef, 4096 ticks) reproduced here: 4e63d100...b251d equals `tests/golden/skeleton.digest`; CI (gcc, Linux) also passes `det.golden`, `det.two_instances`, `det.two_runs`. `api.reset_power_on`, `det.two_instances`, `det.manifest_fields`, `det.sensitivity` pass in the full run. |
| 4 | Refused load leaves session byte-identical; inspect changes neither state nor guest time; advance never exceeds budget and reports consumed + stop reason; snapshot restores in same build and mismatched identity is refused before any state change | VERIFIED | Behavioural tests (`api.load_refusal_unchanged`, `api.inspect_pure`, `api.advance_budget`, `api.snapshot_roundtrip`, `api.snapshot_refusal`) pass. Discrimination proven by mutation in an isolated clone: (M1) `lynx__power_on` moved before load validation -> `api.load_refusal_unchanged` Failed; (M2) budget rounded up -> `api.advance_budget` Failed; (M3) inspect bumps guest time -> `api.inspect_pure` Failed; (M4) altered identity comparison in restore -> `api.snapshot_refusal` Failed. Source read: `lynx_load_ram_image` validates every argument before the first write; `lynx_advance` returns `consumed = steps * 4 <= budget` and a stop reason; `lynx_snapshot_restore` runs ten validations (length, magic, version, header length, build id field, profile/policy/seed/content identity, payload length, payload hash, value ranges) before the single apply block. |
| 5 | Pinned cc65 builds from source on dev host and CI runner with wall time recorded; claim template and draft source-exposure/clean-room record exist | VERIFIED | Dev host: `check.sh` prints "cc65: cached build (built in 2s)" and four tool hashes plus probe sha256 e2fdd981...04af, size 594 (equals `cc65-probe.manifest`; `toolchain.probe` passes). CI runner: "cc65: built in 9s", probe sha256 identical. `docs/claims/claim.template` binds profile, content_sha256, startup_tier, evidence_tier and inventory_sha256; `docs/claims/skeleton-repeatability.claim` has inventory_sha256 d923ff49... which equals `shasum -a 256 tests/inventory/fast.txt` and `check-claims.sh --inventory-sha256`; profile = none. `docs/policy/clean-room.md` and `docs/policy/exposure-record.md` both carry the banner "DRAFT pending owner decision O6"; exposure record has one row per source (Gearlynx, MAME, Holani, Beetle, Handy, lynx-tests, MiSTer, Dormann). `docs.policy`, `docs.provenance`, `docs.claim_template` and their self-tests pass. |

**Score:** 5/5 truths verified; 0 present-but-behavior-unverified.

### PLAN must_haves (additive to the roadmap contract)

All 13 PLAN `must_haves.truths` blocks were read. Those hinging on behaviour (refusal postconditions, budget bound, snapshot validate-then-apply, two-instance independence, digest sensitivity, inventory and skip handling, script fail-closed ordering) are covered by named tests that pass and, for the four highest-risk invariants, were shown to fail under a seeded fault (above). Two plan truths asserted by documentation only were also checked directly: no LICENSE/COPYING file exists, README states "All rights reserved" and the no-BIOS statement and links the four policy documents; PROJECT.md Key Decisions records O8, the public repository, O5 open and O6 draft.

### Required Artifacts (Levels 1 to 4)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `scripts/check.sh` | single local command and CI gate | VERIFIED | steps identities, pins, cc65, probe, shellcheck, workflow, junit, artifacts, history; bare step execution with EXIT trap naming the failed step; exit 0 here and in CI |
| `core/machine.c`, `core/snapshot.c`, `core/sha256.c`, `include/lynxplosion.h` | skeleton machine behind a C ABI | VERIFIED | substantive (init/reset/load/advance/inspect/identity/snapshot); wired into `lynx_core`, `lynx_core_plain`, runner and tests; data flows: runner digests derive from real machine state (sensitivity tests flip register, time, stack byte, seed, content byte, budget) |
| `runner/main.c`, `runner/bundle.c` | `lynx-run` and bundle writer | VERIFIED | executed twice here; manifest and sorted `digests.txt` produced; usage exit 2 behaviour covered by `runner.cli` |
| `pins.txt`, `scripts/fetch-pin.sh` | SHA-256-before-unpack fetch | VERIFIED | exercised live here against the real archive (ok) and a corrupted digest (refused) |
| `scripts/scan-binaries.sh`, `scripts/hygiene.sh` | rights and PII scans | VERIFIED | seeded controls fail then pass (see truth 2) |
| `fixtures/manifests/*.manifest`, `scripts/check-manifests.sh` | fixture manifests and validator | VERIFIED | all ten keys present; every fixture covered |
| `tests/inventory/fast.txt`, `skips.txt`, `tests/cmake/check_junit.cmake`, `inventory_equal.cmake` | exact inventory | VERIFIED | 57 rows, 0 declared skips; missing/extra/undeclared skip all fail |
| `docs/policy/*`, `docs/provenance.md`, `docs/claims/*` | FND-07/08 documents | VERIFIED | present, draft banners intact, claim bound to current inventory hash |
| `.github/workflows/ci.yml` | CI runs only the check | VERIFIED | one `run:` step; actions pinned by commit SHA; ubuntu-24.04 |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `.github/workflows/ci.yml` | `scripts/check.sh` | sole `run:` step | WIRED | `gh run view 42733279755 --log` shows the script's step output |
| `scripts/check.sh` | `fetch-pin.sh`, cc65 build, `cmake --workflow --preset check`, `check_junit.cmake`, `scan-binaries.sh --artifacts`, `hygiene.sh --history` | ordered steps | WIRED | each step's banner and wall time present in local and CI logs |
| `cmake/Tests.cmake` | `tests/inventory/fast.txt` | configure-time equality | WIRED | seeded missing/extra row fails configure |
| `runner/main.c` | `core` (`lynx_*` API, core SHA-256) | static library link | WIRED | digests reproduce the golden |
| `tests/golden/skeleton.digest` | `det.golden` | observation digest compare | WIRED | independently reproduced value equals golden |
| `docs/claims/*.claim` | `tests/inventory/fast.txt` | `inventory_sha256` | WIRED | equals current file hash; `check-claims.sh` rejects stale |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|----------|---------------|--------|--------------------|--------|
| `run.manifest` / `digests.txt` | content_sha256, tick counts, build id, digests | core identity + advance results + build-time id | Yes (changes with seed, content, budget; build id carries git rev) | FLOWING |
| snapshot blob | payload hash, identity header | live instance state | Yes (round-trip and refusal tests exercise both) | FLOWING |

### Behavioral Spot-Checks and Test Execution

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Whole gate | `sh scripts/check.sh` | exit 0, 57/57, 28 s | PASS |
| Run twice | `lynx-run ... --ticks 4000` twice, `cmp digests.txt` | identical | PASS |
| Golden digest | `lynx-run` golden workload | 4e63d100...b251d = golden file | PASS |
| Fail-closed controls | seeded 512-byte / unmanifested binary / `<time.h>` / `double` / writable global + clock symbol / PII / inventory / JUnit skip / hash mismatch | each failed, each passed after removal | PASS |
| Mutation of refusal, budget, inspect, snapshot invariants | four seeded faults in a scratch clone | each intended test Failed | PASS |

Mutations and seeds were applied only in a scratch clone under the session scratchpad; the working tree has no modification (`git status` shows only the untracked `.planning/state.json`).

### Probe Execution

No `scripts/*/tests/probe-*.sh` probes are declared by this phase. The toolchain probe (`fixtures/toolchain/probe.c` -> `.lnx`) is a CTest case (`toolchain.probe`, `.self_test`), passing locally and on CI with the same probe hash.

### Requirements Coverage

Every ID in the phase header is claimed by at least one PLAN `requirements:` field, and REQUIREMENTS.md maps exactly these 18 IDs to Phase 1 (no orphans; the traceability table's other rows belong to Phases 2 to 6).

| Requirement | Source plan(s) | Description | Status | Evidence |
|-------------|----------------|-------------|--------|----------|
| FND-01 | 01-01, 01-04, 01-11, 01-13 | one local command = PR CI gate | SATISFIED | truth 1; CI run 42733279755 |
| FND-02 | 01-09 | pins verified by SHA-256 | SATISFIED | truth 1 (live fetch ok / mismatch refused); `policy.pins`, `policy.no_network` |
| FND-03 | 01-10 | fixture manifests with required fields | SATISFIED | truth 2; `hygiene.manifests` |
| FND-04 | 01-11 | fail closed on firmware-like / unmanifested binary | SATISFIED | truth 2 (seeded 512-byte file rejected, then accepted) |
| FND-05 | 01-05 | forbidden-import check with negative control | SATISFIED | truth 2 (include, float, nm undefined, nm globals); self-tests pass |
| FND-06 | 01-04 | exact inventory, visible skips | SATISFIED | truth 1 |
| FND-07 | 01-12 | source-exposure record and draft clean-room policy | SATISFIED as draft (by design) | truth 5; stays draft pending O6; see flagged prohibition |
| FND-08 | 01-12 | claim template | SATISFIED | truth 5 |
| FND-09 | 01-10, 01-13 | cc65 reproducible build on dev host and CI, wall time | SATISFIED | truth 5 (2 s cached / 9 s CI; identical probe hash) |
| API-01 | 01-01, 01-05, 01-06 | independent instances | SATISFIED | `api.instances_independent`, `core.nm_globals` |
| API-02 | 01-06 | refused load leaves session unchanged | SATISFIED | truth 4 (M1) |
| API-03 | 01-01, 01-06 | bounded advance with consumed + stop reason | SATISFIED | truth 4 (M2). Stop reasons FRAME and WATCH are defined but reserved for later phases, as documented in CONTEXT |
| API-04 | 01-02, 01-03, 01-06 | reset to declared power-on, RAM policy seed in identity | SATISFIED | truth 3 |
| API-05 | 01-01, 01-06 | side-effect-free inspect | SATISFIED | truth 4 (M3) |
| API-07 | 01-07 | snapshot same build, validate before replace | SATISFIED | truth 4 (M4) |
| DET-01 | 01-03, 01-08 | two fresh runs, identical bundle digests | SATISFIED | truth 3 |
| DET-02 | 01-08 | two instances in one process | SATISFIED | `det.two_instances` |
| DET-04 | 01-02, 01-03, 01-08 | run manifest fields | SATISFIED | truth 3; `det.manifest_fields` |

### Honouring of CONTEXT decisions and owner boundaries

- O8 accepted default honoured: C17 with extensions off, CMake presets/Ninja/CTest, C ABI with opaque handle, caller-provided instance memory, no allocation in core (verified by `core.nm_undefined`).
- Documented deviations (two-step cc65 make, configure-time inventory equality, SHA-256 in `core/`, build-list amendment RC-1) are present exactly as recorded in 01-CONTEXT.md and are not gaps.
- No licence file; README "All rights reserved"; claims name profile `none` (O1 open); every digest line is labelled "repeatability (R), not accuracy"; no firmware or game bytes anywhere (scan clean).
- Open owner decisions O1, O5, O6 (draft), O7, O9 remain open by design. Review Info findings IN-01, 02, 04, 05, 07, 08, 09 remain deferred by design.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | | `TBD`/`FIXME`/`XXX`/`TODO`/`HACK`/placeholder scan over all tracked files outside `.planning/` | none | only `mktemp ...XXXXXX` templates matched; no debt markers, no stubs; every API function has real behaviour and tests |

### Flagged Prohibition (FND-07), autonomous-mode soft gate

`must_haves.prohibitions` of plan 01-12 carries one `status: unresolved, flagged: true, verification: null` item: "Implementers must never open GPL emulator source (Gearlynx, MAME, Holani, Beetle) while writing project code; any exposure is recorded in docs/policy/exposure-record.md and never silently dropped." All other prohibitions in the plans (FND-01, FND-02, FND-04, FND-05, DET-01) are `resolved` with evidence gates that were run here (hygiene.tree/pii, policy.no_network, firmware_scan, the four core boundary tests, the repeatability label).

- Non-authoritative judge verdict: no contradicting evidence. `git grep` for Gearlynx, MAME, Holani, Beetle in `core/`, `runner/`, `include/`, `scripts/`, `cmake/` finds nothing; the only hits are the policy-gate tests that require the exposure rows to exist. `docs/provenance.md` lists the adapted helpers (muhtari MIT, SplitMix64 public domain, FIPS 180-4). The exposure record honestly lists the research-phase source reads (Gearlynx, Holani at source level) rather than hiding them.
- Why this is flagged and not verified: what an implementer did or did not open cannot be established from the repository, and the rule is itself the draft O6 decision. Automation can only prove that the record exists and is complete per research files, which `docs.policy` already does.
- Flag: `unverified-prohibition — human review recommended`. Treatment: the phase completes with one flagged prohibition. It routes to the existing owner decision O6, not to UAT.

### Human Verification Required

None. Every success criterion was verified by automation in this run. The O6 decision is an owner gate already tracked in REQUIREMENTS.md and AUTONOMY.md, not a verification step.

### Gaps Summary

No gaps. The one-command harness is real, green locally and on a clean CI runner, and its negative controls are discriminating (seeded faults fail, then pass when removed; mutation of the core invariants fails the intended tests). The skeleton machine is repeatable, refuses bad input without side effects, and refuses mismatched snapshots before any state change.

Phase-transition follow-ups (not gaps): fix the PROJECT.md "Out of Scope" visibility line; decide whether Phase 1 should keep `mode: mvp` with a non-user-story goal; push the two docs-only commits so CI covers HEAD; O6 remains pending.

---

_Verified: 2026-11-10T01:17:58Z_
_Verifier: Claude (gsd-verifier)_
