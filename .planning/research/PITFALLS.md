# Domain Pitfalls

**Domain:** Accurate, deterministic, headless-drivable Atari Lynx emulator with a strong verification and rights-hygiene bar (Lynxplosion)
**Researched:** 2026-11-09 (all web/repo access dated 2026-11-09 unless stated)
**Mode:** Adversarial pass over the seed (`PROJECT-SEED.md`), `EMULATOR-DNA.md` and the recommended sequence
**Overall confidence:** MEDIUM (repo/source-file observations are HIGH; Epyx HTML pages were read through a summarising fetcher and forum material is LOW; both are labelled and flagged for recheck)

---

## 0. How to read this file

**Evidence labels (every non-obvious claim carries one):**

| Label | Meaning |
|-------|---------|
| `[SRC]` | I read the primary file directly (git clone or raw file) and checked what it says. |
| `[WEB]` | A web page read through a summarising fetcher. The GSD confidence seam rates unverified websearch/webfetch as LOW and cross-checked as MEDIUM. Recheck before it becomes a requirement. |
| `[MNT]` | Maintainer report (author of a test suite or emulator describing its own hardware observations). Not independent. |
| `[FORUM]` | Forum post or hobbyist note. LOW. Lead only. |
| `[INF]` | My inference from the above. |
| `[REC]` | Recommendation. |

**Phase abbreviations used in warnings and the mapping table** (these are research labels, not roadmap decisions):
`FND` = foundation, tooling and evidence harness; `CPU` = CPU, bus, Mikey timers and interrupts; `BOOT` = startup, boot ROM policy, cartridge, EEPROM; `VID` = Mikey display/DMA/palette; `AUD` = audio; `SUZY` = sprites, collisions, math, bus ownership; `HOST` = host API, runner, independent consumer; `CI` = CI and performance; `PROD` = roadmap and product scope.

**Pins observed on 2026-11-09 (candidates, not decisions):**
- `drhelius/lynx-tests` HEAD `a920f01164d37edff94939993fbc0551e5b7000b` (2026-09-20), latest release tag `1.4.9` (2026-08-13). 21 tags/releases since 2025-10-04. `[SRC]`
- `cc65/cc65` master `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2` (2026-11-08). Latest release tag is `V2.19`. `[SRC]`

---

## 1. Adversarial verdict on the seed's recommended sequence

The seed recommends: name one model/CPU profile and one cartridge path, resolve startup, then build a small end-to-end slice using a source-built diagnostic (lynx-tests as a candidate fixture source) with deterministic input/time, headless runner and independent consumer. The shape is sound. These are the places where the evidence says it will break if followed literally.

**V1. "Resolve startup" is the gating item, and the cheap answers do not exist.** Every cc65-built `.lnx` begins with a 52-byte *encrypted* first-stage loader. cc65's `bootldr.s` says the bootloader "creates a signed binary so that the Lynx will accept it", ships the plaintext only as a comment, and embeds the encrypted bytes (`$ff` count byte plus 51 bytes). `[SRC]` Forum sources say the boot ROM decrypts it with RSA, public exponent 3, into RAM at `$0200`. `[FORUM]` So a cc65 `.lnx` cannot boot without (a) the real boot ROM, (b) an independently written boot ROM that reproduces the decrypt-and-jump, (c) an HLE that implements the decryption itself, or (d) bypassing the cart entirely through a RAM-load (BLL/BS93 or `.o`) path with a different link recipe. None of these is "no firmware needed". [REC] Treat startup as three explicitly labelled tiers (direct-RAM harness, owner-supplied ROM kept local, independently authored ROM) and do not let the first slice claim "boot" evidence it did not exercise. See P1, P2.

**V2. The seed omits that a free boot ROM and a RAM-load loader already exist in the wild.** Holani (GPL-3.0) states it "embeds 6502 binary blobs": a "Free Lynx Boot Rom, courtesy of lynxdev.atari.org" and a BLL uLoader from 42Bastian/new_bll. `[SRC]` MAME's driver has a `.o` quickload that writes RAM, patches the reset vector and sets PC (BIOS-less). `[WEB]` An AtariAge thread describes a replacement boot ROM that "loads the commercial ones fine" but is not claimed identical, with source availability unclear. `[FORUM]` Their provenance and licence are unverified, so they are candidates to investigate, not assets to adopt.

**V3. Lynx-tests is not a "pure CPU, then devices" ladder.** Its `cpu` suite needs Mikey Timer 6 IRQs, interrupt vectors in RAM, MAPCTL overlays and (after the results are captured) TGI/Suzy text drawing. `[SRC]` The CPU phase cannot be accepted against lynx-tests alone. Pair it with per-opcode vectors filtered to the Lynx profile (see P6), and expect a minimal Mikey timer/IRQ slice inside the CPU phase.

**V4. "Do not assume machine-readable results" can be resolved in the owner's favour, with a trap.** All 17 single-console suites (and `refresh-rate`) expose a global `g_results` byte array and 19 of 20 directories link with `-Ln <name>.sym`, so a harness can read results from RAM by symbol and drop OCR from the plan. `[SRC]` The trap is that pass/fail logic is split between the ROM (some suites self-judge, for example `timers2` treats `0` as PASS) and host-side C tables in `main.c` (for example `cpu`), and many "pass" values are `0x00`, which is also what uninitialised BSS looks like (see P20).

**V5. The fixture source is moving fast and has a single author.** Between 2026-07-30 and 2026-09-20 the suite added sprites1-5, sdoneack, uart3, uart4 and timers2, "Improve cpu tests" and "Improve memio tests", and removed a test. `[SRC]` The same author wrote Gearlynx, whose README says it "passes all tests from Atari Lynx Hardware Test ROMs". `[WEB]` Several guide measurements say "the hardware model was not recorded" or "single attempt". `[SRC]` Treat the suite as one tier of evidence with an explicit pin and a "who verified this against what" column (P19).

**V6. Toolchain pin is not optional and not what the host has.** `cpu/tests.s` uses `ENABLE_INT` and `ENABLE_COUNT` from `asminc/lynx.inc`. They exist on cc65 master and are absent from the `V2.19` tag (I grepped both). `[SRC]` Lynx-tests CI builds cc65 from unpinned master. `[SRC]` A distro or Homebrew cc65 (V2.19) will not assemble the suite. The owner's dev host currently has no cc65. `[SRC: PROJECT.md]`

**V7. "160×102 at one refresh rate" is not an output contract.** Timer 0, Timer 2 and PBKUP set line length, line count and refresh; the suite ships an interactive `refresh-rate` utility precisely because games change them. `[SRC]` The seed correctly says to confirm pixel conventions before freezing an output contract; add frame timing to that list.

**V8. One model is right, but "Atari Lynx" CPU tests auto-detect the model and pass on either.** `cpu` Test 7 shows `LYNX 1` or `LYNX 2` as a *pass*; a mismatch against both is the only failure. `[SRC]` An emulator implementing the wrong profile therefore passes unless the harness asserts which model was detected (P6, P19).

**Net effect on sequencing [REC]:** keep the seed's order but insert an explicit "evidence harness and startup tiers" step before any claim of a boot-capable slice; put the Mikey timer/IRQ minimum inside the CPU phase; make the first end-to-end slice a RAM-loaded (BLL or otherwise) diagnostic labelled "HLE start, no boot evidence", then add boot evidence separately.

---

## 2. Critical Pitfalls

### Boot, firmware and rights

### P1: Assuming a cc65 `.lnx` boots without the boot ROM

**What goes wrong:** The team builds `lynx-tests/cpu` with cc65, loads the `.lnx`, and nothing runs, or the project quietly adopts an emulator-style HLE that "just jumps to the cart" and starts producing results that never touched the real startup path.

**Why it happens:** `.lnx` looks like a ROM image. In fact the first cart bytes are an encrypted loader the boot ROM must decrypt (`$ff` count byte + 51 encrypted bytes = one block), which then runs at `$0200`, reads a 151-byte secondary loader from the cart through `RCART0`, jumps to `$FB68`, reads an 8-byte file entry, selects the block by bit-banging `IODAT`/`SYSCTL1`, and copies the main program to RAM. `[SRC: cc65 libsrc/lynx/bootldr.s]` cc65's own docs call it a "very minimal bootloader that does not check the cart or show a title screen". `[SRC]` Gearlynx, Holani and libretro/Gearlynx all need a BIOS or an embedded substitute. `[WEB][SRC]`

**How to avoid:**
- Decide the startup tier per evidence claim and record it in the run manifest: `start=ram-load | boot-rom-local | boot-rom-clean`.
- Use a RAM-load path only for component tests that do not read the cartridge; its fixture recipe differs (`lynx-bll.cfg`, documented by cc65) so it needs its own manifest and checksum. `[SRC]`
- Do not describe RAM-load results as boot, cart or loader evidence.
- If an HLE decrypts the first stage, source the public modulus from a non-GPL, rights-clear reference and review it; do not copy Handy/libretro `lynxdec` or Gearlynx code without a licence review.

**Warning signs:** a fixture recipe that never mentions a boot path; "passes in CI" with no firmware configured and no RAM-load step; an HLE that consumes the whole `.lnx` header+payload without modelling the cart read pointer.

**Phase to address:** BOOT (decision), FND (manifest fields), HOST (runner option `--start`).

---

### P2: HLE boot shortcuts that hide real behaviour

**What goes wrong:** The emulator starts the guest at the cart's entry with "reasonable" register values; programs run, but timer phase, cart pointer, port direction, display state and RAM contents differ from hardware, so later timing and loader tests disagree and nobody knows why.

**Why it happens:** The ROM does observable work that programs depend on or tolerate:
- Epyx lists ROM side effects as hardware variances: "The Mikey ROM sets the External Power Detect pin to output" and "The REST pin is initialized as an input with data 0. The correct setup is output with data 1." `[WEB]` Software must undo the ROM's `IODIR`/`IODAT` choices.
- The cc65 first stage assumes the cart read pointer is *exactly where the ROM left it* (it reads the secondary loader sequentially from `RCART0` after the 52 encrypted bytes), then forces MAPCTL itself. `[SRC]` [INF] An HLE must leave the cart counter and block state at that post-ROM position, not at byte 0.
- Epyx reset text: Suzy is disabled until the CPU enables it, video/refresh are disabled, ROM overlay enabled, and the slow clocks "have no deterministic phase relationship." The documented init sequence is marked "(more ??)", incomplete. `[WEB]`
- Timer-phase tests (`timers2` ENABLE PHASE, COUNT PHASE) measure the first edge of a shared prescaler relative to when the program enables a timer; the starting phase is a hardware-undefined parameter. `[SRC]`

**How to avoid:**
- Write a "post-boot state record" per start tier (all Mikey/Suzy registers, MAPCTL, IODIR/IODAT, timer counts and prescaler phase, cart counter/block, RAM fill pattern, stack and zero-page contents) and put its hash in the run identity.
- Make the power-on phase and RAM fill explicit profile parameters (seeded, never host random), and run the timing-sensitive suites at more than one declared phase.
- Compare the HLE post-boot record against a real-ROM run *locally* (owner firmware, never in shared CI) and keep only the diff, not the ROM bytes.

**Warning signs:** an HLE function that only sets PC; fixture expectations that were captured under HLE and then reused as "boot" evidence; timers2 or loader tests that pass only at one phase.

**Phase to address:** BOOT, CPU (timer phase), FND (state record schema).

---

### P3: BIOS or commercial bytes leaking through CI caches, artifacts, traces and goldens

**What goes wrong:** An owner-supplied BIOS or a commercial ROM ends up in a GitHub Actions cache, uploaded artifact, release asset, trace log, memory dump, frame capture, or committed "golden" file. Even if never committed to git it is distributed.

**Why it happens (specific to emulators):** Debug facilities dump memory areas (Gearlynx's MCP exposes a "BIOS" memory area `[WEB]`); boot-run frames and audio contain the ROM's own boot output (Recalbox notes that Handy/Mednafen use the BIOS "to reproduce the boot animation"). `[FORUM]` MAME dumps taken from a running machine differ by one byte at `$FFF9` (MAPCTL, unreadable as ROM) so hash allow-lists are both too strict and not proof of rights. `[WEB]` Gearlynx will "load any BIOS" with only a recommended MD5. `[WEB]`

**How to avoid:**
- Fail closed: runner refuses any firmware path unless an explicit local-only flag is set, and CI jobs fail if a firmware env var, file path or cache key is present.
- Allow-list upload paths in CI; scan artifacts and caches for 512-byte boot-ROM-shaped files, `.lnx`/`.lyx`/`.bin`/`.img` files without a manifest, and any file whose SHA-256 matches a deny list.
- Keep boot-ROM-derived frames, audio, traces and memory dumps out of repository and shared CI. Store only pass/fail plus hashes if the owner approves hash storage, and mark these as "contains firmware-derived output".
- Firmware identity is a record (hash, claimed model, dump method), never a rights statement.
- Pre-commit/CI check that every binary under `fixtures/` has a manifest entry with source, recipe, checksum, licence, notices and allowed use.

**Warning signs:** `actions/cache` paths that include a data dir; screenshot goldens captured from a boot run; a "bios/" folder in `.gitignore` but not in the artifact deny list; a runner default that searches the home directory for `lynxboot.img`.

**Phase to address:** FND (policy and scanners), CI (enforcement), BOOT (runner flags).

---

### P4: GPL contamination from reading or copying Gearlynx, MAME, Holani

**What goes wrong:** Implementation details, tables or structure from a GPL emulator enter the codebase, forcing GPL terms or creating a licence dispute, or the owner's later licence choice is foreclosed.

**Facts:** Gearlynx is GPL-3.0 (README). `[WEB]` Holani is GPL-3.0 (GitHub licence API). `[SRC]` MAME's `lynx.cpp` header reads `license:GPL-2.0+`, `copyright-holders:Peter Trauner`. `[WEB]` The Gearlynx logic-analyzer capture the page-mode guide relies on (`lynx2-mikey-page_mode.dsl`) lives in the GPL-3.0 Gearlynx repo, not in MIT lynx-tests. `[SRC]` The MIT lynx-tests guides embed Gearlynx pseudo-code and file links; that text is MIT but points readers into GPL source. `[SRC]` Gearlynx's own docs say its model is a reconstruction. `[SRC]`

**How to avoid:**
- Written clean-room rule: implementers work from Epyx documents, the MIT lynx-tests source/guides, cc65 (zlib) and measurements; Gearlynx/MAME/Holani source is not opened by implementers. A separate "diff investigator" role may run an emulator binary locally and report *behaviour*, not code.
- Treat the DSL capture and any Gearlynx-hosted data as GPL-3.0 data; do not vendor it.
- Do not copy status values, tick units or API shapes from other emulators (DNA already says this).
- Record in each phase's research file which sources were consulted.

**Warning signs:** a table in new code that matches another emulator's naming/ordering; a commit message citing a Gearlynx function; copied MCP tool names.

**Phase to address:** FND (policy), every phase (review gate).

---

### P5: Assuming "MIT" settles lynx-tests (recheck of the actual licence)

**What I checked `[SRC]`:** root `LICENSE` is the standard MIT text, "Copyright (c) 2025 Ignacio Sanchez Gines". There are no per-file licence headers in `*.c`, `*.s`, `*.inc`, `*.h`, `Makefile` (I grepped for copyright/licence/SPDX). Nothing in the repo vendors binary ROMs; `.lnx` files exist only as CI artifacts and release zips.

**What the MIT root file does not cover:**
- Built `.lnx` files include the cc65 runtime (zlib-style licence, "notice may not be removed or altered" for source) and Karri Kaksonen's encrypted loader blob (his `bootldr.s`, 2011). `[SRC]` Fixture manifests need notices for all three.
- `sprites2` ALPINE FLIP embeds an "Exact Alpine Games protection source" literal and a hand-copied SCB (`ScbAlpine`, "Alpine Games SCB $59F9", pen colours and sizes). `[SRC]` Tiny and functional, but its provenance is a commercial game; record it in the manifest and have the owner decide if that is acceptable for a rights-clean fixture set.
- README images and the Gearlynx-hosted capture are separate assets.
- The suite will keep changing; a licence confirmed at one commit says nothing about later ones.

**How to avoid:** pin by commit SHA (not branch or "latest"), vendor nothing until the fixture manifest is written, include MIT + zlib notices in the manifest, re-read `LICENSE` at each bump, and keep the Alpine item flagged "owner rights decision".

**Warning signs:** a manifest with "MIT" as the only licence field; a fixture update PR that changes more than the pin and the checksum.

**Phase to address:** FND.

---

### CPU, bus and timing

### P6: CPU profile mislabelling and reuse of a stock 65C02 core

**What goes wrong:** The CPU is implemented as "65C02" or "65SC02" by name, with a library or a generic core. It then fails or silently diverges on Lynx-specific opcode and interrupt behaviour; or it passes `cpu` because the suite auto-detects the model.

**What the sources actually say:**
- Epyx: "The CPU is a 65C02 core embedded in the Mikey IC." `[WEB]` Generic family name.
- cc65: the *documentation page does not name a CPU* (I searched `doc/lynx.html` for 65SC02/65C02: no hit). The *compiler's target table* maps `lynx` to `CPU_65SC02` (`src/common/target.c`, line 246). `[SRC]` ca65 defines `65SC02` as "first CMOS instruction set (no bit manipulation, no wai/stp)" and `65C02` as "CMOS with Rockwell extensions"; `W65C02` as the full set with `wai/stp`. `[SRC/WEB]`
- lynx-tests: Test 7 emits Rockwell bit opcodes (`$07`, `$87`, `$0F`, `$8F`) as raw `.byte`s inside a file assembled with `.setcpu "65C02"` and expects Lynx I to execute them as NOPs (the operand bytes then run as instructions; the BBR/BBS offset byte `$18` becomes CLC) and Lynx II to execute RMB/SMB/BBR/BBS. `[SRC]` The suite's own comments mislabel: file header says "65SC02 Subset - RMB/SMB/BBR/BBS" while the cc65 meaning of 65SC02 *excludes* them. `[SRC]`
- Test 8 runs every `$x3` and `$xB` reserved opcode (including `$CB` and `$DB`, which are WAI/STP on a W65C02) as NOPs and checks that an IRQ is *not* taken inside the block; Test 6 checks reserved 1/2/3-byte NOPs. `[SRC]` A W65C02S core would hang on `$CB`/`$DB` and mis-handle IRQ timing.
- Test 2 checks D is cleared on interrupt entry and restored by RTI; Test 3 checks BCD flags; Test 1 checks the 65C02 SEI/CLI interrupt-delay; Test 5 checks the 65C02 JMP-indirect fix; Test 4 checks BRK is two bytes and pushed P has B and bit 5 set (`0x30`). `[SRC]`
- Forum sources conflict on whether Lynx I and II have different CPUs. `[FORUM]` The only hardware-based statement is the lynx-tests maintainer's. `[MNT]`

**How to avoid:**
- Name the profile by instruction-set *behaviour table*, not chip name: `bit-ops=on/off`, `wai/stp=NOP`, `reserved NOP lengths and cycle model`, `decimal flag rules`, `irq-entry delay`, `x3/xB IRQ blocking`.
- Per-opcode vector suites (for example SingleStepTests/65x02, MIT; it ships `rockwell65c02`, `synertek65c02`, `wdc65c02` and 6502 sets, none is a Lynx profile, and its README says vectors come from "an implementation that conforms to all available documentation", not hardware) `[SRC]` must be filtered per profile and treated as spec-expectation tier only. Exclude RMB/SMB/BBR/BBS for the Lynx I profile and WAI/STP for both.
- Harness must assert the detected model (`g_results[12..15]`: Lynx I = `FF 00 02 02`, Lynx II = `FE 01 01 01`) matches the selected profile, so passing "either" is a failure.
- Do not extrapolate from bit 0 to RMB1-7/BBR1-7 as "tested"; label as inferred.
- Decimal-mode cycle penalty is not covered by lynx-tests (values only); mark as unverified until measured. `[INF]`

**Warning signs:** a dependency on a "65C02" crate/library; WAI/STP implemented; `cpu` suite passes but profile assertion is absent; per-opcode cycle tables copied from WDC datasheets.

**Phase to address:** CPU (profile table, vectors), FND (profile assertion in the runner).

---

### P7: Timing model: fixed instruction counts, wrong unit, instruction-granular scheduling

**What goes wrong:** A fixed instructions-per-frame loop, or a CPU-cycle unit with after-the-fact "stall cycles", cannot later express page mode, Suzy bus latency, display DMA preemption or Mikey register waits, forcing a core rewrite after tests already encode the wrong timing.

**Facts `[WEB]`/`[SRC]`:**
- System clock 16 MHz (1 µs = 16 ticks). Normal RAM/ROM access 5 ticks; page-mode opcode read 4 (Epyx); measurement shows *instruction-stream bytes* including operands benefit, with a 16-byte boundary where the fetch is normal; data reads, writes, stack, indirect pointers and interrupts close the stream; display DMA/refresh closes the physical row. `[MNT: lynx-tests guide]`
- MAPCTL bit 7 *set* disables sequential cycles. Lynx II read-back returns `(written | $70) ^ $80`, so read-modify-write toggles page mode; Lynx I returns what was written. `[MNT]`
- Epyx: Suzy read 9-15 ticks, Suzy write 5 (blind), cart read 15, Mikey audio DPRAM 5-20, palette DPRAM 5, Suzy bus request to grant up to 40 ticks (2.5 µs). `[WEB]`
- `timers2` tests 7-9 measure timer-register reads/writes and generic Mikey access timing: access cost is not a flat 5 ticks. `[SRC]`
- Standard frame: Timer 0 = 159 µs lines (2544 ticks), Timer 2 = 105 lines, 102 visible + 3 VBlank, 267,120 ticks per frame, about 59.90 Hz; ten 8-byte display-DMA fetches per line, 192 ticks apart; video has the highest bus priority (Video, Refresh, CPU, Suzy). `[MNT/WEB]`
- Epyx: "CPU cycles should not be used as timing for small software timers" and video/refresh accesses can pause the CPU. `[WEB]`

**How to avoid [REC]:**
- Use absolute guest time in 16 MHz ticks (`u64`) as the single time base; derive µs, line and frame events from it.
- Model the CPU at bus-cycle granularity (each bus cycle is a scheduling point with a start tick and duration), even if the first implementation executes whole instructions. If the first version accumulates per-instruction costs, record the exact interface that would be replaced and add a test that fails if a Mikey register read lands mid-instruction at the wrong tick.
- Keep an explicit "bus owner" state (CPU, Suzy, video, refresh, idle/asleep), with CPU-visible stalls produced by arbitration, not added as a fudge.
- Do not make "fast mode" a second renderer with different results; Gearlynx itself keeps a separate fast renderer whose delays are approximate and "must not be used to derive real-hardware ... results". `[SRC]`
- Add a regression that runs `page-mode`, `timers2` and `sdoneack` before any optimisation.

**Warning signs:** `for (i = 0; i < N; i++) step_instruction()` per frame; stall counters added to a "cycles" variable; Timer 6 results in `page-mode` off by constant factors; a single `RAM_ACCESS_TICKS = 5` constant.

**Phase to address:** CPU (before video/Suzy), FND (design note + tests).

---

### P8: Timer, interrupt-acknowledge and chaining semantics

**What goes wrong:** Naive "decrement on tick, reload at zero" timers pass simple tests and fail linking, DONE, IRQ and ordering tests; interrupt acknowledge and sleep interact wrongly.

**Facts (lynx-tests guide `[MNT]`, Epyx `[WEB]`, cross-checked):**
- A count of `N` needs `N+1` source clocks; the *first* period takes between `N` and `N+1` because of undefined source phase. The IRQ is requested when a timer *at zero attempts the next borrow*, not when it first reaches zero.
- Linked timers (selector 7) consume the fixed predecessor's borrow; link graph: T0→T2→T4; T1→T3→T5→T7→A0→A1→A2→A3→T1. T4 has no normal IRQ (UART bit 4). T6 is isolated.
- DONE is sticky and gates counting of *non-reloading* timers: if DONE is set the timer does not count (Epyx hardware bug item 11). `RESET_DONE` (CTLA bit 6) is a *level*, not a pulse, and leaving it high can generate continuous IRQ (Epyx bug item 12).
- DONE, `INTSET/INTRST` pending, and CTLA interrupt-enable are independent. Pending bits coalesce. UART IRQ is level-sensitive.
- Read-modify-write on CTLB can inject a clock (bit 1 = borrow-in).
- Enable order matters: `timers2` T3>T6 vs T6>T3 tests check that the timer enabled first wins the shared 4 µs source; a tie-break rule is part of the hardware behaviour. `[SRC]`
- Any enabled pending Mikey interrupt prevents `CPUSLEEP` from sleeping even with the CPU I flag set; Epyx lists stray interrupt bits as a cause of stalled sprites. `[WEB]`
- A timer interrupt requests the bus and may wait up to Suzy's latency; entry completes at the end of the current CPU cycle. `[WEB]`

**How to avoid:** implement timers as a shared-prescaler device with explicit phase and per-timer enable ordering; model DONE, borrow-out, pending and enable separately; write a table-driven unit suite from the guide's rules plus `timers`, `timers2`, `audio2` link tests; include negative controls (flip one rule, require a named test to fail).

**Warning signs:** a single `tick_timers()` loop with timers iterated 0..7; DONE treated as an interrupt flag; one boolean per timer for "fired".

**Phase to address:** CPU (timers/IRQ), AUD (timer-audio links).

---

### P9: Suzy bus ownership, CPU sleep and SDONEACK

**What goes wrong:** Sprite drawing is modelled as an untimed function call that completes before the next CPU instruction, or sleep/wake is implemented as a boolean; games that rely on `CPUSLEEP` and Suzy completion hang or run fast, and `sdoneack` fails in ten ways.

**Facts:**
- Epyx: "Sleep is broken in Mikey. The CPU stays asleep only while Suzy is using the bus"; an enabled interrupt anywhere prevents sleep; "SDONEACK must be written before the sprite engine runs, including the first time"; any Suzy operation that completes without a following SDONEACK stops the CPU from sleeping. `[WEB]`
- Epyx's earlier launch list places the acknowledge *after* `SPRGO`; the later "Common Errors" section requires it *before*; the lynx-tests guide recommends the conservative order. `[MNT]` The spec contradicts itself.
- `sdoneack` pins ten behaviours: wake condition is sticky, `$A5` acks like `0`, IRQ wake without Suzy done must not create a Suzy-done condition, idle `CPUSLEEP` returns immediately, `SPRGO` with `SUZYBUSEN` clear still sets busy but makes no progress until enabled, removing bus permission mid-run prevents sleep. `[SRC]`
- Suzy releases the bus only at certain cycles and a returning grant from video/refresh looks like Suzy "done" (Epyx lists a grant-edge ambiguity). `[WEB]`
- Do not touch Suzy for 12 ticks after a cart write; back-to-back Suzy accesses can "break" Suzy. `[WEB]` Hardware outcome is undefined.

**How to avoid:** model Suzy as an asynchronous bus master with its own state machine and cycle budget; make CPU sleep a function of (pending IRQ, Suzy busy, bus ownership, Suzy-done latch). For software violations the hardware leaves undefined (back-to-back access, SCB access while running, cart-write window), pick a deterministic, documented fallback, raise a counted *diagnostic event*, and do not claim hardware accuracy for it.

**Warning signs:** `draw_sprites()` called synchronously from the `SPRGO` write; a single `suzy_busy` bool; sleep implemented as "skip until frame end".

**Phase to address:** SUZY (state machine), CPU (arbitration hooks).

---

### P10: MAPCTL overlays and memory-map assumptions

**What goes wrong:** Mapping is implemented as a flat 64 KB array with overlay bits applied to *all* masters, or Lynx II MAPCTL read-back is assumed to equal the written value.

**Facts:** MAPCTL (`$FFF9`): bit 0 Suzy space, bit 1 Mikey space, bit 2 ROM space `$FE00-$FFF7`, bit 3 vector space `$FFFA-$FFFF`, bit 7 sequential-disable. A set bit maps RAM for *CPU* accesses only; video/refresh and the sprite engine see all 64 KB as RAM. `$FFF8` and `$FFF9` cannot be overlaid; `$FFF8` is RAM. `[WEB]` Lynx I/II read-back differs (P7). `[MNT]` cc65's startup writes `$0C` (ROM and vectors overlaid by RAM) before anything else, and the first stage writes `stz MAPCTL`. `[SRC]` Epyx says SCBs and sprite data cannot reside in Mikey ROM. `[WEB]` Multiply-mapped addresses (`$FC40-$FC6F` aliases of Suzy sprite registers) exist; `memio` Test 3 checks them while "avoiding mirror writes that invoke math commands". `[SRC]`

**How to avoid:** separate CPU view from DMA/Suzy view; keep MAPCTL read path model-dependent; include unreadable/write-only/unallocated address behaviour in a table (Epyx lists write-only, read-only, not-implemented and unallocated ranges) rather than defaulting to open bus or zero without a label.

**Warning signs:** one `memory[65536]`; a `read_mapctl()` returning the last write; a Suzy DMA path that goes through the CPU overlay function.

**Phase to address:** CPU (bus), SUZY (DMA view).

---

### P11: Reset and power-on nondeterminism treated as a bug to hide

**What goes wrong:** Hardware-undefined initial state (RAM contents, slow-clock phases, display position) is filled with host randomness or whatever the allocator returns, making tests flaky, or filled with zeros, masking guest bugs that read uninitialised memory.

**Facts:** Epyx: slow clocks have no deterministic phase relationship at reset; `SERDAT` reset undefined, `DISPADR` reset undefined, `PBKUP` reset undefined; reset state tables are in an appendix. `[WEB]`

**How to avoid:** a named `PowerOnState` (RAM fill pattern or seed, timer prescaler phase, display counters) recorded in the profile and in every result manifest; run key suites under two or three declared states and require agreement where hardware is supposed to be state-independent. Log reads of "undefined" registers as diagnostic events.

**Warning signs:** `calloc`/`memset(0)` as the implicit power-on; `rand()` or time-seeded RAM; tests that pass only with the all-zero state.

**Phase to address:** CPU, FND.

---

### Video, sprites, math, audio, serial

### P12: Display: constant-size/rate assumptions, DMA, palette ordering

**What goes wrong:** The output contract is "160×102 at 60 Hz RGB", so games that retime the display produce wrong frame counts; palette registers are decoded in the wrong order; DMA is applied once per frame instead of per line.

**Facts:**
- 160 pens × 102 visible lines in a 105-line frame; refresh derives from Timer 0/Timer 2/PBKUP; the suite includes an interactive calibration utility (T0 backup 0-255, PBKUP, T2 backup). `[SRC]`
- Line = 2544 ticks (about 39 µs non-active + fixed 1920-tick active transfer); ten DMA fetches per line, each 8 bytes (16 pens); visible line 0 has an extra one-off fetch; display address low two bits ignored (`DISPADR` must be 4-byte aligned); latch of `DISPADR` occurs in the third VBlank line. `[MNT]`
- DISPCTL: bit 0 DMA enable, bit 1 flip, bit 2 four-bit mode, bit 3 colour; low nibble must-be-0 bits; write-only. `[WEB]`
- Palette: 16 pen entries of 12 bits; registers are 32 bytes at `$FDA0-$FDBF` (cc65 clears 32 bytes from `GCOLMAP`); `memio` Test 1 exercises "blue/red byte width, green nibble width, incrementing, and wraparound" `[SRC]`. [INF] There are at least three orderings in play: register layout (green byte, blue/red byte), the LCD latch order (R, G, B per vault notes `[WEB]`), and the host pixel format. Keep them separate in the contract.
- The `.lnx` rotation byte exists (cc65 header: 1 = left, 2 = right) `[SRC]`; rotation is host presentation, not machine output.
- Epyx text uses "triad" loosely; the lynx-tests video guide uses "480 physical triads" for the LCD line. [INF] Avoid the word in contracts; say 160 pens/line, 12-bit colour.

**How to avoid:** define raw video output as (line index, pen index data, palette snapshot, timing metadata) or frame-end events with the tick range they cover; never as "a 60 Hz frame". Verify palette packing with `memio` and a tiny original test that writes distinct values to all 32 bytes and reads back through the LCD path.

**Warning signs:** a hard-coded `FRAME_TICKS`; `present_frame()` driven by a host timer; palette decoded as `0x0RGB`.

**Phase to address:** VID.

---

### P13: Sprite engine, collision buffer and documented hardware bugs

**What goes wrong:** A tidy "draw sprite" implementation follows the *documentation* (including corrected-in-spec errors) and diverges from silicon; games that depend on quirks break; or the emulator "fixes" bugs and breaks games that work around them.

**Facts (Epyx pages `[WEB]`, test coverage `[SRC]`):**
- Pitfalls page (lynx3): do not read/write Suzy SCB registers while the sprite engine runs (check SPRSYS); back-to-back Suzy accesses will "probably break Suzy"; for 12 ticks after a cart write do not access Suzy; chained assignment `A=B=C=0` can compile to read-after-write that breaks the hardware. Hardware bug: pen-index palette loader ends 2 bytes early; if a palette starts at `xxFA` the page break is ignored and pens C-F keep the previous SCB's values.
- Known variances (lynx10): signed multiply treats `$8000` as positive; signed multiply treats 0 as negative (delayed effect); remainder mishandles upper bit; auto-clear of SCB word upper byte leaves sign flag; shadow attribute polarity inverted; SCB NEXT zero-detect checks only the upper byte (SCBs cannot be in page 0, with an "approved exemption" allowing the low byte to carry other data); data-packet header `00000` has an unexplained bug.
- Sprite page: 8 sprite types; pen E is non-collideable (a bug) which affects background clears and XOR sprites; data-packet bug requires a zero pad byte when the last meaningful bit lands on bit 0 (about 1/8 of lines); `SPRCTL1` bit 6 sizing algorithm "algo 3 is broke"; stretch values above 128 wrap and reduce size; horizontal size limit 254 source bytes; literal sprites have no pixel count and do not recognise `00000`; direct CPU writes to the video buffer do not touch the collision buffer; SCBs and sprite data cannot reside in ROM.
- lynx-tests covers literal/packed decode and timing, alignment/clipping/super-clip, all 8 operation types with CRCs over video and collision buffers, packed RLE and linked SCBs, zoom/stretch/tilt. `[SRC]` It does **not** list the palette-loader page-break bug, `SPRCTL1` algorithm 3, or the SCB-in-page-0 case. [INF from README inventory]

**How to avoid:** implement sprite behaviour from the measured suites first, then the spec; label each quirk "documented hardware bug" in code and tests; do not invent outcomes for software-violates-rule cases (P9). Keep a quirk ledger with source, test that covers it, and whether any commercial title is known to depend on it (do not copy lists from other emulators).

**Warning signs:** a clean-room sprite routine that passes sprites3 CRCs but fails sprites4 timing; "fixed" shadow polarity; no test for page-break at `xxFA`.

**Phase to address:** SUZY.

---

### P14: Math unit sign, overflow and write-order quirks

**What goes wrong:** Multiply/divide is implemented with host arithmetic and passes simple cases; signed edge cases, retained sign latch, accumulator overflow/status clear and divide-by-zero diverge.

**Facts:** `math` suite: simple/accumulated multiply (near rollover, overflow/carry/unsafe/status-clear), signed multiply followed by a partial operand rewrite "to probe the retained CD sign latch" (added 2026-04-14 as "signed multiply write order test"), `$8000` bug, divide, no-remainder divide, divide by zero, and Timer 6 latency measurement. `[SRC]` Epyx lists several sign errors (P13). `SPRSYS` bit 7 = signed, bit 6 = accumulate; read bit 7 = math in process, bit 6 = overflow (multiply) or divide-by-zero, bit 5 = last carry. `[WEB]` Math registers are written in a specific order and are "unsafe" while running; mirror writes at `$FC40-$FC6F` can start math. `[SRC/WEB]` The suite's `math` expectations are `FF/00` flag pairs, so they confirm the outcome, not intermediate values. `[SRC]`

**How to avoid:** make the unit a state machine with a completion time, retained sign state and write-order sensitivity; derive tests from the suite plus original vectors for combinations the suite does not cover; require the mirror-address negative control.

**Phase to address:** SUZY.

---

### P15: Audio: LFSR/integrator, linking, coverage gap, float resampling inside the core

**What goes wrong:** Audio is produced as host-rate PCM with floating-point filters inside the core, so results are not bit-reproducible, and the suite's single-channel tests give false confidence for channels 1-3.

**Facts:** Four channels built like timers; audio can clock/borrow-link into Timer 1 and Timer 7 into Audio 0 (link ring). `[WEB/MNT]` Audio CTLB upper nibble holds upper LFSR bits (different from timers). `[MNT]` Epyx lists "The lower nybble of the audio out byte is processed incorrectly." `[WEB]` Audio DPRAM access takes 5-20 ticks. `[WEB]` `MSTEREO` bits mute channel-to-ear connections (on/off, not attenuation) per the Epyx register page. `[WEB]` `audio` tests exercise **Channel 0 only** (CTLB, one-shot, integrate/LFSR, max length, clipping, 4096-borrow long test); `audio2` tests linking and hot-switching. `[SRC]` Gearlynx offers a configurable low-pass filter to mimic the hardware. `[WEB]` I did not locate an Epyx audio-register page with LFSR tap layout; treat details beyond the above as unsourced.

**How to avoid [REC]:** the core emits *events* (channel, tick, integrator/sample value) or integer per-channel/mixed samples at a defined tick rate; any filtering, resampling or float conversion lives outside the core and is optional. Add original tests for channels 1-3 and for stereo mute bits; mark lower-nibble behaviour "documented bug, verify".

**Warning signs:** `float` in the core audio path; resampler seeded or timed by the host; audio tests only on channel 0.

**Phase to address:** AUD, HOST (resampling boundary).

---

### P16: UART and ComLynx modelled as byte queues or host networking

**What goes wrong:** TX bytes are delivered to RX through a FIFO or a socket; echo, timing, overrun and level-sensitive IRQ behaviour is wrong, and a later two-instance link inherits a nondeterministic transport.

**Facts:** One open-collector wire shared by TX and RX; every console hears its own echo; the single-console `uart3` tests show the echo arrives about one bit later than frame length and one bit before TXEMPTY, bursts come back one byte short with overrun, only one byte can be read after a slow reader burst. `[SRC]` Frame is fixed 11 bits with a 9th/parity bit and the parity calculation includes the parity bit; IRQs are level-sensitive; power-up drives TTL high until `TXOPEN` is set. `[WEB/MNT]` `uart4` needs two physical consoles and a cable; `wait_for_peer` has no meaning on one emulated machine. `[SRC]`

**How to avoid:** model the wire as a device inside the scheduler; support single-console loopback first (`uart`, `uart2`, `uart3`); keep multi-instance in a later milestone as N machines in one deterministic scheduler, not IPC between processes. Gearlynx links "up to eight emulator instances on the same machine" which is a host-feature design. `[WEB]`

**Phase to address:** CPU (basic UART), later PROD milestone for linking.

---

### Cartridge

### P17: Treating the cartridge as a flat byte array

**What goes wrong:** `cart[offset]` indexing works for the first read, then the cc65 loader, block selection, bank 1/AUDIN banking and EEPROM wiring cannot work.

**Facts:** Cart address comes from an 8-bit shift register (upper bits) plus a counter (lower bits); Mikey drives `CartAddressData` and `CartAddressStrobe`, strobe high holds the counter in reset, the counter increments on the trailing edge of a read; max 19 address bits; read takes 15 ticks; writes are blind and self-timed (562.5 ns) with a 12-tick no-Suzy-access rule. Epyx quotes an 11-bit counter; a hobbyist hardware note quotes a "12-bit counter". `[WEB]` (discrepancy; resolve with a primary schematic or measurement). cc65's loader reads block by block: it sets block via `IODAT` bit 1 and toggles `SYSCTL1`, then counts `_FileBlockByte` against `__BANK0BLOCKSIZE__` (default `$0400`); it does not rely on the hardware to advance blocks. `[SRC]` `CS_0n` strobes on `$FCB2` access and `CS_1n` on `$FCB3`; AUDIN (`$FD8A.4`/`$FD8B.4`) is a GPIO used as banking/write-enable/EEPROM data. `[WEB]`

**How to avoid:** implement the cart device at the register/pin level (shift register, strobe, counter, per-bank strobes, AUDIN, power switch bit), with the `.lnx` payload as backing storage and block-size metadata as parameters; test with the cc65 loader path and an original shift-register test.

**Warning signs:** a `CartRead(addr)` API; loader HLE that bypasses `RCART0`; no concept of "current block".

**Phase to address:** BOOT (cart).

---

### P18: `.lnx` header dialects, EEPROM type and persistence identity

**What goes wrong:** Header fields are mis-parsed or trusted, EEPROM type comes from a hash database, and saves attach to the wrong content after a header edit.

**Facts:** cc65's header generator defines 64 bytes: `LYNX` magic, bank 0 and bank 1 page sizes (words), version `1`, 32-byte name, 16-byte manufacturer, rotation byte (1 = left, 2 = right), five spare bytes. `[SRC: exehdr.s]` The community later used spare bytes for audio-banking and EEPROM type (several proposals, inconsistent byte layouts), and Gearlynx documents "LNX and LNX2 headers", headerless carts, BS93 executables, an EEPROM override (93C46 to 93C86, 8-bit or 16-bit) and "Auto uses the cartridge header or game database", plus SD-cart write modes and persistent cartridge RAM in LNX2. `[WEB][FORUM]` Byte offsets for audio/EEPROM bytes and LNX2 semantics are not confirmed from a primary specification. The cc65 EEPROM drivers show a 93C46 wired with CS on cart pin 18 (A7), CLK on pin 11 (A1), DI/DO shared on AUDIN, clocked by `RCART0` accesses and CS via `SYSCTL1`; 46/66/86 drivers exist. `[SRC]` MAME has no EEPROM support. `[WEB]` The `.lyx` extension is a headerless dump; Handy rejects it. `[FORUM]`

**How to avoid:**
- Parse by a named *container profile*; refuse unknown versions rather than guess; surface rotation/EEPROM metadata as *advice*, not machine behaviour.
- Never ship a commercial hash database. EEPROM type is explicit configuration; ambiguity is an error with a clear message.
- Persistent identity = SHA-256 over the payload bytes under a defined normalisation (excluding mutable header fields) + explicit hardware config (EEPROM type, size, organisation); a header edit must not orphan a save, and an EEPROM type change must not silently reuse bytes. Persistence policy lives outside the core; the core exposes load/export of the 93Cx6 array.
- Defer EEPROM until after the first cart slice; validate with cc65's EEPROM routines compiled into an original diagnostic.

**Warning signs:** `.sav` named by file name or whole-file hash; header "EEPROM byte" trusted without a test; a game DB checked into the repo.

**Phase to address:** BOOT (cart/EEPROM), HOST (persistence boundary).

---

### Verification

### P19: Same-core hash, circular oracles and unrecorded measurement conditions

**What goes wrong:** "Golden" frame/audio/RAM hashes from the emulator itself become the oracle; or lynx-tests passing in Gearlynx is quoted as independent confirmation.

**Facts:** DNA and AGENTS already say a same-core hash is repeatability, not accuracy. Specific new risks:
- The suite author wrote the emulator that "passes all tests" and the guides describe that emulator's model as "observations from original hardware". `[WEB][SRC]` Not independent.
- The guides state measurement caveats: "The hardware model was not recorded", "one attempt per case", "no complete framebuffer/collision oracle", "early single-attempt source page", "approximation". `[SRC]`
- Expected results are sometimes ranges or multi-valued (`EXPECT(0x74, 0x75)` in `page-mode`; Lynx I/II alternatives in `cpu`). `[SRC]`
- Gearlynx's MCP page makes no determinism claim and warns multi-request workflows are not atomic. `[WEB]` It runs in real time when unfocused, so it is a behaviour cross-check, not a deterministic reference runner.
- MAME's own source says timings are not verified, vblank "not accurate", CPU lacks BBR/BBS, Lynx II ROM has no dump, EEPROM not implemented. `[WEB]`

**How to avoid:** each evidence record carries: suite, commit, test id, model(s) claimed, hardware model recorded (or "unrecorded"), measurement repeats, and whether the oracle is spec, hardware-informed, independent emulator or self-hash. The roadmap must not promote an unrecorded or single-attempt measurement to "hardware-verified". A self-hash can gate regression only, and its manifest says so.

**Warning signs:** "verified against hardware" in a status line for a test that records no model; golden updates by re-running the same binary.

**Phase to address:** FND (schema), CI (gates), every phase.

---

### P20: Result capture traps: zero is also "pass", missing sentinels, auto-detect, OCR, duplicated tables, inventory drift

**What goes wrong:** The harness reads `g_results` after a timeout, sees zeros, and records PASS for tests that never ran; or it OCRs the 160×102 screen; or its copy of a suite's expected table silently drifts.

**Facts `[SRC]`:**
- `cpu`: error codes `EXPECT(0x00)` for Tests 1, 2, 8 and `uart4` expects `0x00` for most entries; BSS starts zero; tests begin with `stz`, so an un-run suite reads as PASS in several slots.
- Completion is not signalled by a result byte; `main` runs tests, then initialises TGI, paints results, then loops forever (`loop_forever`). A harness needs the PC reaching a symbol from the `.sym` file, or a progress byte (`cpu` Test 6 has one; most do not).
- Pass/fail logic lives in two places: ROM self-judging (`timers2`: `g_results[i]==0` PASS) and host C tables (`cpu`, `math`, `page-mode`: `k_expected_results`, `k_test_offsets`, `k_test_counts`); the `cpu` Test 7 table has two valid profiles. A harness must re-implement tables per pinned commit.
- Inventory: 18 numbered suites and 127 numbered tests per README and `TEST_COUNT` (122 single-console; `uart4` has 5 that need two consoles); `refresh-rate` is interactive and has no pass/fail; `build-all.sh` lists 18 directories while CI builds every directory with a Makefile (19); `shared/` has none. A README that says "Test 7" and the source may differ after a bump.
- The on-screen text is TGI-drawn at a fixed font; OCR is unnecessary and fragile.

**How to avoid [REC]:** capture = (a) symbol-resolved `g_results` bytes, (b) a completion sentinel defined by "PC reached symbol X after at least the expected guest-time minimum", (c) a hard guest-time bound, (d) a pinned per-suite expectation file generated from the pinned source (script extracts the `main.c` tables) with a count check against README/`TEST_COUNT`, (e) a negative control that corrupts one expected byte and requires the named test to fail, and (f) a "no-run" control (hang the CPU before `run_tests`) that must fail every suite. Optional screenshot is a secondary human aid.

**Warning signs:** a suite marked PASS in less than its minimum guest time; a harness that only reads bytes; the manifest lacks the number of tests expected; `uart4` reported as "skipped" without being listed.

**Phase to address:** FND (harness), HOST (runner), CI.

---

### P21: Skipped/missing/stale evidence, absent negative controls, timeouts masking hangs

**What goes wrong:** A gate stays green when a suite is not built, not run, built from stale artifacts, or hangs until a wall-clock timeout that is treated as success or as an unexplained retry.

**How to avoid [REC] (consistent with AGENTS.md):**
- Required-suite list with expected counts; missing, skipped, unsupported (`uart4`, `refresh-rate`) and stale (fixture SHA differs from manifest, or produced by a different cc65 commit) are distinct red/yellow states printed in the summary.
- Timeouts are *guest-time* bounds first, wall-clock second, and a timeout is a failure with the PC and last Timer 6/IRQ state recorded.
- Each gate has at least one named-fault control that is run in CI on a schedule or on gate change: wrong CPU profile, page mode off, timer DONE gating removed, shadow polarity flipped, cart pointer reset.
- Fixtures are rebuilt from pinned sources in a clean environment before they are trusted; compare SHA-256 to the manifest.

**Phase to address:** FND, CI.

---

### P22: Determinism leaks

**What goes wrong:** Observable results depend on host clock, uninitialised RAM, thread scheduling, locale, hash-map iteration, floating point, or pointer values.

**Lynx-specific hot spots `[INF]`:** timer prescaler phase at start (P11); RAM power-on pattern; audio filters and resampling (P15); frame pacing from a host timer; LCD/DMA ordering if per-line work is dispatched through a thread pool; ComLynx between processes (P16); `CPUSLEEP` implemented as host sleep; save-state or trace code that serialises struct padding; debugger/MCP server running in the same process (Gearlynx notes the emulator keeps running when the window loses focus while MCP is active `[WEB]`).

**How to avoid:** the core takes (profile, content bytes, start state seed, input schedule, guest-tick budget) and returns (state digest, raw outputs, diagnostics); no clock, file, thread, network or float in core logic; golden test that runs the same input twice in one process and in two processes, on two host architectures when the matrix is added; "forbidden imports" lint for the core module.

**Phase to address:** HOST (API), FND (lint/tests), CI.

---

### Performance, CI and product

### P23: Premature optimisation and a scheduler that blocks later accuracy

**What goes wrong:** A fast path (bulk sprite draw, batched timers, idle-skipping) is added before correctness baselines; later accuracy work conflicts with it and results diverge between "fast" and "accurate" modes.

**Facts:** A 16 MHz tick base gives about 16 million ticks per emulated second; native code is fine, scripting hosts are not [INF]. The frame count is 267,120 ticks at standard timing (P7). Gearlynx ships both an accurate renderer and an optional fast renderer that "must not be used to derive real-hardware optimization advice or benchmark results". `[SRC]`

**How to avoid [REC]:** one canonical implementation; optimisations must keep the digest identical across the correctness workloads; keep a short list of named workloads (the pinned suites, a cc65 homebrew-style scene, a worst-case sprite load, a DMA-heavy scene) with correct outputs held constant before any benchmark is quoted; separate emulator throughput, end-to-end latency and CI wall time.

**Phase to address:** CI (benchmarks after correctness), CPU/SUZY (scheduler shape).

---

### P24: Broad CI matrices and unpinned toolchains too early

**What goes wrong:** Multiple OS/arch matrices, a long compatibility loop and per-run `git clone cc65 && make` slow the fast gate and add noise before there is a distinct failure class to catch.

**Facts:** lynx-tests CI clones and builds cc65 master every run (unpinned). `[SRC]` The suite is 18 suites; building from source each run adds toolchain time and drift.

**How to avoid [REC]:** the first CI gate is one local-equals-CI command: pinned cc65 build (cache keyed by commit and checked by checksum), build the pinned suites into a content-addressed fixtures dir, run the harness with negative controls, run determinism twice. Add a second OS/arch only when the first digest mismatch class is demonstrated. Measure queue/setup/build/test time before adding caches or concurrency (AGENTS.md).

**Phase to address:** CI.

---

### P25: Becoming research-only

**What goes wrong:** The project accumulates hardware notes, manifests and a perfect harness for an emulator that never shows a frame, and loses momentum.

**How to avoid [REC]:** schedule a visible scenario early: a tiny original homebrew-style program (cc65-built, source in repo) that draws a sprite scene and plays a tone, run via the RAM-load tier first, visible through a minimal frame-dump viewer. It is an evidence-light "playable" slice, labelled as such. Keep commercial-game progress out of the early plan (rights, and no clean path).

**Phase to address:** PROD (roadmap), HOST.

---

### P26: Promising Lynx I+II, ComLynx, EEPROM or accessories early

**What goes wrong:** Scope balloons because other emulators list those features (Gearlynx lists user-selectable Lynx I/II, eight-instance ComLynx, EEPROM and bank switching `[WEB]`).

**Facts to hold the line:** only a handful of I/II differences are evidenced so far (bit ops, MAPCTL read-back, AUDIN reset level on some revisions `[WEB]`, and a likely stereo difference noted in MAME comments `[WEB]`), all from non-independent sources; a second model needs its own evidence tier. `uart4` needs hardware. EEPROM needs a cart device (P17-P18).

**How to avoid [REC]:** baseline = **Lynx II profile** (the model with the logic-analyzer measurements cited by the suite, the model Gearlynx defaults to when unspecified, and a CPU that is a superset for the bit-op difference), with Lynx I defined as a *named variant* only when its differing behaviours are enumerated and tested. Single-console UART, no EEPROM, no ComLynx link in the first milestone. This is a recommendation resting on MEDIUM/LOW evidence; the owner's model choice is explicit in PROJECT.md as open.

**Phase to address:** PROD.

---

### P27: Compatibility claims without a named profile

**What goes wrong:** "Runs Lynx games" appears without model, start tier, firmware identity, container profile and evidence tier.

**How to avoid [REC]:** a claim template (profile id, start tier, cart profile, evidence tier(s), suite+commit, date); tooling refuses to print a claim without all fields. Game progress is a tier below hardware-informed tests and must say which BIOS/start it used.

**Phase to address:** FND (template), PROD.

---

### P28: Coupling to LibFuturo before a core exists

**What goes wrong:** Early API design bends to an adapter contract that cannot be checked from outside the owner's environment, or Lynx-specific needs (variable frame timing, per-line palette, event audio) are forced into an unsuitable shape.

**Facts:** I could not find LibFuturo in public sources (web search returned only libretro). `[WEB]` The seed itself says it is experimental, with no Lynx profile in the reviewed contract. `[SRC: seed/DNA]`

**How to avoid [REC]:** design the core API from Lynx needs (P12, P15), then write a thin adapter/consumer to test fit; do not freeze ABI or profile names until a real consumer round-trips.

**Phase to address:** HOST.

---

## 3. Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Instruction-granular CPU stepping with per-instruction stall accumulation | Fast to write | Cannot express mid-instruction Mikey reads, bus arbitration; rewrite of CPU/bus/Suzy interface | Only as a labelled first cut with the bus-cycle interface designed and a failing test documenting the gap |
| RAM-load (BLL) as the only start path | Unblocks tests quickly | Hides ROM post-state, cart read pointer, timer phase; "boot" claims become false | Acceptable for component tests; never for boot/cart claims |
| Reusing a W65C02/65C02 library core | Saves CPU work | WAI/STP, reserved opcode lengths/timing, IRQ blocking, no page-mode/bus-cycle hooks | Never as-is; use only as a test-vector oracle after profile filtering |
| Golden hashes from own core as the oracle | Quick regression | Locks in bugs; indistinguishable from accuracy | Regression gate only, labelled self-hash |
| Copying expected tables by hand from `main.c` | Fast harness | Silent drift after suite bump | Never; generate from pinned source with a count check |
| Float audio filter/resampler in core | Pleasant sound | Non-reproducible digests | Never in core; allowed in host layer |
| Game database for EEPROM type | Works for dumps with bad headers | Rights risk, unreviewable data, hidden behaviour | Never in-repo; explicit config instead |
| Fast sprite renderer with approximate busy delay | Speed | Diverges from timing suites | Only as a separate, flagged mode never used for evidence |
| Unpinned cc65 master in CI | Always compiles | Fixture drift, non-reproducible bytes | Never; pin SHA and build once |

## 4. Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| cc65 toolchain | Using distro/Homebrew `V2.19`; `lynx.inc` lacks `ENABLE_INT`/`ENABLE_COUNT` | Pin a master commit, build from source in a hermetic step, record SHA and flags in the fixture manifest |
| lynx-tests | Tracking `main`; trusting README test numbering | Pin commit/tag, generate expectation files from source, check counts, re-read LICENSE per bump |
| Symbol files | Assuming `.sym` exists for every directory | 19 of 20 directories have `-Ln`; verify per suite and fail if `g_results` is missing |
| Gearlynx/MAME/Holani | Reading source to "see how it does it" | Use only as black-box behaviour cross-check by an isolated investigator |
| Owner firmware | Discovered automatically in home dir or CI secrets | Explicit local-only flag; refuse in CI; record identity only |
| Epyx documentation | Treating it as consistent | Note known contradictions (SDONEACK order, counter width, shadow polarity "NEW truth") and resolve with tests |
| `.lnx` header | Trusting rotation/EEPROM bytes | Container profile; advisory metadata only |

## 5. Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Per-tick dispatch of all devices | Slow in any managed/scripted host; CPU-bound in debug builds | Event queue with next-event times; but keep bus-cycle fidelity | About 16 M ticks per emulated second; slow on first sprite-heavy scenes |
| Per-line video work on every tick | Frame time spikes | Schedule line/DMA events (ten fetches per line, 105 lines) | Always; becomes the dominant cost in sprite+DMA scenes |
| Re-simulating Suzy per pixel for all sprites | Low FPS in sprite-heavy scenes | Cycle-budget FSM with batched row work, digest-identical to slow path | Heavy 4-bpp scaled sprites; measure first |
| Allocation in the hot loop | GC/allocator jitter, nondeterministic perf | Fixed-size state structs | Immediately on desktop, severely on mobile/WASM |
| Optimising before baselines | Silent accuracy regressions | Named workloads with constant output digests | From the first optimisation PR |

## 6. Security and Rights Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| BIOS/commercial ROM in CI cache, artifact, release or trace | Redistribution without permission; owner boundary violated | Fail-closed firmware flag, artifact allow-list, deny-list scan (P3) |
| Firmware-derived frames/audio/memory dumps as goldens | Derivative distribution | Keep local; store only owner-approved digests |
| Hash treated as rights evidence | Wrong legal assumption | Manifest "allowed use" field is separate from checksum |
| GPL source consulted by implementers | Licence contamination | Clean-room rule, separate investigator (P4) |
| `.lnx` parser trusting sizes | Out-of-bounds read/overflow from hostile ROMs | Bounds-check page sizes and payload length; fuzz the container parser in the CI hardening stage |
| EEPROM/save path from header or name | Path traversal or save mixing | Core takes bytes in/out only; host owns paths |
| Third-party GitHub Actions without SHA pins | Supply-chain risk | Pin by SHA; prefer plain shell steps (small dependency surface) |

## 7. UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Fixed 60 Hz frame pacing | Judder/early-late audio when a game changes timers | Pace from emitted frame events and audio buffer, keep core independent |
| Ignoring rotation | Rotated games unplayable | Host presentation setting seeded from container advice |
| Remapping hard-coded to host pads | Orientation confusion with rotated games | Logical buttons (D-pad, A, B, Option 1/2, Pause) with host remap outside the core |
| Silent firmware absence | "Does nothing" screen | Explicit startup-tier message and a documented free path |
| Showing compatibility lists from other emulators | Mismatched expectations | Show claims by named profile/evidence only |

## 8. "Looks Done But Isn't" Checklist

- [ ] **CPU:** `cpu` suite PASS but model assertion absent, or WAI/STP implemented, so verify detected model equals the profile (`FF 00 02 02` Lynx I, `FE 01 01 01` Lynx II) and `$CB`/`$DB` are NOPs.
- [ ] **Timers:** simple one-shot works but DONE gating, RESET_DONE level and enable-order tests not run; verify `timers`, `timers2`, `audio2`.
- [ ] **Page mode:** 4/5 ticks implemented but 16-byte boundary, stream closing and Lynx II MAPCTL read-back missing; verify `page-mode` results `35/41`, `69/81`, `74-75/81` under the pinned commit.
- [ ] **Suzy:** sprites draw but `CPUSLEEP`/SDONEACK, `SUZYBUSEN`-off behaviour and mirror writes unverified; verify all 10 `sdoneack` tests.
- [ ] **Video:** frames appear but no variable-timing event, palette packing or DMA-per-line; verify `refresh-rate` style retimings with an original test.
- [ ] **Cart:** loader runs but cart is a flat array; verify shift-register/strobe/counter behaviour with the cc65 loader and a block-boundary test.
- [ ] **Boot:** "boots" under HLE; verify manifest states start tier and the post-boot state record.
- [ ] **Harness:** all green but no sentinel/no-run control; verify a hung CPU fails every suite.
- [ ] **Inventory:** 127 numbered tests (122 single-console) expected; verify counts, skips and `uart4`/`refresh-rate` listed explicitly as unsupported.
- [ ] **Determinism:** same input twice produces identical digests in one and two processes; no host clock/float/thread in core.
- [ ] **Rights:** no firmware/commercial bytes in repo, caches, artifacts or goldens; fixture manifests complete with notices (MIT, zlib, loader blob, Alpine item).
- [ ] **Claims:** each claim has profile + start tier + cart profile + evidence tier + pinned suite.

## 9. Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Core built on instruction-granular timing (P7) | HIGH | Introduce bus-cycle interface under the CPU; port Mikey access and Suzy arbitration first; keep suites as regression |
| Stock 65C02 core adopted (P6) | MEDIUM | Replace opcode table via profile-driven decode; keep core as vector oracle |
| HLE-derived expectations used as boot evidence (P2) | MEDIUM | Re-label evidence; add post-boot state record; regenerate expectations under the real or clean ROM path |
| BIOS/ROM leaked to cache/artifact (P3) | HIGH | Delete caches/artifacts/releases, rotate any affected tokens, scrub history if committed, add scanners and deny-lists, record the incident and owner notification |
| GPL contamination discovered (P4) | HIGH | Isolate affected files, rewrite from clean spec by an uncontaminated implementer, review history, owner decides licensing |
| Fixture drift from unpinned toolchain (P5/P24) | LOW | Pin cc65 and suite SHAs, rebuild fixtures, regenerate manifests, rerun gates |
| Zero-state false PASS discovered (P20) | LOW | Add sentinel and no-run control; re-audit prior green runs |
| Save collision from weak identity (P18) | MEDIUM | Introduce content-identity + hardware-config key, migration tool for old saves |
| Research-only drift (P25) | LOW | Ship the original homebrew scene on the RAM-load tier and put it in the fast gate |

## 10. Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| P1 cc65 .lnx without boot ROM | BOOT, FND | Manifest states start tier; test shows RAM-load run does not claim boot |
| P2 HLE hides state | BOOT, CPU | Post-boot state record exists and is compared locally with a real-ROM run |
| P3 BIOS/ROM leakage | FND, CI | CI fails when firmware path/cache/artifact present; scan test with seeded fake 512-byte file |
| P4 GPL contamination | FND, all | Clean-room policy doc; review checklist per PR; no GPL source consulted by implementers |
| P5 lynx-tests licence/provenance | FND | Manifest has MIT + zlib + loader + Alpine notice; pinned SHA |
| P6 CPU profile | CPU, FND | Profile table; model assertion; `$CB/$DB` NOP; vectors filtered per profile |
| P7 timing model | CPU | `page-mode`, `timers2`, `sdoneack` pass before optimisation; bus-cycle design note |
| P8 timers/IRQ | CPU, AUD | `timers`, `timers2`, `audio2` pass; named-fault controls |
| P9 Suzy bus/sleep | SUZY, CPU | 10 `sdoneack` tests; unspecified-behaviour diagnostics |
| P10 MAPCTL/memory map | CPU, SUZY | `memio`, `page-mode`; CPU vs DMA view test |
| P11 power-on state | CPU, FND | `PowerOnState` recorded; suites run under declared states |
| P12 display contract | VID | Contract carries timing metadata; palette original test |
| P13 sprite quirks | SUZY | `sprites1-5` CRCs; quirk ledger with tests |
| P14 math unit | SUZY | `math` suite plus original vectors, mirror negative control |
| P15 audio | AUD, HOST | Integer/event output; channels 1-3 original tests; no float in core |
| P16 UART/ComLynx | CPU, later PROD | `uart`, `uart2`, `uart3`; link milestone separate |
| P17 cart flat array | BOOT | cc65 loader path works through shift register; block-boundary test |
| P18 header/EEPROM/identity | BOOT, HOST | Container profile; identity key spec; EEPROM original test |
| P19 circular/unrecorded oracles | FND, CI | Evidence schema includes model/repeat/independence |
| P20 result capture traps | FND, HOST | Sentinel, no-run control, generated tables, counts |
| P21 skips/stale/controls/timeouts | FND, CI | Distinct states; guest-time bounds; named-fault controls |
| P22 determinism leaks | HOST, FND | Two-run/two-process digest test; forbidden-import lint |
| P23 premature optimisation/scheduler | CI, CPU | Named workloads with constant digests |
| P24 CI breadth/toolchain | CI | One local-equals-CI command; measured times |
| P25 research-only | PROD, HOST | Visible original scenario by an early milestone |
| P26 scope creep (I+II, EEPROM, ComLynx) | PROD | Baseline profile named; second model has its own evidence plan |
| P27 claims without profile | FND, PROD | Claim template enforced by tooling |
| P28 LibFuturo coupling | HOST | Real consumer round-trip before ABI/profile freeze |

**Phases likely to need deeper phase-level research:** BOOT (boot ROM replacement/HLE decrypt, free boot ROM provenance, cart pin-level model, header dialects), CPU (bus-cycle design, Lynx profile table, decimal/NOP cycle counts), SUZY (state machine from sprites1-5, pen-palette bug coverage, math write order), AUD (no sourced register layout yet), VID (palette packing, per-line DMA ordering). Standard-pattern, low research: CI harness and manifests, determinism lint, container parser hardening.

---

## 11. Audit of bootstrap claims (EMULATOR-DNA.md and PROJECT-SEED.md)

Verdict key: **Accurate**, **Accurate but incomplete**, **Inaccurate as stated**, **Ambiguous**, **Unverifiable**.

| # | Claim in bootstrap | Verdict | Evidence (2026-11-09) |
|---|--------------------|---------|------------------------|
| 1 | "cc65's target documentation says 65SC02" | **Inaccurate as stated** | `doc/lynx.html` never names a CPU (searched for 65SC02/65C02). The compiler's target table assigns `lynx` to `CPU_65SC02` (`src/common/target.c:246`). `[SRC]` Say "cc65's compiler target table", and note ca65 defines 65SC02 as no bit-manipulation and no WAI/STP. |
| 2 | "Epyx-era Handy specification uses 65C02 terminology" | Accurate | lynx4: "65C02 core embedded in the Mikey IC". `[WEB]` Generic; does not distinguish bit ops. |
| 3 | "Hardware test suite CPU suite identifies Lynx I/II-specific RMB/SMB/BBx behavior" | Accurate but incomplete | `[SRC]` Only bit-0 forms tested; verdict is by auto-detect (pass on either); file comment calls this "65SC02 Subset" contrary to ca65 naming; WAI/STP-as-NOP and `$x3/$xB` IRQ behaviour also Lynx-specific; MAPCTL read-back also differs by model `[MNT]`. Forum sources disagree about I/II CPUs. |
| 4 | "Most fixed suites run at startup and display status plus diagnostic bytes" | Accurate but incomplete | README says each suite except `refresh-rate` runs once at startup `[SRC]`. All suites also keep `g_results` in RAM with symbol files (19/20 dirs); machine-readable capture is feasible. Pass/fail logic is split ROM/host. |
| 5 | "README distinguishes interactive refresh-rate calibration and two-console uart4 from fixed tests" | Mostly accurate | `refresh-rate` yes; `uart4` is a numbered fixed suite needing a second console and cable `[SRC]`. |
| 6 | "Do not assume the public hardware test ROMs expose machine-readable results" | Outdated by inspection | See #4; resolve via `g_results` + sentinel (P20). |
| 7 | "Gearlynx's README reports ... passing the Lynx hardware tests" | Accurate; weak as evidence | Verbatim "passes all tests from Atari Lynx Hardware Test ROMs". `[WEB]` Same author as the suite; suite churned since (P19). |
| 8 | "Gearlynx usage guide requires a BIOS" | Accurate | README and libretro docs: "The Lynx boot ROM file is required". `[WEB]` |
| 9 | "Gearlynx repository is GPL-3.0" | Accurate | README. `[WEB]` Also Holani GPL-3.0 `[SRC]`, MAME `lynx.cpp` GPL-2.0+ `[WEB]`. |
| 10 | "Physical handheld includes a 512-byte boot ROM" | Accurate | lynx4 and MAME (0x200). `[WEB]` Two of the 512 addresses (`$FFF8`, `$FFF9`) are not ROM-readable. |
| 11 | "MAPCTL changes CPU-visible mappings; does not remove a device's independent access" | Accurate but incomplete | Matches hardware.html `[WEB]`; omits Lynx I/II read-back difference, bit 7 page-mode control, `$FFF8/9` non-overlay. |
| 12 | "Timing depends on page-mode fetches ..., fixed instructions-per-frame unsafe" | Accurate | Epyx and measurements `[WEB][MNT]`. Epyx says page mode applies to opcode reads; measurements say instruction-stream bytes with a 16-byte boundary, so Epyx text is incomplete. |
| 13 | "160×102 display built from RGB triads and a 4096-colour palette" | Ambiguous | 160 pens × 102 visible of 105 lines; 12-bit palette; "triad" is used for 480 per-line LCD units in the video guide `[MNT]`. Avoid the term in contracts. Refresh is programmable. |
| 14 | "Cartridges may include address-counter/shift-register and banked behavior" | Accurate | Epyx lynx7 `[WEB]`; cc65 loader `[SRC]`. Counter width 11 vs 12 bits unresolved. |
| 15 | "cc65 documents `cl65 -t lynx` producing a bootable `.lnx`" | Accurate, misleading | "Bootable" on real hardware means the encrypted loader plus boot ROM; cc65 docs say the loader "does not check the cart or show a title screen". `[SRC]` Not a statement about emulator boot without ROM. |
| 16 | "lynx-tests is MIT-marked" | Accurate | Root `LICENSE`, Ignacio Sanchez Gines, 2025; no per-file notices; builds embed cc65 runtime and a third-party loader; Alpine-derived literal in sprites2. `[SRC]` |
| 17 | "Pin toolchain and suite revisions" | Accurate; underspecified | cc65 must be a master commit (`ENABLE_INT` missing in V2.19); suite had 21 tags since 2025-10 with new suites added in Aug-Sep 2026. `[SRC]` |
| 18 | "Tests developed by analyzing real hardware" | Maintainer report | Several measurements record no hardware model or a single attempt. `[SRC]` |
| 19 | "Atari 1990 report: Lynx sales slow at launch" | Accurate | "As a result of the release of only 12 titles during 1990, and the higher launch price, sales of the Lynx were slow." `[SRC]` Historical context only. |
| 20 | "MAME Lynx driver is a cross-check" | Valid but weak | Own comments: timings unverified, vblank not accurate, no BBR/BBS, Lynx II disabled (no dump), no EEPROM; GPL-2.0+. `[WEB]` |
| 21 | "Plan a clean test path: owner-supplied firmware, independently authored route, or reviewed combination" | Accurate; incomplete | Omits the BLL/RAM-load route and existing free boot ROM / BLL loader (Holani README) as candidates to evaluate; both need provenance/licence review. |
| 22 | "LibFuturo is experimental; reviewed profiles do not include Lynx" | Unverifiable | No public source found; owner-internal context. |
| 23 | Epyx pitfalls page (lynx3) as a source | Valid; contradicts other Epyx pages | Pitfalls vs "Common Errors" disagree on SDONEACK order; shadow-polarity table says "NEW truth". `[WEB]` |
| 24 | Link `docs.libretro.com/library/gearlynx/` as independent BIOS doc | Valid | Documents `lynxboot.img`, 512 bytes, recommended MD5, "Auto" model defaulting to Lynx II. `[WEB]` |

## 12. Gaps and low-confidence items to recheck before they become requirements

- Epyx pages were read through a summarising fetcher; the SDONEACK, MAPCTL, timer and bug lists agree with the independent lynx-tests guides where they overlap (MEDIUM), but exact wording, bit numbers and any items the summariser omitted must be rechecked against the scanned Handy specification PDF hosted at atarilynxdev.net. I did not locate an Epyx audio-register page.
- Boot ROM details (RSA with exponent 3, "multiple of 51 bytes", decrypt to `$0200`, checksum claims) come from forum posts. Treat the cc65 source comment as the only HIGH source (52-byte encrypted loader, plaintext listed). Whether the ROM does more than decrypt is unknown.
- `.lnx` header bytes after rotation (audio-banking, EEPROM type, LNX2) are unconfirmed; Gearlynx's source and docs were deliberately not mined for them (P4). Find a non-GPL primary spec or measure with an original cartridge profile.
- Lynx I versus II differences are incomplete. Only the following have any evidence: bit ops, MAPCTL read-back, AUDIN reset level (hobbyist note), stereo (MAME comment). Hayato/Mikey revision details are unsourced.
- The provenance, licence and fidelity of the "Free Lynx Boot Rom" embedded by Holani and of the AtariAge replacement ROM are unknown.
- Counter width of the cart address counter (11 vs 12 bits) is unresolved.
- Decimal-mode cycle behaviour, exact reserved-opcode cycle counts, and whether RMB1-7/BBR1-7 follow bit 0 are not covered by any source I read.
- Whether the real boot ROM leaves specific RAM/timer/display state that the cc65 loader relies on is not documented in anything I read; a local real-ROM trace (owner authority) is the only way to settle it.
- LibFuturo could not be checked.

## 13. Sources

Primary files read directly (HIGH for what the file says; accessed 2026-11-09):
- `drhelius/lynx-tests` at `a920f01` (shallow clone): `README.md`, `LICENSE`, `build-all.sh`, `.github/workflows/build.yml`, `cpu/main.c`, `cpu/tests.s`, `shared/util.h`, `shared/lynxcc65.mak`, `sprites2/tests.s`, `math/main.c`, `page-mode/main.c`, `timers2/main.c`, `uart4/main.c`, `lynx-page-mode.md`, `lynx-timers.md`, `lynx-video-timing.md`, `lynx-sprite-performance.md`; tags/releases via `git ls-remote` and `gh api`.
- cc65 master `eebf54a`: `cfg/lynx.cfg`, `libsrc/lynx/bootldr.s`, `exehdr.s`, `crt0.s`, `lynx-cart.s`, `eeprom.s`, `asminc/lynx.inc` (compared with tag `V2.19`), `src/common/target.c`, `doc/ca65.sgml` (CPU definitions).
- `SingleStepTests/65x02` repository listing, README, MIT licence.
- `LLeny/holani` README and GitHub licence API (GPL-3.0).
- Atari Corporation 1990 Annual Report PDF text (`atarimania.com`).
- cc65 Lynx doc page: https://cc65.github.io/doc/lynx.html

Web pages read via summarising fetch (MEDIUM when corroborated by the above, otherwise LOW):
- Epyx/Handy specification (monlynx.de): https://www.monlynx.de/lynx/lynx3.html (pitfalls), lynx4 (CPU/ROM), lynx6 (sprite/collision), lynx7 (ROM cart), lynx8 (timers), lynx8a (UART), lynx10 (reset, known variances, exemptions, common errors), lynx11 (bus interplay), hardware.html (register appendix), sprite.html
- Atari Lynx dev documentation index: https://atarilynxdev.net/doku.php/lynx:doc:start
- Atari Lynx Vault hardware notes: https://atarilynxvault.com/pages/atari-lynx-hardware-notes
- Gearlynx README and MCP guide: https://github.com/drhelius/Gearlynx ; Libretro docs: https://docs.libretro.com/library/gearlynx/
- MAME driver: https://github.com/mamedev/mame/blob/master/src/mame/atari/lynx.cpp

Forum and secondary sources (LOW, leads only):
- AtariAge threads on Lynx encryption, replacement boot ROM, CPU variant, LNX header: https://forums.atariage.com/topic/129030-lynx-encryption/ , https://forums.atariage.com/topic/192912-replacement-testers-wanted/ , https://forums.atariage.com/topic/283383-which-cpu-does-it-actually-have/ , https://forums.atariage.com/topic/344827-lnx-rom-format-compatibility/ (these returned HTTP 403 to the fetcher; content known only from search snippets)
- Recalbox wiki on Handy/Mednafen/Holani BIOS notes; RetroPie notes on lr-handy optional BIOS.

Attempts that did not resolve: LibFuturo (no public hits); AtariAge threads (403); `lynxdev.atari.org` redirects to bjoern.spruck.net, not fetched.

---
*Pitfalls research for: Atari Lynx emulator with strong verification and rights hygiene (Lynxplosion)*
*Researched: 2026-11-09*
