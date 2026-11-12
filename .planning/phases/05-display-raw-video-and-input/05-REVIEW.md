---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T16:51:20Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - consumer/main.c
  - tests/det/test_det.c
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-12T16:51:20Z
**Depth:** standard
**Files Reviewed:** 2
**Frozen HEAD:** `9e1468717c19c827e2d822f51667e45464274788`
**Phase source fingerprint:** `a0d5463d3b3e3a87973b076db23187281f9c965fcfe2e721da28ffeb6c543133`
**Status:** clean

## Summary

This bounded delta review inspected two mechanical compiler-warning fixes. The local path parameter in `frame_input_paths_are_distinct` was renamed from `symbols` to `symbols_path` at its declaration and sole use; this removes the shadowing diagnostic without changing the argument or control flow. The frame-capture and video-packet arrays now use standard C17 aggregate zero initialization (`{0}`), which continues to zero every element and member. The five accepted findings remain resolved. No new behavioral findings were found. I did not rerun builds or tests; GCC 13.3 warning-clean syntax checks and the affected macOS rebuild/focused tests are recorded in `05-GCC-REPAIR.md`. Final-source macOS, Linux, and hosted CI qualification remains pending.

## Narrative Findings (AI reviewer)

No new findings. The rename changes only the parameter identifier and its pointer in the three-element input-path array; it leaves the `stat` calls, path comparison loop, argument order, and callers unchanged. C17 `{0}` aggregate initialization initializes the full arrays to zero, matching the prior nested-brace initialization while avoiding GCC's missing-braces warning.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers two changed paths in this delta, within the expanded 55-path phase scope. Provenance includes the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repair at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), the two-path replay-control repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` (49 unchanged), the one-path ShellCheck repair at `ce2344851c7adebea900d590f34eda31eef25a1e` (50 unchanged), the four-path harness repair at `9290c743e67e9a8aabc60aff841c831c17186410` (all 51 prior paths unchanged, expanding scope to 55), the one-path scratch repair at `3aaf004f08fb40d6aad6c9fc126aaa86fefd681f` (54 unchanged), and the four-path portability repair at `735cfa169b0cfcf7f4687d746c87461dc1677e1f` (51 unchanged). This GCC warning repair changes `consumer/main.c` and `tests/det/test_det.c`; the other 53 paths are unchanged. The current 55-path fingerprint is `a0d5463d3b3e3a87973b076db23187281f9c965fcfe2e721da28ffeb6c543133`. This review did not rerun builds or tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T16:51:20Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
