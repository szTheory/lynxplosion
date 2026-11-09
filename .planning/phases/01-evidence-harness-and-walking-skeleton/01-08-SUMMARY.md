---
phase: 01-evidence-harness-and-walking-skeleton
plan: 08
subsystem: determinism-gates
tags: [c17, determinism, golden-digest, sensitivity, key-value-validator, run-manifest, ci]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-03: lynx-run, bundle_observe, bundle_write, bundle_read_hex, det.two_runs; plan 01-04: expect_fail.cmake and the inventory scripts; plan 01-07: snapshots (not used here)"
provides:
  - "tests/golden/skeleton.digest: committed cross-compiler observation digest of the fixed skeleton workload (4e63d100...b251d), proven on Apple clang and on Linux gcc"
  - "det.golden, det.golden.self_test, det.two_instances: golden comparison, edited-golden rejection, two instances in one process with whole-bundle equality"
  - "det.sensitivity, det.sensitivity_state: seed, content byte, tick budget, and single bits of register a, guest time and RAM each change the digest"
  - "scripts/validate-kv.sh: strict key = value validator (--required, --optional, --no-host-data) for run manifests, and later fixture manifests and claims"
  - "det.manifest_fields, det.manifest_fields.self_test: runner manifests validated; 18 built control manifests"
  - "lynx-test-det executable (cases two_instances, state_sensitivity); inventory now 36 tests"
affects: [01-09, 01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 10200
  tasks: 3
  commits: 4

plan_head_before: aeb609eff87bc176518ca52e2d1dd2065b15268f
plan_head_after: b7a74325c605547f5860edbbfcf659157a8d9a89

tech-stack:
  added: []
  patterns:
    - "Golden digest is the observation digest only: it excludes core_build, so it is build-independent and comparable across compilers; the bundle digest embeds the git revision and can never be golden"
    - "Negative controls build their faulty inputs at test time (changed content, edited golden, faulty manifests, home paths from parts); nothing faulty is committed"
    - "A -P CMake script that uses IN_LIST sets cmake_policy(VERSION 3.25) itself (CI cmake 3.31.6 leaves CMP0057 off in script mode)"

key-files:
  created:
    - tests/golden/skeleton.digest
    - tests/det/golden.cmake
    - tests/det/golden_self_test.cmake
    - tests/det/test_det.c
    - tests/det/sensitivity.cmake
    - tests/det/manifest_fields.cmake
    - tests/det/manifest_controls.sh
    - scripts/validate-kv.sh
  modified:
    - tests/det/tests.cmake
    - tests/inventory/fast.txt
    - tests/cmake/inventory_equal.cmake
    - tests/cmake/check_junit.cmake

key-decisions:
  - "RESEARCH open question 3 resolved as add it: the golden holds the observation digest of the fixed workload, regenerated only when skeleton behaviour changes on purpose, with the reason in the commit message"
  - "validate-kv keys match [a-z][a-z0-9_]* (digits allowed) because content_sha256 contains digits; the plan text said [a-z_]+"
  - "validate-kv is strict about layout: no comments and no blank lines, one ' = ' per line; a later plan that wants comments in fixture manifests must add an option rather than loosen this"
  - "det.golden.self_test builds its edited golden in a wrapper script (golden_self_test.cmake) at test time, then runs golden.cmake through expect_fail.cmake"

patterns-established:
  - "Pattern: in-process whole-bundle equality writes each instance with bundle_write into its own fresh directory and compares every bundle file byte for byte, not only digests.txt"
  - "Pattern: white-box state flips copy the instance with memcpy (no pointers in the struct) and compare bundle_observe digests"

requirements-completed: [DET-01, DET-02, DET-04]

coverage:
  - id: D1
    description: "Two runner processes from fresh state agree on observation and bundle digests (DET-01), and the observation digest equals the committed golden on Apple clang and on Linux gcc; an edited golden fails det.golden"
    requirement: DET-01
    verification:
      - kind: integration
        ref: "ctest det.two_runs, det.golden, det.golden.self_test; CI run 42720600917 (ubuntu-24.04, gcc 13.3.0) green"
        status: pass
    human_judgment: false
  - id: D2
    description: "Two instances advanced interleaved in 512-tick chunks and written with bundle_write into fresh directories produce byte-identical bundle files and equal bundle digests; their observation digest equals the golden"
    requirement: DET-02
    verification:
      - kind: integration
        ref: "ctest det.two_instances (lynx-test-det two_instances), under ASan+UBSan"
        status: pass
    human_judgment: false
  - id: D3
    description: "Digest equality is not vacuous: seed, one content byte, the tick budget, one bit of register a, of guest time and of RAM byte 0x0100 each change the observation digest; an unmodified copy does not"
    requirement: DET-01
    verification:
      - kind: integration
        ref: "ctest det.sensitivity, det.sensitivity_state"
        status: pass
    human_judgment: false
  - id: D4
    description: "run.manifest passes validate-kv with the twelve required keys and --no-host-data, in the documented key order; zero and UINT64_MAX budgets are recorded exactly; the empty input digest is the SHA-256 of zero bytes; digests.txt passes LC_ALL=C sort -c; the seed is 0x plus 16 lowercase hex digits"
    requirement: DET-04
    verification:
      - kind: integration
        ref: "ctest det.manifest_fields"
        status: pass
    human_judgment: false
  - id: D5
    description: "validate-kv rejects a missing key, an unknown key, a repeated key (adjacent or not), an empty value, a malformed line, a 63-digit and an uppercase sha256, startup_tier B, evidence_tier T9, an absolute path, a tilde, home-directory paths and a date under --no-host-data, and accepts a URL without that flag"
    requirement: DET-04
    verification:
      - kind: integration
        ref: "ctest det.manifest_fields.self_test (tests/det/manifest_controls.sh, 18 cases, each exit status and message checked)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Every digest line the runner prints is labelled repeatability (R), not accuracy, and manifests record evidence_tier = R and profile = none (prohibition DET-01 transparency)"
    requirement: DET-01
    verification:
      - kind: integration
        ref: "tests/det/manifest_fields.cmake label and tier checks (ctest det.manifest_fields); tests/det/two_runs.cmake"
        status: pass
    human_judgment: false
  - id: D7
    description: "The full check is green locally and on CI for the pushed HEAD"
    requirement: DET-01
    verification:
      - kind: integration
        ref: "sh scripts/check.sh exit 0; CI run 42720600917 conclusion success, headSha b7a74325c605547f5860edbbfcf659157a8d9a89"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 08: Determinism gates Summary

**The fixed-workload observation digest is committed as a cross-compiler golden (4e63d100...b251d, equal on Apple clang and on Linux gcc), two instances in one process produce byte-identical whole bundles, sensitivity controls show the digest reacts to seed, content, budget and single register, time and RAM bits, and `scripts/validate-kv.sh` strictly validates run manifests with 18 built controls.**

## Performance

- **Duration:** about 25 min (no start timestamp was captured; read from the session and commit times 18:23 to 18:27 local plus the work before the first commit).
- **Tasks:** 3 of 3, 4 commits (3 task commits plus one CI fix).
- **Files:** 8 created, 4 modified; diff 40935 characters, so `actuals.tokens` is 10200 on the chars/4 scale (the estimate was 70000, so the plan was over-estimated by about 7x).
- **Local `sh scripts/check.sh`:** 5 to 6 s total (workflow step 5 s, junit 0 s, shellcheck under 1 s), 36 tests accounted for once each. New tests are fast: det group 1.28 s of process time for 8 tests under ASan+UBSan (det.sensitivity 0.43 s is the slowest, five runner processes).
- **CI, run 42720450205 (first push, head f829228): failure in 16 s job time**, all 8 `det.*` tests passed on gcc including `det.golden`; two `inventory.*.self_test` tests failed (see Deviations).
- **CI, run 42720600917 (head b7a74325, the final HEAD): success, 11 s run, 9 s job.** Steps: set up job 0 s, checkout 1 s, `scripts/check.sh` 5 s (shellcheck 0 s, workflow 4 s with 36 tests, junit 0 s, in-script total 5 s), post-checkout and completion 0 s. Runner image `ubuntu24` 20261101.360.1, cc 13.3.0, cmake 3.31.6, ninja 1.13.2. The check step is 45% of the job; there is no demonstrated wait to cut (CI efficiency review: nothing to change, the critical path is the 4 s workflow step).

## Accomplishments

- **Golden:** `tests/golden/skeleton.digest` holds the observation digest (comments give workload, regenerate command and the rule). `det.golden` runs the runner on the fixed workload and compares; `det.golden.self_test` writes an edited copy (last hex digit changed) at test time and requires exit 1 with `golden: observation digest` through `expect_fail.cmake`. The digest is the same value measured in plan 01-03 and it matched on gcc on the first CI run: the first cross-compiler proof of the skeleton, no core change needed.
- **Two instances:** `lynx-test-det two_instances` builds two instances from the same content with splitmix64 seed `0x0123456789abcdef`, advances them A, B, A, B in eight 512-tick calls (every call must return `LYNX_STOP_BUDGET`, consumed sums to 4096), requires equal observation digests equal to the golden, writes each with `bundle_write` into fresh `a` and `b` directories and compares all five files byte for byte, plus the bundle digests, plus the `bundle_write` observation digest against `bundle_observe`.
- **Sensitivity:** `det.sensitivity` (seed `...cdf0`, first content byte 03 to 04, ticks 4092 each differ; repeating the base is equal; failure message `sensitivity: <variant>: digest unchanged`). `det.sensitivity_state` copies the instance after the workload with memcpy into four buffers and checks one flipped bit of `a`, of `guest_ticks` and of RAM `0x0100` each change the digest and the unmodified copy does not (white-box via `core/internal.h`, `core/` on the test target's private include path).
- **Validator:** `scripts/validate-kv.sh` (POSIX sh plus awk, shellcheck-clean, no interval expressions): findings print as `validate-kv: <file>: line N: <key>: <message>`; exit 0 clean, 1 findings, 2 usage. `det.manifest_fields` validates real runner output (fixed workload, zero budget, and `UINT64_MAX` budget on a halting image written at test time with `db` at offset 3: `tick_budget = 18446744073709551615`, `ticks_consumed = 12`, `stop_reason = halt`), checks key order, the empty-input digest, byte-sorted `digests.txt` and the seed format. `det.manifest_fields.self_test` runs `tests/det/manifest_controls.sh`: 2 valid-case acceptances, 15 rejections each requiring exit 1 and the named message, and the URL acceptance. Home-path values are assembled from parts, so no committed file holds a literal home path.

## Task Commits

1. **Task 1: golden digest and two instances with whole-bundle equality** - `7368318` (test)
2. **Task 2: sensitivity controls for inputs and state bits** - `c8a9c7a` (test)
3. **Task 3: strict key = value validator and run-manifest field gate** - `f829228` (feat)
4. **CI fix: CMP0057 in the inventory scripts** - `b7a7432` (fix)

The pre-push hygiene check ran before the push (T-01-16): no home-directory path, no credential-shaped string (the RESEARCH Pattern 11 regex), and only the noreply address in tracked files; all 24 unpushed commits carried the noreply author and committer. The only emails in tracked files are the noreply addresses. This push also published the earlier unpushed plan commits 01-02 to 01-07.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] `IN_LIST` rejected by CMake 3.31 in `-P` script mode on the CI runner**
- **Found during:** Task 3 (CI watch). Not caused by this plan's tests, but exposed by the first push of plans 01-02 to 01-07, and it blocked the green run this plan must record.
- **Issue:** `tests/cmake/inventory_equal.cmake` and `tests/cmake/check_junit.cmake` use `if(x IN_LIST y)`. In a `-P` script the policy CMP0057 is not set on cmake 3.31.6 (CI), so `inventory.junit.self_test` and `inventory.registered.self_test` failed with "Unknown arguments specified". The dev host's cmake 4.4.3 defaults it on, so local runs were green. `core_includes.cmake` already had `cmake_minimum_required(VERSION 3.25)`.
- **Fix:** `cmake_policy(VERSION 3.25)` at the top of both files (scoped to the `include()` at configure time).
- **Files modified:** `tests/cmake/inventory_equal.cmake`, `tests/cmake/check_junit.cmake`
- **Commit:** `b7a7432`

**2. [Rule 1 - Bug in plan text] Key pattern `[a-z_]+` cannot match `content_sha256`**
- **Issue:** the plan's behavior line says keys match `[a-z_]+`, but the required keys include `content_sha256`, which contains digits.
- **Fix:** keys match `[a-z][a-z0-9_]*`.
- **Files modified:** `scripts/validate-kv.sh`

### Clarifications (not deviations)

- CTest's local summary is "100% tests passed out of N"; the Linux CTest 3.31 prints "100% tests passed, 0 tests failed out of N". Exit status and JUnit were used for verification.
- Task 1 and Task 2 verification commands name the tests by regex; the sensitivity regex `^det\.sensitivity` matches both new tests as the plan intends.
- One file outside the plan's `files_modified` list was added: `tests/det/golden_self_test.cmake`, which builds the edited golden at test time as the action specifies.

**Total deviations:** 2 auto-fixed. **Impact:** none on scope; the CI fix repaired a latent Linux-only failure in plan 01-04's scripts.

## Issues Encountered

- The first CI run failed on the policy issue above, which took one extra push (the plan allows re-push). No gate was weakened.
- `det.two_instances` printed an `ok` line even when a check failed; the line is now printed only when no check has failed (fixed before the Task 2 commit).

## Known Stubs

None. The golden and every gate here describe the provisional skeleton, not hardware: they are repeatability (R) evidence. Phase 2 replaces the skeleton step, which will change the golden on purpose.

## Threat Flags

None new. T-01-14 (vacuous equality): mitigated by the input and state-bit sensitivity controls, the golden self-test and whole-bundle comparison. T-01-15 (manifest information disclosure): `--no-host-data` is enforced on every runner manifest in `det.manifest_fields`. T-01-16 (public push): hygiene check and noreply identity check done before pushing.

## Notes for the next plan

- Use `scripts/validate-kv.sh` for fixture manifests and claims: pass the key list with `--required` (and `--optional`); a claim or fixture manifest with `#` comments or blank lines is currently rejected as malformed, add an option rather than loosening the default.
- The golden is the observation digest, which does not depend on `core_build`; the bundle digest embeds the git revision (plus `+dirty`) and cannot be a golden.
- To add a determinism gate: one `tests/det/*.cmake` or `lynx-test-det` case, its registration in `tests/det/tests.cmake`, and sorted rows in `tests/inventory/fast.txt`, in the same commit.
- Any `-P` CMake script using `IN_LIST` (or other policy-gated syntax) must call `cmake_policy(VERSION 3.25)` or `cmake_minimum_required`; the CI cmake is 3.31.6 and the dev host's is 4.4.3.
- `lynx-test-det` has `core/` and `tests/` on its private include path, like `lynx-test-api`.
- CI wall time for the whole workflow is about 11 s; the check step is 5 s.

## Self-Check: PASSED

All 8 created files exist; commits `7368318`, `c8a9c7a`, `f829228` and `b7a7432` are ancestors of HEAD; `git rev-list --count aeb609e..HEAD` gave 4 before this summary commit; `sh scripts/check.sh` exits 0; CI run `42720600917` concluded `success` for headSha `b7a74325c605547f5860edbbfcf659157a8d9a89`.
