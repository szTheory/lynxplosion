---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T15:44:37Z
depth: standard
files_reviewed: 4
files_reviewed_list:
  - tests/cmake/abi.cmake
  - tests/cpu/test_cpu_vectors.c
  - tests/diag/test_diag.c
  - tests/timers/fault_controls.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-12T15:44:37Z
**Depth:** standard
**Files Reviewed:** 4
**Frozen HEAD:** `9290c743e67e9a8aabc60aff841c831c17186410`
**Phase source fingerprint:** `d9cdad7a5c25079d5b7c38c1c09e7747f518ba0581720bc0cccb4e0f2a376c51`
**Status:** clean

## Summary

This bounded delta review inspected four qualification-harness changes: the CPU seam snapshot offset, diagnostic snapshot layout, timer-mutant source list, and Darwin compiler-helper allowlist. The format-5 layout agrees with the core constants and writer: 256-byte header, payload hash at byte 220, timing policy at byte 252, and 77,948-byte payload. The timer mutant now builds with the input/video modules used by the current core; the ABI addition accepts only the normalized `chkstk_darwin` symbol on Darwin, preserving the existing libc negative control. The five accepted findings remain resolved. No new behavioral findings were found. I did not rerun tests; the 13 focused checks are recorded as passing in `05-QUALIFICATION-REPAIRS.md`. Broad qualification remains pending. Informational context: the unchanged header comment in `core/snapshot.c` still has stale numeric values (252 for `header_len`, 65604 for `payload_len`); the actual constants and writer agree with the format-5 values above. Root plans a comment-only correction before qualification.

## Narrative Findings (AI reviewer)

No new findings. The seam helper now writes the recomputed payload digest at bytes 220–251, leaving bytes 252–255 for timing-policy identity; its compile-time assertions enforce those boundaries against the current header length. The diagnostic forge controls use the full format-5 payload size, the same digest offset, RAM at payload offset 32, and verify the saved flat5 policy and final guest tick. Existing mutation assertions and payload mutations are otherwise unchanged. The timer controls still require every mutant to compile, exit 1 on its named assertion, and have clean positive controls before and after; the added sources complete the current machine link. The ABI allowlist is scoped to Darwin and one normalized compiler-helper name.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers four paths added to the reviewed phase scope. Provenance is the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repaired review at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), the two-path replay-control repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` (49 unchanged since the prior review), and the one-path ShellCheck repair at `ce2344851c7adebea900d590f34eda31eef25a1e` (50 unchanged since the prior review). The current four files were previously outside Phase 5's reviewed source list, expanding it from 51 to 55 paths; all original 51 hashes are unchanged. The current 55-path fingerprint is `d9cdad7a5c25079d5b7c38c1c09e7747f518ba0581720bc0cccb4e0f2a376c51`. This review did not rerun builds or tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T15:44:37Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
