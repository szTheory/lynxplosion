---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "08"
subsystem: diagnostic
tags: [BS93, C, BLL, public-api, P3, WAV, CTest]
requires:
  - phase: 03-07
    provides: Source-built diagnostic fixture, strict linked symbols, public observations, and isolated fixture builder
provides:
  - Runner bundle agreement with independent result, identity, timing, and pixel checks
  - Independent public-only consumer for a labeled 8x8 diagnostic snapshot and placeholder WAV
  - Independent bounded artifact validator with mutation self-tests
affects: [03-09, diagnostic, memory-map, startup, snapshot, evidence]
actuals:
  tokens: 21855
  tasks: 2
  commits: 2
commits: 2
plan_head_before: 91e60f4b634388c5b9495531fd5a0908be104555
plan_head_after: bdcc0cb74bc729cc0039b8e929cabf15db2c0dc2
tech-stack:
  added: []
  patterns:
    - Independent consumers strictly parse actual linked -Ln addresses before public RAM reads.
    - Diagnostic output is generated only after full guest and result validation, with bounded P3/WAV encoding.
key-files:
  created:
    - consumer/main.c
    - tests/consumer/check_artifacts.c
    - tests/consumer/tests.cmake
  modified:
    - CMakeLists.txt
    - docs/policy/exposure-record.md
    - docs/provenance.md
    - tests/diag/test_diag.c
    - tests/diag/tests.cmake
    - tests/tests.cmake
decisions:
  - Keep the existing six diagnostic inventory IDs, including diag.result_controls; plan 03-09 owns consumer registration and final publication hardening.
  - Link lynx-consume only to lynx_core and its public include surface; independently parse linked symbols and expected guest results.
  - Label the 8x8 output SNAPSHOT with diagnostic-palette-v1, and label the exact 800-sample WAV as placeholder silence pending Phase 6.
metrics:
  duration: 34m
  completed: 2026-11-10
status: complete
---

# Phase 3 Plan 08: Runner Agreement and Public Diagnostic Consumer Summary

The runner now agrees with an independent diagnostic result oracle, and a separate public-API C consumer writes and validates a bounded, explicitly labeled diagnostic snapshot and placeholder silence WAV.

## Completed Work

- Extended the existing runner diagnostic bundle path to verify the full accepted object SHA, startup=A, guest tick observations, result block, and independent packed-pixel constants. Wrong-map and broken-result objects fail through the same path. The existing `diag.result_controls` inventory ID remains unchanged; no additional ID was added. Existing CPU fixture gates remain intact.
- Added a public-only `lynx-consume` target. It uses standard/POSIX headers and `lynxplosion.h`, strictly parses linked symbols and validates their extents before reading RAM through the public API. It has no core/internal or private crypto includes and does not share the runner or harness parser as an oracle.
- Successful output is an unscaled 8x8 P3 SNAPSHOT plus companion metadata. Both include the actual guest tick, startup=A, selected profile, full accepted object SHA, and `diagnostic-palette-v1`. The comments state that the pixels are diagnostic-owned and make no canonical Lynx video claim.
- The WAV is mono 8000 Hz, signed 16-bit little-endian PCM with exactly 800 zero samples. Output and metadata sizes are bounded, WAV chunk arithmetic is checked, CLI errors are complete, and an existing output directory is refused without altering its files. Atomic publication and consumer inventory registration remain with plan 03-09.
- Added an independent artifact validator self-test. It accepts the valid P3/WAV/metadata fixture and rejects altered pixels, WAV chunk size, and missing metadata. Direct fixture-driver runs passed for `public_api`, `snapshot_ppm`, `silence_wav`, and `refusal_controls`; refusal cases covered missing inputs/symbols, malformed BLL, guest errors, incomplete completion, bad pixels, and existing-output preservation.
- Updated provenance and the exposure record. The cc65 header inspection is recorded as format evidence; fixture rights remain all-rights-reserved, with no license decision or redistribution permission. O6 remains draft.

## Context Arbitration

The diagnostic initializes MAPCTL to map 0, then guest code writes `$0C` through `$FFF9`; `$0C` is a guest register value, not a host load address. Agreement uses the full object hash and startup=A. Existing post-write watch/restore behavior remains a host observation. The consumer only accepts a strict BLL object; no public raw-image loading path was introduced.

The P3 image is a diagnostic snapshot, not a canonical frame, DMA result, hardware-accuracy claim, or video implementation. The WAV is explicitly placeholder silence pending Phase 6. No firmware, game content, emulator implementation source, physical hardware, license, baseline, release, or publication decision was added by this plan.

## Deviations from Plan

**1. [Rule 1 - Test setup correction] Corrected the wrong-map fixture mutation.**
- **Found during:** Task 1
- **Issue:** The initial negative control selected bytes that did not mutate the guest's map selection, so it did not exercise the intended wrong-map failure.
- **Fix:** Changed the control to derive a one-byte mutation from the validated source fixture and confirmed the runner path rejects it.
- **Files modified:** `tests/diag/test_diag.c`, `tests/diag/tests.cmake`
- **Commit:** `27a4c09`

## Known Stubs

| File | Line | Intentional stub | Follow-up |
|---|---:|---|---|
| `consumer/main.c` | 464 | Phase 3 WAV output is intentional 800-sample silence pending Phase 6 audio. | Replace only when Phase 6 audio output is implemented and validated. Recorded in `.planning/WINDOWS.md`. |

## Verification

- Task 1 configure/build and targeted CTest command passed: `cmake --preset check && cmake --build --preset check && ctest --preset check -R '^(fixture\\.bll_header|diag\\.result_controls|fixtures\\.cpu_asm|policy\\.)' --no-tests=error --output-on-failure` — all 13 selected tests passed.
- Task 2 configure and targeted build passed for `lynx-consume`, `lynx-check-artifacts`, and `lynx-test-diag`; `build/check/tests/lynx-check-artifacts self-test` accepted the contract and rejected all three mutation controls.
- Direct consumer fixture modes (`public_api`, `snapshot_ppm`, `silence_wav`, `refusal_controls`) passed using isolated, owned scratch/output paths; generated artifacts were cleaned before scanning.
- `git diff --check` and `sh scripts/hygiene.sh --tree` passed. The task did not change the test inventory or its bound claim hash.

## Requirements and Follow-up

This plan contributes evidence toward EVD-01, HB-01, CON-01, MEM-03, and DET-03. No project-wide requirement was marked complete from this partial Phase 3 slice. Plan 03-09 owns the final atomic/exclusive publication hardening, consumer inventory registrations, and full qualification.

## Self-Check: PASSED

- SUMMARY file exists.
- Task commits `27a4c09` and `bdcc0cb` are ancestors of HEAD.
