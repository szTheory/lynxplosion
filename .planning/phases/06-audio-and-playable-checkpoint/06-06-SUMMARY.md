---
phase: 06-audio-and-playable-checkpoint
plan: "06"
subsystem: testing
tags: [c, audio, snapshot-format-7, deterministic, av-continuation, clang, gcc]

requires:
  - phase: 06-05
    provides: Caller-owned A/V spans, ordered capacity stops, and snapshot-ready audio state
provides:
  - Fieldwise format-7 audio snapshot validation and exact-build audio policy documentation
  - Complete stream comparisons across sliced, watched, interleaved, and restored guest execution
  - Deferred-ring continuation and output-capacity frontier evidence using the existing source-built A/V guest
affects: [phase-06-continuation, DET-05, AUD-02]

actuals:
  tokens: 23293 # 93,170 changed diff characters / 4; rounded up
  tasks: 2
  commits: 2 # task commits measured from plan_head_before to plan_head_after
  plan_head_before: 60a951a60dfca8970683bc4342471ff24aed1467
  plan_head_after: 2511cb7fabccf52e4e3212cad6eaf60524a2b947

tech-stack:
  added: []
  patterns:
    - Manifest-bound reuse of the pinned ca65/ld65 A/V fixture recipe
    - Fieldwise comparison of complete A/V streams at common guest ticks

key-files:
  created: []
  modified:
    - core/snapshot.c
    - tests/audio/test_audio.c
    - tests/audio/tests.cmake
    - tests/det/test_det.c
    - tests/det/tests.cmake
    - tests/inventory/fast.txt
    - docs/snapshot-audio.md
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Keep format 7 and the exact-build snapshot policy; validate reachable audio phase and ring state before applying a candidate."
  - "Treat event sequence zero as reachable when the current-tick cursor is open, even before an event is emitted."
  - "Keep the input-script cursor outside the guest snapshot and compare complete observation records instead of C structure padding."

patterns-established:
  - "Snapshot candidates are validated fieldwise before any instance or watch mutation."
  - "Continuation comparisons cover input edges, raw frames, PCM ordinals and values, event fields, and final guest time."

requirements-completed: [DET-05, AUD-02]

coverage:
  - id: D1
    description: "Format-7 audio candidates reject malformed reachable state before apply and round-trip accepted phase and deferred-ring boundaries."
    requirement: AUD-02
    verification:
      - kind: unit
        ref: "ctest: audio.snapshot, audio.snapshot_refusal, api.snapshot_refusal, api.snapshot_roundtrip, api.timer_snapshot_controls, api.video_snapshot_refusal"
        status: pass
    human_judgment: false
  - id: D2
    description: "The existing input-responsive A/V guest produces equal complete streams across execution, watch, interleaving, input-edge, sample-boundary, capacity-stop, and restore paths."
    requirement: DET-05
    verification:
      - kind: integration
        ref: "ctest: det.audio_paths and det.audio_sensitivity on AppleClang 21 and GNU 13.3"
        status: pass
      - kind: integration
        ref: "ctest: det.golden, det.golden.self_test, det.video_paths, det.video_sensitivity, api.header_selfcontained"
        status: pass
    human_judgment: false

duration: 75min
completed: 2026-11-13
status: complete
---

# Phase 6 Plan 06: Audio State Continuation Summary

**Format-7 audio snapshots now reject unreachable state before apply, and the source-built A/V guest has stream-by-stream continuation coverage across execution and restore paths.**

## Performance

- **Duration:** 75 minutes
- **Started:** 2026-11-13T02:47:07Z
- **Completed:** 2026-11-13T04:02:21Z
- **Tasks:** 2
- **Files modified:** 8 source, test, inventory, and claim files

## Accomplishments

- Completed fieldwise format-7 audio validation and documented the 160-byte audio record, exact-build audio policy, sample cadence, deferred-ring bounds, and event cursor rules. Accepted controls include the reachable event-sequence-zero state.
- Built the original `fixtures/diagnostic/av_probe.s` with the pinned ca65/ld65 recipe for the deterministic test harness. The manifest-bound BLL and linked `probe_result` symbol are checked; the temporary object is removed after linking.
- Compared complete logical input, raw frame/palette/line-tick, PCM ordinal/value, and audio-event streams across whole, coarse, one-tick, irregular, watched, interleaved, and snapshot-restored execution. Checkpoints cover both sides of the tick 109321 input edge and ticks 399, 400, and 401.
- Exercised an eight-event device prefix plus one sample in the 13-event/one-sample caller spans. The CPU strobe waits with five event slots left, then resumes once at event sequence 8; a format-7 restore continues the deferred ring stream without loss or duplication.
- Added independent sensitivity controls for LFSR, integrator, channel counter, deferred pulse, sample phase, logical input, frame bytes and metadata, PCM values and ordinals, event fields, and guest progress. Nonempty prefixes are checked for all record mutations; zero-output, one-sample, and one-event suffixes are explicit cases.
- Registered `det.audio_paths` and `det.audio_sensitivity`, refreshed only the inventory digest after its existing repeatability controls passed, and kept the committed golden unchanged.

## Task Commits

1. **Task 1: Reject malformed audio candidates and bind immutable output policy** — `0dc6178` (`fix`)
2. **Task 2: Compare every observation stream across complete execution and restore variants** — `2511cb7` (`test`)

The separate guarded summary commit follows these task commits.

## Files Created/Modified

- `core/snapshot.c` — Format-7 audio candidate checks before apply.
- `tests/audio/test_audio.c`, `tests/audio/tests.cmake` — Malformed and accepted audio snapshot controls.
- `tests/det/test_det.c`, `tests/det/tests.cmake` — Manifest-bound fixture build, continuation paths, capacity/ring restore, and sensitivity controls.
- `tests/inventory/fast.txt` — Registered the two deterministic audio tests.
- `docs/snapshot-audio.md` — Format-7 fields and continuation boundaries.
- `docs/claims/skeleton-repeatability.claim` — Refreshed only `inventory_sha256` after the named evidence controls.

## Decisions Made

- Kept format 7, the 40 kHz stereo S16 policy identity, and exact-build restore behavior. No migration or physical-hardware claim was added.
- Preserved host replay-script position outside the snapshot and represented pre-edge and post-edge checkpoints explicitly in the host test.
- Used the existing source-built `av_probe` fixture and its pinned recipe. Synthetic internal state is limited to the otherwise-unreachable capacity and deferred-ring boundary setup.

## Verification

- **Task 1, AppleClang 21 and GNU 13.3:** the six-case snapshot/refusal selector passed at Task 1 commit `0dc6178`.
- **Task 2, AppleClang 21:** `cmake --preset check`, `cmake --build --preset check`, and the exact seven-test selector passed 7/7: `det.audio_paths`, `det.audio_sensitivity`, `det.golden`, `det.golden.self_test`, `det.video_paths`, `det.video_sensitivity`, and `api.header_selfcontained`.
- **Task 2, GNU 13.3/Linux aarch64:** reconfigured and built the exact full commit `2511cb7fabccf52e4e3212cad6eaf60524a2b947`; the same seven-test selector passed 7/7. The reused checkout remained detached with a clean tracked tree, and the generated `av_probe.o` was absent after the build.
- **Inventory binding, AppleClang 21:** `det.two_runs`, `det.two_instances`, and `det.golden` passed 3/3 before the hash-only refresh; `sh scripts/check-claims.sh` passed afterward.
- The initial `det.audio_paths` run exposed an exclusive-target mismatch at frame completion. All tick-target variants now advance one tick beyond the captured frame boundary before comparison; the focused case and final selectors pass.

## Deviations from Plan

The plan scope and interfaces were preserved. Two small correctness fixes were made during Task 1:

### Auto-fixed Issues

**1. [Rule 1 - Bug] Preserve the reachable event-sequence-zero snapshot state**
- **Found during:** Task 1 (snapshot refusal controls)
- **Issue:** A current-tick event cursor may be open with sequence zero before any output transition; treating that state as malformed rejected a reachable continuation.
- **Fix:** Keep sequence zero valid when the event cursor refers to the current guest tick, add an accepted round-trip control, and document the rule.
- **Files modified:** `core/snapshot.c`, `tests/audio/test_audio.c`, `docs/snapshot-audio.md`
- **Verification:** The six-case snapshot/refusal selector passed on AppleClang 21 and GNU 13.3.
- **Committed in:** `0dc6178`

**2. [Rule 3 - Blocking] Add the SHA-256 declarations required by snapshot checksum controls**
- **Found during:** Task 1 (Mac build)
- **Issue:** The test recomputed payload hashes but lacked the header declaring the internal SHA-256 helpers, so the AppleClang build failed.
- **Fix:** Include the existing SHA-256 header; no dependency or helper implementation was added.
- **Files modified:** `tests/audio/test_audio.c`
- **Verification:** The build and six-case snapshot/refusal selector passed on AppleClang 21; the selector also passed on GNU 13.3.
- **Committed in:** `0dc6178`

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking build issue)
**Impact on plan:** Both fixes were needed to accept reachable snapshots and compile the planned controls. They did not expand the plan scope.

## Issues Encountered

- Tick-target calls use an exclusive target, so comparing them at the frame-stop tick omitted the frame-boundary event. Advancing each path through the next tick produced the same complete frame record and common machine state.
- The first synthetic snapshot candidate used an overdue video refresh deadline. It was corrected to a valid future phase before rerunning the control; the format-7 snapshot restore then passed.

## User Setup Required

None — the pinned tools and GNU reuse container were already available.

## Next Phase Readiness

Plan 06 closes its format-7 validation and deterministic continuation evidence. Plan 07 can proceed using the documented A/V span contract and source-built fixture. Physical hardware accuracy, owner rights decisions, release authority, and any phase-wide compatibility qualification remain outside this plan.

## Self-Check: PASSED

- Summary file exists at the required phase path.
- Task 1 (`0dc6178`) and Task 2 (`2511cb7`) commits are ancestors of the current source head.
- `git diff --check` passed before the guarded summary commit.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-13*
