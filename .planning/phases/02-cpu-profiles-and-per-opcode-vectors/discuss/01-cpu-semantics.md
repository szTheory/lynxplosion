# Phase 2 discuss lens 01: CPU profile semantics (hardware / CPU-bus)

Date: 2026-11-09. Status: research input for the Phase 2 discuss synthesis. It
decides nothing. Owner decision O1 (baseline model) stays open; every
recommendation below keeps Lynx I and Lynx II as peer profiles.

Labels: [SO] source observation (with source and section), [INF] inference,
[UNK] unknown, [REC] recommendation. "HW-informed" means the lynx-tests
maintainer states the suite was "made by analyzing actual hardware"; the
per-test hardware run record is not published, so treat it as a strong report,
not a measurement made by this project.

Clean-room note: no emulator source was opened for this lens. The lynx-tests
`lynx-page-mode.md` guide (MIT, prose) describes hardware observations and how
Gearlynx represents them in pseudocode, and links to Gearlynx source files.
Those links were not followed. The guide prose was read and should be listed
in `docs/policy/exposure-record.md` as documentation exposure.

## Sources used (all fetched 2026-11-09)

| Tag | Source | Where |
|---|---|---|
| EPYX4 | Epyx "Handy" hardware spec, ch. 4 CPU/ROM | https://www.monlynx.de/lynx/lynx4.html ("The CPU is a 65C02 cell imbedded in the Mikey IC ... found in the VT1 specification"; "CPU Cycle Timing"; "CPU Cycle Tick Summary"; "CPU NMI Latency") |
| EPYX10 | Epyx spec, ch. 14 System Reset/Power Up | https://www.monlynx.de/lynx/lynx10.html ("1. Disable interrupts 2. Clear decimal mode ...") |
| WDC | WDC W65C02S datasheet, 8 Apr 2022 | https://www.westerndesigncenter.com/wdc/documentation/w65c02s.pdf : sec. 3.4 IRQB, 3.6 NMIB, 3.11 RESB; Table 4-1 (cycles by mode + notes 1-5); Table 5-2 opcode matrix; Table 6-4; Table 7-1 "Microprocessor Operational Enhancements" (p. 30) |
| RKW | Rockwell R65C02/R65C102/R65C112 data sheet, Doc. 29651N52, Order No. 2149 | https://www.6502.org/documents/datasheets/rockwell/rockwell_r65c00_microprocessors.pdf (copy read: https://smartbox.benryves.com/mediawiki/images/4/40/Datasheet-CPU-R65C02.pdf, scanned, read by OCR plus visual check of the matrix page): "Interrupt Request (IRQ)" pin text; Table 2 new opcodes; Table 4 operational enhancements; "Instruction Set Op Code Matrix" p. 1-11 (bytes/cycles + footnotes) |
| CMD | California Micro Devices G65SC02 data sheet (5/00; GTE design lineage) | https://www.6502.org/documents/datasheets/cmd/cmd_g65sc02_mpu_mar2000.pdf : General Description (64 instructions, 178 opcodes); "Enhanced Operational Characteristics" Table V (reserved-opcode bytes/cycles, reset stack behaviour, RMW abs,X) |
| CA65 | cc65 ca65 manual | https://cc65.github.io/doc/ca65.html : `--cpu` list ("65SC02 - first CMOS instruction set (no bit manipulation, no wai/stp)", "65C02 - CMOS with Rockwell extensions", "W65C02 - ... has bit manipulation and wai/stp"); sec. 11.92, 11.100, 11.105 |
| LT | drhelius/lynx-tests, MIT, commit a920f01164d37edff94939993fbc0551e5b7000b (2026-09-19) | `README.md` (cpu/ section), `cpu/main.c` (expected-value table), `cpu/tests.s` (Tests 1-8), `lynx-page-mode.md` |
| SST | SingleStepTests/65x02, MIT, commit 2f6980a2d95757486c7bee24355c360e40e2a224 (2025-05-10) | root `README.md` (methodology); `synertek65c02/README.md`, `rockwell65c02/README.md`, `wdc65c02/README.md`; first test of selected `v1/<op>.json` files (sampled, see section 4) |

## 1. What CPU the Lynx uses (question a)

### Source observations

- [SO EPYX4] "The CPU is a 65C02 cell imbedded in the Mikey IC. The specifics
  of its instruction set and register operations are found in the VT1
  specification." and "While the number of CPU cycles for each instruction are
  defined in the VTI spec, the number of system 'ticks' used by each CPU cycle
  are defined here."
- [UNK] The VTI ("VT1" is very likely an OCR/typo of VTI, VLSI Technology Inc.)
  65C02 cell specification is not public. No search found a VTI 65C02 cell
  data sheet. So the authoritative per-opcode cycle source named by Epyx is
  unavailable; every per-opcode cycle number this project uses is a proxy from
  another vendor's sheet or a hardware-informed test.
- [SO CA65] cc65 defines `65SC02` = no bit manipulation, no WAI/STP;
  `65C02` = Rockwell extensions (bit ops, no WAI/STP); `W65C02` = bit ops plus
  WAI/STP. (Prior project research: cc65's compiler target table maps `lynx`
  to `CPU_65SC02`; the doc page names no CPU.)
- [SO CMD] G65SC02: "expanded microprocessor instruction set (from 56 to 64
  instructions), and expanded operational codes (from 151 to 178)": that is
  the 65SC02 set with no RMB/SMB/BBR/BBS and no WAI/STP. Table V lists
  `X3, X7, XB, XF` as 1-byte, 1-cycle NOPs.
- [SO RKW] R65C02 matrix: "210 Op Codes"; RMBn/SMBn `2 5`; BBRn/BBSn `3 5**`
  ("Add 1 to N if branch occurs to same page; Add 2 to N if branch occurs to
  different page"); `$CB` and `$DB` cells empty (reserved; Table 4 "All are
  NOPs (reserved for future use)").
- [SO WDC] W65C02S has RMB/SMB/BBR/BBS plus WAI (`$CB`) and STP (`$DB`), 212
  opcodes; Table 7-1 lists reserved NOP sizes excluding `$CB`/`$DB`.
- [SO LT cpu/tests.s, cpu/main.c] Test 7 executes raw `$07 $EA` (RMB0),
  `$87 $EA` (SMB0), `$0F $EA $18` (BBR0), `$8F $EA $18` (BBS0) and expects
  Lynx I `FF 00 02 02` (no memory change, no branch) and Lynx II
  `FE 01 01 01` (bit cleared, bit set, both branches taken). `main.c` accepts
  either tuple and prints "LYNX 1" / "LYNX 2"; a mixed tuple fails.
- [SO LT] Test 8 runs `$03,$13,...,$F3` and `$0B,...,$FB` (including `$CB`
  and `$DB`) five times each after `CLI` with a Timer 6 IRQ pending and
  expects the IRQ return address to be `sentinel_a + 1` (after the `INX` that
  follows the block). Only one expected value exists, so both models share it.
- [SO LT] Test 6 has one expected value for `$5B` (1-byte), `$44` (2-byte),
  `$5C` (3-byte) NOPs: no register or N/V/Z/C change, PC advances by 1/2/3.
- [SO LT lynx-page-mode.md] A Lynx II logic-analyzer capture shows a run of
  `$03` NOPs costs 4 ticks each with page mode on and 5 with it off, i.e. one
  instruction-stream fetch per `$03`: [INF] `$03` is a 1-cycle instruction on
  Lynx II. The same guide states Lynx II BBR/BBS cost 23 / 28 / 33 ticks (not
  taken / taken / taken across a 256-byte page) with an open stream: [INF]
  that is 5 / 6 / 7 CPU cycles (4+4+5+5+5, +5, +5), matching RKW's `3 5**`.

### Synthesis

- [INF] Lynx I behaves like the 65SC02 instruction set (GTE/CMD G65SC02-like:
  no bit ops, no WAI/STP). Lynx II behaves like the Rockwell R65C02 set (bit
  ops, no WAI/STP). Neither behaves like a W65C02S: `$CB`/`$DB` are NOPs that
  do not poll IRQ on both models (Test 8).
- [UNK] Whether the two models differ in anything else at the CPU level
  (cycle counts, reserved-NOP bus behaviour, decimal-mode details, reset
  sequence). No source shows a difference beyond the 32 bit-op opcodes, and no
  source shows sameness beyond what Tests 1-6 and 8 check.
- [UNK] The length of the Lynx I `$x7` / `$xF` NOPs. Test 7's Lynx I tuple is
  produced identically by 1-byte NOPs (operand `$EA` runs as NOP, `$18` runs
  as CLC, then `JMP`) and by 2-byte / 3-byte NOPs (they skip the operand bytes
  and land on the same `JMP`). The test comments say "NOP on Lynx I" for each
  operand byte, which implies 1 byte, but the result bytes do not prove it.
  CMD Table V says 1 byte / 1 cycle; SST `synertek65c02` models `$07` as
  2 bytes / 3 cycles, `$77` as 2 bytes / 4 cycles, `$0F`/`$8F` as 3 bytes /
  3 cycles. These disagree, and the Lynx I value is unmeasured.
- [UNK] Only bit 0 forms (`$07`, `$87`, `$0F`, `$8F`) are hardware-informed.
  Bits 1-7 are inferred from the opcode pattern.
- [UNK] Which physical units the maintainer ran each test on (README gives no
  per-test model record). The Test 7 dual tuple implies both models were run.

## 2. Profile as data (question b)

### 2.1 Constraints from the repo

- [SO `tests/cmake/abi.cmake`] `core.nm_globals` fails on any nm symbol of
  type `B b D d C`: "no writable data symbol ... so const integer tables only".
- [INF] A `static const` array of pointers (or of structs holding pointers,
  including `const char *` mnemonic strings) built as PIC on ELF lands in
  `.data.rel.ro`, which nm reports as `d`/`D` and fails the gate on Linux,
  while Mach-O places it in `__DATA_CONST` and passes on macOS. That is the
  Phase 1 Mach-O vs ELF difference. Integer-only `const` arrays land in
  `.rodata` (`r`/`R`) on both.
- [SO `core/internal.h`] `LYNX__HALT_OPCODE 0xDB` makes `$DB` halt the
  skeleton. On both Lynx profiles `$DB` is a 1-byte NOP (LT Test 8). The
  skeleton halt opcode must not survive into the CPU (see footguns).
- [SO `include/lynxplosion.h`] `enum { LYNX_PROFILE_NONE = 0 };` and
  `lynx_config.profile`. Phase 2 adds model values without changing the
  default.

### 2.2 Recommended table shape

[REC] One X-macro definition file is the single source of truth, expanded into
integer-only `const` arrays indexed by an integer profile id. No pointers and
no strings in the core.

```c
/* core/cpu_opcodes.def  (one row per opcode, 256 rows)
   OP(opcode, lynx_i_kind, lynx_i_mode, lynx_ii_kind, lynx_ii_mode, flags) */
OP(0x07, K_NOP,  M_NOP1,   K_RMB, M_ZP_RMW,  F_BITOP_SLOT | F_LYNX1_LEN_UNVERIFIED)
OP(0x0F, K_NOP,  M_NOP1,   K_BBR, M_ZP_REL,  F_BITOP_SLOT | F_LYNX1_LEN_UNVERIFIED)
OP(0x03, K_NOP,  M_NOP1,   K_NOP, M_NOP1,    F_NO_IRQ_POLL)
OP(0xCB, K_NOP,  M_NOP1,   K_NOP, M_NOP1,    F_NO_IRQ_POLL)   /* not WAI */
OP(0x5C, K_NOP,  M_NOP_5C, K_NOP, M_NOP_5C,  F_TIMING_UNVERIFIED)
OP(0x69, K_ADC,  M_IMM,    K_ADC, M_IMM,     F_DECIMAL_EXTRA)
...
```

```c
/* core/cpu_profile.c */
typedef struct lynx__cpu_decode { uint8_t kind, mode, flags, pad; } lynx__cpu_decode;
enum { LYNX__CPU_PROFILE_LYNX_I = 0, LYNX__CPU_PROFILE_LYNX_II = 1, LYNX__CPU_PROFILE_COUNT = 2 };
const lynx__cpu_decode lynx__cpu_decode_table[LYNX__CPU_PROFILE_COUNT][256] = { ... };
```

- [REC] Store `kind` (operation) and `mode` (bus micro-sequence) per opcode
  per profile; derive bit number for RMB/SMB/BBR/BBS from opcode bits 4-6.
  Do not store a "base cycles" column the CPU obeys. Cycle counts must emerge
  from the micro-sequence the `mode` selects, so each cycle is a real bus
  access; a separate expected-cycles table lives in the tests with a source
  per value and is compared against the emitted bus cycles.
- [REC] 2 profiles x 256 x 4 bytes = 2 KiB of `.rodata`. Indexing is
  `table[profile][opcode]`, one load, no branch on profile in the hot path.
- [REC] A test asserts the set of opcodes whose rows differ between the two
  profiles equals exactly `{$x7, $xF}` (32 opcodes). Any new diff must update
  that test and cite a source; this keeps "Lynx I and II are small diffs"
  enforced, not hoped for.
- [REC] Profile-level scalar behaviours that are identical in both Lynx
  profiles (D cleared on interrupt, JMP-indirect fix, CMOS RMW read-read-write,
  CMOS dummy read of last instruction byte on index carry, BRK-then-IRQ, valid
  N/Z in decimal mode, decimal extra cycle) are implemented as fixed CMOS
  behaviour, not runtime switches. A switch that no defined profile flips is an
  untested branch. They are recorded as named, sourced constants in the
  profile manifest (test-side data) so a later profile (or an NMOS reference
  for negative controls) can turn them into switches when a source demands it.
- [REC] Flags bits (uint8): `F_NO_IRQ_POLL` (1-cycle NOPs do not update the
  interrupt-poll latch), `F_DECIMAL_EXTRA` (ADC/SBC add a cycle when D=1),
  `F_BITOP_SLOT` (member of the 32 profile-dependent opcodes),
  `F_TIMING_UNVERIFIED` (bus sequence or count not sourced for Lynx),
  `F_LYNX1_LEN_UNVERIFIED`. The `_UNVERIFIED` bits never change execution;
  they exist so tests and the inventory can enumerate unverified expectations
  mechanically from the same table the CPU uses.
- [REC] Public API: add `LYNX_PROFILE_LYNX_I = 1` and `LYNX_PROFILE_LYNX_II = 2`
  (model names, not chip names). `lynx_config_default` keeps
  `LYNX_PROFILE_NONE`. Under `NONE` the core must not silently pick a CPU
  table (the API lens decides whether `NONE` keeps the skeleton for the Phase 1
  goldens or refuses to run). Claims name the profile.

### 2.3 Behaviour fields and their recommended Phase 2 values

| Field | Lynx I | Lynx II | Source / status |
|---|---|---|---|
| RMB/SMB `$x7` | NOP | RMBn/SMBn zp, 2 B, 5 cyc | LT Test 7 (bit 0, HW-informed); RKW matrix `2 5`; Lynx I length [UNK], cycles [UNK] |
| BBR/BBS `$xF` | NOP | 3 B, 5 cyc, +1 taken, +1 more page cross | LT Test 7 (bit 0); RKW `3 5**`; LT page-mode guide tick counts (Lynx II) |
| WAI `$CB`, STP `$DB` | 1 B NOP, no IRQ poll | same | LT Test 8 (HW-informed, shared value); cycles: [INF] 1, unmeasured |
| `$x3`, other `$xB` | 1 B, 1 cyc, no IRQ poll | same | CMD Table V, WDC Table 7-1; LT Test 8; LT page-mode guide (`$03`, Lynx II, 1 fetch) |
| `$x2` reserved (`02 22 42 62 82 C2 E2`) | 2 B, 2 cyc | same | CMD/WDC tables; Lynx [UNK]; LT does not test |
| `$44` | 2 B, 3 cyc | same | CMD/WDC; length HW-informed (LT Test 6); cycles [UNK] |
| `$54 $D4 $F4` | 2 B, 4 cyc | same | CMD/WDC; Lynx [UNK] |
| `$5C` | 3 B, 8 cyc | same | CMD/WDC say 8; SST models 4 (all three sets); length HW-informed (LT Test 6); cycles [UNK], bus addresses [UNK] |
| `$DC $FC` | 3 B, 4 cyc | same | CMD/WDC; SST agrees; bus address of the read [UNK] (matters: may hit a Mikey/Suzy register) |
| Decimal ADC/SBC | valid N,Z,C; +1 cycle | same | WDC/RKW/CMD tables; result, C, Z, N for valid BCD HW-informed (LT Test 3); V flag [UNK]; invalid BCD digits [UNK]; +1 cycle and its bus address [UNK] on Lynx |
| D on IRQ/NMI/BRK/reset | cleared | same | WDC Table 7-1, RKW Table 4, CMD Table V; IRQ case HW-informed (LT Test 2) incl. restore by RTI |
| BRK | 2 B; pushes P with B=1, bit5=1; sets I, clears D | same | WDC Table 4-1 note 5, Table 6-4 BRK row; LT Test 4 checks return address only (its `$30` byte is hard-coded, not measured) |
| BRK then IRQ | BRK completes, then IRQ | same | WDC Table 7-1, RKW Table 4; Lynx [UNK] |
| JMP (abs) | page fix; 6 cyc | same | LT Test 5 (fix, HW-informed); RKW matrix `3 6` for all `$6C`; WDC Table 4-1 says 6, Table 7-1 says "one additional cycle" at xxFF (internal inconsistency); SST 6 |
| RMW at effective address | read, read, write | same | WDC/RKW/CMD tables; Lynx [UNK] (matters for registers with access side effects) |
| Index page-cross dummy | re-read of last instruction byte | same | WDC/RKW/CMD tables; Lynx [UNK] |
| ASL/LSR/ROL/ROR abs,X same page | 6 cyc | same | CMD Table V and WDC Table 7-1 say 6; RKW matrix says 7; SST says 6; INC/DEC abs,X 7 everywhere sampled; Lynx [UNK] |
| STA abs,X | 5 cyc always | same | WDC Table 4-1 note 1; RKW matrix |
| IRQ poll point | sampled before the last cycle; CLI/SEI/PLP effects one instruction late | same | RKW IRQ pin text ("sampled at the falling edge of phi2 prior to the last cycle of the instruction"); CLI case HW-informed (LT Test 1); SEI, PLP, RTI cases [INF] from that rule, unverified |
| Taken-branch IRQ delay (NMOS quirk) | not modelled | same | no 65C02 source; [UNK]; leave off and mark |
| Reset | 7 cycles, I=1, D=0, vector `$FFFC` | same | WDC 3.11, RKW RES text; stack access kind: CMD says CMOS "writes program counter and status register to stack" vs NMOS "reads"; WDC silent; Lynx [UNK] |
| Reset values of A/X/Y/S and N V Z C | chosen by policy | same | WDC P diagram shows N V Z C undefined; [UNK] for Lynx; deterministic choice must be labelled chosen, not measured |
| NMI | edge, vector `$FFFA` | same | WDC 3.6, RKW; EPYX4 says the Mikey NMI path has clocked delays and is a debug feature; no Lynx source drives it in Phase 2 |

## 3. Delta list across NMOS 6502, 65SC02, Rockwell 65C02, WDC 65C02 (question c)

All CMOS rows: [SO WDC Table 7-1 / Table 5-2; RKW Tables 2-4 and matrix;
CMD Table V]. "Lynx" column is what the core implements for both profiles
unless stated.

| # | Behaviour | NMOS 6502 | 65SC02 (GTE/CMD, Synertek) | R65C02 | W65C02S | Lynx core |
|---|---|---|---|---|---|---|
| 1 | New ops BRA, PHX/PHY/PLX/PLY, STZ (4 modes), TRB/TSB (zp, abs), INC A/DEC A, `(zp)` for ORA/AND/EOR/ADC/STA/LDA/CMP/SBC, BIT #/zp,x/abs,x, JMP (abs,X) | absent | present | present | present | present (both) |
| 2 | RMB/SMB/BBR/BBS (`$x7`, `$xF`) | absent (illegal ops) | absent (NOPs; CMD: 1 B / 1 cyc) | present | present | Lynx I NOP, Lynx II present |
| 3 | WAI `$CB`, STP `$DB` | illegal ops | NOPs | NOPs | WAI / STP | 1 B NOP, no IRQ poll (both) |
| 4 | Undefined opcodes | various illegal ops, some jam | NOPs with defined sizes (CMD Table V) | "All are NOPs", sizes not tabulated | NOPs with sizes (Table 7-1) | NOPs, sizes per section 2.3 |
| 5 | JMP (xxFF) | high byte from same page | fixed, +1 cyc | fixed, +1 cyc (matrix: 6 always) | fixed (Table 4-1: 6) | fixed, 6 |
| 6 | Index page-cross dummy read | invalid address | last instruction byte | last instruction byte | last instruction byte | last instruction byte [UNK on Lynx] |
| 7 | RMW bus pattern | read, write old, write new | read, read, write | read, read, write | read, read, write | read, read, write [UNK on Lynx] |
| 8 | RMW abs,X same page | 7 | 6 (CMD) | 7 per matrix | 6 per Table 7-1 | 6 for shifts, 7 for INC/DEC [UNK] |
| 9 | D after reset | undefined | 0 | 0 | 0 | 0 |
| 10 | D on interrupt | unchanged | cleared | cleared | cleared | cleared |
| 11 | N, V, Z after BCD | invalid | valid, +1 cyc | valid, +1 cyc | valid, +1 cyc | valid N Z C, +1 cyc; V [UNK] |
| 12 | Interrupt during BRK fetch | BRK lost | BRK, then IRQ | BRK, then IRQ | BRK, then IRQ | BRK, then IRQ [UNK on Lynx] |
| 13 | Reset stack cycles | reads | writes (CMD) | not stated | not stated | [UNK]; recommend reads (no RAM mutation) marked unverified, see 5 |
| 14 | 1-cycle NOP IRQ polling | n/a | n/a in sheet | n/a in sheet | n/a in sheet | no poll (LT Test 8) |
| 15 | STA abs,X | 5 | 5 | 5 | 5 | 5 |
| 16 | BIT # flags | n/a | Z only (common doc; sheets ambiguous) | same | Table 6-4 lumps all modes | Z only [INF, mark] |

## 4. Software vector sets vs primary sources (questions d, e)

[SO SST README] "To generate each test set, an implementation is used that
conforms to all available documentation ... passes all other published test
sets; and has been verified by usage in an emulated machine." Each opcode file
has 10,000 random single-instruction tests on a flat 64 KiB RAM with a
per-cycle `[address, value, read|write]` list. `synertek65c02` = "Sometimes
called the 65SC02" (no bit ops); `rockwell65c02` = bit ops, no WAI/STP;
`wdc65c02` omits STP/WAI.

Sampled first test per file (bytes advanced / cycles listed):

| Opcode | SST synertek | SST rockwell | SST wdc | Primary source | Lynx evidence |
|---|---|---|---|---|---|
| `$03 $13 $83 $F3 $0B $5B $EB $FB` | 1 B / 1 | 1 / 1 | 1 / 1 | CMD, WDC: 1 / 1 | Test 8 no poll; `$03` 1 fetch (Lynx II) |
| `$CB` | 1 B / 2 | 1 / 2 | (WAI, absent) | CMD: 1 / 1 | Test 8: no IRQ poll; a 2-cycle polling op would take the IRQ inside the block |
| `$DB` | 2 B / 4 | 2 / 4 | (STP, absent) | CMD: 1 / 1 | Test 8 runs `$DB` five times as 1-byte NOPs |
| `$x2` reserved | 2 / 2 | 2 / 2 | 2 / 2 | 2 / 2 | none |
| `$44` | 2 / 3 | 2 / 3 | 2 / 3 | 2 / 3 | length (Test 6) |
| `$54 $D4 $F4` | 2 / 4 | 2 / 4 | 2 / 4 | 2 / 4 | none |
| `$5C` | 3 / 4 | 3 / 4 | 3 / 4 | WDC, CMD: 3 / 8 | length (Test 6) |
| `$DC $FC` | 3 / 4 | 3 / 4 | 3 / 4 | 3 / 4 | none |
| `$07` / `$77` | 2 / 3, 2 / 4 | RMB 2 / 5 | RMB 2 / 5 | CMD (65SC02): 1 / 1; RKW: 2 / 5 | Test 7 (cannot see length) |
| `$0F` / `$8F` | 3 / 3 | BBR/BBS 5-7 | same | CMD: 1 / 1; RKW `3 5**` | Test 7; page-mode guide 5/6/7 |
| `$1E` ASL abs,X same page | 6 | 6 | (sample crossed: 7) | RKW matrix 7; CMD, WDC 6 | none |
| `$FE` INC abs,X | 7 | 7 | 7 | RKW 7 | none |
| `$6C` JMP (abs) | 6 | 6 | 6 | RKW 6; WDC 6 / +1 | fix (Test 5) |

Findings:

- [SO] The SST generator disagrees with the primary data sheets on `$5C`
  cycles (4 vs 8), on `$CB`/`$DB` sizes and cycles (vs CMD 1/1), and on the
  65SC02 `$x7`/`$xF` sizes (vs CMD 1/1). It agrees with them on most other
  reserved NOPs. Its bit-op opcodes agree with RKW.
- [INF] The SST `$CB` (2-cycle) and `$DB` (2-byte, 4-cycle) models are
  inconsistent with Lynx Test 8 if those instructions poll IRQ like other
  multi-cycle instructions. They must not be used as Lynx expectations.
- [INF] SST sample counts above come from one test per file; a full-file
  scan is needed to confirm that every test in a file has the same length
  and cycle count (decimal and page-cross variants differ, e.g. `$69`/`$E9`
  showed 2 or 3 cycles depending on D).

### 4.1 Mapping and exclusions

[REC] Lynx I runs the SST `synertek65c02` set; Lynx II runs the SST
`rockwell65c02` set; `wdc65c02` is not used for either. Exclusion tiers per
opcode and profile, each recorded in the inventory with a reason and source:

- `full`: compare final state and every bus cycle (address, value, R/W).
- `state_only`: compare registers, flags, PC and RAM, not the cycle list.
  Use when the size is sourced but the cycle sequence is not.
- `excluded`: not compared; replaced by project-authored vectors.

| Opcodes | Lynx I | Lynx II | Reason |
|---|---|---|---|
| `$CB`, `$DB` | excluded | excluded | SST contradicts LT Test 8 and CMD; authored 1 B NOP vectors |
| `$x7`, `$xF` (32) | excluded | full | SST synertek contradicts CMD; Lynx I length [UNK]; authored vectors per CMD, marked unverified |
| `$5C` | state_only | state_only | SST 4 cyc vs WDC/CMD 8 cyc; length HW-informed |
| `$DC $FC $54 $D4 $F4 $44 $x2` | state_only | state_only | sizes agree; bus addresses of dummy reads unsourced for Lynx |
| ADC/SBC with D=1 | state_only for valid BCD; excluded or separate class for invalid BCD digits, and V flag noted | same | extra cycle's bus address unsourced; invalid-BCD and V behaviour are generator choices |
| RMW abs,X shifts | state_only | state_only | 6 vs 7 conflict between primary sheets |

[REC] A pre-plan spike (cheap, host-side script, not committed data) scans
every SST file of both sets and diffs synertek vs rockwell on the 178 shared
opcodes (size, cycle count, bus pattern). Any shared-opcode divergence becomes
an explicit [UNK] row instead of silently becoming a profile difference.

### 4.2 Project-authored vectors (CPU-04)

SST covers no interrupts, no reset and no multi-instruction timing. [REC]
Author small scenario vectors driven by a test bus that raises IRQ/NMI at a
given cycle:

1. CLI then INC with IRQ pending: IRQ after INC (cite LT Test 1).
2. SEI with IRQ pending: one IRQ taken after SEI ([INF] from RKW poll rule,
   mark unverified).
3. PLP clearing I, RTI clearing I: PLP delayed one instruction, RTI immediate
   ([INF], unverified).
4. IRQ in decimal mode: D=0 in handler, restored by RTI (LT Test 2).
5. Block of every `$x3`/`$xB` (incl. `$CB`, `$DB`) after CLI with IRQ pending:
   IRQ return address = after the first multi-cycle instruction (LT Test 8),
   plus the control block of `$EA` NOPs where the IRQ lands inside (LT Test 8
   part B).
6. BRK: return address = BRK+2, pushed P has B and bit 5 set, I set, D clear
   (WDC Table 6-4; LT Test 4 for the return address only).
7. IRQ asserted during BRK's opcode fetch: BRK completes, then IRQ (WDC/RKW
   tables; unverified on Lynx).
8. NMI edge entry and vector `$FFFA` (WDC 3.6).
9. Reset: 7 cycles, I=1, D=0, PC from `$FFFC` (WDC 3.11, RKW); stack-access
   kind and S/A/X/Y/NVZC values marked chosen/unverified.
10. Bit ops bits 0-7 on Lynx II, and Lynx I NOP behaviour with an operand
    byte that is itself a visible instruction (e.g. `$E8` INX) so the authored
    vector pins the chosen 1-byte length; marked unverified with the CMD
    citation.

Every expected value carries a source tag from a closed set (`LT-HW`,
`DATASHEET-WDC`, `DATASHEET-RKW`, `DATASHEET-CMD`, `SST-MODEL`, `INFERRED`)
and an `unverified` flag; the check prints per-profile counts per tag so the
share that rests only on models or inference is visible.

## 5. Unknowns to mark unverified (question d)

- [UNK] All per-opcode cycle counts on Lynx except: `$03` one fetch (Lynx II),
  BBR/BBS 5/6/7 (Lynx II), and what the page-mode test results imply for
  `LDX #` and `LDA abs` (Lynx II). The VTI cycle spec is not available.
- [UNK] Lynx I `$x7`/`$xF` NOP length and cycles.
- [UNK] `$5C` cycles (8 per data sheets, 4 per SST) and which addresses its
  cycles read.
- [UNK] `$CB`/`$DB` cycle count (1 inferred).
- [UNK] Bus address of every dummy cycle (index carry, RMW, reserved NOPs,
  decimal extra cycle, BBR extra cycle, branch-taken cycle). On Lynx a read of
  a Suzy register costs 9-15 ticks vs 5 for RAM (EPYX4 tick summary), and some
  registers have access side effects, so the address is observable.
- [UNK] Decimal mode: V flag, invalid BCD inputs, extra-cycle timing.
- [UNK] RMW abs,X same-page cycle count (6 vs 7).
- [UNK] Reset stack access kind and initial register values.
- [UNK] Taken-branch IRQ-delay quirk on 65C02 / Lynx.
- [UNK] SEI, PLP, RTI IRQ latency on Lynx (only CLI is HW-informed).
- [UNK] Any CPU difference between Lynx I and Lynx II beyond the 32 bit-op
  opcodes.
- [UNK] Whether EPYX10's "Clear decimal mode" step implies the VTI cell does
  not clear D at reset (it may be belt and braces). Lynx boot code clears it
  anyway; the core should still clear D on reset per all three CMOS sheets,
  marked unverified for the VTI cell.

[REC] How tests mark them: the unverified flag lives in the vector manifest
row (test-side), mirrored by `F_*_UNVERIFIED` table bits so a test can assert
the two agree. An unverified expectation still runs and gates regressions
(it pins the chosen behaviour), but it is counted separately and is never
cited as hardware accuracy. Promotion to verified needs a hardware-informed
source (owner-gated physical testing stays a stop).

## 6. Adversarial pass (question e)

1. Circularity. SST vectors and the core both derive from the same public
   data sheets; a pass shows agreement with a documentation model, not with a
   Lynx. Mitigation: per-tag counts; hardware-informed LT values kept
   separate; claims say "spec-model tier".
2. Generator disagrees with primary sheets (section 4). A naive "100% SST
   pass" would encode `$5C`=4 cycles, `$DB`=2 bytes and Lynx I `$07`=2 bytes,
   each contradicted by a primary or HW-informed source.
3. LT Test 7 cannot distinguish NOP lengths on Lynx I; LT Test 4 does not
   measure pushed B (writes `$30` unconditionally); LT Test 3 does not check V
   or invalid BCD; LT Test 1 covers CLI only; LT Test 8 checks where the IRQ
   lands, not cycle counts; bit ops tested for bit 0 only. Passing replicas of
   these tests proves less than their names suggest.
4. Auto-detection. LT `main.c` accepts either model tuple. A Lynx II core
   configured as Lynx I would still print PASS. The harness must assert the
   tuple equals the selected profile (prior research P6 agrees).
5. Self-generated expectations. If expected values are produced by running
   the core (or a script sharing its tables), every table bug becomes a
   passing vector. Authored expectations must come from cited text, written
   independently of the decode table, and reviewed.
6. Count right, kind wrong. Matching cycle counts on a flat bus can hide a
   wrong cycle kind (opcode fetch vs operand fetch vs data read vs dummy).
   On Lynx, page mode discounts only instruction-stream fetches (LT
   page-mode guide; EPYX4 "The CPU makes use of the page mode circuitry in its
   op-code reads"). [REC] The bus-cycle interface tags each access with a
   kind (opcode, operand, data read, data write, dummy read, stack, vector)
   and Phase 2 tests compare address, value, R/W and kind. SST has no kind
   field, so kind expectations are project-authored and partly [INF].
7. Silent exclusion creep. An exclusion list can hide real failures.
   [REC] Exclusions are data rows with reason and source; the inventory test
   fails on an exclusion without both; a negative control adds a reasonless
   exclusion and expects failure.
8. Random coverage misses edges. Add directed vectors for zero-page wrap
   (`(zp,X)`, `zp,X` at `$FF`, `(zp)` pointer at `$FF`), stack wrap at
   `$0100`/`$01FF`, branch and BBR at page ends, JMP (`$xxFF`), index carry
   into page `$FF`, decimal results at `$99`.
9. Wrong-profile control too narrow. Swapping profiles only changes 32
   opcodes, so the CPU-05 control fails only bit-op vectors (as required);
   add a second control that maps `$CB` to a 2-cycle polling op and expects
   the Test 8 replica to fail, so the no-poll rule is also guarded.
10. Undefined becomes "spec". Deterministic choices for undefined reset
    values or NMOS-undefined flags must be labelled as chosen policy; a
    golden digest built on them proves repeatability only.
11. Phase 1 skeleton leak. `$DB` halts the skeleton today. If the halt
    survives (or a test fixture relies on `$DB` to stop), the CPU silently
    diverges from Test 8. [REC] Remove the halt opcode when the CPU lands;
    stop runs by tick budget or a host-side watch, never by an opcode.
12. Interrupt source equivalence. Phase 2 has no Mikey; IRQ scenario vectors
    assert the CPU IRQ line from a test bus. LT Tests 1, 2 and 8 depend on
    Timer 6 and Mikey's interrupt latch, so a Phase 2 replica checks the CPU
    rule only; the full LT `cpu` suite becomes evidence only once Mikey
    exists (later phase).

## 7. Patterns, anti-patterns, footguns

- Pattern: decode table indexed by `[profile][opcode]`, mode selects a
  micro-sequence of bus cycles, cycle counts emerge. Anti-pattern: a cycle
  table the CPU sleeps on, which cannot later express per-access cost.
- Pattern: profile names describe models (`LYNX_I`, `LYNX_II`), and
  behaviour rows cite sources. Anti-pattern: naming a profile after a chip
  ("65SC02", "R65C02"), which imports that chip's unverified details.
- Anti-pattern: pointer tables of handler functions (fail the ELF gate) or
  `const char *` mnemonic tables in the core. Keep mnemonics in host tools.
- Footgun: WDC Table 4-1 vs Table 7-1 disagree on JMP (abs) cycles; RKW
  matrix vs CMD/WDC Table 7-1 disagree on RMW abs,X; cite the row used.
- Footgun: BIT # sets only Z on CMOS; Table 6-4 lists BIT once with N V Z.
- Footgun: decimal-mode ADC/SBC with D=1 are half of SST's random cases;
  invalid BCD digits there encode the generator's model.
- Footgun: using LT Test 4's `$30` as evidence for pushed B.
- Footgun: RMW on hardware registers (e.g. TSB on MAPCTL) toggles Lynx II
  page mode because of the read-back difference (LT page-mode guide). The
  CPU's RMW read-read-write pattern makes this observable; keep it exact.

## 8. Recommendations (one-shot, for the synthesis)

1. Model Lynx I as the 65SC02 opcode set and Lynx II as the Rockwell R65C02
   opcode set, both with `$CB`/`$DB` as 1-byte non-polling NOPs; the only
   table difference is the 32 `$x7`/`$xF` opcodes, enforced by a test.
2. Profile tables: X-macro `.def` file expanded into a
   `const uint8_t`-only `[2][256]` decode array (kind, mode, flags); no
   pointers or strings in the core; bit number from opcode bits.
3. Cycle counts emerge from per-mode bus micro-sequences; expected counts live
   in tests with a source per value.
4. Fixed CMOS behaviours common to both profiles are code, not switches;
   record them as sourced constants in the test-side profile manifest.
5. Add `LYNX_PROFILE_LYNX_I`/`LYNX_PROFILE_LYNX_II`; default stays `NONE`;
   no implicit baseline (O1).
6. Vector mapping: Lynx I <- SST `synertek65c02`, Lynx II <- SST
   `rockwell65c02`, with the full/state_only/excluded tiers in section 4.1;
   run a full-file spike before planning.
7. Author interrupt/reset/BRK/NOP-block scenario vectors (section 4.2), each
   value tagged with a closed source tag and an unverified flag.
8. Bus cycles carry a kind tag; tests compare address, value, R/W and kind.
9. IRQ polling: latch at the penultimate cycle; 1-cycle NOPs do not update
   the latch (`F_NO_IRQ_POLL`); no taken-branch quirk; SEI/PLP/RTI cases
   marked inferred.
10. Remove the skeleton `$DB` halt when the CPU lands.
11. Negative controls: profile swap fails bit-op vectors (CPU-05); `$CB` as a
    polling op fails the Test 8 replica; reasonless exclusion fails the
    inventory.
