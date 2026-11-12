---
phase: 06-audio-and-playable-checkpoint
plan: "01"
subsystem: audio
tags: [C17, audio, PCM, A/V, snapshots, CTest]

requires:
  - phase: 03-memory-map-direct-load-startup-and-first-diagnostic
    provides: Snapshot serialization and the legacy silence regression path
  - phase: 04-timers-interrupts-and-first-public-suites
    provides: Timer and interrupt scheduling
  - phase: 05-display-raw-video-and-input
    provides: Input timeline and raw video capture
provides:
  - Native caller-owned PCM and event spans through a deterministic A/V advance API
  - Format-6 snapshots and canonical digest coverage for the complete audio state envelope
  - Original input-responsive A/V tracer with raw/PPM video and non-silent stereo WAV evidence
affects: [audio expansion, snapshots, consumer, determinism]

actuals:
  tokens: 35197.5
  tasks: 2
  commits: 2
plan_head_before: 9cde3a4a003ecb883609d43d450ba362a1161f91
plan_head_after: a5661929d1f7e4e58fd797fa69ea921f49822207

tech-stack:
  added: []
  patterns:
    - Fixed guest-tick sample lattice with caller-owned output spans
    - Full mutable device state serialized and included in canonical digest
key-files:
  created:
    - core/audio.c
    - docs/audio.md
    - fixtures/diagnostic/av_probe.s
    - fixtures/diagnostic/av_probe.cfg
    - fixtures/manifests/av-probe.manifest
    - tests/consumer/av_driver.cmake
  modified:
    - core/machine.c
    - core/snapshot.c
    - include/lynxplosion.h
    - consumer/main.c
    - scripts/check-manifests.sh
    - tests/api/test_api.c
    - docs/snapshot-video.md
key-decisions:
  - "Expose one immutable digital audio policy: 40000/1 Hz, stereo signed 16-bit PCM."
  - "Use an exact 400-tick sample lattice with ordinal zero at tick 400; device work precedes sampling and sampling precedes CPU grants."
  - "Allocate the complete four-channel audio state envelope now, while exercising only channel 0 in this tracer."
  - "Keep ambiguous hardware behavior explicit as software policy and record physical unknowns separately."
patterns-established:
  - "Audio output is requested through caller-owned spans; legacy advance calls progress guest audio phase without capture."
  - "Independent consumers compare PPM raster pixels against raw frame data and verify WAV metadata and payload."
  - "A/V fixture provenance is validated separately from the stricter input-probe provenance group."
requirements-completed: [AUD-02, HB-03, CON-01, DET-05]

coverage:
  - id: D1
    description: "A recorded A edge drives changed guest progress, raw video, and non-silent 40 kHz stereo PCM through the public consumer path."
    requirement: CON-01
    verification:
      - kind: integration
        ref: "consumer.av_tracer on Clang and GCC; raw/PPM and WAV artifact assertions"
        status: pass
    human_judgment: false
  - id: D2
    description: "The 400-tick sample lattice, capture capacity refusal, resume behavior, and format-6 audio state round-trip are checked."
    requirement: HB-03
    verification:
      - kind: unit
        ref: "api.snapshot_roundtrip, api.snapshot_refusal, api.snapshot_continue"
        status: pass
    human_judgment: false
  - id: D3
    description: "The public ABI, deterministic digest, and copied timer/video builds consume the new state coherently."
    requirement: DET-05
    verification:
      - kind: unit
        ref: "Task 2 focused CTest selector on Clang and GCC; 19 selected tests"
        status: pass
    human_judgment: false

metrics:
  duration: "not recorded; execution resumed after a checkpoint"
  completed: 2026-11-12
  status: complete
---

# Phase 6 Plan 1: Audio and Playable Checkpoint Summary

**An original A-edge tracer now produces changed raw video and non-silent stereo PCM through public caller-owned spans, with format-6 snapshot and digest coverage.**

## Performance

- **Duration:** Not recorded. The original start timestamp was not preserved across the owner-requested checkpoint.
- **Started:** Not recorded before the checkpoint.
- **Completed:** 2026-11-12T21:14:24Z.
- **Tasks:** 2.
- **Files modified:** 25 owned source, test, fixture, and documentation paths.

## Accomplishments

- Added the public A/V advance path and a source-built homebrew that responds to one recorded A edge. The consumer publishes changed raw and PPM frames, non-silent 40 kHz stereo S16 PCM, a WAV file, and event records.
- Implemented the exact 400-tick sample lattice, ordinal zero at tick 400, half-open target behavior, output-capacity refusal and resume behavior, and legacy advancement without capture.
- Added the complete four-channel audio state envelope to format-6 snapshots and the canonical state digest. ABI assertions, raw readers, golden evidence, and both copied timer/video core lists were updated.
- Kept source observations distinct from software choices and physical unknowns. CTLB/status behavior and the physical output path remain unqualified.
- Added a separate strict A/V provenance group for the new fixture. The input-probe all-or-none fields and 201-byte constraints are unchanged.

## Verification

Task 1 Clang command (exit 0; 4/4 selected tests passed):

    cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_tracer|frame_tracer|snapshot_ppm|silence_wav)$'

Task 2 Clang command (exit 0; 19/19 selected tests passed):

    ctest --preset check --no-tests=error --output-on-failure -R '^(api\.(header_selfcontained|abi_symbols|snapshot_roundtrip|snapshot_refusal|snapshot_continue|timer_snapshot_controls|video_snapshot|video_snapshot_refusal)|cpu\.vectors\.(lynx1|lynx2|none)|diag\.(result_symbol|result_controls)|det\.(golden|golden\.self_test|video_paths|video_sensitivity)|timer\.fault\.done|video\.fault\.dma_cost_zero)$'

The committed Linux clone was checked out at a5661929d1f7e4e58fd797fa69ea921f49822207. GCC 13.3.0 on Linux aarch64 built all configured targets.

Task 2 GNU command (exit 0; 19/19 selected tests passed):

    ctest --preset check --no-tests=error --output-on-failure -R '^(api\.(header_selfcontained|abi_symbols|snapshot_roundtrip|snapshot_refusal|snapshot_continue|timer_snapshot_controls|video_snapshot|video_snapshot_refusal)|cpu\.vectors\.(lynx1|lynx2|none)|diag\.(result_symbol|result_controls)|det\.(golden|golden\.self_test|video_paths|video_sensitivity)|timer\.fault\.done|video\.fault\.dma_cost_zero)$'

Task 1 GNU command (exit 0; 4/4 selected tests passed):

    ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(av_tracer|frame_tracer|snapshot_ppm|silence_wav)$'

The GNU manifest and shell checks also passed:

    sh scripts/check-manifests.sh
    shellcheck scripts/check-manifests.sh

The manifest check reported 12 manifests and 24 fixture files; ShellCheck was version 0.9.0. The tracer includes a malformed A/V provenance control that fails for the missing A/V field without triggering the input-probe group.

Four private artifact-checker corruptions for raster pixels, metadata, silent PCM, and WAV rate each failed at the intended checker stage. These previously verified controls were reused; no raw asset bundles were copied into the repository.

No final full gate or 2 GB vector run was performed; those belong to later qualification.

## Task Commits

1. Task 1: native A/V tracer path — 0b0b169 (feat).
2. Task 2: format-6 readers and copied builds — a566192 (fix).

The plan-specific commit count is measured from 9cde3a4, the HEAD immediately before Task 1’s commit. The earlier process/config commit is excluded.

## Actuals Measurement

The 35,197.5 token proxy is 140,790 characters from git diff 9aaf103..a566192, divided by four. It is not measured API usage. The count covers exactly these 25 changed source/test/documentation paths: CMakeLists.txt; consumer/main.c; core/audio.c, core/internal.h, core/machine.c, core/snapshot.c; include/lynxplosion.h; fixtures/diagnostic/av_probe.s, fixtures/diagnostic/av_probe.cfg, fixtures/manifests/av-probe.manifest; tests/consumer/av_driver.cmake, tests/consumer/check_artifacts.c, tests/consumer/tests.cmake; tests/inventory/fast.txt; docs/audio.md; scripts/check-manifests.sh; tests/api/test_api.c; tests/cpu/test_cpu_vectors.c; tests/diag/test_diag.c; tests/det/test_det.c; tests/timers/fault_controls.cmake; tests/video/fault_controls.cmake; tests/header/header.c; docs/snapshot-video.md; tests/golden/skeleton.digest. Planning metadata, parent-owned STATE.md, and the process/config commit are excluded.

## Files Created/Modified

Created:

- core/audio.c — deterministic audio scheduler, channel state, and integer mixing.
- docs/audio.md — source facts, policy choices, timing, reset, mixing, and unknowns.
- fixtures/diagnostic/av_probe.s and fixtures/diagnostic/av_probe.cfg — original input-responsive A/V guest.
- fixtures/manifests/av-probe.manifest — source, config, recipe, guest identity, and rights record.
- tests/consumer/av_driver.cmake — pinned build, replay, artifact, and malformed-manifest controls.

Modified:

- CMakeLists.txt, consumer/main.c, core/internal.h, core/machine.c, core/snapshot.c, include/lynxplosion.h, and scripts/check-manifests.sh.
- tests/consumer/check_artifacts.c, tests/consumer/tests.cmake, and tests/inventory/fast.txt.
- tests/api/test_api.c, tests/cpu/test_cpu_vectors.c, tests/diag/test_diag.c, tests/det/test_det.c, tests/header/header.c, tests/timers/fault_controls.cmake, and tests/video/fault_controls.cmake.
- docs/snapshot-video.md and tests/golden/skeleton.digest.

## Decisions Made

- Keep the public audio identity fixed at digital policy 1, 40000/1 Hz, stereo, signed 16-bit; these are not caller options.
- Mix the wide signed channel sum after per-channel integrator saturation, multiplying the routed value by 64 without floating-point arithmetic.
- Reserve complete per-channel, ring-debt, cursor, and same-tick state now, preventing future channel work from forcing another raw layout change.
- Keep output ownership at the caller boundary. No host pointer, file offset, cursor, or clock becomes guest state.
- Keep fixture source/config hashes in the manifest and verify platform-specific assembler/linker bytes against each local successful build record.

## Deviations from Plan

### Auto-fixed Issues

**1. Rule 1 — The PPM checker compared whole-file lengths for equal raster extents.**

- **Found during:** Task 1 tracer verification.
- **Issue:** PPM metadata lengths differed by four bytes even though the images had equal dimensions; whole-file length was not a valid raster equivalence check.
- **Fix:** The checker now parses the PPM raster, compares each pixel with raw pens and palette data, and checks metadata separately with named failure stages.
- **Files:** tests/consumer/check_artifacts.c.
- **Verification:** The tracer passed; four private corruptions failed at their intended semantic stage.
- **Commit:** 0b0b169.

**2. Rule 3 — The existing manifest validator treated A/V pin fields as the input-probe group.**

- **Found during:** Task 1 fixture integration.
- **Issue:** The input-probe group requires eleven specific keys, so the new fixture failed validation and the generic guest provenance keys were rejected.
- **Fix:** Added a separate all-or-none A/V config, guest hash, and positive byte-count group. The input-probe group and its length checks were left unchanged; local assembler/linker hashes are checked against their successful build record rather than committed as platform-specific values.
- **Files:** scripts/check-manifests.sh, fixtures/manifests/av-probe.manifest, tests/consumer/av_driver.cmake, and 06-01-PLAN.md scope metadata.
- **Verification:** Manifest check and ShellCheck passed; the malformed A/V control was rejected at the expected stage on Clang and GCC.
- **Commit:** 0b0b169.

**Total deviations:** 2 automatic fixes and one approved plan-path metadata update. Both fixes were required for correct fixture verification; no new dependencies or unrelated source changes were made.

## Known Stubs

- The existing diagnostic-only consumer and its checker still describe the legacy silence output as a placeholder (consumer/main.c:686,1166; tests/consumer/check_artifacts.c:327,335,339,346,421,1198,1207,1355,2208). This path is intentionally preserved as the silence regression; the new av-replay path carries audio.
- Existing flat5 CPU-model placeholder wording remains outside this audio slice (include/lynxplosion.h:11,88; tests/diag/test_diag.c:1410). It does not block the A/V tracer.

## Issues Encountered

- core/bus.c was audited for opcode peek, availability, access, and mapped inspection routing; the existing paths required no change.
- No authentication gates or external setup were required.

## User Setup Required

None.

## Next Phase Readiness

The first public A/V path and its format-6 consumers are committed and portable across the checked Clang and GCC builds. Later audio work can exercise additional channel and ring behavior without revising the full-state envelope. Hardware accuracy, physical output, remaining channel behaviors, and final phase qualification remain open for their planned scopes.

## Self-Check: PASSED

- Summary and plan files exist.
- Task commits 0b0b169 and a566192 are ancestors of HEAD.

