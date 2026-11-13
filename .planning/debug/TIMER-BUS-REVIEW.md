---
phase: timer-bus-investigation
reviewed: 2026-11-13T17:02:53Z
depth: standard
files_reviewed: 7
files_reviewed_list:
  - core/machine.c
  - core/video.c
  - core/snapshot.c
  - core/internal.h
  - tests/api/test_api.c
  - docs/bus-video-policy.md
  - docs/snapshot-video.md
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Timer and Bus Focused Review

**Reviewed:** 2026-11-13T17:02:53Z
**Depth:** standard
**Files Reviewed:** 7
**Status:** clean

## Summary

The snapshot validator repair resolves the first-round blocker. It now permits a deferred refresh or DMA request only when the saved CPU release can account for the request's age and no video or refresh owner intervened. It also rejects a snapshot that claims a future CPU release while a device owns the bus. The changed validator and its callers preserve the existing timestamp bounds and snapshot layout.

The regression uses a real JMP loop to reach the natural refresh at tick 256 and the DMA request during a later CPU cycle. It round-trips snapshots inside the CPU interval and at its exact release, checks the first device grant, and compares coarse and sliced continuation. The hostile age case preserves the general ready-span limit (`ready = release + 4` at `ticks = release`) while making the request older than the allowed CPU interval; it reaches the new age guard. The overlap case reaches the new CPU/device exclusivity guard. The evidence note records that the focused regression was red before the validator repair and that the affected 16-case group passed after it; I did not rerun tests or broader gates.

The strict timer comparison remains explicitly unresolved at 12/15 versus source 13. This change does not resolve that mismatch or establish hardware accuracy.

## Narrative Findings (AI reviewer)

No new actionable findings in the re-reviewed snapshot validator, regression, or associated documentation.

## Resolved Finding History

### CR-01: Snapshot restore rejected a deferred refresh at the half-open release boundary — RESOLVED

**First-review severity:** BLOCKER
**Resolution:** `video_state_valid` now receives the already validated `ready` tick and admits an overdue request only when `due < ticks <= ready`, `ready - due < 5`, and both historical device release ticks are strictly before the request. The existing overdue guards remain in place. A separate check rejects `ready > ticks` when either device release is still in the future.
**Evidence:** `check_snapshot_cpu_delayed_bus` covers refresh-only, DMA-only, and combined requests, snapshots inside the CPU interval and at exact release, then checks resumed grants and continuation equivalence. The evidence note records the pre-repair refusal and post-repair focused pass.

## Final Fixture Re-review

The five-line `video_dma_cost_fault` fixture change waits for `cpu_ready_at` before setting the synthetic burst start. `advance_ticks` calls `lynx_advance` for exactly the requested interval and asserts both full tick consumption and `LYNX_STOP_BUDGET`, so the wait reaches the CPU release tick while the half-open budget leaves the posted CPU request ungranted. The burst can then test the intended same-tick video/refresh/CPU priority without overlapping the preceding real operand cycle. The parent reports the canonical failure was limited to this stale fixture and the focused fault case now passes, including its compiled mutant and semantic assertion; no production code changed. No new finding.

---

_Reviewed: 2026-11-13T17:02:53Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
