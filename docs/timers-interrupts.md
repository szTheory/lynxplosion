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
- The source enable bit gates a newly created pending timer latch, following
  Phase 4 decision D-04. Existing pending bits stay asserted after an enable
  is cleared; any nonzero pending byte drives the CPU IRQ line. This latch
  choice is project policy and is not presented as hardware qualification.
- Reset-DONE is treated as a held control level. While it is set, the DONE
  latch remains clear across timer edges. This is an explicit model choice for
  the documented non-pulse behavior.

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
