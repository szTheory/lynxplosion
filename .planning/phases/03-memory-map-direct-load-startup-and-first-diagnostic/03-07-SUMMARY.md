---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "07"
subsystem: testing
tags: [cc65, ca65, ld65, BS93, MAPCTL, CTest, snapshots]
requires:
  - phase: 03-06
    provides: Public machine API and deterministic watch/snapshot continuation behavior
provides:
  - Original source-built BS93 diagnostic with a manifested, reproducible text identity and named completion fault variants
  - Strict public-API diagnostic result, symbol, pixel, runner and continuation controls
affects: [03-08, 03-09, memory-map, diagnostic, snapshot]
actuals:
  tokens: 20149
  tasks: 2
  commits: 2
commits: 2
plan_head_before: 833f304473d731b914a09a7a53773ffccb0ac6d7
plan_head_after: 9ffe4aabf3c2b5497505bc3c49cbadf6f5daac10
tech-stack:
  added: []
  patterns:
    - Pinned ca65/ld65 builds write BLL objects and linked labels into isolated, owned scratch paths.
    - Independent consumers parse actual -Ln symbols and use only public machine APIs for guest observations.
key-files:
  created:
    - fixtures/diagnostic/first.s
    - fixtures/diagnostic/bll.cfg
    - fixtures/diagnostic/first.hex
    - fixtures/manifests/first-diagnostic.manifest
    - tests/diag/test_diag.c
    - tests/diag/tests.cmake
  modified:
    - scripts/check.sh
    - runner/main.c
    - runner/bundle.c
    - tests/api/test_api.c
    - tests/det/runner_cli.cmake
    - tests/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/cpu-profiles.md
decisions:
  - "The harness resolves linked symbols and validates their ranges before reading guest RAM; it reads the fixed result block and checks its schema before reading pixels."
  - "Flat5 tick bounds are hand-derived diagnostic bounds labelled PLACEHOLDER; they are not hardware timing evidence."
patterns-established:
  - "Each diagnostic CTest driver assembles its own variants with the pinned toolchain in an isolated scratch directory and removes successful outputs before artifact scanning."
  - "The independent diagnostic harness includes no private machine headers; its local SHA-256 helper only rehashes test-forged snapshot bytes."
requirements-completed: []
coverage:
  - id: D1
    description: "Project-authored BS93 diagnostic, MAPCTL startup, source-built fault variants and manifested text identity."
    requirement: MEM-01
    verification:
      - kind: integration
        ref: "sh scripts/check-manifests.sh && sh scripts/check.sh"
        status: pass
      - kind: integration
        ref: "fixture.bll_header; fixture.bll_header.self_test"
        status: pass
    human_judgment: false
  - id: D2
    description: "Public API oracle for strict symbols, result/schema/pixels, named failures, runner output and continued state."
    requirement: MEM-04
    verification:
      - kind: integration
        ref: "cmake --preset check && cmake --build --preset check && ctest --preset check -R '^(fixture\\.bll_header|diag\\.|det\\.diagnostic_restore)' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
duration: 56min
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 7: Original source-built diagnostic and result oracle Summary

**A source-built tier-A BS93 diagnostic now reports a bounded, symbol-labelled result that an independent public-API harness checks through faults and continuation paths.**

## Performance

- **Duration:** 56 min
- **Started:** 2026-11-10T20:19:26Z
- **Completed:** 2026-11-10T21:15:20Z
- **Tasks:** 2
- **Files modified:** 15

## Accomplishments

- Added an original ca65 diagnostic and linker layout that builds a canonical BS93 object, initializes MAPCTL in guest code, checks RAM-selected windows, and writes a structured result plus packed 8x8 diagnostic pixels.
- Manifested the source, recipe, text identity, all-rights-reserved status and permitted test use; added normal, deliberate-error and no-done variants using the pinned toolchain.
- Added public-API controls for strict linked symbols, result schema and fields, independent pixel and palette expectations, runner agreement, and watched/sliced/mid-diagnostic snapshot continuation.

## Task Commits

1. **Task 1: Build the original diagnostic and fault variants reproducibly** — `05f409b` (`feat`)
2. **Task 2: Verify result symbols, guest progress and continued diagnostic state** — `9ffe4aa` (`test`)

## Files Created/Modified

- `fixtures/diagnostic/first.s` — Original guest diagnostic with guest-owned MAPCTL initialization, result writes, RAM comparisons and packed pixel data.
- `fixtures/diagnostic/bll.cfg` — Project-owned BS93 output and BSS symbol layout.
- `fixtures/diagnostic/first.hex` — Exact source-built object identity.
- `fixtures/manifests/first-diagnostic.manifest` — Source, build, identity, rights and allowed-use record.
- `scripts/check.sh` — Pinned-toolchain build and validation for normal and fault objects.
- `tests/diag/test_diag.c` — Strict `-Ln` parser, result/pixel oracle, named controls and public snapshot comparisons.
- `tests/diag/tests.cmake` — Isolated fixture build driver and six diagnostic test registrations.
- `tests/tests.cmake`, `tests/inventory/fast.txt` — Test inclusion and exact six-test inventory.
- `docs/claims/skeleton-repeatability.claim` — Inventory hash updated to `d8db2631bbec09eba8198d84580db0a7f556377a204460d78876803c8bb16121`.
- `tests/api/test_api.c` — Profile-divergence white-box probe selects vector RAM so it tests CPU profile behavior without an unrelated unavailable fetch.
- `runner/bundle.c`, `runner/main.c`, `tests/det/runner_cli.cmake`, `docs/cpu-profiles.md` — Preserve and verify the public unavailable stop reason in runner bundles and CLI output.

## Decisions Made

- The harness reads actual linked `-Ln` addresses after strict parsing, validates symbol ranges before guest-memory access, then reads the fixed result block and validates magic/schema before dependent pixel reads.
- The diagnostic’s raw packed pixel values and palette table are test-owned snapshots; they make no canonical Lynx video claim.
- No new owner decision or licence was made. The fixture remains all-rights-reserved, and flat5 remains a PLACEHOLDER timing model.

## Verification

- `sh scripts/check-manifests.sh && sh scripts/check.sh` — passed; 110/110 registered tests passed, followed by the binary artifact scan. This full gate also confirmed the profile-divergence and unavailable-runner integration fixes.
- `cmake --preset check && cmake --build --preset check && ctest --preset check -R '^(fixture\.bll_header|diag\.|det\.diagnostic_restore)' --no-tests=error --output-on-failure` — passed all 6 diagnostic tests. Repeated with Task 1’s retained `build/toolchain/first.bll` and `.lbl` removed; each CTest driver rebuilt the fixture into its own scratch directory.
- `sh scripts/check-claims.sh` and `LC_ALL=C sort -c tests/inventory/fast.txt` — passed.
- Successful diagnostic CTest drivers removed their fixture and runner bundle files; `build/check/scratch/diag` contained no files after the gate.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 — Bug] Preserve the intended CPU profile-divergence probe under the new map**
- **Found during:** Task 1 full check.
- **Issue:** The exhaustive CPU table test loaded a private vector but left MAPCTL at the public startup default, so vector fetches stopped as unavailable before the intended profile-divergence assertion.
- **Fix:** The white-box CPU probe now selects vector RAM before each private vector load and after each reset. This remains isolated from the public tier-A startup contract.
- **Files modified:** `tests/api/test_api.c`
- **Verification:** `cpu.profile_divergence` passed in the full gate.
- **Committed in:** `05f409b`.

**2. [Rule 1 — Bug] Carry the public unavailable stop through runner output**
- **Found during:** Task 1 full check.
- **Issue:** After the map contract added `LYNX_STOP_UNAVAILABLE`, the runner’s stop serializer did not recognize it, so that public stop could not produce its documented partial bundle.
- **Fix:** Serialize the stop as `unavailable`, write the partial bundle, and emit a generic note that no target is implemented for the route.
- **Files modified:** `runner/bundle.c`, `runner/main.c`, `tests/det/runner_cli.cmake`, `docs/cpu-profiles.md`
- **Verification:** `runner.cli` passed and asserted the unavailable manifest value, partial bundle and note.
- **Committed in:** `05f409b`.

**3. [Rule 3 — Blocking] Resolve the shellcheck warning in the fixture build step**
- **Found during:** Task 1 full check.
- **Issue:** Shellcheck flagged an unquoted `set --` expansion in the new source-build step.
- **Fix:** Replaced the ambiguous expansion with an explicit fixed case mapping for the diagnostic variants.
- **Files modified:** `scripts/check.sh`
- **Verification:** Shellcheck and the full `scripts/check.sh` gate passed.
- **Committed in:** `05f409b`.

**Total deviations:** 3 auto-fixed (2 Rule 1, 1 Rule 3). All were direct integration or validation fixes needed to complete the planned gates.

## Issues Encountered

- The first Task 2 CTest attempt stopped at a generated CMake driver syntax error before fixture execution; the calls were corrected. The next run exposed an unsupported regex repetition form in a result-row assertion; checking exact row length and a zero-only suffix fixed it. The final six-test run passed.
- No new dependencies, scanner exemptions, hardware tests, firmware inputs or rights decisions were introduced.

## User Setup Required

None.

## Next Phase Readiness

- This plan’s two tasks and checks are complete. Phase-wide requirement completion remains open for the parent’s review, security audit and verification, and for the remaining Phase 3 plans.
- The 3000–4000 tick completion window is derived from the source instruction trace and uses the flat5 PLACEHOLDER cost; it is not Lynx hardware timing evidence.
- No physical hardware, firmware, commercial game content, release or baseline decision was used or qualified.

## Self-Check: PASSED

- Summary file exists at the planned phase path.
- Task commits `05f409b` and `9ffe4aa` are ancestors of the current HEAD.

---
*Phase: 03-memory-map-direct-load-startup-and-first-diagnostic*
*Completed: 2026-11-10*
