# Feature Research

**Domain:** Accurate, deterministic, headless-drivable Atari Lynx emulator core (Lynxplosion)
**Researched:** 2026-11-09 (all web/GitHub sources accessed 2026-11-09)
**Mode:** Ecosystem (what existing Lynx emulators/cores do) plus feasibility of the smallest first slice
**Overall confidence:** MEDIUM-HIGH. Hardware facts and emulator behavior come from primary specs and source I read directly. Maintainer claims, doc/code disagreements and unverified license questions are labelled.

## How to read this file

Claim labels (per owner policy, kept distinct):

- **[SO]** source observation: I read the primary document or source file myself (URL or file named).
- **[MR]** maintainer report: a README/doc statement I did not independently test.
- **[INF]** inference from sources; not directly stated.
- **[REC]** recommendation for Lynxplosion; a proposal, not a decision.
- **[UNK]** unknown or unverified; needs a spike or primary source before it becomes a requirement.

Evidence tiers (from PROJECT.md; they do not imply each other):

- **T1** spec/vector expectation (Epyx spec, hand-derived vectors, per-instruction vectors)
- **T2** public hardware-informed test (drhelius/lynx-tests, MIT)
- **T3** original diagnostic written for this project, with manifest
- **T4** independent emulator comparison (Gearlynx, Holani, Handy lineage, MAME; behavior cross-check, not an oracle)
- **T5** measured physical hardware (owner-gated; out of scope for ordinary work)
- **T6** game/homebrew progress
- **T7** installed-host behavior (clean consumer, frontend)
- **R** same-core repeatability hash. This is deliberately not an accuracy tier.

Complexity: LOW (days), MEDIUM (a week or two), HIGH (multi-week or research-heavy).

---

## Key corrections and facts verified against dated seed input

| Seed claim | Verdict | Evidence |
|------------|---------|----------|
| 160x102 display | Confirmed with nuance. The LCD is 480x102 physical subpixels, i.e. 160 RGB triads by 102 lines. A frame is 105 lines (102 visible + 3 VBlank) at the default 60 Hz setup. Pens are 4 bits per pixel, 80 bytes per line, 8160 bytes per buffer. Pixels are rectangular and triads are square. | [SO] monlynx.de lynx2.html, lynx5.html; lynx-tests `lynx-video-timing.md`; MAME comment "160x105 total, 160x102 visible" |
| 4096-color palette | Confirmed. 16 palette entries, each 12 bits: `GREEN0-F` at $FDA0-$FDAF (G in bits 3-0) and `BLUERED0-F` at $FDB0-$FDBF (B bits 7-4, R bits 3-0). Only 16 distinct colors per frame (per-line palette changes allowed). | [SO] Gearlynx-bundled copy of Handy address table (`hardware_addresses.md`); Mednafen doc "12-bit BRG with 4096 RGB triplets" |
| Frame rate | Programmable by Timer 0 (line) and Timer 2 (frame lines) plus PBKUP. Default setup gives 59.90 Hz (159 us x 105 lines); 50 Hz and 75 Hz settings exist; spec warns 50 Hz "may not be usable". Libretro cores advertise 75 FPS (a core convention, not the default hardware rate). Holani derives refresh from the Timer 0 backup register. | [SO] lynx5.html, lynx-video-timing.md, docs.libretro.com handy/beetle_lynx, holani `lynx.rs` |
| CPU: Epyx 65C02 vs cc65 65SC02 | Both statements are real and reconcilable. Epyx: "a 65C02 cell imbedded in the Mikey IC". cc65 `src/common/target.c` maps target `lynx` to `CPU_65SC02`. MAME uses `G65SC02` with the comment "stz, but not bbr bbs". lynx-tests `cpu/` test 7 expects Lynx I: RMB/SMB/BBx act as NOPs (results FF 00 02 02); Lynx II: bit ops work (FE 01 01 01). | [SO] monlynx lynx4.html; cc65 target.c; MAME lynx.cpp; lynx-tests `cpu/main.c` |
| "512-byte boot ROM" | Confirmed. It lives in Mikey, reads data from the cart (or tape) and executes it. Reset vector is fetched from ROM at $FFFC/D with the ROM overlay enabled. | [SO] lynx4.html, lynx10.html |
| "Gearlynx requires a BIOS" | Confirmed as a statement. It is a maintainer statement about the whole emulator. Whether `.o` homebrew loads without it is [UNK]: the source has a `PrepareForHomebrew()` path for non-cart media. | [SO]/[MR] Gearlynx README, `gearlynx_core.cpp` |
| "Most lynx-tests have no machine-readable output" | Refined. On screen they paint PASS/FAIL text plus raw hex bytes, but each suite keeps raw results in a `volatile uint8_t g_results[]` RAM array, with expected values in a C table. A RAM-mailbox capture (read `g_results` via the linker map) is therefore feasible without OCR. | [SO] `cpu/main.c`, `shared/util.h` in drhelius/lynx-tests; capture method is [INF] |

---

## Feature Landscape

### Table Stakes (credible first slice; missing = not a Lynx emulator, or not credible as a verified core)

IDs are for requirements traceability. "Tier" is the evidence that can prove the feature.

#### A. Hardware coverage

| ID | Feature | Why expected | Complexity | Depends on | Tier | Notes |
|----|---------|--------------|------------|-----------|------|-------|
| HW-01 | Named model/CPU profile (a data-driven opcode/behavior table, not a hardcoded "Lynx") | Core Value: every claim ties to an exact model | MEDIUM | none | T1, T2 (cpu/ test 7), T4 | [REC] Make the profile a parameter from day one. The Lynx I vs II delta known from the sources is RMB/SMB/BBx (NOP vs implemented), so a second CPU profile is a LOW delta. Other model deltas are [UNK] (see Gaps). |
| HW-02 | 65C02-class CPU: all opcodes, BCD flag behavior, BRK, JMP (ind) page fix, reserved NOPs of 1/2/3 bytes, IRQ entry/latency semantics | lynx-tests `cpu/` checks these: SEI/CLI latency, D flag cleared on IRQ, BCD flags, BRK = 2 bytes with B+bit5 set, JMP-indirect fix, undocumented NOPs, 1-byte NOPs do not acknowledge IRQs | MEDIUM | HW-01 | T1 (vectors), T2 (`cpu/`, 8 tests), T4 | Cycle accuracy depends on the bus (HW-03). A per-instruction vector set can be authored before any device exists. |
| HW-03 | Bus/timing model: 16 MHz tick (62.5 ns); CPU page-mode opcode read 4 ticks vs normal RAM 5 ticks; Mikey/Suzy/cart access times differ; cart read cycle 15 ticks; video DMA has highest priority and can stall CPU or take the bus from Suzy; Suzy bus-grant latency up to 40 ticks | Seed: fixed instructions-per-frame is unsafe. Spec: "CPU cycles can not be used as the timing elements in small software timers" | HIGH | HW-02 | T2 (`page-mode/` 6 tests, `timers2/`, `sdoneack/`), T1 | Choose the work unit here (tick-level scheduler vs event-driven with exact catch-up). This is the single highest-risk architectural decision (ARCHITECTURE/PITFALLS researchers own it). |
| HW-04 | Memory map: 64 KB RAM, Suzy $FC00-$FCFF, Mikey $FD00-$FDFF, ROM overlay $FE00-$FFF7, MAPCTL $FFF9, vectors; MAPCTL page-mode bit 7; Suzy mirror aliases $FC40-$FC6F | Everything software does | LOW-MEDIUM | none | T1, T2 (`memio/` 3 tests, `page-mode/`) | Seed warning: MAPCTL changes CPU-visible mappings, not a device's independent RAM access. |
| HW-05 | Mikey timers: 8 timers, 3-bit prescale selector, 8-bit down counter + backup, reload/one-shot, link chains (Group A: 0->2->4; Group B: 1->3->5->7->Audio0..3->1), interrupt per timer, timer-done and reset-timer-done quirks, shared prescaler phase, register arbitration/access timing | Frame/line timing, audio and UART are all built from timers | HIGH | HW-03 | T1, T2 (`timers/` 7, `timers2/` 10), T4 | Spec pitfalls to model: a timer with reload disabled and DONE already set does not count; reset-timer-done is a level signal (continuous IRQ if left high). [SO] lynx3.html. |
| HW-06 | Interrupt controller + CPU sleep (CPUSLEEP $FD91) incl. the "sleep is broken in Mikey" behavior (sleep persists only while Suzy holds the bus) and wake-ups | Real software (sprite waits) depends on it | HIGH | HW-05, HW-09 | T2 (`sdoneack/` 10 tests) | [SO] lynx4.html |
| HW-07 | Display: DISPCTL ($FD92: DMA enable, flip, 4-bit/colour), DISPADR, PBKUP, per-line DMA (80 bytes in ten 8-byte bursts, 192 ticks apart), 16-entry 12-bit palette, 105-line frame with 3 VBlank, flip mode reads from bottom | The visible output and the DMA bus stall both come from here | MEDIUM | HW-05 | T1, T2 (`sprites4/` display DMA contention), T3 | Raw output contract: pen indices (4 bpp) + palette words + line/frame timing metadata, separate from host RGB conversion. |
| HW-08 | Input hardware: JOYSTICK $FCB0 (d-pad, Option 1, Option 2, "Inside"/"Outside" fire buttons, bit meanings swap with LEFTHAND), SWITCHES $FCB1 (Pause = bit 0, cart I/O inactive bits), power/IODAT bits | The seven physical inputs A, B, Option 1, Option 2, Pause, d-pad | LOW | HW-04 | T1, T3 | [UNK] which of Inside/Outside is "A": confirm against spec before freezing the logical-input contract. Pause+Option1 / Pause+Option2 (restart / flip) are software conventions, not hardware; keep them out of the core. |
| HW-09 | Suzy sprite engine, minimal: SCB list walk, literal sprites 1-4 bpp, normal/background/XOR types, clipping, collision buffer and depositories, executed asynchronously with bus cycles | Nearly all games and cc65 TGI output draw through Suzy | HIGH | HW-03, HW-04 | T2 (`sprites1..5/`, 40 tests total), T4 | Stage it: literal + normal first (`sprites1/`), then types (`sprites3/`), RLE/linking/collision (`sprites4/`), then scale/stretch/tilt (`sprites5/`). Spec errata to carry: shadow-inverter swap, pen-E collision quirks, palette-loader page-break bug. |
| HW-10 | Suzy math unit: signed/unsigned 16x16 multiply, accumulate, divide, divide-by-zero, $8000 edge case, latency, "unsafe access" quirks | Used by games; `math/` suite | MEDIUM | HW-09 (shares SPRSYS busy bits) | T2 (`math/` 8 tests), T1 | Not needed for first light; early because the tests are small and exact. |
| HW-11 | Audio: 4 channels built from timer + LFSR + integrator, one-shot/integrate/reload, channel linking, volume/attenuation, mono mix; panning/stereo register (MSTEREO) exists | Raw audio output is a required observation | HIGH | HW-05 | T2 (`audio/` 6, `audio2/` 7), T4 | Emit guest-tick-stamped channel events and a sample stream; host filtering (Gearlynx low-pass 3.5 kHz default) is presentation. Lynx II stereo behavior is [UNK]. |
| HW-12 | UART/ComLynx single-console: TX/RX timing at 9600/2400/1200/62500 baud, TXEMPTY/TXRDY, parity, overrun, break, loopback echo (a console hears its own output) | Spec and lynx-tests (`uart/` 6, `uart2/` 5, `uart3/` 6) | MEDIUM | HW-05 | T2, T4 | Later slice. Two-console protocol (`uart4/`) is separate (see Anti-features). |
| HW-13 | Reset/power-up state: documented register reset values, Mikey ROM overlay on, Suzy disabled, "known variances" (REST pin, UART TXD) | Determinism: "fresh isolated state" must be a defined state | LOW-MEDIUM | HW-04 | T1 | [SO] lynx10.html. Many registers are "x" (undefined) at reset: model as explicit, configurable undefined-fill, never host randomness. |

#### B. Content

| ID | Feature | Why expected | Complexity | Depends on | Tier | Notes |
|----|---------|--------------|------------|-----------|------|-------|
| CT-01 | `.lnx` loader: 64-byte header = magic `LYNX`, bank0 page size (u16), bank1 page size (u16), version (u16 = 1), cart name (32), manufacturer (16), rotation (u8: 0 none, 1 left, 2 right), `aud_bits` (bit 0 = AUDIN), EEPROM byte, 3 spare. Bank size = page-size field x 256 (e.g. 0x400 -> 256 KB). | Dominant container; cc65 emits it; every emulator loads it | LOW | none | T1 (header vectors), T3 | [SO] cc65 `libsrc/lynx/exehdr.s`, libretro-handy `cart.h`/`cart.cpp`. Reject malformed headers with a defined refusal postcondition. |
| CT-02 | Headerless cart (`.lyx`/raw) handling | Handy/Gearlynx accept it | LOW | CT-01 | T3 | Without a header the bank sizes, rotation, AUDIN and EEPROM are unknown. Existing cores guess from a CRC database (Handy `cart_db.h`; Gearlynx internal DB). [REC] Require explicit sidecar config instead of a bundled commercial-title database. |
| CT-03 | Homebrew `.o`/BS93 direct load: 10-byte header `80 08 <load addr BE> <len BE> "BS93"`, payload copied to RAM, PC set, no boot ROM | Gives a boot path that needs no firmware | LOW | HW-13 | T3 | [SO] MAME `quickload` comment; Handy `HANDY_FILETYPE_HOMEBREW` + `PresetForHomebrew`. The state it presets (registers, Mikey, MAPCTL) must be written down as a profile, because it stands in for the boot ROM. |
| CT-04 | Cartridge access path: Suzy-mediated reads/writes through RCART0 $FCB2 / RCART1 $FCB3; 8-bit address shift register (MSB first, loaded on rising CartAddressStrobe) + counter that auto-increments per read and resets while strobe high; page size 256 B to 2 KB by cart; bank 0/1 selection; AUDIN/bank-1 swap; cart power via the IODAT bit; 12-tick write lockout of Suzy | Real boot code and bank-switched games run through this, so "flat ROM array" is wrong | MEDIUM-HIGH | HW-04, HW-03 | T1, T3 (bank/shifter vectors), T4 | [SO] monlynx lynx7.html and lynx3.html. The spec section lacks some exact register bits (RCART/AUDIN naming); cross-check Handy `cart.cpp` and Gearlynx `media.cpp`. |
| CT-05 | Content identity: hash of header-stripped payload bytes + separately recorded declared config (bank sizes, rotation, EEPROM, AUDIN, model hint) | Ties saves, claims, and evidence to exact content | LOW | CT-01 | R | [REC] Identity is a hash plus a config tuple, and a hash proves identity, not rights. Persistent data keys on this identity. |
| CT-06 | EEPROM (93Cxx) emulation + persistence contract | Needed by several commercial games and homebrew; saves are core-visible | MEDIUM | CT-04, CT-05 | T1 (Microwire command vectors), T3, T4 | [SO] libretro-handy `eeprom.cpp`: EEPROM byte low 3 bits = 1..5 -> 93C46/56/66/76/86; bit 7 = 8-bit organization (else 16-bit); command/address bit counts differ per chip; also a "busy" phase. cc65 ships `eeprom46/66/86.s`. **Deferred** past the first slice (see MVP) but the persistence boundary (core exposes bytes; host decides where they live) belongs in the first API. |
| CT-07 | Boot ROM interface: a 512-byte ROM image slot in the machine, loaded by the host, mapped at $FE00 | Real-BIOS path, owner-supplied and outside source control | LOW | HW-04 | T2/T4 (only with owner-supplied image) | Separate from the free/HLE boot decision (see Boot below). |

#### C. Core and host

| ID | Feature | Why expected | Complexity | Depends on | Tier | Notes |
|----|---------|--------------|------------|-----------|------|-------|
| CH-01 | Headless runner: create instance, load content, set logical input for a guest-time interval, advance by a bounded guest-time amount, read raw outputs, reset, destroy; no host clock, filesystem, window or audio device inside the machine | PROJECT.md core requirement; every serious core is "library plus frontends" (Holani = library + `holani-min` CLI + gtk + libretro + debug; Gearlynx `--headless` needs MCP/debug-monitor/ComLynx) | MEDIUM | HW-02..HW-07 as available | R, T3 | Gearlynx's `--headless` is not a standalone batch runner: it requires an MCP/monitor/ComLynx driver [SO README]. A pure run-N-ticks-and-dump runner would be a differentiator. |
| CH-02 | Logical input sampling at explicit guest-time boundaries (A, B, Option 1, Option 2, Pause, Up/Down/Left/Right) | Determinism requires input as data, not devices | LOW | HW-08 | R | Core takes logical buttons in hardware orientation. Orientation/remap and illegal opposite directions (Gearlynx "Allow Up+Down" option) are host policy. |
| CH-03 | Raw video output: pen-index frame (160x102, 4 bpp) + the 16 palette words as of each line (or frame, as a named contract) + frame boundary markers | "Raw video/palette" observation | LOW-MEDIUM | HW-07 | T3, R | Do not freeze one-palette-per-frame without checking raster-effect use (lynx-tests `video-timing` guide describes per-line palette effects, [SO]). |
| CH-04 | Raw audio output: per-channel events and mono mixed samples stamped with guest ticks; sample rate is an output contract, not a hardware constant | "Raw audio samples/events" observation | MEDIUM | HW-11 | T2, R | Existing cores pick host rates: libretro Handy 22050 Hz, Beetle Lynx 44100 Hz, Gearlynx low-pass default 3500 Hz. These are presentation choices. |
| CH-05 | Whole-machine state serialize/restore | Every non-trivial core has it; it also powers determinism checks | MEDIUM | all devices | R | [REC] Treat serialization as a determinism tool first (restore + run == continuous run), a user save-state UX second. Holani uses `postcard`/serde; Gearlynx and libretro-handy have state serializers. |
| CH-06 | Error/refusal contract: malformed content, unsupported profile, oversize, bad BIOS size/CRC, with defined state after refusal | PROJECT.md requirement | LOW | CT-01 | T3 | Handy/libretro-handy silently falls back to "internal fallback" on a bad BIOS (`rom.cpp`); that implicit fallback is the opposite of a named profile. Refuse or report explicitly. |
| CH-07 | One documented local check command aligned with the fast PR gate | Owner policy | LOW | none | n/a | |

#### D. Verification

| ID | Feature | Why expected | Complexity | Depends on | Tier | Notes |
|----|---------|--------------|------------|-----------|------|-------|
| VF-01 | Pinned, reviewed fixture build: cc65 + lynx-tests at an exact revision, built from source, with manifest (source, recipe, checksum, license/notices, allowed use) | Owner policy; no cc65 installed on this host today | MEDIUM | none | n/a | [SO] lynx-tests: MIT, latest tag 1.4.9 (2026-08-13, commit `b583dc43d532d3c41563a6fa35420614fbcead89`), pushed 2026-09-19; upstream CI builds cc65 from `master` (unpinned), so pin your own. cc65 itself must also be license-reviewed for the runtime libs linked into fixtures [UNK, check]. |
| VF-02 | Test-ROM runner with exact inventory (suite -> test count; e.g. cpu 8, timers 7, timers2 10, math 8, memio 3, page-mode 6, sdoneack 10, sprites1-5 8 each, audio 6, audio2 7, uart 6, uart2 5, uart3 6) and result capture by RAM mailbox | PROJECT.md: exact inventories, named faults, visible skips | MEDIUM | VF-01, HW-02+ | T2 | [INF] `g_results[]` symbol addresses come from the cl65 map/label file; expected values are in `main.c` tables (some tests accept several values, e.g. Lynx I vs II). Record the model-specific expectation set per profile. |
| VF-03 | Named-fault negative controls | PROJECT.md | MEDIUM | VF-02 | T3 | e.g. flip the CPU profile and see `cpu/` test 7 fail; break BCD flags; add a one-tick page-mode error. The gate must fail for the intended fault. |
| VF-04 | Component/vector tests: per-instruction (opcode x flags), timer unit tests, EEPROM command vectors, LNX header vectors | Cheapest early signal | LOW-MEDIUM | HW-02 | T1 | Source expected values from the spec/tests, not from another emulator's output. |
| VF-05 | Same-core determinism check (hash of frames + audio + state across two fresh runs and across state restore) | Repeatability baseline | LOW | CH-01, CH-05 | R | Establishes consistency only. Never report it as accuracy. |
| VF-06 | Independent consumer that drives the core API and renders raw frames (PPM/PNG) | Proves the API is usable without a host frontend | LOW | CH-01, CH-03 | T7 (when packaged) | A different code path from the runner. |

### Differentiators (accuracy, observability, determinism)

No existing Lynx emulator I surveyed treats these as first-class. Gearlynx is the strongest on interactive debugging; none foregrounds deterministic, evidence-tiered verification.

| ID | Feature | Value | Complexity | Depends on | Tier | Notes |
|----|---------|-------|------------|-----------|------|-------|
| DF-01 | Evidence-tiered compatibility claims bound to a named profile (profile ID + content identity + tier + test inventory) | This is the Core Value; no surveyed Lynx emulator publishes per-profile evidence | LOW-MEDIUM | HW-01, CT-05, VF-02 | all | Machine-readable claims ledger, not marketing prose. Gearlynx's "passes all lynx-tests" is a [MR] with no inventory. |
| DF-02 | Four separate but connected observation streams (CPU/device state; guest semantic progress via RAM mailbox/symbol; raw video+palette; raw audio events/samples), all stamped with one guest-tick timeline | Lets an independent consumer cross-check; mirrors PROJECT.md | MEDIUM | CH-03, CH-04, VF-02 | T3 | Semantic progress is read from guest RAM (`g_results`, status bytes), not inferred from a screen. |
| DF-03 | Tick-exact bounded advancement: "run exactly N ticks / until guest condition X / until frame boundary" with stable stop points | Determinism and debuggability; reproducible bug reports | MEDIUM | HW-03 | R | Gearlynx's `debug_step_frame` and Holani's `step_instruction`/`tick` show the idea; neither exposes a spec'd guest-time contract. |
| DF-04 | Deterministic input recording/replay as a data file (logical input events at guest ticks) | Reproducible bug reports, regression gates, TAS-style tests; ties to CH-02 | LOW (given CH-02) | CH-02 | R | Mednafen documents movies and warns that behavior-changing settings desync movies/netplay (settings table note), so a profile hash must be part of the recording header. Gearlynx MCP exposes `controller_button`/`controller_macro` (live, not file-based). |
| DF-05 | Trace/observability API: memory/register reads, breakpoints by exec/read/write, IRQ-source breakpoints, per-subsystem trace log (CPU, Suzy, Mikey, UART, cart), Mikey/Suzy/UART/cart/EEPROM/LCD status snapshots | Observation as a deliverable | MEDIUM-HIGH | HW-* | T3 | The Gearlynx MCP tool list (`get_mikey_registers`, `get_mikey_timers`, `get_suzy_registers`, `get_uart_status`, `get_cart_status`, `get_eeprom_status`, `get_lcd_status`, `set_breakpoint_on_irq`, `set_trace_log`, `set_profiler`, `get_screenshot`, `get_frame_buffer`, `get_sprite`, `rewind_seek`, `save_state`, headless, stdio/HTTP transport) is a good checklist of what to expose [SO MCP_README.md]. Build the API headless-first and let any protocol (MCP, DAP-like, CLI) wrap it later. |
| DF-06 | Machine-readable test-ROM results for the public suites via RAM mailbox plus a manifest of suite/test/expected per profile | Turns "status painted on screen" into inventories and negative controls | MEDIUM | VF-02 | T2 | See VF-02. A Lynxplosion-side manifest that maps `g_results` offsets to test IDs. |
| DF-07 | Guest debug output channel for homebrew (Gearlynx convention: Mikey unused regs $FDC0-$FDC4, string/ASCII/hex append) | Gives original fixtures a semantic-progress channel | LOW | HW-04 | T3 | [SO] Gearlynx README. This is an emulator extension, not hardware. [REC] If adopted, use only in fixtures built for this project, and document it as non-hardware (real hardware ignores unused registers; verify) so it cannot leak into evidence claims. A plain RAM mailbox needs no extension and is simpler. |
| DF-08 | Per-line raw palette/DMA timing exposure | Enables raster-effect correctness checks (video-timing guide) | MEDIUM | HW-07 | T2, T5 | Later. |
| DF-09 | Profile-parameterized Lynx I / Lynx II (CPU opcode set, reset state, ROM variant) with a second-profile qualification procedure | Honest model claims | MEDIUM | HW-01 | T2, T4, T5 | Gearlynx lets users choose model and passes `is_lynx2` into Mikey, memory, CPU and audio resets [SO `gearlynx_core.cpp`]. What Mikey/audio/memory differences it applies is [UNK]. |
| DF-10 | Independent consumer + clean-install package check (installed-host tier) | Distinct from source-tree tests | MEDIUM | CH-01 | T7 | Timing is when a package exists. |

### Anti-Features (commonly present or requested; deliberately NOT built or promised early)

| Feature | Why requested | Why problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Bundling or auto-downloading a BIOS, or embedding commercial ROMs/DB-derived titles | "Just works" | Rights policy: BIOS and commercial ROMs are rights-bearing; a hash is identity, not rights | Owner-supplied BIOS slot kept outside source control (CT-07); original/rights-cleared fixtures only |
| Silent fallback boot (Handy: bad BIOS CRC -> "internal fallback" HLE with preset registers) | Convenience | Hides which boot path produced a result; violates named-profile evidence | Explicit boot-policy enum recorded in every run manifest (see Boot section) |
| HLE boot as the accuracy path | Avoids firmware | HLE sets up post-boot state by hand (clears RAM, loads decrypted blocks to $0200, jumps). The real ROM leaves zero-page and register residue; AtariAge notes the boot code "may rely on leftover state". HLE tests an approximation | Use HLE/BS93 preset only for "no-firmware CI" slices and label results with the boot policy; do not claim boot accuracy from it |
| Copying code, game DB, or assets from Gearlynx (GPL-3.0), Holani (GPL-3.0), MAME, or reusing embedded boot blobs | Fast progress | Owner policy and license compatibility; they are cross-checks not oracles | Clean-room from Epyx spec and MIT tests; use others for behavior comparison (T4) |
| Embedding the "Free Lynx Boot ROM" blob Holani ships | Rights-clean boot in a 512-byte package | License is [UNK]. Holani credits `bjoern.spruck.net/lynx` (the page returned 404 for me; I could not find terms). An embedding project being GPL does not establish the blob's own terms | Treat as a research candidate; seek the author's terms or write an independent boot stub; decision is a rights question for the owner |
| Fixed instructions-per-frame scheduling | Simple and fast | Breaks timers, DMA contention, Suzy bus ownership and page-mode tests | HW-03 timing model |
| Libretro-style 75 FPS / "core-provided" timing as the machine's definition | Matches some cores | 75 FPS is a host convention; machine rate derives from Timer 0/2 + PBKUP | Derive refresh from guest registers (Holani does this) |
| CPU overclock, frameskip, LCD ghosting, shaders, scanlines, aspect modes, temporal blur inside the core | Present in Handy/Mednafen/Gearlynx | Presentation/perf concerns; they change observable behavior (Mednafen warns settings desync movies) | Host-side options on raw outputs; never alter machine state |
| Auto-rotation, d-pad remap and Pause+Option1/2 shortcuts in the core | Seen in all frontends | Orientation (LNX rotation byte 0/1/2) and button shortcuts are presentation/host policy; the two libretro docs even disagree on which Option does restart vs flip | Core reports header orientation + handedness; host maps buttons |
| Rewind, run-ahead, netplay, cheats (AAAA:VV), VGM recorder, achievements | Common user features | Built on CH-05 and add no accuracy evidence; early scope risk | After the state serializer and a stable timeline exist |
| Two-instance ComLynx (shared-memory/netplay linking of emulator processes) and multi-player screen layouts | Gearlynx supports up to 8 local instances | Needs a determinism design across instances; no hardware evidence without two consoles (`uart4/`) | Single-console UART loopback (HW-12) first; multi-instance is a later milestone |
| GameDrive / ElCheapoSD cartridge APIs, tape reader, LNX2 header extensions, Turbo ComLynx | Gearlynx features | Not Epyx-era hardware (or community extensions); LNX2 is a Gearlynx-ecosystem format, not a primary source | Out of first milestone; revisit with their own evidence plan |
| OCR/screenshot comparison of the on-screen test text as the primary oracle | Tests "only" paint text | Fragile, entangles presentation with logic | RAM mailbox (DF-06); frame hash only as a separate raw-video check |
| Claiming "passes all lynx-tests" or compatibility with the "entire commercial catalog" | Gearlynx makes both claims [MR] | No inventory, profile or evidence in the claim | Report counts per profile per suite (DF-01) |
| Physical-hardware qualification steps in CI (refresh calibration, two-console `uart4`) | Strongest evidence | Needs hardware and owner authority | Explicit T5 backlog items |

---

## Boot behavior: what each existing emulator actually does

Source observations unless marked. "Required" below means what the documentation says. Where code and docs disagree, both are listed.

| Emulator | Boot approach | Evidence |
|----------|---------------|----------|
| Handy (K. Wilkins lineage; libretro-handy used as source) | `CRom` loads the 512-byte image and checks CRC32 `0x0D973C9D`. On missing/invalid ROM it sets `mValid = FALSE` and falls back to: preset homebrew state, then HLE intercepts at $FE00 (select cart block via shifter), $FE19, $FE4A and $FF80: clear all 64 KB RAM, load address $0200 into $05/$06, read the encrypted loader block(s) from the cart (51 bytes read per block, `0x100 - firstByte` blocks), run `lynx_decrypt`, write 50 bytes/block to RAM and jump to $0200. Homebrew `.o` skips the ROM entirely. The libretro docs still list `lynxboot.img` as "Required", while the code runs without it. The Handy distribution included a fake boot ROM. | libretro-handy `lynx/rom.cpp`, `lynx/system.cpp` (read via GitHub API); docs.libretro.com/library/handy |
| Mednafen (Handy fork) | Documentation: "requires the 512-byte Lynx boot ROM image, named lynxboot.img" in the base directory. Palette file `lynx.pal` (12-bit BRG, 4096 RGB). Input rotation and lowpass options. | mednafen.github.io/documentation/lynx.html (v1.32.1 doc, 2024-03-19) |
| Beetle/Mednafen Lynx libretro | `lynxboot.img` "Required"; `.lnx`/`.o`; 75 FPS, 44100 Hz; core options Auto-rotate screen, color format, Force 60Hz | docs.libretro.com/library/beetle_lynx |
| Gearlynx | README: "requires a BIOS to work"; any 512-byte BIOS can be loaded, original MD5 recommended. Source has `no_bios.h`/`no_power.h` (UI images, not a boot path) and a `PrepareForHomebrew()` that clears RAM, sets registers and boot address for non-cart media. Whether homebrew runs without any BIOS is [UNK]. | README, `gearlynx_core.cpp`, `media.cpp` |
| Holani (Rust, GPL-3.0) | Embeds a 512-byte "Free Lynx Boot Rom" (const `FREE_BOOTROM` in `src/rom.rs`, comment "Courtesy of bjoern.spruck.net/lynx") and a BLL uLoader; executes the ROM on the emulated CPU (real cycle-stepped boot, not HLE), reads vectors from ROM offset $1FA and MAPCTL from $1F9. `load_rom_from_slice` replaces it with a user image. The libretro wrapper docs still say `lynxboot.img` "Required". Free ROM license [UNK]. | LLeny/holani `src/rom.rs`, `src/lynx.rs`, README; docs.libretro.com/library/holani |
| MAME (`lynx.cpp`, GPL-2.0+) | `ROM_REGION(0x200)` with two selectable dumps (`lynx.bin` CRC e1ffecb6, `lynxa.bin` CRC 0d973c9d), both marked `BAD_DUMP` ("saved from a running machine ... rom byte at 0xfff9 is not readable"); Lynx II ROM is `NO_DUMP` and the driver block is `#if 0`. G65SC02 at 16 MHz/4 clock; `lnx`/`lyx` cart slot; `o` quickload with BS93 header patches vector and PC (no ROM needed). TODOs: "ComLynx emulation is missing/imperfect", "Verify timings from real hardware", "EEPROM Support in some homebrew cartridges?", "Lynx II support, needs internal ROM dump". | raw.githubusercontent.com/mamedev/mame/master/src/mame/atari/lynx.cpp |
| cc65 / lynx-tests (toolchain) | `cl65 -t lynx` yields a `.lnx` whose first loader block is a pre-encrypted signed ("so that the Lynx will accept it") bootloader that sets MAPCTL, clears palette and loads the rest. It is minimal and "does not check the cart or show a title screen". `lynx-bll.cfg` yields BLL-compatible (homebrew `.o`) output. | cc65.github.io/doc/lynx.html; `libsrc/lynx/bootldr.s`, `cfg/lynx.cfg` |

Implications for Lynxplosion (all [REC] or [INF]):

1. A default cc65 `.lnx` cannot be started by an emulator that has neither the real boot ROM nor a decrypting HLE, because the first block is encrypted. That is why the seed's "do not assume a generated `.lnx` boots without settling firmware" is right.
2. The only boot path that is simultaneously rights-clean, trivially implemented and CI-friendly is **homebrew direct-load (BS93 `.o`)** with a documented preset state. Whether lynx-tests can be re-linked with `lynx-bll.cfg` is [UNK] and is a half-day spike (their Makefiles use `lynxcc65.mak`).
3. The three candidates for `.lnx` boot are (a) owner-supplied real image kept outside the repo (works for owner-local runs, not shared CI), (b) HLE with loader decryption (needs the public decrypt algorithm; precedent: Handy `lynxdec.cpp`), (c) a free boot ROM executed on the emulated CPU (best fidelity for the boot path; licensing [UNK]). Treat the boot policy as a first-class, recorded profile attribute, e.g. `boot=homebrew-preset | hle | owner-rom(hash) | free-rom(hash)`.

---

## Competitor / reference feature analysis

| Feature | Gearlynx 1.2.38 (GPL-3.0, active 2026-11) | Handy lineage / Mednafen / Beetle | Holani (GPL-3.0, Rust) | MAME | Our approach |
|---------|------------------------------------------|----------------------------------|------------------------|------|--------------|
| Models | Lynx I / II selectable, auto from DB [MR] | single model; no selector in docs | not documented | Lynx I only; II commented out | named profile, Lynx II later, CPU profile as data |
| CPU | per-model reset parameter (`m6502.Reset(is_lynx2)`) | 65C02 class | 65C02S cycle-stepped (chips-derived) | G65SC02 (no bbr/bbs) | profile table + `cpu/` suite |
| Boot | real BIOS required [MR] | Mednafen/Beetle: real image; Handy: fallback HLE | embedded free ROM, executed | BAD_DUMP images required | explicit boot policy per run |
| Formats | lnx, lyx, o, bin, zip, LNX2, BS93 | lnx, o | via `Cartridge::from_slice`, `lnx_header.rs`, `no_intro.rs` | lnx, lyx, o (quickload) | lnx + BS93 first; LNX2/zip later |
| EEPROM | 93C46..86, 8/16-bit, forced or auto | 93Cxx via header | `ee93cxx.rs` | not implemented (TODO) | later slice; contract first |
| Banking | BANK1 + AUDIN, GameDrive, ElCheapoSD | header-driven | `cartridge_generic.rs` | not documented | standard Epyx banking only |
| Audio | accurate, LPF 3.5 kHz, per-channel volume, VGM | 22.05/44.1 kHz | `audio_sample() -> (i16,i16)` per tick | mono 16 MHz sound stream | events + samples with guest ticks |
| Save states / rewind | yes + preview, rewind, run-ahead | libretro states/rewind (Handy core: states + rewind; no saves) | `serialize_size` / postcard | `MACHINE_SUPPORTS_SAVE` | serializer as determinism tool; UX later |
| Debugger | full ImGui debugger; symbol formats (cc65, lyxass, mads, llvm-nm, llvm-mos ELF); sprite bbox overlay; trace logger; profiler | none standard | separate `holani-debug` | MAME debugger | API-first observation; UI later |
| Remote/automation | MCP (stdio/HTTP, router mode, 7777 loopback + bearer token off-loopback), debug-monitor TCP :6502, VS Code DAP extension, Agent Skills | none | `holani-min` CLI | MAME Lua/CLI | headless runner + independent consumer; protocol wrapper optional |
| Headless | `--headless` (needs MCP/monitor/ComLynx) | none | library + `holani-min` | `-video none` | library-first, pure |
| Input record/replay | MCP `controller_macro` (live) | Mednafen movies (settings-desync warning) | n/a | MAME `-record` | recorded data file with profile hash |
| ComLynx | up to 8 local instances (shared memory) | netplay is state-based, not link cable | shared-memory/external cable features | "missing/imperfect" | single-console first |
| Platforms | desktop + libretro everywhere | libretro | library + libretro + gtk | MAME | host-agnostic core |
| Test evidence | claims it passes all lynx-tests [MR] | none known | none known | none | inventory + negative controls |

---

## Feature Dependencies

```
HW-01 profile ─┬─> HW-02 CPU ──> HW-03 bus/timing ──┬─> HW-05 timers ──┬─> HW-07 display/palette ──> CH-03 raw video
               │                                    │                  ├─> HW-11 audio ──────────────> CH-04 raw audio
               │                                    │                  └─> HW-12 UART
               │                                    ├─> HW-09 Suzy sprites ──> HW-10 math
               │                                    └─> HW-06 sleep/IRQ (needs HW-05 + HW-09)
HW-04 memory map ─> HW-08 input regs ─> CH-02 logical input ─> DF-04 record/replay
HW-04 ─> CT-04 cart shifter/banking ─> CT-06 EEPROM
CT-01 .lnx loader ─> CT-05 identity ─> persistence contract
CT-03 BS93 loader ─> boot policy "homebrew-preset" (no firmware)
CT-07 BIOS slot ─> owner-supplied boot path (outside CI)
HW-13 reset state ─> CH-01 headless runner ─> VF-05 determinism, VF-06 consumer
VF-01 fixtures ─> VF-02 mailbox runner ─> VF-03 negative controls ─> DF-01 claims ledger
CH-05 state serialize ─> VF-05, then rewind/run-ahead (deferred)
```

### Dependency Notes

- **Timing (HW-03) is the root of the device graph.** Timers, DMA, Suzy completion, sleep and UART timing all reach back to it; defer device accuracy claims until the tick model is chosen. A tick-level or exact-catch-up scheduler is still [UNK] in cost; the page-mode and `timers2/` suites make wrong choices visible.
- **HW-06 (sleep/IRQ) needs both Mikey timers and Suzy completion** so `sdoneack/` (10 tests) sits after sprites, not at first light.
- **Display (HW-07) does not require Suzy.** A program can write pen data straight into the buffer at DISPADR, which is what enables the early visible slice below.
- **TGI text in lynx-tests/cc65 draws through Suzy** (cc65 TGI is a sprite-based driver [INF, from the cc65 lynx `tgi` driver existing alongside `cc65` docs; verify]). Painted PASS/FAIL therefore needs HW-09, but the mailbox read does not, because `run_tests()` runs before `init()` in `cpu/main.c` [SO].
- **CT-06 (EEPROM) needs CT-04** (the EEPROM is selected through cart I/O) and the persistence boundary from CT-05.
- **DF-04 depends on CH-02**, and recordings must embed the profile ID and content identity.
- **Conflicts:** HLE boot conflicts with any claim about boot-state accuracy. Presentation options (overclock, frameskip, ghosting) conflict with determinism if applied inside the core.

---

## Smallest end-to-end slice that is visible and playable/homebrew

This is a delivery sequence, not a ceiling. Each slice is independently provable.

### Slice 0 (days): vectors only, no devices
HW-01 + HW-02 CPU on a flat 64 KB RAM bus with per-instruction vectors (VF-04), LNX header vectors (CT-01), determinism harness shell. Proves: T1. No visible output yet; keep it to a few days so the visible slice starts early.

### Slice 1 (first light): original homebrew, direct-load, CPU-written framebuffer
**Pieces:** CT-03 BS93 loader + documented preset state (HW-13), HW-02/03 CPU and a first-cut tick scheduler, HW-04 memory map, HW-05 timers 0 and 2 (line/frame) with IRQ, HW-07 display DMA + palette, HW-08 joystick/switch registers, CH-01/02/03 runner + logical input + raw frame, VF-06 consumer writing PPM/PNG, DF-04 recorded input.

**Fixture (original, T3):** a small assembly program (assemble with a pinned ca65/ld65 or any pinned 65C02 assembler; source + recipe + checksum manifest) that:
1. sets the 16-entry palette,
2. fills the framebuffer (a gradient or test card),
3. moves a block on screen according to JOYSTICK d-pad and changes palette on A/B,
4. writes a status byte (e.g. "palette set", "frame N") to a fixed RAM mailbox.

**Why this is the smallest:** it needs no Suzy, no audio, no cart, no BIOS, no EEPROM, no encrypted loader, and the picture is observable. It is a genuinely interactive homebrew scenario: replaying the same recorded input produces the same raw frame hash (R), and an independent consumer decodes the raw frame (T7-lite). It does not prove hardware accuracy.

### Slice 2: Suzy sprites + cc65 toolchain
HW-09 (literal normal sprites first) + VF-01 pinned cc65. Build a small cc65 program (own code, sprite via TGI or direct SCBs) as BS93 via `lynx-bll.cfg` [UNK: confirm]. Visible sprite demo; run `lynx-tests/sprites1`. Adds T2.

### Slice 3: lynx-tests ladder via mailbox
In order of cheapness: `cpu/` (CPU only, 8 tests, includes Lynx I/II discriminator), `memio/` (3), `timers/` (7), `page-mode/` (6), `math/` (8), `timers2/` (10), `audio/` + `audio2/`, `sdoneack/`, `sprites1-5/`, `uart/`, `uart2/`, `uart3/`. Each suite gets an exact inventory and one named-fault control. Run under both the BS93-preset boot policy and, when available, an owner-supplied boot ROM, recording which.

### Slice 4: `.lnx` cart boot path
CT-04 shifter/banking, CT-07 BIOS slot (owner-supplied, local only), and a decision between HLE-with-decrypt and a free boot ROM (rights decision). Then CT-05 identity, CT-06 EEPROM.

### Slice 5: game progress and installed host
Owner-local commercial title progress (T6, hashes only in the repo, never bytes), packaged consumer (T7), host presentation (rotation/scaling).

---

## MVP Definition

### Launch With (first credible slice = Slices 0-2, then 3 begins)

- [ ] HW-01 named profile (data-driven) — claims need a profile name
- [ ] HW-02/03 CPU + a tick model chosen from page-mode/timer/DMA evidence — required before any accuracy claim
- [ ] HW-04, HW-05 (timers 0/2 first), HW-07, HW-08, HW-13 — minimum to show a frame and read input
- [ ] CT-03 BS93 direct-load + written preset state — only boot path that needs no protected firmware
- [ ] CH-01/02/03 + VF-06 + VF-05 — headless, deterministic, independently consumed
- [ ] DF-04 recorded input data — makes the playable scenario reproducible
- [ ] VF-01/04 pinned fixtures, vectors, manifests; CH-07 one local check command
- [ ] CH-04 raw audio output contract present from the start, even if it only carries silence, so later audio does not break the API

### Add After Validation (v1.x)

- [ ] HW-09/HW-10 Suzy sprites and math — after first light proves the loop
- [ ] HW-11 audio, HW-06 sleep/IRQ coupling — after sprites (`sdoneack/`)
- [ ] VF-02/03 mailbox runner and named-fault controls for each lynx-tests suite
- [ ] CT-01/04/05 `.lnx` + shifter banking + identity; CT-07 BIOS slot; boot-policy decision
- [ ] HW-12 single-console UART
- [ ] CT-06 EEPROM + persistence contract; CH-05 serialize as determinism tool
- [ ] DF-05 trace/breakpoint/status API; DF-09 second model profile (Lynx II) with qualification

### Future Consideration (v2+)

- [ ] ComLynx multi-instance, GameDrive/ElCheapoSD, tape, LNX2, Turbo ComLynx
- [ ] Rewind, run-ahead, cheats, VGM, netplay, achievements
- [ ] Rich debugger UI, protocol wrappers (MCP/DAP)
- [ ] Physical-hardware tier (T5): refresh calibration, Lynx I/II comparison, two-console `uart4/`
- [ ] LibFuturo adapter (needs a real core + consumer; reviewed profiles exclude Lynx)

## Feature Prioritization Matrix

| Feature | User/Evidence value | Implementation cost | Priority |
|---------|--------------------|---------------------|----------|
| HW-01 named profile | HIGH | MEDIUM | P1 |
| HW-02 CPU | HIGH | MEDIUM | P1 |
| HW-03 timing model | HIGH | HIGH | P1 |
| CT-03 BS93 homebrew path | HIGH | LOW | P1 |
| HW-05 timers (0,2 first) | HIGH | MEDIUM | P1 |
| HW-07 display/palette | HIGH | MEDIUM | P1 |
| CH-01/02/03 runner, input, raw video | HIGH | MEDIUM | P1 |
| DF-04 input recording | HIGH | LOW | P1 |
| VF-01/04 fixtures + vectors | HIGH | LOW-MEDIUM | P1 |
| CH-04 raw audio contract | MEDIUM | LOW | P1 (contract), P2 (emulation) |
| HW-09 Suzy sprites | HIGH | HIGH | P2 |
| VF-02/03 mailbox runner + faults | HIGH | MEDIUM | P2 |
| HW-11 audio | HIGH | HIGH | P2 |
| HW-06 sleep/IRQ | HIGH | HIGH | P2 |
| CT-01/04/05 `.lnx` + banking + identity | HIGH | MEDIUM | P2 |
| HW-10 math | MEDIUM | MEDIUM | P2 |
| DF-05 observation API | MEDIUM | MEDIUM-HIGH | P2 |
| CT-06 EEPROM | MEDIUM | MEDIUM | P2-P3 |
| HW-12 UART single console | MEDIUM | MEDIUM | P2-P3 |
| DF-09 Lynx II profile | MEDIUM | MEDIUM | P3 |
| Rewind / run-ahead / cheats | LOW | MEDIUM | P3 |
| Multi-instance ComLynx | LOW | HIGH | P3 |

**Priority key:** P1 must have for the first credible slice; P2 add after first light; P3 future.

---

## Gaps and unknowns (do not become requirements until resolved)

1. **Baseline model/CPU choice.** [REC, MEDIUM-LOW confidence] Start with the Lynx I-style 65SC02 profile (no RMB/SMB/BBx), because cc65 targets 65SC02, MAME models it, and `cpu/` test 7 has explicit expectations for both. Counter-consideration: Lynx II is the later, more common unit, and a Lynx II boot ROM is not in MAME. This is an owner/architecture decision. Whether cc65's `65SC02` excludes Rockwell bit instructions is [INF]: confirm in cc65 docs before relying on it.
2. **Free boot ROM licensing.** [UNK] Holani's embedded 512-byte ROM credits `bjoern.spruck.net/lynx` (404 when fetched). No license text found. Do not use until terms are found or an independent stub is written.
3. **Whether lynx-tests can be linked as BS93 `.o`.** [UNK] Upstream Makefiles target `.lnx`. A spike is needed.
4. **cc65 TGI behavior / which suites need Suzy to even reach `run_tests()`.** [UNK] `cpu/main.c` calls `run_tests()` first, which is encouraging; other suites need checking.
5. **Gearlynx homebrew without BIOS.** [UNK] Source shows a homebrew-prep path; README says BIOS required. Do not cite either as established without running it (not done: no BIOS available and none to be downloaded).
6. **Inside/Outside vs A/B button mapping**, and exact LEFTHAND effect on JOYSTICK bits. [UNK] Confirm in Epyx spec appendix 3 / Handy source before freezing the logical-input contract.
7. **Per-line vs per-frame palette contract** and rectangular-pixel aspect presentation. [UNK] depends on raster-effect evidence.
8. **Lynx I vs II differences beyond CPU bit ops** (audio stereo, boot ROM contents, Mikey/Suzy revision registers both read '01' in the spec copy). [UNK]
9. **Mednafen/Beetle/Handy save-state, rewind and movie specifics.** The Lynx doc page does not mention states; movie support is only implied by the settings-desync note. [UNK] Not load-bearing for requirements.
10. **cc65 and lynx-tests license review** for any runtime library bytes linked into redistributed fixtures. [UNK]
11. **Version drift.** Gearlynx 1.2.38 (2026-11-04) and lynx-tests 1.4.9 are moving; pin before use. Gearlynx README, MCP tool list and test inventory above reflect 2026-11-09 and may change.

## Sources

Primary (read directly; accessed 2026-11-09):
- Epyx Handy specification mirror: monlynx.de/lynx/lynx2.html (hardware overview), lynx3.html (pitfalls), lynx4.html (CPU/ROM), lynx5.html (display), lynx6.html (sprites/collision), lynx7.html (audio/tape/ROM cart), lynx8.html (timers), lynx10.html (reset/savegame); index at atarilynxdev.net/doku.php/lynx:doc:start
- Handy address table and system-reset text as bundled in Gearlynx (`platforms/shared/desktop/mcp/resources/hardware/*.md`), used for register tables only
- cc65: cc65.github.io/doc/lynx.html; `src/common/target.c`, `libsrc/lynx/exehdr.s`, `libsrc/lynx/bootldr.s`, `cfg/lynx.cfg` (GitHub API)
- drhelius/lynx-tests: README, `cpu/main.c`, `shared/util.h`, `.github/workflows/build.yml`, `lynx-video-timing.md`; releases/tags (1.4.9, commit b583dc43...)
- libretro-handy (K. Wilkins' Handy): `lynx/rom.cpp`, `lynx/system.cpp`, `lynx/cart.cpp`, `lynx/cart.h`, `lynx/eeprom.cpp` (GitHub API)
- Gearlynx (GPL-3.0, 1.2.38): README, MCP_README.md, `src/gearlynx_core.cpp`, `src/media.cpp`
- Holani (GPL-3.0): README, `Cargo.toml`, `src/rom.rs`, `src/lynx.rs`, repository tree
- MAME `src/mame/atari/lynx.cpp`
- Mednafen Lynx documentation (mednafen.github.io/documentation/lynx.html)
- libretro docs: handy, beetle_lynx, gearlynx, holani pages (docs.libretro.com/library/...)

Secondary / LOW confidence (web search, unverified beyond snippet): AtariAge threads on replacement boot ROMs and Handy boot state; gametechwiki (HTTP 403 when fetched); AtariAge `.lnx` EEPROM-byte proposal (community, not a standard; the libretro-handy bit layout above is the source I rely on).
