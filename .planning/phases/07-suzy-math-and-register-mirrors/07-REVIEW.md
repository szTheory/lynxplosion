---
phase: 07-suzy-math-and-register-mirrors
reviewed: 2026-11-14T04:32:54Z
depth: standard
files_reviewed: 43
files_reviewed_list:
  - CMakeLists.txt
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - core/suzy.c
  - core/video.c
  - docs/audio.md
  - docs/claims/skeleton-repeatability.claim
  - docs/public-suites.md
  - docs/snapshot-audio.md
  - docs/snapshot-video.md
  - docs/suzy-math.md
  - docs/timer-source-index14-pending.json
  - docs/video-input.md
  - fixtures/manifests/math-tests.manifest
  - fixtures/manifests/memio-tests.manifest
  - fixtures/suites/NOTICE
  - fixtures/suites/math-wrapper.s
  - fixtures/suites/memio-wrapper.s
  - scripts/build-suites.sh
  - scripts/generate-suite-oracle.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/audio/fault_controls.cmake
  - tests/consumer/check_artifacts.c
  - tests/cpu/test_cpu_vectors.c
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/diag/test_diag.c
  - tests/diag/tests.cmake
  - tests/gates/audio-script-controls.sh
  - tests/gates/math-script-controls.sh
  - tests/golden/skeleton.digest
  - tests/inventory/fast.txt
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
  - tests/suzy/fault_controls.cmake
  - tests/suzy/test_suzy.c
  - tests/suzy/tests.cmake
  - tests/tests.cmake
  - tests/timers/fault_controls.cmake
  - tests/video/fault_controls.cmake
findings:
  critical: 1
  warning: 2
  info: 0
  total: 3
  necessity: 0
status: resolved
---

# Phase 07: Code Review Report

**Reviewed:** 2026-11-14T04:32:54Z  
**Depth:** standard  
**Files Reviewed:** 43  
**Status:** issues_found

## Summary

Reviewed the scoped Phase 7 implementation, source-backed suite harness, artifact checks, and public contracts. The emulator paths reviewed did not show a concrete behavior defect in the targeted register, arithmetic, event-ordering, snapshot, or palette contracts. The oracle parser has a reproducible out-of-bounds access on malformed group ranges. The harness also leaves two promised runtime/strict-record checks without independent evidence.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: [BLOCKER] Oracle group range check can wrap and index out of bounds

**Severity:** BLOCKER  
**File:** `tests/suites/test_public_suites.c:817-833`  
**Issue:** The parser checks `start + span > meaningful` using `uint32_t`; that addition can wrap. A malformed group with `start=4294967295` and `span=1` passes the range check, is narrowed into the stored metadata, then indexes `covered[suite][start]` outside its 18-byte array. A targeted malformed-oracle run under UBSan reported an out-of-bounds index at line 829.
**Fix:** Validate without addition overflow before storing or indexing:

```c
if (span == 0u || start >= meaningful || span > meaningful - start) {
    free(text);
    return fail("math/memio oracle: malformed group row");
}
```

## Warnings

### WR-01: Completion record claims source return without checking runtime PC/return boundary

**Severity:** WARNING  
**File:** `tests/suites/test_public_suites.c:4178-4180, 4311-4328`  
**Issue:** The run record hard-codes `guest_return=source-call-returned`. Completion is accepted when the sentinel watch fires after progress reaches 8, but the observer does not capture or validate the CPU PC or another return-boundary signal. This falls short of Plan 03's required real source-call/return evidence and lets the recorded claim rely only on the wrapper's sentinel/progress convention.
**Fix:** Capture CPU PC and relevant return state when the sentinel watch fires, validate them against the linked wrapper/source return labels, and write the verified boundary into the run record.

### WR-02: Strict-record controls omit several promised hostile-record cases

**Severity:** WARNING  
**File:** `tests/suites/run.cmake:2555-2638`  
**Issue:** The strict control starts from one valid observation, changes timing result bytes at offsets 14 and 15, and verifies those mismatches remain visible. It does not independently construct the other hostile records required by Plan 03: omitted timing, timing mislabeled as reserved, missing strict verdict, stale mismatch count, discarded mismatch row, or a scoped-pass record promoted to whole-suite pass. The parser may reject these shapes, but the focused controls do not establish that.
**Fix:** Add a separate complete-record mutation for each listed case, rehash the changed artifacts where appropriate, and require the specific strict-record invariant to reject each one while a clean positive record continues to pass.

---

_Reviewed: 2026-11-14T04:32:54Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_

## Resolution

- CR-01: fixed in `0fe0734` (overflow-safe range check; regression control).
- WR-01: fixed in `7848e48` (runtime return boundary verified).
- WR-02: fixed in `612c1e4` (eight hostile strict-record controls).
- Independent security audit finding T-07-03 (coarse advance past a pending
  Suzy deadline): fixed in `b287750` with `suzy.coarse_deadline`; claim rebound
  in `3b50de6`. See 07-SECURITY.md and 07-QUALIFICATION.md.
