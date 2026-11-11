---
phase: 04-timers-interrupts-and-first-public-suites
reviewed: 2026-11-11T22:07:06Z
depth: standard
files_reviewed: 9
files_reviewed_list:
  - README.md
  - docs/policy/exposure-record.md
  - docs/public-suites.md
  - docs/timer-source-index14-pending.json
  - scripts/build-suites.sh
  - scripts/generate-suite-oracle.sh
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
  necessity: 0
status: issues_found
---

# Phase 04: Final Delta Code Review

**Reviewed:** 2026-11-11T22:07:06Z
**Depth:** standard
**Files Reviewed:** 9
**Status:** issues_found

## Summary

Reviewed the requested nine-file post-fix delta against the Phase 4 decision and evidence records. The output cleanup, atomic publication, source/build/object/label checks, exact case manifests, portable shell refusals, and user-facing pending-row claims were checked in context. One warning remains: the captured-record validator accepts extra content in the pinned-source marker. The prior complete phase review remains applicable to unchanged files. No tests were rerun; the supplied macOS, Ubuntu, and exact-head CI results were treated as existing qualification evidence.

## Warnings

### WR-02: Pin marker parser accepts trailing content

**File:** `tests/suites/test_public_suites.c:1144-1146`
**Severity:** WARNING
**Issue:** `verify_pinned_revision()` checks that the marker has at least 64 bytes, that its first 64 bytes match the expected archive hash, and that byte 64 is either a newline or NUL. It does not require the marker to end there, so a malformed marker containing the expected hash followed by `\nextra-data` passes. This weakens the validator's fail-closed source identity check for CPU and timer evidence. The pin writer documents a 64-character lowercase SHA-256 marker and writes only that hash plus one newline (`scripts/fetch-pin.sh:23,195`).
**Fix:** Require exactly 64 bytes or exactly 65 bytes with a single trailing newline, and reject any other suffix before accepting the marker. Add a focused malformed-marker control with trailing data.

---

_Reviewed: 2026-11-11T22:07:06Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
