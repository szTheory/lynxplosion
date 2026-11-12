---
phase: 05-display-raw-video-and-input
plan: "05"
subsystem: input
tags: [logical-input, guest-bus, replay, sample-timing]

# Dependency graph
requires:
  - phase: 05-04
    provides: bounded guest-time/frame stop behavior used by edge-boundary tests
provides:
  - All 512 logical input masks checked through real guest CPU device reads in both Lefthand states
  - Limited SPRSYS input-bit policy and MAPCTL input overlay documented and covered
  - Exact CPU bus-grant sampling contract checked across adjacent ticks, DMA/refresh holds, frame completion, and watches
affects: [05-06, 05-07, 05-08, 05-09, 05-12]

# Actuals (#2632): chars/4 over the realized plan diff; not model/API usage.
actuals:
  tokens: 8179
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns: [original-guest-CPU-register-reads, independent-input-bit-tables, inventory-bound-CTest]

key-files:
  created: [docs/input-replay.md]
  modified:
    - core/input.c
    - core/machine.c
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/inventory/fast.txt
    - docs/claims/skeleton-repeatability.claim

key-decisions:
  - "Accept all nine-bit masks, including opposite directions and simultaneous Option 2 plus Pause; these remain independent logical levels."
  - "Sample input at the actual CPU read grant with zero additional sample pricing; changing that point requires a separately specified contract."
  - "Return disconnected cartridge lines as zero only under the explicitly unverified input-disconnected-zero-1 software policy."

patterns-established:
  - "Use original guest CPU reads and independently transcribed register tables to test input mapping."
  - "Apply host input changes at exact half-open guest-tick boundaries and record the actual bus sample tick."

requirements-completed: []

# Shared phase requirements remain pending until all declaring plans are complete.
coverage:
  - id: D1
    description: "Logical levels, both guest direction mappings, MAPCTL overlay behavior, and limited SPRSYS semantics are checked through guest reads."
    requirement: INP-01
    verification:
      - kind: unit
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^api\\.(input_bits|input_orientation|input_refusal)$'"
        status: pass
    human_judgment: false
  - id: D2
    description: "Guest reads observe input at the actual grant tick, including exact adjacent edges, retained bus requests, frame completion, and watch stops."
    requirement: API-06
    verification:
      - kind: integration
        ref: "ctest --preset check --no-tests=error --output-on-failure -R '^api\\.(input_edges|input_orientation|video_bus_edges)$'"
        status: pass
    human_judgment: false

# Metrics
duration: "38min (approx.; dispatch creation to completion, exact plan start not captured)"
completed: 2026-11-12
status: complete
plan_head_before: 7a42ad2729ee073ed99666c9005a296223153a2d
plan_head_after: a8d93203ecbab84903abd560fc8725413409560c
---

# Phase 5 Plan 5: Logical input and sample edges Summary

**Every logical input level now has independent all-mask guest-read coverage, and replay edges are pinned to the actual CPU bus grant tick.**

## Performance

- **Duration:** About 38 minutes from dispatch artifact creation to completion; the exact executor start was not separately captured.
- **Started:** Approximately 2026-11-11T23:17:57-05:00 (dispatch artifact creation time).
- **Task 2 commit:** 2026-11-12T04:55:08Z; metadata closeout commit: 2026-11-12T05:03:23Z. These are Git commit timestamps; the earlier completion timestamp incorrectly labelled local time as UTC.
- **Tasks:** 2.
- **Files modified:** 7.

The required GSD diff-size proxy counts 32,716 characters from `git diff 7a42ad2729ee073ed99666c9005a296223153a2d a8d93203ecbab84903abd560fc8725413409560c -- core/input.c core/machine.c tests/api/test_api.c tests/api/tests.cmake docs/input-replay.md tests/inventory/fast.txt docs/claims/skeleton-repeatability.claim`; dividing by four gives `actuals.tokens: 8179`. This measures the realized diff size for estimate calibration and is not model or API token usage.

## Accomplishments

- Added independent expected JOYSTICK/SWITCHES tables and exercised all 512 logical masks through real guest CPU reads with both guest Lefthand settings. Opposing directions and Option 2 plus Pause remain valid, separate levels.
- Covered guest SPRSYS bit 3, unsupported Suzy requests, MAPCTL `0C` device overlays, MAPCTL `0F` underlying RAM, and side-effect-free raw/mapped inspection.
- Proved exact sample ticks for edges at `T-1`, `T`, and `T+1`, including a pending read held across video DMA and refresh, a frame endpoint, and a write-watch return. Zero-budget advancement leaves the pending request and event time unchanged.
- Added [input replay documentation](../../../docs/input-replay.md) and stable mutation anchors for direction mapping, action mapping, and actual bus-grant sampling.
- Kept the CTest inventory sorted and rebound the repeatability claim to each committed inventory revision.

## Task Commits

Each task was committed atomically:

1. **Task 1: Make logical levels and guest orientation visible through real bus reads** - `a53d3a5` (`test`).
2. **Task 2: Observe exact input edges at a retained CPU request's sample tick** - `a8d9320` (`test`).

## Files Created/Modified

- `docs/input-replay.md` - Host replay timing, register mapping, limited Suzy support, and software-policy evidence limits.
- `core/input.c` - Mutation anchors around orientation and action mapping; runtime behavior already matched the tested contract.
- `core/machine.c` - Mutation anchor at the actual granted CPU input read; runtime behavior already sampled at that point.
- `tests/api/test_api.c` - Original guest programs, independent mapping expectations, exact-edge/hold/frame/watch coverage, and no-progress checks.
- `tests/api/tests.cmake` and `tests/inventory/fast.txt` - Registered `api.input_bits`, `api.input_orientation`, and `api.input_edges` in the exact inventory.
- `docs/claims/skeleton-repeatability.claim` - Bound to the final inventory digest `9d02511a3af61a64ae8b59060afa50f9e8321611424d01db127112b91ed5ae94`.

## Decisions Made

No new owner decision was introduced. The plan's declared logical mappings and current sample-at-grant policy were exercised as software behavior. Disconnected cartridge lines remain fixed zero under an explicitly unverified policy; this is not hardware qualification.

## Verification

- Task 1 focused command passed `api.input_bits`, `api.input_orientation`, and `api.input_refusal` (3/3; final per-task run wall time 0.85 s).
- Task 2 focused command passed `api.input_edges`, `api.input_orientation`, and `api.video_bus_edges` (3/3; wall time 0.45 s).
- Final focused input/video-edge regression passed `api.input_bits`, `api.input_edges`, `api.input_orientation`, `api.input_refusal`, and `api.video_bus_edges` (5/5; wall time 0.76 s), after a successful configure/build.
- `sh scripts/check-claims.sh` and `LC_ALL=C sort -c tests/inventory/fast.txt` passed.

Both TDD tasks were run against the existing production behavior before source behavior changes. No semantic RED was found, so no production behavior was altered. Task 1 baseline identity was HEAD `7a42ad2729ee073ed99666c9005a296223153a2d`, `core/input.c` SHA-256 `e62e8cf61b172144430e5fb6e958c4bcfc49a88ca96bdff265faf049b9b15049`, and `core/machine.c` SHA-256 `e86b70ff8c73e5db47252ce53b7f3e5cc6587444a5941247039644303d595de9`. Task 2's first focused run was at HEAD `e4ec0c7f2e5e6c9a7c6222bdbaefedc67b5a87d3`, with `core/input.c` SHA-256 `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` and `core/machine.c` SHA-256 `b9e37062e2899aa7b5a6a5b6480cefe276f50229452d4a956a7a96648d567625`; those files differed from the earlier identity only by comments/anchors. The final combined run after the Task 1 metadata amendment used build identity `git=a53d3a53c696575eaa3d2e5445f019ac53879a77+dirty`, AppleClang 21.0.0.21000101, Darwin-arm64.

## Requirement Status

The plan frontmatter repeats `INP-01`, `API-06`, and `HB-02` across sibling Phase 5 plans. The read-only `requirements.ready-ids` check returned no ready IDs and blocked all three because sibling plans remain unfinished. This plan proves the current input register mapping and sample boundary; it does not implement the canonical host timeline or the recorded input-responsive homebrew. Their requirement checkboxes remain pending until the owning plans complete. `HB-02` has no deliverable in this plan.

## Issues Encountered

- An initial orientation assertion ran before the guest reached its SPRSYS read. The test now waits for the real pending guest read before asserting orientation; the focused suite passes against unchanged runtime behavior.
- `sh scripts/check-claims.sh --inventory-sha256` prints a digest rather than editing the claim. The claim was updated explicitly and verified against each task's inventory.

## Deviations from Plan

None. The plan executed within its scope, and the temporary claim-binding correction was included in the appropriate task commits.

## Next Phase Readiness

The input contract and exact bus-sample edges are ready for the serialization, replay-host, mutation, and final qualification plans. EVD-02 remains pending its Phase 5 timer offset-14 comparison; EVD-05 remains partial pending later device-mutation closure; CON-01 remains partial pending the original recorded-input homebrew and consumer evidence. Hardware accuracy, firmware/commercial content, physical testing, shared LibFuturo changes, publication, and release remain outside this plan.

None - no external service configuration required.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists.
- Task commits `a53d3a5` and `a8d9320` are ancestors of HEAD.
- No missing files or commits.
