# Digital audio policy 1

This file separates the register observations supported by the cited Lynx
hardware descriptions from the deterministic host-facing policy used by this
core. The implementation is not a measurement of a physical console.

## Source-backed register observations

The Mikey audio block occupies `$FD20..$FD2F`, with four channel records at
eight-byte strides. The register descriptions identify signed 8-bit volume,
an 8-bit backup and current counter, control A/B, feedback, and a 12-bit
noise/LFSR shifter. Chapter 7 documents channel 0's divider and feedback
registers, including FD25 bit 7 as the feedback tap. The hardware appendix
maps FD20/FD28/FD30/FD38 as channel volume and FD27/FD2F/FD37/FD3F as channel
control B; FD50 contains the stereo-disable control. The described timer link
order is group B T1→T3→T5→T7→group A T0→T1→T2→T3→group B T1.

Primary references: [Mikie audio chapter](https://www.monlynx.de/lynx/lynx7.html),
[Mikie timer chapter](https://www.monlynx.de/lynx/lynx8.html), and
[hardware address appendix](https://www.monlynx.de/lynx/hardware.html).

The cited audio text says FD27's lower nibble behaves as a timer control except
for bit 3, whose function it does not explain. It also describes CTLB bit 3 as
a channel status / lifetime control. This version treats that interaction as
software policy and refuses the not-yet-implemented linked source mode rather
than claiming a physical interpretation. The cited materials do not settle
stereo routing, power-on oscillator phase, or exact output amplitude.

## Versioned deterministic policy

`LYNX_AUDIO_POLICY_DIGITAL_1` uses fixed 40,000/1 Hz stereo signed-16 PCM.
Sample ordinal 0 is taken at guest tick 400; each following sample is exactly
400 guest ticks later. Device and timer-link work at a tick precedes sample
emission, and sample emission precedes CPU bus grants. All work at the
half-open target tick remains pending. This 400-tick sample lattice and the
channel-0 ordinary divider/LFSR transition order are implementation policy,
not hardware-qualified rates or phase.

Reset starts counters, divider phase, feedback, shifter, output, stereo routing
and integrators at zero. The selected channel divider uses
`16 << (CTLA & 7)` guest ticks. The 12-bit shift register uses the FD21 tap
selection and FD25 bit 7 selects tap 7; the tap mapping and shift direction
are deterministic policy because the cited register descriptions do not
fully specify this sequence. The output polarity follows the signed-volume
sign bit. Channel integrators saturate to signed 8-bit range. Their routed
values are summed in a wide signed integer and multiplied by 64 for each S16
sample. FD50 bits 0..3 gate channels 0..3 on the right output and bits 4..7
gate those channels on the left; a zero reset routes every channel to both
sides. These routing choices are not claims about measured hardware.

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
