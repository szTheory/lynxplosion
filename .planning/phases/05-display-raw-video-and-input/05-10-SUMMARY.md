---
phase: 05-display-raw-video-and-input
plan: "10"
subsystem: testing
tags: [page-mode, c, cmake, ctest, pinned-source, blocker-evidence]

requires:
  - phase: "05-09"
    provides: [video bus and input contracts, Phase 5 fault-control patterns]
provides:
  - Source-audited tier-A page-mode object and strict six-row oracle
  - Per-profile records for the exact unsupported SERCTL request and missing results
  - Isolated source, tool, marker, notice, parser, and output-ownership hostility controls
affects: [Phase 05 Plans 11-12, EVD-02, EVD-06]

actuals:
  tokens: 28043
  tasks: 2
  commits: 3
commits: 3
plan_head_before: f59b3622ab52b40a6009777a1bcefa9f613a1d5a
plan_head_after: 04c4f79c98d0ed9f5db73776e13e1dc430f22e10

tech-stack:
  added: []
  patterns:
    - Exact source-derived oracle rows with pinned source, tool, notice, and recipe identities
    - Guest stop records that preserve the first unsupported request and missing observation state

key-files:
  created:
    - fixtures/manifests/page-mode-tests.manifest
    - fixtures/suites/page-mode-wrapper.s
    - scripts/build-page-mode.sh
    - scripts/generate-page-mode-oracle.sh
    - tests/gates/page-mode-script-controls.sh
  modified:
    - tests/suites/test_public_suites.c
    - tests/suites/run.cmake
    - tests/suites/tests.cmake
    - tests/inventory/fast.txt
    - tests/inventory/skips.txt
    - tests/cmake/no_skip.cmake
    - docs/public-suites.md
    - docs/claims/skeleton-repeatability.claim
    - scripts/check-manifests.sh
    - scripts/validate-kv.sh
    - tests/gates/manifests_controls.sh

key-decisions:
  - "Keep the page-mode build and source oracle separate from guest agreement; the six results remain missing when the guest stops before the tests."
  - "Declare only the two profile observations skipped, tied to the exact SERCTL write at $FD8C, and keep EVD-02 open."
  - "Preserve strict no-skip scanning with a single exact exception for the required page-mode CTest property."

patterns-established:
  - "Each page script is exercised against a separate copied source and tool tree, with successful positive brackets around destructive input mutations."
  - "A visible test skip is accepted only when the exact registered test name has a reason and the JUnit report confirms it did not run."

requirements-completed: [VID-02]
coverage:
  - id: D1
    description: "The unchanged page-mode fixture builds with a source-derived six-row oracle and bound source/tool identities."
    verification:
      - kind: integration
        ref: "build-page-mode.sh, generate-page-mode-oracle.sh, validate-page-mode-build"
        status: pass
    human_judgment: false
  - id: D2
    description: "Both profiles preserve the exact source blocker, guest progress, model, timing bounds, and six missing result bytes."
    verification:
      - kind: integration
        ref: "suite.page_mode.observation.lynx1, suite.page_mode.observation.lynx2, suite.page_mode.controls"
        status: pass
    human_judgment: false

duration: 77min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 10: Page-Mode Source Build and Blocker Evidence Summary

**The pinned page-mode suite now builds from unchanged source and records the precise pre-test SERCTL blocker for both profiles without claiming results or agreement.**

## Performance

- **Duration:** 77 min
- **Started:** 2026-11-12T10:17:43Z
- **Completed:** 2026-11-12T11:34:19Z
- **Tasks:** 2
- **Files modified:** 16
- **Actuals method:** 112,174 changed diff characters from the recorded plan base through the Task 2 commit, divided by four; planning metadata and this summary are excluded.

## Accomplishments

- Added a page-only BS93 source build, manifest, wrapper, and strict oracle. The six expected bytes are `35 41 69 81 74|75 81`; pinned page-mode source and assembly remain unchanged.
- Added fresh Lynx I and Lynx II observations, strict records, and no-run hold/resume controls. Both profiles stop after their source probe and setup, before any result write.
- Registered exactly two profile observation skips and one controls test. Added isolated hostile-input checks for both page scripts and actual-name JUnit tests for declared, undeclared, and stale skip behavior.
- Kept EVD-02 open. The timer index-14 disagreement and complete page-mode results remain missing evidence; no MAPCTL readback or UART behavior was fabricated.

## Verification

- `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(page_mode\.(observation\.(lynx1|lynx2)|controls)|oracle_controls|symbol_controls)$'` — passed all five accounted tests; the two observation tests were visibly skipped for the exact blocker.
- `ctest --preset check --no-tests=error --output-on-failure -R '^(policy\.no_skip(\.self_test)?|inventory\.junit\.self_test)$'` — passed all three. The JUnit controls retain missing, extra, duplicate, silent/undeclared, and stale-skip failure cases; an isolated near-match skip property was rejected by the no-skip scanner.
- `sh scripts/build-page-mode.sh --out build/check/scratch/task1-final-62bb0b1ccd33e9ae2f70a06c`, matching direct oracle generation, manifest validation, claim validation, and tree hygiene — passed. Artifact identities were recorded before removing only the exact nonce-owned outputs. The fixed sample output path already existed, so the same gate was run with a fresh owned path.
- `sh tests/gates/manifests_controls.sh ~/projects/lynxplosion /private/tmp/lynx-phase5-executor-10-manifests-controls` — passed the strict manifest and input-probe identity controls, including malformed, incomplete, and unknown-key cases.
- `git diff --check`, `sh scripts/check-claims.sh`, `sh scripts/check-manifests.sh`, `sh scripts/hygiene.sh --tree`, and the six legacy SHA-256 identity checks — passed.

Both profile records were validated against the current oracle before cleanup. Lynx I stopped at 267,429 guest ticks and Lynx II at 267,414. Each had progress `1`, completion `00`, model probe/marker `01/a1` or `00/a2`, MAPCTL `0c`, VIDEO_POLICY1, and the first unsupported operation was a write of `$06` to `$FD8C`. Both retained `meaningful_count=0`, `raw_observation=missing`, and `source_comparison=not-run`.

## Task Commits

1. **Task 1: Build source-audited tier-A page-mode objects and oracle** — `338173d` (`feat`)
2. **Task 1 follow-up: Preserve LF-terminated oracle rows** — `b07a07f` (`fix`)
3. **Task 2: Inventory per-profile blocker evidence and controls** — `04c4f79` (`feat`)

## Decisions Made

- A source build and oracle prove pinned construction only. The unsupported source request remains a blocker and cannot become an observation or source agreement.
- Only `suite.page_mode.observation.lynx1` and `suite.page_mode.observation.lynx2` are declared skipped; both use the same exact reason and matching anchored CTest signal.
- Keep VID-02’s existing status and EVD-02’s open status unchanged; this plan supplies source evidence but not completion or timer agreement.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Extended strict manifest validation for Phase 05-07 input-probe fields**
- **Found during:** Task 1
- **Issue:** The existing validator rejected legitimate input-probe fields in the already-recorded manifests, blocking the exact Task 1 gate.
- **Fix:** Added only the established field names, types, and values to the strict allowlist, with malformed and unknown-key negative controls. Content and rights checks remain strict.
- **Files modified:** `scripts/check-manifests.sh`, `scripts/validate-kv.sh`, `tests/gates/manifests_controls.sh`
- **Verification:** Manifest controls and the fresh Task 1 build/oracle/manifest/privacy gate passed.
- **Committed in:** `338173d`

**2. [Rule 1 - Bug] Kept each generated page-mode oracle row newline-terminated**
- **Found during:** Task 1
- **Issue:** Shell command substitution removed the final LF, while the strict C parser requires LF-terminated rows.
- **Fix:** Emit the final newline in production and shape-only output.
- **Files modified:** `scripts/generate-page-mode-oracle.sh`
- **Verification:** The six-row oracle passed strict parsing and build identity validation.
- **Committed in:** `b07a07f`

**3. [Rule 3 - Blocking] Allowed only the required exact page-mode skip registration through the no-skip scanner**
- **Found during:** Task 2
- **Issue:** The blanket scanner rejected the planned CTest skip property even though the two profile tests must visibly skip at a real unsupported device request.
- **Fix:** Added a single line-exact exception for the two named tests and exact anchored signal. Other skip and disable properties remain errors.
- **Files modified:** `tests/cmake/no_skip.cmake`, `tests/suites/tests.cmake`
- **Verification:** The scanner and its self-test passed; a near-match test name was rejected; JUnit negatives still reject missing, extra, duplicate, silent/undeclared, and stale skips.
- **Committed in:** `04c4f79`

**4. [Rule 3 - Blocking] Report a missing pinned oracle tool with its exact refusal**
- **Found during:** Task 2
- **Issue:** With `ca65` absent, the oracle invoked `shasum` before checking the executable and emitted a generic file error, so the tool-hostility control could not prove the named refusal.
- **Fix:** Check the pinned tool executable before hashing it.
- **Files modified:** `scripts/generate-page-mode-oracle.sh`, `tests/gates/page-mode-script-controls.sh`
- **Verification:** The isolated build/oracle hostility suite passed with positive brackets and exact diagnostics for missing/changed inputs and unsafe output paths.
- **Committed in:** `04c4f79`

**Total deviations:** 4 auto-fixed (3 Rule 3 blocking integration fixes, 1 Rule 1 bug fix)
**Impact on plan:** These narrow fixes were necessary to complete the approved source, manifest, and skip gates. No emulator device behavior or unrelated legacy artifacts were changed.

## Issues Encountered

- The first isolated hostility run exposed the missing-tool diagnostic gap described above. The control failed on that exact assertion after positive build/oracle brackets and missing-source/notice controls passed; the same complete control test passed after the guard was added. The mutation was confined to a copied pinned input tree.
- The plan’s example output path was already occupied and correctly refused for reuse. A fresh nonce-owned output was used and its successful identities were captured before verified cleanup.
- A GSD state refresh briefly rendered the unresolved O1 profile as blank; I restored the existing `none` wording after reviewing the generated diff.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

The source build, oracle, profile stop evidence, and hostile-input controls are ready for later Phase 05 work. EVD-02 remains open until the source reaches result writes and the remaining strict timer agreement is resolved. The exact unsupported `$FD8C` serial write is the current runtime blocker; no owner action is needed for this plan.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists at the required phase path.
- Task commits `338173d`, `b07a07f`, and `04c4f79` are ancestors of the current HEAD.
- The persisted plan ledger measures three task commits from `f59b362` through `04c4f79`.
