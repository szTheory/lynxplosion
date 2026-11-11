# Timer-suite comparison: proposed disposition

Status: **owner decision pending**. No acceptance gate, oracle, hardware policy,
requirement completion, or phase boundary has been changed.

## Decision in plain language

Both CPU profiles complete the timer suite and agree with 15 of its 16 source
expectations. The fixed-loop cascade result is 12; the pinned source expects
13. An independent diagnosis explains 12 exactly under the current provisional
five-tick/no-DMA model. It does not establish why the original environment
produced or expected 13.

The original CRT enables display DMA and page-mode timing, which are outside
Phase 4 and scheduled for Phase 5. The test also increments a zero-page counter
without explicitly initializing it. The tier-A wrapper deliberately clears
zero page. Historical initial-state/measurement evidence does not establish
the expected counter value. These are known precondition differences, not a
proven hardware diagnosis or permission to seed the counter to one.

**Recommendation:** finish Phase 4's timer/harness evidence with this one
strict timing comparison explicitly pending, then revisit it during the
already planned Phase 5 bus/video work. This changes the Phase 4 plan's
all-values-agree acceptance condition and therefore needs an owner decision.
It does not accept 12 as hardware-correct or claim the suite passes.

## Concrete changes if the recommendation is selected

1. Keep the exact source expectation 13, all accepted sets, and the strict
   comparator's failing result. Preserve complete raw records for both
   profiles, including observed 12 and the exact failing row.
2. Distinguish completed raw execution from source agreement in test names,
   records, documentation, and the final phase report. Never make a generic
   nonzero exit, timeout, or absent result satisfy a positive observation.
3. Add a narrowly identified pending comparison for this row and provisional
   bus environment. All other result rows, model/completion/count/symbol
   checks, negative controls and independent timer/IRQ tests remain required.
   Any new disagreement is a failure, not covered by this proposed exception.
4. Map the outstanding agreement evidence into Phase 5's existing bus/DMA
   prerequisite and keep it open in the rolling roadmap and requirements.
   Do not silently skip it, mark global evidence complete, or weaken O1.
5. Complete remaining Phase 4 controls, reviews, fixes and final local/Linux/CI
   qualification under the revised explicit gate. The existing ShellCheck
   finding remains a required fix. No release, firmware, physical test, new
   dependency, baseline adoption or raw-video API decision is included.
6. In Phase 5, establish initial-state and bus/video preconditions from
   independent sources before repeating the unchanged comparison. If evidence
   still does not justify 13, retain the disagreement; do not tune timing or
   fixture layout to obtain agreement.

## Alternative: move prerequisites forward

Keep Phase 4's all-values-agree gate and first research/plan the required
page-mode/refresh/display-DMA bus behavior as an explicit prerequisite slice.
This brings work currently reserved for Phase 5 into Phase 4. Exact agreement
is not guaranteed: the counter-initialization and measurement-provenance gaps
remain. Raw video API form, input and presentation would stay outside that
prerequisite unless independently required and approved.

## Current checkpoint and evidence

- Plans 04-01 through 04-08 are complete. Plan 04-09 Task 1 is incomplete;
  Task 2 and Plans 04-10 through 04-12 have not started.
- CPU suite: all 18 source expectations agree under both explicitly selected
  profiles, including independent guest probe/tuple evidence and fresh memory
  cases. These are software observations; O1 remains open.
- Timer suite: lynx1 completes at 359941 ticks with A1; lynx2 at 359916 with
  A2. Both compare 15/16; result offset 14 is 0C versus 0D.
- The earlier wrapper IRQ storm was separately fixed only for the saved
  post-call continuation after all source result stores. It preserves the
  pending latch and leaves in-suite interrupt frames unchanged.
- Current ordinary builds/runs reproduce the strict mismatch. Manifest,
  claim, whitespace and tree-hygiene checks pass at the checkpoint. This is
  not full-phase verification; the known ShellCheck issue remains.
- Executor edits remain uncommitted in the timer wrapper/manifest,
  build-suites.sh and test_public_suites.c. No pin or core timing was changed.
- Raw/comparison records are in ignored
  build/check/scratch/suites/manual-timers/build. Debug session and normalized
  detailed evidence are in .planning/debug/timer-suite-cascade-count.md and
  .planning/debug/evidence/timer-suite-cascade-count.md.

Until the owner chooses, preserve the strict comparison failure and Task 2
halt automatically. The owner acts next on the disposition; no acknowledgment
is needed merely to keep the existing stop in place.
