# Logical input and replay contract

The core stores a copied nine-bit logical pressed-level mask. It does not
discover physical controllers, interpret their electrical polarity, or keep a
recording. Hosts own the mapping from physical controls and the ordered
`(guest_tick, mask)` timeline. `lynx_set_input` accepts every mask from zero
through `LYNX_INPUT_MASK`, including opposite directions and simultaneous
Option 2 plus Pause. It rejects bits outside the mask. A successful call only
replaces the instance's input levels: it does not advance guest time, change
orientation or palette, or retain a caller pointer.

## Guest register view

Use MAPCTL `0C` in a CPU fixture to expose the Suzy/Mikey device overlays.
MAPCTL `0F` selects underlying RAM at these addresses and is not an input
device fixture.

With the Suzy input view selected, JOYSTICK at `$FCB0` reports these active
levels:

| Logical level | Lefthand = 1 | Lefthand = 0 |
|---|---:|---:|
| Up | `$80` | `$40` |
| Down | `$40` | `$80` |
| Left | `$20` | `$10` |
| Right | `$10` | `$20` |
| Option 1 | `$08` | `$08` |
| Option 2 | `$04` | `$04` |
| B | `$02` | `$02` |
| A | `$01` | `$01` |

SWITCHES at `$FCB1` reports Pause in bit 0. Cartridge input lines in bits 1
and 2 are not logical buttons; this build returns zero for disconnected lines
under the `input-disconnected-zero-1` software policy. That electrical value
is unverified. Other unused SWITCHES bits also read zero by policy.

SPRSYS at `$FC92` models only the input-owned Lefthand bit 3. A guest may write
`$00` or `$08`; reads expose only that bit. Other SPRSYS control bits request
unsupported sprite or math behavior and stop as unavailable before changing
the modeled orientation. Other Suzy registers remain unavailable. This is a
limited software register view, not a claim that the complete SPRSYS state is
implemented. The input API never changes Lefthand automatically: Option 2 and
Pause remain two independent levels, and guest software controls orientation.

The direction table follows the Epyx hardware-address appendix's JOYSTICK and
SPRSYS descriptions, reconciled with the pinned cc65 Lynx aliases. The appendix
does not establish the electrical value of disconnected cartridge lines or
physical switch polarity. These mappings and disconnected-line value are
deterministic software policies; they have not been qualified against
physical hardware. See the [Phase 5 input evidence record](../.planning/phases/05-display-raw-video-and-input/05-INPUT-EVIDENCE-RESEARCH.md)
for source identity and evidence limits.

## Sampling a recorded edge

The current policy samples JOYSTICK and SWITCHES when a CPU read actually
reaches its bus grant, not when the instruction starts and not at a host frame
callback. The sample tick is the guest grant tick. A host applying an event at
tick `T` advances to the half-open boundary `T`, handles any earlier frame or
watch stop, calls `lynx_set_input`, and then resumes guest work at `T`. If a
posted CPU request is held by video DMA or refresh, the input level is sampled
when that retained request is eventually granted.

Frame completion and input edges are separate observations. An input change
at a frame endpoint does not rewrite the frame that just completed; a CPU read
granted at that same tick after resume observes the new level. Hosts should
record the actual sample tick alongside each observed read when building
replay evidence. Current bus sample pricing is zero additional ticks; changing
that policy requires a separately specified and tested sampling contract.

Automated coverage runs original guest CPU code with MAPCTL and Lefthand
explicitly configured. It checks all 512 logical masks through both guest
orientation mappings, the MAPCTL `0F` RAM negative control, exact and adjacent
`T-1`, `T`, and `T+1` input edges, retained reads across video/refresh holds,
zero-consumed advances, and frame/watch boundaries.
Passing these checks establishes deterministic behavior for this software
policy; it does not establish hardware accuracy.
