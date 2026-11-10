---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 05
subsystem: cpu-vector-controls
tags: [vectors, negative-controls, wrong-profile, pin-lint, full-lane, c17, posix-sh, cmake]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-03 pinned samples, manifests, vectors-full.sh pin and sample modes; 02-04 strict reader, run mode, exact per-profile inventories"
provides:
  - "lynx-test-cpu-vectors: --results, --partial, --fault no-step|stray-write|mutant-69 on run, and a complete subcommand over whole upstream files"
  - "tests/cpu/vector_controls.sh: nine named-fault controls (cpu.vectors.wrong_profile ... complete_mode)"
  - "tests/cmake/vectors_pin.cmake and vectors_pin_controls.cmake: pin-record lint and its per-fault control (policy.vectors_pin, policy.vectors_pin.self_test)"
  - "scripts/vectors-full.sh run: the full lane (all 512 downloads verified, samples re-derived and cmp'd, 3 same-profile runs, 2 cross pairings)"
affects: [02-10, 02-11]

actuals:
  tokens: 17653
  tasks: 3
  commits: 3

plan_head_before: 61d34ac466f448bb664ead8285cfcce33a76733d
plan_head_after: 8c867b3e2dc4cbd1c81c27fefe185b58290f9233

tech-stack:
  added: []
  patterns:
    - "Faults built at test time from the real sample with awk (mini-samples, fresh SHA-256 manifest, plan printed by the runner), asserted by exact exit code plus message regex"
    - "Failing opcode set parsed from a --results file and compared with the predicted set, not merely non-empty"
    - "Findings printed with message(STATUS) then one short FATAL_ERROR; lint reads the record line by line so finding numbers are real line numbers"

key-files:
  created:
    - tests/cpu/vector_controls.sh
    - tests/cmake/vectors_pin.cmake
    - tests/cmake/vectors_pin_controls.cmake
  modified:
    - tests/cpu/test_cpu_vectors.c
    - tests/cpu/tests.cmake
    - tests/gates/supply.cmake
    - scripts/vectors-full.sh
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "no_step compares failures with asserted minus 'unmoved' (final state equals loaded state, pc included), not asserted minus 'identity': the plan's wording is false as written because identity ignores pc and every NOP-like vector advances pc"
  - "A ninth control, cpu.vectors.complete_mode, runs the full lane's whole-file mode on three synthetic 10,000-object files built from the sample, so the runner path of the 2 GB lane is checked offline in about 1.5 s"
  - "vectors_pin_controls.cmake is a separate driver (as pins_controls.cmake is) so one test covers eight cases, each its own process with its own exact exit code and regex"

patterns-established:
  - "A partial sample prints a plan with only the present opcodes and 'files N' in the totals line; full samples print byte-identical plans to before"

requirements-completed: [CPU-05, CPU-01]

duration: one session (check.sh 37 to 41 s wall)
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 05: Vector controls, pin lint and the full lane Summary

**The vector gates now fail in the failing direction: lynx2 data under lynx1 and lynx1 data under lynx2 each fail on exactly the 32 `$x7/$xF` bit-operation opcodes, and the full network lane (`scripts/vectors-full.sh run`) is written and statically checked but not run.**

## Accomplishments

- **Task 1 (commit cbb82ac).** `lynx-test-cpu-vectors run` and the new `complete` subcommand take `--results FILE` (one `op<TAB>passed<TAB>failed` line per run row that executed, ascending), `--partial` and `--fault`. The faults are built inside the runner at run time and never committed: `no-step` loads the state and never calls the CPU; `stray-write` makes the bus log and perform one extra write to an unlisted address on the first executed vector; `mutant-69` copies the profile table into a test-local 256-row array, replaces row `$69` with row `$E9` and passes that pointer to the flat bus (no core change). `tests/cpu/vector_controls.sh` implements nine cases, registered as nine CTest tests (LABELS cpu, TIMEOUT 120), nine inventory rows, claim hash refreshed.
- **Task 2 (commit c41eece).** `tests/cmake/vectors_pin.cmake` (`cmake_policy(VERSION 3.25)`, `-DFILE=`) reads the pin record line by line. `policy.vectors_pin` runs it on the real record; `policy.vectors_pin.self_test` runs `vectors_pin_controls.cmake`, which mutates a copy of the real record per case and requires exit 1 plus the finding with its line number.
- **Task 3 (commit 8c867b3).** `scripts/vectors-full.sh run` (see "Full lane" below), shellcheck clean, usage on no mode exits 2, and the complete-mode control was cut from five to three synthetic files.

## Controls: expected versus observed

Every control asserts the exact exit code and a message regex; the failing set is parsed from `--results`.

| Control | Fault | Expected | Observed |
|---|---|---|---|
| `wrong_profile` | rockwell sample under lynx1 | exit 1; failing set = the 32 `$x7/$xF` opcodes | exit 1; set `07 0f 17 1f ... f7 ff`, exactly 32, equal; 7,611 passed, 517 failed; fewest failures on one opcode 11 (`f7`) |
| `wrong_profile_reverse` | synertek sample under lynx2 (hashed poison) | same exact set | exit 1; the same 32 opcodes; 7,626 passed, 502 failed; fewest failures 10 (`a7`) |
| `no_step` | `--fault no-step`, both real samples | failed = asserted - unmoved, nearly all fail | synertek under lynx1: asserted 8,128, passed 0, failed 8,128, identity 3,057, unmoved 0; rockwell under lynx2: asserted 8,128, passed 2, failed 8,126, identity 2,811, unmoved 2; all 254 run rows fail in both |
| `corrupt_expect` | `final.a` of `ea` vector 0 raised by one | exit 1, one failure naming opcode, ordinal, field | `FAIL opcode ea vector 0 a: got N expected N+1`, exactly one FAIL line, set `{ea}` |
| | first `final.ram` value of `8d` vector 0 raised by one | same | `FAIL opcode 8d vector 0 ram: ...`, one line, set `{8d}` |
| | `--fault stray-write` | exactly one `nowrite` failure | `FAIL opcode 00 vector 0 nowrite: got N expected 0`, one line, set `{00}` |
| `malformed` | 12 mini-sample faults (truncated object, unknown key, duplicate key, missing final, `pc` 70000, register 256, negative number, PC byte not the opcode, name token not the opcode, object split across lines, lines header +1 and -1) | exit 3 naming file and vector, no success line | all 12 exit 3 with `mal_<case>.sample: ... opcode ea vector 0: <reason>` (or `opcode ea` for the two layout faults); no `asserted`, `totals`, `plan matches` or `vectors 0` line |
| `inventory_drift` | 9 plan edits on mini-plans | exit 4 at the exact line | missing row (line 14), excluded row without reason (15), reason outside the list (12), table exclusion missing from file (15), stale exclusion in file (16), wrong totals (17), none-only reason in a lynx1 plan (13), none `undefined` row changed to `run` (13), none shared row changed to `undefined` (12): each exit 4 `first difference at line N` with N the edited line |
| | sample-side: 31 objects, duplicated `#file` | exit 3 | `opcode 00 section: the section holds a different object count than 32: 31`; `sections are not in opcode order`; both exit 3 |
| `sample_identity` | one byte of a copy of the real sample changed (still valid JSON: `parse-check` of the copy exits 0, `cmp -l` shows 1 byte) | exit 3 `sha256 mismatch` before any parse | exit 3 `sha256 mismatch: sample <hex>, manifest 2fabae3f...`; no `inventory mismatch`, `FAIL` or `asserted` line |
| `mutant_profile` | `$69` decoded as SBC (table copy) on a mini-sample of `00 69 e9 ea` | failing set exactly `{69}` | `{69}` (the unmutated mini-sample passes first, so the failure is the fault's) |
| `complete_mode` | three synthetic whole files (`07 cb ea`, 10,000 objects each) | clean runs, no-step fails everything, missing file and 9,999-object file exit 3 | lynx1: 30,000 vectors, 20,000 asserted and passed, 10,000 excluded; none: 10,000 `07` vectors stopped, 10,000 passed; no-step: 20,000 of 20,000 failed; missing file exit 3 `cannot open`; 9,999 objects exit 3 `different object count than 10000: 9999` |
| `policy.vectors_pin.self_test` | committed record (must pass), `http://` source, 41-digit commit, 63-digit digest, 511 rows, repeated row, unsorted row, path `wdc65c02/v1/00.json` | exit 1 with the finding and its line | all eight cases as expected, e.g. `line 4: the digest is not 64 lower-case hex digits (63 characters)`, `line 5: rockwell65c02/v1/00.json is not in sorted (byte) order after rockwell65c02/v1/01.json`, `511 rows, not 512` plus `missing row for synertek65c02/v1/ff.json` |

**Controls shown to detect a weakened gate** (temporary, not committed):
- Poison made constant (`0xA5`): `wrong_profile` still gave the 32 opcodes, `wrong_profile_reverse` gave 24 (`0f 2f 5f 7f 9f bf cf ef` missing, the BBR 0, 2, 5, 7 and BBS 1, 3, 4, 6 opcodes RESEARCH 5.4 predicted) and the control FAILED with "the failing opcode set differs". Restored; the committed poison is hashed.
- A runner wrapper that silently drops `--fault X`: `no_step`, `corrupt_expect` (stray-write), `mutant_profile` and `complete_mode` all FAILED with "expected exit 1, got 0".
- A runner that always exits 0: `wrong_profile`, `malformed`, `inventory_drift` and `sample_identity` all FAILED.

## Full lane (`scripts/vectors-full.sh run`)

Written to the plan: verifies all 512 downloads against `upstream.sha256` (abort on mismatch), rebuilds both samples and `cmp`s them with the committed ones (abort on difference), builds the check preset if `build/check/tests/lynx-test-cpu-vectors` is missing, runs `complete` under `--profile none` (synertek data), `--profile lynx1` (synertek) and `--profile lynx2` (rockwell), writes each results file to `build/vectors-full/<name>.results` with its SHA-256 printed, then runs the two cross pairings and prints each failing set together with what lies beyond the 32 bit-operation opcodes and which of the 32 do not fail (the profile-diff spike). It collects every lane's result and exits 0 only if the three same-profile runs are clean and both cross failing sets equal the 32 opcodes exactly.

It was **not run** (the 2 GB download is plan 02-11's). What was checked instead: shellcheck 0.11 and 0.9.0, `sh scripts/vectors-full.sh` exits 2 with usage, the acceptance greps (`cmp`, `--profile none` in code lines), and a throwaway harness outside the repository that replaced only the download, verify and sample-build steps with synthetic whole files (`07 cb ea` for both variants), ran the real post-download code, and showed: all three same-profile lanes exit 0 and write results with printed SHA-256 values; with the real 32-opcode expectation the cross lanes were reported FAILED (only `07` fails on that mini data) and the script exited non-zero; with the expectation patched to `{07}` the script exited 0. Plan 02-11 will exercise the part this could not: real downloads, the byte-for-byte sample equality, and the full-data failing sets.

## Timings (CPU seconds, host load average high; macOS, ASan and UBSan)

| Item | Value |
|---|---|
| `wrong_profile` / `wrong_profile_reverse` | 0.74 / 0.74 s (full samples) |
| `no_step` | 1.45 s (two full samples) |
| `corrupt_expect` / `malformed` / `inventory_drift` | 0.75 / 1.37 / 1.55 s (mini-samples; most of it is process start) |
| `sample_identity` / `mutant_profile` | 0.48 / 0.35 s |
| `complete_mode` | 1.62 s (was 2.49 s with five files; Linux gcc container measured 7.4 s with five files before the cut) |
| `policy.vectors_pin` / `.self_test` | 0.03 / 0.24 s |
| `sh scripts/check.sh` | 37 to 41 s total, 78 tests, 0 skips |

No new test exceeds 2 s here, so no CI efficiency item is raised beyond noting `complete_mode` is the heaviest control. Estimate of the real lane (not measured): 5 runs over 2.56 million vectors each at the measured 30 to 40 microseconds per vector under sanitizers is about 8 minutes of CPU plus the download.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug in the plan's expectation] no_step is "asserted minus unmoved", not "asserted minus identity"**
- **Found during:** Task 1, first measurement
- **Issue:** The plan and RESEARCH 5.4 say a no-step run fails exactly asserted minus identity, where identity ignores pc. After `lynx__cpu_start_at` the CPU's pc is the initial pc, so any vector whose final pc differs (every NOP, every one-byte instruction) fails on pc even if it is an identity vector. Measured: synertek under lynx1, identity 3,057 yet 0 passes; rockwell under lynx2, identity 2,811 yet 2 passes.
- **Fix:** The runner counts `unmoved` (identity with final pc equal to initial pc) and prints it under a fault; the control requires failed = asserted - unmoved, passed = unmoved, failures above 90 percent of asserted, and every run row failing. The control proves the same thing (the gate exercises the CPU) with a count that is exact.
- **Files modified:** tests/cpu/test_cpu_vectors.c, tests/cpu/vector_controls.sh
- **Commit:** cbb82ac

**2. [Rule 2 - Missing critical functionality] a persisted control for complete mode**
- **Found during:** Task 3
- **Issue:** The full lane's whole-file runner path would otherwise first run in plan 02-11 over 2 GB with no offline evidence it works (AGENTS.md: exact inventory and a meaningful negative control for every gate).
- **Fix:** `cpu.vectors.complete_mode` (and its inventory row, claim hash refreshed). The plan said ten new inventory rows; eleven were added (nine `cpu.vectors.*` controls plus two `policy.vectors_pin*`). `ctest -R '^cpu\.vectors\.'` runs 13 tests (3 positive, reader_malformed from 02-04, 9 controls), not the plan's 11.
- **Files modified:** tests/cpu/vector_controls.sh, tests/cpu/tests.cmake, tests/inventory/fast.txt, docs/claims/skeleton-repeatability.claim
- **Commits:** cbb82ac, 8c867b3

### Recorded design choices (inside Claude's discretion)

- **Partial mode needed no reader change** (`sst_open_sample(..., partial)` came from 02-04); `tests/cpu/sst_reader.c/.h` are unchanged. The runner now uses it: a partial plan lists only the opcodes present, prints `# sample = partial, first 32 objects of each of N files, ...` and `files N` in the totals; a full sample's plan is byte-identical to before (the committed inventories still match).
- **New file `tests/cmake/vectors_pin_controls.cmake`** is not in the plan's file list; the plan allowed "a small CMake driver that loops, following `pins_controls.cmake`".
- **`--results` lists run rows only**, as planned. Failures in the 32 undefined rows under none count in `failed` and the exit code but have no results row; the full lane's none run therefore relies on `failed 0`.
- **Complete mode takes `--dir D --variant V`** and reads `D/V/v1/<op>.json`, exactly the layout `ensure_downloads` creates; the variant is restricted to the two pinned names.
- **Controls place mini-samples under `${LYNX_SCRATCH_DIR}/vectors/vc.*`** (removed on exit) and never write under `fixtures/` (`git status --porcelain fixtures` is empty after the run).

**Total deviations:** 2 auto-fixed, 5 recorded choices. **Impact:** none on the plan's goal.

## Known Stubs

None.

## Threat Flags

None. T-02-11 (repudiation, vacuous gate): mitigated by hashed poison plus the exact failing-set comparison in both directions, shown above to catch a constant-poison regression, and by the no-step control. T-02-12 (tampering, full lane): mitigated in `run` by hashing every download against the pin record before use and requiring both samples to be re-derived byte for byte; not yet exercised on real data. No new network surface: the only network code is the existing `scripts/vectors-full.sh`, outside `tests/**`, `cmake/` and the check (`policy.no_network` passes; `vector_controls.sh` contains neither tool name).

## Notes for the next plan (02-06 onward, and 02-11)

- Runner CLI additions: `run ... [--results F] [--partial] [--fault no-step|stray-write|mutant-69]`; `complete --profile P --dir D --variant V [--results F] [--partial] [--fault ...]` (exit 0 clean, 1 failures, 2 usage, 3 data or parse error, including a missing file without `--partial`). `--print-plan` accepts `--partial`.
- Control cases: `sh tests/cpu/vector_controls.sh CASE ROOT RUNNER SCRATCH` with CASE in `wrong_profile wrong_profile_reverse no_step corrupt_expect malformed inventory_drift sample_identity mutant_profile complete_mode`. Helpers worth reusing: `cut_sample`, `mutate_object`, `corrupt_final`, `plan_set`, `failing_set`, `bitop_set`.
- Plan 02-11 runs `sh scripts/vectors-full.sh pin sample run` (or `run` alone with the committed pin record) by hand. Things to record there: the per-profile results-file SHA-256 values and totals it prints, the full-data failing sets of both cross pairings (whether anything beyond the 32 `$x7/$xF` opcodes differs), the wall time, and whether the cross pairings' failing set on 10,000 vectors still equals the 32 (it is only proven for the 32-object samples). `build/vectors-full/*.results` are untracked build output. The script exits 1 if a cross set differs from the 32, by design.
- Linux check: the new tests passed in an `ubuntu:24.04` container (cmake 3.28.3, shellcheck 0.9.0, both gawk 5.2 and mawk); cmake 3.31 and the full `check.sh` were not run in a container.
- `tests/inventory/fast.txt` now has 78 tests (was 67 at 02-04: plus 9 controls and 2 pin tests); claim `inventory_sha256` is `53ed1c29...3b1a` after the last inventory change.
- Estimate: projected 85,000 tokens, actual 17,653 on the chars/4 scale over the realized diff (70,611 characters including added and removed lines).

## Self-Check: PASSED

- FOUND: tests/cpu/vector_controls.sh, tests/cmake/vectors_pin.cmake, tests/cmake/vectors_pin_controls.cmake, scripts/vectors-full.sh (with a `run)` case and `--profile none` in code).
- FOUND commits cbb82ac, c41eece and 8c867b3 as ancestors of HEAD (`plan_head_before` 61d34ac to `plan_head_after` 8c867b3, 3 commits).
- `sh scripts/check.sh` exits 0 ("100% tests passed out of 78", "78 tests accounted for once each (0 declared skips)", no `check: step` line); `shellcheck -s sh` clean on `tests/cpu/vector_controls.sh` and `scripts/vectors-full.sh` (0.9.0 and 0.11.0); `git status --porcelain fixtures` is empty.
