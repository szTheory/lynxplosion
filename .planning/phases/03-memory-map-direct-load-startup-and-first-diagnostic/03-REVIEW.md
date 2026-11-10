---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
reviewed: 2026-11-10T23:31:36Z
depth: standard
files_reviewed: 46
files_reviewed_list:
  - CMakeLists.txt
  - README.md
  - consumer/main.c
  - core/bus.c
  - core/bus.h
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - docs/claims/skeleton-repeatability.claim
  - docs/cpu-profiles.md
  - docs/memory-startup.md
  - docs/policy/exposure-record.md
  - docs/provenance.md
  - fixtures/diagnostic/bll.cfg
  - fixtures/diagnostic/first.hex
  - fixtures/diagnostic/first.s
  - fixtures/manifests/first-diagnostic.manifest
  - include/lynxplosion.h
  - runner/bundle.c
  - runner/bundle.h
  - runner/main.c
  - scripts/check.sh
  - scripts/scan-binaries.sh
  - scripts/validate-kv.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/consumer/check_artifacts.c
  - tests/consumer/tests.cmake
  - tests/cpu/test_cpu_vectors.c
  - tests/det/bll.cmake
  - tests/det/golden.cmake
  - tests/det/golden_self_test.cmake
  - tests/det/manifest_controls.sh
  - tests/det/manifest_fields.cmake
  - tests/det/runner_cli.cmake
  - tests/det/sensitivity.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/det/two_runs.cmake
  - tests/diag/test_diag.c
  - tests/diag/tests.cmake
  - tests/golden/skeleton.digest
  - tests/header/header.c
  - tests/hygiene/scan_control.sh
  - tests/inventory/fast.txt
  - tests/tests.cmake
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
  necessity: 0
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-11-10T23:31:36Z  
**Depth:** standard  
**Files Reviewed:** 46  
**Status:** issues_found

## Summary

The changed source scope was reviewed against `origin/main`, with phase plans, summaries, and acceptance context used to check the implementation boundaries. I found one warning: normal write and close failures and competing publishers are covered, but process interruption at the consumer's manifest/publication boundary has no deterministic control. This is a missing verification guarantee; I did not reproduce partial output at the requested destination.

Disposition notes: scalar getter output aliasing is excluded because the public API explicitly forbids inspect output ranges from overlapping the machine instance; it requires a caller to violate that precondition. Snapshot restore's acceptance of a forged 255-tick ready span is a real pre-existing weakness, but the same guard and positive test were already present unchanged in `origin/main`, so it is outside this Phase 3 diff review.

## Narrative Findings (AI reviewer)

### WR-01: Process interruption is not tested at the publication boundary

**Classification:** WARNING  
**File:** `consumer/main.c:634-662`; `tests/diag/tests.cmake:305-312`  
**Issue:** `publish_outputs` creates a private stage, writes the PPM and WAV, stages the manifest, and publishes by exclusive directory rename. Ordinary errors go through the cleanup block, but process termination bypasses it and can leave a private staging directory. The documentation acknowledges that outcome. The registered refusal controls inject write/close errors and race publishers, but do not stop a child process before manifest staging or immediately before publication. As a result, the acceptance property that interruption cannot expose partial success metadata has no deterministic test evidence. The atomic rename makes a partial final directory unlikely by inspection, but the interruption boundary itself is unverified.

**Fix:** Add test-only checkpoints before manifest staging and before the exclusive rename. Run the consumer as a child, terminate it at each checkpoint, and assert that the requested output path is absent unless the whole independently validated directory was published. Assert the documented private-stage cleanup outcome as well; if handled signals should clean it, add invocation-owned signal cleanup and test that separately.

---

_Reviewed: 2026-11-10T23:31:36Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
