---
phase: 01-evidence-harness-and-walking-skeleton
plan: 13
subsystem: close-out-and-ci-evidence
tags: [project-decisions, readme, ci, evidence, efficiency-review, shellcheck, ctest-parallel]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plans 01-01 to 01-12: the whole gate behind scripts/check.sh, the policy and claim documents the README now links"
provides:
  - "PROJECT.md Key Decisions: O8 (C17, CMake 3.25+ workflow presets, Ninja, CTest, C ABI with opaque handle), public repository (owner), O5 licence open, O6 draft, Phase 1 advance unit"
  - "README Documents section linking clean-room policy, exposure record, provenance notice and claim template, with the inventory-binding sentence"
  - "A green GitHub Actions run of scripts/check.sh on ubuntu-24.04 for the last code commit: run 42726754326 (FND-01, FND-09 evidence on the runner)"
  - "Shellcheck 0.9.0 (CI) clean: the SC2317 report on scripts/hygiene.sh fixed"
  - "ctest runs with four parallel test jobs in the check test preset (CI workflow step 10 s to 7 s)"
  - "CI efficiency review, recorded below so it is not repeated"
affects: []

actuals:
  tokens: 865
  tasks: 2
  commits: 3

plan_head_before: ab573c7a2feab8c0693c68d57cd1c6c6b9eac4e8
plan_head_after: 0f5ee304f00919ff9c88043d87d2064a35c1adeb

tech-stack:
  added: []
  patterns:
    - "A shellcheck disable for an indirectly called function names both SC2317 (shellcheck 0.9.0, CI) and SC2329 (0.11, dev host)"
    - "A Linux check run before pushing: an ubuntu:24.04 container with build-essential, cmake, ninja-build and shellcheck runs scripts/check.sh against a clone of the commit"

key-files:
  created: []
  modified:
    - .planning/PROJECT.md
    - README.md
    - scripts/hygiene.sh
    - CMakePresets.json

key-decisions:
  - "No actions/cache for cc65: the cc65 step is 11 to 16 s on the runner, under the 30 s threshold in the plan's decision rule, so the measured candidate is recorded, not applied"
  - "ctest parallelism of 4 jobs set in the test preset execution block (not an environment variable), so local and CI runs behave the same"
  - "The CI cc65 tool hashes differ from the dev host's; only the probe hash is required to match, and it does"

patterns-established:
  - "Pattern: before a push that follows several local-only plans, run scripts/check.sh once in a Linux container to find portability failures without spending CI runs"

requirements-completed: [FND-01, FND-09]

coverage:
  - id: E1
    description: "PROJECT.md records O8, the public repository, O5 open, O6 draft and the Phase 1 advance unit; no licence file exists and the README still says All rights reserved"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "grep checks from the plan's verify command, sh scripts/hygiene.sh --tree exit 0, test ! -e LICENSE LICENSE.md COPYING"
        status: pass
    human_judgment: false
  - id: E2
    description: "README links the four documents and states that claims are bound to the exact test inventory"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "grep docs/claims/claim.template README.md; ctest hygiene.tree"
        status: pass
    human_judgment: false
  - id: E3
    description: "The full gate is green on ubuntu-24.04 for the last code commit, with the same probe hash as the dev host and the manifest"
    requirement: FND-09
    verification:
      - kind: integration
        ref: "CI run 42726754326 (headSha 0f5ee304), conclusion success; probe sha256 = e2fdd981... equals fixtures/manifests/cc65-probe.manifest"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 13: Decisions, README links and final CI evidence Summary

**O8 and the public-repository decision are recorded, the README links the policy and claim documents, and the whole gate (55 tests, cc65 build, probe, artifacts and history scans) is green on the public ubuntu-24.04 runner in 29 s of job time after a ctest-parallelism change.**

## Performance

- **Duration:** about 25 min of agent time including three CI runs and a Linux container check.
- **Tasks:** 2 of 2, 3 commits (one docs, two fixes found by the CI work).
- **Diff:** 3460 bytes, so `actuals.tokens` is 865 on the chars/4 scale (estimate 45000; the plan was mostly measurement, not text).

## Accomplishments

- **PROJECT.md.** Key Decisions now has: the O8 row (C17 with `CMAKE_C_EXTENSIONS OFF` and `-pedantic-errors`, CMake 3.25+ workflow presets, Ninja, CTest, C ABI with opaque handle; "Owner accepted research default (O8), 2026-11-09"; Owner decision), the repository-visibility row (public at github.com/szTheory/lynxplosion; no PII or credentials; Owner decision), the licence row (O5 and release route; Open, all rights reserved until decided), the O6 row (draft policy, owner decision pending) and the Phase 1 advance unit (`lynx_advance` budget in 16 MHz master ticks, per-device timing model open). Other rows are unchanged. The Out of Scope line about repository visibility change was left as it was (outside the scoped edit; see Notes for the verifier).
- **README.** A "Documents" section links `docs/policy/clean-room.md`, `docs/policy/exposure-record.md`, `docs/provenance.md` and `docs/claims/claim.template` and says a claim records the SHA-256 of `tests/inventory/fast.txt`. "All rights reserved" and the no-BIOS statement are unchanged. No LICENSE, LICENSE.md or COPYING exists.
- **Push.** `hygiene.sh --tree` and `--history` and `scripts/check.sh` passed before the push; all 15 unpushed commits (plans 01-09 to 01-12 and this plan) carried the `szTheory@users.noreply.github.com` author and committer.

## Task Commits

1. **Task 1: PROJECT.md Key Decisions and README document links** - `925a04b` (docs)
2. **Task 2: Final push, CI evidence and efficiency review** - `79f877d` (fix, shellcheck 0.9.0) and `0f5ee30` (perf, parallel ctest)

## CI evidence (FND-01, FND-09)

CI runs for this plan (workflow `CI`, branch main, selected by `headSha`):

| Run | headSha | Conclusion | Note |
|---|---|---|---|
| 42725418399 | 925a04b | failure | first CI for plans 01-09 to 01-13 task 1: shellcheck 0.9.0 SC2317 in `scripts/hygiene.sh` (see Deviations) |
| 42725820300 | 79f877d | success | after the shellcheck fix; baseline for the efficiency review |
| 42726754326 | 0f5ee30 | success | after the ctest parallelism change; final code commit |

Run URL for the final code commit: https://github.com/szTheory/lynxplosion/actions/runs/42726754326

**Final run 42726754326** (0f5ee30): job `check` 29 s (2026-11-10 00:14:24Z to 00:14:53Z); in-script `check: total 26s`. Steps: identities 0 s, pins 2 s, cc65 11 s, probe 1 s, shellcheck 1 s, workflow 7 s, junit 0 s, artifacts 1 s, history 3 s.

**Baseline run 42725820300** (79f877d): job 39 s (00:07:14Z to 00:07:53Z); `check: total 34s`. Steps: identities 0 s, pins 2 s, cc65 16 s, probe 0 s, shellcheck 2 s, workflow 10 s, junit 0 s, artifacts 2 s, history 2 s.

**Runner identities** (both runs): `cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`, `cmake version 3.31.6`, ninja 1.13.2, shellcheck 0.9.0.

**cc65 on the runner.** Built by gcc 13.3.0: `make bin` 11 s (16 s in the baseline run, 7 s in the failed run; the same recipe and the same machine class, so runner speed varies by a factor of two), `make lib TARGETS=lynx` 0 s, `cc65: built in 11s`, build list 1699 files (same as the dev host).

Tool hashes on the runner (gcc, Linux x86_64) against the dev host (Apple clang, macOS arm64):

| Tool | Runner (ubuntu-24.04) | Dev host |
|---|---|---|
| cc65 | `eea9dcf2a749c58e169598405d00e3385835f7a2fd07794b622fd776cebfa829` | `7d634dc0...` |
| ca65 | `62ce4865cd0fe0a2a29f1357633c3ba126955957381a6246fbb5ff2115196389` | `aae89068...` |
| ld65 | `b31f0421d6e433c14544f6267961116313e2f69949a841d2b5ae7f4d1c6b47ae` | `e7493875...` |
| cl65 | `565aebc15f2d260631efe53f9ac4521ad101280d978104291bcb0ee864d8a4c9` | `f4cf8792...` |

The tool binaries differ, as expected: they are native executables built by different compilers for different CPUs, and the build-id pin (`BUILD_ID`) only removes the repository-revision dependence, not the host dependence. The project claim is not that the toolchain binaries are identical, it is that the guest output is. The linked probe hash is: `probe sha256 = e2fdd9812e79c7ba2a7c3b6f1328aedf61bf358b54aab7f35ae6fb45117804af`, `probe size = 594`, equal to the dev host and to the `sha256 =` line of `fixtures/manifests/cc65-probe.manifest`. So the guest probe is bit-identical across a macOS clang build and an Ubuntu gcc build of cc65. A container run (ubuntu:24.04 arm64, gcc) before the push printed a third set of tool hashes (`cc65 eaef14d3...`, `ca65 700b34b6...`, `ld65 ec4b2fa0...`, `cl65 bafb591d...`) and the same probe hash again.

`det.golden`, `toolchain.probe` and the other 53 tests passed on gcc: `100% tests passed, 0 tests failed out of 55` in the runner log.

**Scan lines from the runner (RESEARCH A9):**

- `scan-binaries: artifacts: 6332 files, 1 of exactly 512 bytes` (both CI runs; dev host: `6396 files, 1 of exactly 512 bytes`, difference is the host's extra build outputs).
- The single exactly-512-byte file is text (the scan passes); the source scan summary is printed by `hygiene.firmware_scan` inside ctest and passed.

## CI efficiency review (AUTONOMY.md)

**Critical path** (final run, 26 s in-script of 29 s job): cc65 build 11 s (42 percent of the script, about 38 percent of the job), workflow 7 s, history 3 s, pins 2 s, then 1 s steps. Runner set-up, checkout and post steps add about 3 s. Earlier runs for reference: plan 01-01 and 01-08 jobs were 9 to 16 s because the gate was smaller (36 tests, no cc65 build).

**Duplicated work checked:**
- The artifacts scan (build tree, 2 s) and the source scan inside ctest (`hygiene.firmware_scan`, 0.4 s) read different zones; neither proves what the other does. Kept.
- The `history` step re-reads all commits for home paths and identities; the `hygiene.tree` test reads the current tree. Different failure class. Kept.
- `pins` re-fetches the two archives on every run (2 s, hash-verified before unpack). Nothing to remove.
- The self-tests are the slowest tests (`hygiene.firmware_scan.self_test` 2.6 s, `hygiene.tree` 0.9 s, `api.snapshot_refusal` 0.9 s, `hygiene.pii.self_test` 0.8 s, `api.advance_budget` 0.5 s on the runner). Each proves a distinct seeded failure; none removed.

**Changes made:**
1. **ctest runs with 4 parallel jobs** (`"execution": {"noTestsAction": "error", "jobs": 4}` in the `check` test preset). Proof of safety: every test writes under its own `${LYNX_SCRATCH_DIR}/<area>/<name>` directory (the 16 path uses in `tests/**` and `cmake/` are all distinct) or a per-process `mktemp -d`, and `scripts/hygiene.sh` writes a pid-named file; eight consecutive parallel runs (five at `-j4`, three at `-j16`) were green with a clean working tree; the junit/inventory gate still saw all 55 tests. Effect: workflow step 10 s to 7 s on the runner (job 39 s to 29 s, of which about 5 s is cc65 build noise, so the honest measured saving is about 3 s per run, 8 percent of job time). Dev host: workflow step 39 s to 15 s on one comparable pair (the dev host is noisy; serial ctest ranged 44 to 81 s across today's runs).
2. **Shellcheck SC2317 fix** (not an efficiency change; it removed a failing run). See Deviations.

**Measured candidates not applied:**
- **`actions/cache` for `build/pins/cc65`.** The plan's rule needs the cc65 step to exceed both 25 percent of job time and 30 s. It exceeds the first (38 to 41 percent) but takes 11 to 16 s, not more than 30 s. A cache restore of about 6000 files plus its post-job save would claim a good part of the saving, and the runner variance (7 to 16 s for the same build) is as large as the possible gain. Revisit if the cc65 step passes 30 s or the cc65 pin grows. If it is added: pin `actions/cache@55cc8345863c7cc4c66a329aec7e433d2d1c52a9 # v6.1.0`, key `cc65-ubuntu-24.04-<sha256 of the cc65 pins row>-recipe1`, save only on push to main, and keep the `.lynx-pin-files`, `.lynx-build-files` and `.lynx-build` agreement check so the probe and the artifacts scan still run.
- **Lower job count for cc65 or a prebuilt tool archive.** Rejected: a prebuilt archive would remove the build-from-source proof FND-09 relies on.
- **Concurrency.** The workflow already cancels in-progress runs only for pull requests and keys main runs by commit, so no change.

No gate, control or step was removed or weakened. The workflow still runs only `scripts/check.sh` and has no second runner or lane. `.github/workflows/ci.yml` was not changed, so its single `uses:` line is unchanged and SHA-pinned.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] shellcheck 0.9.0 (CI) reports SC2317 where 0.11 (dev host) reports SC2329**
- **Found during:** Task 2 (first CI run 42725418399)
- **Issue:** `scripts/hygiene.sh` line 106 (`tree_show`, called indirectly as an argument to `check_files`) had `# shellcheck disable=SC2329`, which silences only the newer shellcheck's code. CI's 0.9.0 reported SC2317 (info) on four spans and exited 1, failing the `shellcheck` step; the rest of the gate had passed up to that point, including the cc65 build and the probe hash.
- **Fix:** the directive now reads `# shellcheck disable=SC2317,SC2329`. A shellcheck 0.9.0 run over `scripts/*.sh` and `tests/*/*.sh` in an ubuntu:24.04 container was clean, and a full `scripts/check.sh` in the container passed before the re-push.
- **Files modified:** `scripts/hygiene.sh`
- **Commit:** `79f877d`

**2. [Rule 3 - Scope] CMakePresets.json was not in the plan's files_modified**
- **Found during:** Task 2 (efficiency review)
- **Issue:** the plan's file list names `.github/workflows/ci.yml` and `scripts/check.sh` as the places an efficiency change could land; test-level parallelism is a test preset setting, which is where the AUTONOMY directive and the plan's lead-in place it ("`execution.jobs` in the workflow preset").
- **Fix:** one line in `CMakePresets.json` (`"jobs": 4`). `ci.yml` and `check.sh` are unchanged.
- **Files modified:** `CMakePresets.json`
- **Commit:** `0f5ee30`

**Total deviations:** 2 (one bug found by the first CI run for the unpushed plans, one file outside the plan's list for the efficiency change). **Impact:** none on scope; no gate weakened.

## Issues Encountered

- First CI run for the plans pushed together (01-09 to 01-13 task 1) failed on the shellcheck version difference above, as the directive predicted for new Linux-only differences. Everything else (cc65 build on gcc, BSD/GNU tool differences in the hygiene and scan scripts, `shasum` versus `sha256sum`, `du` and time differences) passed on the first Linux run, which the later container run confirmed.
- The dev host is noisy: the same serial ctest took 44 to 81 s across the day. The runner numbers are the reliable ones.

## Owner follow-ups (not acted on)

- Enable a required-check ruleset on `main` for the `CI / check` job (RESEARCH open question 4).
- Consider the `sha_pinning_required` Actions setting (repository configuration; the one `uses:` line is already SHA-pinned).
- O5 (licence), O6 (clean-room protocol), O7 and O1 stay open; nothing here decided them.

## Notes for the verifier

- The final CI run recorded above is for the last code commit `0f5ee30`. The SUMMARY and STATE commit that follows changes only `.planning/` files; its own CI run cannot be recorded in this file, and the hand-off report from the executor gives its result.
- `.planning/PROJECT.md` Out of Scope still lists "Public publication, release or repository visibility change" as an owner boundary. The new Key Decisions row records that the owner made the repository public on 2026-11-09; the plan limited the PROJECT.md edit to Key Decisions rows, so the Out of Scope wording was left for the phase transition update.
- `tests/inventory/fast.txt` was not changed, so the claim binding in `docs/claims/skeleton-repeatability.claim` still holds (`docs.claim_template` passes).
- The `STATE.md` blocker/decision lists record the efficiency decisions above.

## Known Stubs

None.

## Threat Flags

None. No cache was added (T-01-26 not triggered); the pushed history passed `hygiene.sh --tree` and `--history` before the push (T-01-27), CI uses no secrets and a read-only token.

## Self-Check: PASSED

- `.planning/PROJECT.md`, `README.md`, `scripts/hygiene.sh`, `CMakePresets.json` exist and are modified by commits `925a04b`, `79f877d`, `0f5ee30`, all ancestors of HEAD.
- `git rev-list --count ab573c7..HEAD` gave 3 before this summary commit; `sh scripts/check.sh` exited 0 on the final tree; CI run 42726754326 concluded success for headSha `0f5ee304`.
