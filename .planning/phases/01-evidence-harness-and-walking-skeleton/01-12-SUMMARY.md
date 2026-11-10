---
phase: 01-evidence-harness-and-walking-skeleton
plan: 12
subsystem: rights-and-evidence-docs
tags: [clean-room, exposure-record, provenance, claims, inventory-binding, cmake, shell]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-08: scripts/validate-kv.sh; plan 01-11: hygiene scans that read every new document; plans 01-01 to 01-11: the muhtari-adapted code files and the det.* tests the example claim cites"
provides:
  - "docs/policy/clean-room.md and docs/policy/exposure-record.md: draft FND-07 policy (banner DRAFT pending owner decision O6) and one exposure row per research source (Gearlynx, MAME, Holani, Beetle, Handy and libretro-handy, lynx-tests, MiSTer AtariLynx, Klaus Dormann tests), each with file and line citations into the research files"
  - "docs/provenance.md: the 13 adapted code files (muhtari, MIT, commit 7fd0386), SplitMix64, FIPS 180-4, cc65 (zlib) and lynx-tests (MIT) pins, and the open note on RESEARCH A10"
  - "tests/cmake/docs_policy.cmake and tests/cmake/provenance.cmake with tests docs.policy, docs.policy.self_test, docs.provenance, docs.provenance.self_test"
  - "docs/claims/claim.template, docs/claims/skeleton-repeatability.claim and scripts/check-claims.sh [--root DIR] [--inventory-sha256] with tests docs.claim_template and docs.claim_template.self_test (20 seeded cases in tests/gates/claims_controls.sh)"
  - "Inventory is final for Phase 1 unless plan 01-13 adds a test: 55 tests, fast.txt SHA-256 0e7506ee342a3148cb2f3e7d7471ad4ad86c087e33d0bf0f8cea7923bdc1b8ed"
affects: [01-13]

actuals:
  tokens: 8150
  tasks: 2
  commits: 2

plan_head_before: ad729eccde72809993988c0fb53c31aa6eb27fe7
plan_head_after: d60f595e8aa780c2d427ef6f0dbaff9b0b776ffc

tech-stack:
  added: []
  patterns:
    - "Gate findings are printed with message(STATUS) and a short FATAL_ERROR, because FATAL_ERROR text is re-wrapped at 79 columns and a wrapped finding cannot match an expect_fail REGEX"
    - "A configure-time self-test fixture is built from the real documents with one edit per fault and aborts configure when an edit changed nothing"
    - "A claim is bound to the SHA-256 of the exact bytes of tests/inventory/fast.txt; the gate recomputes it from the live file, and one documented command prints the value"

key-files:
  created:
    - docs/policy/clean-room.md
    - docs/policy/exposure-record.md
    - docs/provenance.md
    - docs/claims/claim.template
    - docs/claims/skeleton-repeatability.claim
    - scripts/check-claims.sh
    - tests/cmake/docs_policy.cmake
    - tests/cmake/provenance.cmake
    - tests/gates/docs.cmake
    - tests/gates/claims_controls.sh
  modified:
    - tests/tests.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "docs.policy.self_test seeds all three faults (no banner, no Holani row, no 7fd0386) in one fixture and requires the three findings in report order; this proves each rule inside the six rows the plan allows"
  - "scripts/check-claims.sh takes validate-kv.sh from its own directory, not from --root, and has --inventory-sha256 as the single documented regeneration command for claim bindings"
  - "docs.provenance matches the header phrase case-insensitively in two forms (adapted from muhtari, adapted from the owner's muhtari); scripts/hygiene.sh and cmake/Tests.cmake style headers are both covered, and hygiene.sh is listed although its phrase sits past line 10"
  - "The example claim's content_sha256 is the value from a fresh run.manifest of the golden workload (decoded content), not the sha256 of the .hex fixture file"

patterns-established:
  - "Pattern: documents that a check parses keep one line per entry in backticks (provenance paths) or one table row per source starting with the source name, so a self-test can drop exactly one line"

requirements-completed: [FND-07, FND-08]

coverage:
  - id: D1
    description: "Draft clean-room policy with the O6 banner, the four GPL emulators named as closed, the allowed sources and the same-day accidental-exposure rule; FND-07 stays a draft and O6 is not decided"
    requirement: FND-07
    verification:
      - kind: integration
        ref: "ctest docs.policy, docs.policy.self_test"
        status: pass
    human_judgment: true
    rationale: "Whether the rule binds implementers is owner decision O6; the text is a draft for the owner to accept or restrict"
  - id: D2
    description: "Exposure record with one row per research source, rows built from the research files only; removing a named emulator row or the banner fails docs.policy"
    requirement: FND-07
    verification:
      - kind: integration
        ref: "ctest docs.policy.self_test (Holani row removed, banner removed, commit removed)"
        status: pass
    human_judgment: true
    rationale: "Completeness of the record is bounded by what the research files say; omissions cannot be reconstructed without opening emulator source"
  - id: D3
    description: "Provenance list names every adapted code file; dropping the expect_fail line fails docs.provenance against the real tree"
    requirement: FND-07
    verification:
      - kind: integration
        ref: "ctest docs.provenance, docs.provenance.self_test"
        status: pass
    human_judgment: false
  - id: D4
    description: "Claim template binds profile, content, startup tier, evidence tier and inventory hash and states that same-core digests are repeatability (R), not accuracy"
    requirement: FND-08
    verification:
      - kind: integration
        ref: "ctest docs.claim_template, docs.claim_template.self_test (cases template_missing_key, template_duplicate_key, template_extra_key, template_lost_repeatability_statement, template_missing)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Every claim validates, has profile none while O1 is open and an inventory_sha256 equal to the SHA-256 of the live fast.txt; missing profile, stale hash, Lynx profile, repeated key, empty value, missing hash, reordered inventory and an added row are rejected; a claims directory holding only the template passes"
    requirement: FND-08
    verification:
      - kind: integration
        ref: "ctest docs.claim_template -V prints 20 'claims_controls: <case>: ok' lines; sh scripts/check-claims.sh exits 0 on the real tree"
        status: pass
    human_judgment: false

duration: 35min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 12: Clean-room draft, provenance list and inventory-bound claims Summary

**The draft clean-room policy, a per-source exposure record and a provenance list of adapted code are machine-checked, and the claim template and example repeatability claim are bound to the SHA-256 of the exact `tests/inventory/fast.txt` bytes, with six new `docs.*` tests.**

## Performance

- **Duration:** about 35 min of agent time (start timestamp not captured separately).
- **Tasks:** 2 of 2, 2 commits.
- **Diff:** about 32.6 KB of new text and script, so `actuals.tokens` is 8150 on the chars/4 scale (estimate 55000; over-estimated by about 6.7x, in line with earlier plans).
- **Measured on the dev host (macOS arm64):**
  - `docs.policy` 0.01 s, `docs.policy.self_test` 0.02 s, `docs.provenance` 0.03 s, `docs.provenance.self_test` 0.03 s, `docs.claim_template` 0.05 s, `docs.claim_template.self_test` 1.8 s (20 cases).
  - `sh scripts/check.sh` total 66 s on the final tree: workflow step 44 s (55 tests, serial), artifacts 11 s, history 10 s, shellcheck 1 s.

## Accomplishments

- **Policy documents.** `clean-room.md` and `exposure-record.md` both open with the banner `> DRAFT pending owner decision O6`. The exposure table has eight rows with `file:line` citations into ARCHITECTURE, STACK, FEATURES, PITFALLS and SUMMARY. Dates are the research files' own and marked "as recorded" where vague (the Gearlynx commit is dated 2026-11-04, the read date is "on or before 2026-11-09"). No emulator source was opened and nothing was written from memory.
- **Provenance.** `docs/provenance.md` lists 13 files (`cmake/Tests.cmake`, `core/sha256.c`, `core/sha256.h`, `scripts/fetch-pin.sh`, `scripts/hygiene.sh`, `tests/check.h`, and `tests/cmake/{abi,check_junit,expect_fail,inventory_equal,no_float,no_skip,pins}.cmake`) plus SplitMix64, FIPS 180-4, cc65 and lynx-tests, and records RESEARCH A10 as an open note.
- **`docs.policy` / `docs.provenance`.** `provenance.cmake` reads only the first 10 lines of each file (a 2 KB prefix) and assembles its match phrases from parts so its own header never matches.
- **Claims.** `check-claims.sh` enforces the exact template key set, the repeatability statement, validate-kv on each claim, `ALLOWED_PROFILES` (default `none`) and the inventory hash. `--inventory-sha256` prints the value a claim must hold. The example claim `skeleton-repeatability.claim` cites `det.two_runs det.two_instances det.golden tests/golden/skeleton.digest`; its `content_sha256` (`ab5f8b5c...`) came from a fresh `run.manifest` of the golden workload and its observation digest matches the golden (`4e63d100...`).

## Task Commits

1. **Task 1: Draft clean-room policy, exposure record, provenance notice and docs.policy check** - `d7057cd` (feat)
2. **Task 2: Claim template and inventory-bound example claim with their check** - `d60f595` (feat)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Findings wrapped by FATAL_ERROR split across lines**
- **Found during:** Task 1 (`docs.provenance.self_test` failed on first run)
- **Issue:** `message(FATAL_ERROR)` re-wraps text at 79 columns, so `... but not listed in provenance.md` was split mid-phrase and the expect_fail REGEX did not match.
- **Fix:** both CMake checks print findings with `message(STATUS)` and finish with a short `FATAL_ERROR` ("findings above").
- **Files modified:** `tests/cmake/docs_policy.cmake`, `tests/cmake/provenance.cmake`
- **Commit:** `d7057cd`

### Clarifications (not deviations)

- The plan's verify text expects "0 tests failed"; CTest here prints "100% tests passed out of N". Exit status was used, as in earlier plans.
- The plan asks the policy self-test for one fault (Holani row); the fixture carries three (banner, Holani row, commit) and the REGEX requires all three findings in order. The test count stays at the planned six.
- `docs.provenance` covers files whose first 10 lines carry the phrase. `inventory_equal.cmake` (phrase on line 12) and `hygiene.sh` (a different wording on line 13) are listed anyway, so they are not enforced by the gate.

**Total deviations:** 1 auto-fixed (1 bug). **Impact:** none on scope.

## Authentication Gates

None.

## Known Stubs

None. The template placeholders (`<one sentence>` and similar) are the intended content of `claim.template`, which is not a claim.

## Threat Flags

None. The new files add no network surface, parsing of untrusted input beyond the existing validator, or schema at a trust boundary.

## Open Items for the Owner (not decided here)

- O6: `clean-room.md` and `exposure-record.md` stay DRAFT; the owner accepts or restricts them.
- O5: no licence file was added; the provenance note on RESEARCH A10 stays open.
- O1: claims may name only profile `none` until the owner extends `ALLOWED_PROFILES` in `scripts/check-claims.sh`.

## Next Plan Readiness (01-13)

- `tests/inventory/fast.txt` now holds 55 tests. **If plan 01-13 changes that file for any reason, regenerate the claim:** `sh scripts/check-claims.sh --inventory-sha256` prints the value to write into `inventory_sha256` in `docs/claims/skeleton-repeatability.claim`, and only after the evidence has been re-run. `docs.claim_template` fails until then, by design.
- `docs.provenance` will fail if a new code file with a muhtari-adapted header lands without a line in `docs/provenance.md`.
- New gates that report through `message(FATAL_ERROR)` must keep each finding short or use the status-line pattern above, or an expect_fail REGEX can miss a wrapped line.
- Plan 01-13 still owes: PROJECT.md decisions, README links to these documents, and the CI evidence lines (including the `scan-binaries` summary lines recorded in 01-11).

## Self-Check: PASSED

- Created files exist: docs/policy/clean-room.md, docs/policy/exposure-record.md, docs/provenance.md, docs/claims/claim.template, docs/claims/skeleton-repeatability.claim, scripts/check-claims.sh, tests/cmake/docs_policy.cmake, tests/cmake/provenance.cmake, tests/gates/docs.cmake, tests/gates/claims_controls.sh.
- Commits `d7057cd` and `d60f595` are ancestors of HEAD; `git rev-list --count` from the recorded base is 2.
- Acceptance: banner on line 1 of clean-room.md; 6 named emulator rows matched; `ctest -R '^docs\.'` 6 of 6 pass; the claim's `inventory_sha256` equals `shasum -a 256 tests/inventory/fast.txt`; no LICENSE, LICENSE.md or COPYING; `shellcheck -s sh` clean; `sh scripts/check.sh` exit 0 (66 s).
