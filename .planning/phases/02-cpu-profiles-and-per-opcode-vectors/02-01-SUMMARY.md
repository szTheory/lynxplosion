---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 01
subsystem: cpu-core
tags: [cpu, 65c02, cycle-stepped, profiles, x-macro, flat-bus, include-gate, c17]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "core gates (core.includes, no_float, nm_globals, nm_undefined), lynx_add_test and the exact inventory, expect_fail.cmake, the claim bound to the inventory hash"
provides:
  - "core/cpu.h, core/cpu.c: a cycle-stepped CPU that posts one request per cycle as data and changes state only in lynx__cpu_complete"
  - "core/cpu_profile.def and cpu_profile.c: one X-macro source expanded into the const [2][256] integer table (224 OP rows, 32 OPX rows), static-asserted"
  - "tests/cpu/flat_bus.h: header-only flat 64 KiB test bus with a per-cycle log and a per-cycle IRQ/NMI line schedule"
  - "lynx-test-cpu with cases tables, tables_mutant, tracer, cycles; CTest cpu.tables, cpu.tables.self_test, cpu.cycles"
  - "core.cpu_includes gate with a seeded control (whole-name allow-list, refuses a vacuous scan)"
affects: [02-02, 02-03, 02-04, 02-05, 02-09, 02-10]

actuals:
  tokens: 26253
  tasks: 3
  commits: 3

plan_head_before: 0c203f27831c23018e3e040a905113eed23c773f
plan_head_after: b496ce311b4415a6b1a9aaef871675ba74ebec11

tech-stack:
  added: []
  patterns:
    - "Per-mode script table of micro-operations (const uint8_t [mode][slot]) dispatched by switch; cycle counts emerge from script length minus inactive conditional micro-operations"
    - "Request-as-data: lynx__cpu_repost is a pure function of state, used after every completion and after a snapshot restore"
    - "Decode table passed as a const pointer argument, never stored in the instance (the mutant control passes a test-local copy)"

key-files:
  created:
    - core/cpu.h
    - core/cpu.c
    - core/cpu_profile.def
    - core/cpu_profile.c
    - tests/cpu/flat_bus.h
    - tests/cpu/test_cpu.c
    - tests/cpu/tests.cmake
    - tests/cmake/cpu_includes.cmake
  modified:
    - CMakeLists.txt
    - tests/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Dispatch is a const integer script table indexed by (mode, slot) holding micro-operation ids, then a switch on the micro-operation, instead of a hand-written switch (mode) then switch (step); the plan's artifact marker 'switch (mode)' is therefore not literally present (see Deviations)"
  - "A branch's taken-dummy cycle records its page-cross answer in ea bits 8 and 9 so the cross cycle is decided before pc moves"
  - "Starting any interrupt entry clears both TAKE latches (and NMI_PENDING for NMI), so an IRQ latched in the same poll cannot start a second entry straight after an NMI entry"
  - "lynx__cpu_table returns NULL for a profile above 2 (the core never passes one); lynx__cpu_state_valid refuses profile 3"
  - "The opcode-fetch request carries neither POLL nor LAST (the opcode is not known when it is posted); a 1-cycle NOP polls nothing by its F_NO_POLL row flag"

patterns-established:
  - "Hand-authored expected cycle lists carry a source tag (SST-MODEL, WDC, RKW, INFERRED) and the run prints counts per tag"
  - "Include gate compares names whole and fails when it finds no file, with a seeded control holding prefix, suffix and angle-bracket look-alikes"

requirements-completed: [CPU-02, CPU-03, CPU-04]

duration: not measured per task (one session; check.sh total 29 s)
completed: 2026-11-09
status: complete
---

# Phase 2 Plan 01: CPU core, profile tables and flat test bus Summary

**A pure, cycle-stepped 65C02-class core whose every access is a posted request, two profile tables from one X-macro file (they differ in exactly the 32 bit-op opcodes), proven white-box on a flat test bus with 64 hand-authored per-cycle lists, interrupt/BRK/reset sequences and a 47,725-state validity sweep.**

## Accomplishments

- Task 1 (tracer, commit c726fe3): table, decode, posted request, flat-bus grant/perform/log, completion and the built binary. The micro-operation design made the whole engine land in this commit; the tracer sequence (`A9 42 EA 03`: 5 cycles, 3 instructions, POLL only on the last cycle of the first two) passes.
- Task 2 (commit b2864b2): `cpu.cycles` compares address, read/write and kind per cycle for one opcode of every mode group in RESEARCH 6.2 (64 cases, including page-cross and no-cross variants, branch backwards across a page, the JMP ($02FF) no-wrap case, (zp,X) and (zp),Y pointer wraps at $FF, lynx2 BBR/BBS not taken, taken and across a page, lynx1 `$x7/$xF` even/odd shapes, `$5C`, the decimal extra cycle). Decimal and binary ADC/SBC spot checks (`99+01` gives `00` with C and 3 cycles, `00-01` gives `99` with C clear, `7F+01` sets V and N). `cpu.tables` and `cpu.cycles` registered; inventory and claim hash refreshed in the same commit.
- Task 3 (commit b496ce3): IRQ (I clear, D set: pushes P `28`, sets I, clears D), IRQ blocked by I, NMI rising edge with I set, held NMI taken once, NMI over IRQ, BRK with D set, reset seam (7 cycles, three STACK_READ, S down 3, I set, D clear, A/X/Y kept), CLI latency, the lynx-tests Test 8 replica (CLI, then `$03 $03 $CB $DB` take no IRQ, INX takes it with return address after the INX; control block of `$EA` takes it after the first), state-validity refusals and acceptance, a sweep of every defined opcode of every profile cycle by cycle under no lines, IRQ held and NMI held (47,725 real states all accepted). `core.cpu_includes` plus `.self_test` and `cpu.tables.self_test` (mutated `$69` lynx2 row fails with exit 1 and "differ set"). Five inventory rows total.

## Verification (measured)

- `sh scripts/check.sh` exits 0 on the dev host (AppleClang 21, arm64): total 29 s, workflow step 13 s, ctest 12.5 s for 62 accounted tests; golden unchanged (the machine still runs the skeleton step).
- `cpu.cycles` 0.06 to 0.35 s, `cpu.tables` 0.06 s, under ASan and UBSan.
- Linux cross-check (not the full check): in `gcc:14` (linux/amd64 under emulation) `cpu.c` and `cpu_profile.c` compile with the project warning set and `-Werror` at `-O2 -fPIC -fvisibility=hidden`; `nm` shows only `t`, `T`, `r`, `R` and undefined project symbols (no `d`, `D`, `b`, `B`: the script table and decode table are read-only; no jump-table data); the test binary builds and `tables`, `tracer`, `cycles` pass and `tables_mutant` exits 1. CI (gcc 13 on ubuntu-24.04) was not run by this plan.
- Counts printed by the tests: differ-set size 32, defined-under-none 224; `cpu.cycles` tags SST-MODEL 51, WDC 1, RKW 5, INFERRED 7, all unverified for the Lynx.

## Evidence each unverified value carries

Clean-room attestation: no emulator source (Gearlynx, MAME, Holani, Beetle/Mednafen, Handy or any other) was opened, read or quoted. Behaviour came from RESEARCH sections 4 and 6 (datasheet, SingleStepTests sample templates and lynx-tests prose as already recorded there).

- Cycle shapes and every dummy-cycle address: SST-MODEL templates transcribed from RESEARCH 6.2. The expected lists in `cpu.cycles` were authored from that table by the same agent that wrote the core, so they catch implementation slips against the table, not errors in the table. Independent evidence on final state arrives with the pinned vectors in plan 02-03; vectors do not assert cycles or bus traces. All unverified for the Lynx.
- `$03` one fetch and the CLI/Test 8 poll behaviour: LT-HW (lynx-tests Test 1 and Test 8) as recorded in RESEARCH; reproduced by the replica.
- lynx1 `$x7/$xF` shapes (2 and 3 bytes, 3 or 4 cycles): synertek templates, flagged `F_UNV_SHAPE`; CMD's datasheet says 1 byte and 1 cycle.
- Decimal ADC and SBC: the RESEARCH 6.5 algorithms (verified there against 1,152 sampled vectors); this plan only spot-checks three cases. The extra-cycle address is INFERRED.
- IRQ/NMI/BRK/reset sequences: WDC-style sequence; reset stack reads, SEI/PLP/RTI and taken-branch latency, NMI over IRQ priority, `$5C` shape: INFERRED. Declared power-on values (A=X=Y=0, S `FD`, P `34`): Claude's discretion, unverified.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Branch page-cross decided after pc had moved**
- **Found during:** Task 2 (the `relative, taken backwards` case, 4 cycles and pc `01FE`)
- **Issue:** the taken-dummy micro-operation set pc to the target when no page was crossed, and the next-slot condition for the cross cycle then recomputed the cross from the new pc, adding a spurious fourth cycle.
- **Fix:** the taken-dummy stores its answer in ea above the offset byte (bit 8 crossed, bit 9 same page) and the condition reads it. `cpu.cycles` covers forward, backward and across-page branches and BRA.
- **Files modified:** core/cpu.c
- **Commit:** b2864b2

**2. [Rule 1 - Test error] RTS dummy address and the LAST flag on a 1-cycle NOP**
- **Found during:** Task 2 first run
- **Issue:** my expected RTS dummy address was the incremented return address; RESEARCH 6.2 says the dummy read is at the pulled address (`1233`), then PC+1. The LAST check also wrongly expected the flag on the opcode fetch of `$03`, which cannot know it is last when posted.
- **Fix:** expectations corrected to the table and the plan text ("one OPCODE cycle with no POLL flag").
- **Files modified:** tests/cpu/test_cpu.c
- **Commit:** b2864b2

### Design deviations (inside Claude's discretion, recorded)

- **Dispatch shape.** The plan and artifact list name `switch (mode)` then `switch (step)`. The implementation uses a `static const uint8_t lynx__script[mode][slot]` table of micro-operation ids and a `switch` on the micro-operation for both the posted request and the applied effect. It is integer-only read-only data (nm clean), it removes the duplicated mode/step switches, and a micro-operation has one definition for both `repost` and `complete`. The per-mode maximum-step table in the plan is replaced by scanning the script (`max_slot`). No cycle count is stored.
- **Task 1 delivered the whole engine.** Tasks 2 and 3 therefore added tests, the gate and one fix rather than new CPU code; the commit boundaries still follow the plan.
- **`lynx__cpu_state_valid` does not refuse a step that points at a conditional micro-operation which real execution would have skipped.** It refuses everything the plan lists (profile 3, P without bits 4 and 5, latch bits above `3F`, non-canonical boundary zeros, step beyond the script, an opcode undefined under `none` mid-instruction, an opcode held during an entry or reset sequence). Tightening is possible if a snapshot plan needs it.

**Total deviations:** 2 auto-fixed, 3 recorded design choices. **Impact:** none on the plan's goal.

## Issues Encountered

None open. A malformed first-run test of the NMI hold (the log restart also restarts the line schedule) was a test-side error fixed before commit.

## Known Stubs

None.

## Threat Flags

None. T-02-01 (state validation) and T-02-02 (include gate on the plain twin) are implemented and proven in both directions; T-02-03 (16-cycle bound) is in `flat_bus_step_instruction`.

## Notes for the next plan (02-02 onward)

- Seam: `lynx__cpu_complete(c, table, data, lines)`; after it, `c->req_addr/req_data/req_kind/req_flags` is the next request (WRITE = 1, POLL = 2, LAST = 4). `lynx__cpu_repost(c, t)` rebuilds the request from state alone (use it after a snapshot restore); `lynx__cpu_state_valid(c, t)` is the restore gate. `lynx__cpu_init(c, profile)` leaves a boundary at pc 0 with the fetch posted; set `pc` through `lynx__cpu_start_at`.
- The machine must perform the posted access and pass the data (the write data for a write); for a read of an unmapped or side-effect address it passes whatever the bus returns. The CPU samples the lines (bit 0 IRQ, bit 1 NMI) on every completion.
- `none` runs the lynx1 rows. The machine, not the CPU, must stop before an opcode that is undefined under none (`lynx__cpu_opcode_defined(0, op)` false for the 32 `$x7/$xF`), reading the opcode through the side-effect-free peek.
- `flags` (the latch byte) holds IRQ_TAKE, NMI_TAKE, NMI_PENDING, NMI_PREV and the 2-bit running sequence (1 IRQ, 2 NMI, 3 reset), so all of it fits the planned CPU state registers. `ea` can carry branch bits 8 and 9 mid-instruction; it is zero at a boundary. At a boundary `opcode`, `ea` and `tmp` are zero.
- The sequences, polls and latches are tested only on the flat bus here. The decimal V flag and the SBC decimal results are checked against vectors starting at plan 02-03.
- Test helpers: `tests/cpu/flat_bus.h` (log capacity 64 per reset; the line schedule restarts when the log is reset; `flat_bus_run_cycles` for cutting an entry).
- Inventory: `fast.txt` now has 5 more rows and the claim `inventory_sha256` is `53b01f86...` (refresh again with `sh scripts/check-claims.sh --inventory-sha256` whenever it changes).
- Estimate: projected 110,000 tokens, actual 26,253 on the same chars/4 scale over the realized diff (the context budget, not the output size, was the constraint).

## Self-Check: PASSED

All created files exist, all three task commits are ancestors of HEAD, and `sh scripts/check.sh` exits 0.
