# Format 5 video snapshot state

Format 5 records the machine's current guest video and logical input state so
an accepted restore can continue the same capture stream. Snapshot restore
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

These checks establish deterministic format-5 restoration within one exact
build identity. The video cadence, palette sampling, refresh period, and input
mapping remain software policy; snapshot equality does not qualify Lynx
hardware accuracy.

The focused restore and refusal checks are registered as
`api.video_snapshot` and `api.video_snapshot_refusal`, beside the existing
snapshot and timer controls.
