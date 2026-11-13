---
phase: 06-audio-and-playable-checkpoint
plan: "08"
subsystem: consumer-testing
tags: [c, cmake, av-replay, publication, ownership]
requires:
  - phase: 06-audio-and-playable-checkpoint
    provides: Complete real-guest A/V replay and in-memory host checkpoints
provides:
  - Malformed replay refusal with accepted dense timeline boundary
  - Exclusive complete-bundle publication and invocation-owned cleanup
  - Independent publisher race and named write/close/interruption controls
affects: [phase-06-plan-09, final-qualification]
actuals:
  tokens: 9572
  tasks: 2
  commits: 3
  plan_head_before: 11fe4656323d4879cf78bf32c7e5eb7a6b0a374a
  plan_head_after: 97927a62b8c17f5470797972f1634d3750d624ba
key-files:
  modified:
    - consumer/main.c
    - tests/consumer/av_driver.cmake
    - tests/consumer/tests.cmake
    - tests/inventory/fast.txt
    - docs/av-replay.md
    - docs/claims/skeleton-repeatability.claim
requirements-completed: [CON-01, HB-03, DET-05]
# Scoped controls only; sibling plans and final qualification govern global completion.
coverage:
  - id: D1
    description: Malformed inputs refuse before publication while the exact 4096-row boundary produces an independently valid bundle.
    requirement: HB-03
    verification:
      - kind: integration
        ref: "consumer.av_controls, consumer.replay_controls, consumer.refusal_controls on Clang and GNU"
        status: pass
    human_judgment: false
  - id: D2
    description: Independent publishers have one complete winner and named interruptions preserve foreign output and sentinels.
    requirement: CON-01
    verification:
      - kind: integration
        ref: "consumer.av_publication_controls, consumer.av_replay, consumer.frame_publication_controls on Clang and GNU"
        status: pass
    human_judgment: false
  - id: D3
    description: Inventory binding retains its named determinism evidence.
    requirement: DET-05
    verification:
      - kind: other
        ref: "det.two_runs, det.two_instances, det.golden; scripts/check-claims.sh"
        status: pass
    human_judgment: false
duration: 186min
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 08: Safe A/V Publication

The consumer refuses malformed replay inputs before output creation and publishes complete A/V bundles through invocation-owned staging.

## Changes and evidence

- Task 1 validates malformed tokens/numbers, overflow, duplicate/decreasing ticks, unknown input bits, file/count bounds, missing files, real file aliases, output-parent links and in-memory checkpoint identity/extent/prefix fields. Caller-owned sentinels remain unchanged. No persisted checkpoint-file interface was added.
- The accepted 4096-row dense timeline exposed the event capture bound: the public API reserves thirteen events while the host had only twelve slots left. The finite capture bound is now 32768. This preserves the legal input rather than retiming it. The boundary test retains final pre/post-edge checkpoint checks while avoiding redundant intermediate checkpoint work.
- Task 2 reuses stage directory identity plus an invocation marker, verifies ownership before writes and publication, tracks partial files for cleanup, and atomically publishes into an absent destination. Cleanup refuses changed or foreign staging.
- Two independent normal consumer processes race for one destination. Exactly one produces an independently validated complete bundle; the loser explicitly refuses. WAV/event/manifest write and close faults, prepublish interruption, existing output, foreign and markerless stages, replaced inode and final symlink controls preserve the expected sentinels and absent final outputs.
- Final native acceptance selector passed all six replay/refusal/publication cases in 31.65 seconds. The three named determinism CTests also passed, and the inventory-only claim binding passed its checker.
- GNU 13.3 with sanitizers on Linux/AArch64 fetched exact commit `97927a62b8c17f5470797972f1634d3750d624ba` into a clean tracked clone, configured and built successfully, and passed all nine corresponding consumer/determinism tests in 119.59 seconds. The claim checker and final clean tracked-tree assertion passed; the durable qualification exit status is zero.
- Native tree/history hygiene, diff whitespace and artifact scanning passed. The artifact scan examined 6380 files. Generated success scratch is removed only after canonical parent and invocation-marker checks.

These results establish software safety and repeatability for the named fixtures. They do not establish physical Lynx accuracy or complete Phase 6 qualification.

## Course correction and limitations

The original publication matrix repeated the full twelve-input replay for every publication fault. It exceeded the unchanged 240-second GCC limit. The publication workload now uses three released/pressed/released input rows with the same source-built guest, every output family and the same independent bundle checker. All ownership/fault assertions remain; the separate complete replay/checkpoint scenario is unchanged. The shorter native publication run took 6.23 seconds; the final GNU publication run took 30.42 seconds. These are observed test timings, not a controlled emulator throughput benchmark.

Earlier native CTest timeouts remain unexplained: direct children and minimal CMake wrappers returned correctly, and later normal-process-access sequential selectors passed. A solo pass does not establish contention or a runner root cause. A temporary cleanup trace referenced a publication-only variable in other cases; this separate diagnostic bug was guarded before the final native acceptance runs. No timeout limit, oracle or scanner policy was weakened.

Twenty-two attributable native failed-run directories were moved into private retained storage after canonical path, nonce/marker or exact fixture/control provenance checks and verified file inventories. The identified GCC timeout directory was similarly retained with identical sorted archive digests before and after its move. Failure evidence was preserved, not deleted; no untracked planning cache was staged or removed. Private log and retention locations are recorded in the session handoff, outside repository evidence.

Elapsed plan handoff time was about 186 minutes from the durable dispatch timestamp. This is not active model time or API usage. The actuals token value is a changed-diff-character/4 proxy across the six source/evidence files, not model usage.

## Commits

1. `f9936bd` — malformed A/V replay preflight and accepted boundary controls.
2. `8d79804` — complete A/V bundle ownership, publication and controls.
3. `97927a6` — remove unrelated full-replay duplication from publication faults.

## Next action

Continue Plan 09's fresh pinned audio/audio2 observations and five-suite inventory. Global requirement completion and the final canonical gate remain pending sibling plans and Plan 12. No user action is needed.
