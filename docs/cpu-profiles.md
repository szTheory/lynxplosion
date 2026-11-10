# CPU profiles

What the three public CPU profiles are, where every value in them comes from, and
what the pinned vectors do and do not show. Read this before citing any CPU
result.

Plain statements first.

- **No profile is a baseline.** Which Lynx I or Lynx II behaviour to treat as the
  reference is owner decision O1, and it is open. Nothing here adopts a profile.
  The default profile is `none`.
- **Ticks are not timing evidence.** The cost model is a placeholder (see "Cost
  model"). No claim may cite consumed ticks or cycle counts as Lynx timing.
- **A repeatability digest is not accuracy.** Two runs of this core agreeing with
  each other, or with a committed golden, says the core is repeatable. It does not
  say the core matches a Lynx.
- The vector evidence is software-model evidence (tier T1), not hardware
  evidence. It shows that this CPU agrees with another project's published
  expectations for two commodity 65C02 variants.

## Why there are three profiles

Epyx described the Lynx CPU as "a 65C02 cell" whose instruction set and cycle
counts sit in a non-public VTI specification. So every per-opcode cycle count and
behaviour in this repository is a stand-in taken from another vendor's datasheet,
from a hardware-informed test, or from a software model. None of them is a
measured Lynx fact unless the table below says so (tag `LT-HW`).

| id | name | meaning |
|---|---|---|
| 0 | `none` | The default. What both Lynx profiles agree on. |
| 1 | `lynx1` | The 65SC02 instruction set: no RMB, SMB, BBR or BBS. The `$x7` and `$xF` opcodes are NOPs. |
| 2 | `lynx2` | The Rockwell R65C02 instruction set: adds RMB, SMB, BBR and BBS. |

Ids and names are append-only. They are written into snapshot headers and run
manifests and are never renumbered or renamed. The profile is chosen once when an
instance is created; there is no setter and reset keeps it. An unknown id is
refused with `LYNX_ERR_ARGUMENT`.

Neither profile is a W65C02S: there is no WAI and no STP, and `LYNX_STOP_HALT` is
reserved and never returned.

## The `none` rule

An instruction runs under `none` only where the `lynx1` and `lynx2` table rows are
identical. Where they differ, advance stops with `LYNX_STOP_UNDEFINED` before the
opcode: 0 ticks are consumed and nothing changes (a repeated advance stops at the
same place). A run that completes under `none` is therefore, by the tables,
identical under both Lynx profiles.

The tables differ in exactly 32 opcodes: the ones whose low nibble is 7 or F
(`$07 $0F $17 $1F ... $F7 $FF`). Under `lynx2` they are RMB0-7, SMB0-7, BBR0-7 and
BBS0-7; under `lynx1` they are NOPs of 2 or 3 bytes. The other 224 opcodes are
shared. Bit numbers for RMB, SMB, BBR and BBS come from opcode bits 4 to 6.

The runner prints a note on stderr when a run stops at an undefined opcode under
`none`, naming the opcode, the PC and the `--profile` flag.

A run that reaches an unavailable mapped memory route also writes its partial
observations. Its manifest records `stop_reason = unavailable`, and stderr says
that the requested route has no implemented target; it is not reported as an
undefined CPU opcode.

## `$CB` and `$DB`

On both profiles `$CB` and `$DB` are 1-byte, 1-cycle NOPs that do not poll for
interrupts. This follows the lynx-tests `cpu` Test 8 (tag `LT-HW`, hardware-informed).
The software-model vectors give `$CB` as a 1-byte 2-cycle op and `$DB` as a 2-byte
4-cycle op, which disagrees with that test. Under the exclusion rule the vectors
for those two opcodes are excluded with the reason
`source-disagrees-with-hardware-informed-test`. Their cycle counts are
`unverified`.

## Behaviour common to both profiles

These are plain code in the core, not a runtime switch. Each one is a stand-in
from a datasheet or model, listed in the unverified table below where it has no
Lynx source.

- Decimal flag D is cleared on interrupt and BRK entry.
- `JMP (abs)` reads the high byte at `abs + 1` with a full 16-bit increment (no
  page wrap).
- Read-modify-write instructions read, read again, then write.
- The last instruction byte gets a dummy read where the model has one.
- BRK followed by IRQ: BRK sets I, so a pending IRQ waits for RTI or CLI.
- In decimal mode N and Z are valid and ADC and SBC take one extra cycle.
- Decimal ADC and SBC follow the algorithms checked against the vectors (a
  software model, not a Lynx measurement). Over 288 sampled ADC and 288 sampled
  SBC vectors per variant the N, Z, C, V and A results agreed under the `0xCF`
  flag mask.

## Declared power-on values

All unverified; declared by this project for a deterministic start.

| Item | Value |
|---|---|
| A, X, Y | 0 |
| S | `$FD` |
| P (as read) | `$34` (I set, D clear, bits 4 and 5 set) |
| PC | the load address (loader) or the reset vector (reset seam) |
| interrupt latches, instruction step | clear, 0 |

The machine's power-on does not execute a reset sequence. The reset sequence
exists on the internal seam only.

## Cost model

The default cost model, `flat5`, is a **PLACEHOLDER**: five ticks for every bus
cycle everywhere. Epyx describes normal RAM and ROM and Mikey register accesses as
5 ticks, page-mode opcode reads as 4, Suzy reads as 9-15 and audio DPRAM as 5-20,
so a flat 5 makes straight-line code run about 20% slow. The CPU holds no cost
table; the bus side prices each granted cycle. Device phases refine the costs and
then re-baseline the golden digest.

A second model, `test_skew`, has unequal per-kind costs. It exists only inside the
tests, so that a wrong price shows up as a wrong total and tick budgets can be
split and compared. It is identity-bound in the snapshot and is not reachable from
the public API.

## Vector evidence

| Item | Value |
|---|---|
| Source | SingleStepTests/65x02, commit `2f6980a2d95757486c7bee24355c360e40e2a224` (MIT licence kept next to the data) |
| `lynx1` data | `synertek65c02/v1` |
| `lynx2` data | `rockwell65c02/v1` |
| `none` data | the synertek data, run only where both profiles agree |
| Evidence tier | T1: software-model vectors, not hardware |
| Startup tier | `none`: the vectors inject CPU state, there is no startup tier A claim |
| Fast-gate sample | the first 32 vectors of each of 256 opcode files per variant (16,384 vectors), upstream bytes verbatim, with the SHA-256 of all 512 whole files |
| Full lane | `scripts/vectors-full.sh run` downloads the 512 pinned files, rebuilds the samples byte for byte and runs all 10,000 vectors of each file |

Asserted per vector: final PC, S, A, X, Y, the listed RAM values, no write outside
the listed addresses, and P under mask `0xCF`. **Not asserted:** cycle counts, bus
traces, unlisted reads and ticks (the upstream traces carry generator-specific
dummy reads). Unlisted reads return an address-hashed poison byte, not a constant.

Tiers per opcode: `full` and `state` rows assert the same fields (the tier is a
label recording that the cycle behaviour of the opcode is unsourced or the sources
conflict); `excluded` rows are parsed and counted but not run.

Reasons for an excluded or state-tier row come from a closed list: `source-disagrees-with-hardware-informed-test`
(`$CB`, `$DB`), `profile-none-stops-undefined` (the 32 bit-op opcodes under `none`,
also asserted to stop with UNDEFINED), `shape-unsourced-for-lynx1`,
`cycles-unsourced-decimal` and `cycles-conflict-sources`. An exclusion without a
reason fails the inventory check (state-tier reasons are explanatory labels). There is no "known failure" status.

Committed per-profile inventories (`tests/inventory/vectors/none.txt`, `lynx1.txt`,
`lynx2.txt`) list one row per opcode and are compared byte for byte with what the
runner reports.

Authored scenario vectors cover what the model data cannot: interrupt entry, the
CLI, SEI, PLP and RTI latency, BRK, NMI, the reset seam, the Test 8 shape, bit
operations, reserved NOP lengths and the `JMP (abs)` page edge. Each carries one
source tag from the closed set below and an unverified flag.

## Source tags

| Tag | Meaning |
|---|---|
| LT-HW | lynx-tests hardware-informed result (`cpu` Tests 1, 2, 5, 8 and the Lynx II BBR/BBS 5, 6 and 7 cycle counts) |
| WDC | W65C02S datasheet |
| RKW | Rockwell R65C02 datasheet |
| CMD | CMD G65SC02 datasheet |
| SST-MODEL | the SingleStepTests software model; bus shapes and dummy-cycle addresses are generator choices |
| INFERRED | chosen here from the poll rule; no source measures it |

## Not covered by the vectors, and unverified for the Lynx

Every item below is `unverified`: no Lynx source measures it. Where a tag is shown
it names the stand-in used.

| Behaviour | Stand-in source | Status |
|---|---|---|
| Every per-opcode cycle count except `$03` (one fetch) and BBR/BBS 5, 6, 7 on `lynx2` | WDC, RKW, SST-MODEL | unverified |
| `lynx1` `$x7` and `$xF` length and cycles (CMD says 1 byte; the synertek vectors give 2 and 3 bytes; lynx-tests Test 7 gives the same result for 1-, 2- or 3-byte NOPs) | SST-MODEL, CMD | unverified |
| `$CB` and `$DB` cycles | LT-HW (Test 8, 1 cycle, no poll) | unverified for cycles |
| `$5C` length and cycles | SST-MODEL | unverified |
| Bus address of every dummy cycle: decimal extra cycle, RMW second read, taken-branch and page-cross dummies, the `JMP (abs,X)` fourth cycle | SST-MODEL | unverified |
| Decimal-mode V flag, invalid BCD input, extra-cycle timing | SST-MODEL | unverified |
| RMW absolute,X taking 6 versus 7 cycles | WDC, SST-MODEL | unverified |
| Reset stack access kind (reads chosen) and reset register values | INFERRED | unverified |
| SEI, PLP and RTI interrupt latency | INFERRED (only CLI is LT-HW, Test 1) | unverified |
| Taken-branch interrupt latency; the NMOS taken-branch delay is not modelled | none | unverified, not modelled |
| NMI hijack of an interrupt entry | none | unverified, not modelled |
| Any `lynx1` versus `lynx2` difference beyond the 32 `$x7/$xF` opcodes | none | unverified |
| Power-on register values (above) | declared | unverified |
| IRQ and NMI entry, CLI and SEI latency, `JMP (abs)` at `$xxFF`, Lynx memory map, hardware timing | authored scenarios or later phases | not covered by the vectors |

Epyx's own statements about the Lynx CPU (a 65C02 cell) are the only Lynx-specific
source for the instruction set, and they give no cycle table.

## Where to look

- Profile data: `core/cpu_profile.def` (one row per opcode, the single source of
  both tables).
- CPU: `core/cpu.c`; pricing: `core/bus.c`.
- Vector runner and scenarios: `tests/cpu/`.
- Full lane: `scripts/vectors-full.sh`. It is not part of `scripts/check.sh` and
  never runs from CI; run it at phase verification, on every pin bump and before
  any claim.
- Provenance of the vector data: `fixtures/manifests/sst65x02-*.manifest` and
  `docs/provenance.md`.
