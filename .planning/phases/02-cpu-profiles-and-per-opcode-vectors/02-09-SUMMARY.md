---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 09
subsystem: machine-switch-and-phase1-rebaseline
tags: [machine-loop, half-open-budget, undefined-stop, snapshot-format-2, registers, golden-rebaseline, linux-confirmation]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-01 cycle-stepped CPU (lynx__cpu_complete, lynx__cpu_repost, lynx__cpu_state_valid); 02-04..02-06 green vectors and scenarios; 02-07 fixtures sum-trap and churn-loop with hand-derived trace tables; 02-08 bus pricing and the LYNX_DET_* parameter block"
provides:
  - "The shipping machine runs the real CPU: every CPU access is granted, priced by lynx__bus_timing_of(cost_model, region, kind), performed on raw RAM and stamped at its grant tick; guest_ticks is the only clock"
  - "Public API: LYNX_PROFILE_LYNX1/LYNX2, LYNX_STOP_UNDEFINED = 4 (HALT reserved), LYNX_REG_CPU_STATE/CPU_LATCH (LYNX_REG_COUNT 8), lynx_profile_name"
  - "Half-open budget loop with the LYNX__TICK_HEADROOM clamp and the UNDEFINED peek under profile none"
  - "Snapshot format 2: 65,568-byte payload tied to the format number by a static assert, value validation before a non-failing apply that reposts the request"
  - "Internal seam lynx__machine_set_cost_model"
  - "API tests on the CPU programs with hand-derived values; det gates, golden and claim re-baselined on the churn-loop workload"
affects: [02-10, 02-11]

actuals:
  tokens: 26800
  tasks: 3
  commits: 4

plan_head_before: 7624035f05951cf165355c9f0eeafff2aede66d2
plan_head_after: b5f42b8

tech-stack:
  added: []
  patterns:
    - "Machine loop: if inside a cycle, jump to its end or stop at the target; at a boundary whose posted request is an opcode fetch, peek and stop UNDEFINED under none; else grant at guest_ticks, price, perform, complete, set cpu_ready_at = grant + ticks. Only one comparison against the target (the while condition, strict <)"
    - "Snapshot restore builds a candidate lynx__cpu on the stack, judges it with lynx__cpu_state_valid under the instance's profile, then applies field by field and calls lynx__cpu_repost; the pending request is never stored"
    - "API tests pin embedded program bytes to the fixture manifests with SHA-256 and compare the whole RAM against a hand-built expected image"

key-files:
  created: []
  modified:
    - include/lynxplosion.h
    - core/internal.h
    - core/machine.c
    - core/snapshot.c
    - runner/bundle.c
    - tests/header/header.c
    - tests/api/test_api.c
    - tests/det/tests.cmake
    - tests/det/test_det.c
    - tests/det/sensitivity.cmake
    - tests/det/manifest_fields.cmake
    - tests/det/runner_cli.cmake
    - tests/golden/skeleton.digest
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "The in-cycle branch of the budget loop stops at the target instead of clamping and looping again (same semantics). A <= fault in the while condition then fails api.advance_budget on its budget-0 checks instead of hanging until the CTest timeout"
  - "Cross-profile snapshots are refused by the existing header identity check (profile field) as LYNX_ERR_SNAPSHOT_MISMATCH; the payload carries no profile byte"
  - "A cost-model id in a snapshot that is undefined, or defined but not the instance's, is LYNX_ERR_SNAPSHOT_INVALID (a value check after the hash), as are a cpu_ready_at outside [guest_ticks, guest_ticks + 255], nonzero reserved bytes and any CPU state lynx__cpu_state_valid refuses"
  - "lynx__machine_set_cost_model refuses a NULL or deinitialised machine with LYNX_ERR_ARGUMENT as well as an undefined id; reset and load return the model to flat5"
  - "The golden file keeps its Phase 1 name tests/golden/skeleton.digest (no churn in golden.cmake or the claim evidence list); its header names the churn-loop workload"

requirements-completed: [CPU-02, CPU-03, CPU-04]

duration: about 19 min of work; check.sh 38 s wall, 54.8 s user CPU on the dev host; 32 s in the ubuntu:24.04 container
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 09: Machine switch and Phase 1 re-baseline Summary

The machine now runs the 65C02-class CPU through a half-open, tick-stamped bus loop under per-instance profiles none, lynx1 and lynx2. Profile none stops with UNDEFINED before any opcode the two Lynx profiles disagree on. Snapshots use format 2. Every hand-derived program value matched the machine on the first run. The golden was regenerated once, on the churn-loop workload, and gives the same digest on Apple clang and on Linux gcc.

## What was built

- **Public header** (`include/lynxplosion.h`): profile ids 1 and 2; `LYNX_STOP_UNDEFINED = 4`, with HALT documented as reserved; the CPU_STATE (6) and CPU_LATCH (7) register ids with their bit layout; `LYNX_REG_COUNT = 8`; `lynx_profile_name`. The banner, advance, init, reset, loader, `lynx_read_memory` and snapshot comments were rewritten to describe what the code now does.
- **Instance and loop** (`core/internal.h`, `core/machine.c`): the instance holds `lynx__cpu cpu`, `cost_model` and `cpu_ready_at`. The skeleton step, `LYNX__HALT_OPCODE`, `lynx__budget_steps` and the `pc a x y s p halted` fields are gone. Init accepts profiles 0..2 and copies the config. Power-on calls `lynx__cpu_init`, sets flat5 and sets `cpu_ready_at = guest_ticks`. The loader calls `lynx__cpu_start_at`. `lynx_advance` is the RESEARCH 4.5 loop with a strict `<`. `lynx_profile_name` is a switch over string literals.
- **Snapshot format 2** (`core/snapshot.c`): the RESEARCH 4.6 layout, offsets 0-31 followed by RAM at offset 32. The checks run in this order: version (1 and 3 are refused as VERSION), identity including profile (MISMATCH), hash, then value checks: `lynx__cpu_state_valid` on a candidate, reserved bytes zero, cost model defined and equal to the instance's, `cpu_ready_at` in range. The apply step cannot fail and ends with `lynx__cpu_repost`. The "PC inside content" check is removed.
- **Runner**: `profile_text` uses `lynx_profile_name`, and the stop name for UNDEFINED is `undefined`.
- **Tests**: `tests/api/test_api.c` was rewritten as the plan's behaviour list describes (details below). The det workload moved to churn-loop. The golden was regenerated and the claim re-pointed.

## Hand derivation compared with the machine

The expected values come from the fixture trace tables (RESEARCH 7.1 and 7.2, written from datasheet cycle counts before the CPU met them). The table separates those from the extra values I derived from instruction semantics for this plan.

| Check | Source | Expected | Machine |
|---|---|---|---|
| sum-trap, none, RAM zero, large budget | 7.1 | consumed 525, UNDEFINED, PC $021B A $0F X $FF Y $00 S $FD P $34 | same, first run |
| sum-trap RAM (whole 64 KiB compared) | 7.1 | image + $0010=01 $0011=01 $0300=0F $01FC=11 $01FD=0F, rest zero | same |
| sum-trap repeat advance | 7.1 | UNDEFINED, consumed 0, buffer identical | same |
| advance(524) then 1 then n | 7.1 | BUDGET 524 with final registers, CPU_STATE delta 1; BUDGET 1; UNDEFINED 0 | same |
| advance(525) in one call | OQ5 rule | BUDGET 525, then UNDEFINED 0 | same |
| advance(520) | 7.1 (+ P mine) | A 0, S $FC, PC $021B; P $36 (Z from LDA #$00, my derivation); next call consumes 5 and stops UNDEFINED | same |
| churn-loop after 325, RAM zero | 7.2 | PC $020C X 1 Y 8 A 0 S $FD P $34; RAM image + $0041=05 $01FC=02 $01FB=1A, rest zero | same |
| churn-loop after 70 | mine | PC $020C, A $05, X 0, Y 8, S $FD, P $34 (last flag write LDA #$05) | same |
| churn-loop after 70 + 255k, k = 1..20 | 7.2 | PC $020C, X = k (Y 8, S $FD) | same |
| golden workload end (seeded RAM, 4096) | 7.2 | ends inside PLA of iteration 16 after 201 ticks: PC $021C, X $0F, Y $08, S $FC | runner regs: PC 021C A BB X 0F Y 08 S FC P F4, CPU_STATE 0x04680200 (opcode $68, 4 ticks left) |

No disagreement came up, so no derivation or CPU bug had to be resolved. A and P at the golden point depend on seeded RAM and were not hand-derived. RESEARCH 7.2 allows a C re-implementation of the loop arithmetic for them, and that was not written.

Embedded arrays: `sum_trap` and `churn_loop` in `test_api.c` are checked against `6ae9bada...0f33` and `dcee7216...5b77` (the manifest values) by `api.skeleton`.

## Golden re-baseline and Linux confirmation

- Old golden (Phase 1 skeleton): `4e63d10000e55c643268a601dc47770d6b40af24c02c1f95832b7352c14b251d`.
- New golden (churn-loop under none, splitmix64 seed 0x0123456789abcdef, 4096 ticks): `506f79956f05bbb58bb2c6e759046836e5b40de0f499bb6af52bcf288e2bad3c`.
  - Apple clang (AppleClang 21.0.0, Darwin arm64): `506f7995...2bad3c`.
  - Linux gcc 13.3.0 (ubuntu:24.04 container, aarch64, cmake 3.28.3, shellcheck 0.9.0), run on a clone of commit `b5f42b8`: the same `506f7995...2bad3c`. `scripts/check.sh` passed in the container with "100% tests passed, 0 tests failed out of 84"; `det.golden` is one of the 84.
  - The sum-trap run's observation digest `48c967591f74...3265` is also equal on both hosts.
  - The CI run in 02-11 (x86_64) is the next confirmation, on a third architecture.
- Re-baseline order followed: the vectors were already green from 02-04..02-06, the hand-derived API checks went green in Task 2 (commit `48e903d`), and only then was the golden regenerated in Task 3 (commit `55b0154`, whose message body begins with "re-baseline: ...").

## One-time evidence: `<=` mutation (CPU-03, plan-check required)

- **Seeded:** `while (m->guest_ticks < target)` became `while (m->guest_ticks <= target)` in `core/machine.c`, `lynx_advance`.
- **Result:** `ctest -R '^api\.advance_budget$'` printed `***Failed` and "0% tests passed, 1 tests failed out of 1". The test printed 4 failure lines, each for the intended reason, which is a cycle started at the budget target:
  - `test_api.c:701`: a budget of 0 on the freshly loaded machine changed the buffer.
  - `test_api.c:719`: a budget of 0 with the operand cycle due exactly at the target changed the buffer.
  - `test_api.c:740`: "split 600 + 0 differs from one 600-tick call".
  - `test_api.c:791`: a budget of 0 near the limit changed the buffer.
- **Reverted:** after the revert (`grep -c 'guest_ticks < target' core/machine.c` = 1), all 15 `api.*` tests passed. No mutant is kept in the tree.

## Measured timings (dev host, loaded; CPU seconds where available)

- `sh scripts/check.sh`: 38 s wall, 54.8 s user + 43.3 s system ("check: total 38s"). Plan 02-08 recorded about 38 s wall, so there is no regression.
- Slowest new test: `api.snapshot_refusal` at 0.89 s user. It restores the header-flip loop and the cross-profile pairs, about 1,100 restores with SHA-256 over 64 KiB each. All other `api.*` cases take 0.00-0.03 s user.
- Container (`ubuntu:24.04`): "check: total 32s".

## Task Commits

1. **Task 1: core switch** - `5017810` (feat). Not green alone, by design: `test_api.c` and `test_det.c` still used the removed items.
2. **Task 2: API tests on the CPU programs** - `48e903d` (test). Also contains the loop restructure (deviation 1).
3. **Task 3: det values, golden, claim** - `55b0154` (test, with the re-baseline reason).
4. **Linux fix found by the container run** - `b5f42b8` (fix, deviation 2).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - make a fault visible] The `<=` fault hung instead of failing**
- **Found during:** Task 2, while planning the one-time mutation.
- **Issue:** As first written (`guest_ticks = min(target, cpu_ready_at); continue;`), a `<=` fault loops forever once `guest_ticks == target` with a cycle in flight. The test would then fail only on the CTest timeout, not on an assertion.
- **Fix:** The in-cycle branch now stops at the target (`if (cpu_ready_at >= target) { guest_ticks = target; break; }`) and otherwise moves to `cpu_ready_at`. The semantics are unchanged, and all hand-derived values hold. With the fault, the failures are the assertions listed above.
- **Files modified:** core/machine.c
- **Commit:** 48e903d

**2. [Rule 3 - Blocking, pre-existing from 02-08] `det.sensitivity` and `det.manifest_fields` failed on Linux**
- **Found during:** Task 3, in the ubuntu:24.04 container run.
- **Issue:** Both scripts count occurrences with `while(TRUE)` (added in 02-08). In script mode, CMake 3.28 starts with old policies, so CMP0012 and CMP0130 make `TRUE` an unset variable. The loop never ran and the scripts reported "must occur exactly once ..., found 0". The dev host's CMake 4.x hides this. CI had not run since 02-08.
- **Fix:** Added `cmake_minimum_required(VERSION 3.25)` and `cmake_policy(VERSION 3.25)` at the top of both scripts (the Phase 1 lesson). The container check then passed, 84/84.
- **Files modified:** tests/det/sensitivity.cmake, tests/det/manifest_fields.cmake
- **Commit:** b5f42b8

### Small choices inside the plan's latitude

- **Trap match string.** `LYNX_DET_TRAP_FROM` is `a2 00 a0 08` → `ea 07 a0 08` rather than the bare `a2 00`. The effect is the same (the leading `a2 00` becomes `ea 07`), and the match is more clearly unique.
- **Stale comments.** The comments in `tests/det/runner_cli.cmake` and `tests/det/manifest_fields.cmake` that mentioned the halt opcode were updated.
- **Budget equal to the program length.** RESEARCH 7.1 says "budget of at least 525 gives UNDEFINED". Under the locked OQ5 rule, exactly 525 gives BUDGET and the next call gives UNDEFINED. The tests assert that, and use 100000 for the "consumed 525, UNDEFINED" case.

**Total deviations:** 2 auto-fixed, 3 small choices. **Impact:** none on the plan's goal; deviation 2 fixed a latent CI failure that 02-11's push would otherwise have hit.

## Known Stubs

None. Interrupt lines are passed as 0 and the bus `sample` offset is 0: that is Phase 2 scope (Phase 4 adds real IRQ sources), documented in the code.

## Threat Flags

None beyond the plan's threat model.
- **T-02-17:** restore runs version, identity (including profile), hash and value checks before the single apply. The refusal tests assert a byte-identical target.
- **T-02-18:** the headroom clamp is tested at `UINT64_MAX - 5` and at 7 ticks of room. A maximum-budget run stops on a trap image.
- **T-02-19:** profiles 3 and 0xFFFFFFFF are refused with the buffer untouched, and a caller mutating the config after init has no effect.

## Notes the next plan (02-10) must know

- The API case is still named `skeleton`; 02-10 renames it to `cpu_program` (tests.cmake, fast.txt and the claim inventory hash in one commit). `tests/inventory/fast.txt` was not changed here: 84 tests, and the claim's `inventory_sha256` is still `b4f3dd3f...c9fa`.
- `runner/main.c` has no `--profile` yet (02-10). The runner always uses profile none, and an UNDEFINED stop already writes `stop_reason = undefined` and exits 0.
- Reusable helpers in `test_api.c`:
  - `snap_instance_p(mem, profile, seed, image, len, addr)`
  - `cpu_state_word(flags, step, opcode, delta)`
  - `VALUE_REFUSED(m, src_blob, payload_off, value, nbytes, ref)` (recomputes the payload hash)
  - `CHECK_REGS`, `check_ram` with `want_ram`, `load_at`, `advance_ticks`, `ADVANCE_EXPECT`
  - `wrap_image`, `trap_image`, `sum_trap`, `churn_loop`
  - `PAY_*` offsets of format 2
- Register encoding as implemented: `CPU_STATE = flags | step<<8 | opcode<<16 | (cpu_ready_at - guest_ticks)<<24`, `CPU_LATCH = ea | tmp<<16 | cost_model<<24`. P always reads with bits 4 and 5 set.
- In the tracer divergence values for 02-10 (RESEARCH 7.3), none stops at $0204 after 40 ticks. `test_det.c` flips only `cpu.a`, `guest_ticks` and one RAM byte; 02-10 adds flips of the other fields.
- Latent CI risk: these `-P` scripts still set no policy, which is harmless only while they use no `while(TRUE)`, `IN_LIST` or similar construct:
  - `tests/det/golden.cmake`, `golden_self_test.cmake`, `runner_cli.cmake`, `two_runs.cmake`
  - `tests/cmake/docs_policy.cmake`, `expect_fail.cmake`, `inventory_controls.cmake`, `no_skip.cmake`, `pins_controls.cmake`, `provenance.cmake`
- `.planning/state.json` was untracked before this plan and was left alone.

## Self-Check: PASSED

- FOUND: include/lynxplosion.h (`LYNX_STOP_UNDEFINED = 4`, `LYNX_REG_COUNT = 8`), core/machine.c (`lynx__bus_timing_of`, `lynx__cpu_complete`), core/snapshot.c (`lynx__cpu_state_valid`, `lynx__cpu_repost`), runner/bundle.c (`lynx_profile_name`), tests/golden/skeleton.digest (`churn-loop`), claim (`dcee7216...`).
- FOUND commits 5017810, 48e903d, 55b0154, b5f42b8 as ancestors of HEAD. `git rev-list --count 7624035..b5f42b8` = 4. Every commit ends with the required trailer.
- `! grep -nE 'LYNX__HALT_OPCODE|lynx__skeleton_step|lynx__budget_steps|halted' core/internal.h core/machine.c core/snapshot.c` succeeds. `test_api.c` holds both manifest hashes and no `lynx__budget_steps` or `LYNX__TICKS_PER_STEP`. `tests/det/tests.cmake` has no `LYNX_SKELETON_`.
- `sh scripts/check.sh` exits 0 on the dev host ("100% tests passed out of 84", "84 tests accounted for once each (0 declared skips)", no `check: step` line) and in ubuntu:24.04.
