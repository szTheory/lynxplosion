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
lifetime, manual borrow-in behavior, reset phase and the not-yet-implemented
linked source mode as software policy rather than claiming a physical
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
`16 << (CTLA & 7)` guest ticks for prescaler values 0..6; value 7 is the link
selection and remains unavailable until the ring plan. A channel decrements a
nonzero count on its clock. At zero it borrows, optionally reloads its backup,
updates DONE/reset-DONE policy, then clocks its shifter and output. The zero
value therefore borrows on the next selected edge. The exact control/status
lifetime and first edge are deterministic software policy.

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
snapshot continuation do not change observations. Capture checks event space
before performing a potentially observable tick and returns
`LYNX_STOP_OUTPUT` before progressing when a complete-tick event reservation
or next PCM frame does not fit; it never discards enabled observations.
Legacy `lynx_advance` and `lynx_advance_frame` still advance channel and sample
phase but do not request PCM or event output.

Raw pens/palettes and per-line ticks remain separate from audio samples and
events. Host timeline cursors, file offsets, clocks, and output buffers are not
guest state. The complete four-channel state envelope is included in snapshot
format 6 and the canonical guest-state digest, although this tracer exercises
only channel 0. Snapshot round trips establish software repeatability within
the exact build identity, not physical accuracy.
