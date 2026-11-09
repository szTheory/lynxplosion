---
phase: 01-evidence-harness-and-walking-skeleton
plan: 02
subsystem: core
tags: [c17, sha256, fips-180-4, build-identity, cmake, configure-file, c-abi]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-01: public C ABI, lynx_machine, LYNX_CORE_SOURCES, lynx_add_test and the exact inventory"
provides:
  - "core/sha256.{c,h}: FIPS 180-4 SHA-256 (lynx__sha256_init/update/final/hex), one unit shared by the core and the runner"
  - "lynx_get_identity and lynx_identity: profile, RAM policy, seed, content present flag, load address, length and content SHA-256 computed inside the core"
  - "lynx_build_id(): configure-time string 'git=<rev>[+dirty] cc=<id>-<version> sys=<name>-<processor>' as read-only data, below 128 bytes"
  - "Tests core.sha256, api.identity, api.build_id"
affects: [01-03, 01-07, 01-11, 01-13]

actuals:
  tokens: 3881
  tasks: 2
  commits: 2

plan_head_before: b52fa8fd9d2e6a20f9e2aa6ba83c40f55756c8b7
plan_head_after: 8b28236f385661e1bf1821a2f17e7f4f19d0f4fd

tech-stack:
  added: []
  patterns:
    - "Build identity is a configure_file output appended to LYNX_CORE_SOURCES; read-only const char array, no writable global"
    - "Content identity is computed at load after every refusal check, stored in the instance, zeroed by lynx__power_on"
    - "One SHA-256 unit in core/ with an internal-only API; no public lynx_sha256 utility (RESEARCH F4)"

key-files:
  created:
    - core/sha256.c
    - core/sha256.h
    - core/build_id.c.in
  modified:
    - core/machine.c
    - core/internal.h
    - include/lynxplosion.h
    - CMakeLists.txt
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "The build identity uses the full 40-character git revision (git rev-parse HEAD), not a short one; the current string is 104 characters with +dirty and 98 clean, so the 127-character configure limit has room for a longer compiler version"
  - "The configure limit is 'greater than 127 characters' which is the plan's '128 or longer' (the snapshot header reserves 128 bytes including the NUL)"
  - "lynx_build_id and its header declaration were added in Task 2, not Task 1, so no commit declares a function without a definition"
  - "A tarball or non-git build uses git=nogit; the git checks run only when the source dir holds a .git entry, so a build tree nested in another repository cannot pick up that repository's revision"

patterns-established:
  - "Pattern: identity strings are fixed at configure time and compared whole; the dirty flag covers tracked files only (--untracked-files=no)"
  - "Pattern: the SHA-256 test registers four independently confirmed vectors; the identity test pins the tracer-image hash through the public API"

requirements-completed: [API-04, DET-04]

coverage:
  - id: D1
    description: "lynx_get_identity reports profile, RAM policy, seed, content flag, load address, length and the SHA-256 of loaded bytes; before a load the flag is 0 and the hash is 32 zero bytes; after loading the 32-byte tracer image at 0x0200 the hash is ab5f8b5c...a457"
    requirement: API-04
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#identity (ctest api.identity)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The core's SHA-256 matches FIPS 180-4 vectors for the empty string, abc, the 56-byte two-block message and one million a bytes in 1000-byte chunks"
    requirement: DET-04
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#sha256 (ctest core.sha256)"
        status: pass
    human_judgment: false
  - id: D3
    description: "lynx_build_id returns the configure-time identity: starts with git=, holds cc= and sys=, shorter than 128 bytes, no slash, tilde or colon, same pointer on every call"
    requirement: DET-04
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#build_id (ctest api.build_id)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Configure fails when the build identity reaches 128 characters"
    requirement: DET-04
    verification:
      - kind: other
        ref: "one-off control in a scratch copy (see Negative control below); configure exit 1, 'build identity is 131 characters, limit is 127'"
        status: pass
    human_judgment: false
  - id: D5
    description: "The full check (sh scripts/check.sh) is green with all four registered tests"
    requirement: DET-04
    verification:
      - kind: integration
        ref: "sh scripts/check.sh"
        status: pass
    human_judgment: false

duration: 5min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 02: Content and build identity Summary

**The core now computes the SHA-256 of loaded content (vector-proven FIPS 180-4 unit adapted from muhtari) and reports it with profile, RAM policy and seed through `lynx_get_identity`, and every build carries a configure-time, path-free build identity string through `lynx_build_id`.**

## Performance

- **Duration:** 5 min (executor wall time, first read to SUMMARY)
- **Started:** 2026-11-09T22:51:21Z
- **Completed:** 2026-11-09T22:55Z
- **Tasks:** 2 of 2
- **Files:** 3 created, 7 modified (395 insertions, 1 deletion in the two task commits)

## Accomplishments

- `core/sha256.{c,h}` copied from `~/projects/muhtari/runner/sha256.{c,h}` with `muhtari__` renamed to `lynx__` and the header comment reworded to cite FIPS 180-4 without dotted section numbers (the no_float scan trap in PATTERNS). Provenance line `Adapted from muhtari (MIT, owner project) at commit 7fd0386.` is in both files. Tables stay `static const` integer arrays.
- `lynx_load_ram_image` hashes the accepted bytes after all validation; `lynx__power_on` zeroes the hash, so a refused load or a fresh power-on leaves a zero hash and `has_content = 0`.
- `lynx_get_identity` validates NULLs and magic before writing, then fills every field and zeroes `reserved`. NULL gives `LYNX_ERR_ARGUMENT`; a deinitialised machine gives `LYNX_ERR_STATE`; `*out` is untouched on error.
- `api.identity` also checks that a refused load (address at the end of the space) leaves the identity unchanged, and that flipping one image byte changes the hash.
- Build identity: `find_package(Git QUIET)`, `git rev-parse HEAD`, `git status --porcelain --untracked-files=no` for `+dirty`, `nogit` fallback, compiler id and version, system name and processor. `configure_file(core/build_id.c.in ... @ONLY)` writes `build/check/generated/build_id.c`, appended to `LYNX_CORE_SOURCES`. No `__DATE__`, `__TIME__` or timestamp anywhere (grep exits 1).
- Measured identity on this host, clean tree at the final commit: `git=8b28236f385661e1bf1821a2f17e7f4f19d0f4fd cc=AppleClang-21.0.0.21000101 sys=Darwin-arm64`. A dirty tree appended `+dirty` during development.

## Task Commits

1. **Task 1: SHA-256 in the core, content identity and FIPS vectors** - `90650d5` (feat)
2. **Task 2: Build identity baked at configure time** - `8b28236` (feat)

TDD note: both tasks are `tdd="true"`; the tests and implementation were written in one commit per task (the plan scopes one commit per task), and each test was run against the real implementation. Independent confirmation of the expected values was done before the constants were written (next section), so no expected constant was edited to match output.

## Vector confirmation (assumption A3)

Each vector was produced independently with `shasum -a 256` before it entered the test:

| Input | shasum -a 256 |
|---|---|
| empty (`printf ''`) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `abc` | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |
| 56-byte two-block message | `248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1` |
| 1,000,000 `a` (`head -c 1000000 /dev/zero \| tr '\0' a`) | `cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0` |
| tracer image (32 bytes via `xxd -r -p`) | `ab5f8b5cb9435354c7b58603592d5faf081e17ceb05f7a7c67f4b666f12ca457` |

All five equal the plan's constants. The core's SHA-256 passed on the first run.

## Negative control (Task 2, 128-character limit)

A scratch copy of the tree under the session scratchpad had ` pad=<70 characters>` inserted into the identity format. Configure there exited 1 with `CMake Error at CMakeLists.txt:54 (message): build identity is 131 characters, limit is 127: git=nogit ...`. A first attempt with a 31-character pad stayed below the limit and configured fine, which also exercised the `nogit` fallback (the scratch copy had no `.git`). The scratch copy is outside the repository; no repo file was changed for the control.

## Measured timings

Local (Apple clang 21, CMake 4.4.3, Ninja 1.13.2, 18 cores), `sh scripts/check.sh`:

| Run | identities | shellcheck | workflow | check total |
|---|---|---|---|---|
| warm | 0s | 0s | 0s | 0s (0.66s wall) |
| cold (`build/check` removed) | 0s | 0s | 1s | 1s |

The four tests (`api.build_id`, `api.identity`, `api.skeleton`, `core.sha256`) take 0.44s together under ASan+UBSan; `core.sha256` is the longest at about 0.15s (one million bytes hashed with instrumentation). No CI run was part of this plan (no push), so there are no CI timings to record; plan 01-01's CI numbers still stand as the baseline.

## Decisions Made

See `key-decisions` in the frontmatter. Notes for later plans:

- Plan 01-03 (runner): include `core/sha256.h` and compile `core/sha256.c` into the runner from `LYNX_CORE_SOURCES`' sibling file list or link `lynx_core`; the unit is internal (`lynx__` names), no public `lynx_sha256`. Read content identity with `lynx_get_identity` rather than hashing the file again; the runner may still hash the bytes it loads as a cross-check, which should agree.
- Plan 01-03 / 01-07: `lynx_build_id()` is the string to record in `run.manifest` and the 128-byte snapshot header field (limit 127 characters plus NUL is enforced at configure time).
- The test target `lynx-test-api` now includes `sha256.h` through its existing `core/` private include path.
- The generated file lives in `build/check/generated/build_id.c`; plan 01-05's uninstrumented twin must build from `LYNX_CORE_SOURCES` (which now holds the generated path as an absolute `${PROJECT_BINARY_DIR}/generated/build_id.c`), not link `lynx_core`.
- Plan 01-04/01-11 scans over `core/`: `core/build_id.c.in` is not a `.c` or `.h` so the glob scans skip it; the generated `.c` sits under `build/`. Neither contains a float, a libc include beyond `internal.h` or a path.
- The build id contains the revision of the commit being built. A commit that only changes `.planning/` still changes the identity string, so golden values must never include it (the observation digest excludes build identity by design, RESEARCH Pattern 9).

## Deviations from Plan

### Auto-fixed Issues

None - plan executed exactly as written, with two small interpretation notes:

- The plan's `<verify>` fails-when text expects the CTest line "100% tests passed, 0 tests failed out of 3". CTest 4.x on this host prints `100% tests passed out of 3` (no failed count when there are none). All three tests passed; the verifier should match "100% tests passed" and the count, not the old wording. Plan 01-04 or later scripts that parse CTest output should use the exit status and JUnit instead.
- `lynx_build_id` was declared in the header in Task 2 rather than together with the rest of the header changes in Task 1, so that each commit builds and no commit declares an undefined function.

**Total deviations:** 0 auto-fixes, 2 interpretation notes. **Impact:** none on scope.

## Issues Encountered

- `.planning/state.json` was untracked at start (orchestrator state) and is not in any commit.
- The agent shell is zsh, so `${PIPESTATUS[0]}` printed nothing in the first negative-control attempt; the control was rerun writing to a file and reading `$?`.

## Known Stubs

None. The skeleton machine and raw RAM loader remain provisional as documented in plan 01-01; this plan adds no stub.

## Threat Flags

None beyond the plan's register. T-01-28: the build id holds only git revision, dirty flag, compiler id and version, and system name and processor; `api.build_id` rejects `/`, `~` and `:`. T-01-29: the content hash is computed in the core from accepted bytes and never supplied by the host; the vectors prove the unit. T-01-SC: no packages installed. The only new trust-boundary surface is the build host's git state and compiler version entering the binary, which the plan covers.

## Next Phase Readiness

Ready for plan 01-03 (runner bundle and manifests): content identity, policy, seed and build identity are available through the public API. Nothing blocks it. Open owner items unchanged: O1, O5, O6.

## Self-Check: PASSED

Created files exist (`core/sha256.c`, `core/sha256.h`, `core/build_id.c.in`, plus generated `build/check/generated/build_id.c`); commits `90650d5` and `8b28236` are ancestors of HEAD; `git rev-list --count` from the ledger base `b52fa8f` gives 2; `core/sha256.c` carries the provenance line; `tests/inventory/fast.txt` holds `api.build_id`, `api.identity`, `core.sha256` in `LC_ALL=C` order; `grep -E '__DATE__|__TIME__'` exits 1; `sh scripts/check.sh` exits 0 on the clean tree with 4 of 4 tests passing.
