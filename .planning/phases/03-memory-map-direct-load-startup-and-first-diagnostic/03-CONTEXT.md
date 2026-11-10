# Phase 3: Memory Map, Direct-Load Startup and First Diagnostic - Context

**Gathered:** 2026-11-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver the MAPCTL-controlled CPU memory map, strict rights-clean direct-load
startup tier A, an original diagnostic with symbol-addressed guest RAM results,
a non-intrusive write watch and snapshot continuation, and an independent public
C API consumer producing a labelled framebuffer snapshot and silence WAV.
Cover MEM-01..04, EVD-01, HB-01, API-08, DET-03 and the Phase 3 portion of
CON-01. Timer-driven frames, input and real audio remain Phases 4–6.

</domain>

<decisions>
## Implementation Decisions

### Authority and autonomous discussion
- Apply the owner's existing delegation in `.planning/AUTONOMY.md`: research
  decision clusters independently, synthesize, and auto-follow recommendations
  within scope. These are orchestrator-selected implementation choices, not
  newly invented owner decisions. No extra acceptance prompt is required.
- Use Codex and resolve each GSD role's configured model and effort together.
  Preserve both runtime mappings and the owner's existing configuration edits.
- O1 remains open; profile `none` stays the default. No baseline, suite-pass
  compatibility claim, licence decision, firmware use, release or physical test.
  O6 remains a draft policy; no emulator implementation source is consulted.

### Memory map and unsupported hardware
- Route CPU reads/writes through one instance-owned decoder: Suzy FC00–FCFF,
  Mikey FD00–FDFF, ROM FE00–FFF7, vectors FFFA–FFFF. MAPCTL bits 0–3 select
  underlying RAM when set. FFF8 is always RAM; FFF9 is always MAPCTL.
- Preserve raw RAM inspection and add a separate side-effect-free CPU-mapped
  inspection path. Validate a whole request before writing its output buffer.
- Cover all 16 low-nibble mappings and every window boundary. Store bit 7 but
  retain explicitly PLACEHOLDER flat5 timing. Source-backed page-mode timing
  and device side effects arrive with their device phases.
- Supply no ROM bytes and no firmware input API. Access to mapped absent ROM
  must be explicitly unavailable, never silently backing RAM or fabricated
  BIOS. Prefer an additive unavailable stop/status with a named control.
- Unimplemented device-register behavior, MAPCTL guest readback and hidden-RAM
  write-through are not established by the sources reviewed. Keep any necessary
  deterministic provisional policy explicit and unverified, separate from the
  source-backed decoder. Expose the MAPCTL latch for host observation without
  claiming it is hardware-accurate guest readback. Do not build a fake full
  device model solely to make the diagnostic pass.

### Direct-load contract and fixture
- Use the canonical BS93/BLL subset measured with the pinned cc65: 10-byte
  header, `80 08`, big-endian load address, big-endian total file length
  INCLUDING the header, and `BS93`; nonempty payload; exact declared length;
  no address wrap. Entry is the payload load address. Other formats/markers
  are refused rather than detected heuristically.
- Replace the provisional raw-image external startup path with this documented
  direct-load path. The planner may retain a clearly named internal test seam
  or a justified low-level RAM helper, but normal runner/consumer/diagnostic
  content loading must use the strict container API; do not leave two
  ambiguous public content formats or use raw loading as firmware support.
- Content identity is computed by the core from the FULL accepted object bytes;
  report the payload load address and length separately. This binds header and
  payload without trusting a host-supplied hash. Include the
  format/startup strategy and entry/load semantics in identity and snapshots.
  Check all refusals before mutation, including aliased instance buffers.
- Tier A sets PC directly and otherwise retains declared power-on values,
  including MAPCTL zero from the cited Epyx reset table. The diagnostic starts
  in low RAM and writes MAPCTL $0C itself to select RAM for ROM/vectors, just
  as the inspected cc65 CRT does. That write is guest initialization, not a
  host-injected post-boot state. No speculative DISPCTL or boot-state preset.
- Write original ca65 source and a project-owned ld65 config/header recipe.
  Rebuild with pinned tools, emit labels with `-Ln`, and verify identities and
  the measured format. Do not copy emulator code or rely on a boot loader/C
  runtime. Record source, recipe, hash, notices and allowed use under current
  project rights; do not choose a new licence.
- Guest results have magic, schema, progress, error and a nonzero done marker.
  Resolve the result address from the linked symbol file; require completion,
  semantic progress and bounded guest time. A deliberately broken diagnostic,
  no execution, corrupt result and timeout must fail their intended checks.

### Observation, continuation and consumer
- Add a bounded host write-watch range with no callback or host code in the
  core. Stop after the matching guest write has completed, preserve CPU/bus
  continuation, and document exact tick/priority/rearm behavior. Resuming to
  the same final tick must yield the same guest state and output digests.
- Watch configuration/hit reporting is host observation, not guest machine
  state. Keep it out of guest-state digests and define reset/load/restore
  handling explicitly. Test actual CPU writes, boundaries and no-hit controls.
- Bump snapshot format for new guest fields, validate before apply, expose
  serialized state through public observations, and prove mid-diagnostic
  restore/continue equals uninterrupted execution (also across budget slices).
- Build a separate consumer linked only against the public header/API. Read
  original diagnostic result and packed framebuffer bytes from guest RAM;
  render a deterministic P3 PPM with metadata identifying it as a SNAPSHOT,
  guest tick, content/startup/profile and palette interpretation. This is
  HB-01 evidence, not display DMA, scanout, frame timing or a VID-04 freeze.
- Put a valid integer PCM WAV writer in that consumer; produce a short silence
  file labelled as placeholder audio until Phase 6. Use temporary outputs for
  binary WAV checks and maintain the existing artifact-rights policy.
- Compare runner and consumer public observations to independent expected
  diagnostic values; sharing the core proves repeatability, not accuracy.

### Verification and delivery
- Extend the existing exact inventory and named-fault controls. Every inventory
  change refreshes the existing claim's inventory hash in the same commit.
- Use targeted checks during implementation and the full `scripts/check.sh`
  at integration. Follow the existing Linux-container-before-push practice and
  inspect CI for the exact tested head plus critical-path efficiency.
- Keep implementation, goal verification, code review/fixes, security and
  validation coverage within this phase's existing approval. No extra
  acknowledgments for ordinary builds, checks, commits or bounded retests.

### the agent's Discretion
- Concrete API names, deterministic stub policy and explicit unsupported
  status details, watch API/event shape, snapshot field layout, diagnostic
  result schema/pixel pattern and test decomposition are reversible choices.
  Preserve existing refusal, ownership and half-open budget guarantees.
- Keep an adversarial conflict pass over all three research notes before
  finalizing plans. Unknown hardware behavior must remain visible.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `core/machine.c`, `core/bus.*`, the posted CPU request interface and
  `core/snapshot.c`; public caller-owned instance API and identity.
- Original ca65 fixture sources/config, pinned toolchain and fixture gate;
  runner artifact bundle, existing SHA-256 unit and deterministic golden.
- Whole-buffer refusal oracles, strict manifest validators, exact test
  inventory, claim hash binding and seeded fault-control harness.

### Established Patterns
- C17, integer-only core, no allocation, mutable globals or host imports.
- Separate build-independent observations from build-bound artifact identity;
  no hardware accuracy inference from shared-core digest equality.

### Integration Points
- Public header/ABI allowlist, CPU bus routing and price regions, load/reset,
  register/RAM inspection, snapshots, runner CLI/manifests, consumer target,
  fixture manifests, provenance/exposure records and check inventory.

</code_context>

<specifics>
## Specific Ideas

- Discuss research is recorded in `03-MEMORY-RESEARCH.md`,
  `03-STARTUP-RESEARCH.md` and `03-DIAGNOSTIC-RESEARCH.md`.
- The measured pinned-tool BLL probe settles an important ambiguity: total
  length includes the header. Do not apply the separate serial-upload wire
  length encoding to an on-disk BS93 object.
- Use nonzero sentinels and independent expected framebuffer pixels so empty
  or unexecuted RAM cannot appear to be a successful diagnostic.

</specifics>

<deferred>
## Deferred Ideas

- Real timers/IRQ and bus arbitration (Phase 4); page-mode calibration,
  timer-generated display/DMA/raw-video form and logical input (Phase 5);
  actual audio (Phase 6). No raw-video compatibility decision for O9 here.
- MAPCTL model-specific readback and electrical write-through qualification
  need additional source evidence; provisional behavior is never a claim.
- BIOS/HLE, cartridge boot, licences, releases and hardware tests remain
  owner-gated. Long-path vector diagnostics and history-scan growth remain
  carried follow-ups unless measured phase verification justifies a small fix.

</deferred>
