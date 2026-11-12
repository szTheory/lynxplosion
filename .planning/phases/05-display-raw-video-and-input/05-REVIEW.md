---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T15:59:21Z
depth: standard
files_reviewed: 1
files_reviewed_list:
  - tests/consumer/frame_driver.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-12T15:59:21Z
**Depth:** standard
**Files Reviewed:** 1
**Frozen HEAD:** `3aaf004f08fb40d6aad6c9fc126aaa86fefd681f`
**Phase source fingerprint:** `69f6a655f0aeff73f8270cdb14deebb5775d3cee99b6cf7f9d6e0b2f5ac716c4`
**Status:** clean

## Summary

This bounded delta review inspected the consumer frame driver's scratch lifecycle. It now binds work under the canonical `build/check/scratch/consumer` directory, refuses path aliases before mutation, creates a fresh nonce-named child with exclusive `mkdir`, and only removes that child after all semantic checks pass and its real path and ownership marker are revalidated. The four successful early-return branches and the normal end path call the guarded cleanup; failures retain generated artifacts, and startup no longer clears an earlier `WORK`. The guard matches the check preset's fresh Linux/macOS build path (`${sourceDir}/build/check`, with scratch under `${PROJECT_BINARY_DIR}/scratch`). The five accepted findings remain resolved. No new correctness or security findings were found. I did not rerun tests; the seven affected consumer CTests and controlled unsafe-path/failure-retention checks are recorded as passing in `05-CLEANUP-REPAIR.md`. Broad qualification remains pending. The prior informational format-comment discrepancy has been corrected in the current source binding: the header comment now states the format-5 256-byte header and 77,948-byte payload.

## Narrative Findings (AI reviewer)

No new findings. Before any directory creation or file writes, the driver normalizes and resolves the supplied project scratch path and checks the expected case directory name. It resolves the scratch parent again before creation to reject aliases. Each invocation appends a 24-hex nonce and uses plain `mkdir`, so a collision or preexisting child fails rather than replacing it. The success cleanup checks that the child is not a symlink, resolves to itself directly below the expected parent, and still contains the matching non-symlink owner marker. The final `REMOVE_RECURSE` targets only that verified nonce child. Failure assertions exit before cleanup, preserving generated guest/output evidence; the external artifact gate's successful-path expectations are met without deleting prior scratch.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers one changed path in this delta, within the expanded 55-path phase scope. Provenance includes the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repair at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), the two-path replay-control repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` (49 unchanged), the one-path ShellCheck repair at `ce2344851c7adebea900d590f34eda31eef25a1e` (50 unchanged), and the four-path harness repair at `9290c743e67e9a8aabc60aff841c831c17186410` (all 51 prior paths unchanged, expanding scope to 55). This cleanup repair changes only `tests/consumer/frame_driver.cmake`; the other 54 paths are unchanged. The current 55-path fingerprint is `69f6a655f0aeff73f8270cdb14deebb5775d3cee99b6cf7f9d6e0b2f5ac716c4`. This review did not rerun tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T15:59:21Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
