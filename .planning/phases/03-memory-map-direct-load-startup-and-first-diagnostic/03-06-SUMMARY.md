---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "06"
subsystem: api-and-determinism
tags: [snapshot, watch, determinism, c17]

# Dependency graph
requires:
  - phase: 03-05
    provides: post-write watch API and MAPCTL routing
provides:
  - Successful snapshot restore clears host watch configuration and hit history after validation
  - Refused restore preserves the complete machine including watch state
  - Permanent edge, continuation, independent-instance and digest-isolation controls
affects: [03-07, 03-08, 03-09]

# Actuals (#2632), measured from the realized task diff at chars/4 scale.
actuals:
  tokens: 12782
  tasks: 2
  commits: 3
commits: 3
plan_head_before: 577289b65e7dc8775f357f4f4b68401b12439c2a
plan_head_after: 1d63e3db30621a96551f423c2d2df34227c6c623

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Host observer fields remain outside guest snapshots and normalized guest digests.
    - Restore validates and applies guest state before clearing host watch observation.

key-files:
  created:
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-06-T1-RED.json
  modified:
    - core/snapshot.c
    - tests/api/test_api.c
    - tests/det/test_det.c
    - runner/bundle.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/memory-startup.md

key-decisions:
  - "Keep the established watch event semantics: hit at the bus grant, return at grant+1 after store and CPU completion."
  - "No golden or guest-digest rebaseline was needed because host-only watch state does not change guest observations."
  - "Keep DET-03's unclassified probe visible as an assumption; this plan's controls do not auto-classify it."

patterns-established:
  - "Normalize segmented and watched run metadata to cumulative budget, total guest ticks, and final stop before comparing guest digests."
  - "Use an independently changing guest counter to detect a replayed watched store."

# This is a phase slice; global requirement completion remains parent-owned.
requirements-completed: []

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "Validated restore clears host observer state; refused restore preserves the whole instance."
    verification:
      - kind: unit
        ref: "api.snapshot_continue; api.snapshot_refusal"
        status: pass
    human_judgment: false
  - id: D2
    description: "Uninterrupted, split-budget, watched and restored execution agree on guest observations and digests at common ticks."
    verification:
      - kind: integration
        ref: "det.two_instances"
        status: pass
    human_judgment: false
  - id: D3
    description: "Watch range edges, post-write continuation, instance isolation and digest separation have permanent controls."
    verification:
      - kind: unit
        ref: "api.watch_post_write; api.watch_controls; api.watch_independent; api.watch_controls.self_test; api.snapshot_continue.self_test"
        status: pass
    human_judgment: false

# Metrics
duration: 49 min
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 06: Watch Isolation and Snapshot Continuation Summary

**Snapshot restore now clears host watch state only after guest-state validation, and permanent controls show watched, segmented and restored runs retain identical guest results.**

## Performance

- **Duration:** 49 min
- **Started:** 2026-11-10T19:27:48Z
- **Completed:** 2026-11-10T20:16:42Z
- **Tasks:** 2
- **Files changed:** 9, including the normalized RED evidence record

## Accomplishments

- Successful restore clears watch configuration and hit data after guest state has been validated and applied. Refused restores compare byte-identically, including active watch state.
- Snapshot bytes and bundle observation digests exclude host watch state. Uninterrupted, split-budget, watched/rearmed and restored paths match at tick 4096 across all three profiles and both test cost models. The golden digest is unchanged.
- Permanent API cases cover half-open range start/end-1/outside edges, reads, empty and overflowing ranges, repeated configure/clear, rearm, `$FFFF`, independent interleaved instances, real store continuation, and host-state digest leak controls.

## Task Commits

1. **Task 1 RED: Specify snapshot watch isolation** — `f91e0d3` (`test`)
2. **Task 1 GREEN: Clear watch state after validated restore** — `f792a3b` (`fix`)
3. **Task 2: Register permanent watch continuation controls** — `1d63e3d` (`test`)

The separate plan metadata commit records this summary and state bookkeeping.

## Files Created/Modified

- `core/snapshot.c` — clears host watch observation only after restore validation and guest-state application.
- `tests/api/test_api.c` — checks snapshot/refusal isolation, watch boundary and continuation behavior, and bundle digest exclusion with named faults.
- `tests/det/test_det.c` — compares uninterrupted, split, watched and restored guest observations at common ticks.
- `runner/bundle.c` — documents that canonical observations exclude host watch fields and require normalized aggregate run metadata.
- `tests/api/tests.cmake`, `tests/inventory/fast.txt`, `docs/claims/skeleton-repeatability.claim` — register exact tests and bind the claim to the updated inventory hash.
- `docs/memory-startup.md` — documents restore/rearm, post-write timing, digest separation and evidence limits.
- `03-06-T1-RED.json` — normalized red-run evidence; the private raw log remained in temporary storage.

## Verification

Task 1:

- `cmake --preset check && cmake --build --preset check` — passed.
- `build/check/tests/lynx-test-api snapshot_continue` and `build/check/tests/lynx-test-api snapshot_refusal` — passed.
- `ctest --preset check -R '^det\.golden(\.self_test)?$' --no-tests=error --output-on-failure` — passed; golden unchanged.
- `ctest --preset check -R '^det\.two_instances$' --no-tests=error --output-on-failure` — passed.
- `api.advance_budget` and `api.snapshot_roundtrip` — passed after correcting the white-box vector-wrap fixture to map the vector window explicitly.

Task 2:

- `cmake --preset check && cmake --build --preset check` — passed.
- `cmake --build --preset check && ctest --preset check -R '^api\.(watch_|snapshot_)' --no-tests=error --output-on-failure` — passed, 8/8 tests, including all named fault controls.
- `sh scripts/check-claims.sh`, `sh scripts/hygiene.sh --tree`, and `git diff --check` — passed.

## TDD Evidence

Task 1's `snapshot_continue` RED run is recorded in `03-06-T1-RED.json`; `gsd_run check tdd-red-evidence` returned `RED_EVIDENCE_OK`. Semantic inspection confirmed the intended assertion failed because successful restore retained the host watch configuration/hit. The `fix` commit then cleared the observer after validation and passed the targeted restore and determinism checks. Task 2 only added tests and documentation, so it did not add an artificial RED commit.

## Decisions and Assumptions

- Watch events still report the bus grant tick and return at grant+1, after the guest store and CPU continuation are committed.
- A successful restore requires explicit host rearm. Callers serialize operations on one machine; independent machines have separate observer state.
- No digest or golden rebaseline was required: watched state did not alter the public guest observations or normalized digest inputs.
- DET-03's unclassified probe remains a flagged assumption. Same-build repeatability evidence does not establish hardware accuracy.

## Deviations from Plan

### Auto-fixed Issues

**1. Corrected the snapshot wrap fixture to respect the Phase 3 memory map**
- **Found during:** Task 1 RED verification
- **Issue:** The prior fixture crossed `$FFF9`, which is MAPCTL and no longer behaves as vector-window RAM under the current policy.
- **Fix:** Rebuilt the white-box fixture at `$FFFA`, explicitly selected vector-window RAM with MAPCTL bit 3, and retained the actual CPU `$FFFF->$0000` fetch-wrap proof. The fixture is labeled as white-box construction, not startup evidence.
- **Files modified:** `tests/api/test_api.c`
- **Commit:** `f91e0d3`

**2. Extended the independent-instance budgets to reach each intended store**
- **Found during:** Task 2 targeted CTest run
- **Issue:** The first run budget ended before the second instance's `$0401` store.
- **Fix:** Interleaved bounded advances so one instance hits `$0400`, the other advances to its `$0401` store, and the first event is checked again for cross-talk.
- **Files modified:** `tests/api/test_api.c`
- **Commit:** `1d63e3d`

## Threat Surface

No new network, filesystem, authentication or trust-boundary surface was introduced. Snapshot state remains validate-before-mutate, and refusal preservation includes host watch bytes.

## Self-Check: PASSED

- Summary file exists at the required phase path.
- Task commits `f91e0d3`, `f792a3b`, and `1d63e3d` are ancestors of the current plan head.
