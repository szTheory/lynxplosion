---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 10
subsystem: public-path-cpu-evidence
tags: [runner-profile-flag, profile-select, profile-divergence, cost-model, slicing, seam-agreement, det-profile-agreement, state-sensitivity]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-09 machine loop, snapshot format 2, internal lynx__machine_set_cost_model, hand-derived program fixtures; 02-04..02-06 vector runner, scenarios and per-profile inventories; 02-08 flat5 and test_skew bus pricing"
provides:
  - "lynx-run --profile none|lynx1|lynx2 (default none; any other value exits 2 with the usage line) and a stderr note on an UNDEFINED stop that names the opcode, the PC and the flag"
  - "api.cpu_program (renamed from api.skeleton), api.profile_select, cpu.profile_divergence"
  - "api.cost_model and its persistent control api.cost_model.self_test, api.advance_slicing, api.seam_agreement"
  - "det.profile_agreement and a det.sensitivity_state that flips every CPU field, the cost model, guest time and a RAM byte"
affects: [02-11]

actuals:
  tokens: 12900
  tasks: 3
  commits: 3

plan_head_before: 2e1ac0b30212a775c90308ed67e9d03b30d0ab7c
plan_head_after: 77e6ed09fedc213a8a429b53603d94e1ca1df7b6

tech-stack:
  added: []
  patterns:
    - "Public-path tests compute expected values from the opcode bits and the datasheet cycle counts inside the test (slot_defined, slot_result, slot_taken, a copy of the test_skew prices), never from the core's tables"
    - "A persistent control is the same test case run with a named fault argument through expect_fail.cmake (cost_model fault-constant)"
    - "seam-agreement builds a format-2 payload on a saved snapshot, recomputes the payload hash with the core SHA-256 and restores it through lynx_snapshot_restore"

key-files:
  created:
    - tests/cpu/seam_agreement.cmake
  modified:
    - runner/main.c
    - tests/det/runner_cli.cmake
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/golden/skeleton.digest
    - tests/cpu/test_cpu_vectors.c
    - tests/cpu/tests.cmake
    - tests/det/test_det.c
    - tests/det/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "The UNDEFINED note is built in the runner from public reads only (PC, the raw RAM byte at PC, the identity's profile name); the core holds no message text"
  - "api.seam_agreement runs through a small -P script (tests/cpu/seam_agreement.cmake) so one test covers none, lynx1 and lynx2 with the right sample per profile"
  - "The cost-model fault control leaves the machine on flat5 in the test (the plan's wording); the core itself is not mutated by a persistent control"
  - "det.sensitivity_state asserts every flip leaves a state lynx__cpu_state_valid accepts and that no two flips give the same digest"

patterns-established:
  - "The bit-op slot rule (low nibble 7 or F; bits 4-6 the bit number; bit 7 chooses RMB/BBR against SMB/BBS) is stated once in test_api.c and used by profile_select, profile_divergence and cost_model"

requirements-completed: [CPU-02, CPU-03, CPU-05]

duration: about 1 h 15 min of work; check.sh 47 s wall on the dev host
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 10: Public-path profile, cost-model and agreement tests Summary

The runner now takes `--profile`, and the public path proves what hosts rely on: profile selection per instance, bit-op behaviour per profile, a non-default per-access cost that changes ticks without changing results, exact slicing, and agreement between the vector seam and the shipping machine for every opcode of every profile. The golden is unchanged (`506f7995...2bad3c`, now also checked under lynx1 and lynx2).

## What was built

- **Runner** (`runner/main.c`): `--profile none|lynx1|lynx2`, default `none`, closed set (anything else, an empty value or a repeat exits 2 with the usage line). An UNDEFINED stop still writes the bundle and exits 0, and prints `lynx-run: stopped at undefined opcode $07 at PC $0200 under profile none; the two Lynx profiles disagree here; select one with --profile lynx1 or --profile lynx2`. `runner.cli` covers the flag, the manifest `profile = ` line for all three values, the default, and the note (and its absence under lynx2).
- **api.cpu_program**: the old `skeleton` case renamed, golden header comment gains `--profile none` (digest line unchanged).
- **api.profile_select**: profile names and reset keeping the profile for 0..2; the plan's named image (`a9 ff 85 10 07 10 ...`): lynx2 leaves `$10 = $FE`, lynx1 `$FF` and the next instruction at `$0206`; then all 32 slots under all three profiles with four cell values each (all set, all clear, only bit n, all but bit n). Under none each stops at `$0204` after 25 ticks, repeats with consumed 0 and an unchanged buffer. Under lynx1 and lynx2 marker code shows the instruction length and the branch direction. Under lynx2 the slot also runs for exactly its Rockwell cycle count (RMB/SMB 5, BBR/BBS 5, plus 1 taken) and stops with PC at the next byte (or the target).
- **cpu.profile_divergence** (case `profile_divergence`, executable `lynx-test-api`): the retained tracer: none stops at `$0204` after 40 ticks, lynx1 ends at `$0207` after 60, lynx2 at `$0234` after 70 (computed from the image bytes in the test). Under none each of the 32 bit-op opcodes placed first stops with consumed 0 and an unchanged buffer; the other 224 run 100 ticks.
- **api.cost_model** and **api.cost_model.self_test**: sum-trap under flat5 and test_skew: equal registers (including CPU_STATE) and RAM, the tick difference equals the seam-log sum of (price - 5) from the test's own copy of the prices (525 against 597), the seam's logged writes replayed onto the initial RAM equal the machine's RAM, `lynx__machine_set_cost_model` refuses 2, `0xFFFFFFFF` and NULL with the buffer unchanged. The self-test runs `cost_model fault-constant` (machine left on flat5) and requires exit 1 and `cost model ignored`.
- **api.advance_slicing**: churn-loop with seeded RAM under both models: one 4096-tick call against chunks of 1, 3, 7, 64, 255, 512 and every two-chunk split from 1 to 600 (whole-instance memcmp); one-tick stepping for 600 ticks, where each grant is seen as the CPU_STATE delta restarting at the cycle price (delta + 1 = price from the seam log kinds) and `g[i+1] = g[i] + price[i]`.
- **api.seam_agreement** (`seam-agreement` subcommand in `test_cpu_vectors.c`, run by `tests/cpu/seam_agreement.cmake`): per profile, the first vector of every run row (254 under lynx1 and lynx2, 222 under none) restored through a recomputed format-2 snapshot, advanced by the seam's cycle count x 5, compared on PC, A, X, Y, S, P, CPU_STATE, CPU_LATCH, guest time and the whole 64 KiB RAM. Under none all 1,024 vectors of the 32 undefined rows stop UNDEFINED with consumed 0 and a byte-identical instance. The counts are checked against the profile table's classification so a silent zero cannot pass.
- **det.profile_agreement**: churn-loop, seeded RAM, 4096 ticks under all three profiles: identical eight registers, RAM, guest time and observation digest, each equal to the committed golden. The manifest profile line differs by design and is excluded (stated in the test).
- **det.sensitivity_state**: 15 flips (pc, a, x, y, s, p carry, latch flags, step, opcode, ea, tmp, cpu_ready_at, cost model, guest time, one RAM byte); each leaves a reachable state, each changes the digest, none collides with another.

## One-time evidence (seeded, then reverted; no mutant is kept)

| Fault seeded | Result |
|---|---|
| `core/cpu.c`: the two operand slots of `ABS_R` swapped | `cpu.cycles` failed, as did `cpu.vectors.lynx1/lynx2/none` and both wrong-profile controls (the swap also changes the address) |
| `core/machine.c`: price lookup with model argument fixed to 0 | `api.cost_model` and `api.advance_slicing` failed |
| `core/machine.c`: writes to the stack page dropped | `api.seam_agreement` and `api.cost_model` failed |
| `runner/bundle.c`: observation reads 6 registers instead of 8 | `det.sensitivity_state` printed "digest unchanged" for exactly the seven new-state flips (latch flags, step, opcode, ea, tmp, cpu_ready_at, cost model); the golden gates failed too |

After each revert `git status --short core runner` was clean and all affected tests passed again.

## Hand derivation compared with the machine

| Check | Source | Expected | Machine |
|---|---|---|---|
| tracer under none | RESEARCH 7.3 (3-byte offset arithmetic done in the test) | UNDEFINED, 40 ticks, PC `$0204`, P `$36` | same, first run |
| tracer under lynx1 / lynx2 | 7.3 | PC `$0207` after 60 ticks / `$0234` after 70 | same, first run |
| lynx2 slot cycles | Rockwell datasheet counts (RMB/SMB 5, BBR/BBS 5 and 6 taken) | PC after exactly 50 or 55 ticks | same for all 16 lynx2 slots x 4 cell values |
| sum-trap under test_skew | test's own price copy over the seam log | 597 ticks | same |

The only failure while writing the tests was in the test itself (below).

## Measured timings

- `sh scripts/check.sh`: 47 s wall on the dev host (02-09: 38 s), 91 tests. The increase is almost all `api.seam_agreement`.
- `api.seam_agreement`: about 10 s serial (user CPU: none 7.0 s, lynx1 and lynx2 about 1.6 s each). The 1,024 undefined-row restores under none dominate: each is two 64 KiB SHA-256 passes plus copies under ASan and UBSan at -O0, about 6 ms per vector. It runs beside other tests (4 jobs) so it adds about the difference between 38 s and 47 s to the critical path. If this matters, split it per profile into three tests (the none one first) or sample fewer undefined vectors; neither was done because the plan names one test and all 1,024 vectors.
- Other new tests: `api.advance_slicing` 0.8 s, `api.cost_model` 0.07 s, `api.profile_select` 0.09 s, `cpu.profile_divergence` 0.08 s, `det.profile_agreement` 0.08 s, `det.sensitivity_state` 0.13 s.
- ubuntu:24.04 container (gcc 13, fresh clone of `77e6ed0`, includes the cc65 build): "100% tests passed, 0 tests failed out of 91", `check: total 77s`; the runner there prints the UNDEFINED note for sum-trap (`ticks_consumed = 525`, `stop_reason = undefined`).

## Task Commits

1. **Task 1: runner flag, note, rename, profile_select, profile_divergence** - `66fa096` (feat)
2. **Task 2: cost model, slicing, seam agreement** - `405577d` (test)
3. **Task 3: det.profile_agreement, full state sensitivity** - `77e6ed0` (test)

Every commit that edits `tests/inventory/fast.txt` also refreshes the claim `inventory_sha256` (now `9fb8502745815cbf67ae32ab497dabfdb4243c0608309c06423d7dff1325e0f3` after Task 1; each later commit refreshed it again, the final value is whatever `sh scripts/check-claims.sh --inventory-sha256` prints).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - test premise] The "224 others run 100 ticks" sweep first used operand bytes of zero**
- **Found during:** Task 1, `cpu.profile_divergence`.
- **Issue:** with image `[op, 0, 0]`, `DEC $00` ($C6, $CE, $D6, $DE) legitimately turned the BRK byte at `$0000` into `$FF`, a bit-op slot, so the machine stopped UNDEFINED after 60 to 70 ticks. The machine was right; the test's assumption was wrong.
- **Fix:** the operand byte is `$80`, pointing zero-page and absolute operands at `$0080`, and the comment says why.
- **Files modified:** tests/api/test_api.c
- **Commit:** 66fa096

### Small choices inside the plan's latitude

- `api.seam_agreement` runs via `tests/cpu/seam_agreement.cmake` (sets `cmake_minimum_required` and `cmake_policy(VERSION 3.25)`) rather than inline in `tests.cmake`.
- `api.cost_model.self_test` is registered with `-P ${PROJECT_SOURCE_DIR}/tests/cmake/expect_fail.cmake`: `LYNX_TEST_SCRIPTS` is only defined by gate files included after `api/tests.cmake`.
- `profile_select` also checks lynx2's exact tick counts for every slot, and `det.sensitivity_state` checks flip reachability and pairwise distinct digests; both go beyond the plan's list.
- `api.seam_agreement` skips `$CB` and `$DB` (excluded rows, as in the vector runs).

**Total deviations:** 1 auto-fixed (test premise), 4 small choices. **Impact:** none on the plan's goal.

## Known Stubs

None.

## Threat Flags

None beyond the plan's threat model.
- **T-02-20:** `--profile` accepts a closed set; `foo`, `LYNX1`, an empty value and a repeated flag exit 2 with the usage line, no profile is a default (runner_cli cases).
- **T-02-21:** `api.seam_agreement` ties the vector seam to the shipping machine for 254, 254 and 222 opcodes plus 1,024 undefined-row vectors; the stack-write mutation above shows it fails when the machine path diverges.

## Notes the next plan (02-11) must know

- `tests/inventory/fast.txt` now has 91 tests. Rows added in this plan: `api.advance_slicing`, `api.cost_model`, `api.cost_model.self_test`, `api.cpu_program` (replaces `api.skeleton`), `api.profile_select`, `api.seam_agreement`, `cpu.profile_divergence`, `det.profile_agreement`.
- The golden file and the claim's evidence are unchanged apart from the inventory hash; the golden is `506f79956f05bbb58bb2c6e759046836e5b40de0f499bb6af52bcf288e2bad3c` and `det.profile_agreement` now ties it to all three profiles.
- `lynx-run` has `--profile`; for docs and README: the default is `none` and neither Lynx profile is ever a default or named in a claim.
- Wall time of `scripts/check.sh` is 47 s here; `api.seam_agreement` is the longest single test (about 10 s). CI timing for the critical path should be read with that in mind.
- `.planning/state.json` was untracked before this plan and was left alone.
- The Linux container run took 77 s on a fresh clone (the cc65 build is included); the same checks passed 91/91.

## Self-Check: PASSED

- FOUND: runner/main.c (`--profile`, `undefined_note`), tests/cpu/seam_agreement.cmake, tests/api/test_api.c (`case_profile_select`, `case_profile_divergence`, `case_cost_model`, `case_advance_slicing`), tests/cpu/test_cpu_vectors.c (`cmd_seam_agreement`), tests/det/test_det.c (`case_profile_agreement`).
- FOUND commits 66fa096, 405577d, 77e6ed0 as ancestors of HEAD; `git rev-list --count 2e1ac0b..77e6ed0` = 3. Each ends with the required trailer.
- `grep -P '^api\.cpu_program\tapi$' tests/inventory/fast.txt` matches and `api.skeleton` is gone. `sh scripts/check-claims.sh` exits 0.
- `sh scripts/check.sh` exits 0 ("100% tests passed out of 91", "91 tests accounted for once each (0 declared skips)", no `check: step` line) on the dev host and in ubuntu:24.04.
