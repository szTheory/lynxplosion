# Digital audio policy 1

This file separates the register observations supported by the cited Lynx
hardware descriptions from the deterministic host-facing policy used by this
core. The implementation is not a measurement of a physical console.

## Source-backed register observations

The Mikey audio block occupies `$FD20..$FD3F`, with four channel records at
eight-byte strides. The register descriptions identify signed 8-bit volume,
an 8-bit backup and current counter, seven prescaler selections plus the
previous-clock selection, control A/B, feedback, and a 12-bit noise/LFSR
shifter. Chapter 7 documents the nine selectable taps at bits 11, 10, 7, 5,
4, 3, 2, 1 and 0, inverted-XOR feedback, direct output, and integrate mode
with clipping. A counter value N borrows after N+1 selected source edges.
Appendix 3 maps FD20/FD28/FD30/FD38 as channel volume and
FD27/FD2F/FD37/FD3F as channel control B; FD50 contains the stereo-disable
control. The described timer link order is group B T1→T3→T5→T7→group A
T0→T1→T2→T3→group B T1.

Primary references: [Mikie audio chapter](https://www.monlynx.de/lynx/lynx7.html),
[Mikie timer chapter](https://www.monlynx.de/lynx/lynx8.html), and
[hardware address appendix](https://www.monlynx.de/lynx/hardware.html).

The cited audio text says FD27's lower nibble behaves as a timer control except
for bit 3, whose function it does not explain. It also describes CTLB bit 3 as
a channel status / lifetime control. This version treats the exact CTLB status
lifetime, manual borrow-in behavior, reset phase and linked source mode as
software policy rather than claiming a physical
interpretation. The cited materials do not settle stereo routing, power-on
oscillator phase, or exact output amplitude. The upstream audio suites provide
additional software observations, but do not address MSTEREO and are not
hardware qualification.

## Versioned deterministic policy

`LYNX_AUDIO_POLICY_DIGITAL_1` uses fixed 40,000/1 Hz stereo signed-16 PCM.
Sample ordinal 0 is taken at guest tick 400; each following sample is exactly
400 guest ticks later. Device and timer-link work at a tick precedes sample
emission, and sample emission precedes CPU bus grants. All work at the
half-open target tick remains pending. This 400-tick sample lattice and the
ordinary divider/LFSR transition order are implementation policy, not
hardware-qualified rates or phase.

Reset starts counters, divider phase, feedback, shifters, outputs, stereo
routing and integrators at zero. The selected ordinary channel divider uses
`16 << (CTLA & 7)` guest ticks for prescaler values 0..6; value 7 selects the
previous link source. A channel decrements a
nonzero count on its clock. At zero it borrows, optionally reloads its backup,
updates DONE/reset-DONE policy, then clocks its shifter and output. The zero
value therefore borrows on the next selected edge. The exact control/status
lifetime and first edge are deterministic software policy.

Output changes on an audio clock/underflow or an explicit OUT write. Volume,
seed and control writes retain the current output until that clock; entering
integrate mode starts from the retained output. A CTLB software borrow clocks
the counter even with counting disabled, without manufacturing readable clock
status bits. This behavior is checked against the pinned MIT audio suites.
For the timer/audio ring, borrow-in/out are guest-tick pulses. Linked LAST_CLOCK
falls with the pulse; divider LAST_CLOCK retains its last level until another
clock or CTLB write. DONE remains a separate latch. These lifetime rules are
software policy supported by those source comparisons, not hardware measurements.

The software link order is T1→T3→T5→T7→A0→A1→A2→A3→T1. Timer 0→2→4 remains
separate and keeps its prior ordering. A borrow consumes one link pulse and
emits at most one successor when that next node selects link, including a
one-shot underflow with reload disabled. Reload changes the counter value;
underflow controls DONE, IRQ and linked propagation.

Same-tick link work has a fixed eight-visit budget, starting at the serialized
ring cursor. Pending work retains its exact per-node multiplicity and advances
under the `ring-defer-1` software policy at the next master tick. The absolute
deadline is serialized so a half-open call that stops exactly there, a watch
return, or a save/restore cannot shift delivery another tick. The eight ring
nodes have at most eight natural roots per tick, plus one CPU bus-write root;
propagation itself does not increase total debt. Thus total pending debt is at
most `9 * (guest_ticks + 1)`. Each node stores a 64-bit low word and four-bit
high word, and restore checks the summed debt against that cumulative bound.
This is a bounded software rule, not physical ring timing qualification.

Register volume and output bytes are decoded explicitly as signed 8-bit
values: `0x80` is -128 and `0xFF` is -1. The shifter uses the FD21 tap
selection and FD25 bit 7 selects tap 7. The documented FD21 map selects taps
0..5 with bits 0..5, tap 10 with bit 6, and tap 11 with bit 7; feedback is the
inverted parity of the selected state bits. The core shifts left and inserts
that feedback bit at bit 0. The tap set and register mapping come from the
primary documents; shift/update order and initial phase remain deterministic
software policy where the source notes are ambiguous. In direct mode, the
current shift bit selects positive or negative volume. In integrate mode,
signed volume deltas accumulate and saturate at -128 and +127.

The PCM mixer sums the four already-saturated signed channel outputs in a
wide signed integer, then multiplies each routed side by 64. This yields the
exact S16 interval -32768 through +32512: four -128 outputs reach -32768 and
four +127 outputs reach +32512. The operation uses no rounding, signed shift,
overflow or narrowing outside the representable range. FD50 bits 0..3 disable
channels 0..3 on the right output and bits 4..7 disable those channels on the
left; zero routes every channel to both sides. Muted routes do not stop
channel counters, shifters, integrators or event records. These routing and
mix-scale choices are software policy, not claims about measured hardware.

The public `lynx_advance_av` call copies PCM frames and audio events into
caller-owned spans. It retains no pointer. Event sequence numbers are stable
within a guest tick and the event cursor is machine state so slicing and
snapshot continuation do not change observations. A guest tick is bounded by
four natural channel roots, eight ring visits, and one CPU audio write or
strobe; the public event span minimum is 13 entries for this conservative
same-tick envelope. A device transition stages its state and events together,
so an insufficient span returns `LYNX_STOP_OUTPUT` without committing that
transition. If device work already filled the available prefix, the CPU audio
write remains posted and resumes at the exact tick with a fresh span. A full
span is never required for a PCM sample, but an absent next frame returns
`LYNX_STOP_OUTPUT` before sampling it. Enabled observations are never dropped.
Legacy `lynx_advance` and `lynx_advance_frame` still advance channel and sample
phase but do not request PCM or event output.

When a call has no PCM output span, the scheduler advances sample ordinals
arithmetically between real events and the exclusive target. A sample that
shares a tick with device, video, refresh or CPU work remains in the event
order, so this fast path preserves boundary and stop behavior without skipping
timer, channel, link, video or CPU work.

Raw pens/palettes and per-line ticks remain separate from audio samples and
events. Host timeline cursors, file offsets, clocks, and output buffers are not
guest state. The complete four-channel state and deferred ring work are
included in snapshot format 7 and the canonical guest-state digest, although
this tracer exercises only channel 0. Snapshot round trips establish software
repeatability within the exact build identity, not physical accuracy.
