---
phase: 06-audio-and-playable-checkpoint
plan: "05"
subsystem: api
tags: [c, audio, video, spans, deterministic, capacity]

# Dependency graph
requires:
  - phase: 06-04
    provides: Bounded timer/audio ordering, sample cadence, and the existing same-tick scheduler stages
provides:
  - Full-span A/V buffer validation before machine progress
  - Lossless output-capacity stops and exact retry across caller-owned spans
  - Regression coverage for caller-buffer isolation and guest-time exhaustion
affects: [06-06, native A/V API consumers]

# Measured from the persisted plan ledger at SUMMARY write time (#3968).
plan_head_before: 055da142357d5f241dec0c753aaac85dfb29fbe4
plan_head_after: 2106a08491b13d5e04a26d892c8ef94d37475802
actuals:
  tokens: 11369
  tasks: 2
  commits: 3

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Validate complete declared spans, arithmetic, alignment, and aliases before progress
    - Reserve output at scheduler commit frontiers and resume with fresh caller spans

key-files:
  created: []
  modified:
    - include/lynxplosion.h
    - core/machine.c
    - tests/audio/test_audio.c
    - tests/audio/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Treat device/link/sample work as one reserved group; reserve a later CPU-triggered audio write immediately before that write, preserving Plan04's committed-prefix behavior."
  - "Require at least one PCM frame and thirteen audio-event slots; reject smaller spans before machine or caller memory changes."
  - "Refresh only the repeatability claim's inventory digest when adding the four registered fast tests."

requirements-completed: [AUD-02, CON-01, DET-05]
# Scoped span/capacity evidence; global completion still depends on sibling
# plans and the Phase6 qualification gate.
coverage:
  - id: D1
    description: "Malformed, overlapping, misaligned, wrapped, or undersized A/V spans are rejected without changing the machine or caller bytes."
    requirement: CON-01
    verification:
      - kind: unit
        ref: "audio.span_refusal, audio.span_alias"
        status: pass
    human_judgment: false
  - id: D2
    description: "Capacity stops publish complete ordered prefixes, preserve pending work for retry, and do not retain caller buffers across independent instances."
    requirement: AUD-02
    verification:
      - kind: unit
        ref: "audio.capacity, audio.stops"
        status: pass
      - kind: integration
        ref: "api.frame_stops, api.frame_refusal, api.input_refusal, api.reset_power_on, api.load_refusal_unchanged"
        status: pass
    human_judgment: false
  - id: D3
    description: "The fast-test inventory and repeatability claim stay bound and pass the existing deterministic evidence checks."
    requirement: DET-05
    verification:
      - kind: integration
        ref: "det.two_runs, det.two_instances, det.golden, scripts/check-claims.sh"
        status: pass
    human_judgment: false

duration: 40min
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 05: Transactional A/V spans and lossless capacity stops Summary

**Full-span A/V validation, ordered capacity stops, and exact retry with caller-owned buffers**

## Performance

- **Duration:** 40 min
- **Started:** 2026-11-13T01:55:04Z
- **Completed:** 2026-11-13T02:34:47Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Documented the A/V span contract and added transactional refusal coverage for descriptor fields, nulls, capacity minima, alignment, arithmetic wrap, full-range aliases, and adjacent one-past spans.
- Reserved device/link/sample output before mutation and verified that event and PCM capacity stops preserve the pending frontier and produce the same ordered stream and machine digest after retry.
- Interleaved two independent machines with replaced caller spans and verified old spans remain untouched; added A/V guest-time headroom and repeated zero-progress checks.
- Registered four fast audio tests, updated only the inventory digest in the repeatability claim, and passed the claim checks.

## Task Commits

1. **Task 1: Reject malformed spans and aliases before touching machine or outputs** — `cdb384c` (`feat`).
2. **Task 2: Stop at a complete output frontier and resume once without stale payload** — `5383ae9` (`fix`).
3. **Task 2 focused follow-up: Assert A/V guest-time exhaustion postconditions** — `2106a08` (`test`).

## Files Created/Modified

- `include/lynxplosion.h` — Span minima, validation rules, partial-progress and stop semantics.
- `core/machine.c` — Capacity reservation before device/link/sample state changes; CPU-triggered audio writes retain their separate pre-write check.
- `tests/audio/test_audio.c` — Refusal, retry, stop, isolation, and guest-time boundary assertions.
- `tests/audio/tests.cmake` and `tests/inventory/fast.txt` — Four registered fast audio tests.
- `docs/claims/skeleton-repeatability.claim` — Inventory digest binding only.

## Decisions Made

- A same-tick device/link/sample group is reserved before any of its state changes. A later CPU-triggered audio write is a separate ordered group reserved before the bus write. This follows the accepted Plan04 `audio.order` prefix behavior and keeps its assertions intact.
- Validation covers each full declared capacity range, not only the bytes a call expects to write. NULL never disables an observation stream.
- The inventory digest changed because the inventory changed; content identity, profile, evidence tier, and claim wording stayed fixed.

## Deviations from Plan

None. The parent execution handoff resolved the plan's general “whole tick” wording at the already accepted scheduler commit frontiers: device/link/sample work is indivisible, while a later CPU-triggered audio write has its own pre-write reservation. This preserves the Plan04 capacity case where an eight-event device prefix can commit, the CPU strobe waits when five slots remain, and retry emits the next sequence once. Existing half-open, watch, and scheduler ordering behavior was not changed.

## Issues Encountered

Initial focused test runs exposed fixture assumptions rather than production regressions: the audio channels needed the reset-DONE control value, the selected `$02` opcode was defined for the active CPU profile, and the ample comparison needed two 13-event group reservations. The fixtures now select an actually undefined opcode dynamically and use capacities matching the bounded groups. The final scoped runs pass.

## Verification

**AppleClang 21 on macOS:** `cmake --preset check` and `cmake --build --preset check` passed. The Task 1 selector passed 2/2; the Task 2 selector passed 8/8; preserved `audio.order`, `audio.cadence`, and `audio.ring_zero` passed 3/3; the added A/V headroom case passed 1/1. The updated inventory's `det.two_runs`, `det.two_instances`, and `det.golden` controls passed 3/3, and `sh scripts/check-claims.sh` passed.

**GNU 13.3 on Linux/aarch64:** The exact final source commit `2106a08491b13d5e04a26d892c8ef94d37475802` fast-forwarded from the pinned container base; the tracked tree was clean before configuration and afterward. `cmake --preset check` and `cmake --build --preset check` passed. The Task 1 selector passed 2/2; the Task 2 selector on the final commit passed 8/8; preserved audio regressions passed 3/3; deterministic claim evidence passed 3/3; `sh scripts/check-claims.sh` passed; and the added A/V headroom case passed 1/1.

## Limitations

These are scoped API and deterministic regression checks. They do not establish hardware timing accuracy or qualify the complete Phase 6 checkpoint. The full canonical Phase 6 gate and reviews remain with Plan12; firmware, copyrighted game content, physical hardware, and release qualification remain outside this plan.

## Next Phase Readiness

Plan06 can consume the documented A/V output contract and its capacity/retry controls. No user action is needed for this plan. Continue with the next dependent plan; preserve the existing Phase 6 accuracy and qualification stops.

## Self-Check: PASSED

The summary exists, all three plan commits are ancestors of `HEAD`, and the measured plan range contains three commits before the metadata commit.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-13*
