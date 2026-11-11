---
phase: 03-memory-map-direct-load-startup-and-first-diagnostic
plan: "09"
subsystem: qualification
tags: [C17, CTest, BS93, atomic-publication, P3, WAV]
requires:
  - phase: 03-08
    provides: Independent public-API diagnostic consumer and artifact validator
provides:
  - Exclusive whole-directory consumer publication with deterministic interruption controls
  - Executable post-check demo and qualified macOS/Linux delivery evidence
  - Exact inventory, review, security and coverage closure for phase-goal verification
affects: [phase-04, consumer, verification]
actuals:
  tokens: 14532
  tasks: 2
  commits: 7
plan_head_before: a59e962
plan_head_after: 77218faf00be702c47f13b546763edd7c7d05510
tech-stack:
  added: []
  patterns:
    - Private sibling staging followed by atomic no-replace directory publication
    - Test-only pipe barriers and exact-child interruption controls
key-files:
  created:
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-09-RED.json
    - .planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-09-CHECKPOINT.md
  modified:
    - consumer/main.c
    - tests/consumer/check_artifacts.c
    - tests/consumer/tests.cmake
    - tests/diag/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - README.md
    - docs/memory-startup.md
    - docs/provenance.md
key-decisions:
  - Fail closed when exclusive publication is unsupported; never replace an existing destination.
  - Private staging leftovers are not published success; interruption controls distinguish pre-manifest and pre-publish states.
  - Keep diagnostic colors and silence explicitly provisional; no hardware, rights or licence decision follows from green checks.
requirements-completed: ["HB-01", "CON-01", "EVD-01", "MEM-03", "DET-03"]
coverage:
  - id: atomic-publication
    description: Existing paths survive refusal and interruption cannot publish partial success.
    requirement: CON-01
    verification:
      - kind: integration
        ref: tests/consumer/check_artifacts.c#consumer.refusal_controls
        status: pass
    human_judgment: false
  - id: diagnostic-artifacts
    description: Independent consumer emits the labelled exact snapshot and valid placeholder silence WAV.
    requirement: HB-01
    verification:
      - kind: integration
        ref: consumer.snapshot_ppm,consumer.silence_wav,consumer.public_api
        status: pass
    human_judgment: false
  - id: qualified-delivery
    description: Documented demo, local regression gate, isolated Linux and source-head CI pass.
    verification:
      - kind: integration
        ref: sh scripts/check.sh; GitHub Actions run 42930686204
        status: pass
    human_judgment: false
duration: 128min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 09: Exclusive Artifacts and Delivery Qualification

The public consumer publishes a complete validated snapshot/silence directory exclusively, and the reviewed source passes the local, isolated Linux and exact-source-head CI gates.

## Accomplishments

- Validated inputs precede staging. Exclusive writes and checked closes create a unique sibling stage; the manifest is last, followed by macOS/Linux atomic no-replace publication. Existing directories, files and symlinks are preserved. Unsupported platforms fail closed.
- The four exact consumer tests cover independent artifact parsing, public-only compilation, refusal, competing publishers and injected write/close failures. Review added deterministic barriers before manifest staging and before publication: each child is killed/reaped, the final path stays absent, expected private-stage contents and a sibling marker are checked. A premature-publication mutant fails the intended assertion; restored code passes.
- README and memory/startup documentation rebuild the fixture after check cleanup and describe startup A, MAPCTL/unavailable policy, raw/mapped observations, post-write watch ticks, snapshot v3 identity, diagnostic palette and placeholder silence. The demo and independent validator pass.

## Commits and Adjustments

Task 1 RED `441605b`, GREEN `d851094`, GCC probe correction `1d11450`; task 2 docs/checkpoint `64ec67e`. Parent review records are `4a33e76`; interruption fix `61d79af`; gate/coverage records and explicit CMake regression command `77218fa`. All are ancestors of this summary.

The GCC public-symbol probe needed `-Werror=implicit-function-declaration`. Minimal Ubuntu needed CA certificates. A shared clone's host-only object alternate failed history scanning; a self-contained clone resolved it. No project dependency, full-data CPU-vector rerun, cache or concurrency change was added.

## Verification

Qualified source: `77218faf00be702c47f13b546763edd7c7d05510`. macOS arm64 Apple clang 21.0.0.21000101 passed the configured canonical regression command `sh scripts/check.sh` through `run-with-timeout 600`: exit 0, all 120 tests accounted for, no skips; CTest 32.29 s, total 89 s. Isolated Ubuntu 24.04 arm64/GCC 13.3 passed the same full command: exit 0, same inventory/no skips; CTest 17.48 s, total 53 s. History, rights/artifacts and claim gates passed.

Both compilers retain golden digest `b8bc5a908a4c00be6bb1fc0d5fc8093740b6323ac5fce0cda683ce4e1bad233f` and diagnostic BLL SHA `16ecb83e09554082f1d51212bbe7c31fd67d33ba68873bca3441308eec284ba0`. Inventory/claim SHA is `1e5eee703b7d5a8009c1cb05fd20625b228418d1eedbd92f18c53f6dc1625654`. The rebuilt demo validates at tick 5000, startup A/profile none, exact 8x8 P3 and 800 zero PCM samples.

[Source-head CI run](https://github.com/szTheory/lynxplosion/actions/runs/42930686204) passed its single required check on that exact revision. Run wall 69 s, queue about 2 s, checkout 3 s, check 60 s; cc65 11 s, CMake workflow 24 s, artifact scan 1 s, history 19 s. No duplicated jobs justified a change. Non-failing `tr` broken-pipe scan diagnostics are a follow-up, not suppressed evidence. The docs-only successor is separately checked before parent verification; its result is external evidence rather than a self-referential commit field.

## Reviews and Limits

The full 46-file code review and four-file fix re-review are complete with no remaining finding; disposition is recorded. Coverage maps all 18 tasks and nine phase requirements. All 20 security rows are closed without inferred risk acceptance; the output audit has no required fix. Detailed evidence is in the adjacent REVIEW, REVIEW-FIX, REVIEW-DISPOSITION, SECURITY, VALIDATION and UI-REVIEW artifacts.

No firmware/game content, physical testing, baseline choice, licence, clean-room approval, canonical video form, release or new publication authority was added. Silence is intentional pending Phase 6; the P3 is a diagnostic RAM snapshot, not a hardware frame. The inherited 255-tick snapshot ready-span weakness and scan-log noise remain explicit follow-ups. Global requirements and Phase 3 completion await the parent goal verifier.

## Self-Check: PASSED

Both tasks have committed implementation and actual passing qualification evidence. Named task/fix commits and all declared source files exist; the exact consumer inventory and claim remain aligned.
