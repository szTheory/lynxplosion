---
phase: 07-suzy-math-and-register-mirrors
plan: "03"
subsystem: diagnostic-evidence
tags: [suzy, source-oracles, registers, negative-controls]
requires:
  - phase: 07-02
    provides: mapped arithmetic and lifecycle behavior
provides:
  - complete pinned math and memio captures with separate scoped and strict verdicts
  - source, freshness, result, timing and copied-core fault controls
  - four-bit Green palette storage and canonical snapshot validation
affects: [phase-07-qualification, public-suites, video-snapshots]
actuals:
  tokens: 40952
  tasks: 2
  commits: 7
plan_head_before: 1871417c379198949081d3a32dca9f94d0d8a2be
plan_head_after: c7a66a0b888b68d2558027a1fdfaafb8300c6f6a
tech-stack:
  added: []
  patterns: [source-derived group metadata, independent emitted-record validation]
key-files:
  created:
    - fixtures/suites/math-wrapper.s
    - fixtures/suites/memio-wrapper.s
    - fixtures/manifests/math-tests.manifest
    - fixtures/manifests/memio-tests.manifest
    - tests/gates/math-script-controls.sh
  modified:
    - core/video.c
    - core/snapshot.c
    - tests/api/test_api.c
    - scripts/build-suites.sh
    - scripts/generate-suite-oracle.sh
    - tests/suites/test_public_suites.c
    - tests/suites/run.cmake
    - tests/suites/tests.cmake
    - tests/suzy/fault_controls.cmake
    - tests/gates/audio-script-controls.sh
    - tests/diag/tests.cmake
    - tests/inventory/fast.txt
    - fixtures/suites/NOTICE
    - docs/claims/skeleton-repeatability.claim
    - docs/public-suites.md
    - docs/suzy-math.md
    - docs/snapshot-video.md
    - docs/video-input.md
    - docs/timer-source-index14-pending.json
key-decisions:
  - "Preserve all 18 bytes, source expectations and independent strict/scoped verdicts; observed agreement does not verify silicon timing."
  - "Repair Green palette storage and snapshot reachability rather than changing pinned memio expectations."
patterns-established:
  - "Run actual faulty-core source captures as well as edited-record validation controls."
requirements-completed: []
coverage:
  - id: D1
    description: Complete pinned source observations on both profiles with exact raw and group counts
    verification:
      - kind: integration
        ref: suite.math.observation.lynx1/lynx2 and suite.memio.observation.lynx1/lynx2
        status: pass
    human_judgment: false
  - id: D2
    description: Source, freshness, result, strict timing and faulty-core evidence rejects its intended corruptions
    verification:
      - kind: integration
        ref: suite.math.controls, suite.memio.controls, suite.math.source_controls, suite.math.strict_comparison_controls
        status: pass
    human_judgment: false
  - id: D3
    description: Palette masking and snapshot canonicality preserve register behavior and atomic refusal
    verification:
      - kind: integration
        ref: api.video_bus, api.video_snapshot_refusal and both memio observations
        status: pass
    human_judgment: false
duration: 138min elapsed handoff through parent closeout; active model time unknown
completed: 2026-11-14
status: complete
---

# Phase 7 Plan 03 Summary

**Pinned math and memio now complete on both profiles with whole raw captures, strict source agreement and independent adversarial evidence.**

## Accomplishments and verification

The existing producer/oracle/consumer was extended rather than duplicated. Each capture retains 18 bytes: math has 14 arithmetic bytes, two timing status/diagnostic bytes and two reserved bytes; memio has 13 register bytes in groups of 4/4/5 and five reserved bytes. All meaningful bytes agree with unchanged pinned expectations on both profiles; strict mismatch count is zero. Math completes at 268846/268831 guest ticks and memio at 69581/69601, within the named one-million-tick bound. These are deterministic source observations, not hardware traces.

The final selection requires 14 profile observations and 12 control IDs. Source-shape/identity, no-run, missing return/progress, prior-output replacement, per-group arithmetic/register corruption and both strict timing offsets are checked. Independently compiled result, FC55 alias-index and completion-publication mutants run the actual math guest to completion and fail the scoped gate between clean captures.

Native Debug focused owning group: 11/11 pass; final saved log reports 56.43s wall. GNU GCC13.3 optimized build at exact source HEAD above: 11/11 pass in 41.42s, clean tracked checkout, explicit `-O2 -g -UNDEBUG`, assertions and non-recovering ASan/UBSan. Native ShellCheck0.11 and GNU0.9 pass. Named determinism/docs checks pass after inventory rebinding; manifests and required native artifact scanner exit0. Private invocation logs and status files are under the phase's agent03 temporary evidence directory. Diff character proxy above is not API token usage.

## Task Commits

1. **Task 1 source observation RED:** `3c5a5d8`.
2. **Task 1 palette register and hostile snapshot REDs:** `be6f3d0`, `db76a21`.
3. **Task 1 source-built observations and necessary device repair:** `5e1625e`.
4. **Task 1 exact-selection follow-up:** `45339b2`, `5f39deb`.
5. **Task 2 source/observation/strict/fault controls:** `c7a66a0`.

## Deviations and issues resolved

The unchanged memio guest exposed Green palette high bits: writing FF returned FF instead of0F. The narrow video storage mask and canonical live/captured-row snapshot checks now enforce the four-bit field. Blue/Red remains eight-bit. A mapped opcode fixture was adapted to the newly enforced field and hostile-restore test isolation repaired. These owning video/snapshot/API/doc paths were explicitly added to the plan's scope.

After this device repair, one native registered inventory ran: 266 accounted, 14 failures, two declared page-mode skips. Failures reduced to copied producer fixtures, script-identity bindings and manifest/inventory expectations. All were repaired and their owning groups passed; the full run was not relabeled green or repeated solely for fixture repairs. Final reviewed canonical qualification belongs to Plan04. The timer descriptor changed only its two script identity hashes; original index14 observed0C/expected0D and proof/source policy remain unchanged and its ten owning tests passed.

Fault setup errors (missing assembler includes, assertion labels and malformed control records) were corrected at their actual stage; infrastructure failures were never counted as semantic mutant detection. Retained owned failures were archived outside build after nonce/provenance, ancestor containment and complete byte/type/mode/link-text inventories. One archival command renamed roots before failing a post-move symlink check; the receipt records that partial execution and exact subsequent inventory agreement. The native scanner then passed without exemptions. An additional optional GNU scanner still reports historical timer-evidence scratch; it was left untouched and is not claimed as passing.

The executor returned after implementation/qualification with metadata pending; the parent wrote this summary and performs the guarded state/roadmap commit inline to reduce closeout delay. Phase-wide SMR requirements, EVD-02 and broader deferred SUZ-02 remain pending final verification. No hardware, rights, release, milestone-close or next-phase authority changed.

## Next Phase Readiness

Plan04 independent reviews and final native/hosted Debug canonical plus focused GNU qualification remain. No owner setup or input is needed.

## Self-Check: PASSED

All seven task commits and the named final native/GNU logs/status receipts exist. The tracked tree is clean at the tested source HEAD before metadata; summary fields are checked through the SDK before guarded commit.
