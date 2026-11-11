---
phase: 04
fixed_at: 2026-11-11T19:31:29Z
review_path: .planning/phases/04-timers-interrupts-and-first-public-suites/04-REVIEW.md
iteration: 1
findings_in_scope: 10
fixed: 10
skipped: 0
status: all_fixed
---

# Phase 04: Code Review Fix Report

**Fixed at:** 2026-11-11T19:31:29Z
**Source review:** `.planning/phases/04-timers-interrupts-and-first-public-suites/04-REVIEW.md`
**Iteration:** 1

**Summary:**

- Findings in scope: 10
- Fixed: 10
- Skipped: 0

## Fixed Issues

### CR-01: BLOCKER — Predictable oracle temporary can overwrite a symlink target

**Files modified:** `scripts/generate-suite-oracle.sh`, `docs/policy/exposure-record.md`, `docs/timer-source-index14-pending.json`, `tests/suites/run.cmake`
**Commit:** `2d8d82d`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** The oracle now creates a mode-restricted temporary exclusively in the validated output parent, traps only that file, and atomically publishes a regular final file. The outside-target symlink control proves the planted link is not followed. The exact-pin exposure row now records reads of `cpu/main.c` and `timers/main.c` for oracle generation.

### CR-02: BLOCKER — Text input is not NUL-terminated before C string operations

**Files modified:** `tests/suites/test_public_suites.c`, `tests/suites/run.cmake`
**Commit:** `044543b`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** Bounded text reads allocate room for and append a terminator, including exact-limit and zero-limit inputs, while retaining oversize and embedded-NUL refusal. Boundary controls cover exact size, zero size, over-limit input, embedded NUL, and missing token; the focused harness was built with the check preset's sanitizer configuration.

### CR-03: BLOCKER — Oracle revision copy can overflow its destination field

**Files modified:** `tests/suites/test_public_suites.c`, `tests/suites/run.cmake`
**Commit:** `1b5781a`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** Each metadata field is copied using its actual destination capacity, and the revision length is rejected before copying. A long-revision negative control verifies the intended refusal.

### CR-04: BLOCKER — Completion and probe symbols may alias executable code

**Files modified:** `tests/suites/test_public_suites.c`, `tests/suites/run.cmake`, `docs/public-suites.md`
**Commit:** `f4377d5`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** Preflight now rejects every control byte overlapping the linked code, result buffer, or another control byte, and verifies the payload, zero-page, stack, and BSS ranges. Symbol controls exercise each control-byte code alias with its intended refusal.

### CR-05: BLOCKER — Suite validators do not bind all evidence to the verified build object

**Files modified:** `tests/suites/test_public_suites.c`, `scripts/build-suites.sh`, `docs/timer-source-index14-pending.json`
**Commit:** `96fc354`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** Captured CPU validation now verifies the pinned source/build identity and compares the actual BLL to `cpu_object_sha256`. Strict and no-run timer paths verify the timer object identity before execution or cleanup, and proof/companion validation carries the same source-bound object and label checks. Focused identity controls mutate revision, source hash, object, and labels.

### CR-06: BLOCKER — Lexical scratch-prefix check permits cleanup through a symlinked ancestor

**Files modified:** `tests/suites/run.cmake`, `tests/suites/test_public_suites.c`, `tests/suites/tests.cmake`
**Commit:** `96fc354`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** CMake rejects noncanonical paths and symlink ancestors, creates a unique marked run directory strictly below the configured scratch root, and does not recursively delete caller paths. C output cleanup, proof, and companion writes validate the canonical owned directory and marker, use exclusive temporary files for output, and clean only known direct regular files. The symlink-ancestor control preserves outside sentinels and records.

### WR-01: WARNING — Timer control batches can silently lose or duplicate cases

**Files modified:** `tests/suites/run.cmake`, `docs/public-suites.md`
**Commit:** `96fc354`; documentation in `f1cbaf0`
**Disposition:** fixed; pending independent automated re-audit
**Applied fix:** Ordered unique-name manifests now require exactly 49 result cases and 61 pending cases, reject missing and duplicate names with precise refusals, and keep all mutations data-only. Plan 09's 48 is retained as a historical count; Plan 10 added the copied Lynx II case to make the pre-fix result inventory 49. The pending inventory grows from 55 to 61 through six distinct output/identity cases.

### IN-01: INFO — CLI usage text omits supported timer and validator commands

**Files modified:** `tests/suites/test_public_suites.c`, `tests/suites/run.cmake`
**Commit:** `96fc354`
**Applied fix:** The error help lists timer observation, selected-profile, no-run, bounded, and related command forms, and says profiles are `lynx1` or `lynx2` passed as a single argument. The missing-profile control checks the expanded help.

### IN-02: INFO — Timer observation example is parsed as a shell pipeline

**Files modified:** `docs/public-suites.md`
**Commit:** `f1cbaf0`
**Applied fix:** The command now uses `PROFILE (lynx1 or lynx2)` and explains that a profile is a single argument without a shell pipe.

### IN-03: INFO — README opening omits the newly documented timer suite

**Files modified:** `README.md`
**Commit:** `f1cbaf0`
**Applied fix:** The opening summary now includes headless CPU/timer observations and states that the timer source disagreement remains pending and strict comparison fails on that row.

## Focused Verification

- `cmake --build --preset check --target lynx-test-suites` — passed.
- `ctest --preset check --no-tests=error --output-on-failure -R '^suite\\.timers\\.(result_controls|pending_controls)$'` — result controls passed (43.31s); the initial pending run exposed only CMake diagnostic line wrapping in the new exact-refusal assertion.
- Pure manifest controls — missing and duplicate manifests each exited 1 with their exact normalized refusal.
- `ctest --preset check --no-tests=error --output-on-failure -R '^suite\\.timers\\.pending_controls$'` — passed after whitespace normalization (55.31s); this was the final full pending batch.
- `ctest --preset check --no-tests=error --output-on-failure -R '^suite\\.cpu\\.model_controls$'` — passed (11.76s), including the updated CLI help assertion.
- `sh scripts/hygiene.sh --tree` — passed.
- `sh scripts/scan-binaries.sh --source` — passed (331 files scanned; no 512-byte candidates).
- `git diff --check` — passed before both fix commits.

The source comparison at D-10 remains unchanged: timer index 14 is observed as `0c` while source expects `0d`, and strict timer comparison still exits 1. This work makes no hardware qualification claim; Plan 12 Task 2 and final qualification remain pending, and global EVD-02/EVD-05 remain open.

---

_Fixed: 2026-11-11T19:31:29Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_

## Root independent recheck and supplemental guard

Root independently reviewed the actual delta and ran the affected parser,
symbol, timer observation/strict/pending and CPU/tracer tests successfully.
Source27145c8 additionally validates profiles before timer path construction,
uses unlink so output cleanup cannot remove directories, declares the POSIX
filesystem feature level for Linux, and adds permanent rejection controls. A
sanitized isolated old-remove control proves the prior directory deletion.
All original findings now reconcile as fixed in04-REVIEW-DISPOSITION.md; no
human verification is required. Final platform/CI qualification remains pending.
