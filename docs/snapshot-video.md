# Format 8 video, audio and Suzy snapshot state

Format 8 records the machine's current guest video, logical input, digital
audio and Suzy math state so an accepted restore can continue the same
deterministic guest stream. Snapshot restore
requires the same build, CPU profile, RAM policy and seed, loaded content, and
timing policy. Every scalar is encoded field by field in little-endian order;
the snapshot does not copy C structure padding.

The video and input record begins at payload offset `65604`. Its fixed header
is 72 bytes, followed by the 32 device palette registers, 8,160 packed pen
bytes, 3,264 per-row palette bytes, and 102 `uint64_t` line-start ticks. The
line timestamps occupy 816 bytes: the format stores 102 values, not 816
entries. The complete video record is 12,344 bytes.

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | display control |
| 1 | 1 | PBKUP latch |
| 2 | 1 | frame synchronized flag |
| 3 | 1 | current line ordinal |
| 4 | 1 | row active flag |
| 5 | 1 | completed capture row count |
| 6 | 1 | byte cursor in the active row |
| 7 | 1 | partial-frame flag |
| 8 | 2 | current display address |
| 10 | 2 | backup display address |
| 12 | 2 | nine-bit logical input mask |
| 14 | 1 | SPRSYS left-hand input orientation |
| 15 | 1 | timing policy identity |
| 16 | 8 | next video DMA request tick |
| 24 | 8 | video grant release tick |
| 32 | 8 | next refresh phase tick |
| 40 | 8 | refresh grant release tick |
| 48 | 8 | current frame start tick |
| 56 | 8 | frame end tick |
| 64 | 8 | frame sequence |
| 72 | 32 | current device palette registers |
| 104 | 8,160 | captured pen bytes |
| 8,264 | 3,264 | captured per-row palettes |
| 11,528 | 816 | 102 line-start ticks |

The first 16 current palette bytes and each captured row's first 16 palette
bytes represent Green values and must have a zero high nibble. Snapshot restore
rejects records that violate this canonical device state; the remaining 16
Blue/Red values preserve their full register byte.

The audio envelope begins at payload offset `77948` and is 160 bytes. Its
fieldwise little-endian layout is:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 40 | Four 10-byte channel records: volume, backup, count, CTLA, CTLB, feedback, shift `u16`, output `i8`, integrator `i8` |
| 40 | 1 | FD50 stereo-disable register |
| 41 | 1 | ring work cursor, node 0..7 |
| 42 | 64 | Eight little-endian `u64` low words of pending pulse debt |
| 106 | 4 | Packed high nibbles, one per node, completing each 68-bit debt count |
| 110 | 1 | deferred-deadline valid flag |
| 111 | 1 | same-tick device state: visit count in bits 0..3, audio-applied bit 7 |
| 112 | 1 | same-tick sample cursor |
| 113 | 1 | same-tick event cursor valid flag |
| 114 | 4 | reserved zero |
| 118 | 4 | event sequence within the current guest tick |
| 122 | 8 | event cursor tick |
| 130 | 8 | next sample guest tick |
| 138 | 8 | next sample ordinal |
| 146 | 8 | absolute deferred-work deadline |
| 154 | 6 | reserved zero |

The audio envelope ends at payload byte 78,108. The event and sampling cursors and
ring debt are guest continuation state so output remains identical when an
instance is advanced in different host-sized slices. Caller spans, host
event-script cursors and output-file positions remain host state. The ring uses
a fixed eight-visit budget per master tick in stable cursor order. If pulses
remain, `ring-defer-1` records their absolute next-tick deadline and retains
their full multiplicity. A snapshot taken exactly at that deadline preserves
due-now work. With no debt, the valid flag and deadline are both zero.

The complete timer/audio link cycle is T1→T3→T5→T7→A0→A1→A2→A3→T1. A borrow
consumes one pending pulse and may add at most one successor pulse, whether or
not the source reloads; reload controls the counter value, while underflow
controls IRQ/status and link propagation. There are at most eight natural
ring roots per master tick and one CPU bus write per tick, so cumulative debt
is bounded by `9 * (guest_ticks + 1)`. Restore validates the sum across all
eight 68-bit counters against that bound before applying the candidate; per-
node limits alone would allow forged snapshots to multiply the debt.

The current display address is a 16-bit stream position. At blank ordinal 2,
the backup address is aligned down to a four-byte boundary and transferred to
it; subsequent DMA increments wrap at 16 bits. A saved current address may
therefore be unaligned, and validation does not impose alignment on it.

Restore validates the full candidate before applying any machine byte. The
video policy must match the target instance. Input masks, orientation, flags,
line ordinal, row count, and active-row cursor must be in range. The refresh
deadline stays on the 256-tick phase grid. Future release ticks are at most
five ticks beyond guest time, and CPU, DMA and refresh cannot overlap as bus
owners. A refresh phase can be overdue while a video DMA grant blocks it; a DMA
request can be overdue while refresh owns the bus. Either request can also
wait for an already granted CPU cycle, including a return exactly at its
release tick. That case requires a request less than five ticks before CPU
release and no intervening video or refresh owner. Other overdue requests are
rejected. This preserves reachable arbitration state while refusing forged
stale work.

Captured rows form a contiguous prefix. Their line-start ticks are monotone,
are not earlier than the frame start, and do not exceed guest time. Every
unused row, palette, pen, and timestamp is zero. During an active row, only
bytes before the DMA cursor may contain captured data; the remainder stays
zero. CPU page continuation is also checked against MAPCTL and bus ownership,
because video or refresh grants clear the page latch.

`frame_end_tick` is zero at a public snapshot boundary. A successful frame
delivery clears the staged capture and starts the next frame before returning.
The current input mask is guest state; host event scripts and their cursors
are not. Write-watch configuration and hit reports are also host state. A
successful restore clears watches after applying the validated guest state;
a refused restore leaves the whole machine, including watches, unchanged.
The caller's snapshot bytes are read-only on both paths.

The CPU readiness remainder is limited by the largest reachable priced cycle
for the selected timing policy: four ticks for flat-five or video policy, and
thirteen ticks for the test-skew table. Guest time is bounded below the
64-bit overflow margin, so future release arithmetic is safe.

The 88-byte Suzy record begins at payload offset `78108` and ends the
78,196-byte payload. Its canonical field order is documented in
[`suzy-math.md`](suzy-math.md). The record is appended to snapshots and to the
guest-state digest; restore validates it before applying any candidate state.
Format 7 and earlier are refused as version mismatches.

These checks establish deterministic format-8 restoration within one exact
build identity. The video cadence, palette sampling, refresh period, and input
mapping remain software policy; snapshot equality does not qualify Lynx
hardware accuracy.

The focused restore and refusal checks are registered as
`api.video_snapshot` and `api.video_snapshot_refusal`, beside the existing
snapshot and timer controls.

## Canonical guest-state digest

`lynx_state_digest` returns 32 SHA-256 bytes for an initialized instance,
including one that has not loaded content. Its versioned stream is encoded in
this order: the NUL-terminated tag `lynxplosion.guest-state.v4`; CPU PC as
little-endian `u16`, A/X/Y/S, P with reserved bits set, flags/opcode/step, EA as
little-endian `u16`, and TMP; MAPCTL and CPU page latch as bytes; guest ticks
and CPU-ready tick as little-endian `u64`; all 65,536 RAM bytes; eight timer
records (backup/count/CTLA/CTLB), pending bits, timer phase policy, the
current-tick event-applied bit, and bus hold as bytes; then the complete
fieldwise video record above, the 160-byte audio envelope and the 88-byte
Suzy record. The NUL-terminated domain is `lynxplosion.guest-state.v4`. Video,
audio and Suzy scalars follow their documented widths and little-endian rules.
The timer event bit is derived from the current tick; an
older cursor timestamp is deliberately omitted. The stream contains no C
structure padding, immutable machine identity (profile, RAM policy/seed,
content or build), snapshot header, derived CPU request/wake state, host watch
state, caller pointers, or replay data. Refused calls leave all 32 output bytes
unchanged; the output span must not wrap or overlap the machine.

The runner writes those 32 public digest bytes as 64 lowercase hex digits in
`obs.state.hex`; its `digests.txt` entry is SHA-256 of the decoded 32 bytes.
The observation digest now covers that entry in addition to RAM, public
registers and time. Format 8's domain and Suzy state change the CPU-loop golden:
the digest makes input, timer, bus, video, audio and Suzy state visible even
when RAM/register/time projections agree. This is repeatability evidence, not
hardware-accuracy evidence.

Input event scripts and replay cursors remain host-owned. A host checkpoint
taken at tick `T` before applying event `T` is pre-edge and resumes with the
first event whose tick is at least `T`. A checkpoint taken after applying the
event at `T` is post-edge and resumes with the first event strictly after `T`.
The core has no replay cursor or checkpoint wrapper. Any host checkpoint
wrapper must carry that phase marker and the complete script digest; the core
snapshot stores only the current logical input mask. Applying an event and
taking a checkpoint must be ordered consistently with the chosen marker.

The video digest sensitivity target is `det.video_sensitivity`; Suzy record
sensitivity and host-watch exclusion are checked by `det.suzy_sensitivity`.
Cross-path raw frame,
progress and input comparisons are in `det.video_paths`. The public API and
standalone-header contract are checked by `api.state_digest` and
`api.header_selfcontained`.
