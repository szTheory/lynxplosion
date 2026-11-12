---
phase: 05-display-raw-video-and-input
plan: "11"
subsystem: testing
tags: [timers, video-policy1, strict-comparison, continuation, pinned-source]

requires:
  - phase: "05-10"
    provides: [source-bound suite artifacts, strict CTest and scratch ownership controls]
provides:
  - Independently frozen two-profile VIDEO_POLICY1 timer observations with all 16 source comparisons
  - Strict identity, descriptor-separation, row-shape, tick-boundary, and sliced/watch continuation controls
affects: [Phase 05 Plan 12, EVD-02, timer evidence, public-suite inventory]

actuals:
  tokens: 24313
  tasks: 2
  commits: 2
commits: 2
plan_head_before: dd5ccc59f3bf32be4d62873284a2a71aa680eafd
plan_head_after: b60136e6b16ee25a0d500f2c103f62e0fdf2ba56

tech-stack:
  added: []
  patterns:
    - Exact source-set and identity validation for independently labelled software observations
    - Immutable positive baselines around private hostility cases and common-tick continuation comparisons

key-files:
  created: []
  modified:
    - tests/suites/test_public_suites.c
    - tests/suites/run.cmake
    - tests/suites/tests.cmake
    - tests/inventory/fast.txt
    - docs/timer-video-revisit.md
    - docs/public-suites.md
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Keep the exact D-10 0C-versus-0D exception in legacy flat5/no-DMA/counter-zero records; video-policy records reject descriptor inheritance."
  - "Freeze supported DISPCTL=01 before the new observations; the CRT's DISPCTL=0D remains unsupported and supplies no hardware evidence."
  - "Accept only the observed strict mismatch set: index 14 is 0F against 0D and every other source row agrees."

requirements-completed: [VID-02]
coverage:
  - id: D1
    description: "Fresh Lynx I and Lynx II VIDEO_POLICY1 timer observations preserve all 16 source rows and the strict 0F/0D disagreement."
    requirement: EVD-02
    verification:
      - kind: integration
        ref: "suite.timers.video_revisit; docs/timer-video-revisit.md"
        status: pass
    human_judgment: true
    rationale: "Automation establishes the complete software observation and unresolved mismatch; it does not establish source agreement or hardware qualification."
  - id: D2
    description: "Timer record controls refuse descriptor inheritance, identity drift, extra mismatches and malformed records, and compare uninterrupted, sliced and watched runs."
    verification:
      - kind: integration
        ref: "suite.timers.video_revisit_controls"
        status: pass
    human_judgment: false

duration: "53 min from first timestamped work; executor start was not captured"
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 05-11 Summary

**Two-profile VIDEO_POLICY1 timer observations preserve the strict 0F/0D mismatch while controls reject any second mismatch or inherited D-10 exception.**

## Performance

- **Duration:** About 53 minutes from the first frozen-environment timestamp (`2026-11-12T11:54:02Z`); dispatch began earlier and was not timestamped.
- **Completed:** `2026-11-12T12:47:04Z`.
- **Tasks:** 2.
- **Files modified:** 7 unique plan-owned files.
- **Actuals method:** `tokens` is the character count divided by four for the realized task diff from `dd5ccc5` through `b60136e`; the measured diff is 97,252 characters.

## Accomplishments

- Re-ran the legacy flat5/no-DMA timer observations, strict comparison controls, and independent cascade proof before interpreting the new environment. The single legacy pending row remains index 14 actual `0c` versus source `0d`; no other mismatch or pending admission was added.
- Froze a separate VIDEO_POLICY1 setup before observation: Timer0 `9e/18`, Timer2 `68/1f`, PBKUP `29`, DISPCTL `01`, fixed zero-filled display buffer at `2000`, MAPCTL `0c`, page latch `00`, refresh first due at tick `256` every `256` ticks, five-tick CPU/video/refresh costs, serialized calls, and zero counter initialization.
- Recorded the wrapper's zero-page clear separately from the 15 source counter increments. Both profiles retain all 16 exact source comparisons: 15 rows agree and index 14 is `0f` versus expected `0d`; strict comparison exits 1. No legacy descriptor is attached to these observations.
- Added record validation for the exact pinned source/tool/object/core/layout and frozen register/timing state. It requires exactly the one observed mismatch at index 14; all other rows must match the complete source alternatives.
- Added 45 named controls for descriptor separation, changed identities and policies, changed/second mismatch rows, raw/comparison row shapes, null inputs, and integer boundary candidates. Both profiles also compare fresh uninterrupted, 37-tick sliced, and watched executions at the same completion tick, requiring identical results and public-state digests.
- Updated the public-suite inventory, timer revisit report, and skeleton claim digest (`1e74578b2e13db45953c6fd02e4793f0e04623f4d4b3d7158b04a69049bd9c5d`). All six protected legacy scripts, wrappers, layout, and pending-descriptor hashes remain byte-identical.

## Task Commits

1. **Task 1: Run the unchanged timer comparison under independently chosen video preconditions** — `4738c5d` (`test(05-11): capture strict video-policy timer observations`).
2. **Task 2: Prove new environments and changed rows cannot inherit the old pending exception** — `b60136e` (`fix(05-11): enforce timer video record strictness`).

The plan metadata was committed separately after this summary and the roadmap/state updates using the required guarded SDK helper.

## Verification

- Task 1 exact command passed all five tests:
  `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(video_revisit|observation\.(lynx1|lynx2)|strict_comparison_controls|cascade_model)$'`
- Task 2 exact command passed all four tests:
  `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(video_revisit_controls|pending_controls|result_controls|model_controls)$'`
- `sh scripts/check-claims.sh --inventory-sha256` returned the digest recorded above.
- `sh scripts/hygiene.sh --tree` and `sh scripts/hygiene.sh --history dd5ccc59f3bf32be4d62873284a2a71aa680eafd..HEAD` passed.
- The reconstructed pre-fix second-mismatch RED evidence was classified `RED_EVIDENCE_OK`. Raw CTest output and the evidence JSON remain in ignored `build/check` scratch and were not committed.

## TDD Gate Compliance

- **Task 1:** The plan registered its named suite and inventory before configure. The unchanged legacy baseline was green; it is compatibility evidence, not RED. No semantic pre-implementation RED was captured for the new observation path, and this summary does not claim one.
- **Task 2:** A separately reconstructed pre-fix validator, with the strict mismatch-set guard removed, ran the registered control. The target failed because the validator accepted a real second raw mismatch and reported 14/16 agreements. The JUnit report was machine-classified as `RED_EVIDENCE_OK`; the record labels this as reconstructed comparison evidence, not original chronology. Restoring the guard made the exact Task 2 slice pass.
- The plan frontmatter is `type: execute` and global `workflow.tdd_mode` is false, so the plan-level RED/GREEN commit gate did not apply. Both tasks retain their required atomic task commits.

## Deviations from Plan

None in scope or evidence policy. Control defects found during verification were fixed: the extra-row mutation now appends after row 15, the blank result-byte case has its own malformed-byte diagnostic, and the exact case inventory is read from its global CMake property.

## Issues Encountered

The first strictness review found that a consistent comparison record with a second raw mismatch could pass the new-environment validator. The validator now checks each row against the pinned source sets and accepts only the single index-14 mismatch observed in the frozen run. Its reconstructed pre-fix RED and restored GREEN runs are recorded above.

## Threat Flags

None. The changes add no network endpoint, public API, or rights-bearing artifact; generated records stay in invocation-owned ignored scratch.

## Next Phase Readiness

Plan 05-12 can use the complete independent software observations and strict controls. Global EVD-02 remains open because both new profiles still report `0f` against the unchanged source `0d`; the upstream CRT's unsupported `DISPCTL=0d`, hardware accuracy, and profile qualification remain unresolved. No later-plan work, hardware test, publication, push, or release was performed.

---
*Phase: 05-display-raw-video-and-input*
*Plan: 05-11*
*Completed: 2026-11-12*

## Self-Check: PASSED

The summary exists, has `status: complete`, and both task commits are ancestors of the current HEAD.
