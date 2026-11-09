# Project Research Summary

**Project:** Lynxplosion (owner-selected working name; not trademark clearance)
**Domain:** Accurate, deterministic, headless-drivable Atari Lynx emulator core with a C ABI
**Researched:** 2026-11-09
**Confidence:** MEDIUM overall. Desk research only: nothing was built, executed or measured. HIGH on language, build and pin facts read from primary files; MEDIUM on the cc65 RAM-load recipe (not yet built); LOW on any "free" boot ROM.

**Inputs:** `STACK.md`, `FEATURES.md`, `ARCHITECTURE.md`, `PITFALLS.md` (all dated 2026-11-09), read against `PROJECT.md` and `AGENTS.md`.

**Status of this file:** a synthesis for roadmap creation. It makes no owner decision. Items marked **OWNER** are batched for the owner in "Decision ownership". Each researcher's evidence labels are kept as written (see the legend). Where researchers disagree, the disagreement is listed rather than averaged.

---

## Evidence labels

The four researchers used different tags for the same categories. Tags are kept as written in the findings below. Canonical meaning:

| Tag as written | Researcher(s) | Canonical category | Weight for roadmap use |
|---|---|---|---|
| `[SO]` | STACK, FEATURES | Source observation: primary file read directly (cc65, lynx-tests, SingleStepTests, sibling repos) | Strongest available here |
| `[SRC]` | ARCHITECTURE, PITFALLS | Source observation (same meaning as `[SO]`) | Strongest available here |
| `[WEB]` | PITFALLS | Source observation through a summarising fetcher | Verify verbatim before it becomes a requirement |
| `[MR]` | STACK, FEATURES | Maintainer report: a test or emulator author's own claim, not independently reproduced | Medium-low |
| `[MAINT]` | ARCHITECTURE | Maintainer report (same meaning as `[MR]`) | Medium-low |
| `[MNT]` | PITFALLS | Maintainer report (same meaning as `[MR]`) | Medium-low |
| `[IMPL]` | ARCHITECTURE | Other emulator's source behaviour (Gearlynx GPL-3.0; Handy zlib; MAME GPL-2.0+; Holani GPL-3.0) | Behaviour cross-check only. Never hardware evidence. Never copied. |
| `[FORUM]` | PITFALLS | Forum or hobbyist post | Lead only (LOW) |
| `[INF]` | all | Inference from the above | Check before relying on it |
| `[REC]` | all | Recommendation | Not a decision |
| `[UNK]` / `[OPEN]` | FEATURES / ARCHITECTURE | Unknown or open question | Needs a spike or an owner decision |
| **OWNER** | this file | Owner-level decision | Batch to owner; do not lock in phase text |

Evidence tiers from FEATURES, used in claims: **T1** spec or vector expectation; **T2** public hardware-informed test (lynx-tests); **T3** original diagnostic with manifest; **T4** independent emulator comparison (cross-check, not an oracle); **T5** measured physical hardware (owner-gated); **T6** game or homebrew progress; **T7** installed-host behaviour; **R** same-core repeatability (never an accuracy tier).

---

## Executive Summary

Lynxplosion is a greenfield, accuracy-first Atari Lynx core: a deterministic machine behind a C ABI, with every compatibility claim tied to a named profile and to evidence the machine can reproduce. Existing cores (Gearlynx, the Handy/libretro lineage, Mednafen/Beetle, Holani, MAME) synchronise devices at instruction granularity, and most need a boot ROM. The research agrees that this is not sufficient for the accuracy goal: page-mode fetches, display DMA, Suzy bus ownership, timer phase and CPU sleep make fixed instructions-per-frame unsafe. The recommended shape is one canonical 16 MHz master-tick time base; a CPU that makes tick-stamped bus accesses, with devices caught up lazily; a data-driven CPU profile table; the cartridge as its own component with an address port and a data port; Suzy as an explicit bus master; and raw outputs kept separate from host presentation.

The recommended path is evidence first, then visibility. Build the walking-skeleton API and evidence harness. Prove the CPU with per-opcode vectors. Start the machine through a rights-clean direct-load path (startup tier A) with a documented preset. Report first results through a RAM result block written by original diagnostics. Add timers and IRQ, then read the public lynx-tests suites from guest RAM by symbol, pinned to lynx-tests tag 1.4.9 and cc65 commit `eebf54a` built from source. Display, audio, UART, Suzy and the cartridge follow. A visible artifact arrives early: a labelled, evidence-light snapshot in suggested phase 3, a live input-responsive homebrew frame in phase 5, and sound by phase 6.

The main risks are: (1) a cc65-built `.lnx` cannot start in an emulator without the real boot ROM, a replacement, a decrypting HLE, or a RAM-load path, and no rights-clean boot ROM has been validated; (2) the baseline model is unresolved (Lynx I-style or Lynx II first), and the two researchers disagree, which gates every claim; (3) the public suites are single-author, partly self-judging and full of zero-valued results, so harness design carries the evidence; (4) Suzy bus timing and sprite behaviour are the deepest research; (5) rights hygiene: no firmware in the repository or shared CI, and the research itself read GPL-3.0 sources while the project licence is still open.

---

## Key Findings

### Recommended Stack

The stack is C17 with CMake presets and CTest, zero runtime dependencies in the core, a pinned cc65 built from source, and a pinned lynx-tests tag as fixture source. The sibling cores in the owner's portfolio use the same shape `[SO]`, and integer-only hardware needs no floating point `[INF]`. These are routine, reversible defaults, with one caveat about language (see OWNER O8).

| Technology | Pin or version | Purpose | Basis |
|---|---|---|---|
| C17 (`CMAKE_C_EXTENSIONS OFF`, `-pedantic-errors`) | ISO C17 | Core, runner, public header | Sibling cores and LibFuturo are C17 `[SO]`; hardware is integer-only `[INF]`. Rust is the strongest alternative; changing language is OWNER-level `[REC]`. |
| CMake, presets, CTest | floor 3.25 (dev host 4.4.3) | One `cmake --workflow --preset check`; exact test inventory checked by a CMake script | Sibling floor `[SO]`; `[REC]` |
| Ninja | 1.13.2 | Build backend | Already on host `[SO]` |
| cc65 (`ca65`, `ld65`, `cl65 -t lynx`) | master commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2` (2026-11-08), built from source | Guest assembler and linker | V2.19 (2020) is 3,959 commits behind and lacks `ENABLE_INT` and `ENABLE_COUNT`, which lynx-tests `cpu/tests.s` uses `[SO]`. lynx-tests CI builds unpinned master `[SO]`. |
| lynx-tests (fixture source, not a dependency) | tag 1.4.9 = `b583dc43d532d3c41563a6fa35420614fbcead89` (2026-08-13) | Public hardware-informed test ROM sources | MIT `[SO]`. HEAD `a920f01` differs from 1.4.9 only trivially `[SO]`. Prefer the tag. |
| `pins.txt` and `scripts/fetch-pin.sh` | ~60 lines, POSIX sh | SHA-256-verified fetch of every pinned source | Copied from the muhtari pattern `[SO]`; no package manager `[REC]` |
| ASan, UBSan, `-Wall -Wextra -Wconversion -Wshadow` as errors | compiler built-in | Check lane | `[REC]` |
| SingleStepTests/65x02 | pinned commit, MIT | Per-opcode functional vectors, filtered per profile | Generated from documentation, not hardware `[SO]`; its bus traces assume standard 65C02 timing, which Lynx page-mode does not share `[INF]` |
| shellcheck; clang-format (version pinned later) | host has shellcheck | Lint and format | `[SO]` / `[REC]` |

Infrastructure: GitHub Actions with explicit runner labels (`ubuntu-24.04`, `macos-15`) and SHA-pinned actions `[REC]`. Number of runners is a disagreement (see Disagreements, item 6). Core runtime dependency count: zero (links only `memcpy`, `memmove`, `memset`, `memcmp`) `[REC]`. libFuzzer and Valgrind are deferred `[SO]`.

**Do not use:** cc65 V2.19 or a distro/Homebrew cc65 in the evidence path; unpinned cc65 master or `*-latest` runner labels; Gearlynx, MAME, Holani or Handy code copied or linked; the lynx-tests release zip as a committed fixture (comparison target only); the Holani "free" boot ROM; the no-licence repositories `atarilynx/handy-examples`, `handy-bootloaders`, `handy-encryption`, `handy-docs` and `emulator`; Klaus Dormann's functional tests (GPL-3.0); Python or Node in the required check path.

### Expected Features

Feature IDs are FEATURES.md's. Complexity is that file's estimate.

**Must have (first credible slice):**
- Hardware: HW-01 data-driven CPU profile; HW-02 65C02-class CPU with IRQ entry rules; HW-03 timing model on a master-tick base (complexity HIGH, the single highest-risk decision); HW-04 memory map with MAPCTL; HW-05 timers 0 and 2 first, then the full bank; HW-07 display and palette; HW-08 input registers; HW-13 defined power-on state.
- Content: CT-03 direct-load container with a documented preset (first boot path, no firmware); CT-01 strict 64-byte `.lnx` header parsing (needed for the cartridge phase).
- Core and host: CH-01 headless runner; CH-02 logical input sampled at guest-time boundaries; CH-03 raw video; CH-04 raw audio contract (silence is acceptable early); CH-06 refusal postconditions; CH-07 one local check command.
- Verification: VF-01 pinned fixtures with manifest; VF-04 per-instruction vectors; VF-05 same-core repeatability (labelled as repeatability); VF-06 independent consumer; DF-04 recorded input data.

**Should have (differentiators, after first light):** DF-01 evidence-tiered claims bound to a profile; DF-02 four connected observation streams on one guest-tick timeline; DF-03 exact bounded advancement; DF-06 machine-readable lynx-tests results via RAM mailbox with a manifest; DF-09 Lynx I and Lynx II profiles with a qualification procedure; DF-10 installed-host check; DF-05 trace, breakpoint and status API (later; a trace ring starts in phase 1); DF-07 guest debug output channel (optional; a plain RAM mailbox is simpler and needs no extension).

**Defer (v2+ or later milestones):** ComLynx multi-instance (OWNER O12); GameDrive and ElCheapoSD; tape; LNX2 and Turbo ComLynx; rewind, run-ahead, cheats, VGM, netplay and achievements; rich debugger UI and MCP or DAP wrappers; physical-hardware tier T5 (OWNER O11); LibFuturo adapter (after a real consumer round-trips; OWNER O9); tier D HLE with decryption (OWNER O4); EEPROM (later slice, phase 11).

**Anti-features (do not build or promise early):** bundling or auto-downloading a BIOS or commercial ROMs; silent fallback boot; HLE boot as the accuracy path; copying code, game databases or boot blobs from GPL emulators; fixed instructions-per-frame scheduling; 75 FPS as the machine's definition; overclock, frameskip, ghosting, shaders, rotation or button remapping inside the core; claiming "passes all lynx-tests" or "whole catalogue" without a per-profile inventory; OCR of on-screen text as the oracle.

### Architecture Approach

The architecture is a machine core with no host clocks, files, floats, threads or globals, behind a C ABI facade. Arrows point machine to host; the only host inputs are content bytes, configuration, logical input, tick budgets, caller-owned buffers and state bytes `[REC]`. Content parsing (`.lnx`, direct-load, project container) sits outside the machine and returns a normalised cartridge image or a refusal `[REC]`.

**Major components (ARCHITECTURE `[REC]`):**
1. **timebase/scheduler:** one `u64` master-tick clock; ordered event queue, ordered by (tick, priority, sequence). The same-tick ordering table is part of the determinism contract.
2. **cpu:** 65C02-family engine plus a `CpuProfile` table. Knows nothing of MAPCTL or Suzy.
3. **bus:** 64 KB RAM, MAPCTL decode, the arbiter (video > refresh > CPU > Suzy), access-cost model including page-mode stream state, and two views: a CPU-mapped view and a raw RAM view for Suzy and DMA.
4. **mikey:** timers as one shared-prescaler bank with link graph; irq; audio; uart; display (line and frame generator, DMA bursts as a bus requestor, palette); io (IODIR/IODAT, SYSCTL1, AUDIN, SDONEACK, CPUSLEEP).
5. **suzy:** register file and bus-master state machine; sprite painter (a pure function) plus a timing shell; math unit; cartridge data port; input.
6. **cart:** normalised image, 8-bit shift register and counter, banks, EEPROM. Mikey drives the address port and Suzy drives the data port; neither owns the cartridge.
7. **boot:** startup strategies, each tagged in every run (`startup=`).
8. **observe, state, input, api:** read-only inspection and trace ring; versioned, identity-bound snapshot including the scheduler queue; logical input timeline; C ABI facade with sized structs and status codes.

**Patterns to follow:** deadline scheduler with a total order; shared-prescaler timer bank (per-timer private clocks fail `timers2` by construction); explicit bus-master arbiter state machine (the sdoneack suite is a transition table); pure painter plus timing shell for Suzy; two memory views with side-effect-free peek (some reads have side effects, so peek is a separate path); pull-based, bounded trace; normalised content descriptor.

**Host API operations:** `create`, `destroy`, `load_content` (a rejected load leaves the active session untouched), `load_firmware` (optional, OWNER tier C only), `unload`, `set_input` or an input timeline, `advance(max_ticks, stop mask)` returning ticks, stop reason, frame flag and samples, `reset` (power-on under the configured RAM policy), `inspect` (never changes state or time), `read_state`, trace drain, media copy and import, and state capture and restore (validate before swap).

**Architecture decisions the research implies:**

| ID | Decision | Recommendation (researcher) | Class |
|---|---|---|---|
| AD-01 | Guest-time unit and sync | Master tick; bus-access granularity with lazy device catch-up; frames and instructions are stop conditions, not budgets. Fallback: instruction stepping with per-access start ticks. `[REC]` | Routine; record in spec |
| AD-02 | Baseline model | See Disagreements, item 1 | **OWNER O1** |
| AD-03 | Startup strategy | A (direct-load) first, B (project boot shim) next, C (owner BIOS) separately evaluated, D (HLE with decryption) deferred; every run tagged `startup=` | A routine. B: see Disagreements, item 2. C and D: OWNER. |
| AD-06 | Raw video contract | See Disagreements, item 5 | Routine for native; OWNER for any LibFuturo change |
| AD-07 | Raw audio contract | Exact-rational integer PCM derived from 16 MHz by an integer divisor, plus optional tick-stamped channel events; no float and no filter in the core `[REC]` | Routine; choose the divisor in the audio phase |
| AD-10 | Guest progress | RAM result block, watch, and symbol file; no screen scraping | Routine |
| AD-11 | API shape | Native C ABI satisfying the LibFuturo obligations; adapters stay thin; no ABI promise yet | Routine; OWNER for LibFuturo contract changes |
| AD-12 | Snapshot scope | Same-build, identity-bound, validate-before-apply; round-trip test in an early phase | Routine |
| AD-13 | RAM power-on policy | Config field; fixed pattern by default; seed recorded in identity | Routine |
| AD-14 | ComLynx scope | Single-console loopback first, behind a logical wire port | Roadmap scope; OWNER O12 |
| AD-15 | Cartridge header policy | Strict: refuse unknown or short headers and unknown EEPROM kinds | Routine |
| AD-16 | Language and build | Not decided by owner. Constraints: C ABI, no hidden allocation during advance, no floats in the machine, integer semantics, reproducible build, small dependency tree | **OWNER O8** |

### Critical Pitfalls

Numbering is PITFALLS.md's (P-numbers; V-numbers for the adversarial pass in its section 1).

1. **A cc65 `.lnx` does not boot in an emulator (P1, P2, V1).** The first bank-0 block is an encrypted, signed loader. The real boot ROM decrypts it, reads a secondary loader through the cartridge port, and leaves cartridge and register state that the loader relies on. Prevention: tag every run with its startup tier; never describe a RAM-load result as boot, cartridge or loader evidence; record a post-boot state record and hash it into identity; do not let an HLE that only sets PC pass as boot evidence.
2. **Fixed timing is wrong (P7, P23).** Access costs vary (page-mode 4 versus normal 5 ticks, cart read 15, Suzy read 9 to 15), display DMA steals the bus, Suzy owns it while drawing, and timer phase is undefined at start. Prevention: one `u64` tick base; bus-cycle interface under the CPU even if the first cut costs per instruction; no `RAM_ACCESS_TICKS = 5` constant; regression on `page-mode`, `timers2` and `sdoneack` before any optimisation.
3. **The CPU profile is mislabelled, and the suite passes either model (P6, V8).** Name the profile by instruction-set behaviour, not chip name. `cpu` test 7 accepts Lynx I or Lynx II as a pass, so the harness must assert which model matched the selected profile (Lynx I `FF 00 02 02`, Lynx II `FE 01 01 01`, at `g_results[12..15]`).
4. **Result-capture traps (P20, P19, V4).** Zero is a PASS in several slots and is also uninitialised BSS. `timers2` judges itself. Expectation tables are duplicated from `main.c`. Prevention: symbol-resolved `g_results`, a completion sentinel plus a guest-time minimum and bound, expectations generated from the pinned source with a count check, and a no-run control (a CPU hung before `run_tests` must fail every suite). Same-core hashes are repeatability, not accuracy.
5. **Rights and licence hygiene (P3, P4, P5).** Firmware can leak through CI caches, artifacts, traces and goldens. GPL source can contaminate the code. lynx-tests is MIT at the root, but built `.lnx` files also carry cc65's zlib runtime and an encrypted loader blob, and `sprites2` embeds an Alpine-derived literal. Prevention: fail-closed firmware flag; artifact allow-list and deny-list scan; clean-room rule; manifest with all notices and the owner-approved allowed use.
6. **The cartridge is not a flat array (P17, P18).** Address comes from a shift register plus a counter, with block selection and AUDIN banking. Header dialects and EEPROM kinds are not settled by a primary specification. Prevention: model the cartridge at the port level; treat header fields other than the cc65 layout as advisory; EEPROM kind is explicit configuration, never a game database; save identity is content plus hardware configuration.
7. **Suzy and sleep are an asynchronous state machine (P9, P13, P14).** Sprite drawing as a synchronous call, sleep as a boolean, or quirks fixed to match documentation all fail `sdoneack` and `sprites4`. Prevention: bus-master FSM; labelled, counted diagnostics for software that violates documented rules; quirk ledger with tests.

### Facts the Roadmap Depends On

These are the facts the phases rely on. Labels show the source; "WEB" facts still need a verbatim check against the Epyx PDF.

| Fact | Value | Label |
|---|---|---|
| Master clock | 16 MHz crystal, 62.5 ns tick; device clocks are integer multiples of it | `[WEB]` Epyx; unit choice `[INF]` |
| Access costs | Page-mode fetch 4 ticks; normal RAM or ROM 5; cart read 15; Suzy read 9 to 15, Suzy write 5; Suzy bus grant up to 40 ticks | `[WEB]` Epyx. Page-mode eligibility rules `[MR]` (lynx-tests guide) |
| Page mode scope | Covers the instruction stream (opcode and operand bytes, 16-byte boundary), not only opcode reads. Epyx text is incomplete. | `[MR]`; `[INF]` |
| Display geometry | 160 pens by 102 visible lines inside a 105-line frame (3 VBlank); 4 bits per pen; 80 bytes per line | `[WEB]` and `[MR]` |
| Refresh | Default about 59.90 Hz: 2,544 ticks per line, 267,120 ticks per frame. Timer 0 sets line length, Timer 2 sets line count, PBKUP sets refresh. | `[MR]`; arithmetic `[INF]` |
| Palette | 16 entries of 12 bits (4096 colours), registers at `$FDA0` to `$FDBF` (32 bytes); 16 colours per frame unless palette changes per line | `[WEB]` |
| Memory map | 64 KB; Suzy `$FC00` to `$FCFF`; Mikey `$FD00` to `$FDFF`; ROM overlay `$FE00` to `$FFF7`; MAPCTL `$FFF9`; vectors `$FFFA` to `$FFFF` | `[WEB]`; consistent across the three researchers |
| Bus priority | Video > refresh > CPU > Suzy; one master at a time; no default owner | `[WEB]` |
| Boot ROM | 512 bytes inside Mikey; reads the first cartridge block and runs it | `[WEB]` for the Epyx side; loader side `[SO]` (cc65 `bootldr.s`) |
| `.lnx` header | 64 bytes: magic `LYNX`; bank-0 and bank-1 page sizes; version 1; 32-byte name; 16-byte manufacturer; rotation; 5 spare bytes (cc65 layout) | `[SO]` (cc65 `exehdr.s`) |
| `.lnx` payload | First bank-0 block is an encrypted, signed loader (one count byte plus 51 encrypted bytes, depending on how the count byte is counted) | `[SO]` (cc65 `bootldr.s`) |
| Cartridge address | 8-bit shift register plus 11-bit counter (up to 19 bits) | `[WEB]`; counter width disputed (12 bits in one hobbyist note) `[UNK]` |
| CPU | Epyx: "a 65C02 core embedded in the Mikey IC". cc65 compiler target table: lynx to `CPU_65SC02` (`target.c` line 246). lynx-tests test 7 expects different results for Lynx I and II. | `[WEB]`; `[SO]`; `[SO]` |
| Lynx I versus II | Minimum known difference: RMB, SMB, BBR and BBS act as NOPs on Lynx I and execute on Lynx II. Lynx II MAPCTL read-back differs. AUDIN reset level differs on some revisions. | RMB/SMB/BBR/BBS: `[SO]` (test expectations). MAPCTL read-back: `[MR]`. AUDIN reset level: `[WEB]` (hobbyist note). |
| Suite inventory | 19 directories with a Makefile; 18 numbered suites; 127 numbered tests (122 single-console, 5 in `uart4`); 17 headless fixed suites. `refresh-rate` is interactive; `uart4` needs two consoles and a cable. | `[SO]` |
| Result location | `g_results` array in guest RAM; `-Ln` label file from the Makefile gives addresses. Not yet executed in a harness. | `[SO]` source; execution `[UNK]` |
| Pins | cc65 `eebf54a` (master, 2026-11-08); lynx-tests tag 1.4.9 = `b583dc43` (2026-08-13) | `[SO]` |
| Licences | lynx-tests MIT; cc65 Zlib; SingleStepTests MIT; Gearlynx and Holani GPL-3.0; MAME GPL-2.0+; libretro-handy zlib; `cc65-lynx-gametemplate` CC0-1.0; `new_bll` Apache-2.0 (reference only); Klaus Dormann GPL-3.0 | `[SO]` |

---

## Disagreements Between Researchers

Surfaced, not averaged. Items 1 to 6 are owner-level or routine choices the roadmapper must not settle silently. Items 10 to 15 are provenance or internal-consistency issues.

### A. Owner-level or policy disagreements

**1. Baseline model for the first claims (OWNER O1).**
- **FEATURES** (HW-01, gap 1; `[REC]`, MEDIUM-LOW confidence): start with a Lynx I-style 65SC02 profile without RMB, SMB or BBx. Reasons: cc65 targets 65SC02; MAME models G65SC02; `cpu` test 7 has explicit expectations for both. Counter-consideration: Lynx II is the later, more common unit.
- **ARCHITECTURE** (AD-02, `[REC]`) and **PITFALLS** (P26, `[REC]`): start with Lynx II as one named `CpuProfile`; Lynx I as a later data diff with its own qualified claim. Reasons: the timing captures cited by the page-mode and video-timing guides are attributed to Lynx II `[MAINT]`; Gearlynx's "Auto" model defaults to Lynx II `[WEB]`; Lynx II implements the bit operations, so its profile is a superset.
- **Agreement:** all three recommend that the profile be data, that both tables exist, and that claims name the profile. They disagree only about which is first.
- **Weaknesses to weigh:** (a) cc65's target table is a code-generation choice, so it is weak evidence about the hardware. (b) MAME's driver is GPL-2.0+, uses BAD_DUMP ROMs and has Lynx II disabled, so its G65SC02 is weak evidence. (c) ARCHITECTURE's premise that the timing captures are Lynx II is weakened by PITFALLS, which notes that several guide measurements record "the hardware model was not recorded" or "single attempt".
- **Status:** open owner decision. Neither recommendation is adopted.

**2. Who owns a project-authored boot ROM, tier B.**
- ARCHITECTURE (AD-03) treats tier B as routine and second in order.
- STACK's decision table lists "shipping a project-authored boot ROM" as Owner (rights).
- **Synthesis:** treat tier B as owner-gated (OWNER O2). The synthesis rule on adopting any boot ROM points the same way.

**3. Alpine-derived literal in `sprites2` test 8.**
- STACK: name it in the fixture manifest, since it is a fact about hardware behaviour.
- PITFALLS (P5): flag it as an owner rights decision.
- **Synthesis:** owner decision (OWNER O7). Recording it in the manifest happens regardless.

**4. Research exposure to GPL source.**
- PITFALLS (P4) requires that implementers not open Gearlynx, MAME or Holani source. The research itself read Gearlynx and Holani source (GPL-3.0) and MAME source through a fetcher (GPL-2.0+), plus Handy (zlib) through a fetcher (ARCHITECTURE, STACK, FEATURES).
- **Synthesis:** OWNER O6. The owner should decide whether the clean-room rule binds implementers and how research exposure is recorded. This interacts with the open licence (O5). This summary repeats no GPL-derived values; ARCHITECTURE's preset values from another emulator are deliberately not carried forward (AD-04).

**5. Canonical raw video form.**
- FEATURES (CH-03): pen-index plane, plus the 16 palette words per line or per frame, plus frame markers.
- ARCHITECTURE (AD-06): resolved 12-bit words, optional pen plane, palette dump. States that a pen-index plane alone cannot reproduce the image.
- PITFALLS (P12): (line index, pen data, palette snapshot, timing metadata).
- **Agreement:** all three keep the palette snapshot and timing metadata. They differ on which representation is canonical.
- **Status:** decide in suggested phase 5. OWNER O9 for any LibFuturo format change.

**6. CI breadth at the start.**
- STACK: two runners and two compiler families from the start, as a determinism cross-check.
- PITFALLS (P24) and AGENTS.md: one gate first; add an OS, arch or compiler lane only when a distinct digest-mismatch class is demonstrated.
- **Synthesis:** routine. The suggested default is one runner in phase 1 and the cross-compiler lane in phase 13, after measurement. This fits AGENTS.md's rule that a CI check must catch a distinct recurring failure. Do not treat STACK's two-runner plan as settled.

### B. Sequencing disagreements

**7. First visible output.**
- FEATURES, slice 1: a visible homebrew in the first slice, with timers 0 and 2, display DMA and palette.
- ARCHITECTURE: no video until the full timer bank and IRQ are in place (slice 4); slice 2 reports by result block only.
- **Synthesis:** keep ARCHITECTURE's ordering for timing. Add a labelled, tick-stamped snapshot of the framebuffer in phase 3, so visibility arrives early without a frame model. This meets the "visible early" rule and avoids PITFALLS P25 (research-only drift).

**8. Placement of the `cpu` suite.**
- FEATURES places `cpu/` as the first step of a CPU-only ladder.
- PITFALLS (V3) and ARCHITECTURE say it needs Timer IRQs (Timer 6) and interrupt vectors in RAM.
- **Synthesis:** place it in phase 4, with timers and IRQ. Verify on the pinned commit before relying on either account.

**9. Suzy sprites versus cartridge.**
- FEATURES: literal sprites in slice 2; cartridge in slice 4.
- ARCHITECTURE: bus-master shell and cartridge port together (slice 7); sprite painter after them (slice 8).
- **Synthesis:** follow ARCHITECTURE. Arbitration and the cartridge port are the costliest retrofits (P9 and P17 are rated critical). The owner may prefer an earlier sprite demo; the phase 5 homebrew already provides visibility either way.

### C. Internal inconsistencies and provenance

**10. Frame completion.** ARCHITECTURE AD-08 defines a frame as complete at the last visible pixel of the last visible line. ARCHITECTURE's LibFuturo fit defines a frame as the VSYNC event. These are different events. Settle in phase 5.

**11. Epyx provenance.** FEATURES labels the monlynx.de pages `[SO]`. ARCHITECTURE and PITFALLS state that they were read through a summarising fetcher. **Synthesis:** treat them as `[WEB]` until checked verbatim against the scanned Handy specification PDF.

**12. BLL versus BS93 container.** FEATURES says `lynx-bll.cfg` yields "BLL-compatible (homebrew `.o`)" output. STACK says it yields a "BLL download binary". Nothing shows they are the same container. Spike in phase 3.

**13. Cartridge header layout.** FEATURES CT-01 gives an audio-banking byte, an EEPROM byte and three spare bytes, taken from libretro-handy code read through a fetcher. ARCHITECTURE and the cc65 primary source give rotation followed by five spare bytes. PITFALLS says community layouts are inconsistent. **Synthesis:** cc65's layout is authoritative for cc65 output; other layouts are unverified dialects. Resolve in phase 10.

**14. Holani ROM credit string.** STACK and FEATURES quote `bjoern.spruck.net/lynx`. PITFALLS quotes `lynxdev.atari.org`. ARCHITECTURE says the latter redirects to the former. Not material while the ROM is not adopted.

**15. Directory counts.** PITFALLS says "19 of 20 directories" link with `-Ln` and that "all 17 single-console suites plus refresh-rate" expose `g_results`. STACK counts 19 Makefile directories. The twentieth directory is presumably `shared/`, which has no Makefile. Verify per pinned suite in phase 4.

---

## Corrections to Bootstrap Claims

These corrections are recorded for the roadmap. They are not requirements. The table uses the bootstrap material's own wording where it is quoted.

| # | Bootstrap claim (source) | Status | Correction (evidence) |
|---|---|---|---|
| 1 | "cc65's target documentation says 65SC02" (EMULATOR-DNA, seed) | **Corrected** | The Lynx doc page (`cc65.github.io/doc/lynx.html`) names no CPU. The mapping is in cc65's compiler target table, `src/common/target.c` line 246, which maps `lynx` to `CPU_65SC02`. ca65 defines 65SC02 as the CMOS set without bit manipulation or WAI and STP. PROJECT.md already states this correctly. `[SO]` |
| 2 | "Do not assume the public test ROMs expose machine-readable results" (seed) | **Corrected** | Results are in guest RAM in a `g_results` array, and the Makefile emits a `-Ln` label file giving addresses. The on-screen text is secondary. Source-observed; not yet executed in a harness. `[SO]` |
| 3 | "cc65 documents `cl65 -t lynx` producing a bootable `.lnx`" (PROJECT, DNA) | **Qualified** | The first bank-0 block is an encrypted, signed loader that the real boot ROM must decrypt. cc65's own docs say the loader "does not check the cart or show a title screen". On an emulator it cannot start without the real ROM, a replacement, a decrypting HLE, or a RAM-load path. `[SO]` |
| 4 | "The lynx-tests CPU suite identifies Lynx I and II RMB, SMB and BBx behaviour" (PROJECT) | **Qualified** | Test 7 covers the bit-0 forms only. Its verdict auto-detects the model, so an emulator passes if it matches either model unless the harness asserts one. The suite's file header calls the profile "65SC02 Subset", which conflicts with cc65's naming. `[SO]` |
| 5 | "160 by 102 display with 4096 colours" (seed) | **Confirmed, with nuance** | 160 pens by 102 visible lines inside a 105-line frame. 16 palette entries of 12 bits, so 16 colours per frame unless the palette changes per line. Refresh is programmable. "Triad" is ambiguous; avoid it in contracts. `[WEB]`, `[MR]` |
| 6 | "Gearlynx passes all lynx-tests" (seed) | **Maintainer report** | The README's claim, not independently verified. Same author as the suite. Describes a with-BIOS configuration. `[MR]` |
| 7 | "Gearlynx requires a BIOS" | **Confirmed as a maintainer statement** | The source has a no-BIOS homebrew path. Its runtime behaviour is untested. `[MR]`, `[IMPL]` |
| 8 | libretro Handy and Beetle docs: "lynxboot.img required" | **Qualified** | The libretro-handy code runs without it through an HLE fallback `[IMPL]`. |
| 9 | "Suites run at startup and show status on screen" (seed) | **Qualified** | True for the fixed suites, but a RAM result exists. `refresh-rate` is interactive; `uart4` needs two consoles. `[SO]` |
| 10 | "MAPCTL changes only the CPU-visible mapping" (seed) | **Accurate but incomplete** | Also: bit 7 disables page mode; `$FFF8` and `$FFF9` are not overlaid; Lynx I and II read-back differ `[MR]`. |
| 11 | "The Lynx has a 512-byte onboard boot ROM" | **Confirmed** | Inside Mikey. It reads cartridge data and executes it. `[WEB]` |
| 12 | "lynx-tests is MIT" | **Qualified** | The root `LICENSE` is MIT with no per-file headers. Built `.lnx` files also carry cc65's zlib runtime and an encrypted loader blob (attributed to its 2011 author in `bootldr.s`). `sprites2` contains an Alpine-derived literal. The manifest needs all of these notices. `[SO]` |
| 13 | "Plan a clean path: owner firmware, an independent route, or a combination" (seed) | **Incomplete** | It omits the RAM-load route, and the existing candidates: Holani's embedded ROM and BLL loader. Both need provenance and licence review. |
| 14 | "Timing depends on page mode and a fixed instructions-per-frame model is unsafe" | **Confirmed** | Page mode covers the instruction stream, not only opcode reads (see Facts). `[MR]` |
| 15 | "LibFuturo's reviewed profiles do not include Lynx" | **Unverifiable here** | No public source was found. Owner-internal. |

---

## Decision Ownership

### OWNER-level decisions (batch these; do not lock them in phase text)

| # | Decision | Why it is the owner's | Current state | Needed by |
|---|---|---|---|---|
| O1 | Baseline Lynx model and CPU profile for the first claims (Lynx I or II) | Changes claim text and which tests count as passing (PROJECT.md: open) | Two recommendations, neither adopted (Disagreement 1) | Before any suite result is claimed. Tables can be built earlier. |
| O2 | Shipping a project-authored boot ROM (tier B) | Adopting or shipping boot-ROM bytes is a rights decision | Recommended second; not approved | Phase 10 |
| O3 | Tier C: owner-supplied BIOS, local-only use, and any local cross-check against another emulator | Real firmware is rights-bearing; owner authority | Not used; no BIOS downloaded | Phase 10 (optional) |
| O4 | Tier D HLE with the public decrypt constants, and the Holani "free" ROM | Rights and provenance (LOW; licence unknown) | Deferred; do not adopt | Before any HLE |
| O5 | Project licence | Not chosen (PROJECT.md: open). Affects how GPL exposure is judged. | Open | Before any public artifact |
| O6 | Clean-room protocol, and how the research's GPL-source exposure is recorded | Licence and rights (Disagreement 4) | Owner to accept or restrict | Phase 1 policy document |
| O7 | Whether the Alpine-derived literal in `sprites2` test 8 may appear in the fixture set | Rights (commercial-game-derived bytes) | STACK and PITFALLS disagree on treatment | Phase 9, or earlier if the suite is imported |
| O8 | Implementation language, build tooling and API or ABI | PROJECT.md lists these as not selected by the owner. STACK treats C17 as a routine default. | Recommended C17; confirm | Phase 1 |
| O9 | Canonical raw video form, and any LibFuturo format or tick-unit change | Shared-contract change (LibFuturo is owner-internal) | Researchers differ (Disagreement 5) | Phase 5 (form); phase 12 (LibFuturo) |
| O10 | Repository visibility, publication and release route | Explicit owner boundary (AGENTS.md; PROJECT.md) | Not authorised | Milestone close |
| O11 | Physical hardware qualification (T5): refresh-rate calibration, `uart4` with two consoles, Lynx I or II comparison | Requires hardware and owner authority | Out of scope | Later |
| O12 | ComLynx scope: single-console first, multi-instance later | Scope and cost (PROJECT.md: open) | Recommended: single-console first | Roadmap |
| O13 | Second model qualification (whichever of Lynx I or II is not baseline) | Each model's claims need their own evidence plan | Deferred | Later |

### Routine, reversible defaults (proceed; record in the decision log)

- C17 core with `CMAKE_C_EXTENSIONS OFF` and `-pedantic-errors`; zero runtime dependencies; warnings as errors; no `-ffast-math`; no `-march=native` in evidence builds; no floating point in machine state. (Language itself: see O8.)
- CMake 3.25 or later with workflow presets; Ninja; CTest with an exact-inventory check; `scripts/check.sh` as the single local command aligned with the fast PR gate.
- ASan and UBSan in the check preset.
- Pins: cc65 `eebf54a` built from source (bumps by dedicated PR with recomputed hashes); lynx-tests tag 1.4.9; SingleStepTests at a pinned commit; explicit runner labels.
- Startup tier A (direct load) first, with a preset table in which every value cites a source. Uncited values are left at power-on reset and the fixture sets them itself.
- CPU profile as data; both tables defined; claims name the profile; the detected model is recorded.
- Raw outputs kept separate from presentation; RAM result block read by symbol; stop-on-watch; no OCR.
- RAM power-on: fixed pattern by default, seed recorded in identity.
- Strict `.lnx` parsing: refuse unknown or short headers; rotation and any spare-byte fields are advisory only.
- Snapshot: same build, identity-bound, validate before apply; round-trip test in an early phase.
- Forbidden-import lint for the core; fail-closed firmware check in CI.

---

## Implications for Roadmap

The phases below are **suggested vertical slices**, not final phases. Each ends in a runnable, deterministic, checkable artifact. Phase numbers are for reference only. The roadmapper may merge or split them.

**Important constraint:** cc65-built `.lnx` files cannot be the first homebrew, because they need a boot path. The first playable and homebrew artifacts (phases 3 to 6) ship as direct-load images (startup tier A). The `.lnx` path waits for phase 10. If the `lynx-bll.cfg` spike in phase 3 fails, the fallback is `ca65`/`ld65` with a project config and a project packer (ARCHITECTURE's option 1).

### Phase 1: Foundation: evidence harness and walking skeleton

- **Rationale:** every later claim needs the manifest, the check command, the determinism gate and the refusal postconditions. These are cheap and reversible, and the build graph can enforce the core boundary.
- **Delivers:** C17 skeleton with the layout `core/`, `content/`, `runner/`, `consumer/`, `fixtures/`, `spec/`, `tools/`; `scripts/check.sh` and CMake presets; `pins.txt` and `fetch-pin.sh`; manifest schema (source, commit, recipe, SHA-256, licence and notices, allowed use, startup tier, profile, evidence tier, independence); forbidden-import lint for the core; fail-closed firmware check, tested with a seeded fake 512-byte file; clean-room policy document; claim template; walking-skeleton API (create, destroy, load with refusal that leaves state untouched, advance with a tick budget, inspect); runner that writes an artifact bundle with digests; two-run and two-instance determinism; round-trip snapshot of trivial state; first spike: build pinned cc65 from source on the chosen runner and record wall time and the `TARGETS=lynx` recipe.
- **Addresses:** CH-01, CH-06, CH-07, VF-01 (manifest), VF-05, DF-01 (schema), CH-05 (round-trip early).
- **Avoids:** P3, P4, P5, P19, P21, P22, P24, P27.
- **Checkable artifact:** `check` green; bundle digest identical across two runs and two instances; the seeded firmware-like file fails the scan; a refused load leaves state unchanged.
- **Research flag:** standard patterns, except the cc65 build spike.
- **OWNER gates:** O8 (language, which PROJECT.md still lists as open); O6 before the policy document is final.

### Phase 2: CPU profile and per-opcode vectors

- **Rationale:** the CPU has no device timing dependency and gives cheap T1 evidence early. The profile must be data from the start, so that O1 does not block code.
- **Delivers:** 65C02-class engine on a flat 64 KB test bus; `CpuProfile` tables for Lynx I and Lynx II, both defined, neither adopted as baseline until O1; memory accesses sequenced through a bus-cycle interface with a default 5-tick cost; reserved NOP lengths; WAI and STP handling; interrupt-entry hooks; a result schema that records the detected and asserted profile.
- **Addresses:** HW-01, HW-02, VF-04, DF-09 (data only).
- **Avoids:** P6, P7 (interface), P11 (declared power-on policy), P22.
- **Checkable artifact:** vector suite passes per profile with an exact inventory; named-fault control: a wrong profile fails the bit-operation vectors.
- **Research flag: deeper research needed:** which SingleStepTests directory applies to each profile, and which opcodes to exclude (RMB, SMB, BBR, BBS for Lynx I); decimal-mode cycle behaviour and reserved-opcode cycle counts (unsourced); the shape of the bus-cycle interface (ARCHITECTURE AD-01 spike).
- **OWNER gates:** O1 before any CPU-profile claim.

### Phase 3: Startup tier A, memory map, first original diagnostic, visible snapshot

- **Rationale:** tier A is the cheapest rights-clean start, and all four researchers recommend it first. A small original diagnostic gives semantic progress before any device exists. A labelled snapshot gives early visibility without claiming frame timing.
- **Delivers:** MAPCTL decode with two views (CPU-mapped and raw RAM); direct-load container (BS93-style or project-defined; identity of the `lynx-bll` output to be settled) with a preset table in which each value cites a source or is left at reset for the fixture to set; original 65C02 diagnostic (ca65 with the pinned cc65, or a hand-assembled listing as fallback) that writes a result block (magic, schema, progress, error, payload, done byte); watch and stop-on-write as pure observers; tick-stamped framebuffer snapshot, explicitly not a frame, rendered to PNG or PPM by the independent consumer.
- **Addresses:** HW-04, HW-13, CT-03, CH-01, CH-02, CH-03 (partial), VF-06 (partial), DF-06 (pattern), DF-07 (optional).
- **Avoids:** P1, P2, P10, P11, P20, P25.
- **Checkable artifact:** the diagnostic reports PASS through the result block; the run manifest says `startup=A`; the snapshot PNG is viewable; a repeat run gives an identical digest.
- **Research flag: deeper research needed:** per-register preset provenance (AD-04). Sources disagree on at least one reset value (DISPCTL: Epyx recommends one value, a GPL emulator uses another). Cite the Epyx or test value; do not import the emulator's. Spike whether lynx-tests can be relinked with `lynx-bll.cfg` and whether that output equals the BS93 `.o` format (Disagreement 12).
- **OWNER gates:** none for tier A.

### Phase 4: Mikey timers, IRQ and CPU sleep; first public suites

- **Rationale:** timers are the root of display, audio, UART and IRQ or sleep. The `cpu` and `timers` suites both depend on Timer IRQs, so they cannot run as a CPU-only step (Disagreement 8).
- **Delivers:** shared-prescaler timer bank (phase shared across timers 0 to 7 and the audio timers); link graph; DONE, reset-done and borrow semantics; pending and enable kept separate; IRQ sampling rules (CLI delay; reserved NOPs do not acknowledge an IRQ); minimal CPUSLEEP with the arbiter skeleton and `cpu_bus_request_ff`; lynx-tests harness: symbol-resolved `g_results` from the `-Ln` file; completion sentinel (PC reaches a symbol after a minimum guest time) plus a guest-time bound; expectation tables generated from the pinned source with a count check; a no-run control (CPU hung before `run_tests` must fail every suite); recorded detected model.
- **Addresses:** HW-05, HW-06 (part), VF-02, VF-03, DF-06.
- **Avoids:** P8, P6 (model assertion), P20, P21.
- **Checkable artifact:** `timers` (7) and `cpu` (8) pass under the declared profile, with an exact inventory; named-fault controls (DONE gating removed; wrong profile) each fail the intended test; timer state included in the round-trip.
- **Research flag: deeper research needed:** verbatim re-read of Epyx timer and IRQ material; `timers2` phase and enable-order expectations; `cpu` tests 1 and 8 (interrupt-entry quirks).
- **OWNER gates:** O1 before the `cpu` suite is claimed as passing for any profile.

### Phase 5: Display, raw video and input: first live homebrew frame

- **Rationale:** display makes the project visible and is where frame timing lives. It needs timers (phase 4). It comes before Suzy so that display DMA exists when sprite timing is measured (ARCHITECTURE).
- **Delivers:** line and frame generator driven by Timers 0 and 2; DISPADR latch; per-line display DMA as a bus requestor that closes page-mode streams; refresh event; 16-entry, 12-bit palette with per-line latch behaviour; raw video contract (canonical form per O9; carries palette state per line and timing metadata); frame-complete event (definition to settle, Disagreement 10); joystick and switch registers from a logical input timeline (polled, no strobe); an original input-responsive homebrew scene, using the CC0 `cc65-lynx-gametemplate` layout as reference only, delivered as a direct-load image.
- **Addresses:** HW-07, HW-08, CH-02, CH-03, DF-04, DF-08, VF-06.
- **Avoids:** P12, P7 (page mode and DMA interplay), P22, and the UX pitfall of fixed 60 Hz pacing.
- **Checkable artifact:** the consumer renders input-responsive frames; the same recorded input gives the same raw-frame digest (repeatability only); `page-mode` (6) is run with results; a per-line palette fixture; a DISPADR and palette byte-order test using `memio` test 1 expectations.
- **Research flag: deeper research needed:** DMA burst timing and refresh period and cost (OPEN; the GPL emulator's constants are implementation choices, not sourced hardware values, and must not be imported); per-line versus per-frame palette contract; frame-complete definition; palette byte order versus LCD latch order.
- **OWNER gates:** O9 before the output contract is frozen.

### Phase 6: Audio and the first playable checkpoint

- **Rationale:** audio depends only on timers (ARCHITECTURE: independent of display and Suzy), so it can run in parallel with phases 7 and 8. It adds sound to the playable scenario.
- **Delivers:** four channels (LFSR, integrator, one-shot and reload, volume); link ring with timers (sources give the ring as T1 to T3 to T5 to T7 to A0 to A1 to A2 to A3 to T1); MSTEREO muting; exact-rational integer PCM rate derived from 16 MHz; tick-stamped channel events; no float and no filter in the core; host resampling outside the core; original tests for channels 1 to 3 and the stereo bits.
- **Addresses:** HW-11, CH-04 (contract present from phase 1 with silence; emulation here), DF-02 (audio stream).
- **Avoids:** P15, P8 (link semantics), P22.
- **Checkable artifact:** `audio` (6) and `audio2` (7) pass; PCM digest is repeatable; the homebrew plays a tone on input. **This is the playable checkpoint** (see below).
- **Research flag: deeper research needed:** no sourced register layout (LFSR taps; audio CTLB nibble); lower-nibble bug is marked "verify" only; output rate choice (AD-07); low-pass figures conflict (ARCHITECTURE reports about 4 kHz and about 4.5 kHz from Epyx at LOW confidence; Gearlynx's 3.5 kHz is its own presentation choice).

### Phase 7: Single-console UART

- **Delivers:** SERCTL and SERDAT; timer 4 baud; TX and RX shift with an 11-bit frame and parity; loopback echo; level-sensitive IRQ; the wire modelled as a scheduler device with a logical port. Single console only.
- **Addresses:** HW-12.
- **Avoids:** P16, P21 (visible skips).
- **Checkable artifact:** `uart` (6), `uart2` (5) and `uart3` (6) pass; `uart4` is listed as unsupported (needs two consoles), a visible skip.
- **Research flag: moderate:** `uart3` behaviour (echo timing, overrun) comes from one maintainer's guide; verify against the suite source.

### Phase 8: Suzy bus-master shell, math and cartridge port

- **Rationale:** Suzy and the cartridge share RCART and the bus, and arbitration is the costliest retrofit (P9 and P17 are rated critical). The sprite painter follows.
- **Delivers:** Suzy register file (SUZYBUSEN, SPRGO and SPRSYS); bus-master FSM (request, grant, pause, sticky done latch, SDONEACK, CPUSLEEP coupling); raw RAM view for Suzy; math unit (signed and unsigned multiply, accumulate, divide, busy time, sign latch, write-order sensitivity, mirror aliasing at `$FC40` to `$FC6F`); cartridge address port (strobe, shift register, counter, driven by Mikey) and data port (RCART0 and RCART1 with cost and side effects) as separate components; counted diagnostics for software that violates documented rules.
- **Addresses:** HW-04 (mirror), HW-06, HW-10, HW-09 (shell), CT-04 (port level).
- **Avoids:** P9, P10, P14, P17.
- **Checkable artifact:** `memio` (3), `math` (8) and `sdoneack` (10) pass; an original cartridge shift-and-counter test passes; mirror-write negative control.
- **Research flag: deepest:** Epyx Appendix 7 (the bus state machine) was not obtained; Suzy grant latency and the ambiguous done or grant edge; SPRGO with SUZYBUSEN clear; cartridge counter width (11 or 12 bits).

### Phase 9: Sprite painter, then sprite timing

- **Delivers:** pure painter for literal and packed sprites; normal, background, XOR and boundary types; clipping; collision buffer and depository; RLE and linked SCBs; scale, stretch and tilt. Then the timing shell (phase costs and DMA interplay). A quirk ledger with tests for documented bugs that no suite covers, each labelled as documented hardware behaviour: the palette-loader page break at `xxFA`, SPRCTL1 algorithm 3, and SCBs in page 0.
- **Addresses:** HW-09 (full), CH-01 (sprite outputs).
- **Avoids:** P13, P9, P23 (correctness before performance).
- **Checkable artifact:** `sprites3` CRCs pass; then `sprites1`, `sprites2`, `sprites4` and `sprites5` timing; named-fault controls; a sprite-heavy named workload with a constant digest before any optimisation.
- **Research flag: deepest:** `lynx-sprite-performance.md` (about 59 KB) is the primary map. Several bugs have no suite coverage. Also O7 (the `sprites2` literal).

### Phase 10: Cartridge boot path (`.lnx`), with owner-gated boot shim

- **Delivers:** strict 64-byte `.lnx` parser with bounds checks; refuses unknown or short headers and unknown EEPROM kinds; rotation and spare-byte fields advisory only; content identity = hash of normalised payload plus declared configuration; bank and page-size handling; tier B: project-authored 512-byte boot shim that reads the first block through the real cartridge port, applied to a project container with an unsigned first block (O2); cc65 stage-1 substitution only as a labelled `startup=cc65-stage1-substitute` path, and only with owner approval; block-boundary tests; post-boot state record hashed into identity.
- **Addresses:** CT-01, CT-02 (refused by default), CT-04 (full), CT-05, CT-07 (slot, optional), CH-06.
- **Avoids:** P1, P2, P17, P18, P3.
- **Checkable artifact:** an original cartridge image boots through tier B and runs an original diagnostic; header refusal vectors pass; identity digest is stable; block-boundary test passes.
- **Research flag: deeper research needed:** boot policy (what the real ROM leaves behind, and what the cc65 loader relies on); header dialects (Disagreement 13); cartridge counter width.
- **OWNER gates:** O2 (shipping a boot shim), O3 and O4 (not needed if O2 is approved). **If O2 is not approved, this phase is blocked for `.lnx` boot.** The cartridge path then proceeds with the RAM-load container only.

### Phase 11: EEPROM and persistence boundary; save-state hardening

- **Delivers:** 93Cx6 state machine driven through cartridge I/O (IODAT, AUDIN and strobes); EEPROM kind as explicit configuration (no game database); persistence as host-owned bytes keyed by content identity plus hardware configuration; snapshot includes media; restore validates the envelope before swap; fresh-process save and restore round-trip.
- **Addresses:** CT-06, CH-05, CT-05 (persistence key).
- **Avoids:** P18, P3 (no saves in CI or caches), P17.
- **Checkable artifact:** EEPROM command-vector diagnostic passes; save and restore in a fresh process gives identical digests; identity-mismatch refusal.
- **Research flag: moderate:** EEPROM kinds and wiring (the 93C46, 56, 66 and 86 list comes from Gearlynx; confirm on the pinned cc65 EEPROM sources); organisation bit; DI and DO shared on AUDIN.

### Phase 12: Host integration: packaged consumer and LibFuturo adapter

- **Delivers:** installed-host consumer built from the public ABI only; LibFuturo adapter mapping one frame advance to `advance` with a tick cap and stop-on-frame, reporting BUDGET_REACHED for frame-less guests; Lynx profile documentation. No ABI promise.
- **Addresses:** DF-10, VF-06, CH-01.
- **Avoids:** P28, P22.
- **Checkable artifact:** a clean install of the consumer renders frames and audio from a pinned diagnostic; adapter round-trip test.
- **Research flag:** standard pattern for the consumer. LibFuturo fit cannot be checked publicly (owner-internal).
- **OWNER gates:** O9 for any LibFuturo format change. Any change to the shared contract (for example a new tick-unit enum) is an owner batch.

### Phase 13: CI hardening, performance baselines and second-model qualification (later)

- **Delivers:** CI matrix expansion only where a distinct failure class is shown (Disagreement 6); performance baselines with constant digests for named workloads (emulator throughput, end-to-end latency and CI wall time reported separately); the second model's evidence plan (O13); tier C route only with owner approval (O3); trace breakpoints and status snapshots (DF-05); two-instance design only if O12 says so.
- **Addresses:** VF-02 and VF-03 expansion, DF-05, DF-09, CH-05 (extended).
- **Checkable artifact:** benchmark reports with unchanged digests; second-model suite run with its own inventory.

### Phase ordering rationale

- The dependency spine is timebase, then bus and RAM, then CPU, then timers and IRQ. Nothing else can be timed correctly without it (ARCHITECTURE; FEATURES dependency graph).
- Timers come before display because frames are timer-driven, IRQ and sleep are shared with Suzy, and the `cpu` suite needs Timer IRQs.
- Display comes before Suzy so that visible output exists and display DMA exists when sprite timing is measured.
- Audio and UART depend only on timers, so they can run in parallel with Suzy after phase 4.
- The bus master and cartridge port come before the sprite painter, because arbitration and the cartridge are where rework is costliest.
- EEPROM follows the cartridge. Host integration follows the stable core, so that no ABI is frozen before a real consumer round-trips (P28).
- No suite is claimed as passing before O1 is decided, though the tables are built earlier.
- Visibility is preserved by the phase 3 snapshot and the phase 5 live frame, which addresses P25 (research-only drift) early.

### Research flags, by phase

- **Deeper research before planning the phase:** 2 (vector scope; decimal and reserved-opcode cycles), 3 (preset provenance; `lynx-bll` spike), 4 (verbatim Epyx timer and IRQ text; `timers2`; `cpu` IRQ quirks), 5 (DMA and refresh constants; frame definition; palette contract), 6 (audio register layout), 8 (Suzy bus state machine; grant latency), 9 (sprite timing; deepest), 10 (boot policy; header dialects; counter width), 11 (EEPROM wiring).
- **Moderate:** 7 (`uart3` behaviour), 11 (EEPROM kinds), 12 (LibFuturo fit).
- **Standard patterns:** 1 (except the cc65 build spike), 12 (consumer packaging), and the CI parts of 13.

### Playable checkpoints

- **Phase 3:** visible snapshot. Evidence-light, not playable.
- **Phase 5 (target):** input-responsive original homebrew frame, direct-load.
- **Phase 6 (latest acceptable):** same homebrew with sound. This is the playable checkpoint.
- **Phases 9 and 10:** sprite demo and cartridge boot path upgrade the scenario. Commercial-game progress stays out of the early plan (rights; no clean path), as PITFALLS P25 recommends.

---

## Confidence Assessment

| Area | Confidence | Basis and notes |
|---|---|---|
| Stack | **HIGH** for language, build, pins and fixture identity (primary files read; commits named). **MEDIUM** for the cc65 RAM-load recipe (not yet built; `TARGETS=lynx` recipe and build time unconfirmed). **LOW** for the "free" boot ROM (provenance and licence unknown). | STACK.md; its own rating is HIGH on language, build and fixture facts. |
| Features | **MEDIUM-HIGH** for the catalogue (FEATURES' own rating). **MEDIUM** for Epyx-derived hardware facts (summarised fetch). **LOW** for the first-light slice detail (judgement, not tested). Complexity ratings are estimates. | FEATURES.md |
| Architecture | **MEDIUM** overall. **HIGH** on decomposition and on the timing hazards, where several independent sources agree. **MEDIUM** on exact tick costs. **LOW** on boot-ROM internals and post-boot state. One internal inconsistency (frame definition, Disagreement 10). | ARCHITECTURE.md |
| Pitfalls | **MEDIUM** overall. **HIGH** on repository facts (suite source, cc65 source). **MEDIUM-LOW** on Epyx pages (summarised). **LOW** on forum-sourced boot details. Several counts differ across files (Disagreement 15). | PITFALLS.md |
| **Overall** | **MEDIUM** | Strong agreement on the architecture and on startup tier A. Open on the baseline model, the boot path and Suzy timing. Nothing executed. |

### Gaps to address

1. **Baseline profile (O1).** No source settles it. Handle: build both tables; record the detected model in every run; make no claim until the owner decides.
2. **No rights-clean boot ROM has been validated.** Handle: tier A now; tier B only after O2; HLE or a free ROM only after O4. Do not use Holani's embedded ROM.
3. **The cc65 recipe and per-suite behaviour are unexecuted:** whether `TARGETS=lynx` builds, how long it takes, whether `.sym` files and `g_results` exist in every pinned suite, completion addresses, and which result bytes depend on the model. Handle: phase 1 spike; per-suite checks in phase 4.
4. **The RAM-load route is unvalidated:** whether lynx-tests relinks with `lynx-bll.cfg`; whether that output equals the BS93 `.o` format; whether results hold without the ROM-left state. Handle: phase 3 spike. Label any such result `startup=A` and not boot evidence.
5. **Epyx facts are not verbatim.** Re-read the scanned Handy specification PDF (`atarilynxdev.net`). The Appendix 7 bus state machine was not obtained, and no audio register page was found. Handle: before any requirement cites an Epyx fact.
6. **Per-register preset provenance.** Handle: cite or leave at reset; the DISPCTL disagreement is recorded in phase 3.
7. **Unsourced hardware constants:** DMA burst timing; refresh period and cost (OPEN); decimal-mode cycles; reserved-NOP cycle counts; whether RMB and BBR bits 1 to 7 behave like bit 0; Lynx I and II differences beyond the bit operations (AUDIN reset level, MAPCTL read-back, stereo). Handle: mark unverified; drive each by a test.
8. **Cartridge details:** counter width (11 or 12 bits); header dialect bytes; LNX2; EEPROM kinds. Handle: phase 10 and 11 research; strict refusal meanwhile.
9. **Audio:** LFSR and register layout; channels 1 to 3 are not covered by the suites; low-pass figure; output rate. Handle: phase 6 research; original tests for channels 1 to 3.
10. **Suite evidence quality.** The author also wrote an emulator that claims to pass; several records lack a hardware model; some results self-judge (`timers2` treats 0 as PASS); many results are zero. Handle: generated expectations, sentinel, no-run control, model assertion, and an evidence schema that records model, repeats and independence.
11. **LibFuturo fit** cannot be checked publicly. Handle: O9 and phase 12. Do not freeze an ABI.
12. **Research exposure to GPL sources** (O6). Handle: record which sources each research phase consulted, as PITFALLS P4 recommends.
13. **Counts** (Disagreement 15). Handle: verify per pinned suite in phase 4.

---

## Sources

**Primary (read directly at the pinned revision, 2026-11-09; HIGH for what the file says):**
- cc65 at `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2` (2026-11-08, Zlib): `cfg/lynx.cfg`, `cfg/lynx-bll.cfg`, `libsrc/lynx/bootldr.s` (loader comments; attributed to a 2011 author), `libsrc/lynx/exehdr.s`, `libsrc/lynx/crt0.s`, `asminc/lynx.inc`, `src/common/target.c`, `src/common/cpu.c`, `doc/lynx.sgml`, `doc/ca65.sgml`. Documentation page `cc65.github.io/doc/lynx.html` (names no CPU).
- drhelius/lynx-tests, tag 1.4.9 = `b583dc43d532d3c41563a6fa35420614fbcead89` (2026-08-13), and HEAD `a920f01164d37edff94939993fbc0551e5b7000b` (2026-09-20), MIT: `README.md`, `LICENSE`, `build-all.sh`, `shared/lynxcc65.mak`, `shared/util.h`, `cpu/main.c`, `cpu/tests.s`, `sprites2/tests.s`, `math/main.c`, `page-mode/main.c`, `timers2/main.c`, `uart4/main.c`, `.github/workflows/build.yml`, `lynx-page-mode.md`, `lynx-timers.md`, `lynx-video-timing.md`, `lynx-sprite-performance.md`.
- SingleStepTests/65x02 (MIT; repository listing and README; last push 2025-05-10).
- Sibling precedent, read locally and used as precedent only (not requirements): `~/projects/libfuturo` (`docs/contract.md`, `include/libfuturo/libfuturo.h`, `scripts/check.sh`, `docs/development.md`); `~/projects/muhtari` (`pins.txt`, `scripts/fetch-pin.sh`, `.github/workflows/ci.yml`, `ASSET_POLICY.md`, `quality.json`); nesturbator, gabbaboy and genewhip `CMakeLists.txt`.
- Reference only: `42Bastian/new_bll` (Apache-2.0); `atarilynx/cc65-lynx-gametemplate` (CC0-1.0).

**Secondary (summarised fetch; MEDIUM at best until checked verbatim):**
- Epyx/Handy specification mirror at `monlynx.de` (lynx2 to lynx11, irq, hardware, sprite pages) and the `atarilynxdev.net` index. The scanned PDF was not read verbatim.
- libretro documentation (Handy, Beetle Lynx, Gearlynx, Holani pages); Mednafen Lynx documentation.
- Behaviour cross-checks only, never code sources: Gearlynx at `94068da` (GPL-3.0); libretro-handy at `bc55d46` (zlib); MAME `src/mame/atari/lynx.cpp` (GPL-2.0+); Holani (GPL-3.0; README, `src/rom.rs`, `src/lynx.rs`).

**Tertiary (LOW; leads only):**
- AtariAge threads (returned HTTP 403; known only from search snippets); Recalbox and RetroPie notes; hobbyist hardware notes (cartridge counter width); Atari Lynx Vault hardware notes.
- Historical context only: Atari Corporation 1990 annual report text (launch sales).

**Not used:**
- No-licence repositories `atarilynx/handy-examples`, `handy-bootloaders`, `handy-encryption`, `handy-docs`, `emulator`.
- Klaus Dormann 6502 and 65C02 functional tests (GPL-3.0).
- The lynx-tests 1.4.9 release zip (comparison target only, never a committed fixture).
- Holani's embedded "free" boot ROM (provenance and licence unknown).
- Commercial ROMs and BIOS images: none downloaded.
- LibFuturo public sources: none found; owner-internal sources only.

---
*Research completed: 2026-11-09*
