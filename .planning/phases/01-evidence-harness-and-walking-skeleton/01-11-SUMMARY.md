---
phase: 01-evidence-harness-and-walking-skeleton
plan: 11
subsystem: rights-hygiene
tags: [rights, firmware-scan, pii, credentials, hygiene, shell, cmake, check]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-09: .lynx-pin and .lynx-pin-files formats; plan 01-10: .lynx-build-files, fixture manifests, probe step, check.sh insertion points"
provides:
  - "scripts/scan-binaries.sh --source / --artifacts: control-byte or invalid-UTF-8 non-text detection, hard 512-byte rule in every zone, manifest sha256 allowlist, pin-zone archive and build lists, toolchain magic only inside the CMake binary dir outside scratch/"
  - "scripts/hygiene.sh --tree / --history: home path, non-noreply email, credential-shaped token and non-noreply identity rules"
  - "tests/hygiene/scan_control.sh (28 cases) and tests/hygiene/pii_control.sh (18 cases): seeded controls in isolated temp trees, fake values built at run time"
  - "check.sh steps artifacts (after junit) and history (after artifacts); tests hygiene.firmware_scan, hygiene.firmware_scan.self_test, hygiene.tree, hygiene.pii.self_test (inventory now 49 tests)"
affects: [01-12, 01-13]

actuals:
  tokens: 9970
  tasks: 3
  commits: 3

plan_head_before: 61e8350fceeeb2b59bd49d4b31f37e3ec7f7d252
plan_head_after: 10d2cefbf04578eba81d83815f3aacdf7e9f77a8

tech-stack:
  added: []
  patterns:
    - "Pin-zone scan hashes each pin dir with one bulk find -exec and compares sorted (sha256, path) rows against the sorted union of the archive list and the honoured build list with comm; only exactly-512-byte files and unlisted rows are run through the non-text test"
    - "Fake secrets, home paths and firmware-like bytes are generated at test time under mktemp outside the repository and asserted not to be inside it"

key-files:
  created:
    - scripts/scan-binaries.sh
    - scripts/hygiene.sh
    - tests/hygiene/scan_control.sh
    - tests/hygiene/pii_control.sh
    - tests/gates/rights.cmake
  modified:
    - scripts/check.sh
    - tests/tests.cmake
    - tests/inventory/fast.txt

key-decisions:
  - "Non-text = a control byte (NUL, C0 controls except tab/LF/FF/CR, DEL) via tr byte count, or iconv -f UTF-8 -t UTF-8 failing; never NUL alone"
  - "Findings go to stderr sorted and unique; the success summary line 'scan-binaries: <mode>: <N> files, <M> of exactly 512 bytes' goes to stdout; M counts every file of exactly 512 bytes (text included), as RESEARCH A9 asks"
  - "scripts/hygiene.sh keeps only --tree and --history and reads text files only (same non-text test); binaries belong to scan-binaries.sh. The only exclusion is the script itself"
  - "An unreadable pin-zone hash listing (fewer hashed rows than files) is a finding, never an empty pass"

patterns-established:
  - "Pattern: a gate script that must survive its own pattern text is excluded by exact path (SELF) only; its control script builds fakes from fragments so the tree scan stays clean over the control's source"

requirements-completed: [FND-04, FND-01]

coverage:
  - id: F1
    description: "A non-text file is detected by control bytes or invalid UTF-8, not by NUL alone; a 512-byte file with no NUL (values 128 + i%127) is rejected exactly like a NUL one, and valid non-ASCII UTF-8 text and tab/LF/FF/CR content are accepted"
    requirement: FND-04
    verification:
      - kind: integration
        ref: "ctest hygiene.firmware_scan.self_test (cases source_nul512, source_hi512_no_nul, source_utf8_text_accepted, source_control_bytes)"
        status: pass
    human_judgment: false
  - id: F2
    description: "Exactly 512 bytes fails in every zone with no exemption: source (even manifested), pins (even listed on either list), the CMake binary dir (even with ELF magic), scratch and other build dirs; a 512-byte text file is accepted"
    requirement: FND-04
    verification:
      - kind: integration
        ref: "ctest hygiene.firmware_scan.self_test (cases source_manifested_512_still_rejected, pins_listed_512_rejected, pins_build_listed_512_rejected, check_512_byte_elf_rejected, scratch_and_other_512_rejected, text_512_byte_file_accepted)"
        status: pass
    human_judgment: false
  - id: F3
    description: "Tracked and untracked-not-ignored non-text files pass only on a manifest sha256; ELF/Mach-O magic fails in the source zone even when manifested"
    requirement: FND-04
    verification:
      - kind: integration
        ref: "ctest hygiene.firmware_scan (real repo, 108 files, exit 0); hygiene.firmware_scan.self_test (source_unmanifested_then_manifested, source_toolchain_magic_even_if_manifested, source_elf_and_macho)"
        status: pass
    human_judgment: false
  - id: F4
    description: "Pin zone: a non-text file passes only on a path+sha256 match in .lynx-pin-files or in .lynx-build-files whose pin_sha256 header equals the .lynx-pin digest; added, modified, unlisted or stale-list files fail; listed files pass even with toolchain magic"
    requirement: FND-04
    verification:
      - kind: integration
        ref: "ctest hygiene.firmware_scan.self_test (pins_listed_passes, pins_unlisted_rejected, pins_modified_listed_file_rejected, pins_unlisted_toolchain_magic_rejected, pins_build_list_passes, pins_added_after_build_list_rejected, pins_modified_built_file_rejected, pins_build_list_digest_mismatch_not_honoured); real cc65 tree (4288 archive + 1699 build files) passes in the artifacts step"
        status: pass
    human_judgment: false
  - id: F5
    description: "Toolchain output (ELF, Mach-O, ar, ninja deps) passes only in the CMake binary dir outside scratch/; the same magic in scratch/ or another build dir fails; other non-text build files pass only when manifested"
    requirement: FND-04
    verification:
      - kind: integration
        ref: "ctest hygiene.firmware_scan.self_test (check_toolchain_outputs_pass, check_unmanifested_rejected, scratch_toolchain_magic_rejected, other_build_dir_toolchain_magic_rejected, other_build_file_passes_only_when_manifested)"
        status: pass
    human_judgment: false
  - id: F6
    description: "The artifact-zone scan runs as a check.sh step after workflow and junit, never while tests may be writing files; Finder metadata files under build/ are removed visibly before it"
    requirement: FND-04
    verification:
      - kind: integration
        ref: "sh scripts/check.sh: '== step junit' then '== step artifacts' then '== step history'; by hand: a planted build/toolchain/.DS_Store printed 'artifacts: removed Finder metadata ...' and the scan passed"
        status: pass
    human_judgment: false
  - id: F7
    description: "hygiene.tree and --history fail on a home path, non-noreply email, credential-shaped token (GitHub, fine-grained, AWS, PEM, Slack) or non-noreply author/committer, including content removed by a later commit and a path in a commit message; the allowed noreply forms pass"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "ctest hygiene.pii.self_test (18 cases); ctest hygiene.tree and sh scripts/hygiene.sh --history on the real repo: exit 0, no finding"
        status: pass
    human_judgment: false
  - id: F8
    description: "No fake secret is committed: the control scripts assemble every fake from fragments at run time in a temporary repository outside the checkout, and the tree scan reports nothing for them"
    requirement: FND-01
    verification:
      - kind: integration
        ref: "ctest hygiene.tree passes over tests/hygiene/pii_control.sh and scan_control.sh; both controls assert their work dir is outside REPO_ROOT; git status clean after runs"
        status: pass
    human_judgment: false

duration: 40min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 11: Rights and privacy scans Summary

**The check now fails closed on firmware-like binaries (exact-512-byte rule in every zone, manifest, pin-list and toolchain-magic rules per the amended CONTEXT) and on home paths, non-noreply emails, credential-shaped tokens and non-noreply commit identities, each rule proven by seeded controls that fail and then pass once the seed is removed.**

## Performance

- **Duration:** about 40 min of agent time (start timestamp not captured separately).
- **Tasks:** 3 of 3, 3 commits.
- **Diff:** 39886 characters, so `actuals.tokens` is 9970 on the chars/4 scale (estimate 75000; over-estimated by about 7.5x, in line with plans 01-08 to 01-10).
- **Dev host measurements (macOS arm64, 18 cores):**
  - `artifacts` step: 5 s on a build tree of 6386 files (4288 archive + 1699 build files in `build/pins/cc65` plus `build/check`, `build/toolchain`); the scan alone is 4.4 s wall (1.9 s user, 3.6 s system, dominated by hashing the pin tree with `shasum`).
  - `history` step: 4.6 s over 56 commits (57 now) (it grows with commit count: one `git grep` per commit per pattern; revisit if the history grows past a few hundred commits).
  - Tests: `hygiene.firmware_scan` 1.3 s, `hygiene.firmware_scan.self_test` 8 to 11 s (about 70 scan invocations, each starting several processes), `hygiene.tree` 1.5 to 3.4 s, `hygiene.pii.self_test` 2.8 to 3.1 s.
  - `sh scripts/check.sh` total 33 s warm (workflow step 23 s, up from about 6 s before this plan because of the two self-tests and the tree scan; CTest runs the 49 tests serially).
- **Exact-512-byte files found by the artifact scan on the dev host: 1** (`build/pins/cc65/libsrc/runtime/aslax7.s`, a cc65 assembler source and therefore text; the scan counts every file of exactly 512 bytes, text included, and fails only for non-text ones). The summary line is `scan-binaries: artifacts: 6386 files, 1 of exactly 512 bytes`. The source scan line is `scan-binaries: source: 108 files, 0 of exactly 512 bytes`. Plan 01-13 records the same two lines from the CI run (RESEARCH A9).

## Accomplishments

- **scan-binaries.sh:** implements the three derived ways of "listed in a manifest" from CONTEXT amendment items 1 to 3 and the 512-byte rule exactly; rule order is 512 first, then pin list (pin zone) or toolchain-magic location, then the zone rule. Source mode lists files with `git ls-files -z` into a temp file, fails if the listing fails or a name holds a newline. Artifact mode bulk-hashes each `build/pins/<name>/` and compares with `comm`; the build list is honoured only when its `# pin_sha256 = ` header equals the `.lynx-pin` digest. No file-name exemption exists (the `.DS_Store` removal is a visible `check.sh` step), and `grep -I` is not used.
- **scan_control.sh:** 28 cases in an isolated repository under `mktemp` (asserted outside the checkout), all fake bytes generated by `printf`, `head -c ... /dev/zero | tr` and an octal-escape loop. Also proves a 512-byte text file, UTF-8 text and tab/FF/CR content are accepted and that a control byte (ESC) makes a file non-text. A mutation check (loosening the build-list digest comparison) made the control fail, confirming it can detect the fault.
- **hygiene.sh and pii_control.sh:** adapted from muhtari with the home-path, email and credential patterns from RESEARCH Pattern 11; the muhtari stop-policy, branch, ROM-name, `.a78`, GPL and manifest blocks and the NUL rule are dropped. The real repository, including `.planning/`, is clean for both `--tree` and `--history`; no allowance was added for any personal value.
- **check.sh:** `artifacts` after `junit`, `history` after `artifacts` (skipped visibly on a single-commit history). Classification of build outputs in `build/check/` outside `scratch/`: none needed; everything there is an ELF/Mach-O executable, object or archive (magic list) or text, so no producer change and no exemption was required. `build/toolchain/probe.lnx` passes by its manifest sha256.

## Task Commits

1. **Task 1: Binary and firmware scan per the amended rule, with seeded controls for every zone** - `7e1ac02` (feat)
2. **Task 2: PII and credential hygiene scan for tree and history** - `53652dc` (feat)
3. **Task 3: Artifact-zone scan step in scripts/check.sh** - `10d2cef` (feat)

## Deviations from Plan

### Auto-fixed Issues

None - plan executed as written. Two implementation choices worth knowing:

- File listings are iterated with `while IFS= read -r` over temp files rather than a newline-IFS `for` over command substitution (shellcheck SC2044/SC2013). Newline-bearing paths are rejected up front (`path with a newline`, exit 1) in both modes, so the fail-closed intent of T-01-23 is kept.
- `hygiene.sh --tree` skips symlinks and reads non-text files not at all (plan: "skips non-text files using the same detector"); `tests/hygiene/pii_control.sh` includes a case proving a binary holding a fake home path is not read by this scan.

### Clarifications (not deviations)

- The plan's verify text expects "0 tests failed"; local CTest prints "100% tests passed out of N". Exit status and the JUnit step were used, as in earlier plans.
- The plan says the check run must not leave seeded files in the working tree: `git status` after the controls shows only the pre-existing untracked `.planning/state.json`.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path or trust-boundary schema was introduced.

## Notes for the next plans

- CI (plan 01-13) must check out with `fetch-depth: 0`, otherwise the `history` step sees one commit and skips, visibly. The artifacts and history steps need `iconv`, `tr`, `od`, `comm`, `sha256sum`, all in the base `ubuntu-24.04` image; the GNU `sha256sum` output format matches the sed in the scan.
- Anything new written under `build/` outside `build/check/` (and not manifested) will fail the artifacts step; new binary fixtures need a manifest `sha256` and a `files` entry (plan 01-10 rules). Runner dumps and bundles under `${LYNX_SCRATCH_DIR}` must stay hex text, and no file of exactly 512 bytes may be non-text.
- Workflow time grew from about 6 s to 23 s because the two self-tests spawn many processes; if CI timing review (AUTONOMY) flags it, the cheapest fix is running the hygiene label with `ctest -j` or trimming repeated accept calls in `scan_control.sh`.

## Self-Check: PASSED

- FOUND: scripts/scan-binaries.sh, scripts/hygiene.sh, tests/hygiene/scan_control.sh, tests/hygiene/pii_control.sh, tests/gates/rights.cmake
- FOUND commits: 7e1ac02, 53652dc, 10d2cef
