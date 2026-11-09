# Emulator engineering DNA for Lynxplosion

Prepared 2026-11-09 as self-contained input to the GSD kickoff. This is
research and reusable workflow advice, not a console specification, accepted
architecture, requirements, license decision or implementation report.
Recheck all changing sources before making them load-bearing.

## Transferable emulator lessons

### Contracts and architecture

- Keep one canonical core. Separate CPU/device behavior from host files,
  clocks, physical controllers, A/V presentation and persistence policy.
- Define instance lifecycle, content ownership, logical input sampling, guest
  work units, output buffers, reset/unload, error/refusal postconditions and
  concurrency explicitly. A context-shaped API does not prove safe instances
  or reentrancy.
- Use precise version/profile/configuration identities. Preserve old fields,
  reject malformed/unsupported inputs intentionally, and test whether failed
  operations preserve promised state. Do not copy another emulator's API,
  status values or tick unit.
- Prove packaged output through a clean independent consumer. Source-tree tests
  do not establish relocation, loader or installed-host success.
- LibFuturo is an experimental source API rather than a stable ABI. Its
  documented profiles do not include Lynx in the reviewed contract. Evaluate
  actual controller, time, color/palette, audio, media and persistence fit
  before adopting or proposing a shared contract change.

### Accuracy, tests and evidence

- Distinguish evidence tiers: specification/vector expectation, public
  hardware-informed test, original diagnostic, independent emulator
  comparison, measured hardware, repeatability, game progress and installed
  host behavior. One tier does not imply the next.
- Use authored or permission-checked diagnostics with independent expected
  results. Record source, recipe, digest, notices, license and allowed use. A
  checksum identifies bytes; it does not grant rights.
- Drive deterministic inputs at an explicit guest-time boundary and start from
  fresh isolated state. Assert guest semantics as well as execution progress,
  registers/memory, raw video/palette and raw audio. Same-core hashes establish
  consistency, not hardware correctness.
- Give important gates exact inventories and named-fault controls. Make
  skipped/missing tests, stale artifacts, timeouts and harness failures visible.
- Keep core/device, runner, installed package, frontend/device input, display/
  audio presentation and physical hardware evidence separate.
- Treat Lynx Hardware Tests as a strong candidate source, not an infallible
  oracle. Review exact expectations; test suites based on analysis of hardware
  do not replace independent physical qualification.

### Performance, CI and workflow

- Start with one documented local command aligned with fast PR checks. Put
  each required CI job on a distinct failure class. Defer broad matrices,
  long fuzzing, soak tests and noisy performance distributions until their
  incremental value is clear.
- Measure queue, setup, build and test critical-path time before adding caches,
  concurrency or matrix breadth. A cache or parallel job is worthwhile only
  when a clean comparison shows savings and invalidation/failure behavior is
  checked.
- Benchmark named workloads, builds and configurations while holding correct
  outputs constant. Report emulator throughput separately from end-to-end
  latency/resource use and CI wall time. Profile before optimizing.
- At every milestone discussion, reread current project state and sibling/API
  evidence that can change the decision. Fan out across decision-relevant
  hardware, rendering, timing, sound, product, API/host, QA, security/licensing,
  performance and DevOps roles; use primary sources where possible, compare
  alternatives/footguns and cost, include an adversarial pass, then stop when
  another round no longer changes the recommendation. Keep a rolling roadmap.
- Close each milestone with main green, PR triage, refreshed roadmap/lessons
  and a release target. Release the exact tested artifact when contents,
  rights, qualification and authority are ready. GSD milestones and software
  versions are separate.
- Routine reversible work can proceed autonomously. Automation does not settle
  material scope, rights, public publication or physical-qualification choices.

## Lynx hardware and validation findings

### Model and CPU are explicit

The Epyx-era Handy specification uses 65C02 terminology for the CPU. The cc65
Lynx target documentation describes a 65SC02 target, and the public Lynx
Hardware Tests CPU suite identifies Lynx I/II-specific RMB/SMB/BBx behavior.
Do not resolve this discrepancy by assuming the labels are interchangeable.
Choose a named baseline model/revision and source its actual instruction,
interrupt and timing behavior; qualify another model separately.

The hardware centers on Mikey (CPU-facing registers, timers/interrupts, audio,
UART and display DMA) and Suzy (sprite/blitter, collisions, math, joystick and
cartridge-related functions). Timing is affected by page-mode instruction
fetches, timer phase, display refresh/DMA, interrupts, waits and shared-bus
ownership. Suzy work is asynchronous; model the observed CPU/device
interaction instead of converting it into an untimed drawing call. MAPCTL
changes CPU-visible mappings; do not assume it removes a device's independent
access to the same memory. Verify these details against the Epyx specification,
hardware tests and any measurements.

The Epyx material reports a 160×102 display built from RGB triads and a
4096-color palette with a smaller on-screen set at one time. Preserve raw color/
palette semantics separately from host scaling, rotation or panel presentation.
Verify dimensions and pixel conventions before freezing an output contract.

### Startup, cartridges and rights

The physical handheld includes a 512-byte boot ROM. Gearlynx reports that it
requires a BIOS image. cc65 documentation describes cl65 -t lynx generating a
bootable .lnx cart image with its minimal loader. These facts do not settle
Lynxplosion's boot policy or mean an unlicensed BIOS may enter the repository.
Plan a clean test path: owner-supplied firmware kept outside source control,
an independently authored boot/diagnostic route, or a reviewed combination.
Check that the chosen path works in unattended CI before promising automated
game/test execution.

Cartridges may include hardware beyond flat ROM storage, including address
counter/shift-register and banked behavior; writable cartridge elements and
EEPROM/NVRAM can affect save-game semantics. Pin and name the content/config
profile. Tie persistent bytes to explicit cartridge identity. Keep ComLynx
protocol emulation distinct from host networking and from testing two physical
consoles.

### Automation ladder

The public drhelius/lynx-tests repository is marked MIT and describes its
tests as developed by analyzing real hardware. Suites cover CPU behavior,
memory-mapped I/O, math, audio, timers, CPU/Suzy sleep and bus coordination,
sprite operations, page mode, display timing/DMA and UART. Most fixed suites
run at startup and display status plus diagnostic bytes. The README also
distinguishes interactive refresh-rate calibration and two-console uart4 from
fixed tests. It can support this progression, after exact source and
build/licensing review:

1. Pure CPU/device and loader tests with exact known answers.
2. A fresh-state guest diagnostic that reports semantic progress.
3. Timers, bus interaction, audio, video, math, sprites and collisions in
   independent expected-result groups.
4. Single-console UART behavior; separate multi-instance ComLynx as scope
   requires.
5. Clean installed consumer/package check and selected game/homebrew progress.
6. Physical Lynx I/II comparison for model-specific behavior and display/audio
   characteristics when access and authority exist.

Pin toolchain and suite revisions, build fixture binaries from reviewed source,
collect a stable and inspectable guest result, and use timeouts. Do not assume
the tests expose machine-readable results or that a generated .lnx image boots
without settling firmware behavior. A same-core replay hash is not an
independent answer. Refresh-rate logic-analyzer calibration and physical
ComLynx require hardware; keep them out of ordinary headless CI.

### Product fit and independent references

Atari's 1990 company report describes Lynx sales as slow at launch. It is
historical market context, not a lifetime unit count, current player telemetry
or an unmet-demand measurement. The bounded scan did not find comparable
current Lynx player-use data. Lynx is a useful automation-first breadth and
learning project, while GBA has stronger audience signals; existing managed
mGBA coverage means a new GBA core needs a concrete ownership, integration or
accuracy gap. Do not market the Lynx choice as the most demanded next console.

Gearlynx is a current, capable emulator/debugger. Its repository is GPL-3.0,
its README reports broad compatibility and passing the Lynx hardware tests,
and its usage guide requires a BIOS. These are maintainer statements, not new
independent test results. Use Gearlynx to inspect behaviors and debug a
difference, not as proof by itself or as code to copy. MAME is another
implementation cross-check, not a hardware oracle. Review each license before
reusing code or assets; no runtime dependency is selected.

## Public sources checked 2026-11-09

- [Atari Lynx developer documentation index](https://atarilynxdev.net/doku.php/lynx:doc:start) — explains that original Epyx documentation was converted to HTML and links its scanned specification. Primary-document content is hosted by a community archive.
- Epyx Handy specification: [hardware overview](https://www.monlynx.de/lynx/lynx2.html), [software/hardware pitfalls](https://www.monlynx.de/lynx/lynx3.html), [CPU/ROM](https://www.monlynx.de/lynx/lynx4.html), [display](https://www.monlynx.de/lynx/lynx5.html), and [sprite/collision](https://www.monlynx.de/lynx/lynx6.html).
- [cc65 Atari Lynx target docs](https://cc65.github.io/doc/lynx.html) — toolchain, cartridge build and CPU target documentation.
- [Lynx Hardware Tests](https://github.com/drhelius/lynx-tests) — MIT-marked source repository; its current README and test folders list behavior and tooling. Pin/review the actual revision and each fixture before relying on it.
- [Gearlynx](https://github.com/drhelius/Gearlynx) and its [MCP debugger guide](https://github.com/drhelius/Gearlynx/blob/main/MCP_README.md) — a GPL-3.0 implementation with debugger features, BIOS requirement and maintainer-reported compatibility.
- [Libretro Gearlynx docs](https://docs.libretro.com/library/gearlynx/) — independent documentation of Gearlynx's BIOS setup.
- [Atari Corporation 1990 Annual Report](https://www.atarimania.com/documents/atari-corporation-annual-report-1990.pdf) — historical corporate sales commentary only.
- [MAME Lynx driver](https://github.com/mamedev/mame/blob/master/src/mame/atari/lynx.cpp) — implementation cross-check; inspect current source/license, do not treat as hardware proof.
