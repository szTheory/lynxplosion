---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 11
subsystem: docs-full-lane-and-ci-evidence
tags: [cpu-profiles-doc, readme, full-vector-lane, profile-diff-spike, linux-container, ci-evidence, ci-efficiency-review, cmake-policy]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-01..02-10: profile-driven CPU, vector runner and inventories, scenarios, fixtures, machine switch, public-path tests"
provides:
  - "docs/cpu-profiles.md: profile ids and names, the none rule, the 32 profile-dependent opcodes, the CB/DB rule, declared power-on values, the flat5 PLACEHOLDER cost model, vector evidence, source tags and the full unverified table"
  - "README Profiles section (none default; lynx1 and lynx2 explicit; no baseline, O1 open; ticks are not timing evidence)"
  - "Recorded full-lane results for 5.12 million pinned vectors plus the profile-diff spike answer"
  - "CI evidence for the pushed head and the Phase 2 CI efficiency review"
affects: [phase-2-verification]

actuals:
  tokens: 9000
  tasks: 3
  commits: 3

plan_head_before: 8c9d1a27bf0f617f82c47e5a7ccc12ce8c7c35ae
plan_head_after: e8523539ed236eba1ae0a41f39f044db12c43d4a

tech-stack:
  added: []
  patterns:
    - "One per-profile test per long gate (api.seam_agreement.none, .lynx1, .lynx2) with an explicit ctest COST so the longest starts first"

key-files:
  created:
    - docs/cpu-profiles.md
  modified:
    - README.md
    - .planning/phases/01-evidence-harness-and-walking-skeleton/SKELETON.md
    - tests/cmake/docs_policy.cmake
    - tests/cmake/expect_fail.cmake
    - tests/cmake/inventory_controls.cmake
    - tests/cmake/no_skip.cmake
    - tests/cmake/pins_controls.cmake
    - tests/cmake/provenance.cmake
    - tests/det/golden.cmake
    - tests/det/golden_self_test.cmake
    - tests/det/runner_cli.cmake
    - tests/det/two_runs.cmake
    - tests/cpu/seam_agreement.cmake
    - tests/cpu/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "No CI trigger for the full vector lane: it takes 5 min 47 s wall, 3.5 min of it downloading 2 GB. Run it by hand at phase verification, on every pin bump and before any claim"
  - "api.seam_agreement split into three per-profile tests (every proof kept); no change to .github/workflows/ci.yml"

patterns-established:
  - "Every -P CMake test script sets cmake_minimum_required(VERSION 3.25) and cmake_policy(VERSION 3.25)"

requirements-completed: [CPU-01, CPU-02, CPU-03, CPU-04, CPU-05]

duration: about 1 h 30 min of agent time (full lane 5 min 47 s, two container runs about 80 s each, two CI runs about 1 min each)
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 11: Docs, full vector lane, container check, push and CI evidence Summary

The profile document and README section are written, the full vector lane passed on all 5.12 million pinned vectors (the two cross pairings fail on exactly the 32 bit-op opcodes), the Phase 2 work is pushed and CI is green on the head commit, and the CI efficiency review is recorded. No profile is adopted as baseline (O1 open) and none of this is a profile-naming claim or a suite-pass claim.

## What was built

- **Ten `-P` scripts pinned to CMake policy 3.25** (commit `2bd5d7f`): `tests/cmake/{docs_policy,expect_fail,inventory_controls,no_skip,pins_controls,provenance}.cmake` and `tests/det/{golden,golden_self_test,runner_cli,two_runs}.cmake` each gained `cmake_minimum_required(VERSION 3.25)` and `cmake_policy(VERSION 3.25)`. This is the list 02-09 reported. Full check green locally before the commit.
- **`docs/cpu-profiles.md`, README "Profiles", SKELETON.md pointer** (commit `ad8c4fd`). The document has 19 uses of "unverified", states O1 (no baseline), that ticks are not timing evidence and that repeatability digests are not accuracy, and lists the whole CONTEXT "Source honesty" and RESEARCH 4.4 unverified set with source tags (LT-HW, WDC, RKW, CMD, SST-MODEL, INFERRED). `docs.*` and `hygiene.*` tests passed (14 of 14).
- **`api.seam_agreement` split** (commit `e852353`, see the CI efficiency review).

## Full vector lane (CPU-01, CPU-05; throwaway verification, nothing committed)

Command: `sh scripts/vectors-full.sh pin sample run` in one invocation, one work directory under the session scratch directory (outside the repository, removed by the script's trap). Start 2026-11-10 06:24:03Z at commit `ad8c4fd` (`scripts/vectors-full.sh` unchanged; no script defect found). Pinned source: SingleStepTests/65x02 commit `2f6980a2d95757486c7bee24355c360e40e2a224`.

- **Wall time:** 5 min 46.8 s in total (`130 s user, 30 s system`). Download of 512 files, 2,006,647,698 bytes: 210 s. The five runner invocations: 24, 24, 22, 24 and 25 s.
- **Downloads:** all 512 files matched `fixtures/vectors/sst65x02/upstream.sha256`. The `pin` mode re-derived the pin record and LICENSE, and the `sample` mode re-derived both sample containers: `git status` afterwards showed nothing changed under `fixtures/` (the record, the LICENSE and both samples were reproduced byte for byte). Sample SHA-256: synertek `2fabae3f5acbfe44f9782721ed1cb96a5be6de4575038cc3cab3d309f2a7164b` (3,201,546 bytes), rockwell `1679d8fe0dae742e93dff97d594d7b8cec7468b1d7a43240d1c52f5e9b53f921` (3,230,210 bytes). `run` reported "both samples equal the committed ones byte for byte".
- **Same-profile runs (all must pass; all did, exit 0):**

| Run | Data | Files | run | excluded | undefined | Vectors | Asserted | Passed | Failed | Results SHA-256 |
|---|---|---|---|---|---|---|---|---|---|---|
| none | synertek | 256 | 222 | 2 | 32 | 2,560,000 | 2,220,000 | 2,220,000 | 0 | `418bcc4c13e63b453168075eb9d1641910a0ed22a220b26bd6e35a4f50231411` |
| lynx1 | synertek | 256 | 254 | 2 | 0 | 2,560,000 | 2,540,000 | 2,540,000 | 0 | `683df9d9d411b438ca36aff7fe0493d54b7aa7e70d370ae4a96db78e9ea617ca` |
| lynx2 | rockwell | 256 | 254 | 2 | 0 | 2,560,000 | 2,540,000 | 2,540,000 | 0 | `683df9d9d411b438ca36aff7fe0493d54b7aa7e70d370ae4a96db78e9ea617ca` |

Excluded: 20,000 vectors parsed and not asserted per run (`$CB` and `$DB`, 10,000 each). Under none, 320,000 vectors of the 32 undefined rows were all confirmed to stop UNDEFINED (`undefined_vectors 320000 stopped 320000`). The lynx1 and lynx2 results files are identical because the file lists opcode, passed and failed per run row and every row is `10000 0` in both; the files are not copies of each other (the runner output carries different `identity` counters, 960972 for lynx1 and 881039 for lynx2). `vectors 2560000` per run is 256 files of 10,000.

- **Cross pairings (expected to fail, exit 1):**

| Run | Failed vectors | Failing opcodes | Results SHA-256 |
|---|---|---|---|
| rockwell data under lynx1 | 159,383 of 2,540,000 | 32: `07 0f 17 1f 27 2f 37 3f 47 4f 57 5f 67 6f 77 7f 87 8f 97 9f a7 af b7 bf c7 cf d7 df e7 ef f7 ff` | `bb57bd0c990efbd1cc7aa333e6f5fd339d0b062663564653d1a6b75323020164` |
| synertek data under lynx2 | 160,136 of 2,540,000 | the same 32 | `954ba4d3593e24f345aae7d2426ac389a2b3fe00b89505df0774798ddab231ee` |

Beyond the 32 bit-op opcodes: none. Bit-op opcodes not failing: none.

- **Profile-diff spike answer.** On the full pinned data (10,000 vectors per opcode, not the 32 sampled) synertek and rockwell differ in the 32 `$x7/$xF` opcodes and nowhere else that this setup can see. Caveat, as CONTEXT states it: the two same-profile runs pass on all vectors and the 224 shared rows of the profile table are identical, so a cross pairing could only fail the 32 rows where the tables differ. The cross pairing therefore shows that the CPU and the data agree on the shared rows; it does not independently compare the two data sets on the shared opcodes beyond what each passes under the same shared rows. It also says nothing about the Lynx: the data is a software model (tier T1, "Rockwell-style vs synertek-style" are generator variants, not Lynx hardware) and any lynx1 versus lynx2 difference beyond those 32 opcodes stays unverified.
- **CI-trigger decision (RESEARCH OQ9):** no CI trigger. 5 min 47 s wall, of which 3.5 min is downloading 2 GB, is too large for a per-push lane and the data is pinned by hash. Run it by hand at phase verification, on every pin bump and before any claim.
- A genuine CPU defect was not found: no core change and no new scenario were needed.

## Linux evidence (T-02-22; CPU-03 golden)

- **Container check before the push**, ubuntu:24.04 (aarch64, gcc 13, cmake 3.28.3, shellcheck 0.9.0, a fresh clone): at `ad8c4fd` "100% tests passed, 0 tests failed out of 91", `check: total 48s`; at `e852353` "out of 93", `check: total 38s`.
- **CI (x86_64, ubuntu-24.04 runner image 20261101.360.1, gcc 13.3.0, cmake 3.31.6, ninja 1.13.2, shellcheck 0.9.0):** `det.golden` and `det.profile_agreement` passed. `det.golden` compares the observation digest of the churn-loop workload with `tests/golden/skeleton.digest`, which is `506f79956f05bbb58bb2c6e759046836e5b40de0f499bb6af52bcf288e2bad3c` and was produced on Apple clang (arm64) in 02-09. The same digest on Linux gcc is therefore shown by the passing test on three machines: dev host (clang, arm64), container (gcc 13, arm64) and CI (gcc 13.3, x86_64). This is repeatability across compilers, not accuracy.
- **Pre-push hygiene:** `hygiene.sh --tree` and `--history` exit 0 at both pushes; all 40 unpushed commits (the whole of Phase 2 so far) were authored and committed by the noreply identity (`szTheory@users.noreply.github.com`); no home path or credential in the diff.

## CI evidence

Runs selected by `headSha == git rev-parse HEAD` (never `.[0]` without the filter):

| Run | headSha | Conclusion | Job | Note |
|---|---|---|---|---|
| 42769213262 | `ad8c4fd13c24f36a270c541328171eec466c1921` | success | 52 s | all of Phase 2 plus the policy and docs commits; baseline for the review |
| 42769789076 | `e852353` (full SHA in git) | success | 55 s | after the seam_agreement split |

Run URLs: https://github.com/szTheory/lynxplosion/actions/runs/42769213262 and https://github.com/szTheory/lynxplosion/actions/runs/42769789076. The final documentation commit that holds this SUMMARY (and STATE and ROADMAP) is pushed after these runs; its CI run is reported in the plan's return message because a SUMMARY cannot name the run of the commit that contains it.

## CI efficiency review (AUTONOMY.md)

**Phase 1 baseline (plan 01-13):** job 23-29 s, in-script `check: total 26 s` (workflow 7 s, cc65 11 s, history 3 s, pins 2 s, shellcheck 1 s), 55 tests. Local check about 38 s before this plan.

**Before (run 42769213262, 91 tests, 4 parallel ctest jobs):** queue 3 s (created 06:32:37Z, started 06:32:40Z); set up 1 s, checkout 2 s; `check: total 46 s` in a 52 s job.

| Step | Phase 1 | Run 42769213262 | Run 42769789076 | Change |
|---|---|---|---|---|
| identities | 0 s | 0 s | 2 s | none |
| pins | 2 s | 1 s | 1 s | none |
| cc65 | 11 s | 11 s | 12 s | none, still under the 30 s rule |
| probe, cpu-fixtures, junit | 0-1 s | 0 s | 0 s | none |
| shellcheck | 1 s | 3 s | 3 s | none (more scripts in Phase 2) |
| workflow (configure, build, ctest) | 7 s | 23 s | 21 s | seam_agreement split |
| artifacts | 1 s | 1 s | 2 s | none |
| history | 3 s | 7 s | 8 s | none, grows with commit count |
| **script total** | **26 s** | **46 s** | **49 s** | |
| job | 29 s | 52 s | 55 s | |

- **Phase 2 delta:** +20 s of script time. About +16 s is the workflow step (36 more tests; the ctest run is 21.7 s real for 57.5 s of test CPU over 4 jobs), +4 s is history, +2 s shellcheck.
- **Critical path (before):** the single test `api.seam_agreement` ran 17.3 s serial on CI (about 10 s locally) and was the longest by 3.7x (next slowest: `hygiene.firmware_scan.self_test` 4.6 s, `hygiene.pii.self_test` 4.2 s, `api.snapshot_refusal` 3.2 s, `cpu.vectors.no_step` 3.0 s, `cpu.vectors.complete_mode` 2.1 s, `api.advance_slicing` 1.9 s). Its 1,024 undefined-row restores under none (two 64 KiB SHA-256 passes per restore under ASan and UBSan at -O0) are about 80 percent of it.
- **Change made:** `api.seam_agreement` became `api.seam_agreement.none`, `.lynx1` and `.lynx2` (one profile each; the same subcommand and the same assertions; `tests/inventory/fast.txt` rows added in sorted order and the claim `inventory_sha256` refreshed in the same commit, now `58a26333298e3b8dd2f7006740d52e0e52c3c8cecd7f8a48f66e40865e53b8e8`), with `COST 100` on the none test and `COST 50` on the other two so ctest starts them first. Every proof is kept: each profile still checks its run rows against the vector seam and (under none) the 1,024 undefined vectors.
- **After (run 42769789076, 93 tests):** ctest real 21.7 s to 17.6 s (-4.1 s), workflow step 23 s to 21 s. The seam tests started at time zero, but on the 4-vCPU runner the none test took 17.0 s and lynx1 and lynx2 4.0 s each (they ran at 1.7 s locally), so the per-test times show CPU contention; the job total moved 52 s to 55 s because history (+1 s), cc65 (+1 s) and identities (+2 s) moved the other way, which is run-to-run noise of the size seen in Phase 1 (cc65 7-16 s for the same build). The honest measured saving is about 4 s of test wall time (about 8 percent of the job); the job total is within noise. The lower bound for four workers is 57.5/4 = 14.4 s of ctest time, so splitting `none` further (run rows apart from the undefined sweep) could save at most about 3 s more and was not done.
- **Candidates measured and not applied:**
  - cc65 cache: 11-12 s, 22 percent of the job, below the 30 s rule recorded in LESSONS; unchanged reasoning from 01-13.
  - Speeding up each undefined-row restore (for example hashing once): it changes what the test exercises (the restore-with-hash path is the proof), so rejected.
  - History step 7-8 s: it re-reads all commits for home paths and identities. It is a distinct failure class (a leak in an old commit) and the cost is the commit count, about 0.05 s each. Revisit past 15 s; an incremental scan would need a rule that history before the previous green push cannot change, which a force-push breaks.
  - hygiene self-tests (4-6 s each) each prove a distinct seeded failure; kept.
  - Concurrency: main runs are already keyed by commit; no change.
- No second runner or lane was added. `.github/workflows/ci.yml` is unchanged (single SHA-pinned `uses:`). No gate, control or step was removed or weakened.

## Task Commits

1. **Extra item: policy pins for ten `-P` scripts** - `2bd5d7f` (test)
2. **Task 1: profile document, README Profiles, SKELETON pointer** - `ad8c4fd` (docs)
3. **Task 2: full vector lane** - no commit (throwaway verification; nothing tracked changed)
4. **Task 3 efficiency change: seam_agreement split** - `e852353` (perf)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The plan's `<files_modified>` omitted the CMake policy scripts and the seam test files**
- **Found during:** orchestrator extra items 1 and 2.
- **Issue:** the plan lists README, docs, SKELETON, ci.yml and vectors-full.sh; the orchestrator asked for the ten `-P` policy pins and allowed a seam_agreement split.
- **Fix:** as above; the container and CI runs passed, so the pins and the split are safe on CMake 3.28 and 3.31.
- **Files modified:** the ten scripts listed in the frontmatter, `tests/cpu/seam_agreement.cmake`, `tests/cpu/tests.cmake`, `tests/inventory/fast.txt`, `docs/claims/skeleton-repeatability.claim`.
- **Commits:** `2bd5d7f`, `e852353`

### Small choices inside the plan's latitude

- Task 2 ran `pin sample run` together in one invocation (orchestrator instruction and CONTEXT amendment) rather than `run` alone; this additionally re-derived the pin record, the LICENSE and both committed samples from one download, all unchanged.
- The plan's wording "the efficiency table" is the table above; the `.github/workflows/ci.yml` file was not changed.
- The plan header counted 224 shared opcodes with 2 excluded; under none the runs are 222 run, 2 excluded and 32 undefined.

**Total deviations:** 1 (file list), 3 small choices. **Impact:** none on the plan's goal.

## Known Stubs

None.

## Threat Flags

None beyond the plan's threat model.

- **T-02-22:** `hygiene.sh --tree` and `--history`, the 40-commit noreply identity check and the container check ran immediately before each push; no credential or home path was published.
- **T-02-23:** both CI runs were selected by `headSha` equal to `git rev-parse HEAD`.

## Notes for Phase 2 verification

- All four ROADMAP criteria have recorded evidence on the pushed commits: per-opcode vectors per profile (sample in the check, full lane above), profile-dependent behaviour and the none rule (inventories, scenarios, `cpu.profile_divergence`, `api.profile_select`, cross pairings), the bus-cycle seam and non-default cost (`api.cost_model`, `api.advance_slicing`, `api.seam_agreement.*`), repeatability (`det.golden`, `det.profile_agreement`, same digest on three machines). No profile is a baseline; no claim names a profile (`ALLOWED_PROFILES` stays `none`).
- `tests/inventory/fast.txt` has 93 tests. `api.seam_agreement` no longer exists; its replacement rows are `api.seam_agreement.lynx1`, `.lynx2` and `.none`.
- The full lane outputs live in `build/vectors-full/` (ignored by git) and the hashes are above; rerun `sh scripts/vectors-full.sh run` to reproduce (about 6 minutes).
- The pre-existing untracked `.planning/state.json` was not created by this plan and was left alone.

## Self-Check: PASSED

Files docs/cpu-profiles.md, README.md and tests/cpu/seam_agreement.cmake exist; commits 2bd5d7f, ad8c4fd and e852353 are ancestors of HEAD and were pushed to origin main.
