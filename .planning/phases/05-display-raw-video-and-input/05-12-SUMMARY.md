---
phase: 05-display-raw-video-and-input
plan: "12"
subsystem: testing
tags: [raw-video, input-replay, qualification, ctest, github-actions]

requires:
  - phase: "05-11"
    provides: [timer observations, strict comparison controls, public-suite inventory]
provides:
  - Final-source macOS, self-contained Ubuntu, and exact-head hosted CI qualification
  - Source-bound qualification, validation, and review-resolution records
affects: [Phase 05 security closeout, Phase 06 audio evidence, EVD-02, EVD-05, CON-01]

actuals:
  tokens: 56371
  tasks: 2
  commits: 24
commits: 24
plan_head_before: 0ac23d1260ae1e3ddd1a1d5f2a9c4a815a9a071f
plan_head_after: d3fb0024fe6a732b05f9b42122a18c110bed8ae2

tech-stack:
  added: []
  patterns:
    - Bind each environment result to one exact reviewed source identity and explicit skips
    - Keep strict timer-source disagreement separate from passing observation/control tests

key-files:
  created: [.planning/phases/05-display-raw-video-and-input/05-12-SUMMARY.md]
  modified:
    - README.md
    - .planning/STATE.md
    - .planning/phases/05-display-raw-video-and-input/05-QUALIFICATION.md
    - .planning/phases/05-display-raw-video-and-input/05-VALIDATION.md
    - .planning/phases/05-display-raw-video-and-input/05-REVIEW-RESOLUTION.md

key-decisions:
  - "Bind final gates to source head d3fb0024fe6a732b05f9b42122a18c110bed8ae2 and keep the later documentation-only commit distinct."
  - "Retain the two exact page-mode skips and both strict timer-source disagreements; do not infer hardware agreement from green controls."
  - "Keep the hosted gate as one full-history Ubuntu job; measured timings did not justify adding cache, concurrency, or matrix breadth."

requirements-completed: [VID-01, VID-02, VID-03, VID-04, VID-05, INP-01, API-06, HB-02]
coverage:
  - id: D1
    description: "Final reviewed source passed the canonical gate on macOS and self-contained Ubuntu and passed hosted CI on the exact pushed source."
    verification:
      - kind: integration
        ref: "sh scripts/check.sh; 202 JUnit rows per environment; GitHub Actions run 43268267738"
        status: pass
    human_judgment: false
    rationale: "Software qualification is automated; documented strict disagreements and external owner/hardware limits do not require manual acceptance for this scoped result."

duration: "9h 35m from first recorded plan checkpoint"
completed: 2026-11-12
status: complete
---

# Phase 5 Plan 12 Summary

**The reviewed raw-video and input slice passed final macOS, independent Ubuntu, and exact-head hosted qualification with its declared skips and timer mismatches preserved.**

## Performance

- **Duration:** Approximately 9h 35m elapsed across review and qualification checkpoints.
- **Started:** `2026-11-12T08:02:47Z` (plan commit-ledger creation; active execution time was not separately measured).
- **Completed:** `2026-11-12T17:38:04Z` (qualification evidence; documentation-only metadata commits followed).
- **Tasks:** 2.
- **Files modified:** 35 unique tracked paths across the plan work and final records.
- **Actuals method:** `tokens` is the realized diff character count divided by four; `commits` is measured from the persisted plan-head ledger and excludes the metadata-only followup commits.

## Accomplishments

- Completed the required independent code, security, and coverage reviews; applied accepted repairs through separately reviewed commits.
- Passed `sh scripts/check.sh` on macOS 26.6.2 arm64 and a self-contained pinned Ubuntu 24.04 arm64 checkout at source head `d3fb0024fe6a732b05f9b42122a18c110bed8ae2`. Each accounted for 202 rows: 200 passed, two declared page-mode skips, and zero failures; JUnit, artifact scan, and history hygiene passed.
- Pushed that same source head normally to `origin/main`; hosted run [43268267738](https://github.com/szTheory/lynxplosion/actions/runs/43268267738) passed on the exact SHA.
- Recorded the exact source fingerprint, environment/tool identities, CI critical path, raw observation limits, and retained timer disagreements in `05-QUALIFICATION.md`.
- Kept the README followup documentation-only. Its replacement SHA-256 is `629a416602d47780bee8fdfeb13b64ce9b5e1b9c3c30fab23846f76c2c91a510`; the 54 other paths in the tested 55-path map, including all product, test, fixture, and script files, remain unchanged.

## Task Commits

- **Task 1:** Preparation and accepted review repairs were committed atomically across 24 commits from `353c018` through tested source head `d3fb002`; `05-REVIEW-RESOLUTION.md` records each repair and review disposition.
- **Task 2:** Final gates and hosted CI qualified `d3fb002`. The documentation and state updates are in local metadata-only follow-up commits, separate from the tested/pushed CI head.

## Verification

- **macOS:** CTest 110.25s; workflow 110s; artifacts 6s; history 85s; total 204s. 6,361 files scanned, one 512-byte UTF-8 source file; no findings.
- **Self-contained Ubuntu 24.04:** CTest 111.07s; workflow 111s; artifacts 1s; history 13s; total 127s. The 354-commit clone was non-shallow and had no Git alternates. 6,330 files scanned, one 512-byte UTF-8 source file; no findings.
- **Hosted Ubuntu 24.04 CI:** queue about 2s; runner setup under 1s; checkout 2s; canonical command 171s. cc65 build 19s; CMake configure/build about 5s; CTest 105.60s; JUnit 0s; artifact scan 2s; history hygiene 32s. One job, no matrix or duplicate broad gate. Slowest tests: pending timer controls 58.70s, result controls 47.88s, and none-profile seam agreement 24.68s.
- **Exact skips:** `suite.page_mode.observation.lynx1` and `suite.page_mode.observation.lynx2` stop at the unsupported `$06` write to `$FD8C` before producing a result. Their observations are missing.
- **Strict comparisons:** `LEGACY_FLAT5` retains only its admitted index-14 `0c` versus source `0d` row. `VIDEO_POLICY1` retains index 14 `0f` versus source `0d` for both profiles and agrees on the other 15/16 rows. Strict source comparisons remain unresolved failures even though observation/control tests pass.
- The plan-local claim hash check returned `1e74578b2e13db45953c6fd02e4793f0e04623f4d4b3d7158b04a69049bd9c5d`; final CMake configurations matched the exact sorted test inventory.

Earlier intermediate-source failures are preserved and linked from `05-QUALIFICATION.md`; no failed attempt is presented as a final green gate. No broad CPU vector rerun, extra review, cache, or concurrency change was added.

## Deviations from Plan

The hosted CI monitor script named by the local GitHub-workflows skill was absent from this repository. The exact push-triggered run was identified and monitored through the connected GitHub Actions API instead. No workflow, test, or CI scope change was needed.

## Next Phase Readiness

Plan 05-12 qualification is complete. The exact page-mode observations remain missing; EVD-02 remains open; recurring EVD-05 and CON-01 remain partial for Phase 6 audio work; O1, O5, O6, and O9 remain open. Root owns final security-ledger, phase, roadmap, and next-phase actions; none are advanced here.

## Self-Check

PASSED: qualification, validation, review-resolution, and summary artifacts exist; tested source `d3fb002` is an ancestor of HEAD; 24 task/review commits were measured before the metadata-only followups; README replacement hash matches; the untracked state cache still matches its pre-update backup.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*
