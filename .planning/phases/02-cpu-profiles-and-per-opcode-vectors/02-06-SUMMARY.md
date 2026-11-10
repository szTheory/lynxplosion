---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 06
subsystem: cpu-scenario-vectors
tags: [scenarios, interrupts, irq-latency, test8, bit-operations, source-tags, negative-controls, c17, cmake]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-01 cycle-stepped CPU, profile tables, flat_bus.h seam; 02-05 vector controls and inventory/claim mechanics"
provides:
  - "lynx-test-cpu-scenarios <lynx1|lynx2> [--fault cb-poll] and --scenario-profile P --run-profile Q: authored per-cycle scenario vectors with a closed source-tag set and an unverified flag"
  - "cpu.scenarios.lynx1, cpu.scenarios.lynx2, cpu.scenarios.test8.self_test, cpu.scenarios.wrong_profile.self_test"
  - "per-profile tag counts printed as a final 'tags:' line"
affects: [02-07, 02-09, 02-10, 02-11]

actuals:
  tokens: 12608
  tasks: 2
  commits: 2

plan_head_before: 51d13417a7e02f866706e5009eb6af8c313731f5
plan_head_after: f317f9163441f8e6ac6f74abb6514caf95cab62a

tech-stack:
  added: []
  patterns:
    - "Scenario rows are data (name, tag, unverified, profile mask, registers, memory, per-cycle line schedule, per-cycle expected list, final state); bit-operation and lynx1 NOP-shape rows are generated from the rule, never from the CPU"
    - "An edge that mixes a model choice with an architectural rule is split into a per-cycle row (SST-MODEL) and a final-state row (WDC or LT-HW), so the stronger tag never covers a dummy address"
    - "A wrong-profile control exits 4 (not 1) if any scenario passes under the wrong profile, so a partly vacuous control cannot satisfy the exact-exit-code check"

key-files:
  created:
    - tests/cpu/test_cpu_scenarios.c
  modified:
    - tests/cpu/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "One source tag per scenario row names what that row's expected values claim; only rows whose claim is a lynx-tests result carry LT-HW and verified"
  - "WAI/STP evidence form: no WAI or STP kind in either profile table (cpu.tables) plus the Test 8 replica showing $CB and $DB as 1-byte, 1-cycle, non-polling NOPs on both profiles"
  - "BBR/BBS 5/6/7 cycle counts on lynx2 are LT-HW but run as count-only rows (SC_COUNT), because the dummy-cycle addresses inside those cycles are SST-MODEL choices"

requirements-completed: [CPU-04, CPU-02]

duration: one session (check.sh 40 s wall)
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 06: Authored scenario vectors and their controls Summary

**The CPU now passes 99 (lynx2) and 78 (lynx1) authored scenarios, every one tagged with its source and an unverified flag: CLI latency and the lynx-tests Test 8 shape are the only interrupt claims tagged as hardware-informed, and both new controls fail for the intended reason. No CPU defect was exposed; `core/cpu.c` is unchanged.**

## Accomplishments

- **Task 1 (commit 39cb18f).** `tests/cpu/test_cpu_scenarios.c`: a scenario runner on `flat_bus.h`. Each row carries a per-cycle expected list of (address, data, read/write, cycle kind), a per-cycle IRQ/NMI line schedule (level from a given log cycle on), initial registers and memory, expected final registers and memory, a source tag and an unverified flag. The runner refuses the whole run (exit 1, `scenario NAME: missing or unknown source tag`) on a missing or unknown tag, on a non-LT-HW row not marked unverified, and on a list whose length differs from the cycle count. Scenarios: CLI latency (INC zp after CLI, the Test 1 shape), SEI with an IRQ pending, PLP delay, RTI immediate, D cleared on entry and restored by RTI (Test 2 shape), BRK frame, an IRQ during BRK waiting for CLI, NMI taken once on a held line, a second NMI edge, NMI over IRQ, the reset seam, the Test 8 replica and its `$EA` control, the whole 32-NOP Test 8 block generated from the rule, `$CB`/`$DB` shape, and reserved NOP lengths (`$02 $44 $54 $5C $DC`). Registered `cpu.scenarios.lynx1` and `cpu.scenarios.lynx2`.
- **Task 2 (commit f317f91).** Generated lynx2 rows: RMBn and SMBn for bits 0 to 7 (the only changed bit, flags unchanged), BBRn and BBSn for bits 0 to 7 taken and not taken (the other seven bits hold the opposite value so only bit n decides), page-crossing BBR/BBS, and BBR 5/6/7 counts. Generated lynx1 rows: all 32 `$x7/$xF` shapes with `$E8` as the operand byte, X preserved. Directed edges with a per-cycle row and a final-state row each: `JMP ($02FF)`, `(zp,X)` pointer wrap, `zp,X` wrap, `(zp)` pointer at `$FF`, stack push at S=`$00` and pull at S=`$FF`, JSR at S=`$00`, branch across a page, `LDA $12FF,X`, index carry into page `$FF`, decimal ADC to `$99`, ADC 99+01, SBC from `$00`. Registered the two controls.

## Source-tag counts (per scenario row; unverified count is rows with the flag)

| Profile | Scenarios | LT-HW | WDC | RKW | CMD | SST-MODEL | INFERRED | unverified |
|---|---|---|---|---|---|---|---|---|
| lynx1 | 78 | 7 | 18 | 0 | 2 | 45 | 6 | 71 |
| lynx2 | 99 | 10 | 18 | 50 | 2 | 13 | 6 | 89 |

The runner prints the last line of each run as `tags: LT-HW n WDC n RKW n CMD n SST-MODEL n INFERRED n unverified n`. The 7 LT-HW rows common to both profiles are: CLI latency, D cleared on entry, RTI restoring D, the Test 8 replica, its `$EA` control, the 32-NOP block, and `JMP ($xxFF)` page-fixed (final state only). The three extra lynx2 LT-HW rows are the BBR/BBS 5, 6 and 7 cycle counts. Every LT-HW row is verified; every other row is unverified.

## WAI and STP evidence form (plan-check I-10)

Neither instruction exists on either profile. The evidence is two parts: (1) no WAI or STP kind in either profile table (`cpu.tables`, 02-01: the kind enum has no such kind and rows `$CB`/`$DB` are checked as NOP, `NOP1`, no-poll on both profiles); (2) the Test 8 replica (`cpu.scenarios.lynx1`, `cpu.scenarios.lynx2`, rows `irq.test8.replica` and `irq.test8.block32`) showing `$CB` and `$DB` executing as 1-byte, 1-cycle, non-polling NOPs on both profiles, plus the `nop.cb.one_byte_one_cycle` and `nop.db.one_byte_one_cycle` rows (CMD table V shape, unverified). The no-poll property is LT-HW; the one-cycle count is a CMD stand-in.

## Controls: expected versus observed

| Control | Fault | Expected | Observed |
|---|---|---|---|
| `cpu.scenarios.test8.self_test` | `--fault cb-poll`: `$CB` row replaced by the `$EA` row in a test-local table copy | exit 1; `irq.test8.replica` and `irq.test8.block32` fail | exit 1 under lynx2 and lynx1; 19 mismatch lines, on exactly those two scenarios plus `nop.cb.one_byte_one_cycle` (its single cycle no longer ends at a boundary). The replica shows the IRQ entry starting after `$CB` (pushed return address `$0204` instead of `$0208`, pc `$8002`, S `$F8`, X 0 instead of 1) |
| `cpu.scenarios.wrong_profile.self_test` | `--scenario-profile lynx2 --run-profile lynx1` | exit 1; every lynx2 bit-operation scenario fails | exit 1; 53 of 53 bit-operation scenarios (48 generated plus 5 fixed) failed, 283 mismatches; the control regex names `bitop.rmb0` and `bitop.bbs7.not_taken`; exit 4 is reserved for "a scenario passed under the wrong profile", so this control cannot be partly vacuous |
| tag enforcement | temporary edit (not committed) of one row's tag to `"WDX"` and to `NULL` | exit 1 naming the scenario | `scenario brk.frame: missing or unknown source tag 'WDX'` and `'(none)'`, `scenarios: 1 data errors, nothing run`, exit 1 each; restored |

The reverse direction (`--scenario-profile lynx1 --run-profile lynx2`) has no bit-operation rows and exits 4 by design.

## Timings (this host, load high)

`cpu.scenarios.*`: 0.07 to 0.09 s each (wall); the four together 0.27 s CPU. `sh scripts/check.sh`: 40 s wall (41 s reported), 82 tests, 0 skips, no `check: step` line. Inventory now 82 tests (was 78); claim `inventory_sha256` is `0c312fd55c0f3cebbc7c5ca197eefcbd836cc186a03671b6fe09aa61bc239769` after the last inventory change (refreshed in the same commit as each of the two inventory changes).

## CPU bugs the scenarios exposed

None. All 99 and 78 scenarios passed on first execution against the 02-01 CPU. The expected values were derived from the cited rules before the first run (the stack, vector and pushed-P values from the entry sequence in RESEARCH 4.4, the latencies from where each instruction writes I), so the pass is not circular; the sensitivity of the suite is shown by the two controls above, not by a CPU fix. `core/cpu.c` was not modified.

## Deviations from Plan

### Auto-fixed Issues

None to the CPU. One note on the plan's wording:

**1. [Rule 2 - evidence honesty] Tags are per scenario row, and mixed claims are split rows**
- **Found during:** Task 2
- **Issue:** The plan asks for one tag per expected value, but a per-cycle list holding a wrap rule (WDC) also holds dummy-cycle addresses (SST-MODEL), and BBR/BBS 5/6/7 counts (LT-HW) sit beside dummy addresses of the same instruction. One tag per row would either overclaim or understate.
- **Fix:** Each row carries one tag naming its claim. Mixed edges are two rows (per-cycle SST-MODEL, final-state WDC; `JMP ($xxFF)` page fix is LT-HW final-state only). The BBR/BBS counts are LT-HW count-only rows next to RKW per-cycle rows. The runner prints counts per row.
- **Files modified:** tests/cpu/test_cpu_scenarios.c
- **Commits:** 39cb18f, f317f91

### Recorded design choices (inside Claude's discretion)

- The runner is one new executable `lynx-test-cpu-scenarios` (the plan allowed sharing `lynx-test-cpu` sources; one executable per file kept the CLI of the control modes clean).
- The wrong-profile control requires every scenario to fail (exit 4 otherwise) rather than only some, an addition beyond the plan's exact-exit-code wording.
- `irq.test8.block32` and the generated rows are built from rules in code; this is derivation from the cited rule, not from running the CPU.
- No scenario asserts the NMOS taken-branch delay or the NMI hijack (RESEARCH A7); no WAI/STP row exists.
- `edge.decimal_*` rows compare N, Z, C and A only (V in decimal mode is unverified and excluded by a mask).

**Total deviations:** 1 auto-fixed note, 5 recorded choices. **Impact:** none on the plan's goal.

## Known Stubs

None.

## Threat Flags

None. T-02-13 (repudiation, overclaim): mitigated by the closed tag set the runner enforces (shown to refuse an unknown and a missing tag), the unverified flag required on every non-LT-HW row, per-tag counts printed per profile, and the row-splitting rule above. No new network, auth or file surface (test-only code; `policy.no_network` still passes).

## Notes for the next plan

- CLI: `lynx-test-cpu-scenarios <lynx1|lynx2> [--fault cb-poll]` (exit 0 pass, 1 mismatch or tag/data error, 2 usage) and `--scenario-profile P --run-profile Q` (bit-operation rows only; exit 4 if any passes). Scenario data is in `k_scen[]` plus generators `gen_bitops` and `gen_lynx1_shapes`; add a row with a tag from `LT-HW WDC RKW CMD SST-MODEL INFERRED` and `.unv = 1` unless it is a lynx-tests result. `.mode` is `SC_FULL` (per-cycle), `SC_COUNT` (instruction cycle count) or `SC_FINAL` (final state only); `.sz = 1` starts with S = 0.
- `sz` was added because S = 0 collides with the power-on default in the data layout.
- Interrupt entry, RTI, PLP, SEI, BRK and reset now have per-cycle evidence; 02-09 and 02-10 can cite `cpu.scenarios.lynx1/2` for the CPU-04 evidence line and name the WAI/STP evidence form above.
- The machine loop (02-07) must keep the poll sampling at the grant stamp so these scenarios stay meaningful; the scenarios use the flat bus, not the machine.
- Estimate: projected 70,000 tokens, actual 12,608 on the chars/4 scale over the realized diff (50,431 characters).

## Self-Check: PASSED

- FOUND: tests/cpu/test_cpu_scenarios.c.
- FOUND commits 39cb18f and f317f91 as ancestors of HEAD (`plan_head_before` 51d1341 to `plan_head_after` f317f91, 2 commits).
- `ctest --preset check -R '^cpu\.scenarios\.'` runs 4 tests, all pass; `sh scripts/check.sh` exits 0 ("82 tests accounted for once each (0 declared skips)", no `check: step` line); `grep -c '"LT-HW"' tests/cpu/test_cpu_scenarios.c` is well above 2; `cpu.scenarios.lynx2<TAB>cpu` is in `tests/inventory/fast.txt`; `sh scripts/check-claims.sh` exits 0.
