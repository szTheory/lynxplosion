# Format 6 video and audio snapshot state

Format 6 records the machine's current guest video, logical input and digital
audio state so an accepted restore can continue the same deterministic A/V
stream. Snapshot restore
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

The audio envelope begins at payload offset `77948` and is 88 bytes. Its
fieldwise little-endian layout is:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 10 × 4 | Channel 0..3 records: volume, backup, count, CTLA, CTLB, feedback, shift `u16`, output `i8`, integrator `i8` |
| 40 | 1 | FD50 stereo-disable register |
| 41 | 1 | bounded ring cursor |
| 42 | 8 | bounded pending pulse debt for ring nodes 0..7 |
| 50 | 1 | same-tick device cursor |
| 51 | 1 | same-tick sample cursor |
| 52 | 1 | same-tick event cursor valid flag |
| 53 | 1 | reserved zero |
| 54 | 4 | event sequence within the current guest tick |
| 58 | 8 | event cursor tick |
| 66 | 8 | next sample guest tick |
| 74 | 8 | next sample ordinal |
| 82 | 6 | reserved zero |

The payload therefore ends at 78,036 bytes. The event and sampling cursors are
guest continuation state so output remains identical when an instance is
advanced in different host-sized slices. Caller spans, host event-script
cursors and output-file positions remain host state. The ring fields reserve
the final layout for later channel-link behavior; the current tracer exercises
only ordinary channel 0 and refuses enabled link mode.

The current display address is a 16-bit stream position. At blank ordinal 2,
the backup address is aligned down to a four-byte boundary and transferred to
it; subsequent DMA increments wrap at 16 bits. A saved current address may
therefore be unaligned, and validation does not impose alignment on it.

Restore validates the full candidate before applying any machine byte. The
video policy must match the target instance. Input masks, orientation, flags,
line ordinal, row count, and active-row cursor must be in range. The refresh
deadline stays on the 256-tick phase grid. Future release ticks are at most
five ticks beyond guest time, and DMA and refresh cannot both own the bus at
once. A refresh phase can be overdue while a video DMA grant blocks it; a DMA
request can be overdue while refresh owns the bus. Other overdue requests are
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

These checks establish deterministic format-6 restoration within one exact
build identity. The video cadence, palette sampling, refresh period, and input
mapping remain software policy; snapshot equality does not qualify Lynx
hardware accuracy.

The focused restore and refusal checks are registered as
`api.video_snapshot` and `api.video_snapshot_refusal`, beside the existing
snapshot and timer controls.

## Canonical guest-state digest

`lynx_state_digest` returns 32 SHA-256 bytes for an initialized instance,
including one that has not loaded content. Its versioned stream is encoded in
this order: the NUL-terminated tag `lynxplosion.guest-state.v2`; CPU PC as
little-endian `u16`, A/X/Y/S, P with reserved bits set, flags/opcode/step, EA as
little-endian `u16`, and TMP; MAPCTL and CPU page latch as bytes; guest ticks
and CPU-ready tick as little-endian `u64`; all 65,536 RAM bytes; eight timer
records (backup/count/CTLA/CTLB), pending bits, timer phase policy, the
current-tick event-applied bit, and bus hold as bytes; then the complete
fieldwise video record above and the 88-byte audio envelope. The NUL-terminated
domain is `lynxplosion.guest-state.v2`. Video and audio scalars follow their
documented widths and little-endian rules. The timer event bit is derived from the current tick; an
older cursor timestamp is deliberately omitted. The stream contains no C
structure padding, immutable machine identity (profile, RAM policy/seed,
content or build), snapshot header, derived CPU request/wake state, host watch
state, caller pointers, or replay data. Refused calls leave all 32 output bytes
unchanged; the output span must not wrap or overlap the machine.

The runner writes those 32 public digest bytes as 64 lowercase hex digits in
`obs.state.hex`; its `digests.txt` entry is SHA-256 of the decoded 32 bytes.
The observation digest now covers that entry in addition to RAM, public
registers and time. Adding audio state changes the CPU-loop golden: the digest
makes input, timer, bus, video and audio state visible
even when RAM/register/time projections agree. This is repeatability evidence,
not hardware-accuracy evidence.

Input event scripts and replay cursors remain host-owned. A host checkpoint
taken at tick `T` before applying event `T` is pre-edge and resumes with the
first event whose tick is at least `T`. A checkpoint taken after applying the
event at `T` is post-edge and resumes with the first event strictly after `T`.
The core has no replay cursor or checkpoint wrapper. Any host checkpoint
wrapper must carry that phase marker and the complete script digest; the core
snapshot stores only the current logical input mask. Applying an event and
taking a checkpoint must be ordered consistently with the chosen marker.

The digest sensitivity target is `det.video_sensitivity`; cross-path raw frame,
progress and input comparisons are in `det.video_paths`. The public API and
standalone-header contract are checked by `api.state_digest` and
`api.header_selfcontained`.
