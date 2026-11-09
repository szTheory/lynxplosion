---
phase: 01-evidence-harness-and-walking-skeleton
plan: 05
subsystem: determinism-boundary
tags: [cmake, nm, abi, determinism, negative-controls, header]
status: complete

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-04 expect_fail.cmake, exact inventory and junit step; plans 01-01/01-02 core sources, public header, LYNX_CORE_SOURCES"
provides:
  - "core.includes and core.no_float source scans over core/ and include/, each with a self-test"
  - "lynx_core_plain: uninstrumented twin of the core, the only input of nm checks"
  - "tests/cmake/abi.cmake (MODE=undefined|globals|symbols): core.nm_undefined, core.nm_globals, api.abi_symbols, each with a self-test"
  - "lynx-test-header: public header alone under C17 -pedantic-errors with static layout assertions and a signature table"
  - "Inventory of 21 tests"
affects: [01-06, 01-07, 01-08]

actuals:
  tokens: 9000
  tasks: 2
  commits: 2

plan_head_before: f2e1b9e4cd544adf7de63fa1ef295e9239ae5ff7
plan_head_after: 7bf9945de946e667eebbc33cf1f82fdba8632b2a
commits: 2

tech-stack:
  added: []
  patterns:
    - "Link-surface checks read an uninstrumented twin built from LYNX_CORE_SOURCES, never the sanitized lynx_core"
    - "Fixture libraries for negative controls copy the twin's COMPILE_OPTIONS via get_target_property"

key-files:
  created:
    - tests/cmake/core_includes.cmake
    - tests/cmake/no_float.cmake
    - tests/cmake/abi.cmake
    - tests/abi/tests.cmake
    - tests/abi/libc_fixture.c
    - tests/abi/global_fixture.c
    - tests/header/header.c
  modified:
    - CMakeLists.txt
    - tests/tests.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "lynx-test-header links lynx_core_plain (not 'its own object only'): its signature table references every public function, so the link needs the definitions. The twin is unsanitized, so the executable needs no sanitizer runtime."
  - "Helper allowlist keeps only the Darwin bzero/__bzero and Linux x86_64 _GLOBAL_OFFSET_TABLE_ rows; the Windows and ppc rows from muhtari were dropped"

requirements-completed: [FND-05, API-01]

duration: about 15 minutes
completed: 2026-11-09
---

# Phase 1 Plan 05: Determinism boundary checks Summary

Source scans (include allowlist, floating point) and nm link-surface checks (undefined symbols, writable data, public-name equality) on an uninstrumented twin of the core, each backed by a built violation, plus a header self-containment test.

## What was built

- Task 1 (c0c6667): `core.includes` admits only `stdint.h`, `stddef.h`, `stdbool.h`, `string.h` and project headers present in the scanned root (or `lynxplosion.h`); `assert.h` and `limits.h` are refused. `core.no_float` is the muhtari scan (provenance header, commit 7fd0386). Self-tests: `<stdio.h>` and `<time.h>` are reported at `sample.c:1` and `:2` while a quoted `internal.h` beside the sample is accepted (count is 2); `x = 1.5;` is reported at `sample.c:2` while `0x1E5` and `double_free` are not (count is 1).
- Task 2 (7bf9945): `lynx_core_plain` (STATIC, same `LYNX_CORE_SOURCES`, `-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector`, both file-prefix maps, hidden visibility, PIC, no sanitizer). `abi.cmake` adapted from muhtari with `lynx_`/`lynx__` prefixes. Fixtures `lynx__abi_libc` (time, malloc/free) and `lynx__abi_global` (written static `counter`) are built with the twin's own compile options. The symbols self-test uses a copy of the real header with `lynx_not_defined(void)` added, written under `${LYNX_SCRATCH_DIR}/gates/abi_symbols/` at configure time.

## Measured link surface (Darwin arm64, AppleClang 21)

- Undefined symbols of `liblynx_core_plain.a`: `_bzero` and `_memcpy` only (both allowlisted).
- Writable data (B/b/D/d/C): none.
- Defined externals: `lynx_*` equal the 12 header functions exactly; internal names are `lynx__budget_steps`, `lynx__build_id` (S, read-only data), `lynx__power_on`, `lynx__sha256_{init,update,final,hex}`, `lynx__skeleton_step`.
- Linux x86_64 not measured here (no Linux host in this run); the `_GLOBAL_OFFSET_TABLE_` row is carried over from muhtari and will be exercised by CI.

## Timings

- `sh scripts/check.sh`: total 2 s (workflow step 2 s, junit step under 1 s), 21 of 21 tests passed, 21 accounted for once each in JUnit.
- The 11 tests added here run in about 0.2 s in total.

## Deviations from Plan

**1. [Rule 3 - Blocking] lynx-test-header links lynx_core_plain**
- Found during: Task 2
- Issue: the plan says the header test links only its own object, but its static function-pointer table (required by the plan, with `main` referencing each entry) refers to every public function; at the default `-O0` the table is emitted and an unresolved link results.
- Fix: link the unsanitized twin `lynx_core_plain`. The header is still compiled alone with warnings only (`-Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror`, C17). The test additionally shows that every declared function has a definition.
- Files modified: CMakeLists.txt
- Commit: 7bf9945

**2. [Minor] Fixture libc function returns uint32_t**
- The plan sketched `void lynx__abi_libc(void)`. It returns `uint32_t` and uses a volatile pointer so the optimizer cannot drop `time` or `malloc`; both appear as undefined symbols. Reported finding names `time`.

No core comments were flagged by either source scan, so no rewording was needed.

## Notes for the next plan

- Plans 01-06 and 01-07 must add `lynx_reset` and the snapshot functions to the signature table in `tests/header/header.c` when they declare them, and any new core source must be appended to `LYNX_CORE_SOURCES` (both `lynx_core` and `lynx_core_plain` pick it up).
- `api.abi_symbols` derives declared names from `lynx_[a-z0-9_]*(` anywhere in the header, comments included: a comment that writes an undeclared `lynx_name(` fails the check. Write function names in comments without parentheses unless declared.
- New gate tests follow the existing convention: one file registered by `tests/tests.cmake`, sorted rows in `tests/inventory/fast.txt`, fixtures under `${LYNX_SCRATCH_DIR}`.

## Known Stubs

None.

## Threat Flags

None. T-01-07 and T-01-08 are mitigated: the sanitized `lynx_core` is never an `nm` input (`grep 'TARGET_FILE:lynx_core>' tests/abi/tests.cmake` finds nothing).

## Self-Check: PASSED

All created files exist; commits c0c6667 and 7bf9945 are on HEAD; `sh scripts/check.sh` exits 0.
