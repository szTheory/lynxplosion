# Project idea: Lynxplosion

## What to build

Build an accurate, reproducible and useful emulator for the Atari Lynx,
using the owner-selected working name Lynxplosion. Make the machine testable
and observable, keep its behavior suitable for well-designed hosts, and carry
forward current engineering lessons from the owner's emulator projects and
LibFuturo.

This is kickoff input, not a final hardware specification, compatibility
claim, API/ABI, license, release promise or fixed first milestone. Use GSD to
create the formal project context, evidence-based requirements and a rolling
roadmap. Separate owner direction, current source observations, test results,
inference, recommendations and open decisions.

## North Star

- Emulate clearly named Lynx hardware and cartridge profiles faithfully.
  Tie every compatibility claim to an exact model/configuration and evidence.
- Keep machine execution deterministic and observable. Test CPU and device
  behavior, controller input, guest progress, raw video/palette and raw audio
  as separate but connected evidence.
- Aim for high runtime performance without sacrificing correct behavior.
  Establish representative workloads and a correctness baseline before
  optimizing.
- Keep machine behavior independent from host clocks, filesystems, physical
  controller discovery, windows, A/V presentation and save-file policy.
- Make building, debugging, testing, packaging and host integration repeatable
  with minimal manual steps and a small dependency surface.
- Expand through verified vertical slices while preserving the larger quality
  goal. A first model or cartridge profile is delivery sequence, not a scope
  ceiling.

## Recommended discovery and delivery direction

First choose one explicitly named Lynx model/CPU profile and one cartridge
path. Resolve how startup works before promising an automated guest-boot test:
the physical system has a 512-byte onboard boot ROM, Gearlynx requires BIOS
input, and cc65 can build .lnx cartridge images. Evaluate a user-supplied
firmware path, an independently authored startup path, or both; do not put
copyrighted BIOS bytes in the repository or shared CI.

Then evaluate a small end-to-end slice using an original or rights-reviewed,
source-built diagnostic, deterministic input/time, fresh machine state, a
headless runner and an independent consumer. Capture a guest-semantic success
condition and raw video/audio separately. Use the MIT-marked Lynx Hardware
Tests as a candidate fixture source after pinning and reviewing the exact source,
build recipe, licensing and startup needs. The tests are hardware-informed
but do not, by themselves, prove all game compatibility or independently
qualify the emulator against physical hardware.

This is a recommended sequence, not an accepted first milestone or permanent
scope limit. Keep a playable/homebrew scenario visible early so the project
does not become research-only.

## Hardware and product scope to resolve

The name “Atari Lynx” does not select the full implementation scope. The first
project discussion should explicitly examine:

- Lynx I versus Lynx II, exact model/revision and any model-specific CPU,
  audio, power or timing behavior.
- The CPU profile. The Epyx-era developer specification says 65C02, cc65's
  target documentation says 65SC02, and the hardware test suite tests some CPU
  behavior separately for Lynx I and II. Resolve this through evidence; do not
  normalize the difference based only on chip naming.
- Startup, reset, vectors and boot ROM: user-supplied firmware, an
  independently authored path, or both; what can run in CI without protected
  firmware.
- Cartridge image/container formats, cartridge controller/banking, content
  identity, EEPROM/NVRAM and persistence. Cartridges are not necessarily flat
  ROM byte arrays.
- Mikey timing, timers/interrupts, audio channels, UART, display DMA/palette
  and shared bus behavior.
- Suzy sprite/blitter execution, collisions, math unit, CPU interaction and
  asynchronous work.
- The physical Lynx controls, player mapping, orientation, pause/option inputs
  and host remapping.
- ComLynx local multiplayer, linked-instance count, cable tests and expansion
  accessories; each may be a later milestone.
- Display/raw-color representation, 160×102 panel behavior, palette lookup,
  orientation/scale, audio events/samples and presentation ownership.
- Language/build system, API/ABI, debugger and runner, supported hosts,
  persistence/state policy, dependency set, new-code license, repository
  visibility and release artifacts.

Do not promise Lynx I/II, all cartridge hardware, EEPROM, ComLynx, accessories
or physical-device behavior merely because existing emulators support them.
Name a narrow baseline while preserving a clear path for later coverage.

## Core, API and integration

Keep one canonical machine implementation and a narrow, headless way to drive
it. Define instance ownership, content load/unload, controller sampling,
bounded advancement, outputs, reset, errors and refusal postconditions. Keep
host file I/O, wall-clock pacing, physical controls, display/audio devices and
durable save policy outside machine logic.

LibFuturo is a possible integration route, not a selected dependency or already
qualified Lynx profile. Test a real Lynx core and consumer against its current
input, execution-boundary, video/palette, audio, media and persistence contract
before proposing any profile or shared API change. A source build or adapter
agreement alone proves neither hardware accuracy nor an installed consumer.

## Development and verification intent

- Begin with source-backed component tests, a named boot/model profile, an
  original or carefully reviewed fixture, and a deterministic headless runner.
  Add a clean installed consumer at the point the package exists.
- Pin toolchain and test-suite revisions. Give each fixture a manifest with
  source, build recipe, checksum, license/notices and allowed use.
- Drive repeatable logical input and machine-time intervals from fresh
  isolated state. Check semantic guest progress as well as execution progress,
  register/memory effects, raw video/palette and raw audio samples/events.
- Maintain exact expected test inventories and meaningful negative controls.
  Verify a gate fails for the intended fault; expose skips, missing binaries,
  stale output and harness failures.
- Do not assume the public hardware test ROMs expose machine-readable results.
  Inspect their output and implementation. Choose a robust way to capture
  statuses/diagnostic bytes; keep test-ROM display, raw frame output and OCR or
  other image processing separate if considered.
- The CPU and bus timing cannot safely be reduced to a fixed instruction count
  per frame. Study page-mode fetches, timers, interrupts, display refresh/DMA,
  CPU stalls and Suzy bus ownership before choosing the core's work unit.
- Keep core/device checks, deterministic test-ROM runs, game progress,
  installed-package behavior, frontend input/presentation and physical Lynx
  qualification as distinct evidence.
- Put only recurring, distinct failure classes in fast CI. Stage long
  compatibility loops, fuzzing, broad platform matrices and performance
  distributions until their incremental value and cost are understood.
- Benchmark a named workload/build/configuration with correct outputs held
  constant. Separate emulator throughput and end-to-end latency/resource use
  from CI queue/setup/test time. Optimize the measured bottleneck.
- After every CI run, inspect low-risk opportunities to shorten critical-path
  time and reduce cost without losing proof. At milestone close, keep main
  green, triage PRs, update the rolling roadmap and lessons, and target a
  release of the exact tested artifact when rights, readiness, qualification
  and authority permit.

## Choices for GSD to resolve or keep open

- Baseline Lynx model/revision and CPU profile; second-model qualification.
- Rights-clean firmware/boot policy and a repeatable test boot path.
- Cartridge/container support, hardware banking, EEPROM/NVRAM and identity.
- Timing representation for CPU, timers, DMA, video/audio and shared-bus
  interactions; deterministic advancement units.
- Input and raw output contracts, host orientation/scaling and audio rendering.
- Which licensed hardware test suites are reliable enough for which gate, plus
  fixture capture and known-fault controls.
- Whether ComLynx or other accessories belong in an early milestone.
- Language, build tooling, public core API/ABI, package/host route and whether
  LibFuturo has a concrete fit.
- Save-state and persistence behavior, dependency policy, security/release
  workflows, project license, repository visibility and versioning.
- First guest-visible/playable proof and its independently checkable result.

Prefer evidence-supported, reversible defaults for routine decisions. Bring
the owner only choices that materially change scope, rights, authority, risk,
cost or an irreversible action. Public publication, protected firmware use and
physical hardware qualification remain explicit boundaries.

## Research input

Read .planning/bootstrap/EMULATOR-DNA.md before finalizing scope. Its source
links and claims are dated research accessed 2026-11-09. Follow the sources
directly and recheck mutable revisions, license terms and details when a
milestone depends on them. Emulator behavior is not hardware proof.
