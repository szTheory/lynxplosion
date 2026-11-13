---
phase: 06-audio-and-playable-checkpoint
plan: "07"
subsystem: consumer-testing
tags: [c, cmake, ctest, av-replay, checkpoints, pcm]

requires:
  - phase: 06-audio-and-playable-checkpoint
    provides: Public A/V advancement, exact-build snapshots, and the source-built av_probe fixture
provides:
  - Complete public av-replay transcript with input, progress, frame, event, and PCM streams
  - In-memory host checkpoint paired with a core snapshot and exact restored-suffix comparison
  - Independent complete-bundle syntax and semantic validation with named response controls
affects: [phase-06-plan-08, av-replay-consumers, repeatability-evidence]

actuals:
  tokens: 35284
  tasks: 2
  commits: 4
  plan_head_before: 0c678e3e30da583b6bfe67ef27070271b7f02a16
  plan_head_after: 15c09f505603df68b547627098ea482ac16f64eb

tech-stack:
  added: []
  patterns:
    - Explicit bounded records for applied inputs and guest progress
    - Host publication state kept beside, not inside, the core snapshot
    - Real-bundle identity verification separated from semantic mutation checks

key-files:
  created:
    - tests/consumer/check_artifacts.c
    - docs/av-replay.md
  modified:
    - consumer/main.c
    - tests/consumer/av_driver.cmake
    - tests/consumer/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - .planning/phases/06-audio-and-playable-checkpoint/06-07-PLAN.md

key-decisions:
  - "The checkpoint is an in-memory host sidecar paired with an exact-build core snapshot; it does not resume from a persisted checkpoint file."
  - "The producer emits complete bounded streams, and the independent C checker validates syntax and semantics while the driver checks manifest and bundle hashes."
  - "The test-only interleaving control compares fresh instances without changing the normal producer's single transcript."

patterns-established:
  - "Applied-input records are emitted only after successful public input calls at the declared guest tick."
  - "Guest-progress records retain every completed frame's tick range, input and guest response markers."
  - "Real positive bundles are hash-checked; manufactured mutation copies enter the semantic checker directly and must fail at their named assertion."

requirements-completed: [HB-03, DET-05, CON-01]

coverage:
  - id: D1
    description: Complete input replay, watched stop/resume, and core-plus-host checkpoint restore reproduce the uninterrupted bundle and suffix.
    requirement: HB-03
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_replay|av_response_controls|av_tracer|frame_tracer|snapshot_ppm|silence_wav|av_edges|av_restore)$|^det\.(two_runs|two_instances|golden)$'"
        status: pass
    human_judgment: false
  - id: D2
    description: The independent consumer checks complete A/V records and rejects malformed, stale, silent, reordered, or non-responsive evidence at named semantic stages.
    requirement: CON-01
    verification:
      - kind: integration
        ref: "GNU CTest on exact commit 15c09f505603df68b547627098ea482ac16f64eb: consumer.av_replay and consumer.av_response_controls"
        status: pass
    human_judgment: false
  - id: D3
    description: The inventory-bound repeatability claim remains valid for the registered test inventory.
    requirement: DET-05
    verification:
      - kind: other
        ref: "sh scripts/check-claims.sh; det.two_runs, det.two_instances, and det.golden"
        status: pass
    human_judgment: false

duration: unknown
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 07: Public A/V Replay Summary

**The public `av-replay` consumer now records and restores complete input, progress, frame, audio-event, and PCM streams with independent bundle validation.**

## Performance

- **Duration:** Not measured across the continuation handoff; the original start timestamp was not retained.
- **Started:** Unknown (not preserved across the task continuation).
- **Completed:** 2026-11-13T06:03:17Z
- **Tasks:** 2
- **Files modified:** 8, including the approved Plan 07 task-scope update.
- **Actuals proxy:** 141,136 changed-diff characters ÷ 4 = 35,284 tokens; four task commits measured from the persisted plan ledger.

## Accomplishments

- Extended the existing `--mode av-replay` using its strict `LYNX-INPUT 1` parser and source-built `fixtures/diagnostic/av_probe.s`. The single advancing transcript retains every applied input, guest-progress/frame record, raw frame and palette/line-tick record, PCM sample, and audio event; released and pressed previews remain semantic samples.
- Added a versioned manifest and explicit `input.applied` and `guest.progress` streams with byte counts and SHA-256 identities. The host sidecar checks timeline digest, length, cursor, offsets, mask, phase, output ordinals and offsets, and published-prefix identity before restore or append.
- Verified uninterrupted versus restored complete suffixes and bundles, including a valid watch/clear/resume path. The checkpoint pairs host cursor state with the exact-build public core snapshot and is intentionally in-memory only.
- Added an independent C parser for the complete records, frame/palette/PPM mapping, event ordering, PCM/WAV structure, sample ranges, manifest schema, and expected guest markers. The real-bundle driver checks manifest/per-stream and combined-bundle identity before invoking this checker. The C checker itself does not compute hashes.
- Added named no-run, over-bound, changed-input, silence, stale pre-input audio, truncated/extra WAV data, header-only WAV and frame, sample-count, frame metadata, bad-marker, and event-order controls. Manufactured copies invoke the semantic stage directly; each control asserts its specific failure stage, and the real guest bundle passes before and after the corpus.
- Bound the output capacity to the required scenario plus its final-frame completion window, with overflow and refusal coverage. Test-only interleaving compares two fresh instances while normal production still emits one transcript.

## Verification

- Clang focused CTest inventory passed all 11 selected replay, response-control, tracer, frame, snapshot, silence, edge, restore, and repeatability selectors.
- GNU 13.3 on Linux/AArch64 passed the affected `consumer.av_replay` and `consumer.av_response_controls` selectors after fetching and fast-forwarding to exact commit `15c09f505603df68b547627098ea482ac16f64eb`; both passed, and the tracked tree was clean before configuration.
- The existing `det.two_runs`, `det.two_instances`, and `det.golden` evidence was rerun after the inventory change. `sh scripts/check-claims.sh` and `git diff --check` passed; only the claim's inventory hash was refreshed.
- The full scenario reaches guest tick 1,248,256 and retains 22 frame/progress records and 3,120 PCM frames. Guest response is established by the fixture's JOYSTICK/SWITCHES/display markers, changed rendered pixels, and post-input non-silent audio. The saturating progress counter establishes progress but is not expected to differ between input states.
- These results establish same-build repeatability and the stated fixture behavior. They do not establish hardware accuracy; the final canonical qualification remains scheduled for Plan 12.

## Task Commits

1. **Task 1: Replay every input edge and restore core plus host publication cursor** — `2911c2999d263757615fc25c082c9ca7e71e48e8` (`feat`)
2. **Task 1 portability follow-up** — `9392dfd2567bf5388fed87db6a5ee2f6d4900043` (`fix`)
3. **Task 2: Independently validate the complete input-responsive A/V bundle** — `50f3f58d286dc9c060d041e0a7b357e9d39fbb43` (`feat`)
4. **Task 2 response-control follow-up** — `15c09f505603df68b547627098ea482ac16f64eb` (`test`)

The guarded summary commit records the plan metadata separately.

## Files Created/Modified

- `consumer/main.c` — complete replay, host checkpoint validation, output streams, bounded PCM capture, and test-only interleaving comparison.
- `tests/consumer/av_driver.cmake` — real-bundle identity checks and exact-stage semantic mutation controls.
- `tests/consumer/check_artifacts.c` — independent A/V stream and payload parser.
- `tests/consumer/tests.cmake` and `tests/inventory/fast.txt` — registered replay and response-control selectors.
- `docs/av-replay.md` — timeline, stream, manifest, checkpoint, identity, and validation contract.
- `docs/claims/skeleton-repeatability.claim` — refreshed inventory identity only.
- `.planning/phases/06-audio-and-playable-checkpoint/06-07-PLAN.md` — approved Task 2 source ownership and bounded-capture action.

## Decisions Made

- Checkpoint restore remains an in-memory host-side sidecar plus exact-build core snapshot; no durable checkpoint-file resume is claimed.
- The driver owns real-bundle hash verification. The C checker owns independent syntax and semantic assertions. Mutation checks bypass hash identity checks and match exact semantic stages, bracketed by clean real-bundle passes.
- A separate applied-input transcript and complete guest-progress stream preserve event and frame evidence that a timeline hash or preview images alone cannot provide.

## Deviations from Plan

### Auto-fixed issue

**1. [Rule 3 - Portability] Use a portable zero initializer for frame captures**
- **Found during:** Task 1 GNU compiler checks.
- **Issue:** The nested array initializer triggered a compiler portability warning under the project's strict build flags.
- **Fix:** Changed `{{0}}` to the equivalent portable `{0}` initializer.
- **Files modified:** `consumer/main.c`.
- **Verification:** Clang and GNU CMake/CTest checks passed.
- **Committed in:** `9392dfd2567bf5388fed87db6a5ee2f6d4900043`.

### Owner-approved scope adjustment

Task 2 added `consumer/main.c` to its owned paths for the scenario-derived PCM bound and the test-only interleaving comparison hook. The normal replay path remains a single transcript; no production fault switch or new mode was added.

**Total deviations:** 1 auto-fixed portability issue and 1 owner-approved file-scope adjustment. **Impact:** Both were narrowly required to pass the existing GNU build and complete the plan's bounded response comparison.

## Issues Encountered

- The GNU clone initially lacked the latest source commit. It was advanced using `git fetch` and `git merge --ff-only` with the exact full commit SHA, then reconfigured and tested. No source overlay was used.

## Next Phase Readiness

Plan 08 can consume the complete A/V streams and ephemeral checkpoint contract. Plan 12 still owns final canonical qualification. No user action is needed for this plan.

## Self-Check: PASSED

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-13*
