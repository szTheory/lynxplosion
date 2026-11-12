---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T15:07:52Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - consumer/main.c
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

**Reviewed:** 2026-11-12T15:07:52Z
**Depth:** standard
**Files Reviewed:** 2
**Frozen HEAD:** `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0`
**Phase source fingerprint:** `e4fd27130097f8a23c8162403a58e933104bc7a35b160ff646767e402052c732`
**Status:** clean

## Summary

This final bounded delta review inspected `consumer/main.c` and `tests/consumer/frame_driver.cmake` at the frozen head. It continues the standard-depth review of the original 51-path scope, following the first repair review of six changed paths; the current 51-path fingerprint records that the other 49 paths are unchanged since that review. All five accepted findings from the original and repair reviews are resolved. The final repair gates every `LYNX_TEST_*` environment read behind `LYNX_CONSUMER_TESTING`, routes positive interleaved and checkpoint-restore controls through the test consumer, asserts their success markers, and compares production behavior with each replay override against an independently validated canonical five-event baseline. No new correctness or security findings were found. I did not rerun tests; the focused build and four consumer checks are recorded in `05-REVIEW-FIX.md`. Final exact-source and broad phase qualification remain pending.

## Narrative Findings (AI reviewer)

No new findings. WR-04 is resolved: the two formerly unguarded reads are now compile-time test-only, and the production regression runs each override with a fresh environment, validates the baseline and override artifacts independently, requires four frames, compares manifests and raw/rendered outputs, and retains the dedicated test-consumer success markers for interleaved and restore checks. A sweep of all `getenv`/`LYNX_TEST_*` reads in `consumer/main.c` found the other test controls already within `LYNX_CONSUMER_TESTING` guards or test-only functions. The test driver retains the bad restore-script negative control and the production replay control.

## Resolved Findings

| Prior finding | Verdict | Independent review of fix and regression proof |
|---|---|---|
| CR-01: unsafe manifest-control `WORK` cleanup | Resolved; pre-existing and outside the Phase 5 diff | The script canonicalizes and bounds the target before mutation, rejects existing targets, creates an exclusive directory with a private marker, and limits recursive child cleanup to that owned directory. Synthetic guard controls exercise repository/source paths, aliases, traversal, and stale targets; the fix report records a passing legitimate control run. |
| WR-01: opcode legality used backing RAM for mapped fetches | Resolved | `bus_opcode_peek` follows the RAM, Mikey video/timer, and Suzy input routes used by `bus_access`, with side-effect-free device peeks. Regression controls cover both mapped/RAM mismatch directions and preserve private flat-test content, zero-tick refusal, and page behavior. The focused checks are recorded as passing; this does not claim hardware accuracy. |
| WR-02: left/right source-built guest hashes bypassed validation | Resolved | Both exact field names now use the existing 64-character lowercase-hex validation. Controls reject short and non-hex values for each handedness field while retaining valid, incomplete-group, and unknown-key cases. Focused manifest checks are recorded as passing. |
| WR-03: legacy replay switch reached production | Resolved | Legacy implementation and dispatch are enclosed by `LYNX_CONSUMER_TESTING`; historical tracer assertions run through `TEST_CONSUMER`. The production control uses a provenance-checked guest and verifies equivalent validated manifests and raw/rendered frames with and without the legacy variable. Focused consumer checks are recorded as passing. |
| WR-04: interleaved and checkpoint-restore switches reached production | Resolved | Both environment reads are now enclosed by `LYNX_CONSUMER_TESTING`. The frame driver routes positive controls through `TEST_CONSUMER` and requires success markers; the production comparison covers baseline, legacy, interleaved, and restore-variable runs against the same validated canonical timeline, checker, manifest, and frame outputs. The bad-script negative remains. Focused consumer checks are recorded as passing. |

The report covers two paths in this final delta. Provenance is the initial 51-path review at `013de47ff566ae9c997542ddfba6fe15ff93aebe`, the six-path repaired review at `e74df76b8616cf9daaffc3f90de2ffb36ba0d1f0` (45 unchanged), and this two-path repair at the frozen head above (49 unchanged since the prior review). This review did not rerun builds or tests and does not complete Task 12 or phase qualification.

---

_Reviewed: 2026-11-12T15:07:52Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
