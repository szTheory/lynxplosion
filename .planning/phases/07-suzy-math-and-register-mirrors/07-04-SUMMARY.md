---
phase: 07-suzy-math-and-register-mirrors
plan: "04"
subsystem: review-qualification
tags: [suzy, review, security, qualification]
requires:
  - phase: 07-03
    provides: pinned math and memio diagnostic integration
provides:
  - independent review and ASVS1 security audit with all findings repaired
  - exact-source native, GNU and hosted qualification
affects: [phase-07-closeout]
actuals:
  tasks: 2
  commits: 7
plan_head_before: 7eec3a3
plan_head_after: 3b50de650226f392655c65660c94655c97ea8ea0
key-files:
  created:
    - .planning/phases/07-suzy-math-and-register-mirrors/07-SECURITY.md
    - .planning/phases/07-suzy-math-and-register-mirrors/07-QUALIFICATION.md
  modified:
    - core/machine.c
    - tests/suites/test_public_suites.c
    - tests/suites/run.cmake
    - tests/suzy/test_suzy.c
    - tests/suzy/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/public-suites.md
key-decisions:
  - "A coarse advance must select the pending Suzy deadline before any CPU or sample-only prefix bound; verified by a red-then-green alignment sweep under both timing policies."
  - "Old attributable scratch is preserved outside the build scan after age and inventory checks; scanner policy is unchanged."
requirements-completed: [SMR-01, SMR-02, SMR-03, SMR-04]
coverage:
  - id: D1
    description: Review findings CR-01, WR-01, WR-02 and security finding T-07-03 repaired with owning regression controls
    verification:
      - kind: integration
        ref: suite.math.controls, suite.math.strict_comparison_controls, suzy.coarse_deadline
        status: pass
    human_judgment: false
  - id: D2
    description: Exact-source qualification on native Debug, GNU optimized sanitizer groups and hosted CI
    verification:
      - kind: integration
        ref: "scripts/check.sh 271/271 native; GNU 142/142; CI run 43612316517"
        status: pass
    human_judgment: false
duration: one session
completed: 2026-11-14
status: complete
---

# Phase 7 Plan 04 Summary

**The Suzy math and register-mirror software scope is independently reviewed, security-audited (SECURED, 17/17 threats closed) and qualified at exact source `3b50de6`, with strict timing results and all physical, rights and release stops unchanged.**

## Accomplishments and verification

Review findings CR-01 (overflowing oracle range), WR-01 (unverified guest return boundary) and WR-02 (missing hostile strict-record controls) were repaired in `0fe0734`, `7848e48` and `612c1e4`. The independent ASVS1 audit then found and reproduced one more defect: the machine's sample-only prefix skip ran before the Suzy event was considered, so a granted CPU cycle straddling a math deadline let a coarse advance step past it, leaving the operation busy and its own snapshot unrestorable. The repair (`b287750`) selects the Suzy event first. `suzy.coarse_deadline` sweeps 0-400 NOPs under both timing policies; it failed at policy 1, 68 NOPs, deadline 1198 before the repair and passes after.

Qualification (see 07-QUALIFICATION.md): native Debug `scripts/check.sh` 271/271 accounted (2 declared skips) in 329s; GNU gcc 13.3 `-O2 -g -UNDEBUG` ASan/UBSan owning groups 142/142 in 35.4s plus ShellCheck 0.9; hosted CI run 43612316517 success at the exact pushed source.

## Task Commits

1. Review repairs — `0fe0734`, `7848e48`, `612c1e4`; review record `1262a7f`.
2. Security repair and test — `b287750`; claim rebound `3b50de6`.

## Deviations and issues resolved

The first gate failed at the artifact scan on four stale scratch roots from earlier red-phase controls; they were preserved outside the build after age and SHA-256 inventory checks, with no scanner change. A later gate failed at `docs.claim_template` because the inventory hash was stale after the new test; the claim was rebound. My first regression test did not reproduce the defect (bus hold hid the alignment); it was rewritten from the auditor's concrete scenario and confirmed red before the repair. The GNU run used the optimized flags from the plan in `build/check` (the suite harness owns that scratch root) and covered the affected groups, not the whole inventory.

## Next Phase Readiness

Phase 7 software scope is complete. Full SUZ-02 and the timer comparison (EVD-02) remain open and physical testing, rights and release stops persist. No archive or next phase has been started; milestone state needs an owner-visible disposition.

## Self-Check: PASSED

Source commits, native/GNU logs and hosted run exist; tracked tree is clean at the tested source before metadata.
