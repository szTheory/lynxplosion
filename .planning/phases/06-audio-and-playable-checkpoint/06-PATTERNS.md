# Phase 6: Audio and Playable Checkpoint — Pattern Map

**Mapped:** 2026-11-12  
**Files/seams analyzed:** 9  
**Tracked analogs:** 8 / 9 (audio-specific implementation has no analog)

## File Classification

| New/Modified File or seam | Role | Data Flow | Closest tracked analog | Match |
|---|---|---|---|---|
| `core/audio.c` (new) | service | event-driven, transform | `core/timer.c` | partial; no sound implementation |
| `core/internal.h`, `core/timer.c`, `core/machine.c` | model, service, controller | event-driven, request-response | same files; `core/internal.h` event priorities | exact for scheduler/device seams |
| `include/lynxplosion.h`, `core/machine.c` | service API, controller | request-response | `lynx_advance_frame` | role-match |
| `core/snapshot.c` | utility | transform | `core/snapshot.c` fieldwise video/timer state | exact |
| `tests/api/test_api.c`, `tests/det/test_det.c` | test | request-response, event-driven | `tests/det/test_det.c` continuation cases | role-match |
| `consumer/main.c` | controller | event-driven, file-I/O | `run_frame_consumer`, existing snapshot/WAV publisher | role-match; WAV is placeholder |
| `scripts/build-suites.sh`, `scripts/generate-suite-oracle.sh`, `tests/suites/test_public_suites.c`, `tests/suites/run.cmake` | utility, test | batch, request-response | existing pinned CPU/timer suite path | exact |
| audio/audio2 suite wrappers and private audio fault control (new) | test | batch | `fixtures/suites/cpu-wrapper.s`, `tests/timers/fault_controls.cmake` | role-match |
| Phase 6 throughput driver/report (new; exact filename not fixed upstream) | test | batch | `tests/det/test_det.c` fixed workload/digest checks | partial |

## Pattern Assignments

### Audio device, timer ring, scheduler, and bus

**Analogs:** `core/timer.c`, `core/internal.h`, `core/machine.c`  
**Anchors:** `lynx__timer_borrow`, `lynx__timer_next`, `lynx__timer_apply`; `machine_advance`, `bus_opcode_peek`, `bus_request_available`, `bus_access`.

Timer register state stays instance-owned and advances only from guest ticks. The borrow implementation shows the current bounded chain and linked-counter update:

```c
/* core/timer.c:104-122 */
/* Drain one bounded borrow prefix. Timer links only move to a greater timer
   index, so a single ascending pass is a stable topological order and each
   timer can consume at most one input in this master-tick event. */
static uint8_t lynx__timer_borrow(lynx__timer_bank *bank,
                                  bool incoming[LYNX__TIMER_COUNT],
                                  uint8_t forced_index, bool expose_status)
{
    uint8_t borrow_mask = 0u;
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        if (!incoming[i]) {
            continue;
        }
        lynx__timer *timer = &bank->timers[i];
```

Reuse the device module shape and explicit `*_next` / `*_apply` split, not the timer's ascending-pass assumption: audio has a cycle and D-02 requires a bounded worklist with serialized deferred pulses. The existing priority contract is `core/internal.h:65-79` (`lynx__machine_event` and `LYNX__MACHINE_EVENT_*_PRIORITY`); extend ordering to device/link → sample → CPU while preserving timer/video/refresh order. In `core/machine.c:362-415`, add audio consistently to opcode peek and availability; `bus_access` must route reads and writes through the same device helpers. Keep opcode peek pure and extend `lynx__power_on` reset/load initialization. Check every copied-core source list when adding `core/audio.c`.

### Public caller spans and refusal behavior

**Analog:** `include/lynxplosion.h:246-304`, `lynx_frame_buffers`, `lynx_advance_frame`.

The existing public frame contract specifies exact descriptor sizes, required capacities, pointer alignment/overlap checks, no retained pointers, and no mutation on refusal (`include/lynxplosion.h:268-283`):

```c
/* include/lynxplosion.h:279-283 */
/* line_ticks must meet uint64_t alignment, and the descriptor, result, buffers
   and machine must have pairwise disjoint address ranges. The full declared
   capacities are checked for wrap and overlap before progress. Any invalid
   span/alignment/capacity or API error leaves the machine, result and buffers
   unchanged. The call retains no caller pointer. */
```

Use this contract shape for A/V advancement, then define audio event/sample capacities and stop precedence. Preserve transactional preflight for invalid spans; document and test any permitted partial-progress stop.

### Snapshot, digest, and replay continuation

**Analogs:** `core/snapshot.c:249-269, 568-739`; `lynx_state_digest`, `lynx_snapshot_restore`.

Snapshot payload fields are explicitly encoded in little-endian order and sized with compile-time layout assertions (`core/snapshot.c:88-121, 249-269`). Restore first builds and validates candidate timer/video state, then applies it fieldwise with no later refusal path (`core/snapshot.c:682-738`). The digest hashes canonical field records, not raw padded structs (`core/snapshot.c:272-330`). Follow those patterns for channel counters/LFSRs/integrators, link/deferred-pulse state, and sample ordinal/phase; bump the format and update raw-layout consumers. Host timeline cursor and output publication offsets remain outside core snapshots. For complete suffix comparison, adapt `replay_restore_checkpoint` at `consumer/main.c:1630-1669` and the existing continuation tests in `tests/det/test_det.c`.

### Consumer observations, WAV, and bundle publication

**Analog:** `consumer/main.c:706-720, 980-1085, 1960-2235`; `build_silence_wav`, `publish_outputs`, `run_frame_consumer`.

`build_silence_wav` lays out RIFF/PCM fields with explicit little-endian writers (`consumer/main.c:692-720`); reuse those encoding patterns in a separate writer for the locked stereo 40 kHz PCM stream, preserving the existing snapshot/silence regression under D-08. Keep the public-only replay loop's input timeline, guest-time bounds, raw/rendered stream digests, and fresh/interleaved/restore checks. Reuse the sibling staging directory, exclusive file writes, complete-directory publication, and owned cleanup in `publish_outputs` (`consumer/main.c:980-1085`) for the whole PPM/event/WAV/manifest bundle. Extend the artifact checker to prove exact PCM length/ordinals, non-silence, and input response; a valid WAV header alone is insufficient.

### Pinned suite and source-built evidence

**Analogs:** `scripts/build-suites.sh:114-159`; `scripts/generate-suite-oracle.sh:163-215, 236-280`; `tests/suites/test_public_suites.c:2769-2806`; `tests/suites/run.cmake`.

`build_suite` assembles a project wrapper against unchanged pinned assembly, then verifies the pinned source bytes remained unchanged (`scripts/build-suites.sh:131-159`). `parse_source` validates source shape, the exact result extent, expected rows, and full test-offset coverage before emitting the oracle (`scripts/generate-suite-oracle.sh:163-215`). Extend those paths for `audio/tests.s` and `audio2/tests.s`, 18 oracle rows each, and preserve the audio2 index-9 alternatives from D-10. Do not run the upstream C CRT main. `validate_guest_completion` is the concrete evidence pattern: require actual guest execution, semantic progress, return sentinel, nonzero work, and min/max guest ticks (`tests/suites/test_public_suites.c:2769-2806`). Add both suites exactly once to the runner/inventory and retain existing skips and timer disagreements unchanged.

### Private semantic fault harness

**Analog:** `tests/timers/fault_controls.cmake:10-56, 90-103, 120-147, 177-218`; `run_semantic_fault`, `run_clean_case`.

The harness names the intended assertion, copies sources to private scratch, requires one exact mutation anchor, builds a copied core, rejects compile/runtime/timeout failures as detection, then requires the intended assertion failure bracketed by clean positives. Use the same shape for named audio/LFSR, link, stereo, cadence, and phase faults. Include `core/audio.c` in the copied build source list, keep switches out of production, and ensure every mutant has a clean positive bracket.

## Shared Patterns and Footguns

- Keep audio state per machine and guest-tick-driven; never place host time, file positions, input cursors, or output pointers in the core.
- Extend mapped opcode peek, bus availability, granted bus access, and host mapped inspection together.
- Bump snapshot format/size with fieldwise serialization and update all raw-layout readers and private copied-core lists.
- Keep events, PCM, raw video, input, progress, and host files as separate streams with identities; same-core digests establish repeatability only.
- The new audio module, cycle-ring algorithm, and input-responsive A/V fixture have no sound-specific analog. Resolve those from the phase contract and research; do not present the timer chain or placeholder WAV as audio behavior.
- Keep the 40,000/1 Hz integer sample lattice and half-open target rule from D-03/D-04; current video frame capture is a caller-span example, not a source for audio cadence.

## No Analog Found

| File/seam | Role | Reason |
|---|---|---|
| `core/audio.c` implementation details | service, event-driven | No existing audio register, LFSR, mixer, event stream, or audio ring implementation. |
| Original input-responsive A/V fixture and throughput driver | test fixture, batch | Existing consumer covers video/input replay and placeholder silence; no audio fixture or phase-6 throughput driver exists. |

**Analog search scope:** core device/scheduler/API/snapshot; public consumer; source-suite builder, oracle, completion runner, and timer fault controls. All named existing analogs are git-tracked source paths.
