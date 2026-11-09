---
phase: 01-evidence-harness-and-walking-skeleton
plan: 09
subsystem: supply-chain
tags: [pins, sha256, fetch, no-network, cc65, lynx-tests, shell, cmake]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-04: expect_fail.cmake, lynx_add_test, the inventory scripts and fast.txt; plan 01-08: scripts/check.sh insertion points, the cmake_policy(VERSION 3.25) rule for -P scripts"
provides:
  - "pins.txt: cc65 (commit eebf54af...) and lynx-tests 1.4.9 (commit b583dc43...) by commit-SHA archive URL and SHA-256, digests re-verified by download before commit"
  - "scripts/fetch-pin.sh NAME DEST: hash-before-unpack fetch; DEST untouched on mismatch; writes DEST/.lynx-pin-files then the DEST/.lynx-pin stamp; cache reused only with stamp and list"
  - "check.sh pins step (after identities, before shellcheck) populating build/pins/cc65 and build/pins/lynx-tests; identities prints each pin name and version"
  - "policy.pins, policy.pins.self_test, policy.no_network, policy.no_network.self_test, fetch_pin.mismatch.self_test (inventory now 41 tests)"
affects: [01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 6400
  tasks: 2
  commits: 2

plan_head_before: 20a12f2b50efe875aba0e70e1f1c2d1a4d716ea2
plan_head_after: 0964b4b7762f7052dc88b723749e6f8ac5ea5901

tech-stack:
  added: []
  patterns:
    - "A fetch writes its extraction and per-file list into a temp dir, then moves both into place and writes the stamp last, so a partial run is never mistaken for a verified cache"
    - "Network-token scans assemble every token from string parts (no_network.cmake and the fixture writer in supply.cmake), so the scan stays clean over its own text"
    - "Offline fetch control: file:// payloads in a mktemp dir outside the repository and build tree, asserted not inside REPO_ROOT, removed by an EXIT trap"

key-files:
  created:
    - pins.txt
    - scripts/fetch-pin.sh
    - tests/cmake/pins.cmake
    - tests/cmake/pins_controls.cmake
    - tests/cmake/no_network.cmake
    - tests/gates/supply.cmake
    - tests/gates/fetch_pin_control.sh
  modified:
    - scripts/check.sh
    - tests/tests.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "The .lynx-pin format is defined once in fetch-pin.sh: 64 lowercase hex characters and a newline; readers trim surrounding whitespace, anything else is no stamp"
  - "The .lynx-pin-files format: two leading comment lines (the first is '# sha256 of the verified archive: <hex>'), then '<sha256> <path>' lines sorted by path in LC_ALL=C order; the rights scan in plan 01-11 must skip lines starting with '#'"
  - "fetch-pin.sh supports only .tar.gz and .tgz (the zip branch is dropped); any other URL suffix is refused before the download"
  - "A path containing a newline anywhere in the archive makes fetch-pin.sh exit 1 before DEST is touched; an archive that already holds a file named .lynx-pin or .lynx-pin-files is refused"

patterns-established:
  - "Pattern: a control that needs a payload builds it under mktemp outside the repo and asserts so, rather than using ${LYNX_SCRATCH_DIR} (which is inside build/)"

requirements-completed: [FND-02]

coverage:
  - id: F1
    description: "Both pins are fetched by commit-SHA archive URL and verified against their SHA-256 before unpacking; check.sh runs the pins step and the downloaded digests equal those in STACK.md and RESEARCH"
    requirement: FND-02
    verification:
      - kind: integration
        ref: "sh scripts/check.sh: 'fetch-pin: cc65: verified d3e4bfa5...5c8a1, 4288 files listed' and 'fetch-pin: lynx-tests: verified a70edf38...fab3e, 75 files listed'; both digests also checked by shasum -a 256 before commit"
        status: pass
    human_judgment: false
  - id: F2
    description: "A digest mismatch makes fetch-pin.sh exit 1 with 'payload: sha256 mismatch' and DEST is never created (ordering edge)"
    requirement: FND-02
    verification:
      - kind: integration
        ref: "ctest fetch_pin.mismatch.self_test (cases mismatch, mismatch_dest_untouched)"
        status: pass
    human_judgment: false
  - id: F3
    description: "The pins lint accepts the real pins.txt and rejects an http URL, a 63-digit digest, an uppercase digest, a duplicate name, three and five fields, and a comments-only file (adjacency and empty edges)"
    requirement: FND-02
    verification:
      - kind: integration
        ref: "ctest policy.pins, policy.pins.self_test (8 cases: good_row accepted, 7 rejects each checked for exit 1 and the finding text)"
        status: pass
    human_judgment: false
  - id: F4
    description: "DEST/.lynx-pin-files lists every regular file of the verified archive with its SHA-256 (including a space-named file and a binary file), sorted by path, written before the stamp; the cache is reused only when both stamp and list are present and the stamp is right"
    requirement: FND-02
    verification:
      - kind: integration
        ref: "ctest fetch_pin.mismatch.self_test (cases verified, stamp, list, cached, refetch_without_list, refetch_wrong_stamp, newline_path); real run: cc65 list has 4290 lines (2 header comments + 4288 files)"
        status: pass
    human_judgment: false
  - id: F5
    description: "No CTest test reaches the network: policy.no_network passes on the repository and fails on fixtures naming a network tool and a CMake download command; the fetch control uses only file:// URLs in a temp dir outside the repository and the build tree"
    requirement: FND-02
    verification:
      - kind: integration
        ref: "ctest policy.no_network, policy.no_network.self_test; fetch_pin_control.sh asserts its work dir is outside REPO_ROOT and every control row is file://; find build -name 'payload*' prints 0 after check"
        status: pass
    human_judgment: false

duration: 20min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 09: Supply-chain gates Summary

**The check command now fetches cc65 and lynx-tests by commit-SHA archive URL, verifies each SHA-256 before unpacking, records a per-file SHA-256 list of every verified archive (4288 and 75 files) for the plan 01-11 rights scan, and a no-network scan with built controls keeps every test offline.**

## Performance

- **Duration:** about 20 min of agent time (start timestamp not captured separately).
- **Tasks:** 2 of 2, 2 commits.
- **Diff:** 25620 characters, so `actuals.tokens` is 6400 on the chars/4 scale (estimate 55000; over-estimated by about 8x, in line with plan 01-08).
- **First `sh scripts/check.sh` (cold, downloads both archives):** 20 s total. Pins step 14 s (network download and the 4288-file hash of cc65), shellcheck 0 s, workflow 6 s, junit 0 s. 41 tests accounted for once each, 0 declared skips.
- **Second `sh scripts/check.sh` (cache hit):** 6 s total; both pins print `cached (verified at fetch)`.
- **New tests are fast:** the five new tests add about 0.6 s of process time (policy group 0.25 s for 6 tests including the two earlier no_skip tests, fetch_pin 0.35 s).
- **CI not run for this plan:** the plan lists no push, so none was made (3 local commits are unpushed: the 01-08 docs commit and the two task commits). First CI run of the pins step is therefore still to be observed; the step needs `curl`, `tar`, `sha256sum` and github.com, all present on `ubuntu-24.04`.

## Accomplishments

- **pins.txt:** two rows with the commit-SHA archive URLs and digests from the plan. Both archives were downloaded into the scratchpad and hashed with `shasum -a 256` before the commit; they equal STACK.md and RESEARCH.
- **fetch-pin.sh:** adapted from muhtari (zip branch dropped). Order of operations: row lookup, cache check (stamp equals digest AND list present), archive suffix check, download into `mktemp -d`, hash, mismatch exit 1 (message includes the git-clone fallback identity note), extract with `--strip-components=1` into the temp dir, refuse newline paths and pin-record names, hash all files in bulk with one `find -exec ... {} +`, verify listed count equals file count, write the list, then `rm -rf DEST`, move, write the stamp last.
- **Controls:** `pins_controls.cmake` (8 cases), `fetch_pin_control.sh` (9 case groups, all `file://`), the no-network fixture self-test. `no_network.cmake` matches case-insensitively and reports `no_network: <file>:<line>: <token>`; I verified by hand that it also catches `gh  run` (two spaces), `git fetch` and `FILE(Download` in mixed case.
- **check.sh:** identities prints `pin: NAME VERSION` per row and fails loudly if `pins.txt` is missing; the `pins` step fails loudly on a missing or empty `pins.txt`.

## Task Commits

1. **Task 1: pins.txt, pins lint and the no-network gate, with controls** - `8d5ef97` (feat)
2. **Task 2: hash-before-unpack fetch-pin.sh with per-file list, offline control and pins step** - `0964b4b` (feat)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Extra fetch-pin safety checks beyond the plan text**
- **Found during:** Task 2 design (threat T-01-18).
- **Issue:** the plan fails closed only on newline paths. An archive that itself contains `.lynx-pin` or `.lynx-pin-files` could forge a stamp or list inside a hash-verified payload, and an empty or `/` DEST would make `rm -rf` dangerous.
- **Fix:** refuse an archive holding a pin-record name; refuse DEST empty or `/` (exit 2); verify the listed row count equals the regular-file count before writing the list. Added a `newline_path` control (case 7).
- **Files modified:** `scripts/fetch-pin.sh`, `tests/gates/fetch_pin_control.sh`
- **Commit:** `0964b4b`

### Clarifications (not deviations)

- The pins lint is the muhtari file verbatim except for the provenance header and one finding text: its "no pins" message became "no rows: the file holds no pin rows" so it matches the plan's "no rows" behavior.
- The plan's verify text expects "100% tests passed, 0 tests failed out of N"; the local CTest prints "100% tests passed out of N". Exit status and JUnit were used (as noted in plan 01-08).
- `find_program(LYNX_SH sh REQUIRED)` is repeated in `supply.cmake`; it is idempotent (cached) and keeps the file independent of `det/tests.cmake` ordering.
- `scripts/fetch-pin.sh` and `tests/gates/fetch_pin_control.sh` were made executable, matching `scripts/check.sh`.

**Total deviations:** 1 auto-added (Rule 2). **Impact:** none on scope.

## Issues Encountered

None. First-run results matched every acceptance criterion: 4290 lines in `build/pins/cc65/.lynx-pin-files` (2 header comments + 4288 files, the number RESEARCH measured), `find build -name 'payload*'` prints 0, and no `lynx-fetchpin.*` directory is left in the temp dir.

## Known Stubs

None.

## Threat Flags

None new. T-01-17 and T-01-18 (tampering): mitigated and tested offline (verify-before-unpack, DEST untouched on mismatch, list and extraction moved together, stamp last, missing list forces re-fetch). T-01-32: mitigated by `policy.no_network` and its self-test. T-01-SC: no package-manager installs; both inputs are SHA-256-pinned upstream archives (cc65 zlib, lynx-tests MIT).

## Notes for the next plan

- `build/pins/cc65` and `build/pins/lynx-tests` exist after any `sh scripts/check.sh`; each holds the unpacked archive with the top directory stripped, plus `.lynx-pin` (64 lowercase hex and a newline; compare after trimming) and `.lynx-pin-files`.
- `.lynx-pin-files` format for the rights scan (plan 01-11): lines beginning with `#` are comments (two of them, the first names the archive SHA-256); every other line is `<64 hex> <path relative to the archive top>`, sorted by path in `LC_ALL=C` order; paths may contain spaces, so split at the first space only. Symlinks are not listed (only regular files). The `.lynx-pin` and `.lynx-pin-files` files themselves are not in the list.
- To fetch a pin from a script: `sh scripts/fetch-pin.sh NAME build/pins/NAME` (env `PINS` overrides the file). A cached extraction prints `fetch-pin: NAME: cached (verified at fetch)`.
- `policy.no_network` scans `tests/**` (`*.cmake`, `*.sh`, `*.c`, `*.h`), `cmake/*.cmake` and `CMakeLists.txt` for the tool names `curl`, `wget`, `gh api`, `gh run`, `git clone|fetch|pull|ls-remote` and the CMake download, upload, FetchContent and ExternalProject_Add commands, case-insensitively. A later test script must not write any of these words, even in a comment (build the word from parts, as `supply.cmake` does). `scripts/` is not scanned.
- A pin bump is a dedicated change: edit `pins.txt`, recompute the archive digest, and the stamp mismatch forces a re-fetch, so no manual cleanup of `build/pins/NAME` is needed.
- The first CI run of the pins step has not happened yet; the next push will exercise it (about 14 s of added wall time on a cold runner, none when `build/pins` is cached, and CI does not cache `build/`, so expect it each run unless a cache is added).

## Self-Check: PASSED

All 7 created files exist (`pins.txt`, `scripts/fetch-pin.sh`, `tests/cmake/pins.cmake`, `tests/cmake/pins_controls.cmake`, `tests/cmake/no_network.cmake`, `tests/gates/supply.cmake`, `tests/gates/fetch_pin_control.sh`); commits `8d5ef97` and `0964b4b` are ancestors of HEAD; `git rev-list --count 20a12f2..HEAD` gave 2 before this summary commit; `sh scripts/check.sh` exits 0 twice (cold and cached); `shellcheck -s sh scripts/fetch-pin.sh tests/gates/fetch_pin_control.sh` exits 0.
