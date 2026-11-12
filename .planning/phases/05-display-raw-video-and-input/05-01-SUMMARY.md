---
phase: 05-display-raw-video-and-input
plan: "01"
subsystem: video-input
tags: [c, cmake, ctest, raw-video, logical-input, snapshot-v5]

requires:
  - phase: 04-timers-and-interrupts
    provides: Shared guest-tick timer scheduler, CPU bus ownership hold, snapshot format 4
provides:
  - Public timer-generated bounded raw-frame capture and logical input API
  - Original pinned source-built homebrew tracer with independent frame assertions
  - Snapshot format 5 carrying logical input and in-progress video capture
affects: [05-display-raw-video-and-input, 06-audio-and-playable-checkpoint]

actuals:
  tokens: 21548
  tasks: 2
  commits: 2
commits: 2
plan_head_before: 53232bcd09a5757e90e2380ed28255cc5d57d80e
plan_head_after: f8f7d653b2ef82606e08a2d65776211ae2485dd6

tech-stack:
  added: []
  patterns:
    - One guest-time scheduler arbitrates timer, video DMA, refresh and CPU grants
    - Core-owned bounded frame accumulator publishes only on the frame stop
    - Public consumer independently validates raw frame and guest progress

key-files:
  created:
    - core/input.c
    - core/video.c
    - fixtures/diagnostic/input_probe.s
    - fixtures/diagnostic/input_probe.cfg
    - fixtures/manifests/input-probe.manifest
    - tests/consumer/frame_driver.cmake
    - docs/video-input.md
  modified:
    - include/lynxplosion.h
    - core/internal.h
    - core/machine.c
    - core/timer.c
    - core/snapshot.c
    - consumer/main.c
    - tests/api/test_api.c
    - tests/header/header.c
    - tests/consumer/check_artifacts.c
    - tests/consumer/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "LEGACY_FLAT5 remains the default; VIDEO_POLICY1 is explicit, immutable, and unverified software timing policy."
  - "The public native packet contains raw pens, per-row raw palettes and guest line ticks; it adds no host presentation to the core."
  - "The native C API is this plan's boundary; O9/HOST-02 remains outside it."
  - "No change to run identity: another emulator appears only in the prohibition on importing constants."

patterns-established:
  - "Frame results use bounded caller-owned buffers that are preflighted and never retained."
  - "A timeline event is applied at its exact guest tick before subsequent guest work."
  - "Snapshot format changes preserve real in-progress state and reject old formats."

requirements-completed: []
requirements-partial: [VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02, CON-01]

coverage:
  - id: D1
    description: "The original source-built guest produces released and A-pressed public raw frames with independently checked guest progress."
    requirement: HB-02
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^consumer\\.frame_tracer$'"
        status: pass
    human_judgment: false
  - id: D2
    description: "The native API publishes bounded raw pen, palette and line-tick arrays and restores an in-progress frame identically."
    requirement: VID-04
    verification:
      - kind: integration
        ref: "consumer.frame_tracer: format-5 mid-capture restore and byte-identical replay"
        status: pass
    human_judgment: false
  - id: D3
    description: "Timer 2 defines the documented frame-complete stop and the public ABI pins its descriptors and policy identity."
    requirement: VID-05
    verification:
      - kind: unit
        ref: "api.header_selfcontained"
        status: pass
      - kind: integration
        ref: "consumer.frame_tracer"
        status: pass
    human_judgment: false

duration: 55min
completed: 2026-11-11
status: complete
---

# Phase 5 Plan 01: Original Input-Responsive Frame Tracer Summary

**A source-built guest now produces timer-bounded raw frames through the public C API and visibly changes a pen at the recorded A-input tick.**

## Performance

- **Duration:** approximately 55 minutes
- **Started:** 2026-11-11T19:29:04-05:00
- **Completed:** 2026-11-11T20:24:19-05:00
- **Tasks:** 2
- **Files modified:** 20 implementation, test, fixture, and documentation files

## Accomplishments

- Added `lynx_set_input` and `lynx_advance_frame` with preflighted caller spans, a bounded instance-owned capture, raw pens/palettes/line ticks, and frame stop metadata.
- Added explicit `LEGACY_FLAT5` and `VIDEO_POLICY1` timing identity; legacy remains the default and video/refresh cadence remains labelled unverified software policy.
- Built and ran an original rights-clean BS93 homebrew using pinned ca65/ld65. The consumer saw a zero-row synchronization event at tick 54,272, then 102-row frames over ticks 54,272–108,544 and 108,544–162,816. At the exact event tick 109,321, A changed the guest result from released to pressed and the first raw pen from `0x11` to `0x22`.
- Added format-5 fieldwise snapshot state for the timing policy, copied input mask, video registers, DMA/refresh cursors and active frame bytes. The consumer restores during a row capture and confirms byte-identical snapshot serialization and frame replay.
- Bound fixture source, config, pinned tool binaries and generated guest hash/size in its manifest; the test driver checks each identity before tracing. Updated the exact test inventory and claim inventory hash.
- Documented raw packet geometry, blank/visible row ordinals, address transfer, input mapping, timing policy and evidence limits. The strict D-10 page-mode recipe remains scheduled unchanged; the independent Phase 4 timer offset-14 `0D` versus `0C` mismatch remains pending the Phase 5 revisit.

## Task Commits

1. **Task 1: Run one original input-responsive frame path through the public consumer** — `b341cd7` (`feat`)
2. **Task 2: Pin the native ABI, policy identity and tracer regression boundary** — `f8f7d65` (`test`)

## Verification

- Task 1 tracer gate, before and after its commit: `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\\.frame_tracer$'` — passed both times.
- Task 2 focused set: `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(consumer\\.(frame_tracer|public_api|snapshot_ppm|silence_wav)|api\\.header_selfcontained)$'` — all 5 selected cases passed.
- Snapshot/identity seam: `api.snapshot_roundtrip`, `api.snapshot_refusal`, `api.timer_snapshot`, `api.timer_snapshot_controls`, and `api.identity` — passed after the format-5 updates.
- `sh scripts/hygiene.sh --tree`, `sh scripts/scan-binaries.sh --source --root .`, and `git diff --check` — passed; source scan found 0 exactly-512-byte files.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected the snapshot video-payload size and canonical identity output.**
- **Found during:** Task 1 focused API regressions.
- **Issue:** The first video snapshot header-size constant overran its serialized palette region by 32 bytes, and the appended identity field left ABI padding noncanonical for whole-struct equality.
- **Fix:** Set the fixed video header to its actual 72-byte layout and zero-initialize the public identity output before fieldwise assignment.
- **Files modified:** `core/internal.h`, `core/snapshot.c`, `core/machine.c`.
- **Verification:** Snapshot roundtrip/refusal/timer controls and identity tests passed; the focused tracer restored and replayed an in-progress capture byte-for-byte.
- **Committed in:** `b341cd7`.

**2. [Rule 3 - Blocking issue] Updated existing API regression assertions for the new snapshot and identity format.**
- **Found during:** Task 1 integration.
- **Issue:** Existing API checks encoded snapshot format 4 and the old identity layout, so they would not validate the new public policy identity and format-5 header.
- **Fix:** Updated version, size, timing-policy mismatch, offset and refusal assertions in `tests/api/test_api.c`, retaining refusal checks for old versions.
- **Files modified:** `tests/api/test_api.c`.
- **Verification:** The focused snapshot/identity seam set and Task 2 consumer/header set passed.
- **Committed in:** `b341cd7`.

**Total deviations:** 2 auto-fixed. Both were required for correct snapshot serialization and existing public API regression coverage; no new dependency or unrelated feature was added.

## Remaining Phase Obligations and Boundaries

- `VID-01`, `VID-02`, `VID-03`, `INP-01`, `API-06`, and `CON-01` remain partial at phase scope until their later-plan verification. `CON-01` also requires Phase 6 real audio; this plan retains the existing explicitly labelled silence output.
- EVD-02 timer-offset-14 strict comparison and recurring EVD-05 DMA fault evidence remain open for their owning plans. This tracer makes no page-mode suite or hardware-accuracy claim.
- O1 profile qualification, O5 licence, O6 clean-room policy, conditional O9/LibFuturo integration, firmware/game rights, physical hardware and release/publication stops remain unchanged.
- No new test was skipped and no verification command required by this plan was left unrun. The generated BLL, symbols and trace are confined to the invocation-owned `build/check/scratch/consumer/frame_tracer` directory.

## Next Phase Readiness

Plan 05-01 is ready for the remaining Phase 5 plans to extend timing controls, palette boundaries, input mappings, DMA fault evidence and strict suite observations. No user action is needed for this implementation slice.

## Self-Check: PASSED

The summary and all named primary artifacts exist. Both task commits are ancestors of the current HEAD, the measured plan ledger is 53232bcd09a5757e90e2380ed28255cc5d57d80e, and its two commits match the summary actuals. No TODO/FIXME or pending fixture hash was found in the changed implementation and contract files; the pre-existing untracked `.planning/state.json` remains untracked and unstaged.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-11*
