---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T17:08:49Z
depth: standard
files_reviewed: 5
files_reviewed_list:
  - fixtures/manifests/input-probe.manifest
  - scripts/check-manifests.sh
  - scripts/validate-kv.sh
  - tests/consumer/frame_driver.cmake
  - tests/gates/manifests_controls.sh
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-12T17:08:49Z
**Depth:** standard
**Files Reviewed:** 5
**Frozen HEAD:** `931fa030d514a8084132159afa44b7ace50567b5`
**Phase source fingerprint:** `ea140006093038be023cdceeb67c4c1468576c940f0aa942b1b4b69bbcf1a1f3`
**Status:** clean

## Summary

This bounded delta review inspected five fixture-provenance changes. The fixture manifest now identifies the portable cc65 archive pin and build recipe, while the driver binds that identity through `pins.txt`, the fetched pin marker, and both successful-build record headers. It separately checks the current platform's actual ca65/ld65 executable hashes against the build record, and still verifies the original source/config/legacy-source and source-built guest hash/size identities. The manifest validator and its caller now require the exact all-or-none 11-field group, hash shape, and recipe `1`; controls reject malformed pin and wrong recipe values. Historical macOS executable hashes remain evidence about that build rather than portable fixture identities. The five accepted findings remain resolved. No new provenance or behavior findings were found. I did not rerun tests; nine focused cases on macOS and Ubuntu, seven isolated refusal controls, and both ShellCheck versions are recorded in `05-PROVENANCE-REPAIR.md`. Final-source macOS, Linux, and hosted CI qualification remains pending.

## Narrative Findings (AI reviewer)

No new findings. In the frame driver, the cc65 pin row must be unique and parse to 64 lowercase hex characters; its trimmed marker, stamp pin/recipe rows, build-files pin/recipe headers, and manifest pin/recipe rows must each match exactly. The driver then compares each native executable's current SHA-256 to its corresponding build-files entry. The later source-build checks still compare both handedness outputs and the legacy tracer output against unchanged manifest hashes and byte counts, and the production replay control independently checks its source-built guest. The validator recognizes the two replacement fields as a strict 11-field group, validates the pin as SHA-256, and rejects any recipe other than `1`; the manifest caller's allowed-field list matches. The added controls preserve the complete valid group and reject short/non-hex pins and recipe `2`.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers five changed paths within the existing 55-path phase scope. Provenance includes the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repair at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), the two-path replay-control repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` (49 unchanged), the one-path ShellCheck repair at `ce2344851c7adebea900d590f34eda31eef25a1e` (50 unchanged), the four-path harness repair at `9290c743e67e9a8aabc60aff841c831c17186410` (all 51 prior paths unchanged, expanding scope to 55), the one-path scratch repair at `3aaf004f08fb40d6aad6c9fc126aaa86fefd681f` (54 unchanged), the four-path portability repair at `735cfa169b0cfcf7f4687d746c87461dc1677e1f` (51 unchanged), and the two-path GCC warning repair at `9e1468717c19c827e2d822f51667e45464274788` (53 unchanged). This provenance repair changes the five listed paths; the other 50 paths are unchanged. The current 55-path fingerprint is `ea140006093038be023cdceeb67c4c1468576c940f0aa942b1b4b69bbcf1a1f3`. This review did not rerun builds or tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T17:08:49Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
