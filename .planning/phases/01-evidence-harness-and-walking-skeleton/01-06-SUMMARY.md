---
phase: 01-evidence-harness-and-walking-skeleton
plan: 06
subsystem: host-api-contract
tags: [c17, api, refusal-oracle, memcmp, reset, advance-budget, splitmix64]
status: complete

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plans 01-01/01-02 public header, machine.c, identity; plan 01-04 exact inventory; plan 01-05 header signature table and ABI gates"
provides:
  - "lynx_reset: returns an instance to a buffer byte-identical to a fresh lynx_machine_init (one shared power-on path)"
  - "Six API contract tests (init_refusal, instances_independent, load_refusal_unchanged, inspect_pure, advance_budget, reset_power_on) proving every refusal by whole-buffer memcmp"
  - "Inventory of 27 tests"
affects: [01-07, 01-08, 01-11, 01-13]

requirements-completed: [API-01, API-02, API-03, API-04, API-05]

actuals:
  tokens: 9500
  tasks: 2
  commits: 2

plan_head_before: 6542f7b31eebb195342ce7e68f093b02e9625bf9
plan_head_after: 0d4d314ebf4443dbacf06495e1e8e8ea97036dd4
commits: 2

tech-stack:
  added: []
  patterns:
    - "Refusal oracle: copy the whole instance buffer, call, memcmp against the copy; each family has an accepted-call control proving the comparison can see a change"
    - "White-box tests may poke struct lynx_machine (guest_ticks near UINT64_MAX) through core/internal.h"
    - "init and reset share machine_setup: copy cfg, memset the whole struct, set magic, lynx__power_on"

key-files:
  created: []
  modified:
    - include/lynxplosion.h
    - core/machine.c
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/header/header.c
    - tests/inventory/fast.txt

key-decisions:
  - "lynx_reset on a deinitialised machine returns LYNX_ERR_STATE and does not revive it; NULL returns LYNX_ERR_ARGUMENT"
  - "Reset clears loaded content (no content, guest time 0); the host reloads. Recorded in the header comment as a Phase 1 choice"
  - "Guest-time wrap is proven through the public lynx_advance by setting guest_ticks to UINT64_MAX-5 in the white-box test, because a non-halting image cannot run to a UINT64_MAX budget"

patterns-established:
  - "Later inspect functions must join api.inspect_pure; later refusing calls must join the matching whole-buffer case"

coverage:
  - id: D1
    description: "lynx_machine_init refuses NULL pointers, undersized, misaligned, wrong struct_size, non-NONE profile, unknown RAM policy and non-zero reserved, writing nothing to *out or the buffer"
    requirement: API-01
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#init_refusal (ctest api.init_refusal)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Two instances in separate buffers are independent across advance, load, reset and deinit; init twice is byte-identical; deinit twice and deinit NULL are no-ops"
    requirement: API-01
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#instances_independent (ctest api.instances_independent)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Every refused lynx_load_ram_image leaves the whole instance buffer byte-identical; accepted loads (including exactly to 0x10000 and the full 65536 bytes) do change it"
    requirement: API-02
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#load_refusal_unchanged (ctest api.load_refusal_unchanged)"
        status: pass
    human_judgment: false
  - id: D4
    description: "lynx_advance never consumes more than its budget; sub-step budgets consume 0; halt stops before budget; 0x10000-ending image wraps to its start; guest time never wraps"
    requirement: API-03
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#advance_budget (ctest api.advance_budget)"
        status: pass
    human_judgment: false
  - id: D5
    description: "lynx_reset equals a fresh init byte for byte; default policy is fixed 0x00; SplitMix64 policy matches an independent reference and reports policy and seed"
    requirement: API-04
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#reset_power_on (ctest api.reset_power_on)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Register, memory, guest-time and identity reads (including RANGE and ARGUMENT refusals) leave the instance buffer byte-identical"
    requirement: API-05
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#inspect_pure (ctest api.inspect_pure)"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-11-09
---

# Phase 1 Plan 06: Host API contract Summary

**`lynx_reset` plus six contract tests that prove every host-API refusal and every inspect call by a whole-buffer `memcmp` of the caller-owned instance, with bounded-advance boundaries and SplitMix64 power-on checked against an independent reference.**

## Performance

- **Duration:** about 4 min (first read 18:11 to last commit 18:14, local time; no start timestamp was captured, so this is read from commit times)
- **Tasks:** 2 of 2
- **Files:** 0 created, 6 modified
- **Full check:** `sh scripts/check.sh` exits 0 in 3 s total (identities 0 s, shellcheck 0 s, workflow 3 s, junit 0 s); 27 tests passed, junit accounts for 27 once each.

## Accomplishments

- `lynx_reset` declared in `include/lynxplosion.h` and defined in `core/machine.c`. Init and reset share one static `machine_setup` (copy the config out first because reset passes `&m->cfg`, `memset` the whole struct, set magic, call `lynx__power_on`). The result is byte-identical to a fresh init, so padding can never carry host bytes (T-01-10).
- `api.init_refusal`: ten refusals (NULL mem, cfg, out; size-1; size 0; misaligned with room; misaligned and short; four bad config fields), each with `*out` still the sentinel and both pre-filled `0xA5` buffers untouched, plus a positive control.
- `api.instances_independent`: A (seed 1) and B (seed 2) in separate buffers; advance, load, reset and deinit of A leave B's whole buffer equal to its snapshot and B still advances. Init twice over `0xA5` and `0x5A` garbage gives identical bytes; double deinit and `deinit(NULL)` change nothing.
- `api.load_refusal_unchanged`: NULL bytes, length 0, length 65537, `SIZE_MAX`, address `0x10000` and `0xFFFFFFFF`, `(0xFFF0,17)`, `(0xFFFF,2)` and NULL machine, then a deinitialised machine, all unchanged; accepted loads at `(0xFFF0,16)`, `(0,65536)` and `(0xFFFF,1)` succeed and change the buffer.
- `api.inspect_pure`: all six registers, id 6 and `0xFFFFFFFF` (RANGE), whole-space memory read, `(0xFFFF,2)`, `(0x10000,1)`, over-long and `SIZE_MAX` lengths (RANGE, out untouched), len 0 (ARGUMENT), guest ticks and identity; buffer and guest ticks unchanged; a deinitialised machine refuses every read with STATE.
- `api.advance_budget`: budgets 0 to 3 consume 0; 4 gives 4; 5 gives 4; 4096 gives 4096; ticks equal the sum. Halt image (`0xDB` at offset 3) gives 12 and HALT for `UINT64_MAX`, then 0 and HALT. The 16-byte image at `0xFFF0` keeps PC in `0xFFF0..0xFFFF` on every one of sixteen 4-tick calls and returns to `0xFFF0` at 64 ticks. Guest ticks set to `UINT64_MAX-5` gives one 4-tick step then none, never wrapping. `lynx__budget_steps` checked at `(UINT64_MAX-5, UINT64_MAX)=1`, `(0,7)=1`, `(0,3)=0`, `(UINT64_MAX,100)=0`.
- `api.reset_power_on`: default is ZERO policy seed 0 with all RAM `0x00` after init and after reset; reset after load and advance equals a fresh init byte for byte; SplitMix64 RAM matches four blocks of an independent in-test reference; seed S+1 gives different RAM; identity reports policy 1, seed S and `has_content` 0 after reset; reset of NULL (ARGUMENT) and of a deinitialised machine (STATE, no change).
- `tests/header/header.c` signature table gained `lynx_reset`; `api.abi_symbols` and `api.header_selfcontained` still pass.

## Task Commits

1. **Task 1: init refusal, load refusal, instance independence, inspect purity** - `476e9eb` (test)
2. **Task 2: lynx_reset and bounded advance / reset tests** - `0d4d314` (feat)

TDD note: Task 1 tests passed on the first run against the plan 01-01/01-02 core, so no core gap surfaced. To confirm the oracle is not vacuous I temporarily moved `lynx__power_on(m)` ahead of the argument checks in `lynx_load_ram_image`; `api.load_refusal_unchanged` failed with "instance buffer changed" lines, then I restored the file byte-for-byte (scratch copy, `git status` clean for `core/machine.c` before Task 2). Task 2's `lynx_reset` did not exist before the commit, so the new tests were written against it as one change per the plan's two-commit layout.

## Deviations from Plan

None - plan executed exactly as written, with two clarifications:

- The plan's misalignment recipe ("pass `buffer + 1` with `size - 1`") yields `LYNX_ERR_SIZE`, because the size check precedes the alignment check. `init_refusal` therefore tests alignment with a larger `over_buf` (`over_buf + 1`, `size + 8`) and separately asserts that `machine_buf + 1` with `size - 1` gives SIZE. The core was not changed.
- The plan's `<verify>` fails_when wording says "100% tests passed, 0 tests failed out of N"; CTest prints "100% tests passed out of N" (already recorded in earlier summaries), so exit status and the printed counts were used.

**Total deviations:** 0 auto-fixed. **Impact:** none.

## Issues Encountered

None.

## Known Stubs

None. The skeleton step and raw RAM loader remain intentionally provisional (SKELETON.md); the step semantics were not touched, so the golden digest planned for 01-08 is unaffected.

## Threat Flags

None. T-01-09 (NULL, size, alignment, magic checks first; subtraction-form bounds; `lynx__budget_steps` clamp) is now covered by boundary tests under non-recoverable ASan+UBSan. T-01-10 (struct padding): init and reset both clear the whole struct before setting fields, and `instances_independent` initialises over `0xA5` and `0x5A` garbage and compares buffers.

## Notes for the next plan

- `lynx_reset` is a public, costly-to-reverse contract: reset clears content. Phase 3 direct-load startup builds on it.
- `tests/api/test_api.c` now has shared static buffers (`buf_b`, `snap_a`, `snap_b`, `over_buf`, `mem_out`, `big_image`) and helpers `init_in`, `CHECK_UNCHANGED`, `ADVANCE_EXPECT`; plan 01-07 snapshot refusal cases should reuse them. Snapshot functions must be added to the `tests/header/header.c` pointer table.
- `api.inspect_pure` must gain any new inspect function added later.
- A non-halting image with a huge budget runs in linear time (documented host responsibility); the budget clamp is tested through the white-box guest-tick poke.

## Self-Check: PASSED

Modified files exist; commits `476e9eb` and `0d4d314` are ancestors of HEAD; `git rev-list --count` from the ledger base gives 2; `sh scripts/check.sh` exits 0.
