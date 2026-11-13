---
phase: 06-audio-and-playable-checkpoint
plan: "04"
subsystem: audio
tags: [c, lynx, timers, audio, snapshots, scheduling]

# Dependency graph
requires:
  - phase: 06-03
    provides: Four-channel digital audio state and exact channel/stereo behavior
provides:
  - Bounded, lossless timer/audio link propagation through the complete eight-node cycle
  - Snapshot-resumable ring debt and exact sample/event ordering on guest time
  - Checked no-capture sample phase advancement that retains real event frontiers
affects: [06-05, 06-06, 06-09, 06-10, 06-12]

# Actuals (#2632): 157,741 characters / 4 across 17 owned source/documentation paths.
# The measured ledger count includes four parent-owned follow-up commits in this range.
actuals:
  tokens: 39435
  tasks: 2
  commits: 6

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Stable fixed-budget ring worklist with retained per-node multiplicity and cursor
    - Checked sample-lattice fast-forward bounded by the next real machine event

key-files:
  created: []
  modified:
    - core/timer.c
    - core/audio.c
    - core/internal.h
    - core/machine.c
    - core/snapshot.c
    - include/lynxplosion.h
    - tests/api/test_api.c
    - tests/audio/test_audio.c
    - tests/audio/tests.cmake
    - tests/cpu/test_cpu_vectors.c
    - tests/det/test_det.c
    - tests/diag/test_diag.c
    - tests/golden/skeleton.digest
    - tests/inventory/fast.txt
    - docs/audio.md
    - docs/snapshot-video.md
    - docs/claims/skeleton-repeatability.claim
    - .planning/phases/06-audio-and-playable-checkpoint/06-04-PLAN.md

key-decisions:
  - "Use eight stable ring visits per master tick; defer any remaining pulses one tick with full multiplicity and an explicit due deadline."
  - "Store each node's ring debt as a checked 68-bit count and validate the aggregate reachable debt bound on restore."
  - "Persist ring debt, due work, sample phase and same-tick cursors in snapshot format 7; require room for the proven 13-event envelope."
  - "Keep the 400-tick sample lattice as software policy and arithmetically advance it only when PCM is not requested and no real event is skipped."

requirements-completed: [AUD-01, AUD-02, DET-05]
# Scoped ring/ordering evidence only; sibling plans and Phase6 qualification
# still control global completion of these requirements.

coverage:
  - id: D1
    description: "The full T1→T3→T5→T7→A0→A1→A2→A3→T1 ring preserves pulse multiplicity, terminates on an all-zero cycle, and resumes through snapshots."
    requirement: AUD-01
    verification:
      - kind: unit
        ref: "audio.links, audio.ring_zero, api.timer_links, api.timer_snapshot_controls"
        status: pass
    human_judgment: false
  - id: D2
    description: "Samples and same-tick device, link, watch and CPU work retain exact guest-tick order across slicing and no-capture calls."
    requirement: AUD-02
    verification:
      - kind: unit
        ref: "audio.cadence, audio.order, api.scheduler_order, api.scheduler_watch, api.scheduler_limits, api.frame_edges"
        status: pass
    human_judgment: false

# Metrics
duration: not captured
completed: 2026-11-13
status: complete
commits: 6
plan_head_before: 7b3cccf4ef2f13be1f0595e2c449f9fd180492c0
plan_head_after: d9cb169f36960a82f1e69ce5299137afe386abab
---

# Phase 6 Plan 4: Bounded Timer Ring and Sample Ordering Summary

**A lossless T1→T3→T5→T7→A0→A1→A2→A3→T1 ring now preserves deferred pulse debt through snapshots, with deterministic 400-tick sampling and event ordering.**

## Performance

- **Duration:** Not captured; the original start time was not recorded.
- **Completed:** 2026-11-13.
- **Tasks:** 2.
- **Files modified:** 19, including this summary.
- **Owned diff metric:** 157,741 characters across 17 source/documentation paths from `7b3cccf` through `ef222b4`, excluding plan metadata and this report; 39,435 characters/4 proxy tokens.
- **Commit count:** The measured `7b3cccf..d9cb169` ledger contains 6 commits: the two task commits below and four parent-owned integration/process commits (`4f0b0b8`, `fd4a2e8`, `a3731c7`, `d9cb169`).

## Accomplishments

- Replaced the forward-only audio-link assumption with a stable eight-node worklist. Each master tick permits eight ring visits; further work keeps its exact cursor and multiplicity and is delivered at an explicit next-tick deadline. Borrow propagation remains one-for-one even when a one-shot timer does not reload.
- Added a portable checked 68-bit debt representation and a cumulative restore bound covering natural and CPU roots. Snapshot format 7 serializes and validates the debt, due deadline, sample phase and same-tick cursors. The public event-span minimum is 13 entries for the proven per-tick envelope.
- Fixed the sample origin at ordinal 0/tick 400 and preserved exact ordering at sample, device, watch and CPU boundaries. Calls without a PCM span advance sample phase arithmetically only before the next actual event frontier, preserving legacy large-budget behavior.
- Added ring-edge, zero-cycle, repeated-root, snapshot continuation, arithmetic-headroom, output-capacity, AV tracer and legacy timer controls. Updated the owned raw readers, digest schema sensitivity and inventory bindings where the format and supported audio addresses changed.

## Verification

On native AppleClang, the ring/cadence focused groups, six authored audio regressions plus A3→T1 wake/AV tracing, the affected-reader group (21/21), the API/map controls (4/4), and the claim-template check passed. After the separate integration repairs described below, the parent reported the fresh macOS suite build/oracle/timer control group passed 13/13.

The exact committed source at `ef222b436294ba197dd8c9a0142975986130ec7a` was fast-forwarded into the existing Linux clone and rebuilt with GCC 13.3. The scoped 40-test regression set passed 40/40 in 16.31 seconds. The complete output is preserved at `/private/tmp/lynx-06-04-gnu-ef222b4.log`.

The first full registered CTest run on the Plan04 source reported 18 failures and 2 skips out of 213; it was not a green full-suite run. Parent follow-up repaired the stale timer descriptor bindings and the synthetic source-build fixture. Parent verified that the old and new default timer oracle, guest bytes and labels remained byte-identical (1,178-byte oracle SHA `08b5a4f4…`, 612-byte guest SHA `d766a240…`, 900-byte labels SHA `b1423949…`). The GNU affected group initially passed 12/13; `suite.timers.video_revisit_controls` exposed a platform-specific build-record digest in its control. After `d9cb169` derived the negative-control needle from its platform-local baseline, the pending and video-revisit controls passed 2/2 on both macOS and clean GNU 13.3; the other GNU cases were reused from the passing `fd4a2e8` run. The full 213-test suite was not rerun, so this summary does not claim a phase-wide green gate.

The original full-run per-case logs for the two hygiene controls were lost, so their initial timeout cause remains unknown. Parent ran two bounded 15-second traces that showed active progress. The unchanged serial scripts passed in 19.42 and 24.50 seconds; the unchanged registered CTests passed serially in 18.37 and 15.55 seconds (33.93 seconds total), with output at `/private/tmp/lynx-06-04-hygiene-registered.log`. No scanner source, timeout, or control was changed, and no unrelated process was stopped.

These results establish the planned software behavior and repeatability only. Physical timer/audio accuracy and phase-wide qualification remain open; the final canonical `check.sh` gate belongs to Plan12.

## Task Commits

1. **Task 1: Deliver all links including the closing A3→T1 edge without losing work** — `76df599` (`feat`).
2. **Task 2: Observe exact sample boundaries and stable simultaneous-event ordering** — `ef222b4` (`fix`).

## Files Created/Modified

- `core/timer.c`, `core/audio.c`, `core/internal.h`, `core/machine.c`, `core/snapshot.c` — bounded ring processing, debt validation, sample phase and snapshot state.
- `include/lynxplosion.h`, `docs/audio.md`, `docs/snapshot-video.md` — public event capacity and versioned software timing/snapshot policy.
- `tests/api/test_api.c`, `tests/audio/test_audio.c`, `tests/audio/tests.cmake`, `tests/inventory/fast.txt` — exact ring, cadence, capacity, legacy and snapshot cases.
- `tests/cpu/test_cpu_vectors.c`, `tests/diag/test_diag.c`, `tests/det/test_det.c`, `tests/golden/skeleton.digest` — affected-reader, copied-build and digest-schema controls.
- `docs/claims/skeleton-repeatability.claim`, `.planning/phases/06-audio-and-playable-checkpoint/06-04-PLAN.md` — inventory binding and approved exact scope metadata.
- `.planning/phases/06-audio-and-playable-checkpoint/06-04-SUMMARY.md` — execution evidence and limitations.

## Decisions Made

Ring propagation uses the fixed `ring-defer-1` software policy. It retains due-now work across half-open stops, watch returns and save/restore, and canonicalizes the inactive deadline. The sample lattice and 13-event minimum are deterministic API policy; neither establishes hardware timing accuracy.

## Deviations from Plan

No unapproved implementation deviations. The 68-bit aggregate debt proof, format-7 snapshot layout, public 13-event minimum, and exact reader/digest updates were approved scope expansions required by the planned correctness checks.

## Issues Encountered

The original integration run exposed stale Plan02 descriptor hashes and missing audio/audio2 inputs in its synthetic build root; the parent repaired those in `fd4a2e8` without changing the pinned suite oracle or guest bytes. The subsequent platform-local digest control repair is in `d9cb169`. The original full CTest failure log is no longer available, so no additional per-case cause is inferred.

## User Setup Required

None.

## Next Phase Readiness

Plan04's ring and ordering work is complete with focused AppleClang and exact-source GCC evidence. Plan02's scoped portability controls are repaired and passed; the parent owns phase tracking and successor dispatch. No phase-wide qualification or physical hardware qualification is claimed.

---
*Phase: 06-audio-and-playable-checkpoint*
*Plan: 04*
*Completed: 2026-11-13*

## Self-Check: PASSED

- The summary file exists and all six commits in the recorded plan ledger are ancestors of `plan_head_after`.
- The recorded commit count matches `git rev-list --count 7b3cccf..d9cb169`.
- The summary has no whitespace errors.
