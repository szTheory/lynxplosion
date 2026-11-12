---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T15:16:56Z
depth: standard
files_reviewed: 1
files_reviewed_list:
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

**Reviewed:** 2026-11-12T15:16:56Z
**Depth:** standard
**Files Reviewed:** 1
**Frozen HEAD:** `ce2344851c7adebea900d590f34eda31eef25a1e`
**Phase source fingerprint:** `b5568b960e8ce7f1c265f106e1d8dbc2e58455081497c8d69c1698742af10ed6`
**Status:** clean

## Summary

This bounded delta review inspected the four changed `CDPATH` assignments in `tests/gates/manifests_controls.sh`. Each change spells the empty assignment as `CDPATH=''`; quoting does not change the value or shell behavior relative to the prior `CDPATH=` prefix. The surrounding path containment, ownership, and cleanup logic is unchanged. This continues the standard-depth review of the original 51-path scope through six-path, two-path, and now one-path repair deltas. The five accepted findings remain resolved. No new correctness or security findings were found. I did not rerun tests; ShellCheck, `sh -n`, and the two manifest CTests are recorded as passing after the edit. Final exact-source and broad phase qualification remain pending.

## Narrative Findings (AI reviewer)

No new findings. The four `CDPATH=''` edits explicitly preserve the empty prefix assignment while satisfying ShellCheck SC1007. The diff changes no commands, guards, target paths, ownership checks, or cleanup actions; the CR-01 containment repair remains intact.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers one path in this final delta. Provenance is the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repaired review at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), the two-path replay-control repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` (49 unchanged since the prior review), and this one-path ShellCheck repair at the frozen head above (50 unchanged since the prior review). The current source fingerprint is `b5568b960e8ce7f1c265f106e1d8dbc2e58455081497c8d69c1698742af10ed6`. This review did not rerun builds or tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T15:16:56Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
