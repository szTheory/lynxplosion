---
phase: 05-display-raw-video-and-input
plan: "09"
subsystem: testing
tags: [cmake, c, video, input, ctest, semantic-mutation]

requires:
  - phase: "05-08"
    provides: [video bus and palette contracts, logical input reads, edge-timing helpers]
provides:
  - Eight independent copied-source video and input mutants with exact semantic diagnostics
  - Owned scratch, sanitizer build, source/build identity, and clean-positive brackets for each mutant
  - Phase 5 device-fault evidence with the remaining Phase 6 audio obligation explicit
affects: [Phase 05 Plans 10-12, Phase 06 audio fault evidence]

actuals:
  tokens: 9734
  tasks: 2
  commits: 2
commits: 2
plan_head_before: ed90e6b195a97d25df73d63ce70c2c5733a09c7c
plan_head_after: e6afbbe3658545cbbf1def252457162aa2dca4dd

tech-stack:
  added: []
  patterns:
    - Exact-anchor copied-source mutations built with the current strict C17 sanitizer configuration
    - Invocation nonce, path containment, owner marker, exact diagnostic, and fresh guest brackets

key-files:
  created:
    - tests/video/fault_controls.cmake
    - docs/video-faults.md
  modified:
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Keep all eight faults as independent copied-source mutations with one named failing assertion each."
  - "Preserve EVD-05 as partial because the Phase 6 audio fault obligation remains open."

patterns-established:
  - "A mutation is evidence only after a changed copied-source hash, successful strict sanitizer build, exact exit 1, and one intended assertion."
  - "Clean guest positives run before and after each mutation; successful cleanup rechecks nonce ownership and path containment."

requirements-completed: []
coverage:
  - id: D1
    description: "DMA cost and request mutants fail their independent bus-grant assertions."
    requirement: VID-02
    verification:
      - kind: integration
        ref: "video.fault.dma_cost_zero, video.fault.dma_request_omitted"
        status: pass
    human_judgment: false
  - id: D2
    description: "A skipped first-row palette snapshot fails the palette-boundary assertion."
    requirement: VID-03
    verification:
      - kind: integration
        ref: "video.fault.palette_boundary"
        status: pass
    human_judgment: false
  - id: D3
    description: "Guest input reads detect A/B, Option, Pause, handedness, and one-tick sample faults."
    requirement: INP-01
    verification:
      - kind: integration
        ref: "input.fault.a_b_swapped, input.fault.option1_option2_swapped, input.fault.pause_register, input.fault.orientation, input.fault.edge_late"
        status: pass
    human_judgment: false

duration: 41min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 09: Independent Video and Input Fault Controls Summary

**Eight copied-source DMA, palette, input-map, handedness, and sample-tick faults now fail only at their named guest-semantic assertions.**

## Performance

- **Duration:** 41 min
- **Started:** 2026-11-12T09:22:54Z
- **Completed:** 2026-11-12T10:03:52Z
- **Tasks:** 2
- **Files modified:** 6
- **Actuals method:** 38,936 changed source/document characters across the six plan files, divided by four; planning metadata and this summary are excluded.

## Accomplishments

- Added three isolated video controls for DMA cost, active-row requests, and the palette row latch. The DMA-cost probe compares the full raw pixel, palette, and line-timestamp buffers.
- Added five real guest-read controls for A/B, Option 1/2, Pause, Lefthand orientation, and a one-tick-late input sample. Each positive guest observes a pressed and a released level.
- Bound the exact sorted CTest inventory to the existing repeatability claim without changing its scope or profile. Recorded source, build, policy, profile, assertion, binary, and runtime evidence in [docs/video-faults.md](../../../docs/video-faults.md).
- Kept all mutations in nonce-owned copies of current sources. Each of the eight controls built successfully with strict warnings and ASan/UBSan, failed at exactly one intended assertion with exit 1, passed a fresh clean guest before and after, and removed verified owned scratch.

## Verification

- `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^video\.fault\.(dma_cost_zero|dma_request_omitted|palette_boundary)$'` — passed all 3 video mutants.
- `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^input\.fault\.(a_b_swapped|option1_option2_swapped|pause_register|orientation|edge_late)$'` — passed all 5 input mutants.
- Final combined eight-mutant CTest run — passed all 8 in 4.13 seconds total wall time.
- `ctest --preset check --no-tests=error --output-on-failure -R '^api\.(input_bits|input_edges|input_orientation|video_bus_edges|palette_boundary)$'` — passed all 5 established regressions, including the input edge timing and video tie-order controls.
- An empty `FAULT_ID` and an unrecognized `FAULT_ID` both fail before scratch creation. `git diff --check`, sorted inventory validation, `sh scripts/check-claims.sh`, and `sh scripts/hygiene.sh --tree` passed.
- The full broad check remains assigned to Plan 05-12; no tests were skipped in this plan.

## Task Commits

1. **Task 1: Detect missing DMA cost/request and misplaced palette boundary** — `1113117` (`test`)
2. **Task 2: Detect input alias, handedness and one-tick sample faults** — `e6afbbe` (`test`)

## Decisions Made

- Keep each fault independent, with one exact source anchor and one named assertion; do not add emulator production fault flags.
- Leave the global EVD-05 requirement partial: this plan completes only its Phase 5 device-fault slice, while Phase 6 audio evidence remains outstanding.

## Deviations from Plan

None — the plan was executed as written.

## Issues Encountered

The first video-control attempt exposed a malformed CMake function argument list in the new harness. It was corrected before the Task 1 commit, then the focused and combined runs passed.

## Requirements and Next Phase Readiness

The requirements readiness query reported zero of the four listed requirements ready for closure while sibling Phase 5 and Phase 6 obligations remain open. No requirement checkbox was advanced. The Phase 5 slice for EVD-05 is documented; EVD-05 remains partial for Phase 6 audio. EVD-02 and CON-01 also remain open where their later obligations apply. No user setup or action is required.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*

## Self-Check: PASSED

The summary file exists, and both task commits are ancestors of the current HEAD.
