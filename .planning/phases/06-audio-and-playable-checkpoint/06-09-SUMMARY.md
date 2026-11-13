---
phase: 06-audio-and-playable-checkpoint
plan: "09"
subsystem: testing
tags: [audio, audio2, public-suites, CTest, source-comparison]

# Dependency graph
requires:
  - phase: 06-02
    provides: Audio suite harness, source-oracle conventions, and observation helpers
  - phase: 06-08
    provides: Bounded publication and reproducibility controls reused by the suite harness
provides:
  - Fresh audio and audio2 source observations for lynx1 and lynx2 with complete 18-row comparisons
  - Named audio mutation controls and exact five-suite inventory selection checks
affects: [06-10, 06-11, 06-12, audio, public-suite-evidence]

# Diff-character/4 estimate over the plan's changed tracked paths; not model/API token usage.
actuals:
  tokens: 27740
  tasks: 2
  commits: 2
plan_head_before: 1fe08e5af6a013801dd7eb83320739a8120d7df1
# Measured after the two task commits and before this summary metadata commit.
plan_head_after: f5fb299b34fdbfb0d745688ebf90f8c819d520ac

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Reuse shared observation and model helpers for fresh, source-built guest captures
    - Mutate private copies of immutable validated baselines for negative controls

key-files:
  created:
    - .planning/phases/06-audio-and-playable-checkpoint/06-09-SUMMARY.md
  modified:
    - .planning/phases/06-audio-and-playable-checkpoint/06-09-PLAN.md
    - core/audio.c
    - core/internal.h
    - core/machine.c
    - core/timer.c
    - docs/audio.md
    - docs/claims/skeleton-repeatability.claim
    - docs/public-suites.md
    - tests/audio/test_audio.c
    - tests/inventory/fast.txt
    - tests/suites/run.cmake
    - tests/suites/test_public_suites.c
    - tests/suites/tests.cmake

key-decisions:
  - "Keep the audio oracle and every expected value unchanged; repair source behavior exposed by the fresh captures."
  - "Keep O1 and hardware accuracy unqualified; report only software repeatability and source agreement."
  - "Preserve the two inherited page-mode SERCTL skips and the strict timer row-14 mismatches without adding exceptions."

patterns-established:
  - "Audio controls validate records against a per-profile immutable baseline and recheck its identity around mutations."
  - "The exact selection control checks ten observation IDs and eight controls across the five public suites."

# The scoped deliverables are complete; global requirement status remains pending pending O1 and Phase 6 qualification.
requirements-completed: []
coverage:
  - id: D1
    description: "Four fresh audio/audio2 observations contain all 18 source-comparison rows and agree with the independent software model."
    requirement: AUD-01
    verification:
      - kind: integration
        ref: "CTest audio/audio2 observations on lynx1 and lynx2 at source commit f5fb299b34fdbfb0d745688ebf90f8c819d520ac"
        status: pass
    human_judgment: true
    rationale: "These checks establish software behavior and source agreement, while the Lynx model decision and hardware accuracy remain unresolved."
  - id: D2
    description: "The scoped five-suite selection and its negative controls preserve the exact inventory and visible skips."
    requirement: EVD-06
    verification:
      - kind: integration
        ref: "suite.selection_controls and the scoped native/GNU CTest selector at source commit f5fb299b34fdbfb0d745688ebf90f8c819d520ac"
        status: pass
      - kind: other
        ref: "sh scripts/check-claims.sh"
        status: pass
    human_judgment: true
    rationale: "O1 blocks official suite-pass qualification; the page-mode observations remain visible skips with the SERCTL blocker."

# Metrics
duration: 69 minutes
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 09: Fresh audio observations and suite controls Summary

**Fresh audio and audio2 source observations now retain all 18 rows on both profiles, with named controls guarding the public-suite inventory.**

## Performance

- **Duration:** 69 minutes
- **Started:** 2026-11-13T09:23:39Z
- **Completed:** 2026-11-13T10:32:36Z
- **Tasks:** 2
- **Files modified:** 13 implementation, test, plan, and documentation files

## Accomplishments

- Captured four fresh source-built guests: audio and audio2 on lynx1 and lynx2. Each run retains exactly 18 raw bytes and all 18 source comparisons; all observed values match the unchanged oracle alternatives and the independent software model.
- Repaired the source behavior exposed by the first captures: manual CTLB borrow clocks when status exposure is disabled, output latching for volume/shift/control writes, ring borrow propagation from the pre-write count, and expiry of guest-visible audio borrow status while preserving divider `LAST_CLOCK`. No oracle values or expected alternatives changed.
- Added bounded hold/resume capture, record validation, immutable-baseline mutation controls, and exact inventory checks. The audio corpus has 24 named negative controls and audio2 has 26, including loss of the row-9 `30|34` alternative and a changed accepted observation with stale comparison evidence.
- The exact selection control checks ten observation IDs and eight controls across CPU, timers, page-mode, audio, and audio2. The direct Plan02 audio script-control corpus and all three claim proofs also passed.
- The scoped native and GNU 13.3 selectors each selected 16 CTests: 14 executed and passed, and the same two page-mode observations were skipped. GNU verification ran against exact source commit `f5fb299b34fdbfb0d745688ebf90f8c819d520ac`; its tracked tree was clean afterward. The claim checker passed on both builds.

## Task Commits

1. **Task 1: Capture fresh audio observations and repair source gaps** - `154a5f2` (`fix`)
2. **Task 2: Add audio controls and exact suite selection** - `f5fb299` (`test`)

The final guarded summary and documentation commit is recorded in the execution handoff.

## Files Created/Modified

- `tests/suites/test_public_suites.c` - Audio/audio2 observation, record validation, and mutation controls using shared harness helpers.
- `tests/suites/run.cmake` - Fresh audio capture, baseline identity, bounded completion, and negative-control orchestration.
- `tests/suites/tests.cmake` - Audio/audio2 and source-control CTest registrations.
- `tests/inventory/fast.txt` - Exact five-suite observation and control inventory.
- `docs/public-suites.md` - Scoped qualification results and unresolved blockers.
- `docs/claims/skeleton-repeatability.claim` - Inventory hash binding refreshed and revalidated.
- `core/audio.c`, `core/timer.c`, `core/machine.c`, `core/internal.h`, `tests/audio/test_audio.c`, `docs/audio.md` - Source behavior repairs and corresponding regression coverage.
- `.planning/phases/06-audio-and-playable-checkpoint/06-09-PLAN.md` - Records the bounded source repair scope.

## Decisions Made

- Audio and audio2 must match all pinned source alternatives, including audio2 row 9 (`30|34`); source discrepancies remain software defects to fix.
- Keep strict timer row 14 at legacy `0C` versus source `0D`, and VIDEO_POLICY1 row 14 at `0F` versus source `0D`. No numeric exception, skip, tuning, or hardware claim was added.
- Keep the two existing page-mode skips because the source reaches an unsupported SERCTL write at `$FD8C`.
- Treat these results as bounded software evidence only. O1/profile qualification and hardware accuracy remain open.

## Deviations from Plan

None. The planned software gaps were fixed without changing the pinned source fixtures, oracle values, public APIs, or dependency set.

## Issues Encountered

- The first audio mutation run referenced a nonexistent build-manifest filename; the harness was corrected to use the verified manifest, and the full 24-case corpus passed.
- The first minimum-tick mutation reached the completion-margin validator before the tick-range validator. Validation order was corrected so the malformed record receives its intended bounded-range diagnostic; the rerun passed.
- The two failed diagnostic run roots were retained for parent preservation rather than cleaned: `build/check/scratch/suites/suite.audio.controls/run-eb8cb3ec8657b4bcbdea20b139209dadac4962c02b8ec81f` and `build/check/scratch/suites/suite.audio.controls/run-e16abe706996116d737107f79e5874751c5aec4057b7553e`.

## User Setup Required

None.

## Next Phase Readiness

Plan 10 is next for private semantic audio-fault controls, followed by Plan 11's baseline work and Plan 12's final canonical qualification and review. The timer discrepancies, page-mode blocker, O1 decision, model qualification, and hardware-accuracy questions remain visible; this plan does not close them. Final Phase 6 qualification remains pending.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-13*

## Self-Check: PASSED

- Summary file exists and the two task commits are ancestors of the current HEAD.
- The plan-start-to-Task2 range contains exactly two task commits.
- Native and GNU scoped selector logs report 14 executed passes and the same two page-mode skips; the GNU tree is clean at the tested source SHA.
- `git diff --check` passed before the guarded metadata commit.
