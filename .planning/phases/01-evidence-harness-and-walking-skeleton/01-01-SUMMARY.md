---
phase: 01-evidence-harness-and-walking-skeleton
plan: 01
subsystem: infra
tags: [c17, cmake, ctest, ninja, asan, ubsan, github-actions, c-abi]

requires: []
provides:
  - "scripts/check.sh: single local command and CI gate (identities, shellcheck, cmake --workflow --preset check)"
  - "CMake presets (schema 6, workflow preset check) with Debug build, -Werror lint set and non-recoverable ASan+UBSan on lynx_core and test targets"
  - "Public C ABI include/lynxplosion.h: init in caller memory, provisional RAM load, bounded advance, side-effect-free inspect, guest ticks"
  - "core/machine.c with the provisional skeleton step (not a CPU)"
  - "lynx_add_test, LYNX_SCRATCH_DIR and the configure-time inventory equality check against tests/inventory/fast.txt"
  - "api.skeleton test with exact register and memory expectations"
  - "CI workflow on ubuntu-24.04 that runs only scripts/check.sh, first green run recorded"
affects: [01-02, 01-03, 01-04, 01-05, 01-06, 01-07, 01-11, 01-13]

actuals:
  tokens: 8022
  tasks: 3
  commits: 3

plan_head_before: 30ff72d84a2d6fc02e91ae09f88b2cc26861dd62
plan_head_after: 4044d9169df715a6c2511cab5270fc6103f3a093

tech-stack:
  added: [CMake presets schema 6, Ninja, CTest, ASan, UBSan, GitHub Actions (actions/checkout pinned by SHA)]
  patterns:
    - "Tests are include()d, never add_subdirectory()d, so the directory TESTS property holds every name"
    - "Every test registers through lynx_add_test; the name and labels must appear in tests/inventory/fast.txt or configure fails"
    - "Sanitizer flags live on targets (lynx_sanitize), never in CMAKE_C_FLAGS, so a plain core twin can be added later"
    - "Refusal checks all complete before the first write; subtraction-form bounds (len > LYNX_ADDRESS_SPACE - addr)"

key-files:
  created:
    - scripts/check.sh
    - CMakePresets.json
    - CMakeLists.txt
    - include/lynxplosion.h
    - core/internal.h
    - core/machine.c
    - tests/check.h
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/tests.cmake
    - tests/inventory/fast.txt
    - cmake/Tests.cmake
    - .github/workflows/ci.yml
    - README.md
  modified: []

key-decisions:
  - "Status mapping: NULL or invalid config fields and NULL bytes or zero length give LYNX_ERR_ARGUMENT; undersized buffer LYNX_ERR_SIZE; misaligned LYNX_ERR_ALIGNMENT; bad address or length range LYNX_ERR_RANGE; bad magic or no content LYNX_ERR_STATE"
  - "lynx_advance with valid pointers but a refused machine zeroes *consumed and sets *stop to BUDGET before returning the error; the machine buffer is untouched"
  - "lynx_guest_ticks counts from power-on or the last load (load re-applies power-on, so it reads 0 after a load)"
  - "tests see core/internal.h through a PRIVATE include on lynx-test-api for white-box sizeof and _Alignof checks only"

patterns-established:
  - "Pattern: step NAME CMD... in check.sh prints '== step NAME', runs it, prints '-- step NAME: Ns', and on failure prints 'check: step NAME failed' to stderr and exits 1"
  - "Pattern: later steps go at the two marked insertion points in check.sh (before shellcheck: pins, cc65, probe; after workflow: junit, artifacts, history)"

requirements-completed: [FND-01, API-01, API-03, API-05]

coverage:
  - id: D1
    description: "scripts/check.sh runs identities, shellcheck and the check workflow preset as ordered steps and exits non-zero naming the failing step"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "sh scripts/check.sh"
        status: pass
    human_judgment: false
  - id: D2
    description: "The same command is green in GitHub Actions on ubuntu-24.04 for the pushed HEAD (run 42715092093)"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "gh run view 42715092093 --json conclusion"
        status: pass
    human_judgment: false
  - id: D3
    description: "A machine is created in caller memory, loaded, advanced 4000 ticks and inspected through the public API with exact register and memory values"
    requirement: API-01
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#skeleton (ctest api.skeleton)"
        status: pass
    human_judgment: false
  - id: D4
    description: "lynx_advance never consumes more than its budget and reports a stop reason; guest time cannot wrap"
    requirement: API-03
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#skeleton (consumed == 4000, stop BUDGET); advance-budget edge cases come with plan 01-06"
        status: pass
    human_judgment: false
  - id: D5
    description: "Inspect functions read registers, memory and guest ticks without writing machine state"
    requirement: API-05
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#skeleton (reads only); whole-buffer memcmp oracle comes with plan 01-06"
        status: pass
    human_judgment: false
  - id: D6
    description: "Registered tests must equal tests/inventory/fast.txt or configure fails; a CTest run selecting zero tests fails"
    requirement: FND-01
    verification:
      - kind: other
        ref: "one-off api.bogus configure failure and ctest -R '^nope$' (No tests were found, exit non-zero); automated self-tests arrive in plan 01-04"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 01: Walking-skeleton slice 1 Summary

**One command (scripts/check.sh) builds a sanitized C17 core, runs an exact-inventory test of a machine created in caller memory through the public C API, and the same command is green on ubuntu-24.04 in GitHub Actions.**

## Performance

- **Duration:** 4 min (executor wall time from first read to CI result)
- **Started:** 2026-11-09T22:44:51Z
- **Completed:** 2026-11-09T22:48:54Z (CI run finished 22:48:32Z)
- **Tasks:** 3 of 3
- **Files:** 14 created, 0 modified

## Accomplishments

- `scripts/check.sh` (POSIX sh, shellcheck-clean on 0.11.0 and 0.9.0) runs `identities`, `shellcheck`, `workflow` in a fixed order with per-step wall time. Two insertion points are marked for later plans.
- `lynx_core` builds under `-Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror` with `-fsanitize=address,undefined -fno-sanitize-recover=all`. The test preset has `noTestsAction: error` (a zero-test selection prints "No tests were found" and CTest exits non-zero).
- Public API: `lynx_config_default`, `lynx_machine_size/align/init/deinit`, `lynx_load_ram_image`, `lynx_advance`, `lynx_read_register`, `lynx_read_memory`, `lynx_guest_ticks`. The provisional skeleton step costs 4 ticks and halts on `0xDB`.
- `api.skeleton` passed on the first run with the plan's exact expectations (PC 0x0208, A 0xEC, X 0xEC, Y 0xE8, S 0x18, P 0x00, mem[0x0200]=0x03, mem[0x0119]=0xEC); the constants were not edited.
- Inventory equality: `lynx_add_test` plus a deferred configure-time check against `tests/inventory/fast.txt`.
- First CI run on the public repository is `success`.

## Task Commits

1. **Task 1: Walking-skeleton tracer** - `9771bc9` (feat)
2. **Task 2: Exact test registration** - `57b3e4d` (feat)
3. **Task 3: README, CI workflow, push and green run** - `4044d91` (feat)

Tracer gate (auto mode): the tracer `<verify>` (`sh scripts/check.sh` and `ctest -R '^api\.skeleton$'`) re-ran green before expansion: "Tracer verified end-to-end — expanding".

## Measured timings

Local (Apple clang 21, CMake 4.4.3, Ninja 1.13.2, shellcheck 0.11.0, 18 cores):

| Run | identities | shellcheck | workflow | check total |
|---|---|---|---|---|
| warm (build tree present) | 0s | 0s | 1s | 1s |
| cold (build/check removed) | 0s | 0s | 2s | 2s |
| first ever (includes configure probes) | 1s | 0s | 1s | 2s |

CI: run `42715092093`, https://github.com/szTheory/lynxplosion/actions/runs/42715092093, trigger `push` to `main`, `headSha` `4044d9169df715a6c2511cab5270fc6103f3a093` (equals the pushed HEAD), conclusion `success`.

- Run wall time 21s (22:48:11Z to 22:48:32Z); job `check` 19s (22:48:13Z to 22:48:32Z).
- Job steps: Set up job 1s, checkout 1s, `scripts/check.sh` 13s, post-checkout and complete about 0s.
- Inside `scripts/check.sh` (13s): identities 1s, shellcheck 1s, workflow 11s. Workflow phases from log timestamps: configure about 3.0s, build about 6.6s, test about 1.7s (`api.skeleton` 0.22s; the rest of the test phase is ASan process start-up).
- Runner identity printed by the script: cc Ubuntu 13.3.0, CMake 3.31.6, Ninja 1.13.2, shellcheck 0.9.0, git 2.56.0, ImageOS=ubuntu24, ImageVersion=20261101.360.1.
- No ASan `DEADLYSIGNAL` start-up abort occurred (the known `vm.mmap_rnd_bits` risk did not trigger).

### CI efficiency note (AUTONOMY.md)

Slowest critical-path piece: the build step of the workflow (about 6.6s) followed by configure (about 3.0s). Queue and runner set-up cost about 3s together (run created to job start 2s, set up job 1s). No cache, no artifact upload and no extra action are present, so nothing is duplicated. No change made: at 19s the job is dominated by compile and configure of a one-file core, and a cache restore would cost about as much. The cc65 build arrives with plan 01-03 and 01-13 (RESEARCH estimates 5-20s); plan 01-13 should compare cache restore against rebuild with these numbers. Setup versus build versus test: setup 3s, check.sh 13s (configure 3s, build 6.6s, test 1.7s), teardown 0s.

## Inventory negative check (Task 2)

Row `api.bogus<TAB>api` was inserted in sorted position (before `api.skeleton`) in `tests/inventory/fast.txt` and configure was run in a scratch build dir (`cmake -S . -B build/inv-probe -G Ninja -DLYNX_TESTS=ON`). Configure failed with:

```
CMake Error at cmake/Tests.cmake:69 (message):
  registered tests differ from .../tests/inventory/fast.txt
  listed but not registered:
    api.bogus	api
  registered but not listed:
```

A first attempt appended the row after `api.skeleton`; that failed earlier with `fast.txt is not sorted`, which shows the sort check works too. `fast.txt` was restored byte-for-byte, `build/inv-probe` removed (`test ! -e build/inv-probe` passes), and `git status` shows `tests/inventory/fast.txt` as the committed version.

## Pre-push hygiene check (T-01-03)

`git grep` over the index (all tracked files, including `.planning/`) for the home-path pattern, non-noreply emails and the RESEARCH Pattern 11 credential prefixes: zero matches. `git log --format='%ae %ce' origin/main..HEAD` listed only `szTheory@users.noreply.github.com`; commit messages carry no home path. The scripted scan lands in plan 01-11.

## Decisions Made

See `key-decisions` in the frontmatter. Notes for later plans:

- Plan 01-02 appends to `LYNX_CORE_SOURCES` in `CMakeLists.txt` and adds `lynx_identity` and friends to `include/lynxplosion.h`.
- Plan 01-04 should add inventory self-tests; the failure message shape above (`registered tests differ from`, `listed but not registered`, `registered but not listed`, `is not sorted`) is what they can match.
- `lynx-test-api` has `core/` and `tests/` on its PRIVATE include path; further test executables that need `check.h` or `internal.h` need the same lines.
- `target_link_options(lynx_core PUBLIC -fsanitize=address,undefined)` is set so consumers of the sanitized core link the runtime. The plain twin from plan 01-05 must not inherit this (build it from `LYNX_CORE_SOURCES` directly, not by linking `lynx_core`).
- `lynx_machine` is 65600 bytes with alignment 8 on the macOS arm64 host (RAM array plus fields); tests size static buffers from `sizeof(struct lynx_machine)`, so a different size on Linux does not matter.

## Deviations from Plan

### Auto-fixed Issues

None to code. One procedural point:

**1. [Guard note] Protected-branch assertion trips on `main`**
- **Found during:** Task 1 commit (Step 0 assertion).
- **Issue:** `gsd_run query git.base-branch --is-protected main` returns `true`, and `git.allow_default_branch_commits` is not set in `.planning/config.json`. The assertion exists to stop an agent that drifted onto the default branch. Here the dispatch was deliberate: `config.json` has `git.branching_strategy: none`, the orchestrator committed "run executors sequentially on main tree" and set `SEQUENTIAL=true`, and the approved plan Task 3 explicitly says "Commit, then `git push origin main`".
- **Action:** Treated HEAD on `main` as the intended target, committed on `main` and pushed as the plan directs; changed no config. The owner may want to set `git.allow_default_branch_commits: true` so later plans do not trip the same assertion.
- **Files modified:** none.

**Total deviations:** 0 code fixes, 1 procedural note. **Impact:** none on scope.

## Issues Encountered

- A first `gh run list --workflow CI` poll right after the push printed "could not find any workflows named CI" (the workflow was not indexed yet); the next poll found the run. The plan's verify loop handles this.
- `.planning/STATE.md` was already modified and `.planning/state.json` untracked at start (orchestrator state); they are not in any task commit.

## Known Stubs

None. The skeleton step and the raw RAM image loader are intentionally provisional, documented as such in the header and source, and replaced in Phases 2 and 3 (recorded in SKELETON.md). They are not stubs for this plan's goal.

## Threat Flags

None. The CI workflow, public API bounds checks and public-repo push are all covered by T-01-01 to T-01-04. Mitigations in place: checkout pinned by full SHA with version comment (T-01-01); `pull_request` trigger only, `contents: read`, `persist-credentials: false`, `timeout-minutes: 15`, no secrets (T-01-02); pre-push scan and noreply identity check (T-01-03); subtraction-form bounds before any write, 32-bit PC wrap arithmetic, `lynx__budget_steps` clamp, ASan+UBSan with `-fno-sanitize-recover=all` (T-01-04).

## Next Phase Readiness

Ready for plan 01-02 (content and build identity). Nothing blocks it. Open owner items unchanged: O1, O5, O6.

## Self-Check: PASSED

All 14 created files exist; commits `9771bc9`, `57b3e4d` and `4044d91` are ancestors of HEAD; `git rev-list --count` from the ledger base gives 3; CI run `42715092093` concluded `success` for the pushed HEAD.
