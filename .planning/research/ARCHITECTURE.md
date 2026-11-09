# Architecture Research

**Domain:** Accurate, deterministic, observable, embeddable Atari Lynx emulator core
**Researched:** 2026-11-09 (all "accessed" dates below are 2026-11-09)
**Confidence:** MEDIUM overall. HIGH on component decomposition and on the timing hazards (several independent sources agree). MEDIUM on exact tick costs (Epyx text plus one maintainer's hardware-derived guides, no measurement of our own). LOW on boot-ROM internals and the post-boot machine state (primary text is silent; only emulator reports exist).

## How to read the claim tags

Per `AGENTS.md`, owner decisions, source observations, reports, inference, recommendations and unknowns stay distinct.

| Tag | Meaning |
|-----|---------|
| **[SRC]** | Source observation: read in a primary document (Epyx Handy spec mirror at monlynx.de, cc65 docs/source, lynx-tests README/guides/source) or in a pinned sibling record. |
| **[IMPL]** | Implementation report: what a specific emulator's source does. Evidence that a design exists, not that it is right. I read Gearlynx (GPL-3.0, commit `94068da`, 2026-11-04) and lynx-tests (MIT-marked, commit `a920f01`, 2026-09-20) directly in a scratch clone. Handy/libretro-handy (zlib) and MAME were read through a summarizing fetch. No code was or may be copied. |
| **[MAINT]** | Maintainer report: claims by the lynx-tests/Gearlynx author about real-hardware captures. Not independently reproduced. |
| **[INF]** | My inference from the above. |
| **[REC]** | Recommendation for this project. Not an owner decision. |
| **[OPEN]** | Unknown or needs an owner/phase decision. |

**Provenance caveat (important).** The Epyx pages (monlynx.de lynx2-lynx11, irq, hardware) were retrieved through a summarizing fetch tool, not verbatim, and the confidence seam classes that provider LOW on its own. I only rely on an Epyx claim here when it is (a) quoted consistently by the lynx-tests guides I read verbatim, or (b) marked LOW. Re-read the scanned PDF (`atarilynxdev.net/doku.php/lynx:doc:handy_specifications.pdf`) before any number below becomes a requirement. Appendix 7 (bus state machine) is referenced by the spec but was not in the mirror content I could read.

---

## Standard Architecture

### System overview

```
┌────────────────────────────────────────────────────────────────────────────┐
│ HOST SIDE (owns clocks, files, windows, audio devices, save policy)        │
│  headless runner │ independent consumer │ LibFuturo adapter │ libretro adapter │
└───────────────┬────────────────────────────────────────────────────────────┘
                │  C ABI: bytes in, logical input in, bounded advance, raw out
┌───────────────▼────────────────────────────────────────────────────────────┐
│ api/  facade: instance, load/unload, input, advance, outputs, inspect,     │
│       trace pull, state capture/restore, status + postconditions           │
├────────────────────────────────────────────────────────────────────────────┤
│ MACHINE CORE (no host clocks / files / floats / threads / globals)         │
│                                                                            │
│  ┌──────────┐   access(addr,kind)    ┌───────────────────────────────┐     │
│  │ cpu/     │──────────────────────▶ │ bus/  memory map + MAPCTL     │     │
│  │ 65C02    │◀── irq, stall, sleep ──│       arbiter + access cost   │     │
│  └──────────┘                        └──┬────────┬──────────┬────────┘     │
│       ▲                                 │        │          │              │
│       │ IRQ line               RAM 64K  │   ┌────▼───┐  ┌───▼────┐         │
│  ┌────┴─────────────────────┐  ROM 512B│   │ suzy/  │  │ mikey/ │         │
│  │ timebase/ scheduler      │◀─deadline┼───│ regs   │  │ timers │         │
│  │ u64 master ticks,        │  events  │   │ sprite │  │ irq    │         │
│  │ ordered event queue      │          │   │ math   │  │ audio  │         │
│  └──────────────────────────┘          │   │ input  │  │ uart   │         │
│                                        │   │ cartport│ │ display│         │
│                                        │   └───┬────┘  │ io     │         │
│                                        │       │       └───┬────┘         │
│                                        │   ┌───▼───────────▼───┐          │
│                                        └──▶│ cart/ address ctr, │          │
│                                            │ shift reg, banks,  │          │
│                                            │ EEPROM             │          │
│                                            └────────────────────┘          │
├────────────────────────────────────────────────────────────────────────────┤
│ observe/ (read-only observers)  inspect │ trace │ watch │ capture          │
│ state/   (versioned, identity-bound serialization of ALL of the above)     │
└────────────────────────────────────────────────────────────────────────────┘
        content/ (pure parsers: .lnx, direct-load, project container) sits OUTSIDE
        the machine: it turns host bytes into a normalized CartridgeImage or
        refuses. The machine never sees file paths or container headers.
```

### Component responsibilities and boundaries

| Component | Owns | Talks to (ports) | Must NOT |
|-----------|------|------------------|----------|
| **timebase/scheduler** | `u64` master-tick clock (62.5 ns tick), ordered event queue, same-tick ordering rule | every device via `schedule(tick, event_id)` / `run_until(t)` | know device internals; read host time |
| **cpu/** | A/X/Y/S/P/PC, decode, per-access sequencing, interrupt-sampling rules, **CpuProfile** (Lynx I vs II opcode/behaviour table) | bus via `access()`; IRQ/NMI level in; stall/sleep state from arbiter | decode MAPCTL, know what Suzy is, count "frames" |
| **bus/** | 64 KiB RAM, MAPCTL ($FFF9), page decode of $FC00-$FFFF, boot-ROM region, **arbiter** (video > refresh > CPU > Suzy), access-cost model incl. page-mode stream state, CPU bus-request flip-flop | cpu, suzy (as bus master), mikey (video/refresh requestor), devices as slaves | own device registers; load files |
| **mikey/timers** | 8 timers + 4 audio-channel timers as one shared-prescaler bank, link graph, DONE/borrow/reload semantics | irq, display (timer 0/2), uart (timer 4), audio | have per-timer private clocks (prescaler phase is shared) |
| **mikey/irq** | INTSET/INTRST, pending bits, IRQ level, interaction with CPUSLEEP | cpu line, bus request FF | |
| **mikey/audio** | 4 channels: LFSR, integrator, volume, MSTEREO, output mix | timers (link ring), observe/capture | resample or filter in the canonical path |
| **mikey/uart** | SERCTL/SERDAT, TX/RX shift, parity, loopback echo, IRQ level | timer 4, a logical **ComLynx wire port** | do host networking |
| **mikey/display** | line/frame generator driven by timers 0/2, DISPADR latch, video DMA bursts (bus requestor), palette (GREEN/BLUERED), LCD transfer model, REST | bus (DMA), timers, capture | rotate, scale, convert to RGB |
| **mikey/io** | IODIR/IODAT, SYSCTL1 (cart address strobe, power), AUDIN, SDONEACK, CPUSLEEP | cart (address lines), suzy (done ack), bus | |
| **suzy/regs+bus master** | register file, SUZYBUSEN, SPRGO/SPRSYS, bus-master FSM (request/grant/pause) | bus, mikey/io (SDONEACK), scheduler | assume it owns the bus |
| **suzy/sprite** | SCB walker, unpacker, pixel merge, collision buffer/depository; split into a **pure functional painter** and a **timing shell** | bus (RAM reads/writes as bus-master accesses) | be an instant "paint now" call |
| **suzy/math** | multiplier/divider/accumulate, sign latch, busy duration, known defects | scheduler (completion deadline) | |
| **suzy/cartport** | RCART0/RCART1 read/write cost and side effects | cart | |
| **suzy/input** | JOYSTICK/SWITCHES from the logical input timeline, left-hand swap | input | read physical devices |
| **cart/** | normalized image (bank0/bank1: ROM/RAM/persistent), 8-bit shift register + 11-bit counter, page-size shift, strobe/edge semantics, AUDIN/EEPROM bit, EEPROM state machine | mikey/io (strobe, data), suzy/cartport (data) | parse headers |
| **boot/** | startup strategies (see section below) | bus (ROM slot), cart, cpu | silently differ between strategies |
| **input/** | time-stamped logical button timeline | suzy/input | discover controllers |
| **observe/** | side-effect-free inspect, bounded trace ring, watch/stop rules, raw capture | read-only views of all components | alter timing or state (trace on/off must not change a single tick) |
| **state/** | versioned, identity-bound serialization incl. scheduler queue and every FSM | all | contain pointers or host-dependent layout |
| **api/** | C ABI facade, status codes, profile descriptors | everything above | leak internals, call back into host during advance |
| **content/** | `.lnx`/direct-load/project-container parsing to `CartridgeImage` + refusal reasons | none (pure) | touch machine state |

**[REC]** Components communicate through narrow **port interfaces** (e.g., `CpuBus`, `SuzyBusPort`, `CartAddressPort`, `CartDataPort`, `IrqLine`, `ComLynxWire`), not shared pointers. Gearlynx wires Mikey, Suzy, Media, Bus and Memory to each other by pointer **[IMPL]**; that is workable but makes the Mikey↔cart↔Suzy triangle (below) hard to test in isolation.

### The two awkward couplings (get these right in the boundaries)

1. **The cartridge is a shared peripheral with two controllers.** **[SRC]** Epyx puts the cart *address* lines under Mikey (SYSCTL1 B0 = cart address strobe, which also resets the counter; IODAT B1 = cart address data, 0 turns cart power on) and the cart *data* path under Suzy (RCART0/RCART1 at $FCB2/$FCB3; "Suzy holds off Mikey's DTACK, enables the ROM, waits for access time, then disables it"; the trailing edge of the chip enable advances the counter by 1; address = 8-bit shift register as upper bits + 11-bit counter as lower bits, max 19 bits). cc65's own loader confirms the pattern: it steps the cart block counter by toggling IODAT and clocking SYSCTL1 **[SRC: cc65 `libsrc/lynx/bootldr.s`, read via fetch]**. EEPROM carts bit-bang through the same lines plus AUDIN **[IMPL: Gearlynx `eeprom.cpp` takes `iodir/iodat` and the counter]**. **Consequence:** `cart/` is a separate component with an *address port* driven by Mikey and a *data port* driven by Suzy; neither chip owns it.
2. **Suzy and the CPU share one RAM and Suzy is a second bus master.** **[SRC]** "We will allow only one bus master at a time"; priority Video, Refresh, CPU, Suzy; "There is no default owner of the bus"; bus request = OR of CPU/video/refresh, bus grant comes from Suzy; Suzy can keep running internal work while Mikey holds the bus. **Consequence:** `bus/` owns an explicit arbiter state machine. Suzy and Mikey display DMA reach RAM **underneath** the MAPCTL overlays (the overlay changes what the *CPU* sees; **[SRC]** Epyx: "Neither SCBs nor sprite data may be located in Mikey ROM", which only makes sense if the sprite engine's RAM path is separate from the CPU's mapped view). `Memory` must therefore expose two views: a CPU-mapped view and a raw-RAM view for Suzy/DMA/inspect.

---

## Timing model

### Why a fixed instructions-per-frame model is unsafe (evidence list)

| Hazard | Source | Why it breaks a fixed model |
|--------|--------|------------------------------|
| CPU access costs 4, 5, 9-15 or more ticks depending on *what* is accessed and the current page-mode state | **[SRC]** Epyx table: page-mode RAM/ROM read 4, normal RAM/ROM 5, Mikey hardware 5, Mikey audio DPRAM 5-20, Suzy write 5 (blind), Suzy read 9-15; cart read 15; "CPU cycles can not be used as the timing elements in small software timers" | An "instruction" has no fixed duration |
| Page-mode eligibility is a per-fetch, state-dependent rule | **[MAINT]** lynx-tests `lynx-page-mode.md`: eligible fetch = MAPCTL B7 clear AND instruction stream already open AND `addr & $F != 0`; data accesses, stack, vectors, indirect pointer reads, taken branches to other addresses close the stream; display DMA/refresh loses the DRAM row. Formula: `ticks = 5*base_cycles - eligible_fetches` + stalls | Same code runs at different speeds depending on alignment and DMA |
| Display DMA steals the bus from the CPU | **[SRC]** video is highest priority and "pauses the CPU clock". **[MAINT]** 10 bursts × 8 bytes per visible line, 192 ticks apart, Gearlynx models 28 ticks per burst **[IMPL]** | CPU throughput depends on line phase |
| DRAM refresh | **[IMPL]** Gearlynx injects 4 ticks every 256 ticks and coalesces with LCD bursts; **[SRC]** refresh is a bus requestor | Constant-rate background stall (exact period/cost: **[OPEN]**) |
| Suzy owns the bus while drawing; CPU sleeps or waits up to ~40 ticks for a grant | **[SRC]** Suzy bus request-to-grant max 2.5 µs (= 40 ticks); CPU stays asleep only while Suzy uses the bus | Sprite cost is data dependent and bus contended |
| Timers share prescaler phase | **[MAINT]** timers2 suite: enabling order on a shared 4 µs source decides who interrupts first; first interval is N..N+1 source periods | Timer events fall at phase-dependent ticks |
| Interrupt entry is not "immediately" | **[SRC]** CPU finishes the current cycle; if Suzy owns the bus Mikey must request it first (up to 40 ticks). **[MAINT]** cpu suite: 65C02 interrupt entry delayed one instruction after CLI; reserved 1-byte NOPs do not acknowledge a pending IRQ | Latency is a function of instruction mix and bus owner |
| Frame length is guest-programmable | **[SRC]** line = Timer 0 period, frame = Timer 2 × lines; 60/50/75 Hz recipes in Epyx; **[MAINT]** 3 VBlank lines minimum, visible = min(102, T2backup-2) | No constant frame rate to build on |

**[INF]** Even one real 60 Hz frame is ~267,120 ticks (105 lines × 2544 ticks). A frame-quantised model cannot place a mid-line palette write, a Timer-0 raster interrupt, or a CPU read of a timer register at the right tick; lynx-tests' raster-race description (write framebuffer bytes before DMA fetches the 8-byte group) is exactly this case **[MAINT]**.

### Guest-time work units, compared

| Candidate unit | Pros | Cons | Verdict |
|----------------|------|------|---------|
| **Master tick** (16 MHz crystal, 62.5 ns; Epyx: "the crystal is the only source of timing information") | Every other clock (CPU access, 1/2/4…64 µs timer prescalers, line, DMA group, UART bit) is an integer multiple; exact, lossless, no unit conversion; maps to Gearlynx's own "ticks" **[IMPL]** and to lynx-tests guides | `u64` bookkeeping; ~16 M events/s upper bound if naively per-tick | **[REC] canonical guest time** |
| CPU cycle | Familiar | Not uniform (4/5/9-15/15+ ticks); MAME treats the CPU as 16 MHz / 4 = ~4 MHz **[IMPL]**, Epyx RAM rates 8 MHz page / 4 MHz normal **[SRC]**; three "CPU clocks" for one machine is the hazard | Reject as a unit; keep as a *trace field* only |
| Instruction | Easy stop point | Not time; mid-instruction DMA/IRQ effects hidden | Use as a **stop granularity**, not a unit |
| Frame | Natural for hosts | Variable, programmable, may never occur | Use as a **stop condition** (VSYNC event), never the budget unit |
| Scheduler event | Efficient skipping while CPU sleeps | Not a unit users can reason about | Internal mechanism only |

**[REC]** The native API's budget is `max_ticks` (u64) plus an optional stop-on-event mask (frame complete, IRQ taken, watch hit, CPU halted). Frames, instructions and events are *reports*, not budgets. A LibFuturo/libretro adapter narrows this to "one frame" (see Host boundary).

### Synchronisation granularity: what the leading emulators do

| Emulator | Model (as read) | Notes |
|----------|-----------------|-------|
| **Handy / libretro-handy** (zlib) **[IMPL]** | `Update()`: if `system_cycle >= next_timer_event` run `Mikey::Update()`; run **one CPU instruction**; if CPU asleep jump time to the next event. Mikey timers predict their next event ("overload the end-of-time value"). Suzy paints the **entire sprite list synchronously** when CPUSLEEP is written, accumulates a `cycles_used` figure and wakes the CPU after that delay; no bus arbitration, math has no modelled delay. Display renders per line from RAM at timer events; DMA is charged as a lump `(80+100)*cost` per line | Event-skipping design; accuracy limited by instruction-granular sync and "paint instantly, charge later" Suzy |
| **Mednafen/Beetle Lynx** | Reported lineage: fork of Handy **[MAINT: libretro docs, accessed 2026-11-09]**; requires `lynxboot.img` | Not read at source level |
| **Gearlynx** (GPL-3.0) **[IMPL]** | `RunCycle()`: run **one CPU instruction** → `cpu_ticks`; add `bus` injected ticks (DMA/refresh/waits); `Suzy::ApplyBusStall()` charges stolen display-DMA ticks against Suzy's phase FSM; then `Mikey::Clock(delta)`, `Suzy::Clock(delta)` (order depends on whether CPU is halted), `Audio::Clock(delta)`. `RunToVBlank()` loops until Mikey signals vblank with a 450,000-tick failsafe. Suzy is a **phase FSM** (SCB fetch, row paint, legacy delay…) clocked by elapsed ticks. Two CPU tables (Lynx I vs II). Fixed 44.1 kHz audio via an exact integer phase accumulator (`phase += ticks*44100; emit when ≥ 16e6`) | Instruction-granular with explicit stall accumulators. Maintainer claims it passes all lynx-tests **[MAINT]** |
| **MAME `lynx.cpp`** **[IMPL]** | G65SC02 at 16 MHz/4; RAM backing with Suzy/Mikey/ROM/vector views and custom $FFF9 handlers; timers via the Mikey sound device's scheduler callbacks; **no blitter timing**, vblank "not accurate", no cart bank counter in this file, ComLynx/EEPROM gaps | Useful as a map-structure cross-check; explicitly not timing-accurate |

**[INF]** All three emulators synchronise devices **at instruction boundaries**. That is a sound first approximation and the reason they pass many ROM tests. Where it can lose accuracy: a CPU read of a Mikey timer register or palette write *mid-instruction*, DMA that lands between an opcode fetch and its data cycle, and an IRQ that becomes pending between two bus cycles of one instruction. lynx-tests contains suites targeting these (`timers2` TIMER READS/WRITES/MIKEY ACCESS measure 64 reads/writes per register; `cpu` TEST 8 UNDC NOP IRQ) **[MAINT]**.

### Recommended model: tick-stamped bus accesses with lazy device catch-up

**[REC]** Make the **bus access** the unit of CPU progress and the **event queue** the unit of device progress:

```text
// Sketch of shape only, not a final API.
cpu.step():
    for each micro-step of the instruction:
        access = {addr, kind, wdata?}    // kind: OPCODE | OPERAND | DATA_R | DATA_W | STACK | VECTOR | INTERNAL
        t_start  = arbiter.grant_cpu(now)            // may stall: video/refresh/Suzy owns the bus
        scheduler.run_until(t_start)                 // devices catch up, events ordered by (tick, priority, seq)
        cost     = cost_model.ticks(access, t_start) // 4/5, Suzy 9-15, cart 15, page-mode stream state
        value    = bus.perform(access, t_start)      // MAPCTL-selected target sees devices AT t_start
        now      = t_start + cost
    sample_interrupts(now)                           // per CpuProfile rules (CLI delay, reserved NOPs)
```

Properties this buys:

- A timer-register read observes the timer **at its access tick**, not "end of previous instruction".
- Display DMA and refresh are **events** that request the bus; the arbiter inserts them *between CPU accesses*, closes the page-mode stream, and the CPU's next access is automatically 5 ticks.
- When the CPU is asleep (CPUSLEEP) the loop jumps time to the next event, as Handy does, so a sleeping workload costs almost nothing.
- One ordering rule for same-tick events (fixed priority table, then insertion sequence) is the entire determinism contract for time.
- Instruction-granular stop points remain available (finish the current instruction, then check the budget), so save-state and overshoot reporting stay simple. Overshoot is bounded by one instruction plus one grant latency (tens of ticks).

**Cost of the recommendation [INF]:** slower than pure instruction stepping and more code in the CPU core. The project's constraint is "correctness baseline before performance"; a catch-up scheduler is also the *easiest* design to optimise later (batch when no deadline is near) without changing observable results, which is the property a deterministic claim needs.

**Fallback [REC]:** if the first CPU slice must ship quickly, step by instruction but **stamp each access's start tick inside the instruction** (cost computed per access, devices caught up lazily at each access). That is the same architecture with coarser CPU micro-stepping and can be refined per opcode class.

### Time-related device facts to encode (with provenance)

| Item | Value / rule | Tag |
|------|---------------|-----|
| Tick | 62.5 ns (16 MHz) | [SRC] |
| Timer period | `T = (backup+1) * 2^selector µs`, selectors 0-6 = 1…64 µs, 7 = linked; first interval after enable is N..N+1 source periods; count 0 is valid for one full period; longest direct 16.384 ms | [SRC]+[MAINT] |
| Timer link graph | T0→T2→T4; T1→T3→T5→T7→A0→A1→A2→A3→T1; T6 isolated | [SRC]+[MAINT] |
| Timer defects to reproduce | Non-reload timer does not count while DONE is set; RESET_DONE is a level not a pulse; CTLB read-modify-write hazards; interrupt asserts when already at 0 and a borrow is attempted | [SRC]+[MAINT] |
| Timer roles | T0 line length, T2 line count (frame), T4 UART baud (`baud = f_src / ((backup+1)*8)`) | [SRC]+[MAINT] |
| Standard 60 Hz | T0 backup 158 (159 µs = 2544 ticks), T2 backup 104 (105 lines), PBKUP 41, DISPCTL x'0D' → 267,120 ticks/frame ≈ 59.90 Hz | [SRC]+[MAINT] |
| Line structure | ~624 ticks non-active then 1920-tick LCD transfer in 10 groups of 192 ticks × 16 pens; each group's 8-byte DMA fetch ~8-12 ticks after the group starts; visible line 0 gets a one-off fetch at the Timer-0 borrow | [MAINT] |
| Frame structure | 102 visible + 3 VBlank lines; DISPADR latched at start of the 3rd VBlank line; low 2 bits of DISPADR ignored | [SRC]+[MAINT] |
| Bus priority | Video > Refresh > CPU > Suzy; one master at a time; no default owner | [SRC] |
| CPU bus request FF | Reset only by CPU (CPUSLEEP write); set by reset, unmasked interrupt, or Suzy's grant returning | [SRC] |
| CPUSLEEP semantics | Does not stay asleep unless Suzy is using the bus ("sleep is broken in Mikey"); a pending enabled interrupt prevents/ends sleep even with I set; Suzy-done wake is sticky until SDONEACK is written, and the written value is ignored; an IRQ wake must not create a Suzy-done condition; with SUZYBUSEN clear, SPRGO still makes Suzy busy but CPU cannot sleep | [SRC]+[MAINT] (sdoneack suite, 10 tests) |
| Cart read / write | Read CPU cycle = 15 ticks (cart access time 392 ns); write is blind (562.5 ns strobe) and CPU must not touch Suzy for 12 ticks after | [SRC] |
| Suzy register access | Reads 9-15 ticks, writes 5 (blind, no DTACK); consecutive Suzy accesses can "break Suzy" | [SRC] |
| MAPCTL | $FFF9 reset $00; B0 Suzy, B1 Mikey, B2 ROM ($FE00-$FFF7), B3 vectors ($FFFA-$FFFF) overlay RAM when set; B7 = sequential (page-mode) disable. Lynx I reads back what was written; Lynx II reads `(v | $70) ^ $80`. $FFF8 is RAM. Both chips see writes to $FFF9, only Mikey answers reads | [SRC]+[MAINT] |

---

## Startup: what the boot ROM does and what a rights-clean path needs

### What is established

| Fact | Tag |
|------|-----|
| 512-byte ROM embedded in Mikey; its job is to read initial data from the data input system and execute it | [SRC] Epyx lynx4 |
| At reset the ROM overlay is enabled; the CPU fetches the reset vector from ROM at $FFFC/$FFFD; Suzy is disabled until the CPU enables it; video bus request disabled, CPU requesting the bus | [SRC] Epyx lynx10 |
| Epyx init list: disable interrupts, clear decimal, read SUZYHREV, if 0 jump to test code, read 8 RAM locations (each must raise a new RAS); the list ends with an unfinished "(more ??)" and defers initial register states to Appendix 2 | [SRC] (LOW: summarizing fetch) |
| Mikey ROM known variances: sets external-power-detect to output, REST pin initialised wrongly | [SRC] Epyx lynx10 |
| The Epyx text does **not** describe the cart header, loader decryption or exactly what is copied where | [SRC] (silent) |
| Reported: the loader block is decrypted with a public-key scheme, frame = 51 encrypted bytes → 50 plaintext, first byte = 256 − block-count (up to 5 blocks), plaintext placed at $0200 and executed | [MAINT] AtariAge threads via search snippets; [IMPL] Gearlynx `DecryptEpyxLoader`, libretro-handy `HLE_BIOS_FE4A` |
| cc65's `cl65 -t lynx` cart image uses a **signed (encrypted) stage-1**: `bootldr.s` header says it "creates a signed binary so that the Lynx will accept it"; a 52-byte encrypted table; commented plaintext shows stage-1 loads 151 bytes ($97) from the cart to $FB68 and jumps; stage-2 reads an 8-byte directory entry via RCART0, positions the cart counter, copies the program to its destination and jumps | [SRC] cc65 source via fetch; cc65 docs: "very minimal bootloader … does not check the cart or show a title screen" |
| Therefore the **lynx-tests `.lnx` files built by `cl65 -t lynx` carry the signed stage-1** and an unmodified BIOS-less emulator must either decrypt it, substitute it, or skip it | [INF] from [SRC]+[MAINT] (`lynx-tests/shared/lynxcc65.mak` builds with `cl65 -Ln x.sym -t lynx`) |

### How existing emulators run without the BIOS image

| Emulator | BIOS-less route | Tag |
|----------|-----------------|-----|
| libretro-handy (zlib) | If the ROM image is invalid, `Reset()` writes small stubs at $FE00/$FE19/$FE4A/$FF80 that call **HLE routines**: clear RAM, set load address $0200, cart shifter 0, read loader blocks through the cart peek, run an external `lynx_decrypt()`, copy to RAM, set PC=$0200. Homebrew (`BS93` marker) skips all of it via `PresetForHomebrew`. The libretro docs still list `lynxboot.img` as required | [IMPL] (via fetch) |
| Gearlynx | Normal `.lnx` **requires** a 512-byte BIOS (identity checked by CRC). Separate no-BIOS routes: BS93 homebrew and "Epyx headerless" carts, both through `PrepareForHomebrew()`: zero RAM, A=X=Y=0, S=$FF, P=$34, PC=boot address, program Timer 0 (backup $9E, ctl $18), Timer 2 (backup $68, ctl $1F), DISPCTL=$09, PBKUP=$29, zero palette, MAPCTL=$0C. Epyx headerless carts are decrypted with an embedded public-modulus constant | [IMPL] |
| MAME | BIOS dumps (two, marked BAD_DUMP, differing in the unreadable $FFF9 byte); Lynx II BIOS NO_DUMP; `.o` quickload path | [IMPL] |
| BS93 homebrew container | `0x80 0x08`, boot address (BE16), size (BE16), ASCII `BS93`, then payload; payload loaded at the boot address | [IMPL] (Gearlynx `GatherBS93Header`) |

**Observation that matters:** Gearlynx's preset sets DISPCTL=$09 while the Epyx recommended value is x'0D' (color + 4-bit + DMA enable). Two sources disagree about even one register of the "post-boot" state. **The post-boot state is evidence-poor, and a BIOS-less start is a different initial condition from hardware** (timer phase, DRAM page state, RAM contents, exact register values). Any result obtained through HLE is tagged `startup=<strategy>` and is not evidence about the real boot path.

### What a rights-clean, independently authored startup path needs

Constraints: no BIOS bytes, no commercial ROMs, no copying GPL code, unattended CI (`PROJECT.md`, `AGENTS.md`).

| Tier | Strategy | What it needs | What it proves | Rights/risk | Tag |
|------|----------|---------------|----------------|-------------|-----|
| **A** | **Direct-load HLE preset**: container with load address + size (BS93-style or a project-defined equivalent); loader copies payload to RAM, sets CPU state and a **documented post-boot preset** (each register value cites the Epyx page or lynx-tests expectation; anything uncited is left at power-on reset and the fixture must set it itself) | project container spec, preset table with per-value provenance, fixtures that initialise every device they use | CPU/MAPCTL/bus/devices with guest code; **not** cart hardware, **not** boot ROM | Cleanest. BS93 layout is a public community format; own writer | [REC] first |
| **B** | **Project-authored boot shim** in the 512-byte ROM region: original 65C02 code written from the spec's description ("read initial data … then execute"), reads the first block through the real cart port (shift register/counter, RCART0, strobe) into RAM and jumps; container has an *unsigned* first block | shim source + assembler pin, project container spec, cart port + MAPCTL ROM overlay + reset-vector fetch implemented | cart address counter, bank/page logic, vector overlay, early timing, with real CPU bus traffic | Original work; **must be labelled "not the Lynx boot ROM"** and cannot run commercial signed carts | [REC] second |
| **C** | **Owner-supplied BIOS** (firmware content slot; kept outside source control; hash used only as identity/diagnostic) | firmware slot in the content API, refusal postconditions, opt-in local runs, never in shared CI | real boot path, decryption, post-boot state, cc65-built signed carts incl. lynx-tests as built | Requires owner action; rights evaluation is separate from hash | [REC] separate evaluated route |
| **D** | HLE **with decryption** (Handy-style) | public-key constants and algorithm; legal/provenance review of where the constants come from (they appear in the BIOS and in emulators) | runs signed carts BIOS-less | **Owner/legal decision.** Do not adopt by default; do not copy GPL Gearlynx code | [OPEN] |

**[REC] cc65-built fixtures without BIOS.** Two ways to run lynx-tests-like suites in CI without tier C/D, both needing a *spike* before commitment:
1. Build fixtures with **ca65/ld65 only** (no cc65 runtime/`lynx.lib`, no signed loader), then wrap with the project's own packer into tier A/B containers. For the first diagnostics, hand-assembled bytes with a reviewed listing in the fixture manifest avoid even a toolchain (cc65 is not installed on the dev host per `PROJECT.md`).
2. For the unmodified lynx-tests binaries: **[INF, unverified]** recognise cc65's stage-1 by exact bytes and substitute its documented plaintext semantics (zlib-licensed cc65 source comments), then let stage-2 run for real through the cart port. Needs checking: whether the commented plaintext is complete and stable across cc65 revisions, and whether the substitution changes timing relative to real decrypt (it will; tag results `startup=cc65-stage1-substitute`).

`lynx-tests` already emits a `-Ln <name>.sym` label file next to each `.lnx`, which is what lets the runner find result bytes by symbol (see Observability) **[SRC]**.

**Baseline model/CPU profile (open per `PROJECT.md`) [REC]:** model the CPU as a data-driven `CpuProfile`. Lynx I vs II differ in at least: RMB/SMB/BBR/BBS (one-byte NOP-like on I, real 5-cycle operations on II per Gearlynx's two opcode tables **[IMPL]** and the lynx-tests `cpu` TEST 7 expectations `{I: $FF,$00,$02,$02 | II: $FE,$01,$01,$01}` **[SRC]**), and MAPCTL read-back. Recommend **Lynx II as the first named baseline** because the public timing captures cited by the page-mode and video-timing guides are Lynx II **[MAINT]**, and Lynx I becomes a data diff, qualified later. This is a recommendation for the owner/roadmap, not a decision; MAME's driver says G65SC02 with no BBR/BBS **[IMPL]**, the Epyx spec says 65C02, so the discrepancy is real and must be resolved by tests, not labels.

---

## Recommended project structure (language-neutral)

```
core/
  timebase/    # tick type, event queue, ordering rule, run_until
  cpu/         # 65C02-family engine, CpuProfile tables (lynx1, lynx2), interrupt sampling rules
  bus/         # RAM, MAPCTL, CPU-mapped view and raw view, boot-ROM slot, arbiter, access-cost model
  mikey/
    timers/    # shared-prescaler bank, link graph, DONE/borrow/reload, audio-timer ring
    irq/       # INTSET/INTRST, IRQ line, sleep interaction
    audio/     # 4 channels (LFSR, integrate, volume), MSTEREO, mix
    uart/      # SERCTL/SERDAT, loopback, ComLynx wire port
    display/   # line/frame generator, DISPADR latch, DMA bursts, palette, LCD transfer
    io/        # IODIR/IODAT, SYSCTL1, AUDIN, SDONEACK/CPUSLEEP
  suzy/
    regs/      # register file, SUZYBUSEN, SPRGO/SPRSYS
    sprite/    # painter (pure) + timing shell (bus-master FSM)
    math/      # multiply/divide/accumulate, defects, busy deadline
    input/     # JOYSTICK/SWITCHES
    cartport/  # RCART0/1 cost and side effects
  cart/        # normalized image, counter+shift register, banks, EEPROM
  boot/        # direct-load preset, project boot shim bytes, firmware slot
  input/       # logical input timeline
  observe/     # inspect, trace ring, watches/stops, capture
  state/       # serialization + identity
  api/         # C ABI facade, status codes, profile descriptors
content/       # pure parsers/normalizers: .lnx, BS93-style direct-load, project container
tools/         # fixture packer, symbol-file reader, manifest checker
runner/        # headless runner: run script -> artifact bundle
consumer/      # independent consumer (separate build, public ABI only)
fixtures/      # sources + manifests + build recipes (no third-party binaries)
spec/          # behavior spec: each rule carries source tag + evidence tier + test ids
```

**Rationale.** `core/` has zero dependencies on `content/`, `runner/` or `consumer/`. `runner/` and `consumer/` depend only on `api/`, which is what makes "independent consumer drives the core without host clocks, filesystems, windows or A/V devices" checkable by the build graph. `spec/` is a first-class directory because the project's core value is the claim-profile-evidence link; each rule in it names the source tag and the test that exercises it.

---

## Architectural patterns

### Pattern 1: Deadline scheduler with a total order
**What:** One `u64` clock; each device publishes its next deadline (timer borrow, DMA burst, Suzy phase end, UART bit, math done); the scheduler pops in `(tick, priority, sequence)` order. Writes to timer control re-plan that device's deadline (Handy forces a re-prediction on control writes **[IMPL]**; same idea).
**When:** Always; it is the determinism contract for time.
**Trade-offs:** Slightly more bookkeeping than per-tick loops; far cheaper while the CPU sleeps; event ordering bugs are the classic failure, so the priority table must be written down in `spec/`.

### Pattern 2: Shared-prescaler timer bank
**What:** Timers 0-7 and audio channels are modelled as one bank on a free-running master phase; per-timer state is `{backup, count, ctla, ctlb}` plus link edges. A timer's "next borrow" is computed from the *global* phase, not from when it was enabled.
**When:** Required by `timers2` (enable order, phase, prescaler change sampling, link cascade) **[MAINT]**.
**Trade-offs:** More subtle than independent counters, but independent counters fail those tests by construction.

### Pattern 3: Bus master as an explicit state machine
**What:** `Arbiter{owner ∈ {None, Video, Refresh, Cpu, Suzy}, cpu_bus_request_ff, suzy_enable_ff, pending_requests}`. CPUSLEEP, Suzy-done wake, SDONEACK and interrupts are transitions of this machine, not special cases in the CPU.
**When:** From the first slice that has Mikey timers (IRQ wake) even before Suzy exists.
**Trade-offs:** None worth avoiding; the sdoneack suite is literally a transition table **[MAINT]**.

### Pattern 4: Pure painter + timing shell for Suzy
**What:** `paint_step(scb_state, ram_view) -> {pixel writes, collision writes, ram reads performed, cost units}` is a pure function; a separate shell turns those costs into bus requests and `Suzy phase` deadlines. Gearlynx's phase FSM (SCB fetch, row paint, legacy delay, grant overlap constants) shows the shape **[IMPL]**; lynx-tests sprites1-5 provide timing probes **[MAINT]**; Epyx gives only relative costs (background ~¼ less, XOR up to ¼ more) **[SRC]**.
**When:** Pixels can be verified (sprites3: eight operation types with CRCs) before timing is refined (sprites1/2/4/5, DMA EXP). The boundary stays fixed while timing improves.
**Trade-offs:** Needs a cost-reporting painter from day one; avoids a rewrite from Handy-style instant paint.

### Pattern 5: Two memory views and side-effect-free inspection
**What:** `cpu_view(addr)` applies MAPCTL and may trigger device side effects; `raw_ram(addr)` is used by Suzy/DMA; `peek(domain, addr)` never has side effects. Gearlynx has a `debug` template flag on reads for exactly this **[IMPL]**. Some reads *do* have side effects (RCART0/1 advance the cart counter; INTRST/timer CTLB writes, SDONEACK; CPU `LDA` of a hardware register costs Suzy-specific ticks), so `peek` must be a separately implemented path, not "read with a flag" by default.

### Pattern 6: Trace as a pull-based, bounded observer
**What:** Core appends fixed-size records `{tick, source, kind, a, b, c}` to a preallocated ring if the corresponding bit in a trace mask is set; the host drains via the API; overflow increments a dropped counter. No callbacks during advance.
**When:** From slice 0; tracing off and tracing on must produce identical machine state (test: state hash equal).

### Pattern 7: Normalized content descriptor
**What:** `content/` turns `.lnx`/direct-load/project container bytes into `CartridgeImage{banks[], page_size_shift[], eeprom_kind, rotation_hint, identity}` or a refusal code; the machine loads only that descriptor. `.lnx` header fields reported by Gearlynx's struct: magic, bank0/bank1 page size, version, name(32), manufacturer(16), rotation, audin, eeprom **[IMPL]**. Missing-header fallback guesses `ceil(size/256)` pages **[IMPL]**; for this project **[REC]** refuse or require an explicit configuration instead of guessing silently.

---

## Data flow

### Per-access flow (CPU)

```
cpu ──access(addr,kind)──▶ bus.arbiter ──grant tick──▶ scheduler.run_until(t_start)
                                                          │ (devices fire events in total order:
                                                          │  timer borrow → irq pending;
                                                          │  display DMA request → bus stall;
                                                          │  suzy phase end → bus release / done)
                           bus.decode(MAPCTL) ──▶ RAM | ROM slot | suzy.regs | mikey.regs
                           cost_model.ticks(...) ──▶ now = t_start + cost
cpu ◀── value, irq/stall/sleep state
```

### Display path

```
timers 0/2 ──deadline──▶ display.line_start ──▶ IRQ pending (T0/T2) ──▶ cpu
                         display.dma_group(i) ──request──▶ arbiter ──(steals bus; closes CPU page)──▶ raw RAM read (8 bytes)
                         pen buffer ──palette regs at transfer tick──▶ 12-bit color word per pixel
frame complete = last visible pixel of line 101 transferred ──▶ capture.frame_ready
```

### Suzy path

```
cpu write SPRGO ──▶ suzy.regs ──▶ sprite shell requests bus ──▶ arbiter grants when CPU asleep/idle (Suzy lowest priority)
painter reads SCB/data (raw RAM), writes video + collision buffers + depository ──▶ costs ──▶ shell phases/deadlines
done edge ──▶ cpu_bus_request_ff set (wake) ──▶ sticky until SDONEACK
```

### Cartridge path

```
cpu ─ write SYSCTL1 strobe / IODAT data ─▶ mikey.io ─▶ cart.address_port (shift 8 bits MSB-first; strobe resets counter)
cpu ─ read RCART0 ─▶ suzy.cartport (15-tick CPU cycle; Suzy holds DTACK) ─▶ cart.data_port ─▶ byte; counter += 1
cpu ─ write RCART0 ─▶ blind write (12-tick no-Suzy-access hazard window)
EEPROM carts: cart.eeprom reads strobe/counter/IODAT bits ─▶ AUDIN bit back into mikey.io IODAT B4
```

### Observation flow (read-only fan-out)

```
machine state ──inspect/peek──▶ api ──▶ runner ──▶ artifact bundle
machine events ──trace ring──▶ api ──▶ runner ──▶ trace file
RAM watch hits / stop reasons ──▶ run result
display ──▶ raw frame (pen plane + resolved color plane + palette dump)
audio ──▶ raw PCM (+ optional tick-stamped channel events)
```

Arrows always point **machine → host**; the only host → machine inputs are: content bytes, configuration, logical input timeline, advance requests, state bytes.

---

## Host boundary / API

### Operation set and postconditions

| Operation | Inputs (host → core) | Outputs | Postcondition rules [REC] |
|-----------|---------------------|---------|---------------------------|
| `create(options)` | optional allocator, configuration (model profile, RAM power-on policy + seed) | opaque instance | failure transfers nothing; no globals; two instances never alias state |
| `destroy(&inst)` | handle | clears handle | idempotent on null; host serialises calls (LC-LIFE-01 style) |
| `load_content(bytes, config_id)` | cartridge/direct-load bytes (copied; host may free after return) | load result + identity | success replaces content and power-on-resets; **rejected load leaves the active session untouched, and an unloaded instance stays unloaded** |
| `load_firmware(bytes)` (optional tier C) | 512 bytes | identity | host-supplied only; never a path; absent firmware is a legal configuration |
| `unload()` | – | – | back to pristine-unloaded; EEPROM/persistent media must have been copied out first if the host wants it |
| `set_input(mask)` or `input_timeline(events[])` | logical buttons | – | does not advance time; sampling point is declared in the profile |
| `advance(request)` | `max_ticks`, stop mask, caller-owned video/audio spans | `ticks_advanced`, cumulative ticks, `stop_reason`, overshoot, `frame_completed`, produced samples | **preflight refusals (bad limit, buffer too small) advance nothing and write no output**; HALTED/JAM is reported, not hidden; capacity is checked before the run, not truncated silently |
| `reset()` | – | – | **[REC] Lynx has no reset button (INF); reset = power-on reset under the configured RAM policy.** A warm/RAM-preserving reset is deferred. Content stays loaded |
| `inspect(domain, addr, len)` | domain id, range | bytes | never changes state or time; unsupported domain → `UNSUPPORTED`, out of range → `INVALID_ARGUMENT` |
| `read_state()` (structured) | struct id | CPU/Mikey/Suzy/arbiter/scheduler snapshot | consistent at an instruction boundary |
| `trace_configure/drain` | mask, ring size | records + dropped count | zero effect on emulated time or state |
| `copy_media / import_media` | persistent span (EEPROM, writable cart bank) | bytes + generation token | host owns durable writes; import allowed only between load and first positive advance |
| `state_size / capture / restore` | span | – | size/capture/restore never advance time; **restore validates the envelope and builds the new state off to the side before swapping, so a rejected restore leaves the live machine, media and counters unchanged**; identity covers build, model profile, startup strategy, config, content digest, firmware identity (if any) |

### Core API vs LibFuturo contract: fit assessment

Read from `~/projects/libfuturo/docs/contract.md` and `include/libfuturo/libfuturo.h` (sibling repo, read-only, 2026-11-09). LibFuturo is explicitly an experimental source API with no ABI promise and no Lynx profile.

| Need | LibFuturo today | Fit | [REC] |
|------|-----------------|-----|-------|
| Instance lifecycle, host-owned buffers, rejected-load-keeps-session, preflight refusal, snapshot with validate-before-apply | LC-LIFE/OWN/LOAD/LIMIT/ERROR/SNAPSHOT | Good match with the table above | Design the native API to satisfy these obligations directly so an adapter is thin |
| Work unit | `work_units` in profile unit; `LF_TIME_FRAME` = one completed frame-advance interval; separate `core_ticks_valid/cumulative_core_ticks` with its own unit enum; steady cadence `0/0` = undeclared | Lynx fits **if** a frame is defined as the VSYNC event and cadence is `0/0` (guest-programmable). A tick-bounded advance has no LibFuturo request field | Adapter maps `work_units=1 frame` to `advance(max_ticks = cap, stop_on = FRAME)`; report ticks via core-tick fields. A new tick unit enum (e.g. Lynx master tick) is a **shared-contract change: batch for the owner** |
| Frame never completes (guest disabled DMA/Timer 2) | Stop reasons: BUDGET_REACHED / REJECTED / CORE_HALTED | Gap | Adapter caps at a profile-declared max ticks, returns BUDGET_REACHED with 0 completed units and `video_valid=0`; spell this in the Lynx profile |
| Video | `INDEX8` = raw color-number bytes, "neither grayscale nor RGB", palette policy is the host's; `RGB24` | Lynx output is **12-bit color resolved through palette registers that guests may rewrite mid-frame**; a pen-index plane alone cannot reproduce the image | Native core outputs resolved 12-bit words (+ optional pen plane + palette dump). First LibFuturo adapter may emit RGB24 by a fixed, documented bit-replication of the 4-bit channels, or the owner may extend LibFuturo with a 12-bit/palette format. **Decision for owner** |
| Audio | `S16` interleaved, `audio_rate_numerator/denominator`, produced-samples count | Fits stereo (MSTEREO L/R). Rate must be an exact rational | See Audio decision below |
| Input | `lf_input.buttons` bitmask sampled `AT_ADVANCE_START` | Lynx needs A, B, Option1, Option2, Pause + D-pad = 9 bits; fits | Native timeline is finer; adapter supplies one mask per advance |
| Persistent media | `lf_copy_media/import_media` + generation token | Fits cart EEPROM / writable bank | Same ownership rules |
| Firmware | no firmware slot in the surveyed header | Gap only for tier C | Keep tier C out of the LibFuturo route until the owner decides |

**libretro-like host mapping [INF]:** `retro_run` → one frame advance; `retro_get_system_av_info` geometry 160×102 and a declared fps (guest can change refresh; libretro-handy exposes a "Video Refresh Rate" core option defaulting to 60 Hz **[SRC: docs.libretro.com/library/handy]**); input polling stays in the adapter and is converted to the logical mask *before* `advance`; `RETRO_MEMORY_SAVE_RAM` ↔ EEPROM span, `SYSTEM_RAM` ↔ RAM inspect domain; BIOS lives in the host's system directory and is passed to the core as **bytes** by the adapter. libretro-handy's option list is the cautionary example: a frameskip option and a CPU overclock multiplier are presentation/performance features that must not enter the machine core **[SRC docs]**.

### Keeping host clocks and files out of the core

- Inputs are only: bytes, configuration records, logical input, budgets, caller-owned buffers. No function pointers from the host during `advance` (trace is pulled).
- **[REC] No floating point in the machine** (Gearlynx uses `float` volumes and a Q15 low-pass alpha **[IMPL]**; float is fine for host-side filtering, not for canonical samples). No wall clock, no PRNG except one seeded and serialised, no threads, no hidden statics, no `time()`, no environment reads, explicit-width integer state, explicit endianness in snapshots, no uninitialised memory, defined behaviour for all shifts/overflow.
- RAM power-on content is a **configuration field** (`zero | pattern | seeded`), recorded in the run's identity. Real DRAM is not zeroed **[INF]**; guests that read uninitialised RAM are then reproducibly, not accidentally, deterministic.
- Persistence (EEPROM bytes, writable cart bank) is **copied out/in by the host**. The core never opens a file. Gearlynx's `SaveRam(path)`/`LoadState(path)` entry points show the shape to avoid **[IMPL]**.

---

## Observability

| Layer | What | Mechanism | Notes |
|-------|------|-----------|-------|
| **O0 Inspect** | RAM (raw), CPU-mapped view, boot ROM region, cart banks, EEPROM, Suzy regs, Mikey regs, palette | `inspect(domain,addr,len)`; domains enumerated in the profile | side-effect-free implementations (Pattern 5) |
| **O1 Structured state** | CPU regs/flags/cycle position; timer count/ctl/done/phase; interrupt pending bits; UART; audio channel state; Suzy FSM phase, math busy; arbiter owner, `cpu_bus_request_ff`, `suzy_enable_ff`; scheduler next deadlines | `read_state(struct)` at instruction boundaries | what a debugger and the spec tests need |
| **O2 Event trace** | `{tick, source, kind, args}` for: instruction retire, bus access (addr, kind, value, ticks, owner), IRQ assert/ack/entry, timer borrow, DMA burst grant, Suzy phase change, cart strobe/counter/read, MAPCTL write, UART bit/frame, EEPROM op | bounded ring + mask + dropped counter | Gearlynx's trace logger offers master-clock cycle stamps, absolute pagination and exact Suzy/Mikey/UART/cart event filters **[IMPL, MCP_README]**; same shape, own format |
| **O3 Guest semantic progress** | e.g., diagnostic result bytes in RAM, "progress" and "error code" bytes | **Result-block convention + watch**, not screen scraping | below |
| **O4 Raw video** | per completed frame: 160×102 resolved 12-bit color words; optional 4-bit pen plane; palette register dump at frame end; optional per-line palette log for raster effects | caller-owned spans | rotation/scaling/RGB are host concerns; `.lnx` rotation is a hint only |
| **O5 Raw audio** | canonical PCM at an exact rational rate + optional tick-stamped DAC/channel events | caller-owned span / trace kinds | filtering and resampling are host-side |

### Guest semantic progress (how a headless run knows the guest succeeded)

**[SRC]** lynx-tests suites store results in a volatile array (`g_results[RESULT_COUNT]`, exported from assembly) and paint them on screen with expected-value tables in `main.c`; the `cpu` suite has 18 result bytes with per-test pass/progress codes (e.g. Test 6 progress=3 means all sub-tests passed; Test 7 has Lynx I and Lynx II expected tuples). Their Makefiles emit `-Ln <suite>.sym`. The README promises no machine-readable output; the data is simply in RAM **[SRC]**.

**[REC]**
1. **Machine facts vs fixture interpretation.** The core exposes RAM bytes. A *fixture manifest* (project-owned) says "symbol `_g_results` at address X (from `cpu.sym`), 18 bytes, expected `{…}` with model-specific alternates". The runner evaluates it. The core never knows what a score or test is (matches the sibling doc's rule: machine facts separate from profile assertions).
2. **Own diagnostics** (tier A/B fixtures) define a tiny result-block convention: `magic, schema, progress, error, payload…` at a fixture-declared RAM address, plus a "done" byte so the runner can use **stop-on-watch** (stop when the done byte changes) instead of guessing a frame count.
3. **Watch/stop on write** is a pure observer: it records a trace event and optionally ends `advance` with `stop_reason=WATCH`. It must not use a fake hardware port; adding an MMIO register would change the machine under test.
4. Screen text from the suites is secondary corroboration (video capture), never the success oracle.

---

## Save-state and determinism

- **[REC] Design state as plain, serialisable structs from slice 0** (no pointers between components, scheduler queue included). Gearlynx serialises through a versioned stream serializer and still carries version-gated fields (`version >= 15` cart bank handling) **[IMPL]**; avoid that by having a format *identity* per build and refusing foreign versions (LibFuturo's snapshot rules: exact build identity, validate-before-apply) rather than promising migration.
- **Round-trip property test, early:** run N ticks → capture → run M ticks (hash A); fresh instance → restore → run M (hash B); require A == B. This is the cheapest way to find hidden state in any component, and a precondition for determinism claims.
- Determinism contract: `fresh isolated state + fixed content/config + fixed input timeline + fixed tick budget ⇒ identical state, trace, video, audio`. Two instances in one process and the same run twice in one process must also match (catches statics).
- **A same-core hash is repeatability, not hardware accuracy** (`AGENTS.md`). Evidence tags per test: `spec-vector`, `public-hw-test(lynx-tests@rev)`, `original-diagnostic`, `cross-emulator`, `repeatability`.

---

## Scaling considerations (cost, not users)

| Workload | Approach |
|----------|----------|
| Real-time play ≈ 16 M ticks/s | Bus-access stepping + catch-up costs tens of host ns per access; ample on a modern host. Do not optimise before a named workload baseline exists (`AGENTS.md`) |
| CI headless diagnostics (seconds of guest time) | Trivial; event skipping while CPU sleeps removes most work in sleep-heavy tests (`sdoneack`, timers) |
| Fast-forward / fuzz / many instances | Batch to the next deadline when no observers are enabled; keep the observable results unchanged; per-instance state is small (64 KiB RAM + device registers + cart image) |
| Trace-heavy debugging | Ring size and mask are caller-chosen; dropped counter makes loss visible |

**First bottleneck [INF]:** per-access scheduler `run_until` and trace appends. **Second:** Suzy sprite timing shell at pixel granularity (Gearlynx's `suzy_inline.h` is ~3.4k lines, the largest single component in that code base **[IMPL]**, which is a reasonable proxy for where complexity concentrates).

---

## Anti-patterns

1. **Fixed instructions/cycles per frame.** Wrong for all the reasons in the timing table. *Instead:* tick budget plus stop-on-event.
2. **Instant sprite paint plus a charged delay** (Handy lineage **[IMPL]**). Hides bus contention, `SPRSYS` busy semantics and display-DMA interaction that sdoneack/sprites4 probe. *Instead:* painter/timing-shell split with an explicit bus-master FSM (functional correctness first, timing later, boundary unchanged).
3. **Treating "65C02/65SC02/Atari Lynx" as one CPU.** *Instead:* named `CpuProfile` with per-model tables and tests that record which model's expectation each result matched.
4. **Per-timer private clocks.** *Instead:* shared-prescaler bank.
5. **MAPCTL as a flag the devices check.** It is a CPU-view decode only; Suzy/DMA use raw RAM. *Instead:* two views.
6. **Putting the cartridge inside Suzy or Mikey.** *Instead:* `cart/` with address and data ports.
7. **Core loads the BIOS or a ROM from a path.** *Instead:* bytes in, identity out; firmware is an optional content slot.
8. **Palette applied to RGB inside the machine as "the" output.** Mid-frame palette changes and LCD triad timing are lost. *Instead:* raw 12-bit words + palette dump; presentation outside.
9. **Using the BS93/HLE preset as if it were hardware reset state.** *Instead:* tag runs with `startup=` strategy and cite each preset value.
10. **Success judged from screenshots or same-core hashes.** *Instead:* result-block bytes plus independent expected values, with named-fault negative controls.
11. **Callbacks into the host mid-advance** (libretro-style environment calls inside the core). *Instead:* pull-based trace/inputs; adapters own callbacks.
12. **Copying scope from Gearlynx** (Game Drive, El Cheapo SD, LNX2 custom banks, cheats, VGM recorder, overclock, frameskip). These are product features outside "one named model + one cartridge path" **[IMPL]**. *Instead:* defer; none changes the boundaries.

---

## Integration points

### External

| Service/host | Integration pattern | Notes |
|--------------|---------------------|-------|
| Headless runner | Run script (content ref, config, input timeline, stop rule, observations) → artifact bundle with manifest + raw frames/audio/trace; digests computed by the runner | Rights: content refs are host paths to owner-held files; bundles never embed them |
| Independent consumer | Separate build linking the public ABI only; loads a project-authored diagnostic, drives input, checks result block + raw outputs | Proves the API is sufficient and relocatable (per sibling lessons) |
| LibFuturo adapter | Thin; see fit table | Lynx not in LibFuturo's reviewed profiles; any shared-contract change is an owner batch |
| libretro adapter | Later, optional; owns environment callbacks, system-directory BIOS path, option menu | Must not weaken the native contract |
| Toolchain | ca65/ld65 (cc65, zlib-licensed) pinned for fixtures; hand-assembled bytes for the first diagnostic | cc65 not installed on dev host; building it per CI run is a DevOps cost to measure |
| Hardware | out of scope (refresh-rate calibration with logic analyzer, two-console ComLynx) | lynx-tests `refresh-rate/` and `uart4/` are not headless fixed tests **[SRC]** |

### Internal boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| cpu ↔ bus | `access()` synchronous call returning value and advancing `now`; IRQ/NMI level and stall/sleep status read from arbiter | the only CPU-side port |
| bus ↔ mikey/suzy | device slave interface `read/write(addr, t)` + master requests (video/refresh/suzy) | Suzy RAM access goes through `raw_ram` + arbiter, not through `cpu_view` |
| mikey ↔ cart | `address_port.strobe(level)`, `.data(bit)`; EEPROM bit back to IODAT B4 | |
| suzy ↔ cart | `data_port.read()/write()` with tick cost | |
| any device ↔ scheduler | `schedule(tick, id)`, `cancel(id)`; callbacks are plain function-table entries invoked by the scheduler only | no re-entrancy: a device handler may schedule but never run the scheduler |
| core ↔ observe | read-only views + append to trace ring | observer cannot request machine mutation |
| api ↔ host | bytes, structs with `struct_size`, status codes with per-operation postconditions | version axes kept separate: API revision, ABI layout, behavior revision, build id, state format, content identity |

---

## Open architecture decisions

| ID | Question | Options | Recommendation | Decider |
|----|----------|---------|----------------|---------|
| AD-01 | Guest-time unit and sync granularity | (a) instructions/frame; (b) CPU cycles; (c) instruction-granular device sync with tick deltas (Handy/Gearlynx style); (d) master ticks, bus-access-granular, lazy catch-up | **(d)**, with the per-opcode-stamped fallback; master tick is the canonical unit, frames/instructions are stop conditions | Routine (reversible if encapsulated) but record in `spec/` |
| AD-02 | Baseline model and CPU profile | Lynx I; Lynx II; both | **Lynx II first** as one named `CpuProfile`; Lynx I as a later data diff and separately qualified claim | **Owner/roadmap**; evidence-driven |
| AD-03 | Startup strategy | A direct-load; B project boot shim; C owner BIOS; D HLE+decrypt | **A first, B next, C as a separately evaluated local route, D deferred**; every run tagged `startup=` | A/B routine; **C/D owner (rights)** |
| AD-04 | Post-boot preset provenance | copy another emulator's values; cite spec/tests per value; require fixture self-init | **Cite per value; leave uncited at reset state; fixtures self-initialise**; record disagreements (e.g., DISPCTL x'0D' vs Gearlynx $09) | Routine |
| AD-05 | Suzy modelling path | instant paint; FSM from the start; staged painter+shell | **Staged**: arbiter-aware bus-master shell and painter boundary from the first Suzy slice, pixel correctness first, tick accuracy via sprites1-5/sdoneack | Routine |
| AD-06 | Raw video contract | pen indices + palette regs; RGB24; resolved 12-bit words | **Resolved 12-bit words (160×102) + optional pen plane + palette dump**; adapters convert | Routine for native; **owner for any LibFuturo format change** |
| AD-07 | Raw audio contract | host-rate PCM; exact-rational PCM; tick-stamped events | **Exact-rational integer PCM derived from master ticks (rate = 16 MHz / N, N an integer, e.g., 40,000 or 50,000 Hz) as canonical, plus optional tick-stamped channel events as trace**; no float, no filter in core. Epyx gives a ~4 kHz one-pole low-pass in hardware, reported as 4 kHz in one place and ~4500 Hz in another **[SRC, LOW]**; leave to hosts | Routine; choose N in the audio phase |
| AD-08 | "Frame" definition and frame-less guests | Timer-2 borrow; last visible pixel of line 101; DISPADR latch | **Frame complete = last visible pixel of the last visible line transferred** (raw frame valid); always cap advance by max ticks; frame-less guests return BUDGET_REACHED with no frame | Routine; verify against display tests |
| AD-09 | Input timing | one mask per advance; tick-stamped timeline | **Native: tick-stamped timeline; adapters: one mask per advance.** Joystick/SWITCHES are polled registers (no strobe), so input is read whenever guest reads; left-hand swap is a Suzy-side transform of logical buttons | Routine |
| AD-10 | Guest progress channel | screen scrape; fake MMIO debug port; RAM result block + watch | **RAM result block + watch + symbol file (`-Ln`)** | Routine |
| AD-11 | API shape | LibFuturo-only; libretro-only; native C ABI + adapters | **Native C ABI (sized structs, status codes, caller-owned buffers) that satisfies LC-* obligations; LibFuturo and libretro as adapters**; no stability promise yet | Routine; **owner** for contract changes in LibFuturo |
| AD-12 | Snapshot scope | none; same-build only; versioned migration | **Same-build, identity-bound, validate-before-apply; include scheduler queue; round-trip test in an early slice** | Routine |
| AD-13 | RAM power-on policy | zeros; pattern; seeded PRNG | **Config field, default fixed pattern, seed recorded in identity** | Routine |
| AD-14 | ComLynx scope | none; single-console loopback; multi-instance wire | **Single-console loopback in the UART component behind a logical wire port**; multi-instance coordination is a runner-level, deterministic message exchange, deferred | Scope decision at roadmap |
| AD-15 | Cart header policy | accept `.lnx` with fallbacks; strict | **Strict**: refuse unknown/short headers and unknown EEPROM kinds; LNX2 customs, SD/Game Drive out of scope | Routine |
| AD-16 | Implementation language / build | not selected by owner | Not decided here. Architectural constraints from this document: exports a C ABI, no runtime hidden allocation during advance, no floats in machine, deterministic integer semantics, reproducible build, small dependency tree (`AGENTS.md`). The STACK researcher should evaluate against these | **Owner** |

---

## Suggested build order (vertical slices and dependencies)

Dependency spine: `timebase → bus+RAM → cpu → mikey timers/irq → (display | audio | uart) ; suzy regs → cart → suzy math/sprite`. `observe`, `state` and `api` grow with every slice so each slice ends with a runnable, hashable, replayable artifact.

| # | Slice | Components added | Evidence/exit criteria | Depends on |
|---|-------|------------------|------------------------|------------|
| 0 | **Walking skeleton** | timebase, flat RAM bus, trace ring, `api` create/destroy/load/advance/inspect, runner, independent consumer, artifact bundle, determinism harness, snapshot round-trip on the trivial state | two runs and two instances yield identical bundles; refused load keeps state; named-fault control (e.g., injected nondeterminism) is caught | none |
| 1 | **CPU on flat bus** | 65C02 engine, `CpuProfile` (Lynx I and II tables), per-access sequencing with 5-tick default | generic 65C02 vectors (SingleStepTests/65x02 has rockwell/synertek/wdc 65C02 sets, MIT, per-instruction with bus activity **[SRC: repo page, LOW detail]**) adapted per profile, plus lynx-tests `cpu` expectations noted as pending IRQ support; Lynx-specific deviations listed as spec rules | 0 |
| 2 | **Memory map + tier A startup + first diagnostic** | MAPCTL, ROM slot (empty), vectors, direct-load container and preset table, hand-assembled diagnostic with result block, watch/stop | guest reports success by RAM result block, no video; evidence tag `original-diagnostic, startup=A` | 1 |
| 3 | **Mikey timers + IRQ + CPU sleep (no Suzy)** | shared-prescaler timer bank, link graph, INTSET/INTRST, IRQ sampling rules, arbiter skeleton with `cpu_bus_request_ff`, CPUSLEEP | lynx-tests `timers`, `timers2`, `cpu` (after startup path allows loading them), own timer diagnostics; state round-trip covering timers | 2 |
| 4 | **Display** (visible output early) | line/frame generator, DISPADR latch, DMA bursts as bus requestor + page-mode closing, refresh, palette, raw frame capture, frame-complete stop | CPU-drawn homebrew test pattern visible; palette/mid-frame cases; page-mode suite (`page-mode`); raw video bundle | 3 |
| 5 | **Audio** | 4 channels, LFSR/integrate, link ring with timers, MSTEREO, exact-rational PCM | lynx-tests `audio`, `audio2`; deterministic PCM hash | 3 (independent of display and Suzy) |
| 6 | **UART single-console** | SERCTL/SERDAT, timer 4, loopback echo, level IRQ | `uart`, `uart2`, `uart3` | 3 |
| 7 | **Suzy shell + cart + input + math** | register file, SUZYBUSEN, bus-master FSM, cart address/data ports, tier B boot shim, joystick/switches, timed multiply/divide, left-hand swap | `memio`, `math`, `sdoneack`; tier B boot of a project container through the real cart port | 3, 4 for DMA interplay |
| 8 | **Sprite painter, then timing** | painter (all 8 operation types, collision, depository), then phase timing and DMA interplay | pixel CRCs (`sprites3`), then timing suites `sprites1/2/4/5` | 7 |
| 9 | **EEPROM + persistent media + save-state hardening** | EEPROM state machine, media copy/import, snapshot with media | save/restore fresh-process route, refusal controls | 7 |
| 10 | **Packaged consumer + LibFuturo adapter** | adapter, installed relocation check | clean consumer proof; Lynx profile doc | 0-9 as available |
| 11 | **Playable/homebrew scenario** | needs 4, 7, 8 (and 5) | owner-visible scenario (rights-reviewed or original) | 4, 5, 7, 8 |
| 12 | **Second model qualification, tier C route** | Lynx I diff, firmware slot | separate evidence plan | owner decisions |

**Ordering rationale.** Timers/IRQ/sleep (3) precede display because the frame is built from Timers 0/2 and the interrupt/sleep machinery is shared with Suzy; the `cpu` and `sdoneack` suites depend on Timer 6 IRQs even before Suzy exists. Display (4) comes before Suzy so a *visible* result exists early (CPU-drawn pixels) and so display DMA exists when Suzy timing is first measured (`sprites4 DMA EXP`). Audio, UART and Suzy are independent of each other after slice 3 and can be parallelised. Cart port and the tier-B shim sit with Suzy (7) because RCART lives there; tier A keeps early slices unblocked.

**Research flags for phases:**

| Slice | Flag |
|-------|------|
| 1 | Needs a spike: 65C02 vs 65SC02 vs Rockwell-bit-op sets against the Lynx I/II expectations; confirm which SingleStepTests directory applies and where Lynx differs; unverified here |
| 2 | **Deeper research:** what each preset register should be and from which evidence; whether the cc65 stage-1 substitute is viable; fixture build recipe and manifest |
| 3 | Needs verbatim re-read of Epyx timers/irq appendices and the `timers2` expectations; interrupt-entry quirks (`cpu` TESTs 1 and 8) |
| 4 | Needs hardware-informed constants (DMA burst ticks, refresh period/cost: Gearlynx uses 28 and 4 per 256 as implementation choices, not spec) cross-checked with the logic-analyzer capture notes; **[OPEN]** |
| 7-8 | **Deepest research:** Suzy bus ownership, grant latency, row/packet costs; lynx-tests `lynx-sprite-performance.md` (59 KB) is the primary map; Epyx Appendix 7 bus state machine not obtained |
| 9 | EEPROM wiring and kinds (93C46/56/66/86 are Gearlynx's list; confirm on a pinned source) |
| 5-6 | Standard patterns once timers exist; audio channel LFSR/integrator details are covered by the lynx-tests audio suites |

---

## Sources

Primary/spec (accessed 2026-11-09; Epyx pages via summarizing fetch, so MEDIUM at best until re-read verbatim):
- Epyx Handy specification mirror: https://www.monlynx.de/lynx/lynx2.html (hardware overview, 16 MHz/62.5 ns tick), lynx3.html (pitfalls), lynx4.html (CPU/ROM, access-cost table, sleep), lynx5.html (display, 60/50/75 Hz recipes), lynx6.html (sprites), lynx7.html (audio, ROM cart), lynx8.html (timers), lynx10.html (reset/power-up, variances, common errors), lynx11.html (system bus interplay), irq.html (interrupts and CPU sleep), hardware.html (address map, MAPCTL, registers).
- Index and scanned PDF: https://atarilynxdev.net/doku.php/lynx:doc:start
- cc65 Lynx docs: https://cc65.github.io/doc/lynx.html (the fetched text did not name the CPU; the `PROJECT.md` note that cc65 says 65SC02 remains to be rechecked); cc65 `libsrc/lynx/bootldr.s` (Karri Kaksonen, 2011): https://raw.githubusercontent.com/cc65/cc65/master/libsrc/lynx/bootldr.s
- lynx-tests (MIT-marked), read from a scratch clone at commit `a920f01164d37edff94939993fbc0551e5b7000b`: README.md, `lynx-page-mode.md`, `lynx-timers.md`, `lynx-video-timing.md`, `shared/lynxcc65.mak`, `cpu/main.c`, `.github/workflows/build.yml`. https://github.com/drhelius/lynx-tests
- SingleStepTests/65x02 (MIT; rockwell65c02, synertek65c02, wdc65c02 folders; JSON with bus activity): https://github.com/SingleStepTests/65x02

Implementation cross-checks (behaviour reports only; GPL code not reused):
- Gearlynx (GPL-3.0), commit `94068da354f268cbc060f9fe0b1d3b2228952552`, files read: `gearlynx_core*.{cpp,h}`, `bus_inline.h`, `m6502_timing.h`, `memory*.{cpp,h}`, `media*.{cpp,h}`, `suzy_inline.h` (stall logic), `lcd_screen.h`, `mikey.h`, `audio*.h`, `eeprom.h`, `types.h`, README, MCP_README. https://github.com/drhelius/Gearlynx
- libretro-handy / Handy (zlib): `lynx/system.cpp`, `system.h`, `susie.cpp`, `mikie.cpp` via fetch; docs: https://docs.libretro.com/library/handy/ and https://docs.libretro.com/library/beetle_lynx/ (BIOS requirement, lineage).
- MAME `src/mame/atari/lynx.cpp`: https://github.com/mamedev/mame/blob/master/src/mame/atari/lynx.cpp (via fetch).
- AtariAge community reports on loader framing (search snippets only, LOW): https://atariage.com/forums/topic/129030-lynx-encryption/ and https://forums.atariage.com/topic/175089-original-bootloader-sources/ (the latter returned 403 to fetch).

Sibling project evidence (read-only local files, 2026-11-09):
- `~/projects/libfuturo/docs/contract.md`, `include/libfuturo/libfuturo.h`, `docs/muhtari-profile.md`, `docs/bootstrap/architecture.md` (time-unit, INDEX8, snapshot, refusal obligations; Muhtari's `core_ticks_valid=0` precedent).

No BIOS image, commercial ROM or key material was fetched or stored. Gearlynx's embedded public-modulus constant and `no_bios.h` splash image were observed only as existing in the source tree; their bytes are not reproduced here.

---
*Architecture research for: accurate, deterministic, observable, embeddable Atari Lynx emulator core*
*Researched: 2026-11-09*
