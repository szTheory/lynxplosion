---
phase: 07-suzy-math-and-register-mirrors
plan: "01"
subsystem: core
tags: [suzy, snapshot, deterministic-state, ctest]
requires:
  - phase: 06-audio-and-playable-checkpoint
    provides: Existing machine, bus, input, snapshot, and deterministic test harness
provides:
  - Mapped guest unsigned Suzy multiplication with guest-time busy/completion
  - Snapshot format 8 and canonical Suzy state in digest domain v4
  - Updated API/diagnostic readers, copied builds, and digest evidence
affects: [phase-07-plans-02-through-04, snapshot-readers, deterministic-evidence]
actuals:
  tokens: 17600
  tasks: 2
  commits: 7
tech-stack:
  added: []
  patterns:
    - Instance-owned non-owner guest event for math completion
    - Fieldwise canonical state record shared by snapshot and digest
key-files:
  created:
    - core/suzy.c
    - tests/suzy/test_suzy.c
    - tests/suzy/tests.cmake
    - docs/suzy-math.md
    - .planning/phases/07-suzy-math-and-register-mirrors/07-01-T1-RED.json
  modified:
    - core/internal.h
    - core/machine.c
    - core/input.c
    - core/snapshot.c
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/consumer/check_artifacts.c
    - tests/diag/test_diag.c
    - tests/det/test_det.c
    - tests/golden/skeleton.digest
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
key-decisions:
  - Unsigned multiplication is the only implemented arithmetic path; signed, accumulate, and divide commands remain explicit refusals for later plan work.
  - Input and handedness remain owned by the existing input grant path.
  - The digest golden was accepted only after the actual native and GNU outputs matched and Suzy state sensitivity passed.
patterns-established:
  - One instance-owned non-owner guest event publishes a pending math result before same-tick CPU work.
  - One fieldwise canonical record defines both snapshot bytes and digest input.
duration: 72min
completed: 2026-11-13
status: complete
plan_head_before: 152f784c87d40960e9dca702ac6f213bf37db9ef
plan_head_after: a3938ef3fa4a0cbd8ff11cd9603751a4b912010b
commits: 7
requirements-completed: []
coverage:
  - id: D1
    description: Mapped unsigned multiply starts, reports busy, and returns the expected product on both CPU profiles.
    verification:
      - kind: integration
        ref: CTest suzy.tracer, native and GNU optimized
        status: pass
    human_judgment: false
  - id: D2
    description: Busy operation state resumes identically after snapshot restore and changes the canonical digest.
    verification:
      - kind: integration
        ref: CTest suzy.snapshot_tracer and det.suzy_sensitivity, native and GNU optimized
        status: pass
    human_judgment: false
  - id: D3
    description: Format readers, copied builds, digest golden, and claim binding agree with the format 8 state change.
    verification:
      - kind: integration
        ref: CTest det.golden, docs.claim_template, and copied-reader controls; GNU full inventory
        status: pass
    human_judgment: false
---

# Phase 07 Plan 01: Mapped Suzy multiply and snapshot state summary

**A public mapped unsigned multiply now completes on guest time, survives a busy snapshot, and contributes canonical state to format 8 and digest v4.**

## Performance

- **Duration:** 72 min
- **Started:** 2026-11-13T23:15:17Z
- **Completed:** 2026-11-14T00:27:11Z
- **Tasks:** 2
- **Files modified:** 26 tracked files

## Accomplishments

- Added an instance-owned Suzy register block with mapped mirrors, an unsigned multiply command, non-owner completion event, busy/status controls, and explicit refusal for arithmetic modes reserved to later work.
- Appended an 88-byte canonical Suzy record to snapshot format 8 and digest domain `lynxplosion.guest-state.v4`; updated independent readers, source lists, copied-core builders, and inventory evidence.
- Re-established the skeleton digest from native and optimized GNU runs. Actual digest `7cd1222c5491855a8e7f50b5bc44dbd42871acd04bfc54ceaf69e2fe48221898` matched across compilers after `det.suzy_sensitivity` passed.

## Task Commits

1. **Task 1: public mapped tracer RED** - `c2c8572` (test)
2. **Task 1: correct tracer operand order** - `d8c0bfa` (test)
3. **Task 1: implement mapped multiply and format 8** - `3e321d1` (feat)
4. **Task 1: bind the Suzy digest golden** - `b1b67ac` (test)
5. **Task 2 repair: check frame-publisher cleanup writes** - `c2bb778` (fix)
6. **Task 2 repair: bind map/raw-read fault controls to semantic failures** - `b038efa` (test)
7. **Task 2: record early integration results** - `a3938ef` (docs)

## Files Created/Modified

- `core/suzy.c` - private mapped register state and scheduled unsigned multiply.
- `tests/suzy/test_suzy.c` - public guest tracer, snapshot continuation, and state sensitivity.
- `docs/suzy-math.md` - software policy, evidence limits, digest identity, and early integration results.
- `tests/api/test_api.c` and `tests/consumer/check_artifacts.c` - first-state API fault assertions and checked publisher cleanup write.
- Other modified paths are listed by the 07-01 commit range; all changes passed `git diff --check`.

## Decisions Made

- Keep JOYSTICK, SWITCHES, and lefthand updates in the existing input grant path; SPRSYS composes orientation with Suzy status.
- Preserve the half-open guest-time deadline: an event at the requested target remains pending until the next positive advance.
- Treat 44 guest ticks and the unsafe flag policy as documented software interpretations, not measured hardware facts.
- Leave requirements SMR-01 and SMR-03 pending until their shared coverage in the later Phase 7 plans is complete.

## TDD Gate Compliance

- The public `suzy.tracer` RED run compiled and failed semantically because the pre-change core did not expose the mapped operand register at `FC52`; it did not fail from a missing internal struct or zero test discovery.
- The machine-validated record is `.planning/phases/07-suzy-math-and-register-mirrors/07-01-T1-RED.json`, and `gsd_run check tdd-red-evidence` returned `RED_EVIDENCE_OK`.
- RED commits `c2c8572` and `d8c0bfa` precede the passing implementation commit `3e321d1`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed an optimized-GCC harness warning that stopped the required full build.**

- **Found during:** Task 2, optimized GNU build
- **Issue:** `tests/consumer/check_artifacts.c` discarded the cleanup `write()` result, which GCC reports as `-Werror=unused-result` under the plan's `-O2` flags.
- **Fix:** Added a bounded cleanup write helper that retries `EINTR`, verifies all waiting children are released, and reports cleanup failure without changing the publication-control assertions.
- **Files modified:** `tests/consumer/check_artifacts.c`
- **Verification:** Native and GNU `consumer.frame_publication_controls` passed; the complete GNU build and inventory passed.
- **Committed in:** `c2bb778`

**2. [Rule 1 - Bug] Kept API mutation controls attached to the intended map and raw-read invariants.**

- **Found during:** Task 2, native full inventory
- **Issue:** Mapping Suzy changed the first observable assertion for two existing mutants; their self-tests still expected pre-Suzy status failures.
- **Fix:** Added named RAM-route and raw backing-RAM assertions and updated each control to require that semantic failure. The mutations and positive cases remain intact.
- **Files modified:** `tests/api/test_api.c`, `tests/api/tests.cmake`
- **Verification:** Both direct mutants failed at the named intended assertions; native and GNU API/memory groups passed 72/72.
- **Committed in:** `b038efa`

**Total deviations:** 2 auto-fixed. Both were required for the planned optimized integration and first-state negative controls.

## Issues Encountered

- The initial native full inventory ran 252 tests and reported two API self-test failures plus the two pre-declared page-mode skips. After the self-test repair, the native API/memory group passed 72/72. The full native inventory was not repeated; this remains an explicit final-qualification item for the later Phase 7 qualification plan.
- The GNU full build was first interrupted by the unused-write warning above. After repair, exact-source optimized configure/build succeeded and GNU CTest passed all 252 registered tests with only the two existing declared skips.
- An interrupted GNU build left the deterministic test executable stale; its first sensitivity invocation reported an unknown case. Rebuilding that exact target made `det.suzy_sensitivity` pass. The original output remains in the private evidence logs.

## Integration Evidence

- Native and GNU exact discovery selected all 28 plan-01 required CTest IDs (`expected=28 selected=28 missing=[]`).
- Native named golden/claim checks passed 7/7. GNU digest, claim, two-run, two-instance, Suzy sensitivity, and frame-publication checks passed 8/8.
- GNU used `RelWithDebInfo`, `-O2 -g -UNDEBUG`, ASan+UBSan, and non-recovering sanitizers; effective compile and link commands retained assertions and did not define `NDEBUG`.
- Installed ShellCheck identities were native 0.11.0 and GNU 0.9.0. This was an identity check only; no lint run was part of this plan.
- Durable command output and original failures are retained under `/private/tmp/lynx-phase7-936pa8pp/agent01/`, including `native-full-inventory.log`, `native-api-memory-focused.log`, `gnu-full-inventory.log`, `gnu-flags-proof-retry.log`, `native-discovery-final-retry.log`, and `gnu-discovery-shell.log`. The failing API fault controls produced no scratch bundles or owner markers.

## User Setup Required

None.

## Next Phase Readiness

The first mapped unsigned operation and its format/digest consumers are ready for Plan 07-02 arithmetic expansion. SMR-01 and SMR-03 remain pending for the phase-level verifier; full SUZ-02, EVD-02, hardware accuracy, physical testing, rights clearance, and release authorization remain open.

## Self-Check: PASSED

- The created implementation, test, documentation, and RED-evidence files exist.
- All seven task commits in this summary are reachable from the plan base.
- The GNU full inventory and repaired native owning groups pass. The original native full-inventory failures are preserved and explicitly remain for final native qualification; this summary does not claim that native full run passed.

---
*Phase: 07-suzy-math-and-register-mirrors*
*Completed: 2026-11-13*
