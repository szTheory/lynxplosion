# Mikey timer model and evidence limits

This phase adds an instance-owned timer bank on the machine's integer guest
clock. The implementation is deterministic software policy; it is not a
measured reset phase, Lynx I/II baseline, or hardware-accuracy claim.

## Source observations and project policy

The Epyx timer chapter describes eight 8-bit down counters, seven selectable
source periods from 1 to 64 microseconds, backup/reload, DONE, and fixed link
groups. A backup value `N` represents `N + 1` source edges. The register
appendix places timer registers at `$FD00`–`$FD1F` and interrupt status at
`$FD80`/`$FD81`. Timer 4's interrupt position belongs to UART readiness rather
than timer expiry. See the [Epyx timer chapter](https://www.monlynx.de/lynx/lynx8.html),
the [Epyx register appendix](https://www.monlynx.de/lynx/hardware.html), and the
[reorganized Epyx-derived hardware reference](https://github.com/asterick/atari-lynx-hardware-reference/blob/main/docs/05-timers-interrupts.md).

The implementation makes these choices explicit where the source does not
settle a software event order or reset phase:

- Guest tick 0 is the fixed-zero divider phase on init, reset, and load. A
  timer source edge occurs at an absolute multiple of its period; writes do
  not start private divider clocks. This policy is named
  `timer-phase-fixed-zero-1`.
- Timer-bank events sort before a same-tick CPU bus action; the action's IRQ
  poll and completion follow it. This project tie rule is named
  `timer-cpu-poll-order-1`. Timer indices 0 through 7 are the stable source
  ordinals within a bank event.
- Epyx says timer interrupts can be masked and their pending status can be
  polled independently of the mask. Under Phase 4 decision D-04, the enable
  gates new timer latch events and any nonzero pending byte drives the CPU IRQ
  line; clearing an enable does not clear a previously latched bit. The
  pending-versus-enable distinction is source-backed; the project's exact
  CPU-line sampling point follows the declared scheduler policy.
- Reset-DONE is treated as a held control level. While it is set, the DONE
  latch remains clear across timer edges. This is an explicit model choice for
  the documented non-pulse behavior.

## CTLA/CTLB transition table

The table separates what the primary register notes and pinned test source
state from the deterministic behavior chosen for this software model. The
register names and masks come from the [Epyx address appendix](https://www.monlynx.de/lynx/hardware.html#timers); timer links, borrow timing and first-use cadence come from the [Epyx timer chapter](https://www.monlynx.de/lynx/lynx8.html). The pinned `lynx-tests/timers/tests.s` is a software comparison source, not hardware qualification.

| Input/condition | Source observation | Model transition and evidence label |
| --- | --- | --- |
| CTLA source 0–6, COUNT clear | CTLA has a source selector and separate COUNT enable. | No source event is scheduled. A backup, count, source, COUNT, RELOAD, IRQ or RESET-DONE write does not restart the shared divider phase. **Model rule** consistent with the separate control bits and D-02. |
| CTLA source 0–6, COUNT set, count above zero | A timer is an 8-bit down counter clocked by its selected source. | On an aligned shared-divider event, decrement once; do not set DONE or request a timer interrupt. **Source-backed transition** with fixed-zero shared phase as separately named project policy. |
| CTLA source 0–6, COUNT set, count zero | Epyx says an interrupt signal is generated at zero while a borrow is attempted; CTLB DONE sets on timeout. | On the edge, set DONE unless RESET-DONE is held, latch the timer IRQ when enabled (except timer 4's UART source), reload BACKUP when RELOAD is set, otherwise leave the one-shot at zero and stop later counting while DONE remains set. **Source-backed effect; exact tie/write order is project policy.** |
| DONE set, then count/source/enable written | Epyx's known-variance note says the Timer Done bit must be cleared in order to count. | Keep DONE set across count and source writes; a one-shot remains stopped. **Source-backed.** |
| RESET-DONE asserted, held, then released | Epyx records that Reset Timer Done is not a pulse and CTLA B6 resets DONE. | The CTLA write clears DONE immediately; while the bit remains high, each event leaves DONE clear. Releasing it does not set DONE; a later zero-count borrow can set it. **Held-level behavior follows the variance note; write/edge ordering is explicit model inference.** |
| CTLB written with DONE bit 3 | Pinned Test 5 pre-sets DONE and expects counting to remain stopped until CTLB is cleared; Test 1 expects CTLB `$0A` to read back `$08`. | Bit 3 sets DONE when written as 1 and clears it when written as 0. **Pinned-source observation**, not independent hardware evidence. |
| CTLB written with borrow-in bit 1 | Pinned Test 1 labels `$0A` as DONE plus borrow-in, expects a guest count `$80→$7F`, then reads CTLB `$08`. | Treat B1 as one borrow-input strobe; it changes the count once even with CTLA COUNT clear and does not leave B1 latched in a stable read. **Pinned-source comparison and explicit model choice.** |
| A timer reloads with a supported downstream timer in link mode | Epyx says reloading one timer clocks the next and specifies groups 0→2→4 and 1→3→5→7. | Deliver one borrow-in to the next enabled link timer on the same guest tick; drain the increasing-index graph in stable order. **Source-backed edges; bounded iterative drain and same-tick order are project policy.** |
| Link mode selected for timer 0, 1 or 6 while COUNT is enabled | Epyx does not define timer 6's incoming link and lists audio 3 as timer 1's predecessor; audio is absent. | Refuse the enabling CTLA write before mutation. A disabled selection may be stored, but a later enabling write is refused. **Unsupported input policy**, preserving the unavailable-device contract. |
| CTLB bits 2–0 after a scheduled input event | The appendix names LAST CLOCK, BORROW IN and BORROW OUT; the reviewed primary notes do not define their stable read lifetime. | Expose the most recent scheduled input observation until the next timer-bank event. CTLB writes do not latch these status bits; a direct B1 strobe leaves them clear after its event. **Unresolved lifetime represented by explicit software policy**, not a hardware claim. |

### Borrow and write-edge summary

The input event consumes a nonzero count by subtracting one. At zero it
attempts a borrow, sets the borrow-out observation, latches DONE/IRQ under
their controls, and reloads only when RELOAD is enabled. A reload is the
source-defined output clock for a linked successor. A linked counter must have
COUNT enabled and must not be stopped by DONE; held RESET-DONE permits later
input events by keeping DONE clear. These transitions are drained at most once
per timer per guest tick through the finite, acyclic owned graph.

## Implemented mapped behavior

| Address | Behavior |
| --- | --- |
| `$FD00`–`$FD1F` | Eight four-byte timer blocks: backup, control A, current count, control B. |
| `$FD80` | Reads pending status; writes clear selected pending bits. |
| `$FD81` | Reads the same pending status; writes set selected pending bits. |

The standalone source selectors 0–6 use periods of 16, 32, 64, 128, 256, 512,
and 1024 master ticks. A count edge decrements a nonzero count. An edge at zero
sets DONE and, when enabled, the timer's pending bit. With reload enabled, the
backup value is loaded; otherwise the one-shot remains at zero and DONE blocks
further counting until cleared. Writing zero to control B clears DONE. Writing
control A with reset-DONE set clears DONE immediately and holds it clear.

Timer 4 can count and set its DONE status, but it does not synthesize UART
readiness or the UART pending bit. The owned link sources 0→2→4 and
1→3→5→7 are implemented as bounded same-tick borrow propagation. A link-mode
timer is advanced only by its declared predecessor's reload event; it is not
scheduled from the shared base divider. Enabled link selections for timer 0,
timer 1 (whose documented predecessor is absent audio 3), and timer 6 are
refused before CTLA changes. A disabled unsupported selection can be written,
but enabling COUNT with that selection is refused. Other Mikey and Suzy bytes
remain unavailable.

The CTLB LAST CLOCK/BORROW IN/BORROW OUT bits report the most recent scheduled
timer input event and remain until the next timer-bank event. Their names are
source-backed, but their stable read lifetime is an explicit software policy
because the reviewed primary notes do not define it. A direct CTLB borrow-in
write is a one-event strobe and does not latch those low status bits, matching
the pinned timer test's `$0A`→`$08` read. `lynx_read_cpu_memory` preflights the
whole range and peeks the timer bank without advancing it;
`lynx_read_memory` continues to expose raw backing RAM separately.

## Bounded scheduler and ownership

The machine scans the fixed timer source set for its next edge and compares it
with the pending CPU request by `(tick, priority, sequence)`. It advances
directly between events instead of stepping every master tick. An event at the
exclusive budget target remains pending for the next call. A zero budget makes
no machine-state change. With the private bus ownership gate held, timer events
continue and a no-event span jumps to the requested target without changing the
posted CPU request. A watched CPU write returns at `grant + 1` after all timer
work at the grant tick and the CPU completion have been applied.

All mutable timer state lives in `lynx_machine`; timer code uses no host clock,
allocation, or mutable global state. Guest time is also the canonical shared
divider phase. Each event scan coalesces the fixed timer sources into one
timer-bank candidate, with the CPU request as the other candidate. Timer source
ordinals are the bounded indices 0–7; this scheduler has no dynamic sequence
counter, so it has no sequence exhaustion state to serialize. If a public stop
occurs after timer work at the current tick but before the CPU request
completes, the snapshot stores one `event_applied` bit. Restore reconstructs
that cursor at the saved guest tick. Once guest time advances, the historical
cursor is normalized away; no old event timestamp or candidate queue is stored.

## CPU ownership and guest sleep boundary

The Plan 01 source and wrapper inventory reports that neither pinned public
suite references FD91, and the bounded wrapper trace did not reach it. FD91
remains unavailable: it maps to an unimplemented Mikey byte, and no active
Suzy owns the bus. `api.request_boundary` verifies that a real guest write to
FD91 stays pending, refuses with `LYNX_STOP_UNAVAILABLE`, and leaves hidden
backing RAM unchanged on retry. The private `bus_hold` seam tests scheduler
ownership only; it does not implement CPUSLEEP, active Suzy, WAI, or STP.

`wake_pending` follows the nonzero timer interrupt-pending latch independently
of the CPU I mask. I still controls whether the CPU accepts the interrupt at a
poll. `api.request_hold` keeps a real BLL store request and CPU state unchanged
while timer IRQs progress, then checks one watched store completion after
release. `api.request_wake` aligns timer expiry and that store grant at tick 64
under both masked and unmasked I states, checking timer-before-CPU ordering and
IRQ acceptance separately.

`cpu_ready_at` records the end of a priced bus cycle, not time spent waiting
for ownership. When guest time passes it, the scheduler normalizes readiness
to the current tick. `LYNX_REG_CPU_STATE` reports only remaining priced-cycle
ticks, so a long hold reports zero while the independent hold gate still keeps
the request posted. Format 4 preserves `bus_hold`; `wake_pending` is derived
from the timer pending latch, and the CPU request is reposted from validated
CPU state. The maximum positive public-return residual is four ticks for
flat5 and thirteen ticks for test_skew: each public return follows at least
one tick of a granted cycle, while the largest table prices are five and
fourteen ticks. Waiting for ownership does not increase that priced-cycle
bound. `api.request_boundary` exercises pre-cycle, in-cycle,
expired, zero-budget, long-hold, and test-skew pricing cases, plus repeated
hold transitions. `api.request_hold` also checks per-instance isolation.

## Verification scope and deferred semantics

`api.timer_tracer` runs a project-authored strict BLL guest through timer-0
register writes, expiry, mapped DONE/pending reads, and RAM result stores. Its
independent boundary checks cover targets 79/80/81 and 96/97.
`api.timer_done_borrow` checks the pinned CTLB `$0A` borrow/count result and
held RESET-DONE behavior through guest writes. `api.timer_link_tracer` makes a
real guest-programmed 3→5 reload edge collide with a mapped CPU read, then
checks both CTLB observations, pending bits and result RAM. `api.timer_links`
checks every owned edge, both full chains, simultaneous standalone roots,
reload boundaries and count 0/255 through guest-programmed timers.
`api.timer_unavailable` checks enabled timer 0/timer 1 (absent audio 3)/timer 6
link refusal, later enable refusal after a disabled link selection, arbitrary
unimplemented Mikey/Suzy guest writes, preserved requests and atomic mapped
range inspection. The standalone and phase tests also cover all eight mapped
register blocks, all seven dividers, count 0/255, reload and one-shot behavior,
shared-phase-preserving writes, timer-4's UART exception, reset/load, two
instances, and bounded CPU-held progress.

`api.irq_pending` runs an original BLL guest under both explicit CPU profiles.
It installs an IRQ vector in RAM through guest MAPCTL control, forces and
reads simultaneous pending bits through both `$FD80`/`$FD81` aliases, checks a
zero acknowledgement and selected-bit clearing, then receives a timer-0 IRQ
after the guest disables that timer's IRQ enable. A second guest proves a
count-enabled timer with IRQ disabled reaches DONE without creating a pending
bit. The handler writes a watched marker before acknowledgement; the test
checks its live stack frame, saved return PC/status, pending value, selected
acknowledgement, and RTI return. These checks establish the implemented
software latch path under both profiles, not a Lynx I/II compatibility pass.

`api.irq_poll` gives the timer and CPU independent boundary observations under
both profiles. With the fixed-zero phase policy and flat5 placeholder costs,
timer 0 expires at tick 80: the preceding poll at 75 sees no pending bit; the
event then occurs while the next NOP is in flight, and its poll at 85 accepts
the IRQ. The watched handler store is sampled at tick 150 and returns at 151.
A separate guest clears the timer bit with an `INTRST` write granted at tick
80; the timer event is applied first, the write clears it, and that same CPU
cycle's poll sees zero and does not enter IRQ. The assertions encode the
project's `timer-cpu-poll-order-1` policy. The exact tick values depend on the
named software phase and placeholder bus costs; they are not hardware timing
measurements.

`api.scheduler_order` captures an independent timer-bank event prefix at ticks
16/32/48/64/80, including counter values, CTLB status, pending bits, and
LAST CLOCK mask. The CPU is held during this timer-only prefix; coarse,
one-tick, and irregular `lynx_advance` calls produce the same observations. Its
permutation check feeds all eight stable timer source ordinals and the CPU
candidate through the same private comparator used by `lynx_advance`, in four
traversal orders. The current scheduler coalesces each timer-bank tick into one
candidate and has one CPU candidate; timer ordinals are fixed in the bounded
range 0–7, with no dynamic sequence counter or sequence-exhaustion state.
Timer-before-CPU is the explicit software tie policy, not a hardware
observation. `api.scheduler_watch` checks a separately captured CPU write/IRQ
watch prefix at grant tick 80 and return tick 81 across the same three slicing
patterns under the default `none` profile. `api.scheduler_limits` covers
exclusive targets 15/16/17, zero-budget identity, immediate long idle,
guest-time headroom, and a posted bus request completed at the arithmetic
limit. These tests establish deterministic software scheduler behavior and
arithmetic bounds; they do not establish Lynx I/II timing compatibility.

The pinned `timers2` suite is supporting source evidence for phase and enable
ordering only. Independent project-authored phase/enable cases cover that
failure class, so `timers2` is explicitly outside this phase's required suite
set; the required public-suite observations remain `cpu` and `timers`. Source
agreement is software comparison evidence only while baseline decision O1
remains open.

Snapshot format 4 stores the eight canonical timer records (backup, count,
CTLA, CTLB), pending IRQ byte, phase-policy identifier, current-tick
`event_applied` bit, and `bus_hold`. It derives `wake_pending`, the CPU request,
divider phase and future timer candidates from those fields and guest time.
It does not store a separate deadline queue or historical event timestamp.
Restore validates the complete candidate, including reserved bits, legal
CTLB status combinations, enabled incoming-link support, cursor consistency,
hold range, readiness residual and guest-time headroom, before changing the
machine. A successful apply clears the destination's host watch; a refused
restore leaves the whole instance and watch unchanged. Watch configuration
and hit events never enter guest snapshots.

Readiness controls accept flat5 residual `+4` and reject `+5` and `+255`;
test_skew accepts `+13` and rejects `+14`. Separate controls preserve legal
guest-time headroom and refuse a value beyond the scheduler limit. A bus hold
can preserve an in-flight cycle or an expired request; elapsed hold time is
not added to the priced-cycle residual. Formats 1–3 are refused as
unsupported; no migration or cross-build compatibility is promised. Tests
compare coarse, one-tick, irregular, watched and restored paths using
independent RAM, mapped timer/status bytes, public CPU registers, pending
request and guest time. The watched path observes the tick 80 timer/link
cascade before the CPU store and produces the same guest snapshot as the
unwatched path.

The golden workload's observation digest excludes snapshot bytes. The format
change left `tests/golden/skeleton.digest` untouched; its golden and
sensitivity checks remain controls. No firmware, game image, physical
hardware, or named Lynx profile qualification is used by these checks.
