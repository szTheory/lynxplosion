---
phase: 04-timers-interrupts-and-first-public-suites
plan: "01"
subsystem: testing
tags: [lynx-tests, cc65, ca65, ld65, BS93, CTest, provenance]
requires:
  - phase: 03-memory-map-direct-load-startup-and-first-diagnostic
    provides: [strict public BS93 loader, startup-tier identity, public post-write watch]
provides:
  - Exact pinned CPU and timer suite build recipes with linked symbols and bounded first-stop tracing
  - Rights manifests, complete upstream MIT notice, and accurate source-exposure records
  - Registered positive tracer and six bounded malformed-input/build-refusal controls
affects: [Phase 04 timer scheduler, suite runner, test inventory, fixture provenance]
actuals:
  tokens: 9385
  tasks: 3
  commits: 3
commits: 3
plan_head_before: 067d552328c25588fd655f4af525cd92b133ec25
plan_head_after: 2054302d857bc2ae6e930e600f39aca7c896f667
tech-stack:
  added: []
  patterns: [pinned-source build in owned scratch, source-derived watch acceptance, isolated positive and refusal controls]
key-files:
  created:
    - scripts/build-suites.sh
    - fixtures/suites/bll.cfg
    - fixtures/suites/cpu-wrapper.s
    - fixtures/suites/timers-wrapper.s
    - fixtures/suites/NOTICE
    - fixtures/manifests/cpu-tests.manifest
    - fixtures/manifests/timer-tests.manifest
  modified:
    - tests/diag/test_diag.c
    - tests/diag/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
    - docs/provenance.md
    - docs/policy/exposure-record.md
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-01-PLAN.md
    - .planning/phases/04-timers-interrupts-and-first-public-suites/04-PLANNING-AUDIT.md
key-decisions:
  - "Keep the upstream test assembly and generated BLL/object/symbol outputs in the verified local pin and invocation-owned scratch; commit only the original wrapper, recipe, records and notice."
  - "Accept only the exact source-derived first timer write as unavailable today or as a one-shot watch hit after that write once implemented; budget and unrelated stops remain failures."
  - "Task 1 is a fixture-only tracer with no meaningful product RED. The plan's erroneous TDD marker was corrected after adjudication; no fabricated RED was recorded."
patterns-established:
  - "Build scripts verify pin markers, tool/source/license hashes and unchanged upstream assembly before publishing temporary outputs."
  - "Prefix evidence reports linker labels, exact address/PC, request, progress, model byte and done sentinel separately from suite completion."
requirements-completed: [EVD-02, EVD-04]
coverage:
  - id: D1
    description: "Both untouched suites assemble, link as strict tier-A BS93 objects, load through the public API and stop at their source-derived first timer write."
    requirement: EVD-02
    verification:
      - kind: integration
        ref: "ctest --preset check -R '^suite\.(build|build_controls)$'"
        status: pass
      - kind: other
        ref: "build/check/tests/lynx-test-diag suite_build for cpu/timers × lynx1/lynx2"
        status: pass
    human_judgment: false
  - id: D2
    description: "Suite inputs have exact pin, recipe, licence and exposure records, and malformed inputs and unsafe outputs fail closed."
    requirement: EVD-04
    verification:
      - kind: unit
        ref: "suite.build_controls; scripts/check-manifests.sh; scripts/check-claims.sh; scripts/hygiene.sh --tree"
        status: pass
    human_judgment: false
duration: "144 min (from the recorded Phase 4 session start; includes the TDD adjudication pause)"
completed: 2026-11-10
status: complete
---

# Phase 4 Plan 1: Build and Load the Untouched Suites Summary

**The pinned CPU and timer assembly now builds and loads through the public BS93 path, with the exact first timer-register refusal observable and rights-bound test artifacts.**

## Performance

- **Duration:** 144 minutes from the recorded Phase 4 session start, including the TDD adjudication pause.
- **Started:** 2026-11-11T01:47:40Z
- **Completed:** 2026-11-11T04:11:51Z
- **Tasks:** 3
- **Files modified:** 15

## Accomplishments

- Added original wrappers and a deterministic `build-suites.sh` recipe that verifies the exact pinned cc65 and lynx-tests identities, builds both untouched suites with strict BS93 headers and linker labels, copies the upstream notice to generated output, and keeps generated artifacts in owned scratch.
- Added fixture manifests, the complete upstream MIT notice, exact commit/archive identities, cc65 recipe details and a source-exposure row for the files actually read. Original suite wrappers remain all rights reserved while O5 is open.
- Registered `suite.build` and `suite.build_controls`; controls exercise missing labels, malformed BS93 length, missing pinned tools, missing pinned source, changed assembly identity and a symlink output path. Each fault is bounded and bracketed by positive builds. The final fast inventory hash is bound in the repeatability claim.

## Required Guest Register Evidence for the Next Suite Boundary

The first observed requirements are CPU `TIM6CTLA` `$FD19` and timers `TIM3CTLA` `$FD0D`. Source-wide timer-register references are CPU timer 6 (`$FD18`–`$FD1B`) and timer-suite timers 3 (`$FD0C`–`$FD0F`) and 5 (`$FD14`–`$FD17`); both suites also reference `INTRST` `$FD80` and `INTSET` `$FD81`. The CPU suite uses the RAM interrupt vector at `$FFFE`/`$FFFF`. `lynx.inc` defines `CPUSLEEP` as `$FD91`, but neither pinned suite source references `CPUSLEEP`; the current tracer did not encounter `$FD91`. These are source observations and bounded-prefix measurements, not a statement that the devices are implemented or qualified.

## Task Commits

1. **Task 1: Build and load both untouched suites, then inspect their real first mapped stop** — `7afb82d` (`feat`).
2. **Task 2: Bind original recipes and fetched upstream notices to current rights records** — `5350f97` (`docs`).
3. **Task 3: Make the build tracer and malformed-build controls permanent** — `2054302` (`test`).

## Files Created or Modified

- `scripts/build-suites.sh` and `fixtures/suites/` — original wrapper/build recipe and notice; generated outputs stay in scratch.
- `fixtures/manifests/cpu-tests.manifest` and `fixtures/manifests/timer-tests.manifest` — exact input hashes, source revision, build recipe and allowed use.
- `tests/diag/test_diag.c`, `tests/diag/tests.cmake`, `tests/inventory/fast.txt` — public-load tracer, bounded refusal controls and registered IDs.
- `docs/claims/skeleton-repeatability.claim`, `docs/provenance.md`, `docs/policy/exposure-record.md` — exact inventory binding, third-party notices and actual source exposure.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-01-PLAN.md`, `04-PLANNING-AUDIT.md` — corrected fixture-only TDD classification and adjudication rationale.

## Decisions Made

- The first stop is accepted only when it matches the source-derived timer write and exact post-instruction PC, with progress `1`, model byte `0` and done sentinel `0`. The tracer is future-ready for the exact one-shot post-write watch result after that register write is implemented.
- Task 1 has no meaningful RED for this fixture-only contract: the intended current-core outcome is an unavailable mapped access. The task's incorrect `tdd="true"` marker was corrected to false, phase TDD mode is false, and no compiler, setup or missing-target error was presented as RED.
- O1 remains open. These prefix observations establish neither a CPU-profile suite pass nor hardware accuracy. O5 (project licence), O6 (clean-room policy), and all release/publication and physical-hardware boundaries remain open or unauthorized.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking issue] Missing pinned source needed a stable refusal**
- **Found during:** Task 3 controls.
- **Issue:** A missing `tests.s` would otherwise fail through the platform `shasum` diagnostic rather than the intended source-missing refusal.
- **Fix:** Added an explicit file-existence check with a stable `pinned source missing` diagnostic.
- **Files modified:** `scripts/build-suites.sh`.
- **Verification:** `suite.build_controls` asserted exit 1 and the exact refusal, then rebuilt the restored pin successfully.
- **Committed in:** `2054302`.

**2. [Rule 3 - Claim binding] The inherited repeatability claim hash was stale**
- **Found during:** Task 3 inventory update.
- **Issue:** After Task 1 registered `suite.build`, the claim still contained the prior inventory digest.
- **Fix:** Rebound it to the exact final inventory containing both suite IDs (`64955fa41243854494e4b6b3f938bb382c60eca29e1adeaf533ae6f1a38124be`).
- **Files modified:** `docs/claims/skeleton-repeatability.claim`.
- **Verification:** `scripts/check-claims.sh` passed.
- **Committed in:** `2054302`.

**Total deviations:** 2 auto-fixed. The final build, test inventory, claim hash and fixture records are consistent; the intermediate Task 1 commit did not contain the claim refresh that the plan called for.

## Issues Encountered

The first execution attempt paused because the plan marked a fixture-only tracer as TDD. The parent adjudicated the classification and corrected the plan and audit before execution resumed. No invalid RED was created. The plan's 25 automated-command checks and prohibition review had already passed before implementation. During bookkeeping, `state.advance-plan` initially found no numeric plan pointer in STATE.md; the minimal truthful position (plan 1 of 12) was restored, then the registered handler advanced the pointer to plan 2 only.

## Verification

- `sh scripts/build-suites.sh --out build/check/scratch/phase04-final` — passed; exact pin, source and license hashes recorded and both source files verified unchanged.
- `cmake --preset check && cmake --build --preset check` — passed.
- `ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(build|build_controls)$'` — 2/2 passed.
- Direct `suite_build` for CPU/timers × lynx1/lynx2 — all four bounded prefixes matched their exact expected first-stop evidence.
- `sh scripts/check-manifests.sh` — passed, 9 manifests and 18 fixture files covered.
- `sh scripts/check-claims.sh`, `sh scripts/hygiene.sh --tree`, `sh -n scripts/build-suites.sh`, and `git diff --check` — passed.

## User Setup Required

None. No external service or user action is needed to use this plan's local checks.

## Next Phase Readiness

Plan 04-02 can implement the selected timer boundary using the observed timer and interrupt address set. The tracer remains valid if the exact first write becomes implemented because it will then require the corresponding one-shot watch hit. No timer behavior, full suite completion, CPU-profile qualification, firmware/game use, hardware test or release claim is made here.

## Self-Check: PASSED

Summary files, manifests, wrappers, build script, diagnostic test and task commits exist. All three task commits are ancestors of the current plan head. The verified plan commit count is measured from the persisted ledger.

---
*Phase: 04-timers-interrupts-and-first-public-suites*
*Completed: 2026-11-10*
