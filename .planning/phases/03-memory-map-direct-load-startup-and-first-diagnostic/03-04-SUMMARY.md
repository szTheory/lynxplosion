---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "04"
subsystem: memory
tags: [C17, MAPCTL, cpu-bus, inspection, deterministic-tests]
requires:
  - phase: 03-03
    provides: strict BLL identity and private flat-vector test identity
provides:
  - Pure decoder for all MAPCTL windows and fixed FFF8/FFF9 routes
  - Explicit unavailable stops for unsupported mapped CPU requests
  - Pure whole-request mapped inspection and host MAPCTL latch observation
affects: [03-05, memory-map tests, CPU-vector test identity]
actuals:
  tokens: 9878
  tasks: 2
  commits: 6
plan_head_before: e2248c08b7ad533d3f1bc0c6050dc0b300fad05e
plan_head_after: 79f5468493f6edb259d3c4583b7578705ff93924
tech-stack:
  added: []
  patterns: [single pure route decoder, preflight before CPU request or output mutation]
key-files:
  created:
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-04-T1-RED.json
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-04-T2-RAW-RED.json
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-04-T2-PEEK-RED.json
  modified:
    - core/bus.c
    - core/bus.h
    - core/machine.c
    - include/lynxplosion.h
    - tests/api/test_api.c
    - tests/header/header.c
    - tests/golden/skeleton.digest
key-decisions:
  - "Use FFF8 as fixed RAM and FFF9 as the full-byte MAPCTL latch; only guest writes to FFF9 are implemented."
  - "Unsupported mapped device, ROM, vector, and MAPCTL-read routes refuse explicitly; host register observation reports the latch without guest readback."
  - "Retain flat5 timing as an explicitly unqualified placeholder, independent of route decoding."
patterns-established:
  - "CPU execution and inspection use the same pure per-instance MAPCTL decoder."
  - "Mapped inspection preflights every byte and buffer ownership before copying any byte."
requirements-completed: []
coverage:
  - id: D1
    description: All 16 MAPCTL combinations route boundaries and stop on unavailable CPU accesses without completing the request.
    verification:
      - kind: unit
        ref: "build/check/tests/lynx-test-api map_matrix"
        status: pass
    human_judgment: false
  - id: D2
    description: Raw and mapped reads preserve instance and output state on refusal; mapped reads refuse whole ranges that cross into unavailable routes.
    verification:
      - kind: unit
        ref: "build/check/tests/lynx-test-api memory_peek_pure; rom_unavailable; inspect_pure"
        status: pass
    human_judgment: false
  - id: D3
    description: The deterministic digest records only the appended public MAPCTL observation after CPU, RAM, and tick equivalence checks.
    verification:
      - kind: unit
        ref: "ctest --preset check -R '^det\\.golden(\\.self_test)?$' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
duration: 36min
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 04: MAPCTL routes and pure mapped inspection Summary

**A single MAPCTL decoder now routes CPU accesses and mapped peeks, with atomic refusal for unsupported ranges and host-only latch observation.**

## Performance

- **Duration:** 36 minutes
- **Started:** 2026-11-10T18:19:27Z
- **Completed:** 2026-11-10T18:55:51Z
- **Tasks:** 2
- **Files modified or added:** 10

## Accomplishments

- Added one pure decoder for all 16 low-nibble MAPCTL combinations, preserving independent overlays and special handling for FFF8 RAM and FFF9 MAPCTL. CPU reads and writes to unsupported mapped routes stop before request completion; the flat-vector test identity continues to use its private flat bus.
- Added `lynx_read_cpu_memory` as a read-only two-pass operation. It validates the whole address range, route availability, and output ownership before copying. Raw inspection remains a separate view of backing RAM, and outputs overlapping any part of the machine are refused by both readers.
- Exposed the full guest-written MAPCTL byte through `LYNX_REG_MAPCTL`, including bit 7 and snapshot round trips, while mapped FFF9 readback stays unavailable. The public header documents refusal and continuation behavior.

## Verification

- `cmake --preset check && cmake --build --preset check` — passed; the final build was already up to date.
- `build/check/tests/lynx-test-api map_matrix` — passed all routes and boundaries.
- `build/check/tests/lynx-test-api memory_peek_pure`, `rom_unavailable`, and `inspect_pure` — all passed.
- `ctest --preset check -R '^det\.golden(\.self_test)?$' --no-tests=error --output-on-failure` — both tests passed.
- Focused controls `cpu.vectors.reader_malformed`, `cpu.vectors.none`, `cpu.vectors.wrong_profile`, and `api.seam_agreement.none` passed alongside both golden tests (6/6 CTest tests).
- Three recorded TDD RED runs failed at the intended assertions: the old route completed the FC00 access under a budget stop; raw read accepted an alias and changed machine bytes; the mapped-peek refusal seam rejected a valid RAM read. Each run was classified by `gsd_run check tdd-red-evidence`; normalized evidence is in the three JSON files above.
- Golden changed from `506f79956f05bbb58bb2c6e759046836e5b40de0f499bb6af52bcf288e2bad3c` to `b8bc5a908a4c00be6bb1fc0d5fc8093740b6323ac5fce0cda683ce4e1bad233f`. An independent baseline/current run used the same `churn-loop.hex`, profile `none`, seed `0123456789abcdef`, and 4096-tick budget: `obs.ram.hex` was byte-identical (133,120 bytes; SHA-256 `1da030b1ab28a9cc2af360b12dcb097f5af2385cf52546883a83b1b8195c8b30`), `obs.time.hex` was byte-identical (guest ticks and consumed ticks both 4096; stop `BUDGET`), and the original eight public register values matched. Only the appended `MAPCTL=0` register changed the digest input.
- Full downloaded CPU vectors and cross-compiler qualification were not run here; plan 03-09 owns exact-final-head qualification. No hardware-accuracy claim follows from these same-build comparisons.

## Task Commits

1. **Task 1 RED:** `c6ca74b` — test all MAPCTL routes and stop semantics.
2. **Task 1 GREEN:** `8bdef87` — decode CPU MAPCTL routes.
3. **Task 2 raw ownership RED:** `27bfee6` — prove raw inspection output ownership.
4. **Task 2 raw ownership GREEN:** `4499578` — reject aliased raw inspection outputs.
5. **Task 2 mapped-peek RED:** `fbb24d1` — pin mapped inspection API contract.
6. **Task 2 GREEN:** `79f5468` — add pure mapped inspection and MAPCTL host observation.

## Files Created/Modified

- `core/bus.c`, `core/bus.h` — pure address routing, with timing left separate.
- `core/machine.c` — shared CPU decoder use, explicit unavailable stops, output alias checks, mapped peek, and host MAPCTL read.
- `include/lynxplosion.h` — append-only status/stop/register API and refusal documentation.
- `tests/api/test_api.c`, `tests/header/header.c` — route, purity, boundary, bit-retention, and ABI assertions.
- `tests/golden/skeleton.digest` — updated for the public register observation after CPU/RAM/tick comparison.
- `03-04-T1-RED.json`, `03-04-T2-RAW-RED.json`, `03-04-T2-PEEK-RED.json` — path-normalized semantic RED evidence.

## Decisions Made

- Unsupported mapped devices and ROM/vector routes remain unavailable until their behavior is sourced and implemented; no synthetic registers or backing-RAM fallback are used.
- Guest MAPCTL is write-only under this provisional policy. Hosts can observe the latch through the named register, and raw RAM can still expose the unrelated backing byte at FFF9.
- `MEM-01` and `MEM-04` remain phase-spanning requirements. This plan covers a route and inspection slice, so no global requirement was marked complete.

## Known Stubs

- `include/lynxplosion.h:11` — flat five-tick timing remains an explicit placeholder and does not establish Lynx timing accuracy. This is the existing open Phase 3 stub in `.planning/WINDOWS.md` (entry 1); the plan intentionally preserves it.

## Deviations from Plan

None. The temporary explicit-refusal implementation used for the mapped-peek RED test was replaced by the planned two-pass implementation before completion.

## Issues Encountered

- The expected golden digest changed when `LYNX_REG_MAPCTL` became part of the public register snapshot. The independent CPU/RAM/tick evidence above isolated the change to that appended value.

## Next Phase Readiness

The route decoder and public inspection API are available for plan 03-05 to register permanent map controls and watch behavior. Device contents, guest MAPCTL readback, ROM mapping data, physical accuracy, and timing qualification remain unresolved.

---
*Phase: 03-memory-map-direct-load-startup-and-first-diagnostic*
*Completed: 2026-11-10*

## Self-Check: PASSED

All ten task files exist, and all six task commits are ancestors of the current HEAD.
