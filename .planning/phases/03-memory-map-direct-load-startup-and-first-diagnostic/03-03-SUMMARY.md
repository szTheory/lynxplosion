---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "03"
subsystem: api
tags: [C, BLL, MAPCTL, runner, manifests, CPU-vectors]
requires:
  - phase: 03-02
    provides: strict BLL object identity and runner startup metadata
provides:
  - Strict BLL-only public loading and test runner path
  - Private flat-vector machine-loop adapter with distinct test identity
  - Startup-tag validation and successful-test artifact cleanup
affects: [03-04, 03-09, runner, api, determinism-tests]
actuals:
  tokens: 8696
  tasks: 2
  commits: 6
  files: 17
plan_head_before: 602b53a095912cab84f1d0ab4a95c0da491132f5
plan_head_after: e473d90a3e4b3d86ef0092f6bf9736a5d27e98ad
commits: 6
tech-stack:
  added: []
  patterns:
    - Private test-only flat-bus identity selected only by the CPU-vector adapter
    - Temporary binary BLL inputs are removed after successful CMake test scripts
key-files:
  created: []
  modified:
    - core/machine.c
    - include/lynxplosion.h
    - scripts/validate-kv.sh
    - scripts/scan-binaries.sh
    - tests/api/test_api.c
    - tests/cpu/test_cpu_vectors.c
    - tests/det/runner_cli.cmake
    - tests/det/manifest_fields.cmake
    - tests/det/manifest_controls.sh
    - tests/det/two_runs.cmake
    - tests/det/golden.cmake
    - tests/det/sensitivity.cmake
    - tests/hygiene/scan_control.sh
key-decisions:
  - Keep CPU vector flat-bus cases on a private identity so the existing machine-loop seam remains distinct from public BLL startup.
  - Remove the provisional public raw loader after ordinary callers and fixtures use strict BLL objects.
  - Require any manifest startup tag to be A and require startup and startup_tier to agree when both are present.
patterns-established:
  - Normal API and runner fixtures package payloads in canonical BS93 objects and assert complete-object identity.
  - Test scripts retain failed-run output for diagnosis and remove generated BLL files after success.
requirements-completed: []
coverage:
  - id: D1
    description: Public API and runner normal paths use strict BLL, with the raw loader removed.
    verification:
      - kind: integration
        ref: sh scripts/check.sh
        status: pass
    human_judgment: false
  - id: D2
    description: CPU-vector tests retain flat-bus machine-loop coverage through a private distinct identity.
    verification:
      - kind: unit
        ref: ctest --preset check -R '^(api|cpu|core)\.' --output-on-failure
        status: pass
    human_judgment: false
  - id: D3
    description: Startup manifest tags are validated and run keys remain in canonical order.
    verification:
      - kind: integration
        ref: sh scripts/check.sh
        status: pass
    human_judgment: false
duration: 58min
completed: 2026-11-10
status: complete
---

# Phase 03 Plan 03: Complete caller migration and private flat-vector seam Summary

**Normal API and runner paths now use strict BLL, while CPU-vector tests retain a private flat-bus machine-loop path with distinct identity.**

## Performance

- **Duration:** 58 min
- **Started:** 2026-11-10T17:13:21Z
- **Completed:** 2026-11-10T18:11:06Z
- **Tasks:** 2
- **Files modified:** 17

## Accomplishments

- Migrated ordinary API and runner test inputs to binary BS93 objects, removed the public raw loader, and retained a negative header/link control for the removed symbol.
- Preserved the CPU-vector flat-bus seam, including high addresses and undefined-row restore coverage, with a private adapter and distinct snapshot identity.
- Enforced startup-tag consistency, retained the existing golden and sensitivity results, and cleaned successful test scratch outputs so binary artifacts do not remain in the build tree.
- The full local gate passed after the privacy-history repair: 98 tests, JUnit accounting, artifact scan, source/tree hygiene, and history hygiene. This does not establish hardware accuracy.

## Task Commits

1. **Task 1: Migrate fixtures and CPU seam** — `68902a5` (`refactor`)
2. **Task 2 RED: BLL-only runner and startup controls** — `2147046` (`test`)
3. **Task 2 RED: long UTF-8 scanner regression** — `a5e2ca6` (`test`)
4. **Task 2: Remove public raw loader and enforce BLL startup** — `e473d90` (`feat`)
5. **Privacy evidence path normalization** — `a923e4f` (`docs`)
6. **Unpublished history repair audit/reference refresh** — `cefd617` (`docs`)

**Plan metadata:** recorded in the final closeout commit.

## Files Created/Modified

- `core/machine.c`, `include/lynxplosion.h` — private flat-vector path and removal of the public raw loader.
- `tests/api/test_api.c`, `tests/cpu/test_cpu_vectors.c` — BLL fixture migration and private vector adapter coverage.
- `tests/det/` — binary BLL inputs, runner/header negative controls, startup-manifest controls, and successful-run scratch cleanup.
- `scripts/validate-kv.sh` — startup tag validation and conflict checks.
- `scripts/scan-binaries.sh`, `tests/hygiene/scan_control.sh` — fail-closed UTF-8 validation and a long-text regression control.
- `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-02-T2-RED.json` — local checkout paths normalized; the history repair audit is [03-HISTORY-REPAIR.md](./03-HISTORY-REPAIR.md).

## Decisions Made

- The private CPU-vector adapter may select flat bus behavior only through its internal distinct identity; public BLL loading cannot choose it.
- The validator accepts fixture manifests that omit run-only `startup`, but any present `startup` must be A and match `startup_tier`.
- No golden rebaseline was needed. The existing deterministic observation golden still passes, with no accuracy inference.
- Requirements MEM-02, MEM-03, MEM-04, and DET-03 remain open at the phase level; this plan does not complete phase-spanning requirements by itself.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing required validation] Enforced startup cross-field consistency in the manifest validator.**
- **Found during:** Task 2
- **Issue:** The plan’s success criteria require conflicting startup tags to be rejected, but the validator only checked each key independently.
- **Fix:** Require `startup = A` when present and reject a mismatch with `startup_tier`; add missing, empty, duplicate, invalid, and conflict controls.
- **Files modified:** `scripts/validate-kv.sh`, `tests/det/manifest_controls.sh`
- **Verification:** `det.manifest_fields.self_test` and the full check passed.
- **Committed in:** `e473d90`

**2. [Direct plan adjustment] Removed stale public-loader header assertions.**
- **Found during:** Task 2
- **Issue:** `tests/header/header.c` still asserted the provisional public loader signature that this plan deletes.
- **Fix:** Removed that stale assertion; `runner.cli` now proves the public declaration and linked symbol are both absent while a current BLL consumer compiles and links.
- **Files modified:** `tests/header/header.c`, `tests/det/runner_cli.cmake`
- **Verification:** `runner.cli` and the full check passed.
- **Committed in:** `2147046`, `e473d90`

**3. [Rule 1 - Scanner false positive] Made UTF-8 validation robust to macOS iconv output-device failure.**
- **Found during:** Full check
- **Issue:** macOS `iconv` reported EIO when valid long UTF-8 text containing progress-bar block glyphs was directed to `/dev/null`, so the source scanner mislabeled valid text as binary.
- **Fix:** Capture iconv output in the scanner’s temporary directory, check its exit status, and compare the round-trip bytes. Added a test-first long UTF-8 control; existing invalid-byte and control-byte controls still reject their inputs.
- **Files modified:** `scripts/scan-binaries.sh`, `tests/hygiene/scan_control.sh`
- **Verification:** Scanner controls and the full check passed.
- **Committed in:** `a5e2ca6`, `e473d90`

**4. [Rule 3 - Blocking artifact scan] Removed generated binary test inputs after successful scripts.**
- **Found during:** Full check artifact scan
- **Issue:** Successful CMake tests left BLL fixtures and the temporary current-API executable in `build/check`, which the artifact scan correctly rejected.
- **Fix:** Remove the test work directory only after all assertions pass, preserving diagnostic output on failures. Removed four stale, untracked generated BLL fixtures from the local build directory.
- **Files modified:** `tests/det/golden.cmake`, `tests/det/golden_self_test.cmake`, `tests/det/manifest_fields.cmake`, `tests/det/runner_cli.cmake`, `tests/det/sensitivity.cmake`, `tests/det/two_runs.cmake`
- **Verification:** Focused CTest set and the full artifact scan passed.
- **Committed in:** `e473d90`

**5. [Privacy correction] Repaired checkout paths in unpublished evidence history.**
- **Found during:** Full check history scan
- **Issue:** A tracked 03-02 RED report contained checkout-specific absolute paths across six unpublished commit snapshots.
- **Fix:** Normalize the report paths and coordinate an unpublished-only history repair; the remote main ref was unchanged. The audit records the commit mapping and tree-preservation checks.
- **Files modified:** Prior-plan RED evidence and `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-HISTORY-REPAIR.md`
- **Verification:** `sh scripts/hygiene.sh --history`, `sh scripts/hygiene.sh --tree`, `git diff --check`, and the full check passed.
- **Committed in:** `a923e4f`, `cefd617`

**Total deviations:** 5 (four implementation/verification fixes and one privacy correction coordinated with the parent).
**Impact on plan:** Required to satisfy the plan’s manifest contract and full local gate; no firmware, emulator implementation source, new dependency, or hardware-accuracy claim was added.

## TDD Gate Compliance

- Task 2 runner and startup-tag controls produced targeted RED evidence before the public API removal and validator changes; both targets passed after implementation.
- The long UTF-8 scanner control was committed first as a failing regression, then passed with the fail-closed scanner change.
- Final verification: `sh scripts/check.sh` passed, including all 98 registered tests and both current-tree and history hygiene gates.

## Known Stubs

| File | Line | Reason |
|------|------|--------|
| `include/lynxplosion.h` | 11 | The five-master-tick timing model remains provisional; this plan proves deterministic bounded execution, not hardware timing accuracy. |

## Issues Encountered

The full check initially exposed a history privacy finding, the macOS iconv false positive, and leftover generated BLL artifacts. They were repaired and the final full gate passed.

## User Setup Required

None.

## Next Phase Readiness

The strict BLL path and private flat-vector seam are ready for Phase 03-04 MAPCTL work. The full 2 GB vector lane was not run; later phase qualification remains responsible for cross-platform evidence. No hardware accuracy or boot claim is made.

---
*Phase: 03-memory-map-direct-load-startup-and-first-diagnostic*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary file exists at the required phase path.
- All six measured plan-range commits are ancestors of the current HEAD.
