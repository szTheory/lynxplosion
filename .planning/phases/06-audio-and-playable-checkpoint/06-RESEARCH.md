# Phase 6: Audio and Playable Checkpoint — Research

**Researched:** 2026-11-12  
**Domain:** Deterministic Atari Lynx audio, guest-time scheduling, bounded native observations, snapshots, and source-built evidence  
**Confidence:** HIGH for project seams and locked policy; MEDIUM for documented digital audio behavior; LOW for unqualified physical timing and analog behavior

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

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
  no BIOS/commercial assets, physical tests, release, new distribution
  channel, emulator implementation source exposure or shared host-contract
  decision.

### the agent's Discretion

Exact module seams, ABI field names, capture enablement, integer normalization,
event record fields, bounded-ring implementation and task decomposition are
routine engineering choices within these contracts. Resolve them once using
primary evidence and focused tests. Ask only for a materially new consequence
outside existing authority. Research alternatives are not extra owner gates.

### Deferred Ideas (OUT OF SCOPE)

Host-rate conversion/filtering, live presentation, UART/page-mode unblock work,
physical audio calibration, shared LibFuturo integration, optimization and
release remain outside Phase 6. Record unresolved qualification honestly at
milestone close; historical fingerprint freshness is audit work, not permission
to rerun or silently bless prior phases during this continuation.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|---|---|---|
| AUD-01 | Four audio channels (LFSR, integrator, reload, volume, link to timers) and stereo muting are emulated | Register/bus/scheduler seams; per-channel and stereo authored tests; bounded ring guidance. |
| AUD-02 | Host receives integer PCM at an exact rational rate derived from the 16 MHz clock, plus tick-stamped channel events; no float, filter or resampling inside the core | Locked 40 kHz / 400-tick sample lattice, separate event stream, bounded caller spans, ordered sample point. |
| HB-03 | The homebrew plays sound in response to input (the v1 playable checkpoint) | Public-only consumer vertical slice with input-responsive guest markers, frames, events and WAV. |
| DET-05 | Machine progress, logical input, raw video and raw audio are recorded as separate streams on one guest-tick timeline; same-core digests are labelled repeatability, not accuracy | A/V replay manifest and digest-per-stream, snapshot suffix comparison and distinct stream records. |
| EVD-06 | The lynx-tests `timers`, `cpu`, `page-mode`, `audio` and `audio2` suites run headlessly from pinned, source-built, startup-tier-A images with results captured and inventoried; any suite that cannot run that way is a visible, documented skip with its blocker | Reuse pinned builder, wrapper, C oracle and exact inventory; audio sources avoid the page-mode SERCTL blocker. |
| PERF-01 | A named workload (the homebrew with a fixed input timeline) has a recorded throughput baseline with its output digest held constant; no optimization in v1 | Five same-build fresh runs, constant output digest, monotonic in-memory timing; report median/range separately from consumer and CI time. |
| CON-01 | An independent consumer program, using only the public C API, drives the core and writes raw frames as PNG or PPM and audio as WAV | Add separate A/V consumer mode and validate WAV layout, sample ordinals, stream digests and input response; preserve existing frame tracer. |
| EVD-05 | Each device phase adds at least one named-fault control that is shown to fail the intended test | Private semantic mutants for audio semantics, stereo routes, sample cadence and serialized phase, each with clean brackets. |
</phase_requirements>

## Summary

Implement audio as machine-owned, guest-tick state integrated with the current event scheduler and Mikey bus. Keep the host API responsible for bounded publication of PCM, channel events and raw video. The critical first slice is one original input-responsive fixture running through the public API to an inspectable WAV plus frame artifact; then complete channel fidelity policy, immutable identity/snapshot continuation, public suite evidence and named faults. This follows D-01–D-17 and the three completed decision reports; it does not reopen their choices.

The main integration risk is not PCM file writing. The existing timer borrow walks a finite increasing-index graph, while the selected timer/audio ring closes through audio channel 3 back to timer 1. A bounded same-tick propagation rule must be implemented and serialized as software policy, or the guest can hang or lose pulses. Source-built audio/audio2 wrapper and oracle work is feasible at the current pins and should land early, before implementation integration. No Phase 6 runtime audio observation exists yet; prior phase evidence remains historical.

**Primary recommendation:** extend the existing core bus, scheduler, snapshot/digest and caller-owned output-span patterns; add a focused audio implementation module only if it simplifies that integration, and build the audio suites with wrappers over their unchanged assembly tests and pinned C oracles.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|---|---|---|---|
| Four audio channels, register reads/writes and link propagation | Deterministic core/device | Scheduler and mapped bus | Channel state advances only at guest ticks and must be isolated per machine. |
| Integer PCM and ordered channel events | Deterministic core observation | Public C API | The core defines sample phase/data; API spans publish records without retaining host pointers. |
| Input-responsive A/V replay and WAV/PPM | Host consumer | Public API | Parsing timelines and publishing files are host duties; the consumer must not reach into private core state. |
| Snapshot and replay continuation | Core state plus host checkpoint | API/determinism tests | Device state/phase is immutable machine state; script cursor and output ownership remain host state. |
| Pinned suites and named faults | Test/evidence harness | Build scripts and private copied-core targets | Evidence uses source-built guest code and project-authored controls, not emulator source exposure. |
| Throughput baseline | Benchmark harness | Public consumer | Time in-memory core work separately from artifact encoding and CI. |

## Standard Stack

| Component | Existing choice | Phase use |
|---|---|---|
| Core and public API | C17 project, CMake | Audio state, bus routing, scheduling, snapshot and API integration. No new package. |
| Build/test | CMake, CTest, `scripts/check.sh` | Canonical local gate and exact test inventory. |
| Guest source toolchain | Pinned cc65 `ca65`/`ld65` | Build unchanged `audio/tests.s` and `audio2/tests.s` with project wrappers. |
| Shell validation | POSIX `sh`, ShellCheck | Extend portable suite builder/oracle scripts and check them at both required ShellCheck versions. |
| Output | Project-owned RIFF/WAVE writer | Emit stereo signed-16 PCM at the locked integer rate; no audio library required. |

No dependency, package install or external binary fixture is needed. The current inspected host has CMake 4.4.3, Ninja 1.13.2, Apple Clang 21.0.0.21000101, ShellCheck 0.11.0 and pinned cc65 `V2.19` (`ca65`/`ld65`, revision `eebf54a`). `gcc` resolves to Apple Clang here; GNU GCC and ShellCheck 0.9 availability have not been verified. The checked-in CI uses Ubuntu 24.04. These are environment observations, not evidence from a build or test run.

## Project Constraints (from AGENTS.md)

- Keep deterministic machine state separate from host clocks, filesystem, physical controller discovery, A/V presentation and persistence. Define ownership, guest-time bounds, reset/unload, errors and refusal postconditions.
- Keep progress, logical input, raw video/palette, audio events/samples and host presentation as distinct observations. A same-core digest demonstrates repeatability, not hardware accuracy.
- Preserve named model/profile behavior; neither Lynx I nor Lynx II is an adopted baseline. Preserve profile `none`, LEGACY_FLAT5, opt-in VIDEO_POLICY1 and the timer disagreements.
- Do not use BIOS/commercial content, commit or redistribute unapproved rights-bearing assets, test physical hardware, publish, release or expose emulator implementation source. Keep owner decisions O1/O5/O6/O9 open.
- Keep dependencies few and meaningful; this phase needs no new dependency. Preserve source pins, fixture notice, recipe, hashes and allowed-use records.
- Use deterministic fresh state, semantic guest progress, exact inventories and visible skips. Use the documented fast gate, measure before adding CI/cache/concurrency, and optimize only measured bottlenecks.
- Routine reversible phase implementation and required local/CI checks are within existing authority. Respect the explicit scope and rights stops.

## Architecture Patterns

### Device and scheduler seam

Current scheduler priorities are declared verbatim as `LYNX__MACHINE_EVENT_TIMER_PRIORITY = 0u,`, `LYNX__MACHINE_EVENT_VIDEO_PRIORITY = 1u,`, `LYNX__MACHINE_EVENT_REFRESH_PRIORITY = 2u,`, and `LYNX__MACHINE_EVENT_CPU_PRIORITY = 3u,` [VERIFIED: `core/internal.h:73-78`]. `machine_advance` chooses timer, video-DMA, refresh and CPU candidates [VERIFIED: `core/machine.c:503-683`]. Add audio event and sample candidates to this common tick path, preserve all existing relative order, and implement D-04's device/link → sample → CPU order. A target at T remains half-open: records at T are pending for the next call.

The timer implementation currently stores per-timer incoming borrow state and drains only a finite increasing-index chain [VERIFIED: `core/timer.c:104-167`]. Reuse its counter/reload primitives only where semantics fit. Model the selected cycle with a deterministic queue/worklist and an explicit per-tick work bound. If a pulse is deferred at the bound, retain it in machine state and snapshots; never silently discard it. The exact ring propagation speed and physical reset/divider phase remain unknown, so the bounded rule is a versioned software policy, not a hardware claim. Audio chapter and timer-ring source: [Epyx-derived Chapter 7](https://www.monlynx.de/lynx/lynx7.html) and [Chapter 8](https://www.monlynx.de/lynx/lynx8.html).

### Bus integration

Route each audio register consistently through the machine's opcode-peek, bus-availability, bus-access and public mapped-read paths. The current seams are `core/machine.c` [VERIFIED: `core/machine.c:362-500`, `core/machine.c:865-906`]. Keep opcode peek side-effect-free. Extend mapped read preflight and device peek in the same change as writes, so instruction fetches, CPU reads and host inspection cannot disagree. Reset/load paths must initialize channel and link state [VERIFIED: `core/machine.c:104-135`]. The hardware register appendix identifies four channel blocks at `$FD20–$FD3F` and route-disable control at `$FD50` [CITED: https://www.monlynx.de/lynx/hardware.html]; preserve the software contract and avoid claiming physical stereo coverage.

### PCM and observations

Use the locked sample lattice: the first sample is absolute ordinal zero at guest tick 400; subsequent samples advance by 400 ticks with checked integer addition. At each sample point, finish channel/link work, mix using the explicitly documented bounded integer policy, then grant same-tick CPU work. Emit channel events separately, with tick and stable same-tick sequence/order metadata. This policy follows D-03/D-04; the selected 40 kHz is not a hardware rate. For signed-16 stereo PCM, Microsoft defines PCM block alignment as channels × bits/sample ÷ 8, giving four bytes per frame for this contract [CITED: [WAVEFORMATEX](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatex)]. RIFF uses chunk sizes/padding and a `fmt` plus `data` structure [CITED: [RIFF resource format](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-)].

Expose bounded caller-owned output spans. Preflight capacity, pointer range/overflow, alignment and overlap before mutating state for transactional refusal cases. Specify partial-progress and stop precedence, preserve pending records in machine state, and never retain host pointers. The host consumer owns file offsets and publication. Suggested scheduler shape (implementation sketch, not copied core code):

```text
while next_guest_event_tick < target_tick:
    advance_device_and_link_events_in_total_order()
    if next_sample_tick == current_tick:
        emit_sample(absolute_ordinal)
        checked_add(next_sample_tick, 400)
    process_same_tick_cpu_grant()
```

The output format/policy identity must bind rate, channels, sample format, lattice origin, mix policy and event ordering. Keep the host A/V checkpoint's input cursor/edge phase, timeline digest, next sample ordinal, output offsets and published-prefix identity outside the core snapshot.

### Snapshot and canonical identity

Current machine state has timer/video/input state but no audio [VERIFIED: `core/internal.h:106-124`]. Snapshot format is defined by the exact source line `#define LYNX__SNAPSHOT_FORMAT 5u` [VERIFIED: `core/internal.h:17`]. Add fieldwise audio state and output continuation fields required by D-07 to identity, digest, snapshot serialization and restore validation as one compatibility change. Restore must validate the complete candidate before applying any state [VERIFIED: `core/snapshot.c:1-56`, `core/snapshot.c:568-738`]. Add audio to the canonical digest at the same stable fieldwise point [VERIFIED: `core/snapshot.c:272-330`]. Bump the format and update all raw-layout readers/forgers in `tests/api/test_api.c` [VERIFIED: `tests/api/test_api.c:2400-2410`, `tests/api/test_api.c:2867-2875`], `tests/cpu/test_cpu_vectors.c` [VERIFIED: `tests/cpu/test_cpu_vectors.c:1185-1237`] and `tests/diag/test_diag.c` [VERIFIED: `tests/diag/test_diag.c:1198-1208`]. Preserve the public API continuation coverage in `tests/det/test_det.c` and update its suffix assertions [VERIFIED: `tests/det/test_det.c`].

Snapshot consumers must test pre/post input-edge and sample-boundary restores, then compare complete subsequent frame, event and PCM suffixes including ordinals. Include refusal atomicity, truncated/malformed snapshots, overflow boundaries and reset/load/unload. Keep the Phase 3 snapshot/silence and Phase 5 input-only tracer as separate regressions.

### Copied-core lists and test harnesses

The canonical CMake source list begins with `set(LYNX_CORE_SOURCES` [VERIFIED: `CMakeLists.txt:34`]. Private copied-core fault targets also carry explicit lists: `tests/timers/fault_controls.cmake` [VERIFIED: `tests/timers/fault_controls.cmake:95`, `tests/timers/fault_controls.cmake:145-146`] and `tests/video/fault_controls.cmake` [VERIFIED: `tests/video/fault_controls.cmake:186`, `tests/video/fault_controls.cmake:233-234`]. Add a new core audio source to all applicable source lists and to any new audio fault target; keep mutants private, with no production fault switch.

Extend `scripts/build-suites.sh`, `scripts/generate-suite-oracle.sh` and the existing suite runner rather than duplicating provenance/model/completion logic. Build the unchanged pinned `audio/tests.s` and `audio2/tests.s` through project-authored assembly wrappers and pinned ca65/ld65. Generate the 18 oracle rows for each suite from their pinned C sources; preserve the audio2 index 9 alternative `0x30|0x34` as D-10 specifies. Do not call the C test `main` as guest code: its TGI startup and non-returning loop make it unsuitable as the wrapper entry point. Clear zero page, establish a return marker/sentinel, require progress and return, enforce the existing guest-time bounds, capture exact output extent and raw bytes, then compare against the pinned oracle. Register each required suite/profile/fault once in the exact inventory. Audio assembly avoids SERCTL, so it does not inherit the page-mode skip. Keep the two existing page-mode skips and timer row-14 disagreements exactly as recorded in D-13.

The upstream audio suites do not touch MSTEREO; authored coverage must test every channel and both sides' mute bits. Each EVD-05 private mutant should have an exact semantic assertion and a clean-core positive bracket. A compile failure or generic timeout is not evidence that the intended mutant was detected.

### Public A/V consumer and throughput

Add a separate consumer mode using only the public C API, preserving the old frame tracer. Drive an original source-built fixture with recorded input and produce raw frames/PPM, channel-event records and real WAV. The checker should validate file structure, PCM block alignment and exact data length, sample ordinals, non-silence, per-stream digests, guest progress and a responsive input marker/image/audio difference. Exercise fresh, sliced, interleaved, watched/resumed and restored paths with equal suffixes at common guest ticks. This is software response/repeatability evidence, not hardware accuracy or subjective listening.

For PERF-01, run the same named fixture/input/build from fresh state five times. Hold guest work and all stream/output digests constant. Use a monotonic host timer only around in-memory core work; report median/range and environment. Report encoding/publication time and CI time separately. Do not optimize in this phase.

## Don't Hand-Roll

| Problem | Avoid | Use instead |
|---|---|---|
| Core audio output buffering | An unbounded or retained FIFO with host cursor in machine state | Bounded caller-owned spans and explicit preflight/back-pressure from D-06. |
| Snapshot compatibility | Raw struct copies or test-local hard-coded offsets | Existing fieldwise serializer, identity and validate-then-apply behavior. |
| Test provenance/oracles | New external suite or duplicated runner | Existing pinned suite source, NOTICE, wrapper, C oracle and inventory pattern. |
| Host audio conversion | Core float/filter/resampling or a host-rate bridge | Locked integer 40 kHz output policy; WAV consumer writes the chosen PCM directly. |
| Evidence interpretation | Calling an observation a pass/accuracy claim while O1 is open | Record raw bytes, comparison, profile/model evidence and qualification status separately. |

## Common Pitfalls

- **Ring propagation hangs or drops work.** A cycle differs from the present finite timer chain. Bound work per tick; serialize deferred work and add zero-reload fault coverage.
- **Same-tick off-by-one sample.** Define origin, absolute ordinal and half-open target first. Verify sample-before-CPU and event-before-sample at an exact boundary.
- **One access path misses audio.** Keep mapped opcode peek, bus availability, bus reads/writes and host mapped inspection aligned.
- **Snapshot edits miss consumers.** Update all three raw-layout test consumers and the copied timer/video fault source lists; keep `det` continuation stream checks.
- **Output refusal advances the guest.** Test undersized, overlapping, unaligned and overflowed spans against both state and output, with declared partial-progress cases tested separately.
- **Source-suite harness reports false success.** Require model probe, full zero-page initialization, real guest progress, return/sentinel and exact output extent. Never run the upstream CRT `main`; do not treat O1-open observations as compatibility qualification.
- **Audio suite mistaken for stereo coverage.** Add original stereo mute assertions and named faults; the two pinned suites do not cover MSTEREO.
- **Old result exceptions drift.** Preserve the exact row-14 timer disagreements and page-mode skip entries. Do not widen or add exceptions.
- **WAV syntax passes with silent or stale audio.** Validate payload digest, sample count/ordinal and input-responsive audio/guest markers independently of the header.
- **Benchmark mixes unlike work.** Keep core, consumer file I/O and CI time separate; compare only equal digest-bound runs.

## Assumptions Log

| # | Claim | Section | Risk if wrong |
|---|---|---|---|
| A1 | The bounded same-tick queue/worklist policy is a suitable software model for cycle propagation; the physical propagation timing remains unknown. | Device and scheduler seam | Different pulse ordering changes audio/timer observations. Keep it versioned and test the selected policy; do not claim physical qualification. |
| A2 | A project-owned WAV writer remains sufficient for the limited fixed PCM output. | Standard stack | A later consumer requirement could need a separate encoder, which is outside this phase and would require fresh scope review. |
| A3 | Current build directory and CTest target names should be taken from the configured build rather than assumed here. | Validation Architecture | Hard-coded selectors could run no tests or the wrong subset; exact inventory remains the final authority. |

## Open Questions

These are implementation facts to resolve through focused tests or document as software policy, not owner decision gates:

1. The primary Epyx audio chapter and register appendix contain ambiguous status/register details; verify exact CTLB low-nibble, `$FD25` tap and audio B3/status semantics before coding. The chapter gives conflicting analog cutoff values; filtering is out of scope. [CITED: https://www.monlynx.de/lynx/lynx7.html; https://www.monlynx.de/lynx/hardware.html]
2. Exact physical audio first-edge/reset-divider phase, ring propagation rate, analog mix/pan fidelity and board-specific stereo support remain unqualified. Preserve these as unknowns; do not infer physical behavior from software observations.
3. Choose and document the bounded integer mix/normalization and clipping policy, event record fields, queue bound and deferred-pulse handling as explicit software contract values. These are discretion items under D-02/D-03, not reasons to reopen the locked sample rate or FIFO choice.

## Environment Availability

| Dependency/tool | Required by | Available | Version/observation | Fallback |
|---|---|---|---|---|
| CMake | Core/test build | Yes | 4.4.3 | — |
| Ninja | Core/test build | Yes | 1.13.2 | CMake's configured generator |
| Clang | Local C compiler | Yes | Apple Clang 21.0.0.21000101 | CI compiler lane |
| GNU GCC | D-14 portability | Not verified | `gcc` resolves to Apple Clang | Required CI/other installed GNU environment; verify explicitly before claiming GCC coverage |
| ShellCheck | Shell script lint | Yes | 0.11.0 | — |
| ShellCheck 0.9 | D-14 portability | Not verified | — | Required separate environment; do not infer from 0.11 |
| Pinned cc65 tools | Source suites | Yes | `ca65`/`ld65` V2.19, revision `eebf54a` | Existing pinned tool binaries |
| Docker | Optional environment check | Present | 29.5.2; image tags were visible, no container run | Do not rely on image tag alone as proof of compiler version |

No build, test, container or package installation was performed for this research task. Current CI is configured on Ubuntu 24.04. Missing exact GNU GCC and ShellCheck 0.9 observations are the only portability-environment gaps; use the project-required environment before declaring both lanes covered.

## Validation Architecture

### Test Framework

| Property | Value |
|---|---|
| Framework | CMake/CTest plus POSIX shell harnesses |
| Config | Existing CMake project and test inventory; add phase cases to the canonical inventory. |
| Quick/final documented command | `sh scripts/check.sh` |
| Full suite | `sh scripts/check.sh` (the documented fast PR gate); use exact registered CTest inventory in each required compiler/lint environment. |

The explicit environment instruction for this research prohibited running builds or tests; no validation result is claimed. Do not add a new test framework or external dependency.

### Phase Requirements → Test Map

| Req ID | Behavior to prove | Test type | Future automated command | File exists? |
|---|---|---|---|---|
| AUD-01 | Four-channel registers/counters/LFSR/integrator/link and every stereo route-disable bit | Device unit + guest oracle + named faults | `sh scripts/check.sh` after adding exact audio targets and inventory entries | No Phase 6 audio tests yet; add them. |
| AUD-02 | Exact tick-to-sample phase, signed-16 stereo ordering, events, no float/filter/resample | API/scheduler tests; static forbidden-import check | `sh scripts/check.sh` | No audio API/scheduler tests yet. |
| HB-03 | Input changes semantic progress/marker, image evidence and non-silent WAV | Public API integration | `sh scripts/check.sh` | New source-built fixture and consumer mode required. |
| DET-05 | Distinct streams align on guest tick; fresh/sliced/interleaved/restored suffixes match | Determinism and snapshot integration | `sh scripts/check.sh` | Existing input/video coverage only; extend with audio/events. |
| EVD-06 | Exact five source suites, profiles, raw observations, oracle comparisons, controls and visible existing skips | Suite harness/inventory | `sh scripts/check.sh` | Extend builder, oracle, runner and inventory for audio/audio2. |
| PERF-01 | Five fresh same-build runs have identical output digest and recorded throughput | Benchmark/report | Run the project-owned benchmark command added for Phase 6; then `sh scripts/check.sh` | No Phase 6 benchmark yet. |
| CON-01 | Public-only consumer writes raw frames/PPM and WAV with input-responsive audio | Consumer smoke + artifact checker | `sh scripts/check.sh` | Existing frame/silence consumer remains; add a separate real A/V mode. |
| EVD-05 | Each audio semantic mutant fails its intended assertion and clean brackets pass | Private fault controls | `sh scripts/check.sh` | Add channel/LFSR, ring, stereo, cadence and snapshot-phase controls. |

### Required focused evidence

- Compare whole-call, sliced, interleaved, watched/resumed and snapshot-restored streams; test snapshots immediately before/after input edges and sample boundaries.
- Test API span refusal before mutation for invalid/overflow/overlap/capacity cases, and verify documented partial progress independently.
- Build audio/audio2 from unchanged source at current pins before device completion; keep source-byte identity and C-oracle provenance in artifacts.
- Run targeted GCC and Clang, ShellCheck 0.9 and 0.11 checks early, then one final canonical local gate in each required environment and hosted CI at exact pushed HEAD. This is planned validation, not research execution.
- Check real WAV fields/data length and stream digests, but also assert non-silent PCM and input-dependent semantic/audio changes.
- Record five same-build benchmark samples with equal output digests. Report in-memory core rate, consumer I/O and CI wall time separately.

## Security Domain

Security enforcement is enabled at ASVS level 1 in project configuration. The phase introduces no authentication, session or access-control surface. Relevant controls are input and memory safety at the C API and replay parser: validate lengths, pointer arithmetic, alignment, overlap, output capacities, malformed snapshots and event/sample counts before transactional state mutation. Keep host file parsing/publication outside the deterministic core; do not add filesystem, clock, thread or floating-point imports to core. Existing SHA-256 identifies content/streams; this phase adds no cryptographic primitive. These focused controls are not a claim of complete ASVS conformance.

## Sources

### Project primary sources

- `06-CONTEXT.md` and completed `06-AUDIO-DECISIONS.md`, `06-AV-CONTRACT-DECISIONS.md`, `06-EVIDENCE-DECISIONS.md` — locked policy, primary-source observations, pinned-suite feasibility and evidence shape.
- `core/internal.h`, `core/machine.c`, `core/timer.c`, `core/snapshot.c` — scheduler, bus, machine-state, timer and snapshot seams cited above.
- `include/lynxplosion.h`, `docs/snapshot-video.md`, `docs/input-replay.md`, `docs/public-suites.md` — public ownership, restore, replay and source-evidence contracts.
- `scripts/build-suites.sh`, `scripts/generate-suite-oracle.sh`, `CMakeLists.txt`, `tests/timers/fault_controls.cmake`, `tests/video/fault_controls.cmake` — existing pinned build/oracle and copied-core source patterns.
- `AGENTS.md`, `.planning/AUTONOMY.md`, `.planning/STATE.md`, `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`, `.planning/config.json` — owner constraints, phase mapping and validation/security settings.

### External primary documentation

- [Epyx-derived Lynx audio chapter](https://www.monlynx.de/lynx/lynx7.html) — channel digital behavior and documented ambiguities.
- [Epyx-derived hardware register appendix](https://www.monlynx.de/lynx/hardware.html) — mapped audio register blocks and stereo control.
- [Epyx-derived timer chapter](https://www.monlynx.de/lynx/lynx8.html) — timer/audio ring sequence. Physical timing remains unqualified.
- [Microsoft WAVEFORMATEX](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatex) and [RIFF resource format](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-) — PCM block alignment and RIFF chunk structure.

## Metadata

**Confidence breakdown:**
- Project integration seams: HIGH — inspected current source and project-owned contracts.
- Source-suite feasibility: HIGH — pinned inputs, wrappers/oracle shape and existing scripts were inspected; no build was run.
- Digital audio interpretation: MEDIUM — primary Epyx-derived documents and pinned software oracle, with explicit unresolved ambiguities.
- Physical timing/analog fidelity: LOW — no hardware run or accepted qualification evidence, explicitly out of scope.

**Research date:** 2026-11-12  
**Valid until:** Recheck source pins and project build/API seams when Phase 6 execution begins; hardware/physical claims remain unqualified rather than expiring.
