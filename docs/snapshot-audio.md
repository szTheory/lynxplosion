# Format 7 audio snapshot state

Format 7 stores digital audio state as a fieldwise continuation record. The
audio record begins at payload offset `77948`, is 160 bytes long, and ends at
payload byte `78108`. All multi-byte values are little-endian; signed output
and integrator values are encoded as their two's-complement `i8` bit patterns.
The format does not copy C structure padding.

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 40 | Four 10-byte channel records: volume, backup, count, CTLA, CTLB, feedback, shift `u16`, output `i8`, integrator `i8` |
| 40 | 1 | FD50 stereo-disable register |
| 41 | 1 | Ring work cursor, node 0..7 |
| 42 | 64 | Eight little-endian `u64` low words of pending pulse debt |
| 106 | 4 | Packed high nibbles, one per node, completing each 68-bit debt count |
| 110 | 1 | Deferred-deadline valid flag |
| 111 | 1 | Same-tick device state: visit count in bits 0..3, audio-applied bit 7 |
| 112 | 1 | Same-tick sample cursor |
| 113 | 1 | Same-tick event cursor valid flag |
| 114 | 4 | Reserved zero |
| 118 | 4 | Event sequence within the current guest tick |
| 122 | 8 | Event cursor tick |
| 130 | 8 | Next sample guest tick |
| 138 | 8 | Next sample ordinal |
| 146 | 8 | Absolute deferred-work deadline |
| 154 | 6 | Reserved zero |

The serializer preserves the full byte domain of guest audio latches unless a
field has structural reserved bits: shift must fit 12 bits and CTLB's high
nibble must be zero, while channel byte registers, CTLB's low nibble, and
signed output and integrator bit patterns are not restricted to a narrower
hardware range by snapshot validation. The event cursor must either be clear
with zero tick and sequence, or refer to the current guest tick with a
sequence from zero through 13. Sequence zero is reachable after device
application when no event has been emitted yet. The sample cursor is on the
400-tick sample lattice; its next ordinal and tick
must agree, be no more than 400 ticks ahead, and not point back to an already
applied sample at the current tick.

The eight ring debts are checked together against the reachable bound
`9 * (guest_ticks + 1)`, with the 68-bit per-node representation preserved.
Nonzero debt requires a valid deferred deadline. Deferred work can be due at
the snapshot tick or at the next tick; a future deadline is only reachable
after the same-tick device visit budget is exhausted. With no debt, the valid
flag and deadline are both zero. Restore validates the complete candidate
before applying it. A refused restore leaves machine bytes, caller snapshot
bytes, and host watch state unchanged; a successful restore clears host watch
state after applying the candidate.

The immutable audio output policy is part of the exact build identity used by
snapshot restore. `lynx_get_identity` exposes the instance's audio policy and
its rate, channel count, and sample width (digital policy 1, 40,000/1 samples
per second, stereo S16). The policy also fixes the sample origin and cadence,
mixing behavior, ring deferral, and event ordering. These are not repeated as
untrusted assertions in the snapshot payload. Restore continues to require
the same exact build, CPU profile, RAM policy and seed, loaded content, and
timing policy; format 7 does not promise migration between builds or older
snapshot versions.

The canonical guest-state digest appends this complete 160-byte fieldwise
audio record after the timer and video state. It contains guest continuation
state only. Caller-owned PCM/event spans, host replay-script cursors, output
file positions, and write-watch configuration are not serialized or included
in the digest. Host replay checkpoints must retain their own pre-edge or
post-edge phase marker and script identity.

This establishes deterministic format-7 continuation within one exact build
identity. It does not qualify audio timing or output against physical Lynx
hardware.
