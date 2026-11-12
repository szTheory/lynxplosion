---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T16:36:11Z
depth: standard
files_reviewed: 4
files_reviewed_list:
  - scripts/build-page-mode.sh
  - scripts/generate-page-mode-oracle.sh
  - tests/gates/manifests_controls.sh
  - tests/gates/page-mode-script-controls.sh
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-12T16:36:11Z
**Depth:** standard
**Files Reviewed:** 4
**Frozen HEAD:** `735cfa169b0cfcf7f4687d746c87461dc1677e1f`
**Phase source fingerprint:** `5de70b75e913919755c198ceef2a989ebd8c636908f37986fedbe17ebbd11dc8`
**Status:** clean

## Summary

This bounded delta review inspected four portability/control changes. The compound `test && test || refusal` forms were rewritten with explicit inverse `if` conditions; option duplicate/missing-argument checks, output existence and symlink refusal, pinned hash comparisons, ownership checks, and oracle assertions retain their original accept/refuse conditions. The tool-restoration control now copies the saved bytes to a fresh adjacent file, verifies them with `cmp`, then replaces the private mutated fixture with `mv`; the existing exact failure diagnostics and restored-tool positive build/oracle brackets remain. The five accepted findings remain resolved. No new correctness or security findings were found. I did not rerun tests; both ShellCheck versions and the three affected post-fix tests are recorded as passing in `05-PORTABILITY-REPAIR.md`. Final macOS, Linux, and hosted CI qualification must rerun on this source; no such pass is claimed here.

## Narrative Findings (AI reviewer)

No new findings. For the shell rewrites, each original success conjunction is replaced by `if` with the corresponding disjunction of negated checks; each original refusal on a failed conjunction becomes the same refusal branch. Under `set -eu`, valid tests continue through the same assignments/shift, and invalid arguments still exit through `usage`. Existence checks still reject either an existing path or a dangling symlink. Hash and marker checks preserve the same variables and failure messages. In the negative-control script, the tool is changed only inside the copied fixture; the temporary restore path is refused if present, saved bytes are checked before atomic replacement, and both post-restoration positive brackets execute the restored tool through the normal scripts. The six protected legacy identities and exact negative controls are recorded unchanged.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers four changed paths in this delta, within the expanded 55-path phase scope. Provenance includes the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repair at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), the two-path replay-control repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` (49 unchanged), the one-path ShellCheck repair at `ce2344851c7adebea900d590f34eda31eef25a1e` (50 unchanged), the four-path harness repair at `9290c743e67e9a8aabc60aff841c831c17186410` (all 51 prior paths unchanged, expanding scope to 55), and the one-path scratch repair at `3aaf004f08fb40d6aad6c9fc126aaa86fefd681f` (54 unchanged). This portability repair changes the four listed shell paths; the other 51 paths are unchanged. The current 55-path fingerprint is `5de70b75e913919755c198ceef2a989ebd8c636908f37986fedbe17ebbd11dc8`. This review did not rerun tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T16:36:11Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
