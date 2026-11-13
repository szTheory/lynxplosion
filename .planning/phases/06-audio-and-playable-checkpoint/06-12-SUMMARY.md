---
phase: 06-audio-and-playable-checkpoint
plan: "12"
subsystem: verification
duration: "168min"
tags: [audio, qualification, review, security, coverage, CI, efficiency]
requires:
  - phase: 06-11
    provides: Complete A/V checkpoint and fixed-workload baseline
provides:
  - Independently reviewed software checkpoint with bounded repairs
  - Exact-source canonical macOS/GNU and hosted CI qualification
  - Closed scoped security/automation obligations and measured process lessons
affects: [phase6-verification, milestone-audit]
# Changed-diff characters/4 proxy before this summary; not model/API usage.
actuals:
  tokens: 34364
  tasks: 2
  commits: 6
plan_head_before: c3b12f29d4c85d0b6cd8e37e941dd31cf6a14769
plan_head_after: bc943eecc7a498b4405a150961c036e6d911e60e
tech-stack:
  added: []
  patterns: [exact-source evidence reuse, focused repair batches, proof-specific replay work]
key-files:
  created:
    - .planning/phases/06-audio-and-playable-checkpoint/06-QUALIFICATION.md
    - .planning/phases/06-audio-and-playable-checkpoint/06-REVIEW.md
    - .planning/phases/06-audio-and-playable-checkpoint/06-REVIEW-FIX.md
    - .planning/phases/06-audio-and-playable-checkpoint/06-REVIEW-DISPOSITION.md
    - .planning/phases/06-audio-and-playable-checkpoint/06-SECURITY.md
  modified:
    - core/machine.c
    - scripts/check-manifests.sh
    - tests/gates/manifests_controls.sh
    - tests/audio/test_audio.c
    - tests/api/test_api.c
    - tests/det/test_det.c
    - tests/consumer/frame_driver.cmake
    - .planning/phases/06-audio-and-playable-checkpoint/06-VALIDATION.md
    - docs/performance.md
    - .planning/AUTONOMY.md
    - .planning/LESSONS.md
key-decisions:
  - "Preserve every distinct semantic proof while removing repeated sensitivity interleaving."
  - "Run early integration after later shared semantic repairs and artifact preflight in every reused environment."
  - "Keep strict timer disagreement, SERCTL missing observations and owner stops distinct from software qualification."
requirements-completed: [AUD-01, AUD-02, HB-03, DET-05, EVD-06, PERF-01, CON-01, EVD-05]
coverage:
  - id: D1
    requirement: AUD-01
    description: Four-channel and stereo behavior, source observations and named faults
    verification:
      - kind: integration
        ref: "Final canonical audio/source/fault tests pass on native/GNU/CI"
        status: pass
  - id: D2
    requirement: AUD-02
    description: Integer PCM, tick events, bounded ordering and pure core
    verification:
      - kind: integration
        ref: "Final cadence/order/span/import assertions pass"
        status: pass
  - id: D3
    requirement: HB-03
    description: Source-built guest responds audibly and visually to canonical input
    verification:
      - kind: integration
        ref: "Final public A/V replay and independent semantic controls pass"
        status: pass
  - id: D4
    requirement: DET-05
    description: Separate state/input/progress/video/PCM/event continuation and sensitivity
    verification:
      - kind: integration
        ref: "Final deterministic paths/sensitivity/snapshot assertions pass"
        status: pass
  - id: D5
    requirement: EVD-06
    description: Five source-built suite inventories with honest two-SERCTL skip boundary
    verification:
      - kind: integration
        ref: "Final exact selection/JUnit/source observation and controls pass"
        status: pass
  - id: D6
    requirement: PERF-01
    description: Five equal digest-bound workloads with separate host timings
    verification:
      - kind: integration
        ref: "Final benchmark and parser/no-run controls pass; docs/performance.md"
        status: pass
  - id: D7
    requirement: CON-01
    description: Public-only consumer with complete frames/WAV and exclusive publication
    verification:
      - kind: integration
        ref: "Final bundle semantics/refusal/race/fault controls pass"
        status: pass
  - id: D8
    requirement: EVD-05
    description: Actual semantic mutant evidence across required device slices
    verification:
      - kind: integration
        ref: "Final audio/timer/video/input named mutants fail intended assertions with clean brackets"
        status: pass
residuals:
  human_judgment: false
  notes: "No human-only software UAT. EVD-02, SERCTL observations and owner/hardware/rights/release limits remain explicit; independent goal verification is next."
---

# Phase6 Plan12 — Reviewed software qualification

The public A/V software checkpoint is implemented and qualified at the exact
source above. Independent phase-goal verification is the next gate; this summary
does not mark the milestone complete or qualify physical hardware.

## Task1: review and bounded repairs

Independent code review covers54 paths and is clean after four iterations.
Security declares25 ASVS1 mitigations, all closed with no risk acceptance.
Assertion-level coverage has no remaining scoped automation gap. Three explicit
prohibition judgments preserve hardware/rights/authority limits.

The repair batch fixed natural audio-root eligibility after a successful CPU
grant and static fixture symlink containment, and added active A/V lifecycle
coverage. Old-source controls demonstrated the two code defects; lifecycle was
an assertion gap, not an invented RED. Later integration repaired two files of
stale timer/output sampling assertions without changing production or source
oracles. The matrix repair removes sensitivity's repeated two-instance replay;
baseline interleaving, full inputs/profiles/handedness and every independent
frame assertion remain. Exact commits, old failures and focused results are in
06-REVIEW-FIX.md; independent dispositions are in06-REVIEW.md.

The exact planned26-case group passed before the later integration repairs;
all its registrations pass in the final canonical inventories. Focused native/
GNU repair and portability evidence was reused, not replaced by report claims.

## Task2: exact-source canonical gates and CI

Native macOS and the existing exact-commit GNU clone both passed
`sh scripts/check.sh`. [Hosted source CI](https://github.com/szTheory/lynxplosion/actions/runs/43448772929)
passed for the exact pushed source, not a branch-latest substitute. Each gate
accounts for246 rows once:244 pass, two declared SERCTL skips, no failure;
JUnit, artifacts and full history pass. 06-QUALIFICATION.md records identities,
raw evidence locations, requirement/threat links and CI timings.

Native total531s (CTest306.63s/history201s); GNU total313s
(CTest278.65s/history26s); hosted canonical236s (CTest151.50s/history43s).
Hosted queue-to-job3s, setup1s, checkout2s, cc65 build21s and configure/build
about7–8s are distinct. The one CI job has no duplicate broad job or matrix.
No measured need for another cache, dependency, concurrency setting or lane.

## Failures and course correction

The first native final attempt exposed stale sampling assertions; its real exit8
is preserved. The first GNU attempt passed tests then failed artifact scan on
old suite outputs; both owned roots were privately preserved with verified
full inventories, and the unchanged scanner passed. Missing that GNU preflight
wasted a363.46s test run. The next invocation hit the old frame replay240s limit
after a234.34s pass. Its partly cleaned root and actual status remain preserved;
the exact contribution of execution/cleanup/environmental variation is unknown.

The independently reviewed matrix change passed focused native/GNU in
14.09s/167.93s, final canonical GNU138.69s and hosted12.97s. All12 input events,
23 frames and four profile/handedness combinations remain. These are observed
harness costs, not controlled emulator speedups. No timeout, source oracle,
production performance policy or scanner exemption was changed.

AUTONOMY, LESSONS and06-EFFICIENCY-REVIEW now require late semantic integration,
per-environment artifact preflight and distinct-proof replay work. Warm reuse,
first-complete-path sequencing and batched repairs remain applied. Raw logs/status
survive in private storage; historical failed evidence is never relabeled green.

## Preserved limits and next action

Strict timer row14 remains legacy0C/source0D and video0F/source0D: global EVD-02
is still open. Only the two page-mode SERCTL observations skip; no source/model
pass is claimed for them. Repeatability is not hardware accuracy. O1/O5/O6/O9,
firmware/commercial rights, physical tests, release/new-channel and shared
LibFuturo stops remain unchanged. The historical benchmark is retained separately
from the final-source capture and CI costs.

Parent updates routine tracking, then the independent verifier checks phase goal
achievement and the milestone audit reconciles remaining global requirements.
No user action is needed for those remaining authorized checks.
