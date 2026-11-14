# Suzy math register slice

This note describes the first software slice of the private Suzy math device.
It supports the mapped unsigned multiply path and its snapshot continuation.
It does not claim full Suzy behavior or physical accuracy.

## Source observations and limits

The [original Epyx math chapter](https://www.monlynx.de/lynx/lynx9.html) and
[address appendix](https://www.monlynx.de/lynx/hardware.html) describe the
16-by-16-bit product, paired low/high registers, explicit command ports, math
status and a 44-tick unsigned multiply. The chapter also describes unsafe
access reporting and non-reentrant operations, but calls the unsafe indication
unreliable. These are document observations, not measurements from Lynx
hardware.

The project policy interprets the plain multiply latency as 44 guest master
ticks. The operation inputs are latched when the guest writes the command port;
the product and unsafe status publish at the scheduled completion tick. This is
an explicit software interpretation. It has not been calibrated against a
physical Lynx. The pinned source timing rows are not used to tune this latency.

No emulator implementation source, BIOS, firmware, commercial ROM or
diagnostic payload is included or used by this slice. Full SUZ-02 agreement,
the existing EVD-02 timing gap, and hardware qualification remain open.

## Implemented register behavior

The backing window has 48 bytes. CPU accesses at `FC00..FC2F` and `FC40..FC6F`
use `address & 0x3F`, so the latter range mirrors the former. `FC30..FC3F`,
`FC70..FC7F` and other unsupported Suzy addresses remain unavailable. The
existing `FC92` SPRSYS, `FCB0` JOYSTICK and `FCB1` SWITCHES input path remains
owned by `core/input.c`; mapped reads still sample input only on the actual CPU
bus grant.

| Address | Behavior in this slice |
| --- | --- |
| `FC52..FC53` | CD operand, low byte then high byte |
| `FC54..FC55` | AB operand; an explicit write to `FC55` starts multiplication |
| `FC60..FC63` | Product bytes E, F, G, H from least to most significant |
| `FC6C` | Writing this warning port clears the warning latch |
| `FC92` | Composed SPRSYS status and supported math/input controls |

An even low-byte write clears its paired high byte. That storage effect does
not trigger a command or update a sign latch. Only an explicit write to the
named A command port `FC55` starts this operation; an ordinary write to its
base mirror does not. For `CD=00FF` and `AB=0002`, the independently expected
product is `000001FE`, read as `FE 01 00 00` at `FC60..FC63`.

SPRSYS reads expose busy (`80`), warning (`40`), carry (`20`), existing
lefthand input orientation (`08`) and math unsafe (`04`). The sprite status
bits are zero in this slice. Writes accept signed (`80`), accumulate (`40`),
lefthand (`08`), clear-unsafe (`04`) and ignored (`01`) controls; the unsupported
sprite-control mask `32` is rejected before either input or math state changes.
Signed and accumulate mode bits are stored while idle, but this plan does not
implement those arithmetic modes, so a multiply command in either mode is
refused. Divide at `FC63` is also refused until its separately scoped
implementation.

While an operation is busy, writes to math backing registers and aliases are
refused before mutation. Pure mapped reads and validated SPRSYS clear/orientation
writes remain available. The first operation publishes its product and sets
unsafe at completion. Unsafe remains set until SPRSYS bit `04` clears it. The
warning and carry latches remain zero in this unsigned-multiply slice.

## Scheduling and state format

Multiplication is instance-owned and uses one non-owner guest event. It does
not claim the CPU or video bus, alter `cpu_ready_at`, clear the CPU page latch or
change input sampling. A completion event runs immediately before CPU work at
the same tick. `lynx_advance` has a half-open target: if the deadline equals the
target, the operation remains busy; the next positive advance applies it.
Zero-budget calls and mapped peeks do not apply pending work.

Snapshot format 8 appends one 88-byte canonical record at payload offset
`78108`, producing a `78196`-byte payload. The record stores, in order: the 48
register bytes; AB and CD sign latches; mode; warning, carry and unsafe; the
operation kind and latched mode; AB, CD and NP; dividend and accumulator; two
latched signs; start tick and deadline tick. Every multi-byte field is
little-endian and no C padding is serialized. Idle pending-operation fields
are canonical zeroes. Restore validates the complete candidate before applying
any machine state; formats 1 through 7 are refused.

The canonical guest-state digest domain is
`lynxplosion.guest-state.v4`. It appends the same fieldwise Suzy record after
video and audio state. `det.suzy_sensitivity` checks all 48 backing bytes,
signs, mode, status, active progress and exclusion of host write-watch state.

## Evidence boundary

The public guest tracer runs on both Lynx CPU profiles and checks mapped
operand writes, busy status, completion and product readback. Its snapshot
companion snapshots a pending operation, restores it into a fresh equal-identity
instance, checks the exact half-open deadline and compares the complete RAM,
snapshot and digest suffixes. API controls cover mirror mapping, pure mapped
inspection, hidden-RAM isolation, atomic unavailable spans, active-write
refusal and invalid snapshot candidates.

These checks establish deterministic behavior for the selected software
contract. They do not establish the full register set, signed arithmetic,
accumulation, division, sprite behavior, hardware timing or hardware accuracy.

## Early integration record

The updated skeleton observation digest is
`7cd1222c5491855a8e7f50b5bc44dbd42871acd04bfc54ceaf69e2fe48221898` on both
native Clang and the exact GNU checkout. Before accepting that value,
`det.suzy_sensitivity` passed and the pre-update golden produced the same
different digest in both environments. The previous golden matched the old
state format. The required two-run, two-instance, golden, golden self-test,
claim-template and claim-template self-test checks pass after the refresh.

The early native full inventory initially found two API self-test expectation
drifts after Suzy became mapped: the map-route mutant now first fails the RAM
route sentinel, and the raw-read mutant now first fails the raw backing-RAM
value. Both controls now name those semantic failures. The native API and
memory groups passed 72/72 after repair. The full native inventory was not
repeated here; final native full qualification remains with the later phase
qualification plan.

The final early GNU checkout built and passed all 252 registered CTests with
the two existing declared page-mode observation skips and no failures. The
optimized configuration was `RelWithDebInfo` with
`-O2 -g -UNDEBUG`, ASan+UBSan, and non-recovering sanitizers; the effective
compile and link commands retained assertions and did not define `NDEBUG`.
Its API and memory groups also passed 72/72 after the self-test repair. The
installed ShellCheck identities were native 0.11.0 and GNU 0.9.0; this was an
identity check only, not a lint run.
