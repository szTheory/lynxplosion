---
phase: 05-display-raw-video-and-input
plan: "07"
subsystem: testing
tags: [c, cmake, ctest, input-replay, raw-video, repeatability]
requires:
  - phase: 05-display-raw-video-and-input
    provides: Timer-generated raw frames, logical input sampling, snapshot and silence consumer modes
provides:
  - Strict canonical host input timeline parsing, hashing, exact-tick replay, and explicit checkpoint cursors
  - Original input-responsive diagnostic and independent all-frame raw/rendered repeatability qualification
affects: [05-display-raw-video-and-input, consumer, input, raw-video, repeatability]
actuals:
  tokens: 24754
  tasks: 2
  commits: 4
plan_head_before: bd1223293d262faff6587c4f2ac7c8f94eaee3e2
plan_head_after: dd993011a39f305ee1b355a8e6226fdb4c770404
tech-stack:
  added: []
  patterns: ["bounded canonical replay with explicit edge cursors", "independent frame transcript and artifact digest checks"]
key-files:
  created:
    - fixtures/diagnostic/input_probe_tracer.s
  modified:
    - consumer/main.c
    - tests/consumer/frame_driver.cmake
    - tests/consumer/check_artifacts.c
    - tests/consumer/tests.cmake
    - fixtures/diagnostic/input_probe.s
    - fixtures/manifests/input-probe.manifest
    - docs/input-replay.md
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
key-decisions:
  - "Keep the historical A-only strict tracer fixture separate so frame_tracer retains its pen, palette, timing, and guest-sample positive assertions while the full input probe grows to nine logical inputs."
  - "Check exact visible patterns only on settled frames; during observed software CPU/DMA overlap, check the bounded transitional pen/color set while retaining per-frame guest samples and full raw transcript comparisons."
  - "Observe two final frame periods with a 32-frame bound so the last held input has a settled endpoint frame."
patterns-established:
  - "Validate and hash the complete bounded timeline before creating output; apply each edge only when guest time equals its requested tick."
  - "Compare raw frame metadata and all pen/palette/tick observations across sequential and interleaved runs, separately binding rendered PPM artifacts."
requirements-completed: []
coverage:
  - id: D1
    description: "Canonical input timelines replay at exact ticks, including frame boundaries and restored pre/post-edge cursors."
    verification:
      - kind: integration
        ref: "consumer.replay_edges and consumer.replay_restore"
        status: pass
    human_judgment: false
  - id: D2
    description: "The original guest responds to the recorded input scenario and fresh/interleaved consumers reproduce every raw frame and rendered artifact."
    verification:
      - kind: integration
        ref: "consumer.frame_replay"
        status: pass
      - kind: integration
        ref: "consumer.frame_tracer, consumer.snapshot_ppm, consumer.silence_wav"
        status: pass
    human_judgment: false
duration: 55min
completed: 2026-11-12
status: complete
commits: 4
---

# Phase 05 Plan 07: Canonical Input Replay and Complete Input Frame Qualification Summary

**A bounded canonical timeline now drives exact-tick input edges, and an independent consumer verifies the original guest’s input-responsive raw frames and rendered artifacts across sequential and interleaved runs.**

## Performance

- **Duration:** 55 minutes, measured from the first task commit through verification and closeout preparation.
- **Started:** 2026-11-12T02:09:45-05:00
- **Completed:** 2026-11-12T03:04:14-05:00
- **Tasks:** 2
- **Files modified:** 10 implementation, fixture, test, and documentation files (one new tracer fixture).
- **Actuals method:** The 99,016-character implementation diff from `plan_head_before` through `plan_head_after`, divided by four, yields 24,754 tokens. This reproducible diff-size proxy is not model/API token usage.

## Accomplishments

- Added strict timeline preflight, canonical-byte hashing, exact-tick edge application, frame-budget handling, and explicit pre-edge/post-edge snapshot cursor rules. Malformed or over-limit input is rejected before output creation.
- Expanded the project-authored guest diagnostic to cover released, A/B, four directions, both options, Pause, a chord, and release under both handedness mappings. The independent checker validates real guest sample values and semantic progress on every emitted frame.
- Bound each frame’s raw pens, palette, ticks, metadata, and rendered PPM to separate digests. Sequential fresh runs and interleaved instances were compared against each other and the sequential baseline across the complete transcript. A controlled final-edge mutation preserved earlier settled frames and changed the expected final settled frame.
- Kept the original `consumer.frame_tracer` positive coverage by using a separate A-only tracer guest. Snapshot/PPM and silence/WAV modes remain registered and passing.
- Transitional rows exposed software CPU/DMA overlap: early pixels can contain a mixture of adjacent input patterns. The checker therefore requires exact pattern equality on settled frames and bounds transitional pens to the documented source patterns, cross-checking their PPM colors against raw pen/palette bytes. A corrupted transitional color and a separately corrupted raw pen are both rejected.

## Automated Results

Both exact plan commands passed with the `check` preset and sanitizers enabled:

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(replay_edges|replay_restore|frame_tracer)$'
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(frame_replay|frame_tracer|snapshot_ppm|silence_wav)$'
```

The first selected three cases passed; the second selected four cases passed. The final replay run covered 12 timeline records and 23 frames, both handedness mappings, and Lynx I/II software profiles. Tree hygiene, whitespace validation, and the claim inventory validator passed. Claim inventory digest: `d48d29f0e8dcf2a373d09a178101712b996b00111df89e25c91ccc73f3065590`.

## Evidence Limits

The transitional-row oracle characterizes the current software CPU/DMA overlap and proves deterministic rendering and mutation sensitivity. It does **not** qualify tearing behavior against physical Lynx hardware. The endpoint observes two frame periods after the final input edge, with a 32-frame hard cap; exceeding that cap fails visibly rather than dropping frames.

## Task Commits

1. **Task 1: Validate canonical timelines and drive every event at its exact tick** — `52dbac7` (test scaffold), `b6ed27d` (implementation).
2. **Task 2: Render and independently qualify the complete recorded homebrew scenario** — `75d2081` (test scaffold), `dd99301` (implementation).

The plan commit count is measured from `bd1223293d262faff6587c4f2ac7c8f94eaee3e2` through implementation commit `dd993011a39f305ee1b355a8e6226fdb4c770404`: four commits. The plan ledger was absent after the first task commits and was reconstructed from the parent of the first task commit before measuring this range.

## Files Created/Modified

- `consumer/main.c` — bounded preflight and exact-tick replay driver with frame-limited endpoint handling.
- `tests/consumer/frame_driver.cmake`, `tests/consumer/check_artifacts.c`, `tests/consumer/tests.cmake` — replay scenario orchestration, independent raw/PPM parsing, frame comparisons, and negative controls.
- `fixtures/diagnostic/input_probe.s` — nine-input original guest probe and source-linked result samples.
- `fixtures/diagnostic/input_probe_tracer.s` — preserved A-only positive tracer scenario.
- `fixtures/manifests/input-probe.manifest` — source, configuration, tool, and output identity records for both guest fixtures.
- `docs/input-replay.md` — host timeline, checkpoint, and output contract.
- `tests/inventory/fast.txt` and `docs/claims/skeleton-repeatability.claim` — exact test inventory and rebound claim digest.

## Decisions Made

- Maintain separate full-scenario and historical tracer guest fixtures so new pattern coverage does not erase existing strict tracer assertions.
- Treat transitional pen/color values as a bounded software observation while retaining full-frame repeatability and guest-sample assertions; do not present this as hardware tearing evidence.
- Keep all input classes in the main timeline and the exact collision/frame-boundary cases in the dedicated replay-edge tests.

## Deviations from Plan

### Auto-fixed Issues

**1. Preserved strict tracer assertions with a dedicated A-only diagnostic**
- **Found during:** Task 2.
- **Issue:** The expanded nine-input fixture changes the original A-only byte pattern used by the existing tracer’s exact pen and palette checks.
- **Fix:** Added a separate source-built A-only guest and manifest identity so the original tracer remains a positive, strict control rather than losing an assertion class.
- **Files modified:** `fixtures/diagnostic/input_probe_tracer.s`, `fixtures/manifests/input-probe.manifest`, and the consumer test driver.
- **Verification:** `consumer.frame_tracer` passed with the original pen, palette, timing, and guest-sample assertions.
- **Committed in:** `dd99301`.

**2. Adjusted the independent oracle for observed transitional rows**
- **Found during:** Task 2.
- **Issue:** CPU/DMA overlap in the software model produces transitional rows containing pixels from adjacent input patterns, so applying the settled-frame exact-pattern oracle to those rows is invalid.
- **Fix:** Retained exact pattern checks on settled frames; bounded transitional raw pens to the expected adjacent patterns, cross-checked PPM colors against raw pen/palette data, and added corrupted-color and corrupted-pen negative controls.
- **Files modified:** `tests/consumer/check_artifacts.c`, `tests/consumer/frame_driver.cmake`, `tests/consumer/tests.cmake`, `docs/input-replay.md`.
- **Verification:** `consumer.frame_replay` and its controlled mutations passed; raw transcript equality still covers every frame.
- **Committed in:** `dd99301`.

**3. Repaired the missing plan commit ledger from existing history**
- **Found during:** Summary preparation.
- **Issue:** The required per-plan ledger had not been created before the first task commit.
- **Fix:** Reconstructed its base from the parent of the first task commit, then measured all four plan commits from that immutable base.
- **Verification:** `git rev-list --count bd1223293d262faff6587c4f2ac7c8f94eaee3e2..dd993011a39f305ee1b355a8e6226fdb4c770404` returned 4.

**4. GSD state verbs refreshed the pre-existing untracked state cache**
- **Found during:** State closeout.
- **Issue:** The required SDK state updates rewrote `.planning/state.json`, despite the dispatch instruction to preserve that untracked file.
- **Fix:** Kept the refreshed cache in place and untracked/unstaged. Saved a private copy of its current bytes for recovery; no exact pre-update byte copy was available, so I did not fabricate a restore.
- **Verification:** The current cache's hash matches the private copy and its Phase 05 progress matches `STATE.md`; the earlier byte hash is recorded by the preceding history-repair record.
- **Future protection:** Back up and hash the cache before GSD state verbs, then use a cache-preserving state path or restore the exact backup if byte preservation is required.

**4 deviations** (two oracle/fixture adjustments, one bookkeeping repair, one metadata side effect). **Impact:** Existing positive coverage and all planned evidence classes remain intact; no later-plan scope was added. The state-cache side effect is recorded below.

## Requirement Status

`requirements.ready-ids` reported all six plan requirements as blocked by remaining Phase 05 obligations (`API-06`, `INP-01`, `HB-02`, `CON-01`, `VID-04`, `VID-05`). They remain partial in project tracking until the owning phase obligations are ready; this plan’s specific replay and consumer deliverables are covered above.

## Issues Encountered

The transitional-row oracle is intentionally limited to the current software model; hardware tearing remains unqualified. During the required GSD `state.*` metadata updates, the SDK rewrote the pre-existing untracked `.planning/state.json` cache. Its recorded pre-update SHA-256 was `c8a1c6259216b58c9fed91d24d4ca01c9abaf3c3f098bdbd9e8ce88e21792457`; the current 974-byte GSD cache hashes to `fdc817b903b71179c6513ccf4a5512fd0571213ad0eb7b56ebada55a7de669ee` and reflects the updated Phase 05 progress. No exact pre-update byte copy was present in the checkout or the attributable temporary backup directory, so the field-level change cannot be reconstructed safely. The cache remains untracked and unstaged. For future runs, copy and hash the cache before GSD state verbs, then use a cache-preserving update path or restore from that exact copy when byte preservation is required.

## User Setup Required

None. No external service, firmware, commercial ROM, physical hardware, or new dependency was used.

## Next Phase Readiness

Plan 05-07 is complete. Phase 05 remains in progress: later plans and the remaining requirement evidence still need to close. The replay driver and consumer can now supply deterministic input-responsive frame observations for those plans.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists at the plan output path.
- All four task commits are ancestors of the current HEAD.
- Plan commit count and head boundaries were measured from the repaired ledger base.
