---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 08
subsystem: bus-cost-model-and-det-parameters
tags: [cost-model, flat5-placeholder, test-skew, cmake-gate, parameterisation, behaviour-neutral]

requires:
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "02-02 cycle-stepped CPU with LYNX__CK_* cycle kinds; Phase 1 det gates and golden"
provides:
  - "core/bus.h, core/bus.c: pure bus-side pricing, lynx__bus_timing_of(model, region, kind) over const [model][region][kind] tables, lynx__bus_region_of, lynx__bus_cost_model_valid"
  - "Cost model 0 flat5 (all 5 ticks, labelled PLACEHOLDER) and model 1 test_skew (distinct non-zero per-kind costs, internal only)"
  - "cpu.cycles checks the prices, the id validity and the single Phase 2 region"
  - "LYNX_DET_* workload variables at the top of tests/det/tests.cmake, passed to the det scripts as -D parameters"
affects: [02-09, 02-10, 02-11]

actuals:
  tokens: 2900
  tasks: 2
  commits: 2

plan_head_before: f43ce146446162ff161e3c69439f4c152fd427dd
plan_head_after: 00fa9e66a3489e5e7efdb333b60364e81c2e3019

tech-stack:
  added: []
  patterns:
    - "Pricing is bus-side data: the CPU neither includes nor calls the bus header and holds no cost table; the machine loop (02-09) will ask for the price of the request the CPU posted"
    - "Det gates take workload values as -D parameters; the single place to change them is the LYNX_DET_* block in tests/det/tests.cmake"
    - "Content-edit match rule: unanchored, counted with a string(FIND) loop, exactly one occurrence, else sensitivity/manifest_fields fail with 'FROM <value> must occur exactly once in <content>, found <n>'"

key-files:
  created:
    - core/bus.h
    - core/bus.c
  modified:
    - CMakeLists.txt
    - tests/cpu/test_cpu.c
    - tests/det/tests.cmake
    - tests/det/two_runs.cmake
    - tests/det/sensitivity.cmake
    - tests/det/manifest_fields.cmake
    - tests/det/runner_cli.cmake

key-decisions:
  - "Out-of-range indices to lynx__bus_timing_of clamp to index 0 so a table is never read out of bounds; correctness still rests on the caller checking lynx__bus_cost_model_valid first (T-02-16, applied by snapshot restore and the setter in 02-09)"
  - "runner.cli now also asserts ticks_consumed of its halting image (TRAP_TICKS, today 0), so the switch to a CPU-driven machine has to state the new value in tests.cmake rather than slip past an unchecked field"

requirements-completed: [CPU-03]

duration: about 20 min of work, check.sh 38 s wall
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 08: Bus cost model and parameterised det gates Summary

Per-access pricing now exists as a pure, table-driven bus function (flat5 PLACEHOLDER default, skewed internal test model), unit-checked before any machine uses it, and the det gates take their workload-specific values from one block in `tests/det/tests.cmake`, with the golden digest provably untouched.

## What was built

### Task 1: bus cost tables (commit 1f79815)

- `core/bus.h`: `LYNX__COST_FLAT5 = 0`, `LYNX__COST_TEST_SKEW = 1`, `LYNX__COST_MODEL_COUNT = 2`; `LYNX__REGION_RAM = 0`, `LYNX__REGION_COUNT = 1`; `lynx__bus_timing { ticks, sample }`; the three functions.
- `core/bus.c`: `static const uint8_t lynx__cost[model][region][kind]`, `_Static_assert` on the dimensions and on `LYNX__CK_COUNT == 11`. flat5 is all 5 and carries the PLACEHOLDER comment with the Epyx figures that are not modelled (page-mode opcode reads 4, Suzy 9-15, audio DPRAM 5-20, all-5 about 20% slow, no claim cites ticks). test_skew is `{4, 6, 3, 10, 7, 9, 8, 11, 12, 13, 14}`. `sample` is always 0. No float, no writable global, no pointer table, only `stdbool.h`, `stddef.h`, `stdint.h` and project headers.
- `CMakeLists.txt`: `core/bus.c` added to `LYNX_CORE_SOURCES`.
- `tests/cpu/test_cpu.c`: `check_costs()` inside case `cycles` (no new CTest name, no inventory change). It carries its own copy of the skew values, checks flat5 is 5 with sample 0 for every region and kind, test_skew equals the copy and is distinct and non-zero, ids 0 and 1 valid, 2 and 0xFFFFFFFF invalid, and every sampled address maps to the RAM region.
- Red/green: the new checks were written first and failed to link until `bus.c` was in the build. As a control, changing the last test_skew value to 15 made `cpu.cycles` fail at the `s.ticks == skew[k]` CHECK; restored afterwards.

### Task 2: parameterised det gates (commit 00fa9e6)

- `tests/det/tests.cmake` has a `LYNX_DET_*` block ("workload values; plan 02-09 changes only these"): `CONTENT_SHA256`, `FLIP_FROM` `03 0a` and `FLIP_TO` `04 0a`, `TRAP_FROM` `03 0a 11 18`, `TRAP_TO` `03 0a 11 db`, `TRAP_TICKS` 12, `TRAP_STOP` halt, and the runner.cli trio `CLI_TRAP_HEX` db, `CLI_TRAP_TICKS` 0, `CLI_TRAP_STOP` halt.
- `two_runs.cmake`: the tracer hash literal is gone; `-DCONTENT_SHA256` is required and must be lowercase hex.
- `sensitivity.cmake` and `manifest_fields.cmake`: the `\n`-anchored `string(REPLACE)` became an unanchored, counted, exactly-one match with the generalised message `FLIP_FROM '<value>' must occur exactly once in <content>, found <n>` (and the TRAP_FROM equivalent). Each also fails when the replacement leaves the content unchanged. All new parameters are checked non-empty.
- `runner_cli.cmake`: `TRAP_HEX` replaces the literal `db`; the manifest line check now also includes `ticks_consumed`, which is new coverage.
- Match-rule controls run by hand (not committed as gates): FLIP_FROM `zz` fails with "found 0", `a` fails with "found 10"; TRAP_FROM `zz` and `7` fail with the same message shape; an empty `-DFLIP_FROM=` fails with "is required and must not be empty". The real value `03 0a` occurs exactly once in the fixture (the data line starts it; the second line and the comments contain no match).

## Neutrality proof (golden byte-identical)

- `git diff --quiet f43ce14 HEAD -- tests/golden` and `... -- tests/inventory/fast.txt` both succeed: the golden file and the inventory are unchanged.
- The runner on the skeleton fixture (splitmix64, seed 0123456789abcdef, load 0200, 4096 ticks) printed `observation_digest 4e63d100...251d` before the plan and the same value after; it equals the committed `tests/golden/skeleton.digest`. The four `obs.*` evidence files are byte-identical before and after. The bundle digest differs only because `run.manifest` records `core_build`, which changes with the source tree; that is expected and not part of the golden.
- `det.golden`, `det.two_instances`, `det.two_runs`, `det.sensitivity`, `det.manifest_fields`, `runner.cli` all pass with the parameterised values.
- The tracer hash occurs once in `tests/det/tests.cmake` and not in `two_runs.cmake`; the `"db\n"` literal is gone from `runner_cli.cmake`.

## Measured timings

`sh scripts/check.sh`: 38 s wall, 53 s user, 45 s system (host not heavily loaded at the time). `cpu.cycles` 0.51 to 0.54 s wall for the whole case including the new cost checks. `det.*` and `runner.cli` together 1.8 to 1.9 s of test time. The new bus code is a handful of table reads and adds nothing measurable.

## Deviations from Plan

None - plan executed as written. One within-plan decision: `TRAP_TICKS` for runner.cli was measured rather than taken from an existing assertion (the script did not check ticks before); the `db` image at the load address halts with `ticks_consumed = 0` under the provisional skeleton step.

## Self-check notes and test-script hygiene

`sh scripts/check.sh` exit 0 with "100% tests passed out of 84"; no `check: step` failure lines. Both task commits end with the required trailer.

## Known Stubs

None. `sample` is a constant 0 and the single RAM region are Phase 2 scope by design (CONTEXT: page mode, wait states, DMA and refresh go on the bus side in later phases), not stubs that prevent this plan's goal.

## Threat Flags

None. T-02-16 is mitigated at this layer by the validity predicate, the clamped table indices, the static asserts and the unit checks; the callers that must use the predicate (snapshot restore and the internal setter) arrive in 02-09.

## Notes the next plan (02-09) must know

- To switch the det workload, change only the `LYNX_DET_*` block in `tests/det/tests.cmake`: new content hash, a mid-row flip such as `a0 08` to `a0 09` (the match is unanchored and must be unique in the new image), the new trap image with its expected ticks and stop name, and `LYNX_DET_CLI_TRAP_*` including the tick count of the one-instruction image (now asserted). The golden itself still has to be regenerated deliberately in 02-09 with its stated reason.
- The machine calls `lynx__bus_timing_of(model, lynx__bus_region_of(addr), kind)` per granted request; it must call `lynx__bus_cost_model_valid` on any model id from a snapshot or setter before narrowing it to `uint8_t`. `core/cpu*.c` must still not include the bus header.
- `core/bus.c` is in `LYNX_CORE_SOURCES` and passes `core.includes`, `core.no_float`, `core.nm_globals` and `core.nm_undefined`.
- `tests/inventory/fast.txt` and the claim `inventory_sha256` were not touched.
- `.planning/state.json` is an untracked file that predates this plan and was left alone.

## Self-Check: PASSED

- FOUND: core/bus.h, core/bus.c, this SUMMARY
- FOUND commits: 1f79815, 00fa9e6 (ancestors of HEAD)
