---
phase: 04-timers-interrupts-and-first-public-suites
reviewed: 2026-11-11T18:23:05Z
depth: standard
files_reviewed: 35
files_reviewed_list:
  - CMakeLists.txt
  - README.md
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - core/timer.c
  - docs/claims/skeleton-repeatability.claim
  - docs/memory-startup.md
  - docs/policy/exposure-record.md
  - docs/provenance.md
  - docs/public-suites.md
  - docs/timer-source-index14-pending.json
  - docs/timers-interrupts.md
  - fixtures/manifests/cpu-tests.manifest
  - fixtures/manifests/timer-tests.manifest
  - fixtures/suites/NOTICE
  - fixtures/suites/bll.cfg
  - fixtures/suites/cpu-wrapper.s
  - fixtures/suites/timers-wrapper.s
  - include/lynxplosion.h
  - scripts/build-suites.sh
  - scripts/check-manifests.sh
  - scripts/generate-suite-oracle.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/diag/test_diag.c
  - tests/diag/tests.cmake
  - tests/inventory/fast.txt
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
  - tests/tests.cmake
  - tests/timers/fault_controls.cmake
findings:
  critical: 6
  warning: 1
  info: 3
  total: 10
  necessity: 0
status: issues_found
---

# Phase 04: Code Review Report

**Reviewed:** 2026-11-11T18:23:05Z
**Depth:** standard
**Files Reviewed:** 35
**Status:** issues_found

## Summary

Reviewed the complete Phase 04 source scope against the phase decisions and qualification contracts. The review found six filesystem, parser, evidence-integrity, or memory-safety blockers; one test-inventory warning; and three usability/documentation issues. No tests were run during this read-only review.

## Narrative Findings (AI reviewer)

### Critical Issues

### CR-01: BLOCKER — Predictable oracle temporary can overwrite a symlink target

**File:** `scripts/generate-suite-oracle.sh:246-269`
**Severity:** BLOCKER
**Issue:** The script creates its temporary output at the predictable sibling path `$OUT.$$` and redirects output to it without exclusive creation or a no-follow check. A planted symlink at that path is followed by shell redirection, allowing the generator to overwrite a file outside its validated scratch directory. The final output symlink check does not protect the temporary path.
**Fix:** Create the temporary file exclusively inside the already validated real output parent (for example, with `mktemp`), restrict its permissions, and keep the cleanup trap tied only to that owned file. Rename it atomically to the destination without first deleting or following an existing output path.

### CR-02: BLOCKER — Text input is not NUL-terminated before C string operations

**File:** `tests/suites/test_public_suites.c:135-172`
**Severity:** BLOCKER
**Issue:** `read_file` allocates at most `limit` bytes and returns the bytes and length, while `read_text` casts that allocation to `char *` without appending a terminating NUL. Callers then pass the result to functions such as `strstr`, `strlen`, and `strcmp`; an accepted exact-limit text file with no embedded NUL makes those operations read beyond the allocation. The parser's separate embedded-NUL check does not add a terminator.
**Fix:** Give text reads a `limit + 1` allocation, reject input beyond the limit as today, append `bytes[length] = '\0'`, and retain the embedded-NUL rejection before returning the string.

### CR-03: BLOCKER — Oracle revision copy can overflow its destination field

**File:** `tests/suites/test_public_suites.c:234-288`
**Severity:** BLOCKER
**Issue:** `out->revision` is a 41-byte field, but the metadata loop calls `copy_field` with capacity 65 for every destination. A revision string longer than 40 characters is copied before `valid_hex(..., 40)` rejects it, so malformed oracle input can overwrite adjacent members of `suite_oracle`.
**Fix:** Carry each destination's actual capacity into the metadata loop (using the corresponding field's `sizeof`) and validate each identity's expected length before copying. Ensure the copy helper never receives a capacity larger than the actual destination.

### CR-04: BLOCKER — Completion and probe symbols may alias executable code

**File:** `tests/suites/test_public_suites.c:523-557`
**Severity:** BLOCKER
**Issue:** `ranges_valid` rejects overlap between the code extent and result buffer, and rejects control bytes that overlap the result buffer or each other, but it never rejects the progress, model, completion, or probe byte addresses when they fall inside the code extent. A label file can therefore make the harness treat executable bytes as guest-written completion/model evidence, despite the public suite documentation claiming those ranges are checked for overlap.
**Fix:** Reject every control-byte address in `[code_start, code_end)` as well as in the result extent, and retain the pairwise alias checks. Add negative symbol controls for each control symbol aliased into code and correct the documentation to match the enforced checks.

### CR-05: BLOCKER — Suite validators do not bind all evidence to the verified build object

**File:** `tests/suites/test_public_suites.c:2552-2558, 3001-3090`
**Severity:** BLOCKER
**Issue:** The captured CPU-record validator compares the record's hashes to the current oracle, BLL, and labels, but does not validate the build record's pinned source identity or compare `cpu_object_sha256` with the BLL. A changed oracle and refreshed record hash can therefore be accepted as internally consistent evidence. The timer run path checks that a wrapper hash is present and syntactically valid, but does not compare `timers_object_sha256` in `build-record.txt` with the linked timer BLL. These self-consistency checks do not establish that the captured/run artifacts came from the verified build inputs.
**Fix:** Reuse the verified source/build identity path in captured CPU validation, compare the actual CPU and timer BLL hashes to their build-record object hashes, and bind wrapper/label identities to the source and build records before accepting either suite's evidence.

### CR-06: BLOCKER — Lexical scratch-prefix check permits cleanup through a symlinked ancestor

**File:** `tests/suites/run.cmake:3-20`
**Severity:** BLOCKER
**Issue:** `cmake_path(IS_PREFIX ... NORMALIZE)` checks the textual path, not the filesystem's resolved path. If a directory below `SCRATCH_ROOT` is a symlink to an outside location, `WORK` can still pass this check while `file(REMOVE_RECURSE "${WORK}")` traverses that symlink and removes the corresponding outside directory tree.
**Fix:** Resolve and validate the real scratch root and work directory before any deletion; reject symlinks in every path component and require `WORK` to be a strict canonical descendant. Restrict recursive cleanup to the exact invocation-owned directory after validating its ownership marker.

### Warnings

### WR-01: WARNING — Timer control batches can silently lose or duplicate cases

**File:** `tests/suites/run.cmake:478-510`
**Severity:** WARNING
**Issue:** `timer_batch_end` only requires at least one case. Removing most of the timer result or pending-control cases, or duplicating a case, still leaves the batch eligible to pass. The current inventory is 49 result-control cases and 55 pending descriptor/companion/proof cases, so the nonzero guard does not preserve the intended coverage.
**Fix:** Track case names as well as counts, reject duplicates, and assert the exact expected case-name inventory for each batch. Reconcile the historical result-control count of 48 with the current 49 before locking the manifest.

### Info

### IN-01: INFO — CLI usage text omits supported timer and validator commands

**File:** `tests/suites/test_public_suites.c:3200`
**Severity:** INFO
**Issue:** The usage message documents only `run cpu`, even though the command dispatcher also supports timer runs and observations, no-run controls, and multiple validators. Users who invoke the binary incorrectly receive no guidance for those supported operations.
**Fix:** Document the supported command forms in the usage output, or print a concise usage pointer to `docs/public-suites.md`.

### IN-02: INFO — Timer observation example is parsed as a shell pipeline

**File:** `docs/public-suites.md:29`
**Severity:** INFO
**Issue:** The example `observe timers lynx1|lynx2 ...` uses `|`, which a shell interprets as a pipeline rather than as two permitted profile values. Copying the example does not invoke the documented command correctly.
**Fix:** Write `PROFILE (lynx1 or lynx2)` or show separate concrete invocations for the two profiles.

### IN-03: INFO — README opening omits the newly documented timer suite

**File:** `README.md:3-5`
**Severity:** INFO
**Issue:** The opening summary describes the current slice as covering CPU profiles, startup, and the CPU diagnostic, but the same change adds a public timer suite and later documents timer observations. The overview is now incomplete.
**Fix:** Update the opening summary to mention the timer suite and its software-observation status, or label the current text explicitly as the earlier CPU slice.

---

_Reviewed: 2026-11-11T18:23:05Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
