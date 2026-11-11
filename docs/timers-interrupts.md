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
readiness or the UART pending bit. Selecting link source 7 is currently
unavailable for every timer; a refused control-A write leaves that timer and
the CPU request unmodified. The specified timer-to-timer chains are deferred
to the linked-timer work.

Control-B bits 2–0 (last-clock and borrow observations) are not modeled yet and
read as zero. Writes to those bits have no effect. Other Mikey and Suzy bytes
remain unavailable. `lynx_read_cpu_memory` preflights the whole range and peeks
the timer bank without advancing it; `lynx_read_memory` continues to expose
raw backing RAM separately.

## Bounded scheduler and ownership

The machine scans the fixed timer source set for its next edge and compares it
with the pending CPU request by `(tick, priority, sequence)`. It advances
directly between events instead of stepping every master tick. An event at the
exclusive budget target remains pending for the next call. A zero budget makes
no machine-state change. With the private CPU grant gate held, timer events
continue and a no-event span jumps to the requested target without changing the
posted CPU request. A watched CPU write returns at `grant + 1` after all timer
work at the grant tick and the CPU completion have been applied.

All mutable timer state lives in `lynx_machine`; timer code uses no host clock,
allocation, or mutable global state. Guest time is also the canonical shared
divider phase. The event cursor prevents a current-tick event from being
dispatched twice after a stop at that timestamp.

## Verification scope and deferred semantics

`api.timer_tracer` runs a project-authored strict BLL guest through timer-0
register writes, expiry, mapped DONE/pending reads, and RAM result stores. Its
independent boundary checks cover targets 79/80/81 and 96/97. The standalone
and phase tests cover all eight mapped register blocks, all seven dividers,
count 0/255, reload and one-shot behavior, shared-phase-preserving writes,
timer-4's UART exception, refused link selection, reset/load, two instances,
and bounded CPU-held progress.

The current snapshot format does not contain the timer bank or its event
cursor. A snapshot taken while timers are active does not restore their state;
timer snapshot continuation remains incomplete until the planned snapshot
work. This plan makes no snapshot-continuation claim for timer state. No
firmware, game image, physical hardware, or named Lynx profile qualification is
used by these checks.
