# Phase 6: Audio and Playable Checkpoint - Context

**Gathered:** 2026-11-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Complete the native, firmware-free playable checkpoint: four digital audio
channels, stereo muting, integer PCM and ordered channel observations; an
original input-responsive A/V homebrew replay through the public API; the five
pinned tier-A suite inventory; and a digest-bound throughput baseline. Preserve
all five roadmap success criteria. This is software behavior and repeatability
evidence, not physical hardware or model qualification.
</domain>

<decisions>
## Implementation Decisions

### Sound behavior and guest time

- **D-01:** Implement four channel register blocks, counters/reload, 12-bit
  LFSR, signed volume, integrator/clipping and stereo route-disable semantics
  from primary Epyx/Atari documentation. Keep observed source facts, oracle
  expectations, inference and unverified timing policy distinct. The audio
  research report identifies the primary audio chapter and register appendix;
  verify ambiguous register/status details before implementation.
- **D-02:** Use one bounded, deterministically ordered timer/audio event graph
  covering T1→T3→T5→T7→A0→A1→A2→A3→T1. Audio arithmetic/register helpers may
  live in a focused module. Bound same-tick ring propagation; document and
  serialize any deferred pulse and its versioned software policy. Never permit
  a zero-reload cycle to hang or silently lose pulses. Exact physical ring
  propagation and reset/divider phases remain unqualified.
- **D-03:** Select native **40,000/1 Hz**, stereo signed-16 interleaved PCM:
  exactly 400 master ticks per sample. This is an observation policy, not a
  hardware rate. WAV supports this integer rate; a common 48 kHz host rate is
  not required by the current file consumer and would add fractional sample
  phase. Define an integer-only, bounded mix/normalization policy explicitly;
  no core float, filter or resampler. Keep tick-stamped channel events as a
  separate observation stream.
- **D-04:** Specify the sample lattice origin, absolute ordinal, same-tick
  device/sample/CPU order and half-open target behavior before coding. Prefer
  first sample at tick 400 (ordinal zero), device/link work before sampling,
  sampling before the CPU grant, and leave work at the target pending. Preserve
  existing timer/video/CPU ordering outside the documented audio extension.
- **D-05:** Preserve default profile `none`, immutable LEGACY_FLAT5 default,
  opt-in VIDEO_POLICY1 and all strict timer observations. Do not tune timing
  to force upstream agreement. Audio stereo is a documented software contract;
  do not claim which physical revisions implement it without evidence.

### Native API, state and replay

- **D-06:** Use bounded caller-owned output spans for A/V advancement, not a
  retained core FIFO. Define capacity/back-pressure and stop precedence so
  samples/events cannot disappear. Validate pointers, spans, alignment,
  overlap, arithmetic and capacity before mutation wherever refusal is
  transactional; document any legitimate partial-progress stop. Retain no host
  buffer pointers. Progress, input, video, events and PCM stay distinct.
- **D-07:** Extend immutable identity and fieldwise snapshot/digest contracts
  for audio as required by the existing exact-build compatibility policy.
  Include counter/LFSR/output/integrator/link and sample-phase continuation;
  validate before apply. Keep file positions, input-script cursors and output
  publication ownership in the host checkpoint. Compare complete stream
  suffixes across restore, including sample ordinals and input-edge phase.
  This is a native Lynxplosion API change, not a LibFuturo contract freeze.
- **D-08:** Preserve the Phase 5 input-only frame tracer and the Phase 3
  snapshot/silence regression. Add a separate source-built original A/V fixture
  and consumer mode with raw frames, PPM, channel-event records and real WAV.
  A recorded input edge must change guest progress/response markers, image
  evidence and non-silent audio. Publish a complete exclusive output bundle
  with identities for content, timeline, policy, build and each stream.
- **D-09:** Fresh, sliced, interleaved two-instance, watched/resumed and
  mid-run restored paths must agree stream-by-stream at common guest ticks.
  Include pre/post input-edge and audio/sample boundary checkpoints, output
  capacity failures, reset/load/unload and overflow edges. Same-core hashes
  prove repeatability only; automated file/semantic checks replace subjective
  listening for this scope.

### Source-suite and fault evidence

- **D-10:** Extend the existing suite builder/oracle/runner and provenance
  pattern. Build unchanged pinned MIT `audio/tests.s` and `audio2/tests.s`
  with project-authored assembly wrappers and the pinned toolchain. Generate
  all 18 expected rows each from the pinned C oracle sources, preserving
  audio2 index 9's source alternative 0x30|0x34. Do not run their TGI CRT mains.
  Both assembly sources avoid SERCTL, so page-mode's blocker does not apply.
- **D-11:** Capture fresh observations on both named profiles with independent
  guest model evidence, full zero-page initialization, test return, progress,
  post-return sentinel, exact extent and guest-time bounds. Record raw bytes
  and comparison status without promoting O1-open observations into suite or
  hardware qualification. Missing audio implementation is not a skip reason.
- **D-12:** Neither upstream audio suite accesses MSTEREO. In the roadmap,
  "Original tests" means project-authored coverage: cover all channels and
  eight stereo mute bits independently. Keep named faults for channel-0 LFSR/
  integration, channels 1–3 link propagation (including A3→T1), both stereo
  sides, sample cadence and serialized phase. Each private semantic mutant
  must fail its intended assertion with clean positive brackets; build failure
  or an undiagnosed timeout is not detection. No production fault switches.
- **D-13:** Inventory cpu, timers, page-mode, audio and audio2 exactly once.
  Preserve the exact two page-mode SERCTL skips if still applicable. Preserve
  the strict timer row 14 disagreements: legacy 0C/source 0D and VIDEO_POLICY1
  0F/source 0D. Do not widen the legacy exception, change an oracle, add a new
  pending descriptor or claim the timing issue resolved.

### Execution efficiency and boundaries

- **D-14:** First deliver a thin real input→audio→public API→WAV vertical
  slice, then complete channel/state/host/evidence depth. Front-load source
  building and oracle validation. Audit every raw snapshot consumer and every
  copied-core build source list when changing structure; update mapped opcode
  peek alongside bus routing. Run targeted GCC/Clang and ShellCheck 0.9/0.11
  checks early, before a late full qualification tail can accumulate.
- **D-15:** Record a named fixed-input homebrew throughput baseline over five
  same-build fresh runs, constant guest work and stream/output digests. Use a
  monotonic host timer around in-memory core work; report consumer I/O and CI
  cost separately. Report median/range and environment; perform no optimization.
- **D-16:** Reuse distinct existing regression evidence. After all fixes and
  reviews, one final canonical local gate per required environment and hosted
  CI on exact pushed HEAD qualify the final source. Inspect the measured CI
  critical path; do not add caches, matrices, dependencies or duplicate broad
  runs without evidence. No 2 GB CPU vector lane absent a CPU-specific concern.
- **D-17:** Existing authority covers checked main commits, local/background
  tests, CI, review and bounded fixes. Preserve O1/O5/O6/O9 and rights stops:
  no BIOS/commercial assets, physical tests, release, new distribution channel,
  emulator implementation source exposure or shared host-contract decision.

### The agent's Discretion

Exact module seams, ABI field names, capture enablement, integer normalization,
event record fields, bounded-ring implementation and task decomposition are
routine engineering choices within these contracts. Resolve them once using
primary evidence and focused tests. Ask only for a materially new consequence
outside existing authority. Research alternatives are not extra owner gates.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scope and authority
- `AGENTS.md` — engineering, rights, background execution and owner policy.
- `.planning/AUTONOMY.md` — delegated discussion and phase verification defaults.
- `.planning/ROADMAP.md` — Phase 6 goal and all five unchanged success criteria.
- `.planning/REQUIREMENTS.md` — AUD-01/AUD-02/HB-03/DET-05/EVD-06/PERF-01 and recurring CON-01/EVD-05 obligations.
- `.planning/PROJECT.md` — open owner decisions and current project boundaries.
- `.planning/LESSONS.md` — integration and verification lessons.
- `docs/policy/clean-room.md` and `docs/policy/exposure-record.md` — permitted research sources and exposure record.
- `docs/provenance.md` and `fixtures/suites/NOTICE` — rights and pinned-source handling.

### Existing machine and host contracts
- `include/lynxplosion.h` — public ABI, ownership and bounded advancement.
- `docs/timers-interrupts.md` — timer policy and event ordering.
- `docs/bus-video-policy.md` — immutable timing policy and pricing.
- `docs/snapshot-video.md` — fieldwise snapshot and compatibility boundaries.
- `docs/video-input.md` and `docs/input-replay.md` — raw video, logical input and replay checkpoint rules.
- `docs/public-suites.md` — suite observation and source-agreement distinctions.
- `docs/timer-video-revisit.md` and `docs/timer-source-index14-pending.json` — strict timer disagreement and narrow legacy descriptor.
- `.planning/phases/05-display-raw-video-and-input/05-CONTEXT.md` and `.planning/phases/05-display-raw-video-and-input/05-CLOSEOUT.md` — inherited Phase 5 choices and qualified checkpoint.

### Phase 6 research inputs
- `.planning/phases/06-audio-and-playable-checkpoint/06-AUDIO-DECISIONS.md` — primary audio/register/timer-ring sources; 40 kHz alternative.
- `.planning/phases/06-audio-and-playable-checkpoint/06-AV-CONTRACT-DECISIONS.md` — caller-span/FIFO alternatives, WAV and host checkpoint risks; its 48 kHz recommendation is superseded by D-03.
- `.planning/phases/06-audio-and-playable-checkpoint/06-EVIDENCE-DECISIONS.md` — pinned suite identities, exact oracle extents, source feasibility, controls and benchmark. Its stereo-roadmap reading is clarified by D-12.

Primary external references to verify: Epyx Handy audio chapter
https://www.monlynx.de/lynx/lynx7.html ; register appendix
https://www.monlynx.de/lynx/hardware.html ; timer chapter
https://www.monlynx.de/lynx/lynx8.html . No emulator implementation sources.
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `core/timer.c`, `core/machine.c`, `core/internal.h`: event candidates, fixed-zero timer phase and bounded advancement.
- `core/bus.c`: mapped device access and pure opcode peek must evolve together.
- `core/input.c`, `core/video.c`: guest-time input and raw video observations.
- `consumer/`: public-only consumer, strict timeline parser, owned bundle publication and placeholder WAV writer.
- `scripts/build-suites.sh`, `scripts/generate-suite-oracle.sh`, `tests/suites/`: pinned build, oracle, model, completion and fault evidence.

### Established Patterns
Transactional preflight, fieldwise snapshots, immutable policy identity, exact
CTest inventory, portable source/build recipes and private semantic mutants.

### Integration Points
Audio joins bus routing, the timer ring and same-tick scheduler. Snapshot/header
changes also touch API/CPU-vector/diagnostic/determinism tests and timer/video
private targets. Preserve all historical semantic checks while updating layout.
</code_context>

<specifics>
## Specific Ideas

An input-responsive original fixture produces both changing images and sound;
the native consumer writes inspectable PPM and WAV without taking desktop focus.
Independent stream checks and seeded faults make a silent or non-running guest
fail even if its artifact syntax is valid. No listening or hardware is required.
</specifics>

<deferred>
## Deferred Ideas

Host-rate conversion/filtering, live presentation, UART/page-mode unblock work,
physical audio calibration, shared LibFuturo integration, optimization and
release remain outside Phase 6. Record unresolved qualification honestly at
milestone close; historical fingerprint freshness is audit work, not permission
to rerun or silently bless prior phases during this continuation.
</deferred>
