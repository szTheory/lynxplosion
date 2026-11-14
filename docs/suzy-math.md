# Suzy math register policy

This note records the project’s deterministic software policy for the mapped
Suzy math registers. It separates source observations from selected behavior
and leaves hardware accuracy unqualified.

## Source observations and limits

The [Epyx math chapter](https://www.monlynx.de/lynx/lynx9.html) and
[address appendix](https://www.monlynx.de/lynx/hardware.html) describe paired
operand, product, quotient and remainder registers; signed and accumulated
multiplication; divide; and the SPRSYS math flags. They describe 44 ticks for
plain multiplication, 54 ticks for signed or accumulated multiplication, and
176 plus 14 ticks per leading zero in the 16-bit divisor for division. These
are document observations, not measurements on Lynx hardware.

The pinned Math diagnostic has eight cases. Cases 1–7 check meaningful result
bytes at offsets 0–13. Case 5 contains the expected remainder comparison but
comments out its branch, so it cannot establish general remainder behavior.
Case 8 observes timer deltas at offsets 14–15 and leaves 16–17 reserved. The
pinned Memio diagnostic has three cases whose meaningful result ranges end at
offsets 3, 7 and 12; later result bytes are reserved. These source files are
unchanged, and their comments are not treated as hardware measurements.

The source-built math observation completes all eight groups on both selected
profiles with 14/14 scoped arithmetic bytes accepted. Its timing status and
diagnostic bytes remain separately visible and are not a complete latency
trace. The memio source observation exposed and now checks the Green palette
register's four-bit readback; Blue/Red registers preserve their full byte.
These source results agree with the current deterministic software behavior,
not with a physical unit or the complete SUZ-02 contract.

The selected timing constants below are deterministic guest-time policy. No
row-based parameter fitting was performed. No emulator implementation source,
BIOS, firmware, commercial ROM or diagnostic payload is included in this
project’s implementation. Full SUZ-02 agreement, the existing EVD-02 timing
gap and physical hardware qualification remain open.

## Mapped register and alias behavior

The backing window has 48 bytes. CPU accesses at `FC00..FC2F` and `FC40..FC6F`
use `address & 0x3F`; the latter range mirrors the former. `FC30..FC3F`,
`FC70..FC7F` and other unsupported Suzy addresses remain unavailable. Reads
and writes do not fall through to hidden RAM. The existing `FC92` SPRSYS,
`FCB0` JOYSTICK and `FCB1` SWITCHES path remains owned by `core/input.c`; input
sampling still occurs at the granted CPU read edge.

| Mirror address | Software meaning |
| --- | --- |
| `FC52..FC53` | CD low/high bytes; an explicit high write at `FC53` updates the CD sign latch in signed mode |
| `FC54..FC55` | AB low/high bytes; an explicit high write at `FC55` updates the AB sign latch and starts multiply |
| `FC56..FC57` | NP divisor low/high |
| `FC60..FC63` | EFGH input/output bytes, little-endian by ascending address; explicit write at `FC63` starts divide |
| `FC6C..FC6F` | JKLM accumulator/remainder bytes, little-endian by ascending address; explicit write at `FC6C` clears warning |
| `FC92` | Composed SPRSYS status and supported math/input controls |

An even low-byte write clears the paired high byte in either view. That
storage effect does not start an operation or update a sign latch. Command,
sign-conversion and warning-clear actions belong only to the named mirror
ports in `FC40..FC6F`; corresponding base-window writes are ordinary storage
writes. In particular, a write to base `FC15` or `FC23` does not start
multiplication or division.

In signed mode, an explicit high-byte write converts a value greater than
`8000` to its unsigned magnitude and latches a negative sign. `8000` remains
positive under the selected software interpretation. Explicit high writes of
zero retain a negative-zero sign latch; a zero product is not negated and does
not set carry. Low-byte auto-clearing and mode-only SPRSYS writes preserve
existing sign latches. For `FFFD * 0005`, the selected result is
`FFFFFFF1`; a later low-only CD write can therefore use the stale negative CD
sign. These quirks are software policy based on the pinned diagnostic’s
observed cases, not a complete electrical model.

SPRSYS reads expose busy (`80`), warning (`40`), carry (`20`), the existing
lefthand input orientation (`08`) and math unsafe (`04`). Writes accept signed
(`80`), accumulate (`40`), lefthand (`08`), clear unsafe (`04`) and ignored
bit zero (`01`). Unsupported sprite-control bits are rejected atomically.
During an operation, valid SPRSYS mode, clear-unsafe and orientation changes
remain accepted; the pending arithmetic uses its captured mode. Input
orientation still passes through the existing input writer.

While busy, writes to math backing indices `12..17`, `20..23` and `2C..2F`
(hex) are refused before mutation through either view, including repeat
commands. Ordinary SCB backing outside those ranges remains writable. Reads
and valid SPRSYS writes remain available. Completion sets unsafe; SPRSYS bit
`04` clears it. Warning is sticky until the explicit `FC6C` write or reset.

## Arithmetic and status software policy

Unsigned multiplication uses 16-by-16-bit unsigned operands and publishes the
32-bit result at `FC60..FC63`. A signed operation multiplies the magnitudes
captured by explicit high writes and applies two’s-complement negation only
when exactly one latched sign is negative and the product is nonzero. With
accumulation enabled, the signed or unsigned product is added to the captured
32-bit JKLM value modulo 2³². Carry then reports overflow of that unsigned
addition; without accumulation, carry reports a nonzero signed negation.
Overflow sets warning, while a non-overflowing operation does not clear an
existing warning. A write to `FC6C` clears warning and preserves carry.

Division is unsigned regardless of the signed or accumulate controls. The
software result is quotient `EFGH / NP` at `ABCD` and the mathematical
unsigned remainder at `JKLM`. A nonzero remainder sets carry. Division by
zero produces quotient `FFFFFFFF`, remainder zero, warning and carry. The
general remainder rule is an explicit deterministic software choice; the
pinned diagnostic’s disabled remainder branch cannot establish hardware
correctness.

The selected guest-time schedule is 44 ticks for plain multiplication and 54
ticks when signed or accumulation mode is active. Division takes 176 plus 14
times `clz16(NP)` ticks, with `clz16(0) = 16`. No CPU, video or refresh bus
ownership is taken. At an advance target equal to the deadline, the operation
remains busy; the next positive advance applies the pending event before CPU
work at that tick.

## Snapshot and digest continuity

Snapshot format 8 keeps its 88-byte canonical Suzy record at payload offset
`78108`, within the existing `78196`-byte payload. The record still contains
the 48 register bytes, sign and mode flags, operation kind, latched operands,
controls, dividend, accumulator, signs, start and deadline ticks. No schema or
digest input field changed in this plan. Active multiply and divide restore
requires a reachable operation kind, canonical mode/operand latches, exact
checked deadline and a current tick between start and deadline. Pending
results are recomputed from captured inputs rather than trusted from serialized
outputs. Candidate validation completes before any machine state is applied.

The canonical guest-state digest remains domain
`lynxplosion.guest-state.v4`; the existing skeleton golden is unchanged. Its
workload does not issue math writes, and Suzy sensitivity continues to cover
every serialized operation latch as well as all 48 backing bytes, without
treating host observations as guest state. `det.suzy_paths` compares coarse,
one-tick, irregular, interleaved-instance and restored continuations for plain,
signed, accumulated and division operations under both Lynx profiles.

`suzy.lifecycle` checks each selected multiply timing class and division
`clz16` boundaries 0, 1, 15 and 16 at the half-open deadline. It also aligns a
pending CPU status read with completion, and verifies reset/load cancellation
plus whole-instance preservation after a refused load. `suzy.snapshot_refusal`
rehashes malformed operation, sign, mode, idle, deadline, start and backing
register candidates before restore; each refusal preserves every instance
byte, including host watch state. A reachable snapshot exactly at its deadline
remains busy and completes on the next positive advance. A control change made
while busy survives save/restore without changing the operation's captured
mode.

## Evidence boundary

`suzy.tracer`, `suzy.snapshot_tracer`, `suzy.registers`, `suzy.arithmetic` and
`suzy.flags` exercise mapped guest writes and public mapped reads on Lynx1 and
Lynx2. Arithmetic checks compare edge and deterministic seeded unsigned
vectors with a test-owned `uint64_t` model, plus hand-derived signed,
accumulation and divide cases. The phase’s later lifecycle and copied-source
fault controls add boundary, forged-snapshot and mutation evidence. The four
fault controls independently disable the mapped command action, flip one
product bit, delay completion by one tick and flip the saved active sign while
keeping the candidate reachable; each mutant fails only its named semantic
assertion between clean baseline runs.

These checks establish only the stated deterministic software contract. They
do not establish full Suzy behavior, sprite behavior, general remainder
hardware behavior, Lynx timing accuracy or hardware accuracy. Rights, physical
testing, release and EVD-02 stops remain unchanged.
