---
phase: 01-evidence-harness-and-walking-skeleton
plan: 10
subsystem: fixtures-and-toolchain
tags: [manifests, fixtures, cc65, probe, reproducibility, build-list, shell, cmake]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-09: pins.txt, fetch-pin.sh with .lynx-pin and .lynx-pin-files, the check.sh pins step; plan 01-08: validate-kv.sh; plan 01-04: lynx_add_test, expect_fail.cmake, the inventory"
provides:
  - "scripts/check-manifests.sh [--root DIR]: strict manifest key validation, duplicate-sha256 rejection, full fixture coverage"
  - "fixtures/manifests/skeleton-image.manifest and cc65-probe.manifest (evidence_tier R, startup_tier A, profile none)"
  - "check.sh cc65 step: pristine check, two sequential make runs, cache reuse only when archive list, build list and stamp agree"
  - "check.sh probe step: cl65 probe under env -i, tool hashes, probe hash, build list then stamp written last"
  - "build/pins/cc65/.lynx-build-files (1699 files) and .lynx-build stamp for the plan 01-11 rights scan"
  - "hygiene.manifests, hygiene.manifests.self_test, toolchain.probe, toolchain.probe.self_test (inventory now 45 tests)"
affects: [01-11, 01-12, 01-13]

actuals:
  tokens: 6625
  tasks: 2
  commits: 2

plan_head_before: bc60a1b10a0a9a6061ac1559b91413576c4eb01b
plan_head_after: 9788e836308814475d3700fe30b3b3e5257d2a1a

tech-stack:
  added: []
  patterns:
    - "Findings are collected in a temp file and the exit status comes from its line count (Pitfall 6), sorted so output is stable"
    - "A CMake -P check prints long findings with message(NOTICE) and then fails with a short FATAL_ERROR, because FATAL_ERROR wraps long lines and breaks regex matching in expect_fail.cmake"
    - "A build whose cache is trusted is described by three records that must agree: archive list, build list, stamp (written last)"

key-files:
  created:
    - scripts/check-manifests.sh
    - tests/gates/manifests_controls.sh
    - fixtures/manifests/skeleton-image.manifest
    - fixtures/manifests/cc65-probe.manifest
    - fixtures/toolchain/probe.c
    - tests/cmake/toolchain_probe.cmake
  modified:
    - scripts/check.sh
    - tests/gates/supply.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "cc65 is built with BUILD_ID fixed to 'Git <first 7 hex of the pinned commit>' (see deviation 1)"
  - "The .lynx-build stamp is a key = value file: pin_sha256, recipe (1), build_seconds"
  - "The .lynx-build-files format matches .lynx-pin-files: header '# pin_sha256 = <digest>' and '# recipe = 1', then '<sha256> <path>' rows sorted by path in LC_ALL=C order; plan 01-11 must skip lines starting with '#'"
  - "Cache hit also requires the four tools and lib/lynx.lib to exist and the .lynx-pin-files header to be the stricter agreement the plan names; anything else rebuilds"

patterns-established:
  - "Pattern: check.sh functions share state through shell variables (CC65_FRESH, CC65_BUILD_SECONDS), because step() runs them in the same shell, so the probe step knows whether to finalize the build list"

requirements-completed: [FND-03, FND-09]

coverage:
  - id: F1
    description: "Every manifest has the ten required keys with valid values; every file under fixtures/ other than manifests is named by some manifest's files key; the real tree passes"
    requirement: FND-03
    verification:
      - kind: integration
        ref: "ctest hygiene.manifests (2 manifests, 2 fixture files covered); sh scripts/check-manifests.sh exits 0"
        status: pass
    human_judgment: false
  - id: F2
    description: "Duplicate sha256 across manifests is rejected (adjacency); a duplicated key in one manifest is rejected"
    requirement: FND-03
    verification:
      - kind: integration
        ref: "ctest hygiene.manifests.self_test (cases duplicate_sha256, duplicate_key)"
        status: pass
    human_judgment: false
  - id: F3
    description: "An empty value, a missing key, an unknown key, a 63-digit sha256, an out-of-vocabulary evidence tier, a missing manifest set, an uncovered fixture and a files entry that is missing or escapes fixtures/ are each rejected with the stated wording (empty edge)"
    requirement: FND-03
    verification:
      - kind: integration
        ref: "ctest hygiene.manifests.self_test (cases missing_key, unknown_key, empty_value, bad_sha256, bad_evidence_tier, no_manifests, uncovered_fixture, files_entry_missing, files_entry_escape)"
        status: pass
    human_judgment: false
  - id: F4
    description: "Key order inside a file does not change the verdict, and findings come out in sorted manifest order (ordering edge)"
    requirement: FND-03
    verification:
      - kind: integration
        ref: "ctest hygiene.manifests.self_test (cases key_order, sorted_findings)"
        status: pass
    human_judgment: false
  - id: F5
    description: "cc65 builds from the pinned source in two sequential make runs; check.sh prints the wall time (or the stored time on a cache hit), the SHA-256 of cc65, ca65, ld65 and cl65, and the probe .lnx SHA-256 and size"
    requirement: FND-09
    verification:
      - kind: integration
        ref: "sh scripts/check.sh cold: 'cc65: make bin 2s', 'cc65: make lib TARGETS=lynx 1s', 'cc65: built in 3s', four 'cc65 tool sha256' lines, 'probe sha256 = e2fdd981...7804af', 'probe size = 594'; cached run: 'cc65: cached build (built in 3s)'"
        status: pass
    human_judgment: false
  - id: F6
    description: "After both make runs and the probe succeed, .lynx-build-files is written (header pin_sha256 and recipe, then sha256 path rows for every added or changed file) and only then the .lynx-build stamp"
    requirement: FND-09
    verification:
      - kind: integration
        ref: "head -n 2 build/pins/cc65/.lynx-build-files shows the two header lines; ' bin/cl65' and ' lib/lynx.lib' rows present; 1699 rows; stamp holds pin_sha256, recipe = 1, build_seconds"
        status: pass
    human_judgment: false
  - id: F7
    description: "A cached build is reused only when archive list, build list and stamp agree; otherwise the extraction is checked for pristineness and, when anything extra or changed is present, deleted and fetched again"
    requirement: FND-09
    verification:
      - kind: integration
        ref: "by hand: .lynx-build-files moved away, sh scripts/check.sh printed 'the extraction is not pristine against its archive list; fetching it again', re-fetched (4288 files), printed 'cc65: built in 2s' (not 'cached build') and re-created .lynx-build-files"
        status: pass
    human_judgment: false
  - id: F8
    description: "toolchain.probe passes only when the probe .lnx has the SHA-256 in cc65-probe.manifest; a wrong expected hash fails"
    requirement: FND-09
    verification:
      - kind: integration
        ref: "ctest toolchain.probe (matches); ctest toolchain.probe.self_test (zeroed manifest copy, exit 1, 'differs from')"
        status: pass
    human_judgment: false

duration: 45min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 10: Fixture manifests and the cc65 probe Summary

**Every fixture now has a validated manifest (identity, source, licence, allowed use), and check.sh builds the pinned cc65 in two sequential make runs, prints tool hashes and a 594-byte linked probe whose SHA-256 CTest compares with its manifest, and records every file the build added so the plan 01-11 rights scan can accept them.**

## Performance

- **Duration:** about 45 min of agent time (start timestamp not captured separately).
- **Tasks:** 2 of 2, 2 commits.
- **Diff:** 26501 characters, so `actuals.tokens` is 6625 on the chars/4 scale (estimate 60000; over-estimated by about 9x, in line with plans 01-08 and 01-09).

### FND-09 measured evidence (dev host: macOS arm64, 18 cores, Apple clang, GNU make 3.81, `-j18`)

| Run | `make bin` | `make lib TARGETS=lynx` | Total | Notes |
|---|---|---|---|---|
| Cold (fresh extraction) | 2 s | 1 s | 3 s | `check.sh` total 14 s incl. fetch and CTest |
| Cold after the cache-agreement test (re-fetch, rebuild) | 2 s | 0 s | 2 s | `check.sh` total 12 s |
| Cached | not run | not run | prints stored `built in 3s` | `cc65` step 0 s, `check.sh` total 7 s |

Whole-second granularity (`date +%s`). The research figures were 4.1 s and 0.4 s with `-j8`; this host is faster. The CI number (4 vCPU) comes from plan 01-13.

Tool and probe hashes (identical across all three cold builds in this plan, which also confirms reproducibility after the BUILD_ID fix):

| Item | SHA-256 |
|---|---|
| cc65 | `7d634dc0066e6d4a8a89b4de3f37d15d00f3efd2f79dd250a5571f1881b8d644` |
| ca65 | `aae8906856e0a07e3eb138ed83fd52c1f20ae0b21c35000ce774c896f1785967` |
| ld65 | `e74938752aabfe22609d7cf3009a525630f1d4dc279a53bad569360b42b3ffd6` |
| cl65 | `f4cf8792bc25a278e7efd580822ec487d244d4790580ad88eb26faa3adaf6978` |
| probe.lnx (594 bytes) | `e2fdd9812e79c7ba2a7c3b6f1328aedf61bf358b54aab7f35ae6fb45117804af` |

The probe hash equals the value RESEARCH measured for its scratch probe (same program, comment line aside) on both macOS and Linux, so CI should reproduce it. The tool hashes are the ones the CI run should be compared against (plan 01-13), and they depend on the BUILD_ID fix below.

Build list: 1699 files, written in 0 to 1 s (hash of the whole extraction plus build output with one bulk `find -exec`).

## Accomplishments

- **check-manifests.sh:** validates each `fixtures/manifests/*.manifest` with `validate-kv.sh` (required: ten keys; optional: `files independence`), rejects a repeated sha256 (`duplicate sha256 in <a> and <b>`), rejects `files` entries that are missing, absolute, contain `..` or point outside `fixtures/`, and reports every fixture file not named by a manifest. Manifests are visited in sorted order and findings are sorted.
- **manifests_controls.sh:** 14 cases (pass, key_order, missing_key, unknown_key, duplicate_key, empty_value, bad_sha256, bad_evidence_tier, duplicate_sha256, uncovered_fixture, files_entry_missing, files_entry_escape, no_manifests, sorted_findings), all trees built under `${LYNX_SCRATCH_DIR}/gates/manifests` from text files.
- **check.sh cc65 and probe steps:** as the plan specifies, with `build/pins/cc65` literal in both `make -C` lines and no line holding both goals. Pristine check hashes the whole extraction and requires it to equal the archive list exactly (nothing extra, changed or missing). The probe is compiled in `build/toolchain/` with `env -i`, `probe.o` and `probe.s` are removed, and a size of exactly 512 bytes fails the step (it is 594).
- **toolchain.probe:** reads the single `sha256 = ` line of the manifest and compares it with `file(SHA256)` of `build/toolchain/probe.lnx`.

## Task Commits

1. **Task 1: Fixture manifest validation, fixture coverage and the skeleton fixture manifest** - `d8afe97` (feat)
2. **Task 2: cc65 two-step build with wall time, tool hashes and the reproducible probe** - `9788e83` (feat)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] cc65 tool bytes embedded this repository's commit hash**
- **Found during:** Task 2, first cold build (the make log showed `-DBUILD_ID="\"Git d8afe97\""`).
- **Issue:** `build/pins/cc65/src/Makefile` sets `BUILD_ID := Git $(shell git rev-parse --short HEAD ...)` when `BUILD_ID` is undefined. `build/pins/cc65` sits inside this git repository, so it picked up the lynxplosion HEAD. The tool binaries would then differ on every commit of this repository and between this host and CI, defeating the tool-hash evidence (the research measured identical tools only because its builds were outside any git repository).
- **Fix:** `make -C build/pins/cc65 -j"$jobs" BUILD_ID="Git <first 7 hex of the pin commit from pins.txt>" bin`. `lib` does not compile the tools, so it needs no BUILD_ID. Verified by three cold builds giving identical tool hashes.
- **Files modified:** `scripts/check.sh`
- **Commit:** `9788e83`

**2. [Rule 1 - Bug] CMake FATAL_ERROR wraps long messages, breaking `differs from` matching**
- **Found during:** Task 2, first workflow run (`toolchain.probe.self_test` failed because the wrapped message split `differs from` over two lines).
- **Fix:** `toolchain_probe.cmake` prints the full finding with `message(NOTICE ...)` and then fails with a short `FATAL_ERROR "toolchain.probe: failed"`. Wording of the finding is unchanged.
- **Files modified:** `tests/cmake/toolchain_probe.cmake`
- **Commit:** `9788e83`

**3. [Rule 2 - Missing critical functionality] Extra safeguards beyond the plan text**
- `check-manifests.sh` rejects `files` entries that are absolute, contain `..` or name a manifest or anything outside `fixtures/` (a manifest could otherwise "cover" a path outside the fixture tree), and reports `no manifests` when none exist. Controls: `files_entry_escape`, `no_manifests`.
- `check.sh` verifies the hashed file count equals the `find` count (a failed hash would otherwise yield a short list, and a short pristine or build list weakens the rights scan), and requires the four tools and `lib/lynx.lib` to exist before treating a build as cached.
- **Commit:** `d8afe97`, `9788e83`

### Clarifications (not deviations)

- The plan's verify text expects "100% tests passed, 0 tests failed out of N"; local CTest prints "100% tests passed out of N". Exit status and JUnit were used (as in plans 01-08 and 01-09).
- The insertion-point comment in `check.sh` above the pins step was reworded; the artifacts and history insertion point after `junit` is unchanged.
- `CC65_HOME` is exported by the cc65 and probe steps and so stays set in the environment of the later check.sh steps; nothing there reads it.
- The `.lynx-build-files` list is 1699 files because it includes the 12 tools, `lib/lynx.lib`, `wrk/` and `libwrk/` objects and dependency files; they are all recipe output and all will be accepted by the plan 01-11 scan only by path and hash match.

**Total deviations:** 2 auto-fixed bugs (Rule 1), 1 auto-added (Rule 2). **Impact:** tool-hash evidence is now reproducible across commits and hosts; no scope change.

## Issues Encountered

- A first `sh scripts/check.sh` piped to `head` was cut short by SIGPIPE; `build/pins/cc65` was deleted and rebuilt cleanly afterwards, so no measurement here comes from that run.

## Known Stubs

None.

## Threat Flags

None new. T-01-35 (build list): written only on a cache miss, after both make runs and the probe, from an extraction first proven pristine against the archive list; the cache is reused only when archive list, build list and stamp agree; the cache-agreement test above shows a missing build list forces a re-fetch and rebuild. T-01-33: manifests are hash-keyed, duplicate identities rejected, coverage complete, each with a control. T-01-SC: no package-manager installs; cc65 built only from the SHA-256-verified pin.

## Notes for the next plan

- `build/pins/cc65/.lynx-build-files` format: `# pin_sha256 = <digest>`, `# recipe = 1`, then `<64 hex> <path relative to build/pins/cc65>` rows sorted by path (LC_ALL=C); split at the first space only. The rights scan must accept a pin-zone file only if its path and hash appear on `.lynx-pin-files` or this list, skip lines starting with `#`, and exclude the four metadata files `.lynx-pin`, `.lynx-pin-files`, `.lynx-build-files`, `.lynx-build` (they are not listed). A 512-byte file inside the zone must still fail (the exact-512 rule has no exemption).
- `build/toolchain/` holds `probe.c` (copy), `probe.lnx` (594 bytes) and `probe.sym` (text). The scan should accept `probe.lnx` by its SHA-256 matching `sha256 = ` in `fixtures/manifests/cc65-probe.manifest`; the other two are text.
- `fixtures/manifests/*.manifest` identity is path-independent (`sha256`); the skeleton manifest hashes `skeleton-image.hex` (`8b4da72c...88d8e`). Plan 01-11 can derive the manifest hash set with the same `key = value` lookup `check-manifests.sh` uses.
- Any new fixture file needs a manifest `files` entry in the same commit, or `hygiene.manifests` fails. Inventory is now 45 tests; to add a gate add one `tests/gates/*.cmake` registration plus sorted rows in `tests/inventory/fast.txt`.
- The cc65 step runs third-party Makefiles from the hash-verified archive. On CI expect a cold build every run (no `build/` cache): about 3 s on this host, roughly 5 to 10 s on 4 vCPU; plan 01-13 records the real number.
- The probe `.lnx` is 594 bytes, not 512, so the 512-byte rule does not apply to it.

## Self-Check: PASSED

All 6 created files exist (`scripts/check-manifests.sh`, `tests/gates/manifests_controls.sh`, both manifests, `fixtures/toolchain/probe.c`, `tests/cmake/toolchain_probe.cmake`); commits `d8afe97` and `9788e83` are ancestors of HEAD; `git rev-list --count bc60a1b..HEAD` gave 2 before this summary commit; `sh scripts/check.sh` exits 0 (cold, cached and after the cache-agreement test); `shellcheck -s sh scripts/check.sh scripts/check-manifests.sh tests/gates/manifests_controls.sh` exits 0; `git status --porcelain fixtures/` is clean after the check.
