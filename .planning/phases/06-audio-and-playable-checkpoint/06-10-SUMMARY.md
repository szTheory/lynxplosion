---
phase: 06-audio-and-playable-checkpoint
plan: "10"
subsystem: testing
tags: [audio, fault-controls, snapshots, CTest, reproducibility]

# Dependency graph
requires:
  - phase: 06-09
    provides: Pinned audio/audio2 source observations and repaired audio behavior
  - phase: 06-02
    provides: Audio regressions, determinism claim checks, and test harness patterns
provides:
  - Ten private semantic audio faults with exact behavior assertions and clean brackets
  - Native and GNU evidence for audio routing, cadence, ring links, and restored suffixes
affects: [06-11, 06-12, audio, deterministic-evidence]

# Diff-character/4 estimate over the six plan-owned tracked paths; not model/API token usage.
actuals:
  tokens: 9481
  tasks: 2
  commits: 2
plan_head_before: 10a02eaba61ded440d61cd2788c3187b99b6adfd
# Measured after both task commits and before the guarded summary/documentation commit.
plan_head_after: 0753170ed4217881426f0417f385111c13d8b08a

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Mutate private copies of current source inputs and require exact assertion failures
    - Bracket each semantic fault with clean runs and source/executable identity checks

key-files:
  created:
    - tests/audio/fault_controls.cmake
    - docs/audio-faults.md
  modified:
    - tests/audio/tests.cmake
    - tests/inventory/fast.txt
    - tests/audio/test_audio.c
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Treat an expected behavior assertion between clean brackets as the only valid mutant detection."
  - "Mutate the snapshot's pending ring cursor and require restore success plus a different complete ordered suffix."
  - "Report software fault sensitivity and source repeatability without claiming hardware accuracy."

patterns-established:
  - "Each fault control uses an exact source anchor, private copied sources, and a fresh executable identity."
  - "Snapshot phase evidence compares result, PCM, and ordered event suffixes after successful restore."

requirements-completed: [EVD-05, AUD-01, AUD-02, DET-05]
coverage:
  - id: D1
    description: "Ten private audio mutants detect channel arithmetic, ring links, both stereo routes, cadence, and snapshot phase at their named assertions."
    verification:
      - kind: unit
        ref: "Native Clang and GNU 13.3 CTest fault selectors at 0445c1569d628b0a33c12f57afa48c313856ffd4 (6/6) and 0753170ed4217881426f0417f385111c13d8b08a (4/4)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Audio positive regressions, four 18-row source observations, and the determinism inventory binding remain valid across native and GNU builds."
    verification:
      - kind: integration
        ref: "Native Clang and GNU 13.3 CTest positive/source selectors at both task commits; 7/7 and 10/10"
        status: pass
      - kind: other
        ref: "det.two_runs, det.two_instances, det.golden (3/3) and sh scripts/check-claims.sh at both task commits"
        status: pass
    human_judgment: false

# Metrics
duration: 46 minutes
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 10: Private audio fault controls Summary

**Ten private mutations now prove that the audio regressions detect channel, stereo, cadence, and restored-phase defects.**

## Performance

- **Duration:** 46 minutes elapsed from dispatch; this is wall-clock handoff time, not active model time or API usage.
- **Started:** 2026-11-13T10:38:04Z
- **Completed:** 2026-11-13T11:23:57Z
- **Tasks:** 2
- **Files modified:** 6 implementation, test, and documentation files

## Accomplishments

- Added six private Task 1 mutants for LFSR taps, integrator saturation, and each A0→A1→A2→A3→T1 link edge. All six compiled and failed only their named semantic assertions between clean before/after runs.
- Added four private Task 2 mutants for left routing, right routing, 399-tick cadence, and saved ring phase. All four compiled and failed only their intended assertions; each clean bracket and source/executable identity check passed.
- The snapshot fault explicitly restored successfully before comparing the resumed suffix. It compared the complete result, PCM, and ordered events at tick 401; the expected records were A0 sequence 0 followed by A1 sequence 1. The mutated cursor is an internal fieldwise-codec detail at byte 41, not a public ABI offset.
- Native Clang and GNU 13.3 Linux/aarch64 builds passed both task selectors. GNU was synchronized by real fast-forward and verified clean at exact tested source commits `0445c1569d628b0a33c12f57afa48c313856ffd4` and `0753170ed4217881426f0417f385111c13d8b08a`.
- Positive/source selectors passed 7/7 for Task 1 and 10/10 for Task 2 on both builds. The audio and audio2 observations for lynx1 and lynx2 each retained all 18 comparison rows and matched the independent software model.
- `det.two_runs`, `det.two_instances`, and `det.golden` passed at both task commits. The inventory binding was refreshed to `c040071f4a2ab69fd224adfa29e7c1f75697bf903bc7eedf5055f0cb8b2c1d25`, and `sh scripts/check-claims.sh` passed on native and GNU builds.

## Task Commits

1. **Task 1: Detect LFSR/integrator faults and every linked audio channel** - `0445c156` (`test`)
2. **Task 2: Detect both stereo-side, wrong-cadence and lost-snapshot-phase implementations** - `0753170e` (`test`)

## Files Created/Modified

- `tests/audio/fault_controls.cmake` - Private source-copy mutation harness with build, assertion, identity, ownership, and clean-bracket checks.
- `tests/audio/tests.cmake` - Ten named audio fault tests.
- `tests/audio/test_audio.c` - Exact channel, stereo, cadence, and restored-suffix assertions.
- `tests/inventory/fast.txt` - Sorted entries for all ten fault cases.
- `docs/audio-faults.md` - Mutation mapping, exact assertions, and native/GNU qualification scope.
- `docs/claims/skeleton-repeatability.claim` - Current inventory digest binding.

## Decisions Made

- A compile/link error, signal, sanitizer report, timeout, anchor drift, stale executable, or unrelated assertion does not count as mutant detection.
- The snapshot mutation changes a valid pending ring cursor, then requires restore success and a discriminating complete continuation suffix. The byte offset is documented only as a current internal codec detail.
- Results establish software fault sensitivity and pinned-source repeatability. Lynx model qualification and hardware accuracy remain outside this plan.

## Deviations from Plan

None. The plan's private mutants and positive brackets were completed without production-source changes, new dependencies, public APIs, or mutation switches.

## Issues Encountered

- The first broad LFSR probe also changed downstream channel output, so it hit secondary assertions. A dedicated one-assertion case was added and the focused mutant then failed only `audio.lfsr.tap-map`. The initial failed scratch root remains at `build/check/scratch/audio-fault-lfsr_tap-46c343dfa9ac73a2`.
- The first cadence source anchor matched no text because the enum value is terminal and has no comma. The anchor was corrected to the exact `AUDIO_SAMPLE_TICKS = 400u` definition, after which the mutant passed the intended negative-control protocol. The rejected anchor scratch root remains at `build/check/scratch/audio-fault-cadence_399-273a7e354f7003d2`.
- The snapshot fixture initially did not produce linked clocks because its counters were nonzero. The setup was adjusted to make both pending clocks due, and the clean test then proved restore success and the discriminating ordered suffix before the mutant was accepted.

## User Setup Required

None.

## Next Phase Readiness

Plan 11 can proceed with private audio baseline work. Plan 12 owns final canonical qualification and reviews. No hardware-accuracy, physical-device, or global milestone readiness claim is made here.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-13*

## Self-Check: PASSED

- Both task commits exist and are ancestors of the current plan head; the plan range contains exactly two task commits.
- The summary and all six owned implementation/documentation files exist.
- The combined native fault selector passed 10/10; the task-specific fault selectors and 7-case/10-case positive-source selectors passed on native Clang and GNU 13.3. Each claim proof selector and claim checker passed on both builds.
- `git diff --check` passed before the guarded metadata commit. The pre-existing untracked `.planning/state.json` and `.planning/research/.cache/` were left untouched.
