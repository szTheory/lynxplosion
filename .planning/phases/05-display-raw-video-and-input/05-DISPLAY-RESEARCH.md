# Phase 5 display timing, DMA and palette research

**Research date:** 2026-11-11
**Scope:** Primary Epyx Handy specification only for display/timing facts. No
emulator implementation source, firmware, game image, physical hardware or new
dependency was used. Context7 is not available in this environment.

## Recommendation

Build Timer 0/2 from the documented presets and make Timer 2's underflow the
documented software frame-complete event. Model video and refresh as higher
priority Mikey bus requestors that can pause CPU grants. Treat invalidation of
the CPU page-mode continuation when another master takes the bus as conservative
project policy; the source does not spell out that exact latch transition. The
source establishes priority and CPU page-mode constraints, but not DMA/refresh
cadence, service cost, or all grant-transition details. Keep those values
explicit, versioned project policy and label them unverified; do not use them to
make the unchanged timers offset-14 comparison reach 0D. If source-backed
preconditions still do not explain it, preserve the 0C/0D disagreement and the
strict failure.

For raw video, report pen bytes and the current 16-entry palette separately.
Capture a palette snapshot at each rendered line boundary as the project
observation policy required by VID-04; test writes before a boundary. The manual
supports the palette entries, channel nibbles and writes at any time, but does
not specify a hardware line-latch edge or atomic 12-bit write order. Mid-line
write behavior must therefore remain unverified and must not be implied by the
per-line snapshot format.

## Source-backed facts and limits

| Area | Supported by the Epyx specification | Not established |
|---|---|---|
| Timer-driven display | Timer 0 is the horizontal line timer; Timer 2 is linked to Timer 0 and counts lines. The manual calls Timer 2's VBL interrupt a useful end-of-frame interrupt. The normal 60 Hz preset is Timer 0 source 1 µs / backup 158, Timer 2 linked / backup 104, PBKUP 41, DISPCTL 0D: 159 µs × 105 lines = 16.695 ms. Alternatives are 50 Hz: 190 µs × 105, PBKUP 49; 75 Hz: 127 µs × 105, PBKUP 32. A timer's first use after being set may take between N and N+1 source units for backup N; subsequent reloads take N+1. | Exact phase of the first Timer 0 edge after setup; a complete physical scanout phase/blanking tie order; whether the Timer 2 underflow event is bit-identical to the LCD's first visible line. The reset chapter warns slower timer/audio and CPU clocks may not have a deterministic phase relation. The source warns 50 Hz may flicker heavily. Treat preset values as specified configurations, not hardware qualification. |
| Frame/buffer geometry | Four-bit pen index; 160 pixels × 102 visible lines, 80 bytes per line, 8160 bytes per display buffer. Three lines of vertical blank are specified for the normal presets. DISPADDR's low two bits are ignored; the upper-left buffer address must be four-byte aligned. The backup address transfers at the start of the third vertical-blank line. | Exact first-line DMA fetch tick, line-level fetch schedule and relationship between the DISPADDR transfer edge and the chosen host stop event. Keep blank lines out of the 102-line raw image unless a later API decision explicitly includes them. |
| Mikey DMA and refresh | System bus is exclusively owned by one master at a time, ordered Video, Refresh, CPU, Suzy. Video and refresh should request together where possible. Video/refresh requests can pause CPU at controller-selected points. DISPCTL bit 0 enables video DMA. DMA/refresh see the full 64 KiB RAM; MAPCTL overlays affect CPU accesses only. | The HTML specification does not provide a complete DMA/refresh request cadence, per-byte service cost, burst length, all controller transition ticks, or a complete RAS/page-mode interaction schedule. Do not import emulator constants. The manual's reset prose and MTEST1 register summary also do not give a sufficient, unambiguous reset-time schedule to infer those values. |
| CPU page mode | A page-mode opportunity is an opcode fetch in the same 256-byte page as the previous memory access; opcode decode and other system state gate eligibility. Page-mode opcode reads cost 4 ticks; normal RAM reads/writes and available hardware accesses cost 5. Data reads and all writes use normal cycles. Video and refresh can pause CPU execution. | Same-page addresses alone do not make a cycle eligible. The source does not enumerate every disqualifying state or say exactly how each intervening non-CPU grant affects the page-mode latch. Treat any video/refresh bus ownership as closing the CPU continuation until an exact cycle rule is evidenced. |
| Palette | One 16-entry palette maps 4-bit pens. `FDA0+n` stores green in the low nibble; `FDB0+n` stores blue in the high nibble and red in the low nibble. Palette registers are writable at any time. | No atomic 12-bit write order or hardware line-latch point is specified. The specification's “palette reload” time note is not a definition of when a Mikey palette write becomes visible at the LCD. |

Primary sources: Epyx [4. CPU/ROM, “RAM Page Mode” and “CPU Cycle Tick Summary”](https://www.monlynx.de/lynx/lynx4.html); [4. Display, “Frame Rate,” “Pen Numbers/Color Palette,” and display-buffer address](https://www.monlynx.de/lynx/lynx5.html); [10. Timers/Interrupts, “Timers” and “Timer Utilization”](https://www.monlynx.de/lynx/lynx8.html); [Appendix 1: System Bus Interplay](https://www.monlynx.de/lynx/lynx11.html); [Appendix 3: Hardware Addresses, MAPCTL, DISPCTL, PBKUP, DISPADR and palette registers](https://www.monlynx.de/lynx/hardware.html); [14. System Reset/Power Up and 16. Known Variances](https://www.monlynx.de/lynx/lynx10.html). These are sections of the Epyx hardware specification in its HTML edition.

## Options

| Option | Pros | Cons | Complexity | Recommendation |
|---|---|---|---|---|
| Grant-based bus events with source-backed priority and explicit policy costs | Represents exclusive ownership and CPU stalls; preserves CPU page-mode eligibility as a separate condition; can expose traces needed to diagnose D-10 without changing its oracle | Real request periods, service costs and edge schedule remain unverified; page-mode details still need policy where the manual is silent | Bus arbitration, CPU grants, display/refresh events, snapshots and trace tests — Risk: unsupported schedule values may be mistaken for hardware facts | Rec if Phase 5 is to exercise the page-mode suite and revisit D-10 in the same integrated bus model; label each unsourced policy in records |
| Deterministic line-batched DMA reservations with explicit per-line cost | Smaller event surface; supports timer-driven raw frames and DMA cost sensitivity; repeatable frame traces | Coarse reservation does not establish real bus ownership or CPU fetch interactions; page-mode suite may need a visible skip if its assumptions cannot be represented | Timer/video path, CPU pause accounting, snapshots and line tests — Risk: line batching can hide within-line contention | Rec if the phase prioritizes the raw-frame consumer while cycle-level arbitration remains unsupported; keep the page-mode suite visibly skipped with its blocker |

**Rationale:** The current Phase 4 observation binds flat-five CPU accesses with
page mode, video and refresh disabled, a zeroed counter and project-selected
timer phase. The Epyx sources support restoring the CRT's 60 Hz register
configuration and the missing bus-master priority, but do not supply enough
numbers to infer 13 acknowledged events for timer result 14. Prefer grant-based
events if the phase will make a page-mode claim; choose a disclosed line policy
only for raw-frame progress and leave page-mode evidence incomplete. In either
case retain the pinned source expectation 0D, observed value, run preconditions
and strict comparison separately. Do not change phase, code layout, costs,
counter initialization or expected bytes to manufacture agreement.

## Evidence plan and named faults

- Add source-annotated Timer 0/2 preset cases for 50, 60 and 75 Hz. Assert
  measured guest-tick line and frame intervals from programmed values, not a
  fixed host refresh rate. Check one tick before, on and after Timer 0 and
  Timer 2 boundaries, including first use and steady reload; label the
  first-edge policy unverified.
- Define frame completion as the Timer 2 underflow observation, carry its
  guest tick in the raw-frame record, and test advance-until-frame with a
  half-open bounded budget. This is the project API event definition; it does
  not claim a calibrated LCD edge.
- For offset 14, preserve the pinned wrapper/source, exact oracle, source
  result, build/layout identity and initial `t5_count` value. Record Timer 0/2,
  PBKUP, DISPCTL, MAPCTL/page-mode, refresh policy, DMA policy and phase before
  the comparison. Keep strict 0D-versus-0C failure exact if unresolved. A
  separate source-count trace must distinguish initial counter contents from
  acknowledged increments. Never seed the counter to 1 or relink to force 0D.
- Test page-mode only on eligible CPU opcode reads in a same-256-byte-page
  sequence. A data read/write and a page crossing must remain full-cycle;
  inserting a video or refresh grant must close the prior CPU stream. Run both
  CPU profiles independently without making an O1 baseline or suite-pass claim.
- Use a known aligned, bounded, original RAM buffer with alternating pens to
  verify 80-byte rows, 102 visible rows, pen order, flip/address policy and
  separate palette snapshots. Write different valid palettes immediately
  before consecutive line boundaries. Include pen 0 and pen F and byte values
  that distinguish swapped red/blue nibbles.
- Name a mutation such as `video.dma_cost_zero`: remove the configured DMA bus
  cost/hold while leaving the input and video buffer identical. The intended
  bus/timing assertion must fail with a trace showing changed CPU grants or
  guest progress, then pass on the clean model. Add a separate omitted-DMA
  control if cost and request presence are independent code paths. A digest
  change alone is insufficient proof of the fault.

## Adversarial pass and remaining risks

- **Hardware/video:** vary the first timer edge, timer reload writes near a
  boundary, Timer 0 source/backup, Timer 2 count, buffer base low bits, flip,
  vertical-blank boundary and the third-blank-line address transfer. No
  inferred frame event may silently become a claim about LCD scan phase.
- **CPU/bus:** test same-page eligible opcodes, page crossings, disallowed
  opcodes, data accesses, video/refresh preemption and simultaneous request
  ordering. Same-page fetches after a different bus master must not accidentally
  retain a stale page-mode win.
- **Palette:** mutate one nibble and each component separately; test line-before
  and line-after writes, mid-line writes as explicitly unverified behavior, and
  orientation. Keep the 12-bit pen mapping distinct from host RGB conversion.
- **Audio/input/API:** leave audio device state absent; display and timer work
  must not fabricate audio events. Sample the logical input timeline at its
  documented guest time and record it separately. Raw pen bytes, per-line
  palette observations, guest timing, machine progress and host presentation
  stay distinct; snapshots and split advances must reproduce the same software
  observations. A shared digest proves repeatability only.
- **Verification/performance/rights/DevOps:** use fresh instances and named
  source-backed versus project-policy assertions, exact inventories, negative
  controls and visible skips. Keep `page-mode` skip visible if prerequisites
  are incomplete. Extend the existing check gate without adding a dependency;
  benchmark a named workload before optimizing or widening CI. Use only the
  existing MIT suite source, pinned zlib cc65 and original project fixtures;
  no emulator implementation source, ROM, BIOS, physical test, release or new
  publication channel. O1, O5, O6, O9 and all existing stops remain unchanged.

**Exit condition:** Further browsing would not fill the missing refresh/DMA
cycle schedule or palette latch rule from this primary documentation. Resolve
only through a newly identified independent primary source or owner-authorized
physical measurement; until then retain explicit software policy and visible
uncertainty.
