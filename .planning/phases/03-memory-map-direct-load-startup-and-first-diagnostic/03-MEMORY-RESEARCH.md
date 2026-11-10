# Phase 3 Discuss Research: Memory Map and Bus

**Researched:** 2026-11-10  
**Scope:** MAPCTL, CPU versus raw RAM access, direct-load startup, absent ROM behavior, bus timing seams. This is discuss input only; it does not create CONTEXT.md or PLAN.md and does not implement code.  
**Confidence:** HIGH for address ranges and MAPCTL low-bit meanings from the Epyx Handy specification; MEDIUM for timing opportunities and cc65 startup guidance; LOW / unresolved for model-specific MAPCTL readback and unmapped-ROM behavior.

## Recommendation

Implement one instance-owned memory map in the CPU bus path, with separate raw RAM access and pure inspection. Decode all four independently controlled windows and the fixed `$FFF8` / `$FFF9` locations. Keep the existing all-5-tick model explicitly marked as a placeholder in this phase; record MAPCTL bit 7 and leave page-mode classification and DMA / refresh interactions for the phase where timing evidence can be checked. [CITED: [Epyx Appendix 3: Hardware Addresses](https://www.monlynx.de/lynx/hardware.html), [Epyx CPU/ROM](https://www.monlynx.de/lynx/lynx4.html), codebase references below]

For startup tier A, direct-load bytes into RAM, set the CPU PC to the declared entry point, and do not execute a reset-vector fetch or any ROM routine. Use a documented MAPCTL preset that keeps Suzy and Mikey visible and routes the ROM and vector windows to RAM; `$0C` is the simplest derived candidate (bits 2 and 3 set, bits 0 and 1 clear, bit 7 clear). Keep every other CPU or device value at declared power-on state unless the diagnostic needs it and a source supports the preset. This is a deterministic direct start, not boot evidence. [INFERRED from Epyx MAPCTL definition; constrained by Phase 3 in `.planning/ROADMAP.md` and MEM-02/MEM-04 in `.planning/REQUIREMENTS.md`]

Do not invent a Lynx I/II MAPCTL readback formula. Keep the internal map-control latch distinct from the guest-visible value. A host inspect operation may expose the internal latch without claiming that a guest read returns that same value. The precise model-dependent readback remains an open item because the Epyx map appendix does not state it and O1 is open. [CITED: Epyx Appendix 3; project open decision O1]

## Project constraints and phase boundaries

- O1 remains open: retain both data-defined hardware/CPU profiles, name neither as baseline, and make no suite-pass claim. A map quirk must be tied to a profile only after it has a source. [VERIFIED: `.planning/REQUIREMENTS.md:17-21,163-170` — “No requirement adopts Lynx I or Lynx II as baseline (O1 is open).”]
- O5 (project licence) and O6 (clean-room protocol / GPL exposure) remain open. Preserve the existing draft exposure record; this research is not a license decision. [VERIFIED: `.planning/REQUIREMENTS.md:163-170` — “O5 | Project license”; “O6 | Clean-room protocol and recording of research GPL exposure”]
- Do not use, commit, or distribute copyrighted BIOS, game content, or firmware. Do not run physical-hardware tests. The direct-load path must not accept firmware. [VERIFIED: `AGENTS.md` “Owner direction and workflow” and “Engineering and evidence”; `.planning/AUTONOMY.md` “Explicit stops”]
- Do not move timers, IRQ sources, display DMA, full Suzy behavior, cartridge boot, or a boot shim into this phase. They are later phases or owner-gated. Keep their eventual bus attachment possible without claiming they already work. [VERIFIED: `.planning/ROADMAP.md`, Phase 3 and Phases 4–6]

## Address decoding

The following CPU view is specified by the Epyx Handy address appendix. A MAPCTL low bit of 1 selects RAM for its window; 0 selects the mapped hardware/ROM window. The view applies to CPU accesses from Mikey. Video and refresh generators and Suzy’s sprite engine access the underlying 64 KiB RAM independently of these CPU overlays. [CITED: [Epyx Appendix 3](https://www.monlynx.de/lynx/hardware.html); cross-check: [Atari Explorer, “Lynx Memory Map,” Jan/Feb 1991](https://www.atarimania.com/mags/pdf/Atari_Explorer_January_February_1991.pdf)]

| CPU address | MAPCTL control | CPU-visible target when bit is clear | Target when bit is set |
|---|---|---|---|
| `$0000–$FBFF` | — | RAM | RAM |
| `$FC00–$FCFF` | bit 0 | Suzy | RAM |
| `$FD00–$FDFF` | bit 1 | Mikey | RAM |
| `$FE00–$FFF7` | bit 2 | Boot-ROM region | RAM |
| `$FFF8` | none | RAM, always | RAM, always |
| `$FFF9` | none | MAPCTL, fixed special address | MAPCTL, fixed special address |
| `$FFFA–$FFFF` | bit 3 | Vector/ROM region | RAM |

MAPCTL bit 7 is the sequential/page-mode disable. Bits 4–6 are reserved in the source specification. Consequently, the four mapping bits form 16 low-nibble combinations; cover all 16 in decoder tests instead of treating `$FC00–$FFFF` as one broad overlay. Keep the bit-7 state represented and testable even if this phase does not apply its timing effect. [CITED: [Epyx Appendix 3](https://www.monlynx.de/lynx/hardware.html)]

`$FFF8` must not be swallowed by the vector window or ROM range. `$FFF9` is not an ordinary RAM byte and cannot itself be overlaid; the Epyx appendix describes it as readable/writable through Mikey and write-only through Suzy. The source does not provide the Lynx I/II guest-readback distinction recorded as a maintainer claim in older project research, so that formula remains unverified here. [CITED: [Epyx Appendix 3](https://www.monlynx.de/lynx/hardware.html); lead only: `.planning/research/PITFALLS.md:187,251`]

The source describes the overlays as selecting the CPU-visible RAM or hardware/ROM window; it separately says non-CPU video, refresh, and sprite-engine access sees RAM. Avoid a generic array-only bus that applies CPU overlays to every master. Keep any simultaneous write-through effect for CPU writes to mapped device addresses as an explicit unresolved behavior unless a source is added; do not infer it merely from the word “overlay.” [CITED: [Epyx Appendix 3](https://www.monlynx.de/lynx/hardware.html)]

## Direct-load startup without BIOS

The Epyx map defines a ROM space and vector space, but Phase 3 intentionally supplies no BIOS. Direct-load startup should therefore bypass reset-vector startup and begin at the image's declared entry address in RAM. With the derived `$0C` preset, ROM and vectors resolve to RAM while Suzy and Mikey remain mapped. If later guest code clears bits 2 or 3, those reads would target absent ROM content: do not synthesize a BIOS image, claim boot compatibility, or silently treat all-zero bytes as a real ROM. Prefer an explicit “unavailable ROM” stop/fault path; if that adds too much API scope, keep the condition visibly unsupported and ensure the phase diagnostic cannot enter it. The electrical value returned by a physical system with no ROM is not established by the sources reviewed. [INFERRED from Epyx map; constrained by MEM-02/MEM-04 and `.planning/ROADMAP.md` Phase 3]

The official cc65 Lynx target documentation says its ordinary linker output is a cartridge image and that `lynx-bll.cfg` produces BLL-download-compatible output; it also says a modified startup file and linker config can change this. Do not mistake a `.lnx` cartridge image or its boot path for the project's RAM payload. The project's Phase 3 format/configuration spike must settle how to produce a rights-clean, symbol-bearing direct-load image; this memory-map note does not choose that format. [CITED: [cc65 Atari Lynx documentation](https://cc65.github.io/doc/lynx.html), sections “Binary format” and “Memory layout”]

## Bus, peeks, and device stubs

The current core has one 64 KiB RAM array. CPU `bus_access` reads/writes it directly, the `bus_peek` used by profile-NONE opcode checks also reads it directly, and `lynx_read_memory` copies raw RAM. `lynx__bus_region_of` currently classifies every address as RAM. The CPU already posts one request at a time with cycle kinds, and the machine loop asks the bus for a cycle price and sample offset. [VERIFIED: `core/internal.h:35-47`, `core/machine.c:194-214,221-261,311-323`, `core/bus.c:10-48`, `core/cpu.h:13-47`]

The code's existing vocabulary confirms the intended extension seam. Verbatim source values: `enum { LYNX__COST_FLAT5 = 0, LYNX__COST_TEST_SKEW = 1, LYNX__COST_MODEL_COUNT = 2 };` and `enum { LYNX__REGION_RAM = 0, LYNX__REGION_COUNT = 1 };` [VERIFIED: `core/bus.h:18-23`]

Keep three access concepts separate:

1. **CPU read/write:** MAPCTL-decoded, charged through the existing bus timing seam, and allowed to apply real device side effects at the bus sample point.
2. **Raw RAM inspection:** directly exposes the underlying RAM, never changes machine state or time. Preserve `lynx_read_memory` semantics for result blocks, symbols, snapshots, and hidden RAM.
3. **CPU-mapped peek:** optional additive public inspection for debugger/consumer use; it applies the current CPU mapping but suppresses side effects and consumes no guest ticks. Do not implement it by calling a mutating CPU read with a “debug” flag. [VERIFIED: `include/lynxplosion.h:183-187` — “lynx_read_memory reads the raw 64 KiB RAM, not a CPU-mapped view”; recommendation follows Phase 1 API-05]

For addresses in the mapped Suzy and Mikey windows, use explicit register policies, not a blanket RAM fallback: modeled, read-only, write-only, known-unimplemented, and unallocated addresses should be distinguishable in the stub table. Return or ignore values only according to a documented deterministic placeholder; tag those semantics unverified and do not let a stub masquerade as hardware accuracy. Exact register side effects belong with the timer, display, sound, and Suzy phases. Epyx's address appendix distinguishes access classes and flags unsafe addresses. [CITED: [Epyx Appendix 3](https://www.monlynx.de/lynx/hardware.html)]

When mapped ROM is absent, keep a distinct ROM-unavailable route rather than reading the backing RAM by accident. When an overlay bit selects RAM, read/write that underlying byte. Preserve the current raw RAM inspection path regardless of the CPU mapping. [REC; grounded in Epyx's separate RAM/hardware/ROM targets]

## Variable timing opportunities

Epyx documents normal RAM/ROM reads and writes at 5 ticks, sequential/page-mode opcode reads at 4 ticks, always-available hardware access at 5 ticks, Suzy writes at 5 ticks, Suzy reads at 9–15 ticks, and some eventually available hardware at 5–128 ticks (the source calls 40 probable). It says other hardware activity can pause the CPU. These are source-documented costs/ranges, not a calibrated cycle model for this project. [CITED: [Epyx CPU/ROM](https://www.monlynx.de/lynx/lynx4.html)]

The source is more limited than a full timing rule: it says page mode applies to opcode reads and normal cycles to data reads/writes. The prior Phase 2 bus discussion records a separate lynx-tests maintainer capture that reports instruction-stream operand fetches as eligible and gives closure conditions; it also records unresolved row-boundary, dummy-cycle, sample-point, and stall questions. Treat that as a measurement lead, not as an independently verified hardware contract. [VERIFIED as project note: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/02-bus-cycle-interface.md:31-35,59-77`]

Phase 2 deliberately kept all-5 timing as a placeholder. Verbatim code statement: “Model 0, flat5, is a PLACEHOLDER. Every cycle costs 5 ticks.” The address-to-region function also currently ignores its address and returns the RAM region. [VERIFIED: `core/bus.c:12-17,44-48`]

Recommendation: add region classification and store MAPCTL bit 7 now, but leave flat5 as a labeled placeholder while validating routing. Do not report tick totals as Lynx timing evidence. Phase 4/5 can add timing classifiers and wait/stall sources alongside timers and DMA, with a named cost model and page-mode controls; keep the decoder independent so that work does not require replacing the bus interface. [REC; consistent with `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/02-bus-cycle-interface.md:477-499` and `.planning/ROADMAP.md` Phases 4–5]

## Concrete verification recommendations

- Exercise all 16 combinations of MAPCTL bits 0–3, using distinct backing-RAM bytes and device-stub sentinels to prove each window independently selects the correct target.
- Test `$FFF8` as RAM and `$FFF9` as fixed MAPCTL with every low-nibble combination; test FFFA–FFFF separately from FFF8/FFF9.
- Check CPU read/write routing independently from raw RAM reads. Verify that switching a window to RAM reveals the preserved backing byte; verify non-CPU raw-memory reads are not remapped by MAPCTL.
- Verify each inspection path leaves RAM, device state, CPU state, guest ticks, and snapshot digest unchanged. A mapped peek must not trigger read-clear behavior or other CPU side effects.
- Make tier-A startup assert its declared PC and MAPCTL preset; prove it does not fetch reset vectors or accept a firmware argument. Keep the ROM/vector RAM visible for the diagnostic, and use an explicit named control that attempts an absent-ROM path if the core adopts a stop/fault.
- Include MAPCTL latch and any stub-device state in snapshots. Current payload fields are CPU/clock plus RAM, with no MAPCTL state. Verbatim source values: `#define LYNX__SNAPSHOT_FORMAT 2u` and `#define LYNX__SNAPSHOT_PAYLOAD_LEN 65568u`. Bump the snapshot format when this state layout changes and add mid-diagnostic restore/continue equivalence. [VERIFIED: `core/internal.h:13-24`; `core/snapshot.c:21-28`]
- Test bit 7's decoded state independently of timing. A phase-local check may show that the flag is retained, but must not call its all-5-tick output hardware-accurate. Add named-fault controls for each overlay bit and for conflating raw and mapped reads.

## Adversarial failure modes

| Failure | Why it breaks the phase | Prevention |
|---|---|---|
| Decode one `$FC00–$FFFF` overlay | Loses the four independent MAPCTL windows and mishandles FFF8/FFF9 | Address table plus all-16 low-bit cases |
| Apply CPU overlays to every master | Video/refresh/Suzy can no longer see hidden RAM | Separate CPU view and raw RAM path |
| Return raw RAM for every CPU read/write | Device and ROM mappings are never exercised | Route through MAPCTL before RAM fallback |
| Treat FFF8 as a vector or FFF9 as RAM | Both are explicit exceptions to the overlay windows | Special-case before range decoding |
| Start from a reset vector or fake BIOS | Runs unlicensed/unavailable startup bytes and makes false boot claims | Direct PC start; ROM/vector windows set to RAM for tier A |
| Return plausible zeroes for absent ROM or every unimplemented I/O register | A broken diagnostic may appear to progress for the wrong reason | Label stubs; surface absent ROM distinctly; use a no-run/broken-diagnostic control |
| Use a mutating CPU read for host inspection | Peeks can clear flags, advance ports, or alter guest time | Keep raw read and mapped peek pure and separate from CPU access |
| Claim every CPU cycle costs 5 ticks | Misses page-mode savings, device waits and CPU stalls | Retain explicit placeholder label; defer calibrated timing claims |
| Use the last MAPCTL write as guest readback for every profile | May erase model-specific hardware behavior | Keep latch inspection separate; source any profile-specific readback before locking it |

## Open questions

1. **What does guest code read back from MAPCTL on each hardware profile?** Epyx lists Mikey as read/write and Suzy as write-only but does not specify the model difference noted in bootstrap research. O1 is still open. Keep this unimplemented/explicitly unverified until a suitable source or authorized hardware result exists; do not infer a formula.
2. **What does CPU read/write to a mapped device address do to the hidden DRAM byte simultaneously?** The docs establish the CPU-selected view and the non-CPU RAM view but do not fully specify every overlapping write electrical effect. Do not make a hardware claim for write-through behavior.
3. **What deterministic stop/value is appropriate when guest execution maps absent BIOS ROM back in?** No BIOS data is authorized. Prefer an unavailable-ROM stop; decide whether that requires an additive stop reason in the plan.
4. **Which exact page-mode fetch/stream rules belong in timing code?** Epyx's CPU page covers opcode fetches; the broader stream behavior is a maintainer measurement. Verify against the phase's pinned suite when timing work is in scope, not via this phase's flat5 placeholder.
5. **What values belong in the first diagnostic's startup state?** Phase 3 roadmap explicitly flags conflicting DISPCTL sources. Use only values supported by Epyx or a permitted project-authored test; otherwise leave power-on values and have the fixture initialize them.

## Source exposure log

- Read the Epyx Handy specification's community-hosted HTML conversion, specifically Hardware Addresses, CPU/ROM, and Software Related Hardware Perniciousness. The Atari Lynx Dev documentation index identifies the source as the original Epyx documentation converted to HTML and links a scanned PDF; the PDF viewer exposed no document text in this session, so claims here cite the readable Epyx HTML pages. [Index: [Atari Lynx Dev documentation](https://atarilynxdev.net/doku.php/lynx:doc:start)]
- Cross-checked map-bit and RAM visibility statements against the January/February 1991 Atari Explorer memory-map article hosted by Atarimania. It is a contemporaneous secondary source, not a hardware measurement.
- Read official cc65 Lynx target documentation for cart-image and BLL-output distinctions; no cc65 package or external library is recommended or added by this research.
- Read the project bus, machine, CPU, and public API sources and the Phase 2 bus-cycle discussion cited above.
- No emulator implementation source was opened or used; no BIOS/game binary was opened or used; no firmware was used; no physical hardware test was performed. Open decision O6 and the project's clean-room draft remain in force.

## Evidence and source index

- [Epyx Appendix 3: Hardware Addresses](https://www.monlynx.de/lynx/hardware.html) — map ranges, MAPCTL, special addresses, access classes, CPU-only overlay behavior.
- [Epyx CPU/ROM](https://www.monlynx.de/lynx/lynx4.html) — CPU cycle timing, page-mode statement, device wait ranges, ROM description.
- [Epyx Appendix 1: System Bus Interplay](https://www.monlynx.de/lynx/lynx11.html) — bus masters and CPU pause/stall context.
- [Atari Lynx Dev documentation index](https://atarilynxdev.net/doku.php/lynx:doc:start) — provenance and original scan link for the Handy specification.
- [Atari Explorer, “Lynx Memory Map,” Jan/Feb 1991](https://www.atarimania.com/mags/pdf/Atari_Explorer_January_February_1991.pdf) — independent period cross-check for MAPCTL windows and non-CPU access to underlying RAM.
- [cc65 Atari Lynx documentation](https://cc65.github.io/doc/lynx.html) — target binary formats, linker configuration, memory layout.
- Codebase: `core/bus.c`, `core/bus.h`, `core/internal.h`, `core/machine.c`, `core/cpu.h`, `include/lynxplosion.h`; Phase 2: `discuss/02-bus-cycle-interface.md`.
