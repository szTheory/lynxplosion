> DRAFT pending owner decision O6

# Research exposure record (FND-07, draft)

One row per source the research consulted. Rows are built from the research files
under `.planning/research/` only (file and line cited in the last column). Anything
those files omit is unknown and cannot be reconstructed without opening emulator
source, which the clean-room policy forbids. Where a research file is vague about a
date, the research file's own date is given and marked "as recorded".

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
