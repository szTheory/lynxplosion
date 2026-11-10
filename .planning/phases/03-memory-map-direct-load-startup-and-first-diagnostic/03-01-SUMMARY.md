---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "01"
subsystem: startup
tags: [bll, snapshot, mapctl, guest-identity]
requires: [phase-02-instance-api-and-cpu]
provides: [strict-bs93-direct-load, bll-startup-identity, snapshot-format-3-identity]
affects: [phase-03-test-inventory, phase-03-memory-decoder, phase-03-qualification]
tech-stack:
  added: []
  patterns: [validate-before-mutation, explicit-little-endian-snapshot-layout]
key-files:
  created:
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-01-T1-RED.json
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-01-T2-RED.json
  modified:
    - include/lynxplosion.h
    - core/internal.h
    - core/machine.c
    - core/snapshot.c
    - tests/api/test_api.c
    - tests/header/header.c
key-decisions:
  - "Accept only the exact 80 08 / big-endian address and total length / BS93 header with an exact object length and in-range payload."
  - "Hash the whole accepted BLL object; expose payload range separately from container and startup identity."
  - "Keep MAPCTL as an instance-owned provisional byte latch; retain written bits and do not claim mapped ROM or device behavior."
  - "Use snapshot format 3 with explicit little-endian identity fields and MAPCTL; refuse formats 1 and 2."
metrics:
  duration: 00:25:29
  completed: 2026-11-10
  tasks: 2
  files: 8
  commits: 4
  status: complete
commits: 4
plan_head_before: 76780fc2eba5379d268d60d894c65012a13f3188
plan_head_after: 613ea9dc29de703ca2b318d137ae6e7e955bf251
actuals:
  tokens: 5406
  tasks: 2
  commits: 4
---

# Phase 3 Plan 1: Strict BLL tracer and versioned guest identity Summary

Implemented strict BS93 direct loading and snapshot format 3 so the real guest path can prove load identity, a CPU write to MAPCTL, and a separate raw RAM result.

## Accomplishments

- Added `lynx_load_bll` with exact header and size validation, alias and range preflight before mutation, complete-object SHA-256 identity, copied payload ownership, and entry PC at the load address.
- Added public content/startup identity fields and an instance-owned MAPCTL byte. Init, reset, and successful load leave it at zero; the tracer guest writes `0x0C` through the CPU path and writes `0xA5` to raw RAM at `0x0400`.
- Bumped snapshots to format 3. The explicit little-endian header carries content format, startup tier, entry address, object and payload lengths, and payload hash; the snapshot payload stores MAPCTL. Restore validates identity and other refusal conditions before apply. Formats 1 and 2 remain unsupported.
- Added direct API cases and header layout assertions. RED evidence is persisted in the two TDD JSON records above.

## Verification

Task 1 exact check, including the direct tracer case:

```sh
cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api bll_tracer
```

Passed. The guest ran through the public load/advance/read path, changed MAPCTL itself, and stored `0xA5` at `0x0400`. The independently calculated complete-object digest matched `50ddb4511395952841333f53031b264adde66d40953df60a2a7c4bcdaf52d279`. Malformed, truncated, trailing, length-mismatch, wrapped-payload, and alias refusal controls passed; alias refusal preserved the running instance byte-for-byte. The direct test also checked load ownership and MAPCTL reset behavior.

Task 2 exact check:

```sh
cmake --preset check && cmake --build --preset check && build/check/tests/lynx-test-api snapshot_roundtrip && build/check/tests/lynx-test-api snapshot_refusal && build/check/tests/lynx-test-api bll_identity
```

Passed. Snapshot round-trip and refusal cases exited successfully. `bll_identity` verified identity and MAPCTL restoration at `0x0C`, then verified that all eight latch bits survive save/restore at `0xFF`. Snapshot identity mutations, reserved bytes, and rehashed invalid CPU/clock controls retained their refusal behavior.

The existing focused regression set also passed:

```sh
ctest --preset check -R '^(api\.(load_refusal_unchanged|identity|snapshot_roundtrip|snapshot_refusal|cpu_program))$' --output-on-failure
```

All five selected tests passed. The new direct cases run now; test inventory registration is scheduled by plan 03-02. Full local/Linux and final-head CI qualification is scheduled by plan 03-09 and was not run here.

## TDD Evidence

- Task 1 RED: direct `bll_tracer` failed at the intended new API seam, which returned `LYNX_ERR_STATE`. The persisted record passed `gsd_run check tdd-red-evidence` with `RED_EVIDENCE_OK`. The production tracer and full task command passed afterward.
- Task 2 RED: direct `bll_identity` failed because snapshot format 2 lacked the new identity fields and MAPCTL state. Its persisted record passed `gsd_run check tdd-red-evidence` with `RED_EVIDENCE_OK`. The production snapshot implementation and full task command passed afterward.

## Decisions Made

- The loader accepts only the exact 10-byte BS93 header form required by this plan. It rejects non-exact object sizes, lengths outside `11..65535`, invalid payload ranges, and overlap with the complete instance before changing machine state.
- The whole object, including its header, is hashed. The owned payload address and length remain distinct from container length, format, startup tier, and entry address.
- MAPCTL is currently only an instance-owned byte latch: all written bits are preserved, but hardware reserved-bit semantics and mapped ROM/device accesses remain unqualified until later decoder work.
- Snapshot v3 explicitly records the added identity fields and MAPCTL. Legacy format 1/2 snapshots are refused.

## Requirements Coverage and Limits

This plan is tagged to MEM-01, MEM-02, MEM-04, and DET-03 and implements the stated BLL/startup/snapshot slice. It does not complete the global requirements: the full 64 KB Suzy/Mikey/ROM/vector decoder remains for later Phase 3 work, and the complete uninterrupted-versus-restored artifact-digest criterion for DET-03 is not established by this plan's checks. Firmware remains absent from the implemented path; this plan makes no broader qualification or compatibility claim. Those requirements are therefore not marked globally complete here.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected the payload-wrap negative control**

- **Found during:** Task 1
- **Issue:** The first candidate load address, `0xFFF0`, left 16 bytes available, so the 13-byte test object did not actually overflow.
- **Fix:** Moved the test address to `0xFFF8`, where the payload exceeds the remaining address space and exercises the intended rejection.
- **Files modified:** `tests/api/test_api.c`
- **Commit:** `244a233`

## Known Stubs

- `include/lynxplosion.h:11` — Flat five-master-tick timing remains provisional; this plan proves deterministic bounded guest execution without hardware timing accuracy. Tracked in `.planning/WINDOWS.md`.

## Commits

- `a142fc0` — `test(03-01): add failing strict BLL tracer case`
- `244a233` — `feat(03-01): implement strict BS93 BLL tracer path`
- `129513e` — `test(03-01): add failing snapshot identity case`
- `613ea9d` — `feat(03-01): serialize BLL identity and MAPCTL in snapshots`

## Self-Check: PASSED

- Verified after writing: both TDD evidence records and this summary exist.
- Verified after writing: all four task commits are ancestors of `plan_head_after` (`613ea9dc29de703ca2b318d137ae6e7e955bf251`).
- Verified after writing: the four measured plan commits match the ledger range from `plan_head_before` through `plan_head_after`.
