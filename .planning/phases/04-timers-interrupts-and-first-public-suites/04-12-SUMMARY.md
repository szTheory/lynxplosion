---
phase: 04-timers-interrupts-and-first-public-suites
plan: 12
subsystem: testing
tags: [timers, interrupts, qualification, CTest, CI, D-10]
requires:
  - phase: 04-timers-interrupts-and-first-public-suites
    provides: Timer and CPU suite observations, source comparisons, fault controls, and pending-gate verifier
provides:
  - Exact-source macOS and self-contained Ubuntu 24.04 arm64 qualification
  - Exact-head CI qualification for metadata-only HEAD 356a920
  - D-10 observation/control record with strict index-14 mismatch and Phase 5 carry-forward
affects: [phase-04-closeout, phase-05-display-input, qualification]
actuals:
  tasks: 2
  commits: 18
plan_head_before: 0c4ff91c25261644cbca2913af3e8f60c1d1cab6
plan_head_after: 356a920f21f3f8c573e17056a5bc120e9e20b9f6
tech-stack:
  added: []
  patterns: [exact-source local and hosted qualification, D-10 bounded pending-state admission]
key-files:
  created: [.planning/phases/04-timers-interrupts-and-first-public-suites/04-12-SUMMARY.md]
  modified: [README.md, docs/timers-interrupts.md, docs/public-suites.md, docs/memory-startup.md, .planning/phases/04-timers-interrupts-and-first-public-suites/04-QUALIFICATION.md]
key-decisions:
  - "Apply D-10 only to the single timer result at index 14: actual 0C versus unchanged source 0D on both profiles; strict comparison remains failing."
  - "Treat CI run 43096967098 as historical unexecuted outage-era evidence; run 43104539428 is the separate passing exact-head result."
  - "Leave global EVD-02, recurring EVD-05, O1, and hardware, rights, physical-test, publication, and release gates open."
patterns-established:
  - "Record exact tested source and pushed metadata head separately when qualification docs change after source testing."
  - "Keep local, Linux, strict-comparison, bounded observation, and hosted-CI outcomes distinct."
requirements-completed: [TIM-01, TIM-02, TIM-03, TIM-04, EVD-03, EVD-04]
requirements-partial: [EVD-02, EVD-05]
coverage:
  - id: D1
    description: "Software observation/control qualification passed on macOS, self-contained Ubuntu arm64, and exact-head hosted CI while preserving the approved timer mismatch."
    requirement: EVD-02
    verification:
      - kind: integration
        ref: "sh scripts/check.sh at source 7616f6d (macOS and Ubuntu 24.04 arm64); GitHub Actions run 43104539428 at exact HEAD 356a920"
        status: pass
    human_judgment: false
duration: "4h 14m elapsed from the first Task 1 artifact commit; includes review and hosted-CI wait"
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 12: Timers, Interrupts and First Public Suites Summary

**The reviewed timer observation/control slice passed both local platforms and exact-head CI while retaining the strict index-14 mismatch for Phase 5.**

## Performance

- **Duration:** 4h 14m elapsed from the first Task 1 artifact commit, including review and hosted-CI wait.
- **Started:** 2026-11-11T17:36:01Z (first Task 1 artifact commit; execution began earlier).
- **Completed:** 2026-11-11T21:49:46Z.
- **Tasks:** 2.
- **Files modified:** 5 plan deliverable files, plus this summary and planning bookkeeping.

## Accomplishments

- Recorded the integrated CPU/timer software observations, exact source comparisons, pending-state controls and Phase 5 carry-forward under the owner-approved D-10 boundary.
- Qualified the unchanged product/script source `7616f6d1d12b98eac6cf4d4e979ac1c90f0e01ac` with `sh scripts/check.sh` on macOS Apple Silicon and a self-contained Ubuntu 24.04 arm64 checkout. Both runs passed all 163 tests, exact JUnit accounting, zero declared skips, artifact scan and history hygiene.
- Passed [hosted CI run 43104539428](https://github.com/szTheory/lynxplosion/actions/runs/43104539428) on exact pushed HEAD `356a920f21f3f8c573e17056a5bc120e9e20b9f6`. Its single Ubuntu 24.04 check job passed 163/163 tests, JUnit once each, zero declared skips, artifact scan and history hygiene.
- Kept the prior [run 43096967098](https://github.com/szTheory/lynxplosion/actions/runs/43096967098) distinct as a queued, zero-job outage-era ghost. The official incident timeline and bounded recovery history remain in [04-CI-BLOCKER.md](04-CI-BLOCKER.md).
- Preserved the 16-row timer comparison: 15 rows agree and only index 14 remains actual `0c` versus expected `0d` for both profiles. The strict command still exits 1. The independent 64-phase software proof establishes 12 timer counts under the tested environment; it does not establish hardware behavior or resolve the differing original preconditions.

## Task Commits

The measured plan range contains 18 commits (`0c4ff91` through `356a920`):

1. **Task 1: Audit the exact pending gate and prepare integrated evidence** — `db7149e`, `1764ad1`.
2. **Task 2: Qualify the reviewed source locally, on Linux and exact-head CI** — reviewed fixes `044543b`, `1b5781a`, `f4377d5`, `2d8d82d`, `96fc354`, `27145c8`, `5719e3a`, `7616f6d`; qualification and recovery records `adc5247`, `f920844`, `35b31ba`, `356a920`.
3. Parent review/fix records in the same measured range — `272dd2b`, `f1cbaf0`, `7c245ec`, `ae353db`.

## Files Created/Modified

- `README.md` — describes the implemented timer/interrupt and public-suite boundaries.
- `docs/timers-interrupts.md` — records timer ordering, shared phase, readiness, IRQ/pending, FD91 and snapshot/refusal behavior with software-only limits.
- `docs/public-suites.md` — documents inventory, source comparison, observation/control gates and known mismatch.
- `docs/memory-startup.md` — records startup and memory observations without inferring BIOS or cartridge qualification.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-QUALIFICATION.md` — full evidence ledger, review links, historical outage run and passing exact-head CI evidence.

## Qualification and Efficiency Review

The exact-head CI job used one `check` job on Ubuntu 24.04.5 x86_64, with full-depth checkout. It reported GCC 13.3.0, CMake 3.31.6, Ninja 1.13.2, ShellCheck 0.9.0 and Git 2.56.0. Queue time was about 2s; `check.sh` took 124s, including an 82s configure/build/test workflow (CTest 77.31s), 2s artifact scan and 20s history check. The job took 2m9s. One job and short queue provide no evidence for adding matrix breadth, concurrency or a new cache.

The passing artifact scan emitted 35 `tr: write error: Broken pipe` diagnostics. Their cause was not established; the scan reported success and no scanner changes or output suppression were made. This is recorded for later review, not treated as a failed gate.

## Decisions Made

- D-10 remains limited to the exact observed timer index-14 value. No source pin, oracle content, expected value, counter initialization, timing, wrapper layout or phase was changed to manufacture agreement.
- Qualification names source `7616f6d` and CI metadata head `356a920` separately because the CI push contains documentation only after the source run.
- Software observations and CI do not establish Lynx hardware accuracy or a selected Lynx I/II baseline.

## Deviations from Plan

### Auto-fixed Issues

**1. Successful suite runs retained scratch files and caused the artifact gate to fail.**
- **Found during:** Task 2 final local qualification.
- **Issue:** The initial run exited at artifact hygiene after test/JUnit success, with retained suite outputs.
- **Fix:** Root added current-invocation path, nonce and marker checks before removing only owned successful-run output; failed runs retain scratch.
- **Files modified:** `tests/suites/run.cmake` and its owning controls.
- **Verification:** Four affected controls passed and left zero suite files; the full local and Linux gates later passed.
- **Committed in:** `5719e3a`.

**2. Ubuntu ShellCheck 0.9 rejected compound shell refusal expressions.**
- **Found during:** Task 2 cross-platform qualification.
- **Issue:** The earlier local/macOS pass did not detect SC2015 portability diagnostics.
- **Fix:** Root rewrote nine checks as explicit conditional refusals with unchanged conditions and statuses, and rebound exact descriptor recipe identities.
- **Files modified:** `scripts/build-suites.sh`, `scripts/generate-suite-oracle.sh`, and `docs/timer-source-index14-pending.json`.
- **Verification:** Local syntax/ShellCheck and affected targeted checks passed; the renewed macOS and Ubuntu gates and exact-head CI passed.
- **Committed in:** `7616f6d`.

**Total deviations:** 2 qualification-driven fixes. **Impact:** Both repaired demonstrated gate failures within the planned scope; D-10 admission, source expectations and proof logic were not retuned.

## Issues Encountered

- Repeated reporting-wrapper errors were fixed before the project gate ran. The first artifact-retention attempt and Ubuntu lint rejection are preserved as historical failures, separate from the passing results.
- The outage-era GitHub run remained in inconsistent queued state after recovery; the owner-requested fresh push produced a separate green exact-head run. See [04-CI-BLOCKER.md](04-CI-BLOCKER.md).

## User Setup Required

None.

## Next Phase Readiness

Plan 12 is complete. Formal Phase 4 goal verification and closeout remain root-owned. The strict index-14 source mismatch, global EVD-02, recurring EVD-05, TIM-04 requirement status, O1, O5, O6, O9 and all rights, hardware, physical-test, publication and release gates remain unchanged. Phase 5 must gather independent startup/bus/video evidence before repeating the unchanged timer comparison; keep the mismatch if it persists. Do not describe this plan as a hardware or profile qualification.

---
*Plan: 04-12*
*Completed: 2026-11-11*
