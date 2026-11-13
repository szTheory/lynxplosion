# Roadmap: Lynxplosion

## Overview

v1 builds a rights-clean, deterministic, observable Atari Lynx core in six
vertical slices. Each slice ends in a runnable, deterministic, checkable
artifact. The slices are: an evidence harness and walking-skeleton API; a CPU
proven by per-opcode vectors under data-defined profiles; the memory map with
direct-load startup (tier A) and a first original diagnostic and labelled
snapshot; timers and IRQ with the first public lynx-tests suites read from
guest RAM; display, raw video and input with a live, input-responsive homebrew
frame; and audio, which completes the playable checkpoint and records a
throughput baseline. The phase structure follows the six suggested v1 slices in
`.planning/research/SUMMARY.md`. v1 is a delivery sequence, not a ceiling on
the project goal.

**Owner boundaries this roadmap keeps (see REQUIREMENTS.md "Owner decisions pending"):**
- No Lynx I or Lynx II baseline is adopted (O1 open). Both CPU profiles exist
  as data. Suite results are recorded per selected profile. No suite-pass claim
  is made for any profile until O1 is decided.
- The implementation language, build and API are not locked (O8 open). C17
  with CMake, Ninja, CTest and a C ABI is the research default only.
- Not scheduled in v1: BIOS or owner firmware (O3), HLE decrypt or "free" boot
  ROM (O4), project boot ROM or shim (O2), `.lnx` cartridge boot, publication
  or release (O10), physical hardware (O11). Every v1 run is `startup=A`
  (direct RAM load) and is never boot, cartridge or loader evidence.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Evidence Harness and Walking Skeleton** - One check command, pinned and manifested fixtures, fail-closed rights and core-boundary checks, and a repeatable skeleton machine behind the host API (completed 2026-11-09)
- [x] **Phase 2: CPU Profiles and Per-Opcode Vectors** - 65C02-class CPU with Lynx I and Lynx II profiles as data, tick-stamped bus cycles, proven by pinned vectors (completed 2026-11-10)
- [x] **Phase 3: Memory Map, Direct-Load Startup and First Diagnostic** - MAPCTL memory map, startup tier A, original diagnostic reporting through a RAM result block, first labelled snapshot via an independent consumer (completed 2026-11-10)
- [x] **Phase 4: Timers, Interrupts and First Public Suites** - Master-tick scheduler, Mikey timer bank, IRQ and minimal sleep; `timers` and `cpu` suites read from guest RAM with controls (completed 2026-11-11)
- [x] **Phase 5: Display, Raw Video and Input** - Timer-driven frames, display DMA, per-line palette, raw video and logical input; live input-responsive homebrew frame (completed 2026-11-12)
- [ ] **Phase 6: Audio and Playable Checkpoint** - Four audio channels and integer PCM; homebrew with sound; v1 suite inventory and throughput baseline

## Phase Details

### Phase 1: Evidence Harness and Walking Skeleton
**Goal**: A developer runs one command that builds the core, enforces the rights and determinism boundaries, and proves that a skeleton machine is repeatable and refuses bad input without side effects.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: FND-01, FND-02, FND-03, FND-04, FND-05, FND-06, FND-07, FND-08, FND-09, API-01, API-02, API-03, API-04, API-05, API-07, DET-01, DET-02, DET-04
**Success Criteria** (what must be TRUE):
  1. `scripts/check.sh` on a clean checkout fetches every pinned source and verifies it by SHA-256. It then builds, lints and runs the full fast test set against an exact expected test list. A hash mismatch, a missing or extra test, or an undeclared skip makes it fail. The same command is the PR CI gate.
  2. Each of the following makes the check fail closed, and the check passes again once it is removed: a seeded fake 512-byte firmware-like file, an unmanifested binary in the tree or build artifacts, or a forbidden host import in the core (clock, filesystem, threads, floating point, mutable globals). Every fixture manifest records source, revision, recipe, SHA-256, licence and notices, allowed use, startup tier, profile and evidence tier.
  3. Two runs from fresh state, and two instances in one process, each given the same content, empty input timeline and tick budget, produce identical artifact-bundle digests. Each run manifest records profile, startup tier, content identity, input digest, tick count, core build identity and the RAM power-on policy seed. Reset returns to the declared power-on state.
  4. Host-boundary refusal checks:
     - A refused content load returns a status code and leaves the existing session byte-identical.
     - Inspecting registers and memory changes neither state nor guest time.
     - Advance consumes no more than its tick budget and reports ticks consumed and a stop reason.
     - A snapshot restores in the same build, and a snapshot with mismatched identity is refused before any state changes.
  5. The pinned cc65 builds from source on the dev host and on the CI runner, with wall time recorded. The repo contains a claim template that binds profile, content identity, startup tier, evidence tier and test inventory, plus a draft source-exposure and clean-room record.
**Owner gates**: O8 (language, build and API): no core code until the owner confirms it or accepts the research default (C17, CMake/Ninja/CTest, C ABI). O6 (clean-room protocol and GPL-exposure record): FND-07 stays a draft until it is decided.
**Research flag**: Standard patterns, except the cc65 from-source build spike (FND-09: `TARGETS=lynx` recipe, wall time).
**Plans:** 13/13 plans complete

Plans:
**Wave 1**
- [x] 01-01-PLAN.md — Walking skeleton tracer: check.sh → CMake → lynx_core → public API → C test with exact values; exact registration; first green CI run (FND-01)

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 01-02-PLAN.md — Content identity (core SHA-256, FIPS vectors) and configure-time build identity (API-04, DET-04)

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 01-03-PLAN.md — Runner and artifact bundle: two-run digest equality, run manifest, CLI contract with fixed-0x00 default (DET-01, DET-04)

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 01-04-PLAN.md — Exact test inventory, JUnit completeness, declared skips, negative-control harness (FND-06)

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 01-05-PLAN.md — Core boundary gates: include/float scans, nm on uninstrumented twin, header self-containment (FND-05)

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 01-06-PLAN.md — Host API contract: instances, load refusal, bounded advance, reset/power-on, side-effect-free inspect (API-01..05)

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 01-07-PLAN.md — Versioned snapshot save/restore with validate-then-apply refusal (API-07)

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 01-08-PLAN.md — Determinism gates: whole-bundle two instances, golden cross-compiler digest, input and state sensitivity, run-manifest validator (DET-01/02/04)

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 01-09-PLAN.md — Pins, hash-before-unpack fetch with per-file archive list, no-network gate (FND-02)

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 01-10-PLAN.md — Fixture manifests, cc65 two-step build with derived build list, reproducible probe (FND-03, FND-09)

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 01-11-PLAN.md — Rights binary scan per the amended zone rule with seeded controls; PII and credential hygiene (FND-04)

**Wave 12** *(blocked on Wave 11 completion)*
- [x] 01-12-PLAN.md — Clean-room draft, exposure record, provenance, claim template bound to the inventory (FND-07/08)

**Wave 13** *(blocked on Wave 12 completion)*
- [x] 01-13-PLAN.md — PROJECT.md decisions, README links, final CI evidence and efficiency review (FND-01, FND-09)

### Phase 2: CPU Profiles and Per-Opcode Vectors
**Goal**: The 65C02-class CPU executes correctly under data-defined Lynx I and Lynx II profiles, proven by pinned per-opcode vectors, without either profile being adopted as baseline.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: CPU-01, CPU-02, CPU-03, CPU-04, CPU-05
**Success Criteria** (what must be TRUE):
  1. Pinned per-opcode functional vectors run in the check for every defined profile. Each run reports an exact per-profile inventory (vectors included, excluded and the reason for each exclusion).
  2. A host selects the Lynx I or Lynx II CPU profile per instance, and RMB, SMB, BBR and BBS behave according to that profile's data. A named-fault control that selects the wrong profile fails the bit-operation vectors.
  3. Every CPU memory access appears as a tick-stamped bus cycle. A test bus with a non-default per-access cost changes the consumed ticks without changing architectural results.
  4. Tests cover IRQ entry, CLI/SEI latency, WAI/STP and reserved-opcode NOP lengths, and each follows the selected profile's documented behaviour. Each expected value cites its source or is marked unverified.
**Owner gates**: O1 (baseline model). Both profiles are built and neither is named baseline. No CPU-profile compatibility claim is made until O1 is decided.
**Research flag**: Deeper research needed before planning: which SingleStepTests directory applies to each profile and which opcodes to exclude; decimal-mode and reserved-opcode cycle counts (unsourced); the shape of the bus-cycle interface (AD-01 spike).
**Plans:** 11/11 plans complete

Plans:
**Wave 1**
- [x] 02-01-PLAN.md — CPU core and profile tables as a white-box unit: tracer slice, all modes, decimal, interrupts, include gate (CPU-02, CPU-03, CPU-04)

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 02-02-PLAN.md — startup_tier `none` vocabulary with controls; Phase 2 exposure record; claim-template vector note (CPU-01)

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 02-03-PLAN.md — Pinned SingleStepTests sample, pin record, licence, manifests, provenance; vectors-full.sh pin/sample (CPU-01)

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 02-04-PLAN.md — Strict reader, vector runner, committed per-profile inventories, cpu.vectors.none/lynx1/lynx2 (CPU-01, CPU-02)

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 02-05-PLAN.md — Wrong-profile and other named-fault controls, pin lint, full-lane run mode (CPU-05, CPU-01)

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 02-06-PLAN.md — Authored per-cycle scenario vectors with source tags: latency, Test 8 replica, BRK/NMI/reset, bit ops, edges (CPU-04, CPU-02)

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 02-07-PLAN.md — sum-trap and churn-loop fixtures with hand-derived traces; ca65 re-assembly gate (CPU-03, CPU-02)

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 02-08-PLAN.md — Bus cost model (flat5 PLACEHOLDER, test_skew); det gates parameterised, golden unchanged (CPU-03)

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 02-09-PLAN.md — Machine switch: S2 budget loop, UNDEFINED, profiles per instance, new registers, snapshot format 2, golden and claim re-baseline (CPU-02, CPU-03, CPU-04)

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 02-10-PLAN.md — Public-path tests: runner --profile, profile select/divergence, cost model, slicing, seam agreement, profile agreement (CPU-02, CPU-03, CPU-05)

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 02-11-PLAN.md — Profile document, README, full vector lane, push, CI by head SHA, CI efficiency review (CPU-01..05)

### Phase 3: Memory Map, Direct-Load Startup and First Diagnostic
**Goal**: As a developer, I want to run an original diagnostic through rights-clean startup tier A on the real memory map and read its RAM pass or fail result and labelled viewable snapshot, so that I can inspect reproducible guest progress through the public API without firmware.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: MEM-01, MEM-02, MEM-03, MEM-04, EVD-01, HB-01, API-08, DET-03, CON-01
**Success Criteria** (what must be TRUE):
  1. An original, project-authored diagnostic built with the pinned cc65 starts by direct RAM load with a documented preset state. Every preset value cites its source or stays at power-on reset. The harness reads PASS by symbol from the diagnostic's result block (magic, schema, progress, error, done), and a deliberately broken diagnostic reports FAIL.
  2. Memory-map tests show that the CPU-mapped view decodes Suzy, Mikey, the ROM overlay and vector space according to MAPCTL, and that the raw RAM view stays separate.
  3. Every run manifest and claim record carries `startup=A`. The core has no firmware input path; it is absent or disabled, and a test shows that firmware cannot be supplied.
  4. A memory watch on the result block stops advancement on write without changing any digest. A run that is snapshotted mid-diagnostic, restored and continued gives the same digests as an uninterrupted run.
  5. An independent consumer built only against the public API drives the core and renders a tick-stamped framebuffer snapshot to a viewable PNG or PPM labelled as a snapshot, not a frame. Its WAV writer is in place; the core produces silence until Phase 6.
**Owner gates**: None for tier A. O8 is inherited (public C API). O2, O3 and O4 are not scheduled: no BIOS, boot ROM, boot shim or HLE decrypt. Tier A results are never boot, cartridge or loader evidence.
**Research flag**: Deeper research needed before planning: per-register preset provenance (the DISPCTL sources disagree; cite Epyx or test values, never another emulator's); spike whether `lynx-bll.cfg` output equals the BS93 `.o` container (Disagreement 12). The fallback is `ca65`/`ld65` with a project config and packer.
**Plans:** 9/9 plans complete; goal verification pending

Plans:
- [x] 03-01-PLAN.md — Strict BLL tracer and versioned guest identity
- [x] 03-02-PLAN.md — BLL ownership proofs and strict runner startup
- [x] 03-03-PLAN.md — Complete caller migration and private flat-vector seam
- [x] 03-04-PLAN.md — MAPCTL routes and pure mapped inspection
- [x] 03-05-PLAN.md — Permanent map controls and post-write watch API
- [x] 03-06-PLAN.md — Watch isolation and equivalent snapshot continuation
- [x] 03-07-PLAN.md — Original source-built diagnostic and result oracle
- [x] 03-08-PLAN.md — Runner diagnostic agreement and independent public consumer
- [x] 03-09-PLAN.md — Atomic consumer artifacts and exact-head qualification

### Phase 4: Timers, Interrupts and First Public Suites
**Goal**: As a developer, I want to advance Mikey timers and interrupts on one master-tick scheduler and run the pinned cpu and timers suites headlessly, so that I can inspect bounded, reproducible per-profile guest results without firmware.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: TIM-01, TIM-02, TIM-03, TIM-04, EVD-02, EVD-03, EVD-04, EVD-05
**Success Criteria** (what must be TRUE):
  1. All devices advance from one master-tick scheduler with a documented total order (tick, priority, sequence). The timer bank (shared prescaler phase, link graph, DONE, reset-done, borrow) survives a snapshot round-trip with identical digests.
  2. Timer interrupt pending bits are observable separately from enables, and the CPU takes IRQs under the selected profile's sampling rules. The lynx-tests `timers` and `cpu` suites run headlessly from source-built tier-A images. `g_results` is read by symbol, completion needs a sentinel plus a guest-time minimum and bound, and results are compared with expectations generated from the pinned source, with a count check.
     Owner-approved 2026-11-11: only timers result offset 14 remains an explicit
     pending source agreement (0C versus 0D on both profiles). All sixteen rows
     are compared and recorded; strict comparison remains failing. Complete
     Phase 4's observation/control gate under D-10 and carry exact agreement to
     Phase 5. All other errors still fail; EVD-02 remains globally open.
  3. A no-run control (CPU halted before the tests start) fails every suite. The harness fails any suite whose detected model does not match the selected profile.
  4. Named-fault controls (for example, DONE gating removed, or the wrong profile selected) each fail their intended test.
  5. A minimal CPU-sleep and bus-request model exists and is exercised by a test, so that later Suzy arbitration can attach without reworking the CPU or timers.
**Owner gates**: O1. Results are recorded against the selected profile, and no "passes" claim is made for any profile until O1 is decided.
**Research flag**: Deeper research needed before planning: verbatim re-read of the Epyx timer and IRQ material; `timers2` phase and enable-order expectations; `cpu` tests 1 and 8 (interrupt-entry quirks); confirm on the pinned commit that `cpu` needs Timer IRQs, and per-suite `.sym`/`g_results` counts (Disagreements 8 and 15).
**Plans**: 12/12 plans complete

Plan 12 local macOS and self-contained Ubuntu qualification passed, and exact-head hosted CI passed on `356a920`. Phase 4 remains in progress pending formal goal verification and root closeout; see the [qualification ledger](phases/04-timers-interrupts-and-first-public-suites/04-QUALIFICATION.md). The outage-era `43096967098` run remains historical and unexecuted.
- [x] 04-01-PLAN.md
- [x] 04-02-PLAN.md
- [x] 04-03-PLAN.md
- [x] 04-04-PLAN.md
- [x] 04-05-PLAN.md
- [x] 04-06-PLAN.md
- [x] 04-07-PLAN.md
- [x] 04-08-PLAN.md
- [x] 04-09-PLAN.md
- [x] 04-10-PLAN.md
- [x] 04-11-PLAN.md
- [x] 04-12-PLAN.md

### Phase 5: Display, Raw Video and Input
**Goal**: As a public API host, I want to receive timer-generated raw frames from an original homebrew responding to a recorded logical input timeline, so that I can render and reproduce its input-responsive frames.
**Mode:** mvp
**Depends on**: Phase 4
**Requirements**: VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02, CON-01 (real-frame consumer slice), EVD-02 (remaining timer agreement), EVD-05 (DMA fault slice)
**Success Criteria** (what must be TRUE):
  1. Changing the Timer 0 and Timer 2 settings changes line and frame timing as measured in guest ticks (no fixed 60 Hz). The host can stop advancement on a frame-complete event whose definition is documented.
  2. Display DMA appears as a bus requestor that costs ticks and closes page-mode streams. The lynx-tests `page-mode` suite runs with results captured and inventoried, or appears as a visible, documented skip. A named-fault control on DMA cost fails its intended test.
     Revisit the unchanged timers offset-14 oracle with independently justified
     startup/counter, page-mode, refresh and display-DMA preconditions. Preserve
     the exact disagreement if evidence still cannot justify source agreement;
     never tune the model or fixture to obtain the expected count.
  3. A per-line palette fixture shows the 16-entry, 12-bit palette latching per line. The host receives raw video with pen data, per-line palette state and timing metadata, separate from any scaling, rotation or colour presentation.
  4. The host supplies a logical input timeline, and the joystick and switch registers (d-pad, A, B, Option 1, Option 2, Pause) reflect it at the sampled guest time. The core performs no controller discovery.
  5. An original homebrew direct-load image, driven by a recorded input timeline, shows input-responsive frames that the consumer renders. The same recording yields the same raw-frame digest, labelled as repeatability.
**Owner gates**: O9 before the canonical raw video form is frozen, if the form affects LibFuturo. O1 for any `page-mode` claim.
**Research flag**: Deeper research needed before planning: DMA burst timing and refresh period and cost (open; do not import another emulator's constants); per-line versus per-frame palette contract; frame-complete definition (Disagreement 10); palette byte order versus LCD latch order.
**Plans**: 12/12 plans complete
**Wave 1**
- [x] 05-01-PLAN.md

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 05-02-PLAN.md

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 05-03-PLAN.md

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 05-04-PLAN.md

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 05-05-PLAN.md

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 05-06-PLAN.md

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 05-07-PLAN.md

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 05-08-PLAN.md

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 05-09-PLAN.md

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 05-10-PLAN.md

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 05-11-PLAN.md

**Wave 12** *(blocked on Wave 11 completion)*
- [x] 05-12-PLAN.md

### Phase 6: Audio and Playable Checkpoint
**Goal**: As a public API host, I want to replay an original homebrew with input-responsive audio and video on one guest-tick timeline, so that I can reproduce an A/V checkpoint and inspect its v1 public-suite inventory and throughput baseline without firmware.
**Mode:** mvp
**Depends on**: Phase 4 (timers), Phase 5 (homebrew and input)
**Requirements**: AUD-01, AUD-02, HB-03, DET-05, EVD-06, PERF-01, CON-01 (final real-audio/video consumer closure), EVD-05 (final device-fault closure)
**Success Criteria** (what must be TRUE):
  1. Four audio channels (LFSR, integrator, reload, volume, timer links) and stereo muting are emulated. Original tests cover channels 1 to 3 and the stereo bits. Results from the `audio` and `audio2` suites are captured, and a named-fault control fails its intended test.
  2. The host receives integer PCM at an exact rational rate derived from the 16 MHz clock, plus tick-stamped channel events. The forbidden-import check still shows no float, filter or resampler in the core.
  3. The homebrew plays sound in response to a recorded input timeline, and the consumer writes its frames as PNG or PPM and its audio as WAV. Machine progress, logical input, raw video and raw audio are separate streams on one guest-tick timeline, and their repeatable digests are labelled as repeatability, not accuracy.
  4. The `timers`, `cpu`, `page-mode`, `audio` and `audio2` suites run headlessly from pinned, source-built tier-A images, with results captured and inventoried. Any suite that cannot run that way is a visible, documented skip that names its blocker.
  5. A named workload (the homebrew with a fixed input timeline) has a recorded throughput baseline with its output digest held constant. No optimisation is done.
**Owner gates**: O1 for any suite claim. O8 is inherited.
**Research flag**: Deeper research needed before planning: audio register layout and LFSR taps (no sourced page found); CTLB lower-nibble behaviour; exact-rational output rate choice (AD-07); conflicting low-pass figures.
**Plans:** 8/12 plans executed

Plans:
**Wave 1**
- [x] 06-01-PLAN.md — One input-responsive audio path through public spans and WAV
- [x] 06-02-PLAN.md — Source-built audio and audio2 objects and exact source oracles

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 06-03-PLAN.md — Four-channel digital behavior and all stereo routes

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 06-04-PLAN.md — Complete bounded timer/audio ring and sample order

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 06-05-PLAN.md — Transactional A/V spans and lossless capacity stops

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 06-06-PLAN.md — Validate complete audio state and prove all continuation paths

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 06-07-PLAN.md — Complete public A/V replay, host checkpoint and artifact semantics

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 06-08-PLAN.md — Exclusive whole-bundle publication and malformed replay controls

**Wave 8** *(blocked on Wave 7 completion)*
- [ ] 06-09-PLAN.md — Fresh source-suite observations and exact five-suite inventory

**Wave 9** *(blocked on Wave 8 completion)*
- [ ] 06-10-PLAN.md — Private semantic faults for channel, link, stereo and phase behavior

**Wave 10** *(blocked on Wave 9 completion)*
- [ ] 06-11-PLAN.md — Digest-bound five-run homebrew throughput baseline

**Wave 11** *(blocked on Wave 10 completion)*
- [ ] 06-12-PLAN.md — Review, bounded fixes and exact-source qualification

## Later Milestones (unscheduled)

These come from REQUIREMENTS.md v2. They are listed so the roadmap stays
rolling, but they are not scheduled, numbered or committed. Each returns
through a later milestone discussion with its own evidence plan and owner
gates.

- **UART / ComLynx**: single-console UART (`uart`, `uart2`, `uart3`); multi-instance link (O12). `uart4` needs physical hardware (O11).
- **Suzy**: bus-master state machine (`sdoneack`), math unit and register mirrors (`math`, `memio`), sprite painter (`sprites3`), sprite timing (`sprites1`, `sprites2`, `sprites4`, `sprites5`; O7 for the `sprites2` literal).
- **Cartridge and `.lnx` boot**: port-level address and data components, strict `.lnx` parser with content identity, cartridge boot via a project-authored shim (O2; O3 and O4 remain owner-gated).
- **EEPROM and persistence**: 93Cx6 as explicit configuration, with host-owned persistence keyed by content plus hardware identity.
- **Host integration / LibFuturo**: installed independent consumer; LibFuturo adapter after a real consumer round-trips (O9); trace ring, breakpoints and status API.
- **Second model**: qualification of whichever of Lynx I or II is not the baseline, with its own inventory (O13).
- **CI and performance**: CI lane expansion only after a distinct failure class is shown; optimisation of a measured bottleneck with digests held constant.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Evidence Harness and Walking Skeleton | 13/13 | Complete    | 2026-11-09 |
| 2. CPU Profiles and Per-Opcode Vectors | 11/11 | Complete    | 2026-11-10 |
| 3. Memory Map, Direct-Load Startup and First Diagnostic | 9/9 | Complete    | 2026-11-10 |
| 4. Timers, Interrupts and First Public Suites | 12/12 | Complete    | 2026-11-11 |
| 5. Display, Raw Video and Input | 12/12 | Complete    | 2026-11-12 |
| 6. Audio and Playable Checkpoint | 8/12 | In Progress | - |
