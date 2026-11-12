---
phase: "06"
slug: "audio-and-playable-checkpoint"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-12"
---

# Phase 6 — Validation Strategy

Planning contract only: no audio runtime result is claimed. The planner fills
the exact per-task map from its final plans; execution records actual evidence.

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing C17 CMake/CTest and POSIX shell harnesses |
| Config | CMakeLists.txt, CMakePresets.json, tests/inventory/fast.txt |
| Focused command | `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '<exact planned selector>'` |
| Canonical gate | `sh scripts/check.sh` |
| Runtime | Measure new cases; Phase 5 final hosted CTest was about 87 seconds, not a Phase 6 estimate |

## Sampling Rate

- After each task, run the exact semantic tests named by that task. Missing
  registration or an empty selector is failure, not skipped evidence.
- At structural changes, include existing API/CPU-vector/diagnostic snapshot
  consumers and timer/video copied-core fault builds immediately.
- Run focused GNU GCC/Clang and ShellCheck 0.9/0.11 portability checks early.
- After review and required fixes, run one final canonical gate per required
  environment and hosted CI on exact pushed HEAD. Reuse it as regression
  evidence; do not duplicate broad gates on unchanged source/prose changes.
- Max feedback target for focused cases: 30 seconds where practical; measure
  slow semantic fault cases rather than dropping distinct controls.

## Per-Task Verification Map

Exact task IDs, commands and threat references are assigned with PLAN.md files.
The following are mandatory coverage obligations, all initially pending.

| Requirement | Secure behavior / observation | Test type | Required evidence |
|---|---|---|---|
| AUD-01 | Four channels, reload/LFSR/integrator/volume, complete bounded link ring, every stereo bit | Device + guest integration | Exact values, finite zero-reload ring, channel/link/stereo negative controls |
| AUD-02 | 400-tick S16 stereo lattice, ordered separate events, safe caller spans | API + scheduler + static | Boundary/ordinal and transactional refusal assertions; no forbidden core imports |
| HB-03 | Input changes guest/image/audio response | Public consumer | Non-silent PCM plus semantic input/guest markers and real frame checks |
| DET-05 | Separate aligned streams and stable continuation | Determinism + snapshot | Fresh/sliced/interleaved/watched/restored complete stream suffixes; malformed-state refusals |
| EVD-06 | Five pinned source suites, exact inventory and honest status | Provenance + guest oracle | Both audio suites' 18 rows/profile, model/return/progress/bounds; exact existing skips and timer failures |
| CON-01 | Public-only host produces raw/PPM and WAV | Consumer artifact integration | RIFF fields, sample count/payload, stream identities, exclusive publication controls |
| EVD-05 | Named faults fail intended semantic assertions | Private copied-core controls | Clean brackets, successful mutant build and exact intended failure; no production fault flags |
| PERF-01 | Fixed workload/output baseline | Host benchmark | Five same-build fresh runs, equal stream/output digests, monotonic core timing, median/range |

## Wave 0 Requirements

- [ ] Add the thin A/V tracer's exact test registrations and inventory entries.
- [ ] Build unchanged audio/audio2 source through owned wrappers; validate
  source-derived oracle extents and audio2's existing alternative before guest runs.
- [ ] Add new audio tests and private semantic controls in their owning plans.
- [ ] Update all raw snapshot-layout consumers and explicit copied-core source
  lists with the audio structure change, retaining historical assertions.
- [ ] Fill the final per-task/threat/command map after planning.

Existing test infrastructure is sufficient; no framework installation is needed.

## Manual-Only Verifications

All scoped software behaviors have automated verification. Physical hardware,
subjective listening, release, licence and model-baseline decisions retain their
existing owner boundaries and are not software acceptance tasks in this phase.

## Validation Sign-Off

- [ ] Every task has runnable automated verification or a Wave 0 dependency.
- [ ] No three consecutive tasks lack automated verification.
- [ ] Missing cases are registered before they can count as evidence.
- [ ] No watch-mode or desktop-focus requirement.
- [ ] Feedback latency and slow cases measured.
- [ ] Final validation audit sets status and Nyquist compliance from evidence.

**Approval:** pending execution and independent validation audit.
