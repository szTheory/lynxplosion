---
phase: 05-display-raw-video-and-input
plan: "06"
subsystem: core-api
tags: [C17, snapshots, video, input, sha256, ctest]
requires:
  - phase: 05-display-raw-video-and-input
    provides: "Format 5 video/input state, raw frame capture, timer-driven video policy"
provides:
  - "Reachability validation for complete format 5 restore candidates"
  - "Public canonical guest-state digest including timer, bus, video and input state"
  - "Runner state observations and cross-path raw frame determinism coverage"
affects: [runner-observations, determinism, host-replay-contract]
actuals:
  tokens: 16368 # UTF-8 changed-content characters / 4 over the measured plan diff; see method below
  tasks: 2
  commits: 5 # Measured: base..plan_head_after; metadata closeout commit is separate
commits: 5
plan_head_before: d04e395f116a22b7dfa972783fd544a5438ed873
plan_head_after: 0135ca3dfff5a50b5e9d2230ca3a8704e9deef3f
tech-stack:
  added: []
  patterns:
    - "Version-tagged fieldwise SHA-256 over dynamic guest state"
    - "Compare complete raw frame packets and guest progress separately from run identity"
key-files:
  created:
    - docs/snapshot-video.md
    - .planning/phases/05-display-raw-video-and-input/05-06-task2-red.json
  modified:
    - core/snapshot.c
    - include/lynxplosion.h
    - runner/bundle.c
    - tests/det/test_det.c
    - tests/api/test_api.c
    - tests/golden/skeleton.digest
key-decisions:
  - "Guest-state digest uses the version tag lynxplosion.guest-state.v1 and fieldwise bytes, omitting immutable identity, padding, derived requests, watches and replay scripts."
  - "The CPU-loop golden was rebaselined only for the added obs.state.hex stream; its workload and prior RAM/register/time observations are unchanged."
  - "VID-04, VID-05, INP-01 and API-06 remain unchecked because requirements.ready-ids reports 0/4 ready while sibling Phase 05 obligations remain open."
patterns-established:
  - "Canonical state evidence is a public digest; hosts compare progress, input samples and raw frame packets as separate observations."
requirements-completed: []
coverage:
  - id: D1
    description: "Format 5 restores reachable ongoing video capture and refuses rehashed impossible candidates atomically."
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#api.video_snapshot"
        status: pass
      - kind: unit
        ref: "tests/api/test_api.c#api.video_snapshot_refusal"
        status: pass
    human_judgment: false
  - id: D2
    description: "Canonical guest state includes input/video/bus data and complete raw frames agree across slicing, watches, restores and interleaved instances."
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^(det\\.(video_paths|video_sensitivity|golden|golden\\.self_test)|api\\.(state_digest|header_selfcontained))$'"
        status: pass
      - kind: integration
        ref: "17 directly affected API, runner and determinism CTests"
        status: pass
    human_judgment: false
duration: 61min
completed: 2026-11-12
status: complete
---

# Phase 5 Plan 06: Format 5 Restore and Video Path Summary

**Reachable format 5 states restore atomically, and versioned guest digests preserve input-aware raw frame observations across sliced, watched and restored execution.**

## Performance

- **Duration:** 61 min
- **Started:** 2026-11-12T05:20:27Z
- **Completed:** 2026-11-12T06:21:43Z
- **Tasks:** 2
- **Files modified:** 19 plan artifacts and source/test files
- **Actuals method:** `git diff --no-ext-diff --unified=0 <plan_head_before>..0135ca3`, count added and removed UTF-8 content characters (excluding diff prefixes and headers), divide by 4; 65,473 characters / 4 = 16,368 tokens. The plan head range is recorded above. This is a diff-size proxy, rounded down in the recorded field, and does not measure model or API token usage.

## Accomplishments

- Completed format 5 hostile-state validation for reachable CPU readiness, timer cursor, video DMA/refresh ownership, page continuation, input fields, captured rows and zeroed unused tails. Valid boundary restores reproduce the next raw frame; invalid rehashed candidates preserve machine, watch and caller state.
- Added `lynx_state_digest`, a fixed 32-byte fieldwise SHA-256 API with atomic refusal for invalid, wrapping or overlapping output spans. The runner now records the digest in `obs.state.hex` and includes it in the repeatability observation digest.
- Added `det.video_paths` and `det.video_sensitivity`: full pen/palette/line-tick packets, frame metadata, progress and guest input samples agree across unsliced, sliced, watched/rearmed, mid-blank restore, mid-row restore and interleaved paths. Distinct input masks produce distinct raw pen output.

## TDD Evidence

- **Task 1 RED:** `api.video_snapshot` failed at the planned restore assertion because a reachable overdue refresh waiting behind active DMA was rejected. The unmodified TAP report passed `gsd_run check tdd-red-evidence` as `RED_EVIDENCE_OK`; see `05-06-task1-red.json`.
- **Task 1 refusal RED:** `api.video_snapshot_refusal` showed a rehashed snapshot with simultaneous future DMA and refresh owners was accepted and cleared the target watch. The TAP target passed the classifier as `RED_EVIDENCE_OK`; see `05-06-task1-refusal-red.json`.
- **Task 2 RED:** `api.state_digest` showed that changing only the current input mask left the existing runner observation unchanged. Its TAP report passed the classifier as `RED_EVIDENCE_OK`; see `05-06-task2-red.json`.
- **GREEN:** Both task implementations pass the named semantic assertions. The digest test also verifies equal-state agreement, watch exclusion, input sensitivity, invalid pointer-span refusal and unchanged output/machine state on refusal.

## Verification

- Task 1 owning command passed: `cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(api\.(video_snapshot|video_snapshot_refusal|snapshot_refusal|timer_snapshot_controls))$'`.
- Final focused command passed 6/6: build, `api.state_digest`, `api.header_selfcontained`, both video determinism tests, `det.golden`, and its negative control.
- 17 directly affected CTests passed, including snapshot continuation/refusal, timer snapshot controls, two-run and two-instance bundle checks, profile agreement, golden, runner CLI and manifest controls.
- `sh scripts/check-claims.sh` passed. The sorted inventory SHA-256 is `a3c690c0a94302a69528112d785f1cec2f6e9d34b3c05dc6837d92425cb2ed16` and matches the claim.

## Task Commits

1. **Task 1 RED:** `f9dc36c` — failing test for reachable queued video snapshot restore.
2. **Task 1 refusal RED:** `d3a1b32` — refusal controls for impossible video snapshot owners.
3. **Task 1 GREEN:** `d7e2cbe` — validate reachable format 5 video snapshot state.
4. **Task 2 RED:** `f9946c2` — failing test for input-sensitive state observation.
5. **Task 2 GREEN:** `0135ca3` — public guest digest and complete video path observations.

## Decisions Made

- The canonical digest uses the NUL-terminated domain tag `lynxplosion.guest-state.v1`. It hashes CPU fields, MAPCTL/page latch, guest time/readiness, RAM, timer state and current-tick event status, bus hold, then the full format 5 video record. It omits build/content/profile/RAM identity, C padding, derived CPU request/wake fields, host watches, pointers and replay data. See `docs/snapshot-video.md` for exact field order and widths.
- The prior CPU-loop observation golden was intentionally rebaselined from `b8bc5a...` to `b260934...` because `obs.state.hex` expands the observation schema. API and video sensitivity controls prove the new stream is nonvacuous; the fixture, workload and prior guest projections are unchanged.
- No replay/checkpoint wrapper exists in the core or runner. Documentation now requires any host wrapper to carry the pre-edge/post-edge marker and full script digest; the core snapshot stores only the current mask.
- Requirement IDs remain unchecked: `requirements.ready-ids` returned 0/4. Broader Phase 05 requirements, EVD-02, recurring EVD-05 and CON-01 remain pending their sibling evidence.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated bundle-file consumers for the new state observation**
- **Found during:** Task 2
- **Issue:** The bundle format gained `obs.state.hex`, while the bundle header and two-run test still described and required the old four-file list.
- **Fix:** Updated the bundle contract and deterministic two-run inventory/comparisons so every digest-covered file is checked.
- **Files modified:** `runner/bundle.h`, `tests/det/two_runs.cmake`
- **Verification:** `det.two_runs` and the 17-test directly affected regression set passed.
- **Committed in:** `0135ca3`

The golden rebaseline was explicitly allowed by the plan when accompanied by a semantic reason and sensitivity proof; it is documented above. No other plan deviations.

## Issues Encountered

- The first video-path test fixture wrote its display byte to CPU-unmapped `$FC00`, so advancement correctly stopped with `LYNX_STOP_UNAVAILABLE`. The fixture was corrected to use mapped RAM at `$0400`; the new video path and input controls then passed.
- The first focused run against the old observation golden reported the expected schema mismatch. The golden was updated only after the new state digest and sensitivity checks passed.

## Known Stubs

| File | Line | Reason |
|---|---:|---|
| `include/lynxplosion.h` | 11, 87 | Existing flat-five CPU timing prose remains explicitly provisional; this plan does not qualify hardware CPU timing. Already tracked as open entry 1 in `.planning/WINDOWS.md`; it does not block the video snapshot/digest goals. |

## User Setup Required

None.

## Next Phase Readiness

Plan 05-06 is ready to be consumed by Plan 05-07. The local build and focused/directly affected tests pass. VID-04, VID-05, INP-01 and API-06 stay unchecked until all linked Phase 05 plans finish; EVD-02, EVD-05 and CON-01 remain open. O1, O5, O6 and O9 remain unchanged, and this plan used no firmware, commercial ROMs, physical hardware or external publication.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*

## Self-Check: PASSED

The summary, snapshot-video documentation and all three RED evidence records exist. All five task commits are ancestors of HEAD. The persisted plan ledger measures five task commits from `d04e395f116a22b7dfa972783fd544a5438ed873` through `0135ca3dfff5a50b5e9d2230ca3a8704e9deef3f`.
