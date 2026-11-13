---
phase: quick-261113-h5w
reviewed: 2026-11-13T19:58:26Z
depth: standard
files_reviewed: 13
files_reviewed_list:
  - fixtures/timer-evidence/timer-evidence.s
  - fixtures/timer-evidence/bll.cfg
  - fixtures/timer-evidence/NOTICE
  - fixtures/timer-evidence/images.sha256
  - fixtures/manifests/timer-evidence.manifest
  - scripts/build-timer-evidence.sh
  - tests/timer-evidence/test_timer_evidence.c
  - tests/timer-evidence/run.cmake
  - tests/timer-evidence/producer-controls.cmake
  - tests/suites/tests.cmake
  - tests/inventory/fast.txt
  - docs/claims/skeleton-repeatability.claim
  - docs/timer-hardware-evidence.md
findings:
  critical: 1
  warning: 0
  info: 0
  total: 1
  necessity: 0
resolved: 1
open_findings: 0
status: passed
---

# Phase quick-261113-h5w: Code Review Report

**Reviewed:** 2026-11-13T19:58:26Z  
**Depth:** standard  
**Files Reviewed:** 13  
**Status:** passed after the recorded repair

## Summary

The initial review found one privacy blocker. A bounded recheck of that repair confirms the generated package and retained test artifacts are restricted to the invoking user, with a negative control for public package permissions. The source13 mismatch and the physical-execution boundary remain explicit.

## Critical Issues

### CR-01: BLOCKER — Scratch package and failure artifacts are not kept private

**File:** `scripts/build-timer-evidence.sh:2,252-260`; `tests/timer-evidence/run.cmake:56-73`; `tests/timer-evidence/producer-controls.cmake:13-16`

**Disposition:** Fixed in the current workspace. The builder sets `umask 077`, creates package directories as `0700`, and rejects verification unless the package is owned by the current user with exact mode `0700`. The CTest wrapper chmods both its invocation directory and temporary subdirectory to `0700` before writing its marker or log. A producer control widens the package to `0755`, requires verification to refuse it, then restores `0700`. The supplied focused run reports all 8 selected tests passed, including all three timer groups and 16 controls.

**Issue:** In the initially reviewed version, the builder accepted output beneath `build/check/scratch` and created the package with plain `mkdir`, without setting a restrictive umask or owner-only permissions. The CTest wrapper likewise created `RUN_DIR` and appended `run.log` without restricting permissions. With the current scratch directory mode `0755` and the usual `umask 022`, those directories and files were accessible to other local accounts. Failed runs deliberately retain `RUN_DIR`, which contains the generated BS93 images, receipts, and logs, so this violated the stated private-retention boundary.

**Fix:** Set `umask 077` before creating outputs, explicitly restrict the package and CTest invocation directory to mode `0700`, and keep retained logs under that private directory. If callers can supply a temporary root directly, reject roots that are not private to the invoking user. This repair is present and verified as described above.

---

_Reviewed: 2026-11-13T19:58:26Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
