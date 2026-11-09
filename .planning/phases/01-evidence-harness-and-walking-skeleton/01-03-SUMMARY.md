---
phase: 01-evidence-harness-and-walking-skeleton
plan: 03
subsystem: runner
tags: [c17, runner, artifact-bundle, run-manifest, digests, determinism, cmake-tests]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-02: lynx_get_identity, lynx_build_id, core/sha256 (lynx__sha256_*), LYNX_CORE_SOURCES"
provides:
  - "runner/bundle.{h,c}: bundle_observe, bundle_write, bundle_read_hex, bundle_run (host side, static library lynx_bundle)"
  - "lynx-run executable: --content --load-addr --ticks --out [--ram-policy] [--ram-seed], public API only"
  - "Bundle files: run.manifest (12 keys, fixed order), obs.ram.hex, obs.regs.hex, obs.time.hex, digests.txt (byte-sorted)"
  - "fixtures/skeleton/skeleton-image.hex: the 32-byte tracer image as hex text"
  - "Tests det.two_runs and runner.cli (inventory now 6 tests)"
affects: [01-04, 01-07, 01-08, 01-10, 01-11, 01-13]

actuals:
  tokens: 9600
  tasks: 2
  commits: 2

plan_head_before: ddf15c0ff8fc7f37286b24f4fdd582bad398adcb
plan_head_after: 97fe709f883255854a048c2b11579e6a853a475d

tech-stack:
  added: []
  patterns:
    - "Runner targets get _POSIX_C_SOURCE=200809L via target_compile_definitions on lynx_bundle and lynx-run only, never core/"
    - "Digest of an obs.* file is over canonical bytes (ram, 6 u32 LE registers, u64 LE ticks, u64 LE consumed, u32 LE stop); the file is lowercase hex text, 32 bytes per line"
    - "Fresh-process tests are CMake scripts run with cmake -P that execute_process the runner and recompute the digests independently"

key-files:
  created:
    - runner/bundle.h
    - runner/bundle.c
    - runner/main.c
    - fixtures/skeleton/skeleton-image.hex
    - tests/det/tests.cmake
    - tests/det/two_runs.cmake
    - tests/det/runner_cli.cmake
  modified:
    - CMakeLists.txt
    - tests/tests.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "A refused run creates no output directory: main loads and advances first, then mkdir, then writes the bundle"
  - "--load-addr is 1 to 8 hex digits and --ram-seed 1 to 16 hex digits, no 0x prefix; an address of 10000 or more parses but the core refuses it (exit 1), while a malformed one is a usage error (exit 2)"
  - "Repeated flags are a usage error (exit 2); a value is whatever follows its flag, even when it starts with --"
  - "bundle_read_hex refuses an empty result and caps decoded content at 1 MiB; the core still refuses anything above 64 KiB"
  - "bundle_write refuses a machine with no loaded content, since the manifest would carry a zero content hash"
  - "content_name in bundle_run is kept per the interface but never written to any bundle file"

patterns-established:
  - "Pattern: observation digest = SHA-256 of the sorted obs.* lines of digests.txt; bundle digest = SHA-256 of the whole digests.txt, which lists run.manifest's own hash so build identity enters only the bundle digest"
  - "Pattern: tests check the runner's stdout digests against a CMake file(SHA256) recomputation of the lines and of digests.txt"

requirements-completed: [DET-01, DET-04, API-04]

coverage:
  - id: D1
    description: "Two fresh runner processes with the same inputs write byte-identical digests.txt, run.manifest and obs dumps, and print equal observation and bundle digests, each labelled repeatability (R), not accuracy"
    requirement: DET-01
    verification:
      - kind: integration
        ref: "tests/det/two_runs.cmake (ctest det.two_runs)"
        status: pass
    human_judgment: false
  - id: D2
    description: "run.manifest holds profile, startup_tier, content_sha256, content_load_addr, input_digest, tick_budget, ticks_consumed, stop_reason, core_build, ram_policy, ram_seed, evidence_tier in that order, with no timestamp, host name or path; content_sha256 is the tracer hash"
    requirement: DET-04
    verification:
      - kind: integration
        ref: "tests/det/two_runs.cmake key-order and content_sha256 checks (ctest det.two_runs)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The runner defaults to RAM policy zero and seed 0 and records ram_policy = zero, ram_seed = 0x0000000000000000"
    requirement: API-04
    verification:
      - kind: integration
        ref: "tests/det/runner_cli.cmake default ram policy case (ctest runner.cli)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Exit 2 with a usage: line for a missing, unknown, repeated or valueless flag and for a non-decimal, negative or out-of-range tick count; exit 1 with a lynx-run: message for a missing file, malformed hex, a load past the address space and a load address out of range, with no output directory created"
    requirement: API-04
    verification:
      - kind: integration
        ref: "tests/det/runner_cli.cmake (ctest runner.cli), run under ASan+UBSan"
        status: pass
    human_judgment: false
  - id: D5
    description: "The runner includes only the public header (no core/internal.h)"
    requirement: API-04
    verification:
      - kind: other
        ref: "! grep -q internal.h runner/main.c"
        status: pass
    human_judgment: false
  - id: D6
    description: "The full check (sh scripts/check.sh) is green with all six registered tests"
    requirement: DET-01
    verification:
      - kind: integration
        ref: "sh scripts/check.sh"
        status: pass
    human_judgment: false

duration: 10min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 03: Runner and artifact bundle Summary

**`lynx-run` drives the public API and writes a text-only bundle (run.manifest, three hex observation dumps, byte-sorted digests.txt); two fresh processes agree byte for byte, every printed digest is labelled "repeatability (R), not accuracy", and the CLI contract including the fixed-0x00 default is pinned by tests.**

## Performance

- **Duration:** about 10 min (executor wall time, first read to SUMMARY; start time not captured at launch)
- **Completed:** 2026-11-09T23:05Z
- **Tasks:** 2 of 2
- **Files:** 7 created, 3 modified

## Accomplishments

- `runner/bundle.c` hashes canonical bytes, not the hex text: RAM (65536 bytes), registers (6 x u32 LE, order PC A X Y S P) and time (u64 LE guest ticks, u64 LE ticks consumed, u32 LE stop). Files are lowercase hex, 32 bytes per line. `digests.txt` has four `name sha256` lines sorted with `strcmp` inside the runner. The observation digest hashes the three `obs.*` lines (each with its newline); the bundle digest hashes the whole `digests.txt`.
- `run.manifest` is built from `lynx_get_identity` and `lynx_build_id()`, so the content hash comes from the core. `input_digest` is the SHA-256 of the empty timeline.
- `lynx-run` allocates the instance with `malloc(lynx_machine_size())`, refuses a misaligned block, frees everything on one exit path, and creates the output directory only after load and advance succeed.
- Independent confirmation (not from the runner's own code): with the fixed workload, a Python recomputation of the SHA-256 of the decoded `obs.ram.hex`, `obs.regs.hex` and `obs.time.hex` matched the `digests.txt` lines. `det.two_runs` also recomputes both stdout digests with CMake's `file(SHA256)` over the obs lines and over `digests.txt`.
- Fixed-workload values on this host (Apple clang 21, git rev of plan 01-02's final commit plus dirty tree): observation digest `4e63d10000e55c643268a601dc47770d6b40af24c02c1f95832b7352c14b251d`. The bundle digest changes with the build id (git revision), as designed; only the observation digest is a candidate golden value for plan 01-08. After 4096 ticks the registers read PC 0x0200, ticks 4096, stop budget.

## Task Commits

1. **Task 1: Runner, artifact bundle and two-run digest equality** - `1189ed4` (feat)
2. **Task 2: Runner CLI contract, defaults, usage errors and refusals** - `97fe709` (test)

TDD note: both tasks are `tdd="true"`; the plan scopes one commit per task, so test and implementation landed together and each test was run against the real runner. The Task 2 test found one real error in its own first draft (below), not in the runner.

## Measured timings

Local (Apple clang 21, CMake 4.4.3, Ninja 1.13.2), `sh scripts/check.sh`:

| Run | identities | shellcheck | workflow | check total |
|---|---|---|---|---|
| cold (`build/check` removed) | 0s | 0s | 4s | 4s |
| warm | 0s | 0s | 4s | 4s |

Per test under ASan+UBSan: `det.two_runs` 0.29s (two runs plus recomputation), `runner.cli` about 2.0s (about 20 process launches; the slowest test now, still far under its 30s timeout), the four earlier tests unchanged at 0.11 to 0.18s each. Total test time 3.1s. No push and no CI run in this plan, so no CI timings.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Largest-tick-budget case ran for geological time**
- **Found during:** Task 2, first `runner.cli` run (it hit the 30s timeout; a manual rerun had to be killed)
- **Issue:** my first draft ran the 32-byte tracer image with `--ticks 18446744073709551615`; the skeleton machine never halts on that image, so the core simulated its way toward the budget. The runner was correct; the test case was wrong. Parsing the full range is what the plan asks to pin.
- **Fix:** the case writes a one-byte `db` content file at test time (the skeleton's halt opcode) and runs the maximum budget against it; the manifest must then say `tick_budget = 18446744073709551615` and `stop_reason = halt`. A zero-tick case was added too.
- **Files modified:** `tests/det/runner_cli.cmake`
- **Commit:** 97fe709

### Interpretation notes

- The plan's fails-when text expects "100% tests passed, 0 tests failed out of N"; CTest 4.x here prints "100% tests passed out of N". Rely on exit status or JUnit (as plan 01-02 already noted).
- The plan's behaviour list has seven CLI cases; the test has 18 cases plus the no-output-directory check (repeated flag, missing value, negative ticks, bad load address, bad policy, bad seed, load address 10000, zero ticks, maximum budget). Nothing was relaxed.
- `content_name` is carried in `bundle_run` as the interface lists it, but not written anywhere, because a name could be a path.

**Total deviations:** 1 auto-fix (test-case bug), 3 interpretation notes. **Impact:** none on scope.

## Issues Encountered

- `.planning/state.json` was untracked at start (orchestrator state) and is not in any commit.
- BSD `sed` has no `\b`, so a rename of a local named `time` (to avoid shadowing the libc function) was done with perl instead.

## Known Stubs

None. The machine and the raw RAM loader remain provisional (plan 01-01); the fixture is provisional loader content, and its fixture manifest is deferred to plan 01-10 by design.

## Threat Flags

None beyond the plan's register. T-01-30: `bundle_read_hex` is a strict hex-pair parser, `--ticks` is full-range decimal with overflow refusal, hex flags are length-bounded, and a core refusal exits 1; `runner.cli` pins each case under ASan+UBSan. T-01-31: the bundle holds no timestamp, host name, path, flag or pointer; the only variable text is `core_build` (git revision, compiler id and version, system), already limited by plan 01-02. All bundle files are text. T-01-SC: no packages installed.

## Notes for the next plans

- Plan 01-08 (determinism gates): the observation digest is build-independent and stable across the second run of the same workload; the bundle digest is not golden-able because `core_build` carries the git revision. Sensitivity controls can use `--ram-seed`, one flipped content byte (via a test-written hex file), and `--ticks`. `bundle_observe` computes the observation digest from a live machine with no files, for the two-instances-in-one-process gate; it links through `lynx_bundle`.
- Plan 01-10: the fixture manifest for `fixtures/skeleton/skeleton-image.hex` is still to be added; its SHA-256 of the decoded bytes is `ab5f8b5c...a457` (file hash differs, since the file has comment lines).
- Plan 01-11 rights scan: the fixture and every bundle file are text with no `0xDB`-style magic; bundles go under `build/check/scratch/`.
- Runner targets link `lynx_core` (which links the sanitizer runtime publicly), so the runner uses `lynx__sha256_*` from the core and has no second copy; `lynx_bundle` has a private include of `core/` for `sha256.h` only.
- Output directory handling is one `mkdir`: the parent must exist.

## Next Phase Readiness

Ready for the next plan in the wave order. Nothing blocks. Open owner items unchanged: O1, O5, O6.

## Self-Check: PASSED

Files present: runner/bundle.h, runner/bundle.c, runner/main.c, fixtures/skeleton/skeleton-image.hex, tests/det/tests.cmake, tests/det/two_runs.cmake, tests/det/runner_cli.cmake. Commits present: 1189ed4, 97fe709. `sh scripts/check.sh` green on a cold and a warm build.
