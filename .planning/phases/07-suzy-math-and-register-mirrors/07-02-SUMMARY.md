---
phase: 07-suzy-math-and-register-mirrors
plan: "02"
subsystem: suzy
tags: [atari-lynx, arithmetic, snapshots, determinism, fault-controls]
requires:
  - phase: 07-01
    provides: mapped unsigned math operation, canonical Suzy record, snapshot codec and digest domain
provides:
  - signed and accumulated multiplication, unsigned division, aliases and software status policy
  - deadline, reset/load, snapshot-refusal and continuation coverage
  - copied-source decoder, result, completion and snapshot fault controls
affects: [phase-07-plans-03-and-04, suzy-math, determinism, requirements-smr]
actuals:
  tokens: 16027
  tasks: 2
  commits: 3
commits: 3
plan_head_before: ba2e4fcb9564c30c7f8f2682a5a562910b14b482
plan_head_after: 74d9e0ea2cfba277f5377f05913a106807a8d067
tech-stack:
  added: []
  patterns: [independent guest-mapped vectors, nonce-owned copied-source fault controls]
key-files:
  created: [tests/suzy/fault_controls.cmake]
  modified:
    - core/suzy.c
    - tests/suzy/test_suzy.c
    - tests/suzy/tests.cmake
    - tests/det/test_det.c
    - tests/det/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/suzy-math.md
key-decisions:
  - "Arithmetic, remainder and guest-tick latency remain explicit deterministic software policies, not hardware claims."
  - "Busy SPRSYS mode and orientation writes remain accepted; each operation uses its captured mode."
  - "The existing format-8 88-byte Suzy record and guest-state digest domain remain unchanged."
patterns-established:
  - "Compare guest-visible operation paths across coarse, sliced, interleaved and restored runs, then compare canonical fields and snapshots."
  - "Require each copied-source mutant to fail only its named behavioral assertion between clean positives."
requirements-completed: []
coverage:
  - id: D1
    description: "Signed, accumulated and divide operations follow independent arithmetic vectors and explicit mirror-port rules."
    verification:
      - kind: unit
        ref: "suzy.arithmetic, suzy.flags, suzy.registers; /private/tmp/lynx-phase7-936pa8pp/agent02/t1-native-task-group-final.log"
        status: pass
    human_judgment: false
  - id: D2
    description: "Operation deadlines, busy refusal, reset/load cancellation and atomic snapshot refusal are checked at their boundaries."
    verification:
      - kind: unit
        ref: "suzy.lifecycle, suzy.snapshot_refusal; /private/tmp/lynx-phase7-936pa8pp/agent02/t2-native-owning-final-edges.log"
        status: pass
      - kind: integration
        ref: "api.reset_power_on, api.load_refusal_unchanged, api.scheduler_order, api.scheduler_limits, api.input_orientation; /private/tmp/lynx-phase7-936pa8pp/agent02/t2-native-owning-final-edges.log"
        status: pass
    human_judgment: false
  - id: D3
    description: "All four math kinds produce equal deterministic state through sliced, interleaved and snapshot-restored paths on both profiles."
    verification:
      - kind: integration
        ref: "det.suzy_paths, det.suzy_sensitivity, det.two_runs, det.two_instances, det.golden; /private/tmp/lynx-phase7-936pa8pp/agent02/t2-native-owning-final-edges.log"
        status: pass
    human_judgment: false
  - id: D4
    description: "Independent decoder, result-bit, completion-tick and accepted-snapshot mutations fail only their named assertions."
    verification:
      - kind: integration
        ref: "suzy.fault.decoder, suzy.fault.result, suzy.fault.completion, suzy.fault.snapshot; /private/tmp/lynx-phase7-936pa8pp/agent02/t2-gnu-owning-final.log"
        status: pass
    human_judgment: false
  - id: D5
    description: "The repeatability claim is bound to the current exact test inventory and software-policy limits are documented."
    verification:
      - kind: other
        ref: "docs.claim_template, docs.claim_template.self_test; /private/tmp/lynx-phase7-936pa8pp/agent02/t2-native-owning-final-edges.log"
        status: pass
    human_judgment: false
metrics:
  duration: 335min
  completed: 2026-11-14
status: complete
---

# Phase 7 Plan 02: Arithmetic, aliases and lifecycle summary

**Signed, accumulated and divide register behavior now has independent mapped vectors, deadline boundaries, atomic snapshot checks and four semantic fault controls.**

## Performance

- **Duration:** 5h 35m (elapsed from the first Plan 07-02 execution receipt)
- **Started:** 2026-11-13T20:18:53Z
- **Completed:** 2026-11-14T01:54:06Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- Task 1 added signed, accumulated and unsigned division behavior with seeded unsigned vectors, hand-derived sign cases, flags and both register views. The real public arithmetic tracer failed against the pre-change core because signed math was unavailable, then passed after implementation. No snapshot schema or digest-domain change was needed.
- Task 2 checks both profiles at multiply deadlines and division `clz16` boundaries 0, 1, 15 and 16; the exact-deadline state remains busy until the next positive advance. Reset and accepted load cancel pending math, refused load and seven rehashed malformed snapshots preserve every machine byte, and the deadline headroom boundary is tested on both sides.
- Busy writes are rejected for every protected math index through both mirrors. Public guest writes to representative base and mirror addresses stop as unavailable without changing hidden RAM. `det.suzy_paths` compares plain, signed, accumulated and divide state through coarse, one-tick, irregular, interleaved and restored paths on Lynx I and II.
- Four copied-source controls independently detect a disabled FC55 action, a flipped product bit, a one-tick delay and a changed active snapshot sign. Each control uses a private copy, source/build identity checks, sanitizers and clean positive runs before and after the mutant.
- The skeleton claim now binds inventory SHA-256 `e2169d99af7a2723d4156c56653796232a492791992113794be21866d2de9e77`; the existing golden remains unchanged.

## Task Commits

1. **Task 1: Add the failing public arithmetic tracer** — `df354f2` (`test`)
2. **Task 1: Complete signed, accumulated and division math** — `00d4ec3` (`feat`)
3. **Task 2: Add lifecycle and fault qualification** — `74d9e0e` (`feat`)

## Files Created/Modified

- `tests/suzy/test_suzy.c` — independent arithmetic, timing, busy-write, reset/load and snapshot cases.
- `tests/suzy/fault_controls.cmake` — four copied-source semantic mutants with clean brackets and ownership checks.
- `tests/det/test_det.c` and `tests/det/tests.cmake` — math path equality and all-latch digest sensitivity.
- `tests/suzy/tests.cmake` and `tests/inventory/fast.txt` — exact CTest registration.
- `docs/suzy-math.md` and `docs/claims/skeleton-repeatability.claim` — evidence limits and inventory-bound claim.

## Verification

- Native: exact 18-ID owner selection; all passed: `/private/tmp/lynx-phase7-936pa8pp/agent02/t2-native-owning-final-edges.log` (exit 0).
- GNU: same exact 18 IDs passed on commit `74d9e0e` using GCC 13.3, `RelWithDebInfo`, `-O2 -g -UNDEBUG` and ASan/UBSan: `/private/tmp/lynx-phase7-936pa8pp/agent02/t2-gnu-owning-final.log` (exit 0). The existing clone was fetched from the checked main checkout and fast-forwarded to that commit.
- Binary scan passed: `/private/tmp/lynx-phase7-936pa8pp/agent02/t2-binary-scan.log` (exit 0).
- The final native and GNU groups each discovered exactly 18 expected tests. No full inventory rerun was performed for this plan.

## Decisions Made

- Keep normal division remainder and all selected timing values labeled as software policy; the disabled diagnostic remainder branch does not prove hardware behavior.
- Keep global SMR requirements pending until the later source-built evidence and final phase verification are complete.

## Deviations from Plan

### Test-harness corrections

1. The snapshot serializer is implemented in `core/suzy.c`, so the snapshot mutant was anchored there rather than the plan’s anticipated `core/snapshot.c` location. The mutation changes both saved sign fields so the candidate remains structurally valid and the continuation result differs.
2. The first decoder mutation changed only the duration classifier while leaving the actual command branch active; that mutant remained behaviorally equivalent and was rejected. The mutation now disables the FC55 command action. The ineffective scratch, owner marker, original log and byte/type/mode inventory are retained under `/private/tmp/lynx-phase7-936pa8pp/agent02/retained/` with receipts.
3. Lifecycle test fixtures were corrected to snapshot before completion, watch the command before the later SPRSYS control write, and encode the signed operand pair in the intended mirror order. No Task 2 production-core defect was found.

## Evidence Limits and Remaining Requirements

The evidence establishes the stated deterministic software contract only. It does not establish full SUZ-02 behavior, normal remainder silicon behavior, hardware timing accuracy or hardware accuracy. EVD-02, physical tests, rights and release remain stopped. SMR-01 through SMR-04 remain pending at phase level; Plan 03 and Plan 04 still own required coverage and review.

## Next Phase Readiness

The mapped arithmetic and lifecycle slice is ready for Plan 07-03 source-built math/memio evidence. The phase is not complete, no release qualification is claimed, and no user setup is required.

## User Setup Required

None - no external service or local configuration is required.

## TDD Gate Compliance

The unsigned/signed behavior began with a semantic public guest-path RED against the pre-change core, followed by implementation and passing independent vectors. Task 2 added lifecycle behavior checks against the Task 1 core and four injected semantic mutations; it did not change production arithmetic or claim an unobserved silicon result.

## Self-Check: PASSED

The summary file exists, and all three task commits are ancestors of the checked main head.

---
*Phase: 07-suzy-math-and-register-mirrors*
*Completed: 2026-11-14*
