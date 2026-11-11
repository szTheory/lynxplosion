---
phase: "05"
slug: "display-raw-video-and-input"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-11"
---

# Phase 5 — Validation Strategy

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | C17 CTest with existing ASan/UBSan and CMake/shell controls |
| Config | CMakePresets.json, tests/tests.cmake, tests/inventory/fast.txt |
| Existing quick command | `ctest --preset check --no-tests=error -R '^api.advance_budget$'` |
| Full command | `sh scripts/check.sh` |
| Runtime | Baseline hosted ~109s; focused groups target <60s; new source fault batches measured separately |

## Sampling Rate

Run each task's owning assertions after its last relevant edit, then reuse completed focused evidence. Each wave runs the affected groups; final source gets one full local/macOS, one self-contained Linux and exact-head hosted gate. Do not repeat the unchanged full gate after prose-only edits. Missing CTest matches fail with `--no-tests=error`.

## Per-Task Verification Map

Planner must replace the provisional groups below with exact task IDs, unique threat refs, commands and implementation dependencies before plan approval. No new test is claimed present or passing.

| Task group | Requirements | Secure behavior / intended proof | Test type | Command | Exists | Status |
|---|---|---|---|---|---|---|
| Native tracer | VID-01/04/05, API-06, HB-02, CON-01 slice | Known guest pens, frame ticks and semantic progress through public-only consumer | Integration | Owning registered CTest assigned by plan | Wave 0 | pending |
| Bus / palette | VID-02/03, EVD-05 slice | Bounded grants, real DMA-cost fault, line palette boundaries | C API + semantic mutant | Owning registered CTest assigned by plan | Wave 0 | pending |
| Input / replay | INP-01, HB-02 | Exact bit/handedness/sample edges; malformed timeline refuses before output | API + consumer | Owning registered CTest assigned by plan | Wave 0 | pending |
| Continuation / buffers | API-06, VID-04 | All-span preflight/refusal, reset/load, format5 hostile restore, split/watch equality | API | Owning registered CTest assigned by plan | Wave 0 | pending |
| Public suites | VID-02, EVD-02 | Pinned untouched sources; actual model/count/completion; precise page-mode blocker if skipped; unchanged timer comparison | Integration/control | Owning registered CTest assigned by plan | Wave 0 | pending |

## Wave 0 Requirements

- Planner assigns exact fixture/build/test paths and no-missing-reference dependencies.
- No new test framework or dependency is needed.
- Preserve existing inventory, JUnit, rights, source identity and cleanup controls.

## Manual-Only Verifications

None for scoped software behavior. Physical calibration, baseline adoption, licence/rights and shared LibFuturo changes remain their explicit external owner boundaries; they are not sign-offs for repeatability.

## Validation Sign-Off

- [ ] Every actual task maps to an automated command and implemented dependency.
- [ ] No three consecutive tasks lack automated feedback.
- [ ] Missing test references are covered by predecessor work.
- [ ] No watch-mode/background orphan process.
- [ ] Actual feedback times recorded and broad verification reused appropriately.
- [ ] nyquist_compliant true only after final coverage audit.

Approval: draft; independent plan checker owns planning acceptance.
