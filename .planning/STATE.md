---
gsd_state_version: "1.0"
current_phase: 2
current_phase_name: CPU Profiles and Per-Opcode Vectors
status: executing
stopped_at: Phase 1 complete, ready to plan Phase 2
last_updated: "2026-11-10T03:00:47.649Z"
last_activity: 2026-11-09
last_activity_desc: Phase 1 complete, transitioned to Phase 2
state_head: 6e9650ace227d250e6bb2934a30c5b6074f9e5f1
progress:
  total_phases: 6
  completed_phases: 1
  total_plans: 24
  completed_plans: 13
  percent: 17
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-09)

**Core value:** Every compatibility claim is tied to an exact, named Lynx model/configuration and to reproducible evidence from a deterministic, observable machine.
**Current focus:** Phase 1 — Evidence Harness and Walking Skeleton

## Current Position

Phase: 2 (CPU Profiles and Per-Opcode Vectors) — READY TO EXECUTE
Plan: Not started
Status: Ready to execute
Last activity: 2026-11-09 — Phase 1 complete, transitioned to Phase 2

Progress: [██░░░░░░░░] 17%

## Performance Metrics

**Velocity:**
- Total plans completed: 13
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 13 | - | - |

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
| Phase 1 P07 | 5min | 2 tasks | 8 files |
| Phase 1 P08 | 25min | 3 tasks | 12 files |
| Phase 1 P09 | 20min | 2 tasks | 10 files |
| Phase 1 P10 | 45min | 2 tasks | 9 files |
| Phase 1 P11 | 40min | 3 tasks | 8 files |
| Phase 1 P12 | 35 min | 2 tasks | 12 files |
| Phase 1 P13 | 25min | 2 tasks | 4 files |

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
- [Phase 1]: 01-07: Snapshot format v1 (236-byte header + 65552-byte payload, SHA-256 protected); restore validates ten ordered checks then applies in one non-failing step; identity comes from the instance, restore needs the identical build id (reversible)
- [Phase 1]: 01-08: golden = observation digest only (build-independent), proven equal on Apple clang and Linux gcc; regenerate only on intended skeleton change
- [Phase 1]: 01-08: validate-kv keys are [a-z][a-z0-9_]*; strict layout, no comments or blank lines; -P scripts using IN_LIST set cmake_policy(VERSION 3.25)
- [Phase 1]: 01-09: .lynx-pin is 64 lowercase hex plus newline; .lynx-pin-files is '#' header lines then '<sha256> <path>' sorted LC_ALL=C (rights scan in 01-11 skips '#' lines)
- [Phase 1]: 01-09: fetch-pin.sh fails closed on newline paths, pin-record names in the archive, and unlistable archives; only .tar.gz/.tgz supported
- [Phase 1]: 01-10: cc65 is built with BUILD_ID fixed to the pinned commit, because the Makefile otherwise embeds the enclosing repository HEAD into the tool bytes
- [Phase 1]: 01-10: .lynx-build-files (header pin_sha256 and recipe, then sha256 path rows) and the .lynx-build key=value stamp are written only after both make runs and the probe succeed
- [Phase 1]: 01-11: non-text means a control byte or invalid UTF-8 (never NUL alone); exactly-512-byte non-text files fail in every zone; scan-binaries findings go to stderr, summary line to stdout
- [Phase 1]: 01-11: hygiene.sh keeps only --tree and --history, reads text files only, excludes only itself; binaries are owned by scan-binaries.sh
- [Phase 1]: 01-12: claims are bound to the SHA-256 of the exact fast.txt bytes; regenerate with sh scripts/check-claims.sh --inventory-sha256 after re-running evidence; profile stays none while O1 is open; clean-room and exposure record remain DRAFT pending O6
- [Phase 1]: 01-13: no actions/cache for cc65; the step takes 11-16 s on the runner, under the 30 s rule (measured candidate recorded in 01-13-SUMMARY)
- [Phase 1]: 01-13: ctest runs with 4 parallel jobs in the check test preset; every test has its own scratch path, 8 parallel runs green
- [Phase 1]: 01-13: cc65 tool hashes differ between macOS clang and Linux gcc builds; only the linked probe hash (e2fdd981...) must match and does

### Pending Todos

None yet.

### Blockers/Concerns

No blockers. Open owner decisions that shape claims but do not stop work (the roadmap
schedules around each):

- O1 (baseline Lynx model/CPU profile): gates every CPU-profile and suite-pass claim. Phase 2 builds both profiles as data and claims only profile `none`.
- O6 (clean-room protocol): the policy and exposure record stay DRAFT; implementers do not open GPL emulator source.
- O5 (licence): no licence file; all rights reserved until decided.
- O9 (raw video form if it affects LibFuturo): gates freezing the form in Phase 5.
- Resolved in Phase 1: O8 (C17, CMake/Ninja/CTest, C ABI, owner-accepted), public repository (owner-authorized), `.planning/AUTONOMY.md` created.
- Research flags (deeper research before planning): Phases 2, 3, 4, 5, 6.

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| *(none)* | | | | |

## Session Continuity

Last session: 2026-11-10T00:16:31.065Z
Stopped at: Phase 1 complete, ready to plan Phase 2
Resume file: None
