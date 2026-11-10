---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "02"
subsystem: startup
tags: [bll, runner, manifest, determinism, api]
requires:
  - phase: 03-01
    provides: [strict-bs93-loader, bll-identity, snapshot-format-3]
provides:
  - bounded-binary-bll-runner-input
  - core-derived-startup-manifest
  - bll-ownership-and-refusal-coverage
affects: [phase-03-runner-cli, phase-03-qualification, phase-03-requirements]
tech-stack:
  added: []
  patterns:
    - bounded-max-plus-one-file-read
    - runner-manifest-derived-from-public-identity
    - deterministic-fixtures-wrapped-as-bll
key-files:
  created:
    - docs/memory-startup.md
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-02-T2-RED.json
  modified:
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - runner/main.c
    - runner/bundle.c
    - runner/bundle.h
    - tests/det/test_det.c
    - tests/header/header.c
    - tests/det/sensitivity.cmake
    - .planning/WINDOWS.md
key-decisions:
  - "The runner accepts one complete binary BS93 object and uses the core's parsed load and entry address."
  - "Manifest format, startup, whole-object hash, entry and payload fields come only from public lynx_identity."
  - "Keep the provisional public raw-image API until plan 03-03 removes it."
  - "The required sensitivity test moves to binary BLL here; the broader runner.cli migration remains in plan 03-03."
requirements-completed: [MEM-02, MEM-03, MEM-04]
coverage:
  - id: D1
    description: "BLL source ownership, full-object identity, strict bounds and whole-instance refusal behavior."
    requirement: MEM-02
    verification:
      - kind: unit
        ref: "ctest --preset check -R '^api\\.(bll_|snapshot_)' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
  - id: D2
    description: "Binary BLL runner startup and public-identity manifest fields."
    requirement: MEM-04
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^(api\\.(header_selfcontained|abi_symbols)|det\\.(two_instances|sensitivity|sensitivity_state))$' --no-tests=error --output-on-failure"
        status: pass
      - kind: integration
        ref: "Manual lynx-run BLL smoke: valid, empty, trailing, oversized and obsolete --load-addr cases."
        status: pass
    human_judgment: false
  - id: D3
    description: "Startup documentation, exact test inventory and inventory-bound claim."
    requirement: MEM-03
    verification:
      - kind: other
        ref: "sh scripts/check-claims.sh and cmake --preset check"
        status: pass
    human_judgment: true
    rationale: "Automated checks bind the claim and inventory, but do not judge whether the hardware and rights limits are clearly described."
actuals:
  tokens: 7617
  tasks: 2
  commits: 3
plan_head_before: d24653371cb0a412b6f70bef7a5e2dd370b93d49
plan_head_after: 573efc5a1fd42331ee64d278a18c293cd0bbcd61
duration: 00:26:42
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 2: BLL ownership proofs and strict runner startup Summary

**Bounded binary BS93 runner loading with core-owned startup metadata and complete-object ownership/refusal tests**

## Performance

- **Duration:** 26m 42s through summary and state bookkeeping.
- **Started:** 2026-11-10T16:42:41Z
- **Completed:** 2026-11-10
- **Tasks:** 2
- **Files changed:** 13, including the persisted RED record and fixed deviation ledger entry.

## Accomplishments

- Added API cases for BLL load bounds, exact-touch and overlapping input, copied-source ownership, header-sensitive identity and unsupported object types. The existing tracer and snapshot cases still pass.
- Changed `lynx-run` to accept a bounded binary BLL object and call `lynx_load_bll`; removed `--load-addr`. The bundle reports BS93, startup A, whole-object SHA-256, entry, object length and payload range from public identity.
- Updated deterministic tests to package original payload bytes in canonical BLL containers. Their independent observation golden and state-sensitivity checks remain active.
- Added startup policy documentation, registered the four API tests, and refreshed the inventory-bound claim in the same task commit.

## Verification

Task 1:

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check -R '^api\.(bll_|snapshot_)' --no-tests=error --output-on-failure
```

Passed: 6/6 tests. `sh scripts/check-claims.sh` and `git diff --check` also passed.

Task 2:

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check -R '^(api\.(header_selfcontained|abi_symbols)|det\.(two_instances|sensitivity|sensitivity_state))$' --no-tests=error --output-on-failure
```

Passed: 5/5 tests. The test-first `det.two_instances` RED was classified `RED_EVIDENCE_OK`; its failure was limited to the not-yet-added manifest fields. The persisted report is `03-02-T2-RED.json`.

A runner smoke loaded a 23-byte BLL object, ran 300 ticks, and confirmed the manifest hash equals an independent SHA-256 of the complete object. Empty, trailing-byte, 65,536-byte and obsolete-`--load-addr` inputs returned the expected refusal codes and created no output directory.

## Task Commits

1. **Task 1: Prove BLL ownership, refusal boundaries and startup policy** — `fe57a9c`
2. **Task 2 RED: Assert BLL runner identity metadata** — `d37170d`
3. **Task 2 GREEN: Load strict binary BLL in runner** — `573efc5`

## Files Created/Modified

- `tests/api/test_api.c`, `tests/api/tests.cmake`, `tests/inventory/fast.txt` — BLL bounds, ownership and exact inventory cases.
- `docs/memory-startup.md`, `docs/claims/skeleton-repeatability.claim` — startup limits and refreshed inventory binding.
- `runner/main.c`, `runner/bundle.c`, `runner/bundle.h` — bounded file reader and identity-derived manifest.
- `tests/det/test_det.c`, `tests/header/header.c` — deterministic BLL loads and public-signature policy.
- `tests/det/sensitivity.cmake` — required sensitivity harness now produces binary BLL input.
- `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-02-T2-RED.json` — TDD evidence.
- `.planning/WINDOWS.md` — records the completed sensitivity-harness migration; the pre-existing timing stub remains open.

## Requirements Coverage and Limits

This is a plan-level contribution to MEM-02, MEM-03 and MEM-04. No global requirement status was changed. MAPCTL mapping, full diagnostic behavior and broader Phase 3 acceptance remain with later plans. No BIOS, commercial content, physical test, baseline/accuracy claim, licence decision or release work was introduced.

## Decisions Made

- The file boundary is one exact binary object, capped by the BLL 16-bit total length; a max-plus-one read detects oversized input without unbounded allocation.
- The manifest retains `startup_tier = A` and adds the explicit `startup = A` tag plus the full payload and entry metadata.
- Keep `lynx_load_ram_image` and its public signature until plan 03-03 removes the transitional raw path.

## TDD Evidence

- Task 1 added coverage for behavior already implemented in 03-01, so it used passing tests without an artificial RED.
- Task 2 RED: `det.two_instances` executed and failed on missing manifest fields; the CTest JUnit report and semantic assessment passed `gsd_run check tdd-red-evidence`.
- Task 2 GREEN: the runner and manifest changes passed the focused API/determinism set above.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Migrated the required sensitivity harness to binary BLL**

- **Found during:** Task 2.
- **Issue:** `det.sensitivity` still invoked the runner with hex content and `--load-addr`, so the required Task 2 verification failed after the runner contract changed.
- **Fix:** Repackaged its source fixture as a bounded BS93 object for each content variant.
- **Files modified:** `tests/det/sensitivity.cmake`.
- **Verification:** The full Task 2 focused command passed 5/5 after the migration.
- **Commit:** `573efc5`; deviation recorded as fixed in `.planning/WINDOWS.md`.

## Known Stubs

- Flat five-master-tick timing remains `PLACEHOLDER`, documented at `docs/memory-startup.md:42` and already tracked by the existing open timing entry in `.planning/WINDOWS.md`. No new stub was introduced.

## Issues Encountered

- The RED run exposed the expected missing manifest fields; implementation fixed them.
- The existing `tests/det/runner_cli.cmake` still uses the prior hex/`--load-addr` CLI and is assigned to plan 03-03. The required checks here do not execute that test; the manual smoke covers this plan's new CLI behavior.

## Auth Gates

None.

## Next Phase Readiness

Plan 03-03 can remove the provisional public raw-image entry point and migrate the remaining `runner.cli` fixture/script to binary BLL. Do not treat this plan's direct-load observations as firmware boot or hardware compatibility evidence.

---
*Phase: 03-memory-map-direct-load-startup-and-first-diagnostic*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Verified this summary and the persisted RED evidence file exist.
- Verified task commits `fe57a9c`, `d37170d` and `573efc5` are ancestors of `plan_head_after`.
- Verified the ledger range from `plan_head_before` through `plan_head_after` measures 3 task commits.

