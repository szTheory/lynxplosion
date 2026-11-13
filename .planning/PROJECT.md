# Lynxplosion

## What This Is

Lynxplosion (owner-selected working name; not trademark clearance) is a new
Atari Lynx emulator aimed at accuracy, reproducibility and usefulness. It
targets clearly named Lynx hardware and cartridge profiles through one
canonical, deterministic, headless-drivable machine core, so behavior can be
tested and observed separately from any host frontend. It is built for the
owner's emulator portfolio as an automation-first breadth and learning
project, and for hosts (including, possibly, LibFuturo consumers) that need a
well-specified core. It is not positioned as a proven demand-led product.

## Core Value

Every compatibility claim is tied to an exact, named Lynx model/configuration
and to reproducible evidence from a deterministic, observable machine. If
everything else fails, that link between claim, profile and evidence must hold.

## Requirements

### Validated

- ✓ One documented local check command aligned with the fast PR gate — Phase 1 (`scripts/check.sh`, same command in CI; CI green on the pushed HEAD)
- ✓ Exact test inventories, named-fault negative controls and visible skips/missing/stale evidence — Phase 1 (57 tests accounted for once each; every gate has a seeded control)
- ✓ A canonical machine-core skeleton behind a host API with instance ownership, bounded guest-time advancement, reset, errors and refusal postconditions, validated by whole-buffer comparison — Phase 1 (logical input, CPU, devices and raw outputs come in later phases)
- ✓ A 65C02-class CPU as data-defined Lynx I and Lynx II profiles (plus the default `none` = what both agree on), selectable per instance, every memory access a tick-stamped bus cycle with a variable cost model, proven by 7.3 million pinned per-opcode vectors (0 failed), exact per-profile inventories and named-fault controls; Lynx-specific cycle counts stay labelled unverified (Epyx's VTI spec is not public) and no baseline is adopted — Phase 2
- ✓ Repeatable runs: two fresh runs and two in-process instances give identical artifact-bundle digests, labelled repeatability, not accuracy — Phase 1

- ✓ Strict BS93/BLL startup tier A on the MAPCTL decoder, full-object identity, pure raw/mapped inspection and explicit unavailable firmware/device routes — Phase 3; timing/readback policies remain labelled unverified.
- ✓ Original source-built diagnostic reports semantic progress/pass/fail through linked-symbol RAM results; post-write watches and snapshot continuation preserve common-tick repeatability — Phase 3.
- ✓ Independent public-API consumer produces the labelled 8x8 diagnostic RAM snapshot and valid placeholder-silence WAV with exclusive whole-directory publication and interruption controls — Phase 3 slice; real frames/audio are supplied by the separate Phase5–6 replay modes below.

- ✓ Instance-owned master-tick timer scheduling, stable same-tick ordering, IRQ latching, a private bus-hold seam and format-4 snapshot continuation — Phase 4; fixed phase and bus pricing remain unverified software policies.
- ✓ Pinned, unchanged public CPU/timer sources with symbol-derived result capture, exact identities and named-fault controls — Phase 4 observation slice. CPU source comparisons agree; timer offset 14 remains explicitly pending under D-10, with strict comparison still failing.

- ✓ Bounded raw-frame/input API and original input-responsive homebrew replay,
  independently checked from raw pens, palettes, line ticks and rendered PPM —
  Phase5; all raster/bus policies remain unverified physical behavior.
- ✓ Four-channel digital stereo PCM, tick-stamped events, bounded timer/audio
  links, format7 continuation and a public-only A/V consumer with independent
  frame/WAV/event/input/progress checks — Phase6. Scoped Phase6 software checks
  pass native/GNU/hosted qualification; strict timer EVD-02 and SERCTL missing
  observations remain explicit limits.
- ✓ Named full homebrew workload with five equal same-build output captures and
  separate public-API/host/CI timing — Phase6; no core optimization or hardware
  accuracy claim.

### Active

All items are hypotheses until shipped and verified. Formal, numbered
requirements live in `.planning/REQUIREMENTS.md`.

- [ ] One explicitly named baseline Lynx model/revision and CPU profile, chosen
      from cited evidence (Epyx spec says 65C02; cc65's target table in
      `src/common/target.c` maps lynx to CPU_65SC02 — its Lynx doc page names
      no CPU; the lynx-tests `cpu` suite expects RMB/SMB/BBx to be NOPs on
      Lynx I and implemented on Lynx II)
- [ ] A separately evaluated owner-supplied firmware route outside source control, only after owner authority; the firmware-free tier-A path is validated in Phase 3
- [ ] One named cartridge path (container format, loader, banking/addressing
      behavior) with explicit content identity
- [x] A canonical machine core with defined instance ownership, content
      load/unload, logical input sampling, bounded guest-time advancement,
      raw outputs, reset, errors and refusal postconditions
- [x] Deterministic execution: fresh isolated state + fixed logical input +
      fixed guest-time interval yields identical observable results
- [x] Separate, connected observations for CPU/device state, guest semantic
      progress, raw video/palette and raw audio samples/events
- [x] A headless runner and an independent consumer that drive the core
      without host clocks, filesystems, windows or A/V devices inside machine
      logic
- [x] Source-backed component tests, then a source-built original or
      rights-reviewed diagnostic fixture with a manifest (source, recipe,
      checksum, license/notices, allowed use)
- [x] Exact test inventories, named-fault negative controls and visible
      skips/missing/stale evidence
- [x] One documented local check command aligned with a fast PR gate
- [x] A playable or homebrew scenario visible early so the project does not
      become research-only
- [x] Representative named workloads and a correctness baseline before any
      performance optimization

### Out of Scope

For the initial project definition. Each can return through a later milestone
with its own evidence plan.

- Committing or redistributing BIOS images, commercial ROMs, saves, manuals or
  other rights-bearing assets — no documented permission (owner policy)
- Public release, firmware distribution or a licence decision — explicit owner
  boundaries. The repository itself is public (owner decision, 2026-11-09), with
  no PII or credentials; publishing a release or choosing a licence still needs the owner
- Physical hardware qualification (Lynx I/II comparison, refresh-rate
  logic-analyzer calibration, two-console ComLynx cable tests) — requires
  hardware access and owner authority
- Promising Lynx I *and* II, all cartridge hardware, EEPROM, ComLynx or
  accessories up front — must not be claimed merely because other emulators
  support them; second model is qualified separately
- Copying code or assets from Gearlynx (GPL-3.0), MAME or other emulators —
  they are behavior cross-checks, not code sources or hardware oracles
- Marketing Lynx as the most-demanded next console — no current player-use
  evidence supports it

## Context

### Owner direction (from AGENTS.md and the seed, 2026-11-09)

- Aim for a high-quality, accurate, useful Lynx emulator; a narrow first
  delivery is sequencing, not a ceiling.
- Proceed autonomously on routine, reversible work. Bring the owner only
  choices that materially change scope, rights, authority, risk, cost or an
  irreversible action. Batch unavoidable questions.
- Keep owner decisions, measured results, source observations, reports,
  inference, recommendations and unknowns distinct.
- Every milestone discussion fans out across hardware, CPU/bus, video, sound,
  input, product, API/host, verification, security/licensing, performance and
  DevOps lenses, with an adversarial pass and a rolling roadmap.
- Small, flat dependency tree; a small copied helper can beat a new dependency.
- Milestone close: main green, PR triage, roadmap/lessons refreshed, release
  of the exact tested artifact when rights, readiness, qualification and
  authority permit. GSD milestone numbers and software versions are separate.

### Source observations (dated research input, accessed 2026-11-09; recheck before relying on them)

- Hardware centers on Mikey (CPU-facing registers, timers/interrupts, audio,
  UART, display DMA/palette) and Suzy (sprite/blitter, collisions, math unit,
  joystick, cartridge-related functions). Suzy work is asynchronous relative
  to the CPU.
- Epyx material reports a 160×102 display and a 4096-color palette with fewer
  colors on screen at once. Dimensions and pixel conventions need confirming
  before an output contract is frozen.
- The physical handheld has a 512-byte onboard boot ROM. Gearlynx reports
  needing a BIOS image. cc65 documents `cl65 -t lynx` producing a bootable
  `.lnx` image with its minimal loader. None of these settles a boot policy.
- Correction (project research, 2026-11-09): a cc65 `.lnx` begins with an
  encrypted/signed first-stage loader that the boot ROM decrypts, so it cannot
  start by copying bank 0 into RAM. Rights-clean startup candidates are a
  direct RAM load (BLL/BS93 `.o`) with a cited preset state, a
  project-authored boot ROM/shim, or an HLE decrypt — the last needs an owner
  rights decision. See `.planning/research/`.
- Correction (project research, 2026-11-09): lynx-tests suites store raw
  results in a `g_results` RAM array whose address is in the build's `.sym`
  file, so results can be captured without OCR (needs a completion sentinel
  and negative controls; many PASS values are 0x00).
- Cartridges may include address-counter/shift-register and banked behavior,
  and writable elements/EEPROM that affect saves; they are not necessarily
  flat ROM arrays.
- drhelius/lynx-tests is MIT-marked and describes hardware-informed suites
  for CPU, MMIO, math, audio, timers, CPU/Suzy sleep and bus coordination,
  sprites, page mode, display timing/DMA and UART. Most suites show status and
  diagnostic bytes on screen; machine-readable output is not assumed.
- Timing depends on page-mode fetches, timer phase, display DMA, interrupts,
  waits and shared-bus ownership; a fixed instructions-per-frame model is not
  safe.
- LibFuturo is an experimental source API; its reviewed profiles do not
  include Lynx.

### Inference and recommendations (not decisions)

- Recommended sequence: name one model/CPU profile and one cartridge path,
  resolve startup, then build a small end-to-end slice — source-built
  diagnostic, deterministic input/time, fresh state, headless runner,
  independent consumer, with semantic success plus raw video/audio captured
  separately.
- Evidence tiers are distinct and do not imply each other: spec/vector
  expectations, public hardware-informed tests, original diagnostics,
  independent emulator comparison, measured hardware, repeatability, game
  progress, installed-host behavior.

### Environment observations (2026-11-09)

- Repository was empty apart from bootstrap material; git initialized during
  project init. No cc65 toolchain installed on the development host.
- `.planning/config.json` was prepared by the owner before init and refers to
  `.planning/AUTONOMY.md`, which does not exist yet.

## Constraints

- **Rights**: No copyrighted BIOS or commercial game bytes in source control or
  shared CI — owner policy; a hash establishes identity, not rights.
- **Determinism**: Machine behavior is independent of host clocks, filesystems,
  physical controller discovery, windows, A/V presentation and save policy —
  required for reproducible evidence.
- **Evidence**: Compatibility claims name an exact model/configuration and cite
  evidence — core value.
- **Dependencies**: Small, flat dependency surface; each dependency must justify
  its lifetime and supply-chain cost — owner policy.
- **Fixtures**: Pinned toolchain and test-suite revisions; each fixture has a
  manifest (source, recipe, checksum, license, allowed use) — reproducibility
  and rights.
- **Authority**: Public publication, protected firmware use and physical
  hardware qualification need an explicit owner decision — owner boundary.
- **Timing**: Core work unit chosen only after studying page-mode fetches,
  timers, interrupts, DMA, CPU stalls and Suzy bus ownership — correctness.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Working name "Lynxplosion" (owner) | Owner-selected; not trademark clearance | ✓ Owner decision |
| Rights boundary: no BIOS/commercial ROM bytes in repo or shared CI (owner) | Rights-bearing content | ✓ Owner decision |
| No public publication, firmware distribution or physical testing without explicit owner approval (owner) | Authority boundary | ✓ Owner decision |
| Baseline Lynx model/revision and CPU profile | Needs cited evidence; affects CPU tests | — Open |
| Boot/firmware policy (independent startup, owner-supplied BIOS, or both) | Determines unattended CI path | — Open |
| Cartridge/container format, banking, EEPROM/identity | Defines content contract | — Open |
| Timing representation and advancement unit | 16 MHz master-tick budgets; Phase 4 implements timer/IRQ ordering and private hold. Fixed phase and flat5 prices are unverified; video DMA/audio arbitration remain later work | Partially validated (software policy) |
| Implementation language, build tooling, API/ABI: C17 (`CMAKE_C_EXTENSIONS OFF`, `-pedantic-errors`), CMake 3.25+ workflow presets, Ninja, CTest, C ABI with opaque handle | Owner accepted research default (O8), 2026-11-09 | ✓ Owner decision |
| Repository visibility: public at github.com/szTheory/lynxplosion; no PII or credentials (owner, 2026-11-09) | Owner authority; recorded here so scans and CI assume a public tree | ✓ Owner decision |
| Project licence (O5) and release route | Owner authority; no licence file until decided | — Open (owner); all rights reserved until decided |
| Clean-room protocol (O6) | Rights boundary for emulator-source exposure | — Draft policy in docs/policy/clean-room.md; owner decision pending |
| D-10 timer comparison disposition (owner, 2026-11-11) | Preserve unchanged expected 0D versus observed 0C at offset 14 only; all other comparisons and controls required; revisit with Phase 5 startup/bus/video evidence | ✓ Observation slice approved; strict agreement open |
| LibFuturo fit | Needs a real core/consumer test | — Open |
| ComLynx/accessories timing in roadmap | Scope/cost | — Open |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-11-11 after Phase 4*
