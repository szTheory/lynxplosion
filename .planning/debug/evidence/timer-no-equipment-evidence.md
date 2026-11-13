# Timer Test6 evidence without hardware

Date: 2026-11-13. Bounded source check after the owner could not identify
available equipment. Treat compatible equipment as unavailable for planning;
its actual ownership is unestablished. No hardware was assumed, purchased, or run. No author
was contacted; no ROM/binary was downloaded; no product source, source13, or
the eight owned images were changed.

## Finding

**The public primary sources checked do not resolve the Test6 IRQ acknowledgment
counter disagreement.** They publish timer rules and describe the suite as
hardware-analyzed, but provide no Test6-specific hardware result or trace
binding a count to the measured image, startup memory, display state, and bus
grants. The hardware documentation establishes timing dependencies without a
complete schedule from which this test's count can be derived.

## Source observations

- The current [drhelius/lynx-tests README](https://github.com/drhelius/lynx-tests)
  calls the project hardware tests made by analyzing actual hardware. It says
  Timer Test6 uses Timer3 to clock linked Timer5 and checks repeated linked
  interrupts. It describes raw diagnostic bytes, but does not publish a
  Test6 measurement, trace, machine identity, or run preconditions.
- The current [Atari Lynx Timer Guide](https://github.com/drhelius/lynx-tests/blob/main/lynx-timers.md)
  combines Epyx/Atari documentation with measurements from the repository's
  tests. It documents N+1 reload timing, a phase-dependent first interval,
  timer linking, and pending-interrupt behavior. It gives no Test6 ack count
  or measurement record connecting a count to startup state, image/layout, or
  a specific Lynx.
- GitHub's current [1.4.9 release](https://github.com/drhelius/lynx-tests/releases/tag/1.4.9)
  says it adds `timers2` tests; it contains no Test6 measurement or trace. The
  current [only issue](https://github.com/drhelius/lynx-tests/issues/1) is a
  documentation question about Suzy's math carry flag. Its general comment
  about Lynx timing supplies no Test6 evidence. The issues API's other entry is
  a closed pull request for build support, also unrelated.
- The original [Epyx timer/interrupt chapter](https://www.monlynx.de/lynx/lynx8.html)
  says timer links propagate on reload, first use can be phase dependent, and
  interrupt requests wait for the current CPU cycle to finish. This constrains
  timer behavior but does not quantify Test6's loop or count acknowledgments.
- The original [Epyx CPU/ROM chapter](https://www.monlynx.de/lynx/lynx4.html)
  says CPU ticks vary with opcode and other hardware; eligible opcode fetches
  may use page mode, and video/refresh accesses pause the CPU. The [system bus
  appendix](https://www.monlynx.de/lynx/lynx11.html) specifies Video, Refresh,
  CPU, Suzy priority and grants at controller-selected points. It refers to
  cycle-by-cycle state-machine detail that is not present in the linked
  [documentation contents](https://www.monlynx.de/lynx/lynxdoc.html).

## Inference and unknowns

The suite README's project-level hardware provenance and its generic timer
measurements are useful evidence for hardware rules, but they are not an
independent Test6 result. The Epyx material explains why the software-only
counts can differ and why flat CPU pricing cannot settle it; it does not
select 12, 13, or 15 for this execution. The pinned [Test6 source](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers/tests.s)
contains the unchanged source expectation of 13, but no linked evidence that
this value came from a trace matching the project run. Existing preparation
companions noted in this handoff report counts 13 (DMA off) and 14 (DMA on).
Because they alter startup, they are not historical CRT measurements.

Still unknown are the historical Test6 result's initial counter value, exact
linked image/layout and relevant display/page-mode state, and the physical
video/refresh grant schedule. These sources do not establish which condition
produced the published source expectation of 13.

## Recommendation

Keep the strict EVD-02 mismatch narrow and open. With equipment unavailable,
park the physical evidence packet; do not infer a model, buy hardware, or
start a physical run. Keep the 12/15 comparison failing and do not claim
milestone or accuracy passage. Do not continue timer policy/parameter sweeps.
Reopen this gap only if an independent source provides a Test6-matched hardware
result/trace or a sufficiently complete primary bus schedule. No user action
is needed to preserve this stop.
