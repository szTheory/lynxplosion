> DRAFT pending owner decision O6

# Research exposure record (FND-07, draft)

One row per source the research consulted. Each row cites the research record
that established the exposure in the last column. Anything those records omit is
unknown and cannot be reconstructed without opening emulator source, which the
clean-room policy forbids. Where a research file is vague about a date, that
file's own date is given and marked "as recorded".

Initial rows derive from `.planning/research/`. Phase 2 rows cite the Phase 2
discuss and research records. Phase 3 rows cite that phase's source and format
research records.

| Source | Licence | What was read | How | When | Recorded in |
|---|---|---|---|---|---|
| Gearlynx | GPL-3.0 | Source files: core, bus, 6502 timing, memory, media, Suzy stall logic, LCD, Mikey, audio, EEPROM and type headers; README and MCP_README; bundled Handy address tables | Direct read in a scratch clone at commit 94068da (commit dated 2026-11-04; read on or before 2026-11-09, as recorded) | 2026-11-04 to 2026-11-09, as recorded | ARCHITECTURE.md lines 14 and 571; FEATURES.md lines 331 and 335; STACK.md line 260; SUMMARY.md lines 182-184 |
| MAME | GPL-2.0+ | The Lynx driver file and the licence file | Summarising fetch, not a clone | 2026-11-09, as recorded | ARCHITECTURE.md lines 14 and 573; FEATURES.md line 337; STACK.md line 261; SUMMARY.md lines 182-184 |
| Holani | GPL-3.0 | README, build manifest, the ROM-embedding source file and the main emulator source file, repository tree | Direct read, per the research files | 2026-11-09, as recorded | FEATURES.md line 336; STACK.md line 261; PITFALLS.md line 696; SUMMARY.md lines 182-184 |
| Beetle (Mednafen Lynx, libretro) | GPL-2.0 | Documentation pages only (BIOS requirement, lineage); no source read | Documentation fetch; "Not read at source level" | 2026-11-09, as recorded | ARCHITECTURE.md line 137; STACK.md line 261; FEATURES.md lines 338-339 |
| Handy and libretro-handy | zlib | System, ROM, cart and EEPROM source files and the licence file at libretro-handy commit bc55d46; libretro docs | Summarising fetch and GitHub API reads | 2026-11-09, as recorded | ARCHITECTURE.md lines 14 and 572; FEATURES.md line 334; STACK.md lines 133 and 261 |
| lynx-tests | MIT | README, guides, shared build rules, the CPU suite source and the CI workflow | Direct read in a scratch clone at commit a920f01 (dated 2026-09-20) | On or before 2026-11-09, as recorded | ARCHITECTURE.md lines 14 and 567; STACK.md lines 98 and 259 |
| MiSTer AtariLynx core | GPL-2.0 | Repository licence only | API read | 2026-11-09, as recorded | STACK.md line 261 |
| Klaus Dormann 6502 functional tests | GPL-3.0 | Repository licence only | API read | 2026-11-09, as recorded | STACK.md line 262 |
| WDC W65C02S datasheet | Vendor datasheet (no source code) | Instruction set, cycle counts and interrupt and reset sequence prose | Prose read; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/01-cpu-semantics.md`, `discuss/02-bus-cycle-interface.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| Rockwell R65C02 datasheet | Vendor datasheet (no source code) | Instruction set and the bit-operation opcodes (RMB, SMB, BBR, BBS) prose | Prose read; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/01-cpu-semantics.md`, `discuss/03-vectors-and-verification.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| CMD G65SC02 datasheet | Vendor datasheet (no source code) | Instruction set differences from the other 65C02 parts, prose | Prose read; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/01-cpu-semantics.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| Epyx Lynx specification chapters | Vendor documentation (no source code) | The CPU chapters of the Lynx hardware specification | Prose read; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/01-cpu-semantics.md`, `discuss/02-bus-cycle-interface.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| ca65 / ld65 manual | zlib (cc65 documentation) | Assembler and linker options for raw binaries | Documentation read; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/01-cpu-semantics.md`, `discuss/04-api-snapshot-determinism.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| SingleStepTests/65x02 | MIT | README files, the LICENSE file and sampled vector data (JSON test data) at commit 2f6980a2d95757486c7bee24355c360e40e2a224; the generator was not read | Direct read of data and prose; the generator and any emulator source were not opened | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/03-vectors-and-verification.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| lynx-tests `lynx-page-mode.md` | MIT | Prose describing how an emulator models page mode; the document links to that emulator's source and the links were not followed | Prose read; no link followed | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/01-cpu-semantics.md`, `discuss/02-bus-cycle-interface.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| nesdev interrupts page | Community wiki (prose) | Interrupt and reset sequence prose | Prose read; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/02-bus-cycle-interface.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| Design article on cycle-stepped 6502 CPUs | Published article (prose) | Design prose on stepping a 6502 one bus cycle at a time | Prose only; no code | 2026-11-09 (Phase 2 discuss and research) | `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/02-bus-cycle-interface.md`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-RESEARCH.md` |
| cc65 pinned source (zlib; commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2`) | zlib | `libsrc/lynx/bllhdr.s:13-18` and `cfg/lynx-bll.cfg:8-24`; BLL header and linker placement expressions | Direct read at the existing source pin, solely to measure format/output identity; no emulator code copied | 2026-11-10 | `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-STARTUP-RESEARCH.md`, Sources and exposure record |
| Epyx-derived Lynx hardware address table | Vendor hardware documentation (no source code) | MAPCTL reset value, overlay bits/ranges, and fixed FFF8/FFF9 addresses | Read the published hardware table as prose; no hardware was accessed | 2026-11-10 | `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-STARTUP-RESEARCH.md`, Sources and exposure record |
| lynxcc SDK guide, BLL objects | Documentation (no source code) | Ten-byte BS93/BLL header fields and distinction from cartridge ROM objects | Documentation read for format comparison; no code copied | 2026-11-10 | `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-STARTUP-RESEARCH.md`, Format documentation |
| Programming Atari Lynx guide, cc65 uploader | Documentation (no source code) | Load address as execution start; on-disk BLL versus ComLynx transfer sequence | Documentation read for format boundary; no code copied | 2026-11-10 | `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-STARTUP-RESEARCH.md`, Format documentation |

Phase 2 and Phase 3 research opened no emulator implementation source. Phase 3
read the pinned cc65 BLL header/config source only as format evidence; the
project-authored diagnostic does not copy that code. No firmware, commercial
game content, physical hardware, or new package was used. O6 remains draft.
