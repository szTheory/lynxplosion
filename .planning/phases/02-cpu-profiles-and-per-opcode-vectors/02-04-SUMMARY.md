---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 04
subsystem: cpu-vectors
tags: [vectors, singlesteptests, strict-reader, inventory, profiles, c17]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-01 cycle-stepped CPU, profile tables and flat bus; 02-03 pinned samples, manifests and provenance"
provides:
  - "tests/cpu/sst_reader.h/.c: strict allocation-free reader for the sample container (full and partial) and complete upstream files, both physical layouts"
  - "lynx-test-cpu-vectors: parse-check, self-test-reader, run (--print-plan) with exit codes 0/1/2/3/4"
  - "tests/inventory/vectors/{lynx1,lynx2,none}.txt: committed exact per-profile plans (run, excluded, undefined rows with closed-list reasons)"
  - "CTest cpu.vectors.lynx1, cpu.vectors.lynx2, cpu.vectors.none and cpu.vectors.reader_malformed"
affects: [02-05, 02-10, 02-11]

actuals:
  tokens: 22231
  tasks: 3
  commits: 2

plan_head_before: f6736500edde0205cbcf9d9e7b2014eca2e18113
plan_head_after: 5c0333bfd7091d9df55fd0b12b80afadeb2dbb03

tech-stack:
  added: []
  patterns:
    - "Plan derived from data plus profile table at run time and compared byte for byte with a committed file; pass counts never committed"
    - "Reader iterator with fixed key order, located messages (label, opcode, 0-based vector ordinal) and no stdio or allocation"
    - "Address-hashed poison for unlisted reads, restore only touched addresses"

key-files:
  created:
    - tests/cpu/sst_reader.h
    - tests/cpu/sst_reader.c
    - tests/cpu/test_cpu_vectors.c
    - tests/inventory/vectors/lynx1.txt
    - tests/inventory/vectors/lynx2.txt
    - tests/inventory/vectors/none.txt
  modified:
    - tests/cpu/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Reason selection order for a run row: vec EXCL_HW, then F_UNV_SHAPE, then F_DEC_EXTRA, then vec TIER_STATE (cycles-conflict-sources), else '-'; this yields exactly 2 / 32 / 18 / 5 rows for lynx1 and the plan's expected sets"
  - "Registered a fourth test, cpu.vectors.reader_malformed, so the reader's rejection list is a persisted gate rather than a one-time verify command"

requirements-completed: [CPU-01]

duration: one session (check.sh 34 to 38 s wall)
completed: 2026-11-09
status: complete
---

# Phase 2 Plan 04: Vector runner and exact per-profile inventories Summary

**All 8,128 asserted synertek vectors pass under lynx1, all 8,128 rockwell vectors under lynx2 and 7,104 synertek vectors under none (with the 32 bit-op opcodes proven undefined), each against an exact committed inventory, with no CPU change needed.**

## Accomplishments

- Task 1 (commit eb6a549): `sst_reader` parses both samples to 256 sections and 8,192 vectors each. `self-test-reader` prints 24 `ok` lines (22 rejections plus the two accept cases of the synthetic container and the 10,000-object file; the 9,999 file is counted among the rejections) with a located message, covering every item of the plan's behavior list (truncated object, unknown/duplicate/missing key, `pc` 70000, register 256, negative, leading zero, PC byte not the opcode, name token not the opcode, line-count differences both ways, zero-vector section, 65 ram entries, 17 cycles, bad direction, duplicate `initial.ram` address, trailing bytes after the last section, truncated sample, an object split across lines) and accepts a synthetic full container and a 10,000-object complete file while rejecting 9,999.
- Task 2 (commit 5c0333b): `run` checks the sample SHA-256 against the manifest before any parse (exit 3 `sha256 mismatch`), derives the plan, compares byte for byte (exit 4 naming the first differing line, expected and derived), then executes. Inventories were generated once with `--print-plan` and reviewed against RESEARCH 5.5.
- Task 3: `sh scripts/check.sh` exits 0, "100% tests passed out of 67", no `check: step` line. No file change (no TIMEOUT adjustment needed).

## Per-profile counts (measured, `ctest -R '^cpu\.vectors'`)

| Profile (sample) | files | run | excluded | undefined | vectors | asserted | passed | failed | identity | excluded vectors | unverified-cycles rows/vectors | unverified-shape rows/vectors |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| lynx1 (synertek) | 256 | 254 | 2 (`cb`, `db`) | 0 | 8192 | 8128 | 8128 | 0 | 3057 | 64 | 4 / 128 | 32 / 1024 |
| lynx2 (rockwell) | 256 | 254 | 2 (`cb`, `db`) | 0 | 8192 | 8128 | 8128 | 0 | 2811 | 64 | 4 / 128 | 0 / 0 |
| none (synertek) | 256 | 222 | 2 (`cb`, `db`) | 32 | 8192 | 7104 | 7104 | 0 | 2033 | 64 | 4 / 128 | 0 / 0 |

For none, all 1,024 vectors of the 32 undefined rows were checked (1,024 stopped). Reasons per inventory: 18 `cycles-unsourced-decimal` (ADC/SBC), 5 `cycles-conflict-sources` (`1e 3e 5c 5e 7e`), 2 `source-disagrees-with-hardware-informed-test` (`cb db`), 32 `shape-unsourced-for-lynx1` in lynx1 only, 32 `profile-none-stops-undefined` in none only. Sample SHA-256 values are the manifest values (synertek `2fabae3f...164b`, rockwell `1679d8fe...f921`), unchanged; `git diff --quiet f673650 HEAD -- fixtures/vectors` succeeds.

## Timings (CPU seconds are small; host load average was 15 to 30)

| Item | Value |
|---|---|
| `cpu.vectors.lynx1` / `lynx2` / `none` | 0.37 / 0.37 / 0.35 s (first run), 0.40 / 0.41 / 0.40 s inside the full check; `real` 0.35 s, `user` 0.28 s per run, under ASan+UBSan |
| `cpu.vectors.reader_malformed` | 0.31 to 0.33 s |
| `parse-check` of one sample | 0.12 to 0.14 s |
| `sh scripts/check.sh` | total 38 s (first full run, 67 tests) and 34 s (second, on HEAD); workflow step 13 s; history step 16 to 18 s |

RESEARCH A2 estimated 0.5 to 0.9 s per variant; the measured cost is about 0.4 s. No vector test exceeds 10 s, so no CI efficiency item is raised for plan 02-11 from this plan and no TIMEOUT was changed.

## CPU bugs exposed by the vectors

None. The 02-01 CPU and tables passed every asserted vector of both samples on first run, including the decimal-mode ADC/SBC vectors (292 with D set in the synertek sample, 286 in the rockwell sample) asserted at the final-state tier. No row of `core/cpu_profile.def` and no line of `core/cpu.c` changed in this plan. To make sure this is not a vacuous pass, I ran (not committed as gates here, they are plan 02-05 controls) the rockwell sample under lynx1 (exit 1, 502 failed of 8,128, failures first seen at opcode 07), the synertek sample under lynx2 (failures present), an inventory copy missing one row (exit 4 naming line 22: expected `0b`, derived `0a`) and a sample with one byte changed (exit 3 `sha256 mismatch`). Two runs of the same command give byte-identical stdout (`cmp`).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] The reader's rejection list needed a persisted gate**
- **Found during:** Task 1
- **Issue:** the plan runs `self-test-reader` only in the Task 1 verify command and lists three inventory rows; a control that is not registered is never run by `check.sh` and can rot silently (AGENTS.md: a meaningful negative control for every gate, exact inventory).
- **Fix:** registered `cpu.vectors.reader_malformed` (LABELS cpu, TIMEOUT 60), added its inventory row and refreshed the claim `inventory_sha256` in the same commit. Four rows were added in the plan instead of three; no later plan counts rows.
- **Files modified:** tests/cpu/tests.cmake, tests/inventory/fast.txt, docs/claims/skeleton-repeatability.claim
- **Commit:** eb6a549

**2. [Rule 3 - Blocking] macOS SDK marks `sprintf` deprecated, and the project builds with `-Werror`**
- **Found during:** Task 1 build
- **Fix:** the self-test builders use one `vsnprintf` wrapper (`spf`) that aborts on truncation; the plan's `run` code uses `vsnprintf` too.
- **Files modified:** tests/cpu/test_cpu_vectors.c
- **Commit:** eb6a549

### Recorded design choices (inside Claude's discretion)

- **Reader provides a `partial` mode now** (`sst_open_sample(..., partial)`): ascending subset of sections, at least one. The `run` command does not use it yet; plan 02-05's `--partial` needs it, and the `none`-plan derivation, totals and execution loops would have to learn a section subset (rows for missing opcodes have 0 vectors today only because the full-sample check rejects fewer than 256 sections).
- **Plan header details not fixed by RESEARCH 5.5:** the source line is rebuilt from the sample's own `#source` line (commit and variant), so a rockwell sample run under lynx1 derives a self-consistent plan for the cross controls; `none.txt` appends `(same data as lynx1; rows defined only where lynx1 and lynx2 agree)` to its source line; totals lines carry `asserted`/`excluded_vectors` (plus `undefined_vectors` for none).
- **The undefined-row check under none is seam-level.** `probe_undefined` asserts the boundary rule the machine loop applies (CPU at a boundary, posted request is the opcode fetch at PC, `lynx__cpu_opcode_defined(0, mem[pc])` false and the byte equals the row opcode, no cycle logged, CPU struct and touched memory unchanged). It makes no `lynx__cpu_complete` call, so the CPU-unchanged assertion is true by construction here; the real UNDEFINED stop with consumed 0 is asserted at machine level in plan 02-10 Task 2, as the plan states.
- **Vector ordinal in messages is the 0-based index** of the object inside its section or file.

**Total deviations:** 2 auto-fixed, 4 recorded choices. **Impact:** none on the plan's goal.

## Known Stubs

None.

## Threat Flags

None. T-02-09 (reader bounds, no allocation, fail closed with location, sanitizers on, malformed-input self-test) and T-02-10 (derived plan compared byte for byte, closed reason vocabulary generated from the table, no pass counts committed, hash before parse) are implemented as planned. The reader is test-side code; `core/` is untouched.

## Notes for the next plan (02-05 onward)

- Runner CLI: `lynx-test-cpu-vectors run --profile none|lynx1|lynx2 --sample F --manifest M --expect-plan P` (all four required; usage error exit 2), or `--sample F --print-plan` (no manifest or plan needed, prints the derived plan, exit 0). A byte mismatch prints `inventory mismatch: first difference at line N` with both lines (exit 4); a hash mismatch prints `sha256 mismatch: sample <hex>, manifest <hex>` (exit 3, before any parse); reader errors print `<label>: opcode xx vector N: <what>` (exit 3). Unlisted-read poison is `poison(a)` in `test_cpu_vectors.c`.
- Reusable internals for 02-05: `run_vector` (execution), `classify` (row status, tier, reason), `g_res.op_passed[256]` and `g_res.op_failed[256]` (already filled, ready for `--results`), `is_identity`, `execute_pass`. Not yet implemented: `--partial`, `--results`, `--fault no-step|stray-write|mutant-69`, the `complete` subcommand (the reader's `sst_open_file` exists and is self-tested with 10,000 and 9,999 objects).
- The hardcoded size constants in some plan verify commands are not used by anything here; the committed samples are 3,201,546 and 3,230,210 bytes (211 more than the research scratch sizes).
- `tests/inventory/fast.txt` now has 4 more rows (67 tests total); claim `inventory_sha256` refreshed in each commit that changed it (latest value from `sh scripts/check-claims.sh --inventory-sha256`).
- Estimate: projected 100,000 tokens, actual 22,231 on the chars/4 scale over the realized diff (88,924 characters including the three inventories).

## Self-Check: PASSED

- FOUND: tests/cpu/sst_reader.h, tests/cpu/sst_reader.c, tests/cpu/test_cpu_vectors.c, tests/inventory/vectors/lynx1.txt, lynx2.txt, none.txt.
- FOUND commits eb6a549 and 5c0333b as ancestors of HEAD (`plan_head_before` f673650 to `plan_head_after` 5c0333b, 2 commits).
- `sh scripts/check.sh` exits 0 (67 of 67 tests passed); no malloc, calloc or realloc in tests/cpu/sst_reader.c; `git diff --quiet f673650 HEAD -- fixtures/vectors` succeeds.
