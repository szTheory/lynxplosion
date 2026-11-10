---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "05"
subsystem: memory
tags: [MAPCTL, CPU bus, watch, C17, CTest]
requires:
  - phase: 03-04
    provides: MAPCTL decoder, CPU-mapped peek, and unavailable-route policy
provides:
  - Permanent map, raw-versus-mapped, and unavailable-ROM controls
  - Per-instance one-shot post-write watch with bounded return ticks
affects: [03-06, memory-map verification, snapshot continuation]
actuals:
  tokens: 9391
  tasks: 2
  commits: 3
plan_head_before: b59358d6334630b015a71394d22e069744ca214a
plan_head_after: 0507cf3e72697f37c7a416a992bc79942b5ebbdc
tech-stack:
  added: []
  patterns: [exact public-path fault controls, separate host observation state]
key-files:
  created:
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-05-T2-RED.json
  modified:
    - core/internal.h
    - core/machine.c
    - docs/claims/skeleton-repeatability.claim
    - docs/memory-startup.md
    - include/lynxplosion.h
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/header/header.c
    - tests/inventory/fast.txt
key-decisions:
  - "A matching CPU write returns WATCH at grant+1 only after bus mutation, CPU completion, and the next-ready tick are committed; host watch fields stay outside serialized guest state."
requirements-completed: [MEM-01, MEM-04, API-08]
coverage:
  - id: D1
    description: MAPCTL routes, fixed special addresses, raw/mapped separation, and unavailable ROM refusals have accepted controls and intended failing-direction mutants.
    requirement: MEM-01
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^(memory\\.|api\\.memory_peek_pure)' --no-tests=error --output-on-failure (6/6)"
        status: pass
    human_judgment: false
  - id: D2
    description: Absent ROM, write, vector-window, and opcode requests stop before completion; MAPCTL-selected RAM counterparts execute visible markers.
    requirement: MEM-04
    verification:
      - kind: integration
        ref: "memory.rom_unavailable and memory.rom_unavailable.self_test in the six-test targeted CTest run"
        status: pass
    human_judgment: false
  - id: D3
    description: The watch observes committed CPU writes, obeys half-open budgets, disarms after one hit, and preserves state on invalid calls.
    requirement: API-08
    verification:
      - kind: unit
        ref: "build/check/tests/lynx-test-api watch_post_write"
        status: pass
      - kind: unit
        ref: "build/check/tests/lynx-test-api watch_controls"
        status: pass
    human_judgment: false
duration: 17min
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 05: Permanent map controls and post-write watch API Summary

**Exact inventory controls now exercise MAPCTL and unavailable-target behavior, while a one-shot CPU write watch returns after committed execution at grant+1.**

## Performance

- **Duration:** 17 min
- **Started:** 2026-11-10T19:07:59Z
- **Completed:** 2026-11-10T19:25:15Z
- **Tasks:** 2
- **Files modified or added:** 10, including the normalized RED evidence record

## Accomplishments

- Registered `memory.map_matrix`, `api.memory_peek_pure`, and `memory.rom_unavailable` with grouped controls for a wrong map bit, FFF8 special-address ordering, raw-as-mapped substitution, and unavailable-to-RAM fallback. Each control runs the accepted case first and requires each injected child to exit 1 at its named assertion. ROM read, write, vector-window read, and opcode-fetch paths use actual CPU requests with RAM-selected positive counterparts.
- Added the public configure/clear/get watch API. A matching bus write is performed, completed by the CPU, and assigned its next-ready tick before WATCH returns. At flat5 grant 25, the event records `hit_tick=25`, returns at tick 26, and leaves the next request ready at tick 30. The one-shot disarms, and resumption reaches a later store without replaying the first request.
- Kept watch settings and event data in a distinct host-observation subobject. Successful reset and load clear it; invalid ranges and aliased output preserve it. Snapshot serialization remains explicit and unchanged; plan 03-06 owns restore clearing and digest/continuation controls.
- Updated the inventory-bound claim hash and documented which MAPCTL decoder behavior is source-backed versus provisional refusal/readback policy.

## Verification

- `cmake --preset check && cmake --build --preset check && ctest --preset check -R '^(memory\\.|api\\.memory_peek_pure)' --no-tests=error --output-on-failure` — passed, 6/6 tests. This includes all three accepted controls and their failure-direction controls.
- `sh scripts/check-claims.sh` — passed; the claim hash matches the exact inventory bytes.
- `sh scripts/hygiene.sh --tree` and `git diff --check` — passed before the task commits.
- Task 2 TDD RED: CTest/JUnit ran `api.watch_post_write` and failed at the valid-configuration assertion because the temporary buildable refusal seam returned `LYNX_ERR_STATE`. `gsd_run check tdd-red-evidence` returned `RED_EVIDENCE_OK`. The repository evidence at `03-05-T2-RED.json` has normalized path presentation; the raw private report stayed in temporary storage.
- `cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api watch_post_write && build/check/tests/lynx-test-api watch_controls` — passed after implementation. The header target compiled and linked with the public signatures.
- Linux qualification, hardware behavior, and phase-wide review/security gates were not run here; plan 03-09 and the parent workflow own them. No hardware-accuracy claim follows from these checks.

## Task Commits

1. **Task 1:** `ab7f8f2` — register permanent MAPCTL and ROM controls.
2. **Task 2 RED:** `529b292` — define post-write watch behavior and preserve classified RED evidence.
3. **Task 2 GREEN:** `0507cf3` — stop after committed watched CPU writes.

## Files Created/Modified

- `tests/api/test_api.c`, `tests/api/tests.cmake`, `tests/inventory/fast.txt` — public CPU-path controls, grouped negative controls, and permanent exact registrations.
- `docs/claims/skeleton-repeatability.claim`, `docs/memory-startup.md` — updated inventory binding and sourced decoder/provisional target policy.
- `include/lynxplosion.h`, `core/internal.h`, `core/machine.c`, `tests/header/header.c` — public watch contract, separate host observation state, post-write stop, reset/load clearing, and ABI checks.
- `03-05-T2-RED.json` — classified, path-normalized TDD RED evidence.

## Decisions Made

- Preserve the Phase 3 boundary: this plan proves the decoder and explicit refusal policy, not device behavior, ROM contents, timing accuracy, firmware, or hardware compatibility.
- Do not change snapshot layout or test restore behavior here; watch restore clearing and guest-digest continuation remain with 03-06.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed a duplicate raw-view test local after the first build caught it**
- **Found during:** Task 1
- **Issue:** The new raw-versus-mapped assertion reused a local name that the existing test declared later in the same function, preventing compilation.
- **Fix:** Removed the redundant older read; the new assertion already covers that raw byte.
- **Files modified:** `tests/api/test_api.c`
- **Verification:** The required configure/build and targeted six-test CTest filter passed.
- **Committed in:** `ab7f8f2`

**Total deviations:** 1 auto-fixed (Rule 1). **Impact:** Build-only test correction; no scope change.

## Issues Encountered

- An initial temporary CTest registration landed in a generated test file that CTest did not scan, producing “No tests were found.” That run was not treated as RED. The registration was moved to the generated root test file; the named test then executed, failed on the intended assertion, and passed the RED evidence classifier.

## Known Stubs

- `include/lynxplosion.h:11` — flat five-master-tick timing remains explicitly provisional and does not establish Lynx timing accuracy. This is already tracked as open entry 1 in `.planning/WINDOWS.md`; this plan does not add a duplicate ledger entry, and the stub does not prevent the plan goal.

## Next Phase Readiness

- 03-06 can build on the public watch API and host-only state layout. It still owns restore clearing, observer/digest isolation controls, and watch/snapshot continuation equivalence.
- No user action is needed for this plan. Phase-wide review, security, Linux qualification, and final verification remain with the parent workflow and plan 03-09.

## Self-Check: PASSED

All 11 task/evidence files exist, and task commits `ab7f8f2`, `529b292`, and `0507cf3` are ancestors of the current HEAD.

---
*Phase: 03-memory-map-direct-load-startup-and-first-diagnostic*
*Completed: 2026-11-10*
