---
phase: 01-evidence-harness-and-walking-skeleton
plan: 04
subsystem: test-inventory
tags: [cmake, ctest, junit, inventory, negative-controls, no-skip]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-01 cmake/Tests.cmake (lynx_add_test, configure-time inventory check), tests/inventory/fast.txt, scripts/check.sh insertion points; plan 01-03 inventory of 6 tests"
provides:
  - "tests/cmake/inventory_equal.cmake: lynx_inventory_compare(registered_file inventory_file), shared by configure and the self-tests"
  - "tests/cmake/check_junit.cmake: JUnit completeness (-DJUNIT -DINVENTORY -DSKIPS) with declared-skip support"
  - "tests/cmake/expect_fail.cmake: the single negative-control runner (-DCMD -DCODE -DREGEX), reused by later gates"
  - "tests/cmake/no_skip.cmake and policy.no_skip: no test can skip or disable itself"
  - "tests/cmake/inventory_controls.cmake: 25 built-fault cases over the two checkers"
  - "scripts/check.sh step junit after workflow"
  - "Inventory of 10 tests; build/check/inventory/registered.txt written at configure time"
affects: [01-05, 01-06, 01-07, 01-08, 01-09, 01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 6600
  tasks: 2
  commits: 2

plan_head_before: 46a1102ec61827b377cf161312a3a782a0522ce3
plan_head_after: cf0b01e9c39b9389053b16390b243751719dcfce

tech-stack:
  added: []
  patterns:
    - "A negative control builds its fault at test time, runs the real checker in a separate process and requires the exact exit code plus a message regex"
    - "Gate tests are registered in tests/gates/*.cmake and include()d from tests/tests.cmake, so every name stays in the top-level TESTS property"
    - "Forbidden property names are assembled from parts in the registering file, so no committed file matches the no-skip scan"

key-files:
  created:
    - tests/cmake/expect_fail.cmake
    - tests/cmake/inventory_equal.cmake
    - tests/cmake/check_junit.cmake
    - tests/cmake/inventory_controls.cmake
    - tests/cmake/no_skip.cmake
    - tests/gates/inventory.cmake
    - tests/inventory/skips.txt
  modified:
    - cmake/Tests.cmake
    - tests/tests.cmake
    - tests/inventory/fast.txt
    - scripts/check.sh

key-decisions:
  - "Name comparison and label comparison are separate: a label mismatch reports `inventory: labels differ: <name>`, a name mismatch reports registered-but-not-listed or listed-but-not-registered"
  - "A declared skip is also checked against the inventory (declared but not in the inventory) and against the JUnit file (declared but ran); a declared skip absent from JUnit reports the 0-times finding, not a misleading 'ran'"
  - "Name and field splitting uses string(FIND) on the tab, because the CMake regex `^[^\\t]*\\t?` returned an empty remainder on this CMake"

patterns-established:
  - "Pattern: later gate plans add one tests/gates/<gate>.cmake file with a self_test per rule and one include line in tests/tests.cmake, plus rows in fast.txt (LC_ALL=C order; tab sorts before the dot, so x.y precedes x.y.self_test)"
  - "Pattern: to see a control's output run `ctest --preset check -R '^<name>' -V` and look for `inventory_controls: <case>: ok` lines"

requirements-completed: [FND-06, FND-01]

coverage:
  - id: D1
    description: "Configure fails when a registered test is missing from fast.txt, a listed test is not registered, a row is duplicated, or the file is not LC_ALL=C sorted; registration order does not matter"
    requirement: FND-06
    verification:
      - kind: integration
        ref: "tests/cmake/inventory_controls.cmake MODE=registered (ctest inventory.registered.self_test)"
        status: pass
      - kind: other
        ref: "manual: removed core.sha256 from fast.txt, configure printed `inventory: registered but not listed: core.sha256`"
        status: pass
    human_judgment: false
  - id: D2
    description: "check_junit.cmake fails unless every inventory name appears once with status run and no failure, error or skipped element; duplicates, extras, an empty JUnit file and name adjacency (x.y vs x.y.self_test) are handled"
    requirement: FND-06
    verification:
      - kind: integration
        ref: "tests/cmake/inventory_controls.cmake MODE=junit (ctest inventory.junit.self_test)"
        status: pass
      - kind: integration
        ref: "sh scripts/check.sh step junit against the real build/check/junit.xml (8 then 10 tests accounted for)"
        status: pass
    human_judgment: false
  - id: D3
    description: "A skip passes only when skips.txt lists it with a non-empty reason; an empty reason and a declared skip that ran both fail; a header-only skips.txt means no skips"
    requirement: FND-06
    verification:
      - kind: integration
        ref: "inventory_controls junit cases declared_skip, undeclared_skip, empty_reason, stale_skip, all_run"
        status: pass
    human_judgment: false
  - id: D4
    description: "No CMake file can set a skip or disable test property: policy.no_skip scans tests/**/*.cmake, cmake/*.cmake and CMakeLists.txt, and its self-test requires exit 1 naming the fixture file and line"
    requirement: FND-06
    verification:
      - kind: integration
        ref: "ctest policy.no_skip and policy.no_skip.self_test"
        status: pass
    human_judgment: false
  - id: D5
    description: "The check command runs the JUnit completeness step after the workflow and the full check is green with 10 registered tests"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "sh scripts/check.sh (cold and warm)"
        status: pass
    human_judgment: false

duration: 3min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 04: Exact test inventory and negative-control harness Summary

**A test that is missing, extra, duplicated, failed, or skipped without a declared reason can no longer leave `scripts/check.sh` green: the registered set must equal `fast.txt` at configure time, the executed set must equal it in the JUnit file, and 25 built-fault cases (plus the no-skip fixture control) prove each rule fires with the exact exit code and message.**

## Performance

- **Duration:** about 3 min of executor wall time (start captured after the context reads)
- **Completed:** 2026-11-09
- **Tasks:** 2 of 2
- **Files:** 7 created, 4 modified

## Accomplishments

- `lynx_inventory_compare` was factored out of `cmake/Tests.cmake` into `tests/cmake/inventory_equal.cmake`. The configure-time check writes `build/check/inventory/registered.txt` and calls it, so the code a developer hits at configure is the code the self-test exercises. It reports every finding in one `FATAL_ERROR`, one `inventory:` line each.
- `check_junit.cmake` extends the muhtari file with `-DSKIPS` and drops its label filters (one lane). It also catches a declared skip missing from the inventory and rejects a name that ran without being listed.
- `inventory_controls.cmake` runs 25 cases across the two self-tests (15 junit-mode, 10 registered-mode), including the three FND-06 edges: adjacency (`x.y` and `x.y.self_test` distinct, and swapped order reported as unsorted), empty (`junit: no test cases`, header-only `skips.txt` accepted) and ordering (registration order free, file order enforced).
- `policy.no_skip.self_test` writes a fixture at configure time with its forbidden property assembled from parts, so the PII/rights scans and `policy.no_skip` itself never see the name committed.
- I confirmed end to end, outside the self-tests, that a tampered `build/check/junit.xml` (one test renamed to another) makes the junit step exit 1 with `api.identity: ... 0 times` and `api.skeleton: ... 2 times`, and that deleting a row from `fast.txt` fails configure.

## Task Commits

1. **Task 1: Shared inventory compare, JUnit completeness with declared skips, and their self-tests** - `dacd5b1` (feat)
2. **Task 2: No self-skip policy and the junit step in scripts/check.sh** - `cf0b01e` (feat)

TDD note: Task 1 is `tdd="true"`; the plan scopes one commit per task, so checker and controls landed together. The controls found two real defects while being written, both in my first draft (below).

## Measured timings

Local (Apple clang 21, CMake 4.4.3, Ninja 1.13.2), `sh scripts/check.sh`:

| Run | identities | shellcheck | workflow | junit | check total |
|---|---|---|---|---|---|
| cold (`build/check` removed) | 0s | 1s | 3s | 0s | 4s |
| warm | not separately timed (exit 0, total about 4s) | | | | |

Per test (JUnit `time`, ASan+UBSan): `runner.cli` 1.96s (still the slowest), `det.two_runs` 0.27s, `core.sha256` 0.16s, `inventory.junit.self_test` 0.15s, `inventory.registered.self_test` 0.10s, the three api tests 0.10 to 0.11s, `policy.no_skip` 0.03s, `policy.no_skip.self_test` 0.02s. The junit step itself costs under 1s. No push and no CI run in this plan, so no CI timings.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] CMake regex dropped the labels/reason field**
- **Found during:** Task 1, first run of `inventory.junit.self_test` (`declared_skip` case reported `empty reason` for a row that had one)
- **Issue:** `string(REGEX REPLACE "^[^\t]*\t?" "" ...)` returned an empty string on this CMake instead of the text after the first tab. The same pattern in the label comparison would have silently compared empty labels.
- **Fix:** split on the first tab with `string(FIND)` and `string(SUBSTRING)` in both `check_junit.cmake` and `inventory_equal.cmake` (shared helper `lynx__inventory_split`).
- **Files modified:** `tests/cmake/check_junit.cmake`, `tests/cmake/inventory_equal.cmake`
- **Commit:** dacd5b1 (fixed before the commit)

**2. [Rule 1 - Bug] Own control regex too strict for the unsorted message**
- **Found during:** Task 1, `adjacent_names_swapped` case
- **Issue:** the first draft of the message carried an explanatory suffix, so the line-anchored regex for `inventory: not sorted` did not match.
- **Fix:** the message is exactly `inventory: not sorted`, as the plan specifies; the tab-sorts-first explanation moved to the file header comment. No expected value was relaxed.
- **Files modified:** `tests/cmake/inventory_equal.cmake`
- **Commit:** dacd5b1

### Interpretation notes

- The plan's fails-when text expects "100% tests passed, 0 tests failed out of N"; CTest 4.x prints "100% tests passed out of N" (as plans 01-02 and 01-03 noted). I relied on exit status, `-V` output and JUnit.
- The plan lists behaviour cases for the compare and the JUnit checker; the controls add extras it did not list: `labels_differ`, `empty_inventory`, `errored` and `did_not_run` (status notrun without a skipped element), `adjacent_names_twice` and `adjacent_names_missing`. Nothing was relaxed.
- The JUnit finding for a duplicated name reads `<name>: in the JUnit file 2 times, expected once` (the muhtari suffix is kept); the plan's text is a prefix of it.
- `inventory_controls.cmake` calls the checkers with `execute_process` directly rather than through `expect_fail.cmake`, to keep the `inventory_controls: <case>: expected ..., got ...` message the plan asks for. `expect_fail.cmake` is used by `policy.no_skip.self_test` and is the runner for later gates.

**Total deviations:** 2 auto-fixed (both test-writing bugs caught by the controls), 4 interpretation notes. **Impact:** none on scope.

## Issues Encountered

- `.planning/state.json` was untracked at start (orchestrator state) and is not in any commit.
- A stray shell line of mine tried `rm` on `/tmp/x`, which is a directory, and failed harmlessly; nothing was removed.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. T-01-05 (registration and JUnit repudiation): mitigated by configure-time equality, run-time JUnit completeness (each name once, `status="run"`, no failure/error/skipped), the declared-skip list with reasons and `policy.no_skip`. T-01-06 (negative controls tampering): `expect_fail.cmake` and `inventory_controls.cmake` both require the exact exit code and a message regex, so a crash, typo or vacuous pass fails the control. T-01-SC: no packages installed.

## Notes for the next plans

- To add a gate: one `tests/gates/<gate>.cmake` with the real test plus a `*.self_test` using `expect_fail.cmake` (`-DCMD` list with the `string(REPLACE ";" "$<SEMICOLON>" ...)` idiom, `-DCODE`, `-DREGEX`), one `include()` line in `tests/tests.cmake`, and rows in `fast.txt` in `LC_ALL=C` order. Fixtures live under `${LYNX_SCRATCH_DIR}` and are written at configure or test time, never committed.
- `scripts/check.sh` insertion point for artifacts and history steps is now after the `junit` step; the pins/cc65/probe point before shellcheck is unchanged.
- Any plan that adds a test must add its row to `fast.txt` in the same commit, or configure fails; a test that skips must be declared in `tests/inventory/skips.txt` with a reason (Phase 1 has none).
- `policy.no_skip` scans for the skip and disable test properties in `tests/**/*.cmake`, `cmake/*.cmake` and `CMakeLists.txt`; a new gate file that must mention such a name should assemble it from parts as `tests/gates/inventory.cmake` does.
- The label check means a label change on a registered test also needs the `fast.txt` row updated.

## Next Phase Readiness

Ready for the next plan in the wave order. Nothing blocks. Open owner items unchanged: O1, O5, O6.

## Self-Check: PASSED

Files present: tests/cmake/expect_fail.cmake, inventory_equal.cmake, check_junit.cmake, inventory_controls.cmake, no_skip.cmake, tests/gates/inventory.cmake, tests/inventory/skips.txt. Commits present: dacd5b1, cf0b01e. All acceptance criteria re-run: `ctest -R '^inventory\.'` shows `: ok` for every case, `inventory_equal.cmake` and `check_junit.cmake` exit 0 after the workflow, `LC_ALL=C sort -c` on fast.txt exits 0, both provenance lines present, `no_skip.cmake -DROOT=.` exits 0, `sh scripts/check.sh` prints `== step junit` after `== step workflow` and exits 0 cold and warm.
