---
phase: 05-display-raw-video-and-input
plan: "03"
subsystem: timing
tags: [C17, CMake, CTest, video-dma, refresh, MAPCTL, snapshots]

requires:
  - phase: 05-02
    provides: deterministic video, timer, and event scheduling base
provides:
  - Exclusive video, refresh, and CPU bus grants with retained posted CPU requests
  - A conservative, explicitly unverified page-mode fetch subset under VIDEO_POLICY1
  - Format-5 snapshot persistence for page continuation without changing payload size
affects: [05-04, 05-09, 05-10, 05-11, 05-12]

actuals:
  tokens: 11354 # 45,414 realized-diff characters / 4, rounded up
  tasks: 2
  commits: 5
commits: 5
plan_head_before: 0140d06581eaa801e3d5d45044d756707fa230b0
plan_head_after: f94771f4ccad645c70aa5296afeec7dce5894dc7

tech-stack:
  added: []
  patterns:
    - One deterministic scheduler owns video, refresh, and CPU grant ordering
    - Page continuation is private machine state and is validated with snapshots

key-files:
  created: [docs/bus-video-policy.md]
  modified: [core/bus.c, core/bus.h, core/internal.h, core/machine.c, core/snapshot.c, core/video.c, tests/api/test_api.c, tests/api/tests.cmake, tests/inventory/fast.txt, docs/claims/skeleton-repeatability.claim]

key-decisions:
  - "MAPCTL B7 set disables sequential cycles; only B7-clear candidates are considered."
  - "Whitelist only the shared NOP opcode for this first software page-mode policy because primary documentation does not enumerate all eligible decodes."
  - "Keep the flat5 lane and its cost table unchanged; serialize the page latch in the existing format-5 byte slot."

requirements-completed: [VID-02]

duration: 35min
completed: 2026-11-11
status: complete
---

# Phase 05 Plan 03: Exclusive Video Bus and Page-Mode Pricing Summary

Video and refresh grants now serialize ahead of CPU requests, and VIDEO_POLICY1 applies four-tick pricing only to a narrow, tested same-page NOP fetch case.

## Performance

- Duration measured from the first task commit through closeout: about 35 minutes; initial reading and setup happened before that timestamp.
- Task 2 focused suite: 13/13 tests passed in 2.78 seconds total CTest wall time.
- The task diff was 45,414 characters; the recorded estimate-scale actual is characters divided by four.

## Accomplishments

- One scheduler grants video before refresh before CPU. A posted CPU request retains its address, data, flags, and request kind through bounded ownership holds. Video rows and refresh catch-up remain bounded.
- The primary Epyx reference identifies MAPCTL B7 as sequential-disable, so bit 7 set disables page mode. Since the CPU chapter says opcode decode can veto eligibility but does not list every eligible decode, this policy considers only the common NOP opcode. That whitelist and the five-tick video/refresh schedule are unverified software choices; no suite result was used to tune them.
- Video or refresh grants close the page continuation. The latch is serialized in the existing format-5 payload byte slot, keeping its size fixed and preserving exact-build compatibility checks. The flat5 timing table and lane remain unchanged.
- Both Lynx CPU profiles, enabled and disabled page mode, ordinary CPU cycles, page crossings, video/refresh closure, snapshot continuation, invalid policy refusal, and flat5 controls are covered by focused tests.
- The fast inventory and claim digest are synchronized. Inventory SHA-256: 9656287c0febcf3bdfe5d6196f172351b3f5318cf4f6a54533c511987cdf68b0.

## Verification

- Task 1: cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_bus|video_bus_edges|request_hold|request_wake|request_boundary)$' — 5/5 passed.
- Task 2 and related seams: cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^api\.(video_page_mode|video_bus|video_bus_edges|request_hold|request_wake|request_boundary|cost_model|cost_model\.self_test|snapshot_refusal|snapshot_roundtrip|snapshot_continue|timer_snapshot|timer_snapshot_controls)$' — 13/13 passed on AppleClang 21, Darwin arm64.
- sh scripts/check-claims.sh --inventory-sha256 — matched the recorded digest above.
- gsd_run check tdd-red-evidence .planning/phases/05-display-raw-video-and-input/05-03-task2-red.json --raw — RED_EVIDENCE_OK.

## Task Commits

1. Task 1 RED: b100e95 — added failing exclusive video-bus assertions.
2. Task 1 GREEN: ec8ba06 — serialized video and refresh grants.
3. Task 2 initial scaffold: 8cfb48d — added page-mode cycle assertions; its MAPCTL polarity was wrong and is superseded below.
4. Task 2 corrected scaffold: 068f536 — corrected polarity and hand-derived trace, then captured the valid baseline RED.
5. Task 2 GREEN: f94771f — priced the conservative page-mode fetch subset.

## TDD Evidence and Provenance

The final Task 2 RED record names the exact source identity. The test ran after restoring core/bus.c, core/bus.h, core/internal.h, core/machine.c, and core/snapshot.c byte-for-byte from Task 1 production commit ec8ba06; git diff --exit-code confirmed those production files were unchanged. The corrected test scaffold was at 068f536. On that baseline, ordinary five-tick controls passed while eligible four-tick fetch assertions failed under both profiles. The record was accepted by the TDD evidence checker.

An earlier scaffold at 8cfb48d reversed MAPCTL B7 polarity. It remains in history but is explicitly superseded; it is not evidence of the source baseline. An intermediate controlled no-discount mutant run is also excluded from baseline evidence. The exact baseline RED is the restored ec8ba06 production source described above.

## Files Created or Modified

- docs/bus-video-policy.md — records exclusive bus grants, unverified numeric schedules, narrow page eligibility, stream closure, and snapshot latch policy.
- core/video.c and core/machine.c — serialize bounded video/refresh requests with CPU grants and preserve held requests.
- core/bus.c and core/bus.h — decide whether a fetch is in the conservative page-mode subset.
- core/internal.h and core/snapshot.c — retain and validate the private page continuation.
- tests/api/test_api.c and tests/api/tests.cmake — hand-derived assertions and exact registration for the new cases.
- tests/inventory/fast.txt and docs/claims/skeleton-repeatability.claim — registered test inventory and matching claim identity.
- .planning/phases/05-display-raw-video-and-input/05-03-task1-red.json, 05-03-task1-edges-red.json, and 05-03-task2-red.json — semantic TDD records.

## Deviations and Issues Encountered

### Corrected MAPCTL polarity before accepting RED evidence

The first page-mode scaffold treated B7 set as enabling sequential cycles. The primary Epyx Appendix 3 reference identifies B7 as sequential-disable. The test setup and documentation were corrected, and the original scaffold is recorded as superseded rather than rewritten as valid evidence.

The first corrected microcycle setup also omitted the NOP byte at the post-refresh address. The trace was fixed before the unmodified-baseline RED run. A missing public profile-constant include in the private bus helper was fixed during compilation. These issues were resolved before the passing focused verification.

No stubs, skipped tests, unrun verification steps, new external interfaces, dependencies, or threat surfaces remain in this plan's changes.

## Requirements and Remaining Scope

VID-02 is complete for its stated DMA cost and page-mode interaction behavior. EVD-05 remains partial: this plan adds DMA mutation anchors, while the named-fault execution remains assigned to later Phase 5 work; Phase 6 device-fault closure also remains open. EVD-02 and CON-01 likewise remain partial as recorded in project state.

No user setup or owner action is required for this plan. Plan 05-03 is complete; later phase plans retain the remaining source-oracle, device-fault, and consumer obligations.

---
*Phase: 05-display-raw-video-and-input*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary exists, and all five recorded task commits are ancestors of the current HEAD.
