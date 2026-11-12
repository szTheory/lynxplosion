---
phase: 05-display-raw-video-and-input
plan: "08"
subsystem: testing
tags: [c, cmake, ctest, replay, raw-video, publication, semantic-controls]
requires:
  - phase: 05-display-raw-video-and-input
    provides: Canonical input replay, source-bound diagnostic frames, snapshots, and independent artifact checks
provides:
  - Hostile timeline, path, alias, symbol, identity, and publication refusal controls
  - Exclusive owner-verified frame staging and complete-directory publication
  - Source-built no-progress and no-response controls with distinct semantic diagnostics
affects: [05-display-raw-video-and-input, consumer, input, raw-video, publication, evidence]
actuals:
  tokens: 12402
  tasks: 2
  commits: 4
plan_head_before: 981527f662b7a929aab0bcce79450b582dd518c7
plan_head_after: 42aea18b57a881cd50d1e80d92c5ddbaee592137
tech-stack:
  added: []
  patterns:
    - Preflight file identity and symlink containment before reading or writing artifacts
    - Publish only complete output directories owned by a verified invocation marker
    - Bind guest-response mutants to source, tool, driver, object, BLL, and labels identities
key-files:
  created: []
  modified:
    - consumer/main.c
    - tests/consumer/frame_driver.cmake
    - tests/consumer/check_artifacts.c
    - tests/consumer/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim
key-decisions:
  - "Reject same-file aliases by device/inode and refuse symlink traversal in output parents before artifact creation."
  - "Keep staging ownership tied to a per-invocation marker and cached directory identity; publish only after the whole bundle is complete."
  - "Diagnose malformed result schema, stalled guest progress, missing input response, and rendered-color mismatch independently."
  - "Keep Task 2 baseline-green evidence distinct from a comparative failure on the reconstructed pre-plan consumer."
patterns-established:
  - "Construct guest behavior controls only in owned scratch from the project fixture and pinned assembler/linker, and record source/build identities."
  - "Bracket negative controls with fresh positive guest runs and require the intended semantic diagnostic."
requirements-completed: []
coverage:
  - id: D1
    description: "Malformed host inputs and competing or interrupted publishers refuse safely while preserving existing and outside files."
    requirement: "HB-02"
    verification:
      - kind: integration
        ref: "consumer.replay_controls, consumer.frame_publication_controls, consumer.refusal_controls"
        status: pass
    human_judgment: false
  - id: D2
    description: "Fresh source-built no-progress and unchanged-input guests fail their named semantic checks while responsive replay remains valid."
    requirement: "CON-01"
    verification:
      - kind: integration
        ref: "consumer.frame_response_controls, consumer.frame_replay"
        status: pass
    human_judgment: false
duration: 23min
completed: 2026-11-12
status: complete
commits: 4
---

# Phase 05 Plan 08: Hostile Replay Inputs and Semantic Response Controls Summary

**Replay now rejects aliased or hostile host paths before side effects, publishes frames from an invocation-owned complete bundle, and distinguishes guest progress/input response failures from malformed results and rendered-color faults.**

## Performance

- **Duration:** 23 minutes, measured from the first task commit through final focused verification and closeout preparation.
- **Started:** 2026-11-12T03:52:27-05:00
- **Completed:** 2026-11-12T04:15:57-05:00
- **Tasks:** 2
- **Files modified:** 6 implementation, test, inventory, and claim files.
- **Actuals method:** The plan diff contains 49,608 changed-content bytes (added and removed line bodies, with line endings), divided by four for a reproducible 12,402-token source-size proxy. This is not model/API usage.

## Accomplishments

- Added exact refusal controls for malformed and oversized timelines, duplicate or invalid linked symbols, source/content/profile/policy/script identity mismatches, same-file aliases, symlinked destinations, and existing outputs. Refusals produce no success output and preserve existing targets.
- Hardened frame publication with verified staging ownership, exclusive publication, injected write/close/prepublish failure cases, and cleanup checks that retain ownerless decoys and outside sentinels.
- Added source-built no-progress, unchanged-input, malformed-schema, and rendered-color controls. Each behavior-negative requires a successful pinned build, exact diagnostic, and fresh positive guest runs around it.
- Preserved the existing tracer, nine-input replay, replay edge/restore, snapshot, and silence controls; rebound the fast inventory claim digest after registering the new cases.

## Task Commits

1. **Task 1: Reject malformed host inputs and preserve exclusive publication ownership**
   - `a038278` — add replay hostility and publication controls
   - `ace3559` — check replay identity and rendered observations
   - `1559c57` — harden replay inputs and frame publication
2. **Task 2: Prove semantic responsiveness rejects no-run and unchanged-input scenarios**
   - `42aea18` — add source-bound response mutants

## Files Created/Modified

- `consumer/main.c` — path preflight, stage ownership, cleanup, and specific guest-observation diagnostics.
- `tests/consumer/frame_driver.cmake` — hostility, publication interruption, and source-built response controls.
- `tests/consumer/check_artifacts.c` — independent identity, raw-response, rendered-color, and complete-bundle checks.
- `tests/consumer/tests.cmake` — registered the plan's focused CTest cases.
- `tests/inventory/fast.txt` — kept the exact sorted fast test inventory.
- `docs/claims/skeleton-repeatability.claim` — rebound the inventory digest without changing its profile or scope.

## Verification

- `cmake --preset check` and `cmake --build --preset check` passed.
- Task 1 gate passed: `ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(replay_controls|frame_publication_controls|refusal_controls)$'`.
- Task 2 gate passed: `ctest --preset check --no-tests=error --output-on-failure -R '^consumer\.(frame_response_controls|frame_replay)$'`.
- Direct predecessor regressions passed: `consumer.frame_replay`, `consumer.frame_tracer`, `consumer.replay_edges`, `consumer.replay_restore`, `consumer.snapshot_ppm`, and `consumer.silence_wav`.
- `sh scripts/check-claims.sh --inventory-sha256` confirmed inventory digest `bd99829a259802406860f196faa460d2a4b0ceb2087d3b8261a77516a229c270`; `sh scripts/hygiene.sh --tree` and `sh scripts/hygiene.sh --history 981527f..42aea18` passed.

## TDD Gate Compliance

- **Task 1:** The registered alias assertion was run against the exact pre-plan `consumer/main.c` from `981527f` (SHA-256 `608c31855230652d1cf9f76b4d1618c915ce5aa6192cf958708c225748da7534`) in owned scratch. The rights-clean fixture build succeeded; the intended same-file alias assertion failed because the old consumer reached generic timeline parsing instead of rejecting the alias during preflight. The raw report stayed in private temporary storage; its semantic assessment is recorded in the owned scratch `red-evidence.json`. The current target was restored byte-for-byte.
- **Task 2:** At its actual start, commit `1559c57` already contained the semantic diagnostics and the new assertions passed. This is baseline-green coverage, not RED evidence or a claim that tests preceded implementation. A separately reconstructed pre-plan consumer (same source SHA above) was used only as a comparative negative control; the original project fixture identity was SHA-256 `e2ed6a86f63c1eafcd2f45cefe2706d6cd4081469c34bbcc38c6de2bf8b2e2ca`. The three private mutant builds record their own source/config/tool/driver/object/BLL/labels hashes under ignored CTest scratch.

## Decisions Made

- Preflight device/inode identity and symlink containment before creating output, so alternate spellings of one file cannot pass as distinct inputs.
- Remove staging only after checking the invocation marker and cached directory identity; publish the complete bundle with exclusive directory rename.
- Report the point of failure separately for result schema, guest progress, logical-input response, raw-frame evidence, and rendered PPM color.
- Keep shared requirement IDs `HB-02`, `CON-01`, `API-06`, and `EVD-05` partial: `requirements.ready-ids` reports all four blocked by remaining Phase 05 plans.

## Deviations from Plan

None. Task 2 was green on its actual starting baseline; this chronology is recorded above rather than manufacturing a RED result.

## Issues Encountered

An initial raw-versus-rendered fault assertion still expected the older combined diagnostic. The checker order and expected text were aligned so raw guest-response corruption and rendered-color corruption remain independently diagnosed; the focused regression set then passed.

## Known Stubs

- `consumer/main.c:682` retains the explicitly labeled placeholder PCM silence pending Phase 6 audio. Plan 05-08 does not introduce or rely on it for the frame/input result; it remains an intentional future-phase stub, recorded as open entry 6 in `.planning/WINDOWS.md`.

## Next Phase Readiness

The remaining Phase 05 plans can use the stricter replay host boundary and semantic response checks. No owner gate was closed and no hardware-accuracy claim was added. The Phase 6 audio plan still owns replacing placeholder silence.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*

## Self-Check: PASSED

All six modified implementation/test/inventory/claim files and this summary exist. Task commits `a038278`, `ace3559`, `1559c57`, and `42aea18` are ancestors of the current HEAD.
