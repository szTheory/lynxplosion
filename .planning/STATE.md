---
gsd_state_version: "1.0"
current_phase: 1
current_phase_name: Evidence Harness and Walking Skeleton
status: executing
stopped_at: Completed 01-06-PLAN.md
last_updated: "2026-11-09T23:15:28.113Z"
last_activity: 2026-11-09
last_activity_desc: Phase 1 execution started
state_head: 0d4d314ebf4443dbacf06495e1e8e8ea97036dd4
progress:
  total_phases: 6
  completed_phases: 0
  total_plans: 13
  completed_plans: 6
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-09)

**Core value:** Every compatibility claim is tied to an exact, named Lynx model/configuration and to reproducible evidence from a deterministic, observable machine.
**Current focus:** Phase 1 — Evidence Harness and Walking Skeleton

## Current Position

Phase: 1 (Evidence Harness and Walking Skeleton) — EXECUTING
Plan: 7 of 13
Status: Ready to execute
Last activity: 2026-11-09 — Phase 1 execution started

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 1 P01 | 4 min | 3 tasks | 14 files |
| Phase 1 P02 | 5min | 2 tasks | 10 files |
| Phase 1 P03 | 10min | 2 tasks | 10 files |
| Phase 1 P04 | 3 min | 2 tasks | 11 files |
| Phase 1 P05 | 15min | 2 tasks | 10 files |
| Phase 1 P06 | 4 min | 2 tasks | 6 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: v1 follows the six research slices; all v1 runs are startup tier A (direct RAM load), never boot evidence.
- [Roadmap]: No Lynx I/II baseline adopted (O1 open); both CPU profiles built as data; no suite-pass claim before O1.
- [Roadmap]: Language/build/API not locked (O8 open); C17 + CMake/Ninja/CTest + C ABI is the research default only.
- [Phase 1]: Plan 01-01: status mapping fixed (ARGUMENT for NULL/invalid config, SIZE, ALIGNMENT, RANGE for bounds, STATE for bad magic/no content); lynx_guest_ticks counts from power-on or last load
- [Phase 1]: Plan 01-01: sanitizer flags live on targets (lynx_sanitize), never CMAKE_C_FLAGS, so the plain nm twin stays uninstrumented; first CI run green in 19s on ubuntu-24.04 (run 42715092093)
- [Phase 1]: 01-02: build identity uses the full 40-char git revision; configure fails above 127 characters (snapshot header reserves 128 bytes)
- [Phase 1]: 01-02: one internal SHA-256 unit in core/ shared with the runner; content hash computed inside the core at load, no public sha256 utility
- [Phase 1]: 01-03: a refused lynx-run creates no output directory; usage errors exit 2, runtime refusals exit 1; observation digest is build-independent, bundle digest embeds core_build
- [Phase 1]: 01-04: inventory compare, JUnit completeness and no-skip policy are CMake scripts with built-fault self-tests; declared skips live in tests/inventory/skips.txt with reasons — A missing, extra, duplicated or silently skipped test can never leave the check green; each rule has a negative control with exact exit code and message
- [Phase 1]: 01-05: lynx-test-header links lynx_core_plain because its signature table needs the definitions; nm checks read only the uninstrumented twin
- [Phase 1]: 01-05: abi helper allowlist limited to Darwin bzero/__bzero and Linux x86_64 _GLOBAL_OFFSET_TABLE_; no mem* wildcard
- [Phase 1]: 01-06: lynx_reset clears loaded content and equals a fresh init byte for byte; a deinitialised machine refuses reset with LYNX_ERR_STATE
- [Phase 1]: 01-06: whole-buffer memcmp refusal oracle with accepted-call controls is the API contract proof; guest-time wrap proven by white-box guest_ticks poke

### Pending Todos

None yet.

### Blockers/Concerns

- [Phase 1]: O8 (language/build/API) must be confirmed or the research default accepted before core code is written.
- [Phase 1]: O6 (clean-room protocol, GPL research exposure) needed to finalize FND-07; draft proceeds.
- [Phase 2+]: O1 (baseline model) gates every CPU-profile and suite-pass claim; tables and harness proceed.
- [Phase 5]: O9 gates freezing the raw video form if it affects LibFuturo.
- [.planning/config.json]: refers to `.planning/AUTONOMY.md`, which does not exist yet.
- Research flags (deeper research before planning): Phases 2, 3, 4, 5, 6. Standard: Phase 1 (except cc65 build spike).

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| *(none)* | | | | |

## Session Continuity

Last session: 2026-11-09T23:15:28.086Z
Stopped at: Completed 01-06-PLAN.md
Resume file: None
