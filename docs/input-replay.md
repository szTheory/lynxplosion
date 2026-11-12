# Logical input and replay contract

The core stores a copied nine-bit logical pressed-level mask. It does not
discover physical controllers, interpret their electrical polarity, or keep a
recording. Hosts own the mapping from physical controls and the ordered
`(guest_tick, mask)` timeline. `lynx_set_input` accepts every mask from zero
through `LYNX_INPUT_MASK`, including opposite directions and simultaneous
Option 2 plus Pause. It rejects bits outside the mask. A successful call only
replaces the instance's input levels: it does not advance guest time, change
orientation or palette, or retain a caller pointer.

## Canonical host timeline

Frame replay accepts a complete byte file in this versioned ASCII format:

```text
LYNX-INPUT 1
<tick> <mask>
```

Every record is one LF-terminated row with exactly two unsigned decimal
columns separated by one ASCII space. There are no signs, extra whitespace,
leading zeroes except for `0`, CR bytes, or trailing bytes. The header alone is
an empty, all-released recording. The whole file is limited to 128 KiB and
4,096 records; masks are 0 through `0x1ff`. Ticks must strictly increase,
duplicates are refused, and each tick stays below the public guest-time
headroom bound. The consumer parses and hashes the entire original byte file
before it creates the output directory or private staging area. `input_sha256`
identifies those exact canonical bytes, including the final LF.

The host advances through `lynx_advance_frame` in bounded intervals ending at
the next event tick. An earlier `FRAME` is captured and the remaining interval
is resumed; it never moves the event. The setter runs only after the public
guest clock equals the recorded tick. A tick-zero event is applied immediately
after a successful load. If a frame event is pending exactly at an edge tick,
the returned zero-consumption frame describes the pre-edge guest image; the
edge then controls subsequent CPU reads and frames. A zero-consumed `BUDGET`
is a guest-time limit error. `WATCH`, `UNDEFINED`, and `UNAVAILABLE` stops
abort frame replay without applying an edge early. At most 16 renderable frames
are retained; reaching that output bound is a visible error and never drops a
frame silently.

Replay checkpoints are host metadata. They bind the complete timeline digest,
guest tick, cursor phase, cursor index and currently applied mask. Before an
edge at `T`, the cursor is `lower_bound(T)` and the current mask reflects only
earlier records. After applying that edge, the cursor is `upper_bound(T)` and
the current mask includes it. The public format-5 snapshot contains machine
state only: no timeline bytes, cursor or host pointer are added to the core.
The replay restore check restores both checkpoint phases through the public
snapshot API and compares the public state digest and guest tick before
resuming.

Each retained frame has a PPM and a separate raw observation file. The raw
file stores each row as 80 pen bytes, 32 palette bytes and one little-endian
64-bit line tick. The manifest records the full script digest, frame sequence,
start/end tick, rows, flags, input mask at capture, guest A sample and separate
SHA-256 digests of the raw bytes and rendered PPM bytes. The PPM applies the
documented nibble conversion only; it does not rotate the guest output.

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
