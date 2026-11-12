---
phase: 06-audio-and-playable-checkpoint
plan: "03"
subsystem: audio
tags: [c, lynx, audio, lfsr, stereo, pcm, ctest]

# Dependency graph
requires:
  - phase: 06-01
    provides: Four-channel storage, public A/V capture and PCM/event spans
  - phase: 06-02
    provides: Pinned source-backed audio guest and build setup
provides:
  - Exact authored evidence for four-channel register, counter, LFSR, integrator and clipping behavior
  - Exact evidence for all eight stereo disable routes and signed-wide S16 mix limits
  - Audio register mapping coverage for side-effect-free memory inspection
affects: [06-04 audio ring and timer linkage, 06-12 final audio gates]

# Actuals use the realized owned source diff's chars/4 as a proxy, not API usage.
actuals:
  tokens: 9715
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Independent literal expected vectors for source-backed channel arithmetic
    - Exact named audio cases registered together with the fast inventory

key-files:
  created:
    - tests/audio/test_audio.c
    - tests/audio/tests.cmake
  modified:
    - core/audio.c
    - tests/tests.cmake
    - tests/inventory/fast.txt
    - tests/api/test_api.c
    - docs/audio.md
    - .planning/phases/06-audio-and-playable-checkpoint/06-03-PLAN.md

key-decisions:
  - "Decode signed audio bytes with explicit portable arithmetic instead of unsigned-to-signed narrowing."
  - "Preserve the existing machine layout and public A/V version while completing this digital-behavior slice."
  - "Keep source facts, software policies and physical unknowns distinct; leave timer linkage and the full ring to Plan 04."

patterns-established:
  - "Test all four channel slots with independent expected LFSR vectors and exact register/counter values."
  - "Keep each exact audio test registration in sync with tests/inventory/fast.txt."

# These IDs mirror the plan requirements but this slice does not complete them globally.
requirements-completed: [AUD-01, AUD-02]

coverage:
  - id: D1
    description: "Four mapped channels expose independently verified FD20–FD3F register, counter, LFSR, integrator and clipping behavior; pure inspection does not advance machine state."
    requirement: AUD-01
    verification:
      - kind: unit
        ref: "audio.registers, audio.counter, audio.lfsr, audio.integrator; api.memory_peek_pure"
        status: pass
    human_judgment: false
  - id: D2
    description: "Each FD50 channel/side disable bit mutes only its route while channel and event evolution continues."
    requirement: AUD-01
    verification:
      - kind: unit
        ref: "audio.stereo"
        status: pass
    human_judgment: false
  - id: D3
    description: "The public PCM mixer produces exact signed-wide extrema and cancellation values without overflow or implementation-defined signed arithmetic."
    requirement: AUD-02
    verification:
      - kind: unit
        ref: "audio.mix"
        status: pass
    human_judgment: false

# Measured task-commit ledger, captured before the plan metadata commit.
commits: 2
plan_head_before: 7c046e65cb3b03848c9e96179e05ef19a65ac762
plan_head_after: eea86e68214d543842b8637614922971ec460af9

duration: 33min
completed: 2026-11-12
status: complete
---

# Phase 6 Plan 3: Four-Channel and Stereo Behavior Summary

**Four-channel audio arithmetic and all stereo routes now have exact authored tests, including portable signed-volume decoding and side-effect-free mapped inspection.**

## Performance

- **Duration:** 33 minutes
- **Started:** 2026-11-12T22:03:10Z
- **Completed:** 2026-11-12T22:36:21Z
- **Tasks:** 2
- **Files modified:** 9, counting the seven source paths, the approved plan metadata change and this summary

## Accomplishments

- Added six exact-value audio cases covering all four channels' mapped registers, independent LFSR vectors, counter edge/reload behavior, signed integration and clipping, all eight stereo routes, and wide-mix extrema/cancellation.
- Kept event and channel evolution assertions active while PCM routes are muted. Verified pure mapped inspection preserves the full machine snapshot and guest tick, and updated the API test's FD20 expectation to reflect its now-supported mapping.
- Documented source observations separately from software policy and physical unknowns. No machine layout or public A/V version change was needed; timer linkage and full-ring timing remain for Plan 04.

## Verification

On AppleClang 21.0.0.21000101 (Darwin arm64), both task commands passed:

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(registers|lfsr|integrator|counter)$'
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^audio\.(stereo|mix|registers|integrator)$'
```

The affected API/tracer continuation regressions also passed (5/5): `consumer.av_tracer`, `api.memory_peek_pure`, `api.inspect_pure`, `api.advance_slicing`, and `api.snapshot_continue`.

The same task commands and five regressions passed with GCC 13.3.0 in the reused Linux environment after confirming both `/source` and `/work/lynxplosion` were at `eea86e68214d543842b8637614922971ec460af9`. The selected audio inventory cases are exactly `audio.registers`, `audio.lfsr`, `audio.integrator`, `audio.counter`, `audio.stereo`, and `audio.mix`; no selected test was skipped or missing. Shell scripts were unchanged, so ShellCheck was not rerun.

The owned seven-source-path diff measured 38,861 characters (9,715 chars/4 proxy tokens). This is a diff-size estimate, not API usage, and excludes plan metadata and this report.

## Task Commits

1. **Task 1: Expose complete four-channel counter, shifter and integrator behavior** — `733ac8d` (`fix`)
2. **Task 2: Prove every stereo mute bit and bounded integer mix at signed limits** — `eea86e6` (`test`)

## Files Created/Modified

- `core/audio.c` — Portable signed-byte decoding for audio register volume values.
- `tests/audio/test_audio.c` — Six independent exact-value audio behavior cases.
- `tests/audio/tests.cmake`, `tests/tests.cmake`, `tests/inventory/fast.txt` — Exact test registration and inventory.
- `tests/api/test_api.c` — Positive FD20 mapped-volume peek assertion with unchanged-state checks; other negative routes retained.
- `docs/audio.md` — Digital behavior and evidence boundaries.
- `.planning/phases/06-audio-and-playable-checkpoint/06-03-PLAN.md` — Approved Task 1 file/read-first metadata expansion for the API assertion.

## Decisions Made

Signed register bytes are decoded with explicit arithmetic so volumes above `0x7f` do not depend on implementation-defined unsigned-to-signed conversion. Existing channel storage, reset policy and public A/V layout were retained.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Portability] Replaced implementation-defined signed-byte conversions.**
- **Found during:** Task 1
- **Issue:** Casting a `uint8_t` above `0x7f` directly to `int8_t` is implementation-defined in C.
- **Fix:** Decode the two's-complement register value with explicit bounded integer arithmetic.
- **Files modified:** `core/audio.c`
- **Verification:** Task audio tests passed under AppleClang and GCC.
- **Committed in:** `733ac8d`

**2. [Rule 1 - Test expectation] Updated stale FD20-unavailable API assertion.**
- **Found during:** Task 1 regression run
- **Issue:** The existing pure-memory-peek test still treated mapped audio volume at FD20 as unavailable.
- **Fix:** Added the approved positive mapped-volume read and full-instance/tick unchanged assertions while keeping all other unavailable-route controls.
- **Files modified:** `tests/api/test_api.c`, `.planning/phases/06-audio-and-playable-checkpoint/06-03-PLAN.md`
- **Verification:** `api.memory_peek_pure` and four adjacent continuation regressions passed on both compilers.
- **Committed in:** `733ac8d`

**Total deviations:** 2 auto-fixed (1 portability, 1 stale test expectation). Both were necessary to establish portable behavior and maintain accurate route coverage.

## Issues Encountered

Initial authored test iterations exposed incorrect fixture assumptions for the BLL object length field and expected counter values. The fixtures were corrected; the final plan commands and affected regressions pass on both compilers.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

The four-channel digital behavior, route mask and mix bounds are covered. Plan 04 still owns the complete timer/audio ring and temporal-link-depth behavior; this summary does not claim global completion of AUD-01 or AUD-02.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-12*

## Self-Check: PASSED

Summary file exists, both task commit hashes are ancestors of HEAD, required metadata is present, and the summary/plan diff passes `git diff --check`.
