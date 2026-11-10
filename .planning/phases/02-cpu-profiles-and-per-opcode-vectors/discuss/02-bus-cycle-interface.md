# Phase 2 discuss lens: bus-cycle interface and timing model

Date: 2026-11-09. Lens: emulator architect, performance and determinism.
Scope: CPU-03 and Phase 2 success criterion 3, shaped so that TIM-01 and
TIM-04 (Phase 4), VID-02 (Phase 5) and SUZ-01 (v2) attach without changing
CPU code. This is research input to the Phase 2 discuss step, not a decision.

## Tags

- [SO] Source observation, with the source named. Primary hardware text
  (Epyx Handy specification mirror, WDC W65C02S datasheet) or a public prose
  document. "[SO maint]" marks a maintainer's report from hardware captures
  (lynx-tests guides), not reproduced by this project.
- [RR] Repository research (`.planning/research/*`, dated 2026-11-09),
  second hand. Not re-verified unless the line says so.
- [INF] Inference by this lens.
- [UNK] Unknown. Needs a source, a measurement or a later phase.
- [REC] Recommendation for the orchestrator to lock or reject.

Clean-room note: no emulator source code was opened for this note. The
repository research includes implementation reports about other emulators
([IMPL] lines in ARCHITECTURE.md). Nothing below depends on them. The
lynx-tests page-mode guide embeds some code from another emulator. It was
read through a summarising fetch that was told to ignore code blocks, and
only its prose rules are used here.

## 1. Sources checked for this note

| Source | What it gave |
|--------|--------------|
| Epyx Handy spec mirror, CPU/ROM page: https://www.monlynx.de/lynx/lynx4.html (via a summarising fetch that was asked for verbatim quotes) | Access-cost table. "A page mode op-code read takes 4 ticks, a normal read or write to RAM takes 5 ticks." "All writes and all data reads are done in normal memory cycles." Eventually-available hardware: "minimum requirement of 5 ticks and a maximum requirement of 128 ticks (probable actual maximum of 40)". Suzy writes are blind, 5 ticks. "CPU cycles can not be used as the timing elements in small software timers." The CPU "can be paused by the video and refresh accesses". The page names no CPU clock frequency. |
| Epyx system bus page: https://www.monlynx.de/lynx/lynx11.html | Bus masters in priority order: Video, Refresh, CPU, Suzy. "only one bus master at a time". "There is no default owner of the bus". The bus controller "will pause the CPU clock and give control to the video or refresh requestor". The CPU bus-request flip-flop "can be set by system reset, any unmasked interrupt, and Suzys bus grant coming on". |
| Epyx interrupts page: https://www.monlynx.de/lynx/irq.html | No interrupt latency figure. CPUSLEEP wakes on an interrupt even when I is set. Whether code needs a NOP after `STZ CPUSLEEP` is left open on the page ("I'm standing in the mist"). |
| WDC W65C02S datasheet (westerndesigncenter.com/wdc/documentation/w65c02s.pdf; text extracted locally, not committed), section 3.10 and Table 7-1 | RDY "allows the user to single-cycle the microprocessor on all cycles including write cycles", meant for slow memory and DMA. Reserved opcodes: `X3, 0B-BB, EB, FB` are 1 byte, 1 cycle. `44` is 2 bytes, 3 cycles. `54, D4, F4` are 2 bytes, 4 cycles. `5C` is 3 bytes, 8 cycles. `DC, FC` are 3 bytes, 4 cycles. `02, 22, 42, 62, 82, C2, E2` are 2 bytes, 2 cycles. An indexed page crossing does an "Extra read of last instruction byte". An RMW at the effective address does "Two read and one write cycle". Decimal ADC/SBC give "Valid flags. One additional cycle." A BRK interrupted after its fetch: "BRK is executed, and then interrupt is executed." |
| lynx-tests `lynx-page-mode.md` (MIT-marked prose), https://github.com/drhelius/lynx-tests | [SO maint] Lynx II logic-analyzer capture. Opcode and encoded operand bytes are instruction-stream fetches and can take 4 ticks. The stream survives instruction boundaries. A fetch at an address with low nibble 0 costs 5 ticks and leaves the stream open. Data reads and writes (hardware registers included), stack, indirect pointers, vectors, JSR/RTS/RTI/indirect JMP, and taken branches to another target close the stream. Absolute JMP and untaken branches do not. Display DMA or refresh drops the DRAM row, so the next access costs 5. Formula: instruction ticks = 5 x base cycles - eligible fetches, plus wait states and stalls. Internal cycles get no discount. The guide does not say whether internal cycles drive a memory access. MAPCTL bit 7 set disables page mode. |
| nesdev wiki "CPU interrupts": https://www.nesdev.org/wiki/CPU_interrupts | NMOS 6502 family (NES) prose. Interrupts are polled at the end of the second-to-last cycle. CLI, SEI and PLP change I after the poll, which delays an IRQ by one instruction. RTI restores I before the poll, so the IRQ is taken immediately. A taken branch skips one poll. Interrupt hijacking. The page does not say whether 65C02 parts match. |
| floooh, "A new cycle-stepped 6502 CPU emulator" (2019-12-13): https://floooh.github.io/2019/12/13/cycle-stepped-6502.html (read for its design prose only) | Design literature. A per-cycle CPU becomes "just one chip amongst many": each call runs one clock cycle and returns a pin state with address, R/W and data, and the system performs the access. The author claims easier single-cycle debugging and multi-chip systems. No performance figure. |

## 2. What is known and unknown about Lynx CPU access costs (topic b)

Known (primary text):

- [SO lynx4] Normal RAM or ROM read/write: 5 ticks. Page-mode opcode read
  from RAM or ROM: 4 ticks. Always-available hardware (Mikey registers):
  5 ticks. Mikey colour palette DPRAM: 5. Mikey audio DPRAM: 5 to 20. Suzy
  write: 5 (blind, no DTACK). Suzy read: 9 to 15. "Eventually available"
  hardware: 5 minimum, 128 maximum, "probable actual maximum of 40".
- [SO lynx11] Video and refresh pause the CPU clock. Suzy releases the bus
  only on some cycles. Suzy's request-to-grant latency is set at 2.5 us,
  which is 40 ticks ([SO lynx4]).
- [RR] Cartridge read: one CPU cycle of 15 ticks. Cartridge write: blind,
  with a 12-tick window afterwards in which Suzy must not be touched. Not
  re-read here: the lynx4 table has no cart row, so the figure comes from
  another Epyx page (lynx7, per the research). Re-read before it becomes
  data.
- [SO maint] Every base 65C02 cycle is costed: 5 ticks, minus 1 for each
  eligible instruction-stream fetch.

Unknown:

- [UNK] Whether internal (dummy) cycles drive a real memory access, and so
  cost a device's access time or trigger its side effects. Example: an RMW
  on a Suzy register gives two reads and one write on a WDC 65C02 ([SO WDC
  Table 7-1]); on the Lynx that might be 9-15 + 9-15 + 5 ticks [INF]. A
  dummy read of `RCART0` would advance the cart counter [INF]. The 65C02
  places the page-cross dummy read on the last instruction byte ([SO WDC]),
  which avoids hardware addresses there, but RMW double reads still hit the
  target address.
- [UNK] The exact cost inside the 9-15 and 5-20 ranges, and what it depends
  on: clock phase, Suzy's state, audio-timer phase. Phase 4 needs the
  `timers2` tests 7-9 (TIMER READS/WRITES, MIKEY ACCESS) for this [RR].
- [UNK] At which tick within a bus cycle a read samples device state and a
  write lands. This matters when a timer changes inside a CPU read. It is a
  `timers2` calibration item for Phase 4.
- [UNK] Whether dummy cycles keep the page-mode stream open (implied-mode
  second cycles re-read PC+1, a sequential address). The guide says only that
  they get no discount.
- [UNK] The decimal-mode extra cycle on the Lynx core. WDC adds one ([SO
  WDC]); the research marks the Lynx value unsourced [RR].
- [UNK] The cycle counts and IRQ behaviour of reserved opcodes on the Lynx.
  The WDC table is a reference, not a Lynx source. lynx-tests `cpu` tests 6
  and 8 constrain lengths and IRQ blocking [RR].
- [UNK] The NMI path: Epyx says it "contains clocked delays", with no figure.

Verdict on the "default 5-tick cost" in the research:

- [INF] 5 ticks is correct for normal RAM/ROM cycles and for Mikey
  registers. As a default for all CPU cycles it is wrong in a known
  direction. MAPCTL resets to $00 [RR], bit 7 clear means page mode is on,
  so straight-line code on real hardware runs up to about 20% faster than an
  all-5 model. It is also wrong for Suzy reads (too fast), audio DPRAM and
  the cart.
- [REC] Keep 5 as the Phase 2 default for every cycle, labelled
  `PLACEHOLDER: page mode, device wait states and stalls not modelled; not
  hardware timing`. Phase 2 has no MAPCTL and no devices, and no Phase 2
  oracle can check page mode. The page-mode suite needs timers (Phase 4) and
  DMA interplay (Phase 5).
- [REC] Do not write it as a constant. PITFALLS P7 lists
  `RAM_ACCESS_TICKS = 5` as a warning sign. The default is a const integer
  table indexed by (region, cycle kind) that happens to hold 5 everywhere.
  The test cost model (section 6) proves the table is live.

## 3. Interface options compared (topic a)

Constraints from Phase 1 that rule options in or out:

- [SO repo: tests/cmake/abi.cmake] The `globals` gate fails any nm type
  `B b D d C` in the core. Its comment says "const integer tables only".
- [INF, footgun] A `static const` table of function pointers or `const char
  *` built as position-independent code on ELF lands in `.data.rel.ro`, and
  GNU nm reports that as `d`. The table passes on macOS (`__DATA_CONST` shows
  as `s`) and fails on the Linux CI leg. Use enum ids and switches, and
  `const char name[][N]` arrays instead of pointer arrays.
- [SO repo: ARCHITECTURE anti-pattern 11, AUTONOMY] No callbacks into the
  host during advance. Instance state lives in the caller buffer: no
  pointers in it, so it can be snapshotted and copied.
- [SO repo: tests/cmake/core_includes.cmake] Only `stdint.h`, `stddef.h`,
  `stdbool.h` and `string.h`, plus project headers. No `setjmp`, no
  `ucontext`, no threads. Coroutine or cothread designs are therefore out:
  no portable stack switching in C17, and a mid-coroutine snapshot is not
  possible.

### Option 1: CPU calls bus functions (instruction-at-once, "CPU drives")

Shape: `static uint8_t bus_read(lynx_machine *m, uint16_t a, kind k)`
catches devices up, adds cost to `m->now` and returns data. The CPU runs a
whole instruction per call.

- Variable per-access cost: yes. Arbitration and stalls: yes, handled inside
  `bus_read`. Cycle-exact IRQ: yes, through the catch-up done inside each
  access.
- Budget: an instruction cannot stop part-way. It either overshoots the
  budget, which breaks API-03 and the Phase 1 test that consumed never
  exceeds the budget, or carries a debt in which the reported time lags the
  machine by up to a whole instruction plus any stall inside it. During that
  lag, device catch-up has already run past the reported time, so frame and
  audio output can lead the reported clock.
- Test bus: needs either a function pointer (rejected above) or a second
  compiled copy of the CPU, in which case the vectors test code that does
  not ship.
- Determinism footgun: `x = bus_read(a) + bus_read(b)` or
  `f(bus_read(p), bus_read(q))` leaves the bus-cycle order unspecified in
  C, so the stamps differ between compilers. det.golden would catch it
  late; the vectors would catch it only if they compare cycle order.
- Performance: the best of the four (direct calls, no per-cycle dispatch).
- Snapshot: instruction boundary plus the debt field. Simple.

### Option 2: per-cycle resumable CPU; requests are data ("CPU is one chip")

Shape: the CPU never touches memory. It holds a pending bus-cycle request
(address, R/W, write data, kind, flags). The machine loop, which owns the
clock, grants the bus, prices the cycle, performs it, stamps it, and hands
the result back with the interrupt line levels. The CPU then advances one
micro-step and posts its next request. This is the pin model from the
floooh article. The 65C02 RDY pin works the same way ([SO WDC 3.10]): the
CPU can be held on any cycle, writes included.

- Variable per-access cost: yes. The cost is computed by the bus side from
  the request before the access, so the CPU holds no cycle table that can
  drift.
- Arbitration, stalls and sleep: the loop does not service the request
  until the bus is granted. CPU sleep means the loop does not step the CPU.
  The CPU code never learns about Suzy, DMA or sleep. This is what TIM-04
  and SUZ-01 need.
- Cycle-exact IRQ: the line level is handed in per cycle at that cycle's
  stamp, after devices have caught up to it. The CPU latches the poll on the
  cycle the profile marks as its poll point.
- Budget: the loop can stop between any two bus cycles. Section 5 gives a
  rule that consumes exactly the budget and never puts an effect after the
  reported time.
- Test bus: the vector harness is a different loop around the same compiled
  CPU, with a flat array and a cycle log. A cost change through
  `lynx_advance` is a data change (section 6).
- Snapshot: mid-instruction state is about 12 named bytes (section 4).
  Restore must validate them.
- Performance: one dispatch per bus cycle. About 3.2 M cycles per guest
  second at 5 ticks per cycle [INF]. Even at a pessimistic 10-20 ns per
  cycle that is 30-60 ms of host time per guest second [INF, unmeasured].
  Measure it in the Phase 6 baseline.
- Cost: more intricate CPU code (opcode x step). The per-cycle traces in
  the vectors check exactly this.

### Option 3: CPU runs against a bus vtable and reports accumulated cost

- Function pointers in instance state or in const tables: the gate and
  relocation hazards above. A host-supplied vtable is a callback during
  advance (anti-pattern 11), and a host cost function can be
  nondeterministic. Accumulated cost per instruction loses the stamp of each
  access, which defeats CPU-03.
- Rejected.

### Option 4: compile-time bus selection (macro or inline include)

- Zero overhead, no pointers. But the vector build and the shipped build
  are different object code, so a pass does not prove the shipped CPU. The
  same order-of-evaluation footgun applies if it is combined with option 1.
- [REC] Not for Phase 2. It remains a later optimisation tool (for example
  a specialised fast loop), allowed only behind a digest-equality gate on the
  named workloads (PITFALLS P23).

### Comparison

| Need | Opt 1 | Opt 2 | Opt 3 | Opt 4 |
|------|-------|-------|-------|-------|
| Per-access cost (page mode, device, DMA stall) | yes | yes | partial (accumulated) | yes |
| Arbitration, sleep and bus request without CPU changes | mostly (stall inside the call) | yes (RDY-like) | no | mostly |
| Cycle-exact IRQ poll | yes | yes | no | yes |
| Budget honoured exactly, no lead | no (overshoot or debt) | yes | no | no |
| Snapshot | boundary plus debt | plain data, mid-instruction allowed | pointers | boundary plus debt |
| Test bus on the shipped code | no (pointer or copy) | yes | pointer | no |
| nm gate and no-callback rule | at risk | clean | violates | clean |
| Speed | best | good enough [INF] | worst | best |
| Determinism footguns | argument-order evaluation | few (sequential by construction) | host code | as option 1 |

## 4. Recommended design (exact C shape)

[REC] Option 2. Absolute master-tick stamps. The machine owns the clock.

```c
/* core/cpu.h: no machine type is visible here; cpu.c includes only this. */
enum lynx__ck {               /* cycle kind, set by the CPU */
    LYNX__CK_OPCODE,          /* SYNC: opcode fetch at PC */
    LYNX__CK_OPERAND,         /* instruction-stream byte after the opcode */
    LYNX__CK_DUMMY_STREAM,    /* internal cycle that re-reads the stream (PC or last byte) */
    LYNX__CK_DUMMY,           /* internal cycle at another address */
    LYNX__CK_READ, LYNX__CK_WRITE,          /* data */
    LYNX__CK_RMW_READ,        /* the second read of a 65C02 RMW */
    LYNX__CK_STACK_READ, LYNX__CK_STACK_WRITE,
    LYNX__CK_POINTER,         /* indirect pointer byte */
    LYNX__CK_VECTOR,          /* IRQ/NMI/BRK/RESET vector byte */
    LYNX__CK_COUNT
};
enum { LYNX__CF_WRITE = 1u, LYNX__CF_POLL = 2u, LYNX__CF_LAST = 4u };

typedef struct lynx__cycle {  /* one CPU bus cycle request and result */
    uint16_t addr;
    uint8_t  data;            /* write data out, or read data in */
    uint8_t  kind;            /* enum lynx__ck */
    uint8_t  flags;           /* LYNX__CF_* */
} lynx__cycle;

typedef struct lynx__cpu {    /* plain data, no pointers */
    uint16_t pc; uint8_t a, x, y, s, p;
    uint8_t  profile;         /* id into a const integer profile table */
    uint8_t  opcode, step;    /* micro-step position; step 0 = at a boundary */
    uint16_t ea;              /* effective-address latch */
    uint8_t  tmp;             /* operand/data latch */
    uint8_t  irq_poll;        /* latched poll result for the next boundary */
    uint8_t  halted;          /* profile-defined stop state */
    lynx__cycle req;          /* the pending request */
} lynx__cpu;

enum { LYNX__LINE_IRQ = 1u, LYNX__LINE_NMI = 2u };

void lynx__cpu_reset_to(lynx__cpu *c, uint8_t profile, uint16_t pc); /* posts the opcode fetch */
/* Completes c->req with `data` (ignored for writes) and the line levels at
   the cycle's stamp, then posts the next request. Pure: no memory, no time. */
void lynx__cpu_complete(lynx__cpu *c, uint8_t data, uint8_t lines);
bool lynx__cpu_state_valid(const lynx__cpu *c);                     /* for snapshot restore */
```

```c
/* core/bus.h and machine.c: the only place where CPU memory is touched */
typedef struct lynx__bus_timing { uint16_t ticks; uint16_t sample; } lynx__bus_timing;

/* Earliest tick >= now at which the CPU owns the bus. Phase 2: returns now.
   Phase 4/5: arbitration, sleep, DMA and refresh stalls live here. */
uint64_t lynx__bus_grant(const struct lynx_machine *m, uint64_t now);
/* Pure. Duration of the cycle and its sample offset, from region decode,
   cycle kind, the page-mode stream state and the cost-model id.
   Phase 2: {5, 0} from the DEFAULT table. */
lynx__bus_timing lynx__bus_timing_of(const struct lynx_machine *m, const lynx__cycle *c, uint64_t at);
/* Performs the access with its side effects at tick `at`. Called once per cycle. */
uint8_t lynx__bus_access(struct lynx_machine *m, const lynx__cycle *c, uint64_t at);
```

Machine fields added: `uint64_t guest_ticks` (now, unchanged meaning),
`uint64_t cpu_ready_at` (earliest tick of the CPU's next bus action),
`lynx__cpu cpu`, `uint8_t cost_model` (DEFAULT or TEST; see section 6) and,
from Phase 3/5, bus stream state (`stream_open`, `last_fetch`).

The tick stamp:

- [REC] A stamp is the absolute guest master tick (u64 since power-on or
  load) at which the granted cycle starts. The duration is `ticks`. The
  access is performed at `stamp + sample` (Phase 2: sample = 0). Absolute
  stamps compare directly with scheduler deadlines in Phase 4. Do not use
  per-instruction deltas: they accumulate drift and cannot be ordered
  against device events. Tests may derive a per-instruction sum from the
  stamps.
- [REC] One clock owner: `lynx_machine.guest_ticks`. The CPU has no time
  field and no base-cycle table. Instruction length comes from its
  micro-steps, so no second timing table can disagree with the bus.

Machine loop (Phase 2 form; Phase 4 adds the scheduler `next_event`):

```text
target = guest_ticks + clamp(budget)            // clamp as in Phase 1: never wrap
while guest_ticks < target:
    if cpu.halted: stop HALT; break             // HALT consumes nothing more (Phase 1 rule)
    if cpu_ready_at > guest_ticks:              // CPU busy inside a cycle, or stalled
        guest_ticks = min(target, cpu_ready_at) // Phase 4: also min(next_event); run events <= t first
        continue
    g = bus_grant(m, guest_ticks)               // Phase 2: g == guest_ticks
    if g > guest_ticks: cpu_ready_at = g; continue
    t = bus_timing_of(m, &cpu.req, g)
    data = bus_access(m, &cpu.req, g + t.sample)
    observe(g, t, cpu.req, data)                // optional internal trace ring; never alters state
    cpu_complete(&cpu, data, lines_at(g))       // Phase 2: lines = 0 (no IRQ sources yet)
    cpu_ready_at = g + t.ticks
consumed = guest_ticks - start
```

Where a non-zero sample offset is needed later, the loop gains an
"access pending at tick X" phase. The pending request is already data, so
this is a loop change, not a CPU change.

Structural guards:

- [REC] A new include gate: `core/cpu*.c` may include only `cpu.h` and the
  allowed system headers. The CPU can then never reach `lynx_machine` or
  RAM, and a CPU bypass of the bus does not compile. This is stronger than
  any runtime test.
- [REC] Profile and cost data are `static const` integer tables indexed by
  enum ids. No pointer members, no string-pointer arrays (the
  `.data.rel.ro` footgun).

## 5. Budget, stop reasons and Phase 1 gates (topic c)

Budget rule options:

- S1 (Phase 1 style): stop before a cycle that does not fit, and leave the
  remainder unconsumed. Consumed equals clock progress and is at most the
  budget. Footgun: a host that loops `advance(remaining)` livelocks when
  `remaining` is smaller than the next cycle cost (for example 2 ticks
  against 5, or a 15-tick cart read). Every host then needs target-based
  accounting.
- S2 (recommended): half-open interval. `advance` to target T performs
  every bus action whose stamp is below T, and consumes exactly the budget
  unless it stops for HALT, FRAME or WATCH. A cycle may start before T and
  end after it. Its effects happened at its stamp (at most the reported
  time), and the CPU's next action waits for `cpu_ready_at`. No effect is
  ever stamped at or after the reported time, and devices are never caught
  up past it.
- S3 (debt): run whole instructions and clamp consumed. Rejected: the
  machine leads the reported time by an instruction and its stalls.

[REC] S2. Properties to test:

1. `consumed <= budget` always (API-03 unchanged). `consumed == budget`
   unless HALT, FRAME or WATCH.
2. Slicing independence: for any split of a span into budgets, the state at
   reported clock T is byte-identical. Test: run to T in one call and in
   slices of 1..17 ticks, then compare full-state digests at T.
3. Every trace stamp is below the reported clock, and the stamps form a
   strictly increasing chain `stamp[i+1] == stamp[i] + ticks[i]` while
   nothing stalls (true in Phase 2).
4. Overflow: keep the Phase 1 clamp. A cycle whose end would pass
   `UINT64_MAX` is not started (keep the `UINT64_MAX - 5` case, with new
   expected values).

Mid-instruction stop:

- [REC] BUDGET may stop mid-instruction. Registers then show the state after
  the last performed cycle (for example, A is not yet written during an
  RMW). Keep an internal `lynx__cpu_at_boundary()` and test it. A public
  inspect id for "at instruction boundary" can wait for the debugger API.
  It would be a provisional header addition, so it is not needed now.
- [REC] HALT is reported at an instruction boundary and then consumes
  nothing (Phase 1 postcondition kept). Phase 4 must revisit this: on
  hardware the timers keep running while the CPU is stopped. CPU sleep is
  not HALT; time passes during sleep.
- [REC] WATCH (Phase 3): stop immediately after the watched write cycle
  (stamp below the reported time), possibly mid-instruction. Budget always
  takes precedence. Phase 3 confirms.
- [INF] Phase 1 halts on `0xDB`. On the Lynx, `$DB` (STP on WDC) runs as a
  NOP [RR, lynx-tests `cpu` test 8]. Neither Lynx profile may halt on it.
  Keeping the HALT path tested needs either an internal reference profile
  where STP stops (useful anyway as a wdc65c02-vector oracle) or HALT tests
  that stay reserved. Decide this with the CPU-profile lens.

Re-baselining Phase 1 on purpose, with evidence:

- `core/machine.c`: remove `lynx__skeleton_step`, `LYNX__TICKS_PER_STEP`
  and `lynx__budget_steps` and their unit tests. Rewrite the `lynx_advance`
  header comment: replace "a budget smaller than one machine step consumes
  nothing" with the S2 rule.
- `api.advance_budget`: new exact expectations from a hand-assembled
  program with a reviewed listing. Example: after a budget of `5k` ticks,
  exactly k cycles have been performed and PC/A have the values the listing
  gives. After `5k+1`, one more cycle has been performed (sample at its
  start). Budget 0 performs nothing. The analogue of Phase 1 mutation M2 (a
  `<=` instead of `<`, or consumed above the budget) must fail this test.
  Re-run the seeded mutation and record it.
- `det.golden` and `tests/golden/skeleton.digest`: replace the skeleton
  fixture with a project-authored 65C02 program (hand-assembled, listing and
  manifest) that stays inside its RAM page and loops. The current image,
  run as real 65C02 code, would wander. Evidence for the new golden: (1) the
  same digest from Apple clang and from gcc on the Linux CI leg, which is
  the gate's purpose; (2) an `api` test pins hand-derived values for the
  same image (register values, a RAM byte, and ticks = sum of WDC-table
  cycles x 5), so the golden is not self-justifying; (3) `det.sensitivity`
  still flips on seed, content and budget, and gains a cost-model flip. The
  commit message gives the reason, as the digest file already requires.
- Snapshot: format version 2. The payload adds the CPU micro-state
  (opcode, step, ea, tmp, irq_poll, halted, request), `cpu_ready_at` and
  `cost_model`. Restore validates the new fields with
  `lynx__cpu_state_valid`, enum ranges and `cpu_ready_at >= guest_ticks`
  bounded by the longest cycle. Same-build only, no migration (Phase 1
  decision). Expect test churn in snapshot sizes and offsets.
- Expected policy for later phases: every intentional timing refinement
  (page mode, device costs, DMA) changes the golden. det.golden is a
  repeatability gate, so re-baselining it with cross-compiler equality and
  a stated reason is the normal procedure, not a failure.

## 6. Test bus and the cost model (success criterion 3)

- [REC] The cost model is data: `static const uint8_t
  lynx__cost[MODEL][REGION][LYNX__CK_COUNT]`, selected by
  `m->cost_model`. It holds `DEFAULT` (all 5) and an internal `TEST_SKEW`
  model with distinct values per kind (for example opcode 4, operand 6,
  read 7, write 9, stack 8, pointer 10, vector 11, dummy 3). It is set
  through an internal call before load, is not in the public config, and
  is identity-bound in the snapshot (a mismatch is refused).
- Tests:
  1. Vector harness (CPU only): drive `lynx__cpu` against a flat 64 KiB
     array, log `(addr, data, rw, kind)` for each cycle, and compare with
     the vector's per-cycle list and final state. This shows that every
     access appears as a cycle. Profile filtering is the CPU lens's job.
  2. Uniform scaling: with a uniform cost c, the state after `k*c` ticks is
     identical for every c (architecture unchanged, ticks scale).
  3. Skewed model through `lynx_advance`: run the same program to the same
     instruction count under DEFAULT and TEST_SKEW. Registers and RAM must
     match. The consumed-tick difference must equal the sum, over the logged
     kinds, of (skew - default), predicted from the harness log. This is
     criterion 3 exactly.
  4. Replay control: applying the logged writes to the initial RAM must
     reproduce the final RAM. This catches any write that bypassed the
     bus. The include gate covers reads.
  5. Named-fault controls: (a) a cost function that ignores the table (the
     constant-5 anti-pattern) fails test 3; (b) a swapped cycle order in one
     addressing mode fails the vector cycle compare; (c) `<=` in the loop
     fails the exact-budget test.
- Trace: an internal fixed-size ring of `{u64 stamp; u16 addr; u8 data;
  u8 kind|rw; u16 ticks; u16 flags}`, off by default, excluded from
  snapshot and digests. A test shows on/off does not change any digest
  (ARCHITECTURE pattern 6). It later grows into the public trace (DF-05).

## 7. IRQ/NMI sampling relative to bus cycles (topic d)

- [SO nesdev] 6502-family parts poll at the end of the second-to-last
  cycle. CLI, SEI and PLP take effect after the poll (one-instruction
  delay). RTI takes effect before it. A taken branch skips a poll.
- [REC] Mapping: the line level is sampled at the stamp (grant tick) of the
  cycle the CPU flags `POLL`. With no stall in between, the start of the
  last cycle equals the end of the second-to-last. Devices have been caught
  up to that tick, and same-tick device events run before the CPU access
  (Phase 4 ordering: the CPU access is the lowest priority at its tick). The
  CPU latches `irq_poll`. At the next boundary, a set latch with I clear
  turns the next opcode fetch into the interrupt sequence.
- [INF] With per-cycle register updates, the CLI/SEI/PLP delay and the
  RTI immediacy come out of where I is written relative to the POLL cycle.
  They need no special-case flags. A profile can still override with data
  if a Lynx test disagrees.
- [INF] Lynx `cpu` test 8 (no IRQ taken inside a run of `$x3`/`$xB` NOPs)
  [RR] matches 1-cycle instructions ([SO WDC Table 7-1]) that carry no POLL
  cycle. Per-opcode poll placement is profile data. [UNK] until the CPU lens
  confirms the opcode list and the expected values.
- [UNK] Polling when a stall falls between the second-to-last and last
  cycles (RDY held). Phase 4/5 detail. The flag-based design can move the
  sample point without a CPU change.
- [UNK] NMI on the Lynx (clocked delays, no figure). Keep the line in the
  interface and tie it low.
- Phase 2 tests drive the line levels per cycle from the CPU harness. No
  machine-level IRQ source exists before Phase 4.

## 8. Phase 4 and 5 attachment check (no rework)

- Scheduler (TIM-01): `next = min(cpu_ready_at, next_event)`. Events with
  tick <= t run before a CPU access at t. Total order (tick, priority,
  sequence), with the CPU access as the lowest class at its tick. Device
  handlers may schedule events but never call the loop or the CPU. The pull
  model has no CPU call stack to re-enter.
- Sleep and bus request (TIM-04): the CPU bus-request flip-flop lives in the
  arbiter (`lynx__bus_grant`). While it is clear, the loop skips the CPU and
  jumps to the next event. A wake sets it ([SO lynx11]). The CPU code is
  unchanged.
- Display DMA and refresh (VID-02): grant returns the tick after the burst.
  The burst marks the DRAM row lost, so the next instruction-stream fetch is
  priced at 5 ticks ([SO maint]).
- Page mode (Phase 3 MAPCTL bit 7, verified in Phase 5): a bus-side
  classifier over (kind, address, `stream_open`, `last_fetch`, MAPCTL.7).
  The rule is tuned against the page-mode suite. This is the reason the
  kinds are richer than read/write: the guide's rules (absolute JMP keeps
  the stream open, taken branches close it) depend on more than address
  sequence.
- Device wait states (Suzy 9-15, cart 15, audio DPRAM 5-20): region and
  phase entries in `bus_timing_of`. A sample offset appears if `timers2`
  needs it.

## 9. Adversarial pass (topic e)

| Failure mode | Where | Mitigation in the recommendation |
|--------------|-------|----------------------------------|
| Snapshot state explosion (coroutine stacks, hidden temporaries) | Cothreads; option 2 with unions or reused locals | Named fields only (about 12 bytes plus the request); `lynx__cpu_state_valid`; round-trip test mid-instruction at every step of a sample of opcodes |
| Tick drift (CPU cycle table and bus costs disagree; deltas summed) | Options 1 and 3 | No CPU-side cycle table; absolute stamps; chain test (section 5, property 3) |
| Order-of-evaluation nondeterminism | Option 1 expressions with two bus calls | The CPU cannot call the bus; one request per step |
| Reentrancy (device handler re-enters the CPU or loop) | Any catch-up design | Handlers schedule only; the loop is the sole driver; document it in Phase 4 |
| Budget livelock with small budgets | S1 | S2 consumes exactly the budget |
| Machine ahead of reported time (outputs lead the clock) | S3 and option 1 debt | S2: every effect stamped below the reported clock |
| Side-effect reads done twice (recompute after a stall) | Any | Grant, then price (pure), then access once; a test counts accesses per cycle |
| Dummy cycles skipped as "internal" (no address, no cost) | Instruction-at-once cores | Every 65C02 cycle is a request with its real address ([SO WDC]); the cost and side effects of dummies are refinable data [UNK] |
| Const pointer tables pass on macOS, fail Linux nm | Profile names, dispatch tables | Enum ids, switches, `char[][N]` |
| Test bus tests different code | Option 4, two builds | One compiled CPU; the test model is data in the shipped machine |
| A placeholder taken as hardware timing | Default 5 everywhere | PLACEHOLDER label in code and docs; no timing claim from Phase 2; page-mode and device rows tracked as open |
| Performance collapse from per-cycle dispatch | Option 2 | 3.2 M cycles per guest second estimated as cheap [INF]; measure at Phase 6; later batch-to-next-event fast path behind digest equality |
| Mid-instruction inspect confuses hosts or tools | S2 | Internal boundary flag; documented semantics; snapshot exact |
| Profile-dependent poll placement wrong | IRQ tests | Data per opcode; each expectation cites a source or is marked unverified (criterion 4) |

## 10. Recommendations to lock (summary)

1. Option 2: a per-bus-cycle resumable CPU that posts requests as data. The
   machine loop is the only clock owner and the only memory accessor.
2. Stamp = absolute u64 master tick at the grant of each cycle, plus a
   duration and a sample offset. No per-instruction deltas.
3. Cost = a pure, bus-side function over const integer tables indexed by
   (cost model, region, kind). The Phase 2 DEFAULT is 5 everywhere, labelled
   as a placeholder. Never a constant.
4. Cycle-kind taxonomy and POLL/LAST flags are set by the CPU. Page mode,
   wait states, stalls and arbitration are added later on the bus side
   without touching the CPU.
5. Budget rule S2 (half-open, consumes exactly the budget). Slicing
   independence and the chain property become tests.
6. Snapshot format v2 with CPU micro-state, `cpu_ready_at` and cost model.
   Same-build only.
7. Internal TEST_SKEW cost model plus vector harness, uniform scaling,
   replay and named-fault controls satisfy criterion 3.
8. Re-baseline `api.advance_budget`, det.golden and the snapshot tests on
   purpose. The new golden must match on both compilers and be pinned by
   hand-derived values from a hand-assembled fixture.
9. IRQ sampled at the stamp of the POLL-flagged cycle. CLI/SEI/PLP/RTI
   behaviour emerges from where each cycle writes I. Per-opcode poll
   placement is profile data.
10. New include gate (`core/cpu*.c` sees only `cpu.h`). No pointer tables
    anywhere in the core.

## 11. Open items handed on

- [UNK] Dummy-cycle device cost and side effects on the Lynx (Phase 4/8).
- [UNK] Sample point within a cycle (Phase 4, `timers2`).
- [UNK] Page-mode treatment of dummy cycles; the 16-byte rule is [SO maint]
  only (Phase 5 page-mode suite).
- [UNK] Lynx decimal extra cycle and reserved-opcode cycle counts (CPU lens).
- [UNK] What HALT means for the Lynx profiles (CPU lens), and HALT versus
  running devices in Phase 4.
- Re-read the Epyx cart timing (15-tick read, 12-tick window) from the
  scanned specification before it becomes data.
