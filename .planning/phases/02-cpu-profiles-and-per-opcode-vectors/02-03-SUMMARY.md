---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 03
subsystem: test-data
tags: [singlesteptests, vectors, sample, pin-record, provenance, mit-notice, shell]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "plan 02-02 startup_tier vocabulary (fixture manifests accept startup_tier none)"
provides:
  - "scripts/vectors-full.sh with modes pin and sample (run mode stubbed for 02-05, exits 2)"
  - "fixtures/vectors/sst65x02/upstream.sha256: 512-row pin record for synertek65c02 and rockwell65c02 at 2f6980a2d95757486c7bee24355c360e40e2a224"
  - "fixtures/vectors/sst65x02/synertek65c02.sample and rockwell65c02.sample: first 32 objects of each of 256 files, upstream bytes verbatim, 16,384 vectors in all"
  - "fixtures/vectors/sst65x02/LICENSE: upstream MIT text verbatim"
  - "two fixture manifests (startup_tier none, T1, lynx1 / lynx2) and the provenance section"
affects: [02-04, 02-05, 02-11]

actuals:
  tokens: 1627958
  tasks: 2
  commits: 2

plan_head_before: 3d64d6a225a5fffba7a9c84ce230f1119b8b971f
plan_head_after: c554608e53af66cd7aaece49e3aac6b160450bf7

tech-stack:
  added: []
  patterns:
    - "Full-lane script with ordered modes sharing one mktemp work directory and one verified download set (pin then sample in a single invocation)"
    - "Third-party data committed as hash-keyed manifest entries with the verbatim licence as the notice"

key-files:
  created:
    - scripts/vectors-full.sh
    - fixtures/vectors/sst65x02/upstream.sha256
    - fixtures/vectors/sst65x02/synertek65c02.sample
    - fixtures/vectors/sst65x02/rockwell65c02.sample
    - fixtures/vectors/sst65x02/LICENSE
    - fixtures/manifests/sst65x02-synertek65c02.manifest
    - fixtures/manifests/sst65x02-rockwell65c02.manifest
    - .gitattributes
  modified:
    - docs/provenance.md

key-decisions:
  - "The synertek manifest names the shared files (LICENSE and upstream.sha256) in its files value; the rockwell manifest names only its sample. check-manifests accepted this unchanged."
  - "The sample container keeps the three-line header the plan prescribes, so the files are 211 bytes larger than the research scratch sizes (see Deviations)."

requirements-completed: []

duration: 400 s wall including the 200 s pin-and-sample run (one session; the full check added 76 s)
completed: 2026-11-09
status: complete
---

# Phase 2 Plan 03: Pinned vector sample and full-lane script Summary

**`scripts/vectors-full.sh pin sample` downloads the 512 pinned SingleStepTests files over https, verifies them, and reproduces two committed 16,384-vector sample containers plus the 512-row pin record and the upstream MIT LICENSE; `sh scripts/check.sh` passes with the data, manifests and provenance notice and no scan rule touched.**

## Accomplishments

- Task 1 (commit b1b2f26): the script, pin record, both samples, LICENSE and `.gitattributes` (`fixtures/vectors/sst65x02/* -text`; `git check-attr text` reports `unset`).
- Task 2 (commit c554608): two fixture manifests and the "Third-party data committed with notice" section of `docs/provenance.md`.
- Script properties: POSIX sh, `set -eu`, shellcheck clean on 0.11.0 (one justified `SC2016` disable on the `xargs sh -c` child command, in the style of `fetch-pin.sh`); https only with `--proto '=https' --proto-redir '=https' --max-filesize 8000000`; op names generated as two hex digits; work directory from `mktemp -d`, asserted outside the repository (build/ is inside it), removed by an EXIT trap (verified: no `lynx-vectors.*` directory remained); each download is hashed against the pin record before use; the LICENSE is checked against its known SHA-256; each container is built twice from the same verified files and compared with `cmp` before it is written. Usage errors (no mode, unknown mode) and the `run` stub exit 2; `sample` without a pin record fails with exit 1.

## Measured (the single `pin sample` invocation)

| Item | Value |
|------|-------|
| Wall time of the invocation | 200 s (macOS, 8 parallel streams) |
| Download phase | 512 files, 2,006,647,698 bytes (about 2.0 GB), 188 s |
| Pin rows written | 512 |
| `synertek65c02.sample` | 3,201,546 bytes, sha256 `2fabae3f5acbfe44f9782721ed1cb96a5be6de4575038cc3cab3d309f2a7164b`, 256 sections, 8,192 vectors |
| `rockwell65c02.sample` | 3,230,210 bytes, sha256 `1679d8fe0dae742e93dff97d594d7b8cec7468b1d7a43240d1c52f5e9b53f921`, 256 sections, 8,192 vectors |
| Sample line count (synertek) | 8,698 (3 header lines, 256 `#file` lines, 8,192 + 247 `[` lines) |
| Regenerated (`lines 32`) sections | synertek: `e1 e5 e9 ed f1 f2 f5 f9 fd` (9); rockwell: the 25 files of RESEARCH 3.2 (`0f 1f ... df ef ff` plus `e1 e5 e9 ed f1 f2 f5 f9 fd`) |
| Whole-file recheck | `synertek65c02/v1/e9.json` row equals `3a33b31a76638a83748484cbd3ba8c5a70f04bdbb778b0ff87e2a375455b1b23` |
| `sh scripts/check.sh` | exit 0, total 76 s (workflow 31 s, artifacts 15 s, history 28 s); 62 of 62 tests passed once each; `scan-binaries: artifacts: 6423 files, 1 of exactly 512 bytes`; no `check: step`, `check-manifests:` finding or `hygiene:` line |
| Pack size | repository pack still 153 KiB loose (the data compresses at the next gc; RESEARCH 3.1 measured 1.85 MiB packed) |

The reproducibility check built into `sample` (two builds, `cmp`) passed. The plan forbids repeating the large download, so a second independent run was not made; the committed bytes are what that one verified build wrote.

## Recheck before use (CONTEXT)

- `gh api repos/SingleStepTests/65x02/commits/main --jq .sha` printed `2f6980a2d95757486c7bee24355c360e40e2a224`: upstream HEAD has not moved from the pin (committer date 2025-05-10T14:49:00Z).
- The LICENSE at the pinned commit hashes to `5fb64d1f2e4d74fabc85c768e1f6ffa73b95dc93e217c55acfe7e4a89ba32b04`: "MIT License, Copyright (c) 2024 Thomas Harte et al", unchanged from the research reading.

## Deviations from Plan

### Auto-fixed Issues

**1. [Documentation discrepancy, no code change] Sample sizes in the plan's verify command exclude the header**
- **Found during:** Task 1 verification
- **Issue:** The plan's verify command asserts sizes 3201335 and 3229999. The built containers are 3201546 and 3230210 bytes, exactly 211 bytes larger on both. The research sizes (RESEARCH 3.2 and 5.2: "in the scratch build") were measured on a scratch container without the three-line header that the plan's own action step and RESEARCH 5.2 prescribe (`#sst65x02-sample v1`, `#source ...`, `#rule ...` = 20 + 107 + 84 = 211 bytes).
- **Resolution:** The prescribed header was kept (it is the format the 02-04 reader and the identity control will parse). The body below the three header lines is exactly 3,201,335 and 3,229,999 bytes (`tail -n +4 | wc -c`), matching the research measurements to the byte, which is independent confirmation that the data is the intended data. No constant was edited; the plan's `wc -c` equality tests would fail by 211 bytes as written, and later plans that hard-code the size should use 3,201,546 and 3,230,210 (or the body sizes).
- **Files modified:** none
- **Commit:** n/a

**2. [Rule 3 - Blocking] shellcheck SC2016 on the `xargs sh -c` child command**
- **Found during:** Task 1
- **Issue:** the single-quoted child command is meant to be expanded by the child shell.
- **Fix:** an explanatory `# shellcheck disable=SC2016` line (one occurrence), like the SC2086 disable in `fetch-pin.sh`.
- **Files modified:** scripts/vectors-full.sh
- **Commit:** b1b2f26

**Total deviations:** 1 plan-text discrepancy recorded, 1 trivial lint fix. **Impact:** none on the data or gates.

## Notes for later plans

- The shared files (`LICENSE`, `upstream.sha256`) are named by the synertek manifest only. A change to either changes nothing in the rockwell manifest; the check only requires each file under `fixtures/` to be named once.
- Container grammar for the 02-04 reader: line 1 `#sst65x02-sample v1`; line 2 `#source https://github.com/SingleStepTests/65x02 <commit> <variant>/v1`; line 3 `#rule ...`; then 256 sections `#file <op>.json lines <L>` (L = 33 when upstream line 1 is `[`, L = 32 when line 1 starts `[{ "name"`) followed by exactly L upstream lines each ending `\n`. In standard-layout sections the 32nd object line still ends in its upstream `,`.
- `scripts/vectors-full.sh run` exits 2 with "run mode arrives in plan 02-05". Fetching all 512 files takes about 190 s and about 2.0 GB of scratch space under `${TMPDIR:-/tmp}` (removed on exit); plan 02-11 should budget that and should pass modes together in one invocation.
- Any change to the vector-runner test count in `tests/inventory/fast.txt` later must refresh `docs/claims/skeleton-repeatability.claim` `inventory_sha256` in the same commit; this plan added no CTest name, so the inventory and claim are unchanged.
- `.planning/state.json` was untracked before this plan and was left alone.

## Known Stubs

The `run` mode of `scripts/vectors-full.sh` is a deliberate stub that exits 2 and states "arrives in plan 02-05"; 02-05 resolves it. It does not affect this plan's goal (the sample and pin record). Not added to the windows ledger since the owning plan is already scheduled.

## Threat Flags

None. T-02-06 to T-02-08 mitigations are implemented as planned: commit-pinned https-only URLs with `--proto-redir` and `--max-filesize`, hash verified before any read, generated op names, work directory outside the repository removed by trap, MIT data with its verbatim LICENSE manifested and recorded in provenance, no scan rule or allow-list touched (`git diff 3d64d6a HEAD -- scripts/hygiene.sh scripts/scan-binaries.sh` is empty).

## Self-Check: PASSED

- FOUND: scripts/vectors-full.sh, the two `.sample` files, LICENSE, upstream.sha256, `.gitattributes`, both manifests; LICENSE sha256 equals `5fb64d1f2e4d74fabc85c768e1f6ffa73b95dc93e217c55acfe7e4a89ba32b04`; 512 pin rows; 256 sections per sample; `lines 32` sets match; `git check-attr text` on LICENSE reports `unset`; no `wdc65c02` or `nes6502` in the script.
- FOUND commits b1b2f26 and c554608 as ancestors of HEAD (`plan_head_before` 3d64d6a to `plan_head_after` c554608, 2 commits).
- `sh scripts/check.sh` exits 0.
