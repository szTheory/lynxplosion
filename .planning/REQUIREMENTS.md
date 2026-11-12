# Requirements: Lynxplosion

**Defined:** 2026-11-09
**Core Value:** Every compatibility claim is tied to an exact, named Lynx model/configuration and to reproducible evidence from a deterministic, observable machine.

**Scope of v1 (first milestone):** a rights-clean, deterministic, observable Lynx core
from evidence harness through CPU, memory map, direct-load startup, timers/IRQ,
display, input and audio, ending in an original, input-responsive homebrew with
sound run headlessly and rendered by an independent consumer. Derived from
`.planning/research/SUMMARY.md` (suggested phases 1–6). v1 is delivery
sequence, not a ceiling on the project goal.

**Reading rules:**
- Requirements are hypotheses until verified. They do not decide owner-level
  questions; where one depends on an open owner decision, it names it (O-numbers
  refer to "Owner decisions pending" below and to SUMMARY.md).
- "Profile" means a data-defined Lynx model/CPU profile. No requirement adopts
  Lynx I or Lynx II as baseline (O1 is open). Claims of a suite passing name
  the profile and are not made before O1 is decided.
- "Startup tier A" means direct RAM load of an image with a documented preset
  state; it is never boot, cartridge or loader evidence.

## v1 Requirements

### Foundation and evidence harness

- [x] **FND-01**: Developer runs one documented local command (`scripts/check.sh`) that builds, lints and runs the full fast test set, and the same command is the PR CI gate
- [x] **FND-02**: Every external source (cc65, lynx-tests, CPU vectors) is fetched from a pinned revision and verified by SHA-256 before use; a mismatched hash fails the build
- [x] **FND-03**: Every fixture has a manifest recording source, revision, build recipe, SHA-256, license and notices, allowed use, startup tier, profile and evidence tier
- [x] **FND-04**: CI and the local check fail closed when a firmware-like or unmanifested binary appears in the tree or in build artifacts, proven by a seeded fake 512-byte file that the check rejects
- [x] **FND-05**: The core is built without access to host clocks, filesystem, threads, floating point or mutable globals, enforced by a build-time forbidden-import check with a negative control
- [x] **FND-06**: The test inventory is exact: the check compares the executed test list to an expected list, and any skipped, missing or extra test is reported and fails unless explicitly declared as a visible skip
- [x] **FND-07**: A source-exposure record and draft clean-room policy document which reference sources (including GPL emulators read during research) were consulted, pending owner decision O6
- [x] **FND-08**: A claim template binds every compatibility claim to profile, content identity, startup tier, evidence tier and test inventory
- [x] **FND-09**: cc65 builds reproducibly from the pinned source commit on the dev host and the CI runner, with wall time recorded

### Core API and host boundary

- [x] **API-01**: Host can create and destroy independent machine instances that share no mutable state
- [x] **API-02**: Host can load content as bytes; a refused load returns a status code and leaves any existing session exactly unchanged
- [x] **API-03**: Host can advance the machine by a bounded master-tick budget and receives ticks consumed and a stop reason (budget, frame, watch hit, halt)
- [x] **API-04**: Host can reset the machine to a declared power-on state, including a configurable RAM power-on policy whose seed is recorded in run identity
- [x] **API-05**: Host can inspect registers and memory through a side-effect-free path that never changes machine state or guest time
- [ ] **API-06**: Host supplies logical input as a timeline sampled at defined guest-time boundaries; no physical controller discovery happens in the core
- [x] **API-07**: Host can capture a snapshot and restore it in the same build; restore validates identity before replacing state and refuses mismatches
- [x] **API-08**: Host can register a memory watch that stops advancement on write without altering machine behavior

### Determinism and observation

- [x] **DET-01**: Two runs from fresh state with the same content, profile, input timeline and tick budget produce identical artifact-bundle digests
- [x] **DET-02**: Two instances run in the same process produce the same digests as separate runs
- [x] **DET-03**: A run that captures a snapshot mid-way, restores it and continues produces the same digests as an uninterrupted run
- [x] **DET-04**: Every run writes a manifest recording profile, startup tier, content identity, input digest, tick count and core build identity
- [ ] **DET-05**: Machine progress, logical input, raw video and raw audio are recorded as separate streams on one guest-tick timeline; same-core digests are labelled repeatability, not accuracy

### CPU

- [x] **CPU-01**: The 65C02-class CPU passes pinned per-opcode functional vectors for each defined profile, with an exact per-profile inventory
- [x] **CPU-02**: Lynx I and Lynx II CPU profiles exist as data (including RMB/SMB/BBR/BBS behavior), selectable per instance, with neither adopted as baseline until O1
- [x] **CPU-03**: All CPU memory accesses go through a tick-stamped bus-cycle interface so access costs (page-mode, normal, device) can vary per access
- [x] **CPU-04**: IRQ entry, CLI/SEI latency, WAI/STP and reserved-opcode NOP lengths follow the selected profile's documented behavior
- [x] **CPU-05**: A named-fault control (wrong profile selected) fails the bit-operation vectors

### Memory map and startup

- [x] **MEM-01**: The 64 KB memory map decodes Suzy, Mikey, ROM overlay and vector space under MAPCTL, with a CPU-mapped view and a separate raw RAM view
- [x] **MEM-02**: Content can start through startup tier A: a direct-load image placed in RAM with a preset state in which every preset value cites its source or is left at power-on reset
- [x] **MEM-03**: Every run is tagged with its startup strategy (`startup=A` for v1) in its manifest and in any claim
- [x] **MEM-04**: The core accepts no firmware in v1; any firmware input path stays absent or disabled until owner decision O3

### Timers, interrupts and scheduling

- [x] **TIM-01**: One master-tick clock and a scheduler with a total (tick, priority, sequence) order drive all devices
- [x] **TIM-02**: Mikey timers form one shared-prescaler bank with linking, DONE, reset-done and borrow semantics
- [x] **TIM-03**: Timer interrupts set pending bits separately from enables, and the CPU samples IRQs per the profile's rules
- [x] **TIM-04**: A minimal CPU-sleep and bus-request model exists so later Suzy arbitration does not require rework of the CPU or timers

### Display and input

- [ ] **VID-01**: Timers 0 and 2 drive a line and frame generator with programmable timing (not a fixed 60 Hz)
- [x] **VID-02**: Display DMA reads the framebuffer as a bus requestor that costs ticks and interacts with page-mode fetches
- [ ] **VID-03**: The 16-entry, 12-bit palette is emulated with its per-line latch behavior
- [ ] **VID-04**: Host receives raw video carrying pen data, palette state per line and timing metadata, separate from any scaling, rotation or color presentation (canonical form decided in-phase; LibFuturo implications are owner decision O9)
- [ ] **VID-05**: Host can stop advancement on a defined frame-complete event, with the event definition documented
- [ ] **INP-01**: The joystick and switch registers reflect the logical input timeline (d-pad, A, B, Option 1, Option 2, Pause) at their sampled guest time

### Audio

- [ ] **AUD-01**: Four audio channels (LFSR, integrator, reload, volume, link to timers) and stereo muting are emulated
- [ ] **AUD-02**: Host receives integer PCM at an exact rational rate derived from the 16 MHz clock, plus tick-stamped channel events; no float, filter or resampling inside the core

### Test fixtures and evidence

- [x] **EVD-01**: An original, project-authored diagnostic built with the pinned cc65 reports progress and pass/fail through a RAM result block (magic, schema, progress, error, done) read by symbol
- [ ] **EVD-02**: A lynx-tests harness reads `g_results` by symbol from the build's label file, requires a completion sentinel and a guest-time minimum and bound, and compares against expectations generated from the pinned source with a count check
  - Owner-approved 2026-11-11 split: Phase 4 completes the harness/control slice;
    strict agreement for timer result offset 14 (0C versus unchanged 0D) stays
    explicitly pending for Phase 5's startup/bus/video evidence. Global EVD-02
    remains open; no other comparison or control is deferred.
- [x] **EVD-03**: A no-run negative control (CPU halted before tests start) makes every suite fail in the harness
- [x] **EVD-04**: The harness asserts which model a suite detected and fails when it does not match the selected profile
- [ ] **EVD-05**: Each device phase adds at least one named-fault control that is shown to fail the intended test
  - Recurring obligation: Phase 4 coverage is completed by its own named-fault
    work; Phase 5 DMA and Phase 6 audio controls already required by the roadmap
    remain outstanding. A completed Phase 4 slice does not close all phases.
- [ ] **EVD-06**: The lynx-tests `timers`, `cpu`, `page-mode`, `audio` and `audio2` suites run headlessly from pinned, source-built, startup-tier-A images with results captured and inventoried; any suite that cannot run that way is a visible, documented skip with its blocker

### Playable scenario and consumer

- [x] **HB-01**: A tick-stamped framebuffer snapshot from an original diagnostic is rendered to a viewable image early, labelled as a snapshot, not a frame
- [ ] **HB-02**: An original homebrew direct-load image shows input-responsive frames when driven by a recorded input timeline
- [ ] **HB-03**: The homebrew plays sound in response to input (the v1 playable checkpoint)
- [ ] **CON-01**: An independent consumer program, using only the public C API, drives the core and writes raw frames as PNG or PPM and audio as WAV
- [ ] **PERF-01**: A named workload (the homebrew with a fixed input timeline) has a recorded throughput baseline with its output digest held constant; no optimization in v1

## v2 Requirements

Deferred to later milestones. Tracked, not in the current roadmap.

### Serial

- **UART-01**: Single-console UART (SERCTL/SERDAT, timer-4 baud, framing, parity, loopback, IRQ) passes `uart`, `uart2`, `uart3`
- **UART-02**: Multi-instance ComLynx link between emulated consoles (owner decision O12)

### Suzy

- **SUZ-01**: Suzy bus-master state machine (request, grant, done latch, SDONEACK, CPUSLEEP) passes `sdoneack`
- **SUZ-02**: Math unit passes `math`; register mirrors pass `memio`
- **SUZ-03**: Sprite painter (literal, packed, types, clipping, collisions, scaling) passes `sprites3`
- **SUZ-04**: Sprite timing passes `sprites1`, `sprites2`, `sprites4`, `sprites5` (O7 for the `sprites2` literal)

### Cartridge and persistence

- **CART-01**: Cartridge address and data ports (shift register, counter, RCART0/1) are separate components
- **CART-02**: Strict `.lnx` parser with refusal vectors and content identity
- **CART-03**: Cartridge boot through a project-authored boot shim (owner decision O2)
- **CART-04**: 93Cx6 EEPROM as explicit configuration with host-owned persistence keyed by content plus hardware identity

### Host integration and quality

- **HOST-01**: Installed package consumed by a clean, independent installed consumer
- **HOST-02**: LibFuturo adapter after a real consumer round-trips (owner decision O9)
- **HOST-03**: Trace ring, breakpoints and status API for debugging
- **QUAL-01**: Second-model qualification with its own inventory (owner decision O13)
- **QUAL-02**: CI lane expansion only when a distinct failure class is demonstrated
- **QUAL-03**: Performance optimization of a measured bottleneck with digests held constant

## Out of Scope

| Feature | Reason |
|---------|--------|
| Bundling, auto-downloading or committing a BIOS or commercial ROM | Rights-bearing content; owner policy |
| HLE boot with decryption constants; Holani "free" boot ROM | Rights/provenance unknown; owner decision O4 |
| Owner-supplied BIOS (tier C) | Owner decision O3; not needed for v1 |
| Silent fallback between boot strategies | Hides which evidence a run represents |
| Fixed instructions-per-frame scheduling; 75 FPS as machine definition | Incorrect timing model; host convention, not hardware |
| Scaling, rotation, shaders, frameskip, overclock or button remapping inside the core | Presentation belongs to the host |
| OCR of on-screen test text as the oracle | Fragile; RAM results exist |
| Copying code, databases or boot blobs from Gearlynx, MAME, Holani, Handy | License and clean-room boundary; cross-check only |
| Claims like "passes all lynx-tests" or "whole catalogue" | Must name profile, tier and inventory |
| Physical hardware qualification (refresh-rate calibration, `uart4`, Lynx I/II comparison) | Requires hardware and owner authority (O11) |
| Public publication, release, repository visibility change | Owner authority (O10) |
| Commercial game progress tests | No rights-clean path |
| Rewind, run-ahead, cheats, netplay, achievements, GameDrive, LNX2 | Not needed for accuracy goal now; revisit later |

## Owner decisions pending

Batched for the owner; requirements stay neutral until decided.

| # | Decision | Blocks |
|---|----------|--------|
| O1 | Baseline Lynx model/CPU profile for first claims (research split: Lynx I-style vs Lynx II) | Any suite-pass claim (CPU-02, EVD-06 claims) |
| O5 | Project license | Any public artifact |
| O6 | Clean-room protocol and recording of research GPL exposure | Finalizing FND-07 |
| O8 | Implementation language/build/API (research default: C17, CMake, Ninja, CTest; C ABI) | Phase 1 code |
| O9 | Canonical raw video form if it affects LibFuturo; any LibFuturo contract change | VID-04 freeze for LibFuturo; HOST-02 |
| O2, O3, O4, O7, O10–O13 | Boot shim, owner BIOS, HLE/free ROM, `sprites2` literal, publication, hardware, ComLynx, second model | v2 items only |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| FND-01 | Phase 1 | Complete |
| FND-02 | Phase 1 | Complete |
| FND-03 | Phase 1 | Complete |
| FND-04 | Phase 1 | Complete |
| FND-05 | Phase 1 | Complete |
| FND-06 | Phase 1 | Complete |
| FND-07 | Phase 1 | Complete |
| FND-08 | Phase 1 | Complete |
| FND-09 | Phase 1 | Complete |
| API-01 | Phase 1 | Complete |
| API-02 | Phase 1 | Complete |
| API-03 | Phase 1 | Complete |
| API-04 | Phase 1 | Complete |
| API-05 | Phase 1 | Complete |
| API-06 | Phase 5 | Pending |
| API-07 | Phase 1 | Complete |
| API-08 | Phase 3 | Complete |
| DET-01 | Phase 1 | Complete |
| DET-02 | Phase 1 | Complete |
| DET-03 | Phase 3 | Complete |
| DET-04 | Phase 1 | Complete |
| DET-05 | Phase 6 | Pending |
| CPU-01 | Phase 2 | Complete |
| CPU-02 | Phase 2 | Complete |
| CPU-03 | Phase 2 | Complete |
| CPU-04 | Phase 2 | Complete |
| CPU-05 | Phase 2 | Complete |
| MEM-01 | Phase 3 | Complete |
| MEM-02 | Phase 3 | Complete |
| MEM-03 | Phase 3 | Complete |
| MEM-04 | Phase 3 | Complete |
| TIM-01 | Phase 4 | Complete |
| TIM-02 | Phase 4 | Complete |
| TIM-03 | Phase 4 | Complete |
| TIM-04 | Phase 4 | Complete |
| VID-01 | Phase 5 | Pending |
| VID-02 | Phase 5 | Complete |
| VID-03 | Phase 5 | Pending |
| VID-04 | Phase 5 | Pending |
| VID-05 | Phase 5 | Pending |
| INP-01 | Phase 5 | Pending |
| AUD-01 | Phase 6 | Pending |
| AUD-02 | Phase 6 | Pending |
| EVD-01 | Phase 3 | Complete |
| EVD-02 | Phases 4 and 5 | Phase 4 observation/control slice complete; exact timer offset-14 agreement pending Phase 5 |
| EVD-03 | Phase 4 | Complete |
| EVD-04 | Phase 4 | Complete |
| EVD-05 | Phases 4, 5 and 6 | Phase 4 slice complete; Phase 5 and 6 device-fault obligations remain open |
| EVD-06 | Phase 6 | Pending |
| HB-01 | Phase 3 | Complete |
| HB-02 | Phase 5 | Pending |
| HB-03 | Phase 6 | Pending |
| CON-01 | Phases 3, 5, 6 | Phase 3 snapshot/silence slice complete; actual frames/audio pending |
| PERF-01 | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 54 total
- Mapped to phases: 54
- Unmapped: 0 ✓

---
*Requirements defined: 2026-11-09*
*Last updated: 2026-11-09 after roadmap creation*
