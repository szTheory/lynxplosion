---
phase: 06-audio-and-playable-checkpoint
plan: "11"
subsystem: testing
tags: [benchmark, A/V replay, CTest, reproducibility, performance]

# Dependency graph
requires:
  - phase: 06-01
    provides: Public A/V advancement and caller-owned capture spans
  - phase: 06-09
    provides: Repaired audio behavior and pinned A/V source observations
  - phase: 06-10
    provides: Audio regression controls and deterministic inventory conventions
provides:
  - Five-run fixed-workload public A/V throughput baseline with complete output identities
  - Named benchmark-report parser controls based on one validated immutable corpus
  - Native performance baseline with separate API, encoding, assembly, publication, and consumer timing
affects: [06-12, performance, deterministic-evidence, consumer]

# Diff-character/4 estimate over the six plan-owned tracked paths; not model/API token usage.
actuals:
  tokens: 18547
  tasks: 2
  commits: 2
plan_head_before: ea5c2f87ad5e32a61b6ea3625ad7255e782e99e4
# Measured after both task commits and before this guarded summary commit.
plan_head_after: 38fd41d4d74a499afa5d3f65882257c6c256854d

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Measure host time around public A/V advancement without importing clocks into the core
    - Derive parser mutations from one validated immutable five-run corpus

key-files:
  created:
    - docs/performance.md
  modified:
    - consumer/main.c
    - tests/consumer/av_driver.cmake
    - tests/consumer/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Report the actual successful public progress tick separately from the guest-window bound."
  - "Keep core API duration, frame encoding, stream assembly, publication, full consumer time, and CI qualification as separate measurements."
  - "Report host-specific software throughput without hardware accuracy claims or cross-host comparisons."

patterns-established:
  - "Compare all 13 streams by size and SHA-256 across five fresh same-build runs."
  - "Label fabricated report mutations as parser evidence and assert their exact rejection diagnostics."

requirements-completed: [PERF-01, HB-03, DET-05]
coverage:
  - id: D1
    description: "A fixed source-built A/V workload produces five equal fresh-run observations and stream/output identities, with API and host work timed separately."
    requirement: PERF-01
    verification:
      - kind: integration
        ref: "Native consumer.av_benchmark at Task 1 commit 93959ca26a0017a0130ffa0abd71856ffad8262c; exact five-run result and identities in docs/performance.md"
        status: pass
      - kind: integration
        ref: "Native and GNU 13.3 consumer.av_benchmark selector at Task 2 content; 1/1 and 1/1 passes"
        status: pass
    human_judgment: false
  - id: D2
    description: "Incomplete or unlike benchmark reports cannot pass the baseline parser, including timing, identity, output, and no-progress cases."
    requirement: HB-03
    verification:
      - kind: integration
        ref: "Native and GNU 13.3 consumer.av_benchmark_controls; one real five-run baseline and 28 named parser rejections per run"
        status: pass
    human_judgment: false
  - id: D3
    description: "The fast test inventory remains bound to the repeatability claim after adding the benchmark parser control."
    requirement: DET-05
    verification:
      - kind: other
        ref: "det.two_runs, det.two_instances, det.golden (3/3) and sh scripts/check-claims.sh on native and GNU 13.3"
        status: pass
    human_judgment: false

# Metrics
duration: 53 minutes
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 11: Fixed A/V throughput baseline Summary

**Five identical A/V workload runs now report public API throughput separately from frame encoding, stream assembly, and publication.**

## Performance

- **Duration:** 51 minutes elapsed from dispatch; this is wall-clock handoff time, not active model time or API usage.
- **Started:** 2026-11-13T11:27:19Z
- **Completed:** 2026-11-13T12:20:35Z
- **Tasks:** 2
- **Files modified:** 6 plan-owned implementation, test, inventory, claim, and documentation files.

## Accomplishments

- Added the opt-in host-only `av-benchmark` mode around the existing source-built A/V guest. The monotonic timer brackets only calls to `lynx_advance_av`; output capture, parsing, assembly, hashing, and publication have separately reported intervals. Normal A/V replay bundles remain unchanged.
- Five fresh native runs at source commit `93959ca26a0017a0130ffa0abd71856ffad8262c` produced equal 1,248,256-tick work and identical 13-stream/output digests. The 1,253,985 tick value is the work-window bound, not the observed completion point. The exact native environment, five raw samples, medians, ranges, workload identities, and limitations are in [performance.md](../../../docs/performance.md).
- Added `consumer.av_benchmark_controls`. It builds one real five-run baseline, validates each output stream against its report and the independent artifact checker, then rejects 28 static malformed-corpus mutations at their named diagnostics. A real no-run guest is refused before publishing.
- Native AppleClang and GNU 13.3 builds passed the focused benchmark/control selector: two selected tests, two executed passes, no skips. The GNU run used a real fast-forward to exact commit `38fd41d4d74a499afa5d3f65882257c6c256854d`, then confirmed exact HEAD and a clean tracked tree before configure/build.
- `det.two_runs`, `det.two_instances`, and `det.golden` passed 3/3 on native and GNU; `sh scripts/check-claims.sh` passed on both. Only the inventory hash was refreshed in the claim.

## Task Commits

1. **Task 1: Expose a reproducible in-memory benchmark of the real A/V workload** - `93959ca` (`feat`)
2. **Task 2: Record median/range and prove unlike or incomplete samples cannot form a baseline** - `38fd41d` (`test`)

## Files Created/Modified

- `consumer/main.c` - Host-only benchmark mode and separate timing/report fields.
- `tests/consumer/av_driver.cmake` - Five-run identity checks, timing summaries, and named parser evidence.
- `tests/consumer/tests.cmake` - Registered the benchmark parser-control test.
- `tests/inventory/fast.txt` - Added the sorted parser-control inventory row.
- `docs/performance.md` - Recorded the native workload, environment, exact samples, medians/ranges, and pending CI timing obligation.
- `docs/claims/skeleton-repeatability.claim` - Refreshed only the inventory SHA-256 after its required proofs.

## Decisions Made

- The benchmark reports successful guest progress and the outer window bound independently. Five-run comparison includes all output stream extents and hashes, not just elapsed time.
- The API throughput number is specific to the public advancement calls and the named build/workload. It is not a hardware accuracy result, an emulator-wide cost, or a cross-host comparison.
- CI queue/setup/build/CTest/hygiene/history measurements remain a Plan 12 qualification obligation. No performance optimization was made.

## Deviations from Plan

None. The benchmark and controls use the existing public API, pinned fixture, test inventory, and build tools; no core performance switches, new dependencies, or production behavior changes were added.

## Issues Encountered

- The first native control wrapper reached a successful CTest result but its final status-file assignment used zsh's reserved `status` variable. The CTest and LastTest logs confirmed the pass; the wrapper was corrected to use `ctest_rc`, and the full native selector was rerun with durable status capture.
- The exact host CPU model could not be read because the targeted Darwin metadata query was denied. The report records Darwin 25.6.0 arm64 and explicitly marks the exact model unavailable.

## User Setup Required

None.

## Next Phase Readiness

Plan 12 owns final qualification and CI timing capture. It should record queue, setup, build, test, hygiene, and history costs separately, then link those measurements from the performance document. No owner action is needed for this handoff.

## Self-Check: PASSED

- Both task commits exist and are ancestors of the current plan head; the plan range contains exactly two task commits.
- The summary and all six plan-owned files exist.
- Native and GNU benchmark/control selectors passed 2/2; each control run rejected all 28 named mutations. The deterministic inventory proofs and claim check passed on both builds.
- `git diff --check` passed, and the guarded metadata commit will rerun final tree hygiene after this summary and the performance qualification update are written.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-13*
