---
phase: 05-display-raw-video-and-input
plan: "04"
subsystem: api
tags: [c, frame-api, logical-input, raw-video, ctest]

requires:
  - phase: 05-display-raw-video-and-input
    plan: "03"
    provides: video bus scheduling and grant ownership
provides:
  - Full-span transactional refusal coverage for frame buffers and logical input
  - Explicit nonframe stop metadata, frame/watch ordering, and single deferred-frame delivery
  - Reset/load coverage for partial capture, input, watches, and independent instances
affects: [05-05-input, 05-06-video-snapshot, 05-07-frame-replay, 05-12-final-gates]

actuals:
  tokens: 11799.5 # Deterministic chars/4 proxy over the realized source diff; not model/API token usage.
  tasks: 2
  commits: 2
  realized_diff_chars: 47198
  measurement_command: "git diff --no-ext-diff --unified=0 953df60f..05faa42f -- tests/api/test_api.c tests/api/tests.cmake tests/inventory/fast.txt docs/claims/skeleton-repeatability.claim docs/video-input.md | wc -m"
plan_head_before: 953df60f9c85a8b1f16165c0d2584b60222a456a
plan_head_after: 05faa42fad864b6ce18b4791b59c324c8be0f274

tech-stack:
  added: []
  patterns: [whole-instance refusal snapshots, full-capacity span alias matrix, frame/watch boundary controls]
key-files:
  created: [.planning/phases/05-display-raw-video-and-input/05-04-SUMMARY.md]
  modified: [tests/api/test_api.c, tests/api/tests.cmake, tests/inventory/fast.txt, docs/claims/skeleton-repeatability.claim, docs/video-input.md]
key-decisions:
  - "Keep the existing frame/input implementation: the complete new regression matrix passed without a production-code gap."
  - "Leave VID-04, VID-05 and API-06 unchecked until all sibling plans that declare them have summaries."
patterns-established:
  - "A refused frame call compares the entire machine, descriptor, result and each full caller-provided payload span."
  - "Nonframe stop results carry stop/time metadata while frame metadata is zero and output payloads remain unchanged."
requirements-completed: []
coverage:
  - id: D1
    description: "Frame refusal is transactional across null, malformed, undersized, misaligned, wrapping and overlapping full-capacity spans."
    requirement: VID-04
    verification:
      - kind: unit
        ref: "api.frame_refusal (C041)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Logical input accepts only the nine supported bits, copies levels without guest-time progress, and preserves the instance on refusal."
    requirement: API-06
    verification:
      - kind: unit
        ref: "api.input_refusal (C041)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Frame, budget, device, undefined and write-watch stops have explicit metadata and publication behavior; reset and load clear stale partial capture."
    requirement: VID-05
    verification:
      - kind: unit
        ref: "api.frame_stops, api.video_reset_load, api.frame_edges, api.reset_power_on, api.load_refusal_unchanged (C042)"
        status: pass
    human_judgment: false

duration: 28min
completed: 2026-11-12
status: complete
---

# Phase 5 Plan 4: Atomic Frame Buffers and Session Reset Summary

**Full-capacity transactional frame/input checks with stop ordering and reset/load capture isolation.**

## Performance

- **Duration:** 28 min
- **Started:** 2026-11-12T03:42:53Z
- **Completed:** 2026-11-12T04:11:16Z
- **Tasks:** 2
- **Files modified:** 5
- **Diff-size token proxy:** 47,198 changed-diff characters / 4 = 11,799.5; this is the required estimate scale, not measured model/API usage.

## Accomplishments

- Added full-capacity frame refusal checks for all output pairs, machine overlaps, descriptor/result aliases, integer wrap, tick alignment, malformed metadata, and the 102-entry tick canary boundary. Accepted synchronization frames and nine-bit input updates provide nonvacuous controls.
- Verified zero-budget, exclusive-target, frame, device, undefined, watch, and headroom stops. Tests prove timer/frame wins a common-tick CPU grant, a deferred frame is delivered once, and ordinary advancement discards prior frame deliveries.
- Built real partial DMA capture in two independent instances, then checked that reset and accepted load clear staged pixels, palettes, line ticks, input, watches, and bus ownership. A rejected load preserves the whole instance.
- Documented nonframe result metadata and reset/load ownership behavior; registered both tasks' CTest cases and rebound the repeatability claim to the current fast-test inventory.

## Task Commits

1. **Task 1: Prove complete all-span preflight and transactional refusal** - `396bfb9` (`test`)
2. **Task 2: Make every nonframe stop and session transition observable without stale payload** - `05faa42` (`test`)

## Files Created/Modified

- `tests/api/test_api.c` - refusal, stop-ordering, capture-reset, load, and instance-isolation cases.
- `tests/api/tests.cmake` - registered `api.frame_refusal`, `api.input_refusal`, `api.frame_stops`, and `api.video_reset_load`.
- `tests/inventory/fast.txt` - added the four focused tests to the sorted fast inventory.
- `docs/claims/skeleton-repeatability.claim` - rebound the inventory SHA-256; claim scope, profile, content identity, and evidence remain unchanged.
- `docs/video-input.md` - clarified nonframe metadata, frame/watch ordering, and reset/load effects.

## Verification

- C041: `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(frame_refusal|input_refusal)$'` — passed 2/2; CTest time 0.38 s on the final test source.
- C042: `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(frame_stops|video_reset_load|frame_edges|reset_power_on|load_refusal_unchanged)$'` — passed 5/5; CTest time 0.16 s on the final test source.
- Compiler/build identity: AppleClang 21.0.0.21000101, Darwin arm64; task test source was built from `396bfb9` plus the uncommitted Task 2 diff immediately before commit `05faa42`.
- The initial C042 attempt caught a test-only sentinel expectation after a successful FRAME publication. The assertion was corrected by resetting the sentinel after FRAME, and the exact focused group passed afterward.

## TDD Gate Compliance

Both tasks registered their tests before configure/build and verification. Code inspection at the starting revision showed the production API and reset/load behavior already in place, so the new cases passed without a behavioral RED or a production implementation change. The initial C042 assertion failure was a test setup error, not evidence of a product defect; it was fixed before the passing run. No baseline-green claim is made for the wider suite.

## Decisions Made

No production code change was needed: the existing API satisfied the new full-span, stop, and lifecycle assertions. The three requirements remain unmarked because `requirements.ready-ids` reports them shared with sibling plans that still lack summaries.

## Deviations from Plan

None. Both task-specific test groups and their inventory/claim sidecars were completed as planned.

## Issues Encountered

One new assertion initially compared the WATCH-stop payload against a sentinel that a preceding successful FRAME had correctly replaced. The test now restores its sentinel after FRAME and passes.

## User Setup Required

None.

## Next Phase Readiness

Plan 05-05 can reuse the registered input refusal case. VID-04, VID-05 and API-06 remain partial until their sibling plans complete; `requirements.ready-ids` currently blocks all three. EVD-02, recurring EVD-05 and CON-01 remain partial where later evidence obligations continue. This plan does not close hardware-accuracy, rights, LibFuturo, or release stops.

## Self-Check: PASSED

The summary exists and both task commits (`396bfb9`, `05faa42`) are ancestors of the current HEAD.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*
