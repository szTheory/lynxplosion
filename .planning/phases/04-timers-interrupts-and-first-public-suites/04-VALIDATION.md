---
phase: "04"
slug: timers-interrupts-and-first-public-suites
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 4 — Validation Strategy

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing C17 executables, CTest, CMake/POSIX shell controls; no new framework |
| Configuration | CMakePresets.json, tests/tests.cmake, exact tests/inventory/fast.txt and bound claim |
| Build/targeted command | Configure/build check preset, then nonempty CTest selection for the owning new behavior; planner assigns exact registered IDs |
| Full regression/qualification command | `sh scripts/check.sh` (also workflow.test_command and CI command) |
| Current measured baseline | Phase3 source: macOS full89s; Linux full53s; closeout CI single job42s with check37s. Phase4 fixture/build/runtime cost not yet measured. |

## Sampling Rate

After each meaningful task, run its exact direct mode or targeted registered CTests with `--no-tests=error`. After an integration wave or source concern, run affected cross-module regressions. Run the full script at phase integration and after review fixes that change source, then the self-contained Linux and exact-head CI gates. Reuse one full local run for the regression gate and final local qualification. This applies the owner's targeted-during-implementation/full-at-integration efficiency policy rather than duplicating history/rights scans after every small plan. Preserve every distinct negative control.

## Requirement Verification Map (planned)

| Requirement | Required behavior and independent controls | Status |
|---|---|---|
| TIM-01 | Total order, stable tie sequence, target boundaries, no overshoot, slicing, idle jumps and arithmetic/sequence limits | Tests to be registered by owning plan |
| TIM-02 | Shared phase retained across writes; counters/reload/DONE/reset-DONE/borrow; supported cascades; explicit unavailable links; forged snapshot and named faults | Tests to be registered |
| TIM-03 | Pending separate from source enables, INTSET/INTRST, Timer4 UART exception, profile POLL before/at/after edge, I masks acceptance | Tests to be registered |
| TIM-04 | Held posted request unchanged, timers keep progressing, wake without double bus effects; no WAI/STP or fabricated Suzy | Tests to be registered |
| EVD-02 | Exact pin/notice/build identity, strict BLL/link symbols, source-generated 18 CPU/16 meaningful timer expectations, alternatives/counts, independent sentinel/progress/time/model | Tests to be registered |
| EVD-03 | CPU truly prevented from executing the first test instruction; both suites rejected despite zero-like results | Tests to be registered |
| EVD-04 | Guest-executed per-suite model evidence; wrong selected profile rejected; no copied host value | Tests to be registered |
| EVD-05 | Named DONE/reset-DONE/order/IRQ/completion/model/count faults each fail intended assertions with positive controls | Tests to be registered |

## Per-Task Verification Map

The planner must replace this paragraph with one row per actual task, naming plan/wave, requirements, threat reference, expected secure behavior, test type, exact command, existing versus Wave0 file and pending status. Candidate IDs are not evidence until registered in CTest and the exact fast inventory. Every inventory edit updates the claim hash in the same commit.

## Wave 0 Requirements

- [ ] Original source-built suite wrapper/manifest/generator/labels and count controls, with no upstream binary/source import into the public tree.
- [ ] Minimal original guest timer/IRQ tracer plus bank/scheduler/CPU/snapshot targeted tests.
- [ ] Source-oracle/semantic sentinel/no-run/model-mismatch controls under isolated machine state.
- [ ] Ownership/refusal, overflow, same-tick watch prefix and restore equivalence controls.
- [ ] Registration/inventory and claim-hash updates, with explicit named out-of-scope suite entries.

## Manual-Only Verifications

None required for this software phase. Hardware measurements, exact phase/slot calibration, baseline/suite-pass claims, firmware/rights decisions, project licence/release and physical tests remain outside authority. Missing hardware evidence is visible as a limitation, not a manufactured software UAT approval.

## Validation Sign-Off

- [ ] Every task has a meaningful automated check or explicit Wave0 dependency.
- [ ] No three consecutive implementation tasks lack targeted feedback.
- [ ] All registered IDs match the fast inventory and claim hash, with visible skips/missing evidence.
- [ ] Same-tick, split/watch/restore and hostile input/state controls test independent observations.
- [ ] Bounds and costs measured from actual fixtures; no watch/foreground-app mode.
- [ ] Coverage auditor sets validated/nyquist_compliant only after actual evidence.

Approval: implementation/verification is covered by existing phase authority; this draft claims no passing Phase4 check.
