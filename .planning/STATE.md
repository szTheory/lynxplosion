---
gsd_state_version: "1.0"
current_phase: 4
current_phase_name: Timers, Interrupts and First Public Suites
status: planning
stopped_at: Phase 4 context gathered
last_updated: "2026-11-11T01:47:41.419Z"
last_activity: 2026-11-10
last_activity_desc: Phase 3 complete, transitioned to Phase 4
state_head: 7b848d0b647083596a2506fcc8fb91808d669c5e
progress:
  total_phases: 6
  completed_phases: 3
  total_plans: 33
  completed_plans: 33
  percent: 50
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-10)

**Core value:** Every compatibility claim is tied to an exact, named Lynx model/configuration and to reproducible evidence from a deterministic, observable machine.
**Current focus:** Phase 4 — Timers, Interrupts and First Public Suites

## Current Position

Phase: 4 — Timers, Interrupts and First Public Suites
Plan: Not started
Status: Ready to plan
Last activity: 2026-11-10 — Phase 3 complete, transitioned to Phase 4

Progress: [█████░░░░░] 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 33
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 13 | - | - |
| 2 | 11 | - | - |
| 3 | 9 | - | - |

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
| Phase 2 P01 | one session | 3 tasks | 12 files |
| Phase 2 P02 | one session | 2 tasks | 8 files |
| Phase 2 P03 | 7 min | 2 tasks | 9 files |
| Phase 2 P04 | 1 session | 3 tasks | 9 files |
| Phase 2 P05 | one session | 3 tasks | 9 files |
| Phase 02 P06 | one session | 2 tasks | 4 files |
| Phase 2 P07 | 15 min | 2 tasks | 12 files |
| Phase 2 P08 | 20min | 2 tasks | 9 files |
| Phase 2 P09 | 19 min | 3 tasks | 14 files |
| Phase 02 P10 | 75 min | 3 tasks | 12 files |
| Phase 2 P11 | 90min | 3 tasks | 16 files |
| Phase 03 P01 | 00:25:29 | 2 tasks | 8 files |
| Phase 03 P02 | 00:26:42 | 2 tasks | 13 files |
| Phase 03 P03 | 58 min | 2 tasks | 17 files |
| Phase 03 P04 | 36 min | 2 tasks | 10 files |
| Phase 03 P05 | 17 min | 2 tasks | 10 files |
| Phase 03 P06 | 49 min | 2 tasks | 9 files |
| Phase 03 P07 | 56min | 2 tasks | 15 files |
| Phase 03 P08 | 34 min | 2 tasks | 9 files |

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
- [Phase 2]: Dispatch is a const (mode,slot) micro-operation script table plus a switch on the micro-operation, not hand-written switch (mode) then switch (step)
- [Phase 2]: Branch taken-dummy records its page-cross answer in ea bits 8 and 9
- [Phase 2]: Starting any interrupt entry clears both TAKE latches
- [Phase 2]: lynx__cpu_table returns NULL above profile 2; opcode fetch request carries neither POLL nor LAST
- [Phase 2]: 02-02: validate-kv.sh --startup-tiers list (default A); fixture manifests and claims accept none, run manifests A only; malformed list is usage error
- [Phase 2]: 02-03: sample containers keep the three-line header, so files are 3201546 and 3230210 bytes (bodies equal the research sizes 3201335 and 3229999)
- [Phase 2]: 02-03: vectors-full.sh takes several modes in one invocation sharing one work dir; full download is about 2.0 GB and 190 s
- [Phase 2]: 02-04: vector reason order is EXCL_HW, UNV_SHAPE, DEC_EXTRA, TIER_STATE; reader self-test registered as cpu.vectors.reader_malformed
- [Phase 2]: 02-05: no_step control compares failures with asserted minus unmoved (identity incl. pc), since identity ignores pc and every pc-advancing vector fails with no CPU step
- [Phase 2]: 02-05: added cpu.vectors.complete_mode so the full lane's whole-file runner path is checked offline; vectors_pin_controls.cmake as a separate loop driver
- [Phase 02]: 02-06: one source tag per scenario row; mixed claims (wrap rule plus dummy address) are split into a per-cycle SST-MODEL row and a final-state WDC or LT-HW row
- [Phase 02]: 02-06: WAI/STP evidence form is table absence (cpu.tables) plus the Test 8 replica showing CB/DB as 1-byte 1-cycle non-polling NOPs on both profiles
- [Phase 2]: 02-07: fixture .s files are the source of truth for bytes; committed hex is compared with pinned ca65/ld65 output on every check (fixtures.cpu_asm), never regenerated by a script
- [Phase 2]: 02-08: bus timing indices clamp to 0 and callers must check lynx__bus_cost_model_valid first; runner.cli now asserts ticks_consumed of its halting image
- [Phase 2]: 02-09: the budget loop's in-cycle branch stops at the target (same semantics) so a <= fault fails api.advance_budget instead of hanging
- [Phase 2]: 02-09: cross-profile snapshots are refused by the header identity (MISMATCH); cost model, reserved bytes, cpu_ready_at range and CPU state are value checks (INVALID) after the hash
- [Phase 2]: 02-09: golden re-baselined on churn-loop under none (506f7995...2bad3c), equal on Apple clang and Linux gcc 13.3; file name tests/golden/skeleton.digest kept
- [Phase 02]: 02-10: lynx-run --profile is a closed set (none default); the UNDEFINED note is built from public reads in the runner, not the core
- [Phase 02]: 02-10: api.seam_agreement runs all three profiles through tests/cpu/seam_agreement.cmake; about 10 s serial, dominated by 1,024 undefined-row restores under none
- [Phase 2]: Full vector lane has no CI trigger (5 min 47 s, 2 GB download); run at phase verification, pin bumps and before claims
- [Phase 2]: api.seam_agreement split per profile with ctest COST so the longest starts first; ci.yml unchanged
- [Phase 03]: Strict BLL loading accepts an exact BS93 object and hashes the complete object while keeping payload and startup identity distinct.
- [Phase 03]: MAPCTL remains a provisional instance-owned byte latch; written bits are retained, but mapped ROM and device behavior is not qualified.
- [Phase 03]: Snapshot format 3 carries explicit identity fields and MAPCTL; format 1 and 2 are refused.
- [Phase 03]: The runner accepts a complete binary BS93 object and derives the load and entry address from the core.
- [Phase 03]: Runner manifests derive format, startup, complete-object hash, entry and payload fields from public identity.
- [Phase 03]: The provisional raw-image API remains until plan 03-03 removes it.
- [Phase 03]: Keep CPU-vector flat-bus machine-loop tests behind a private distinct identity that public BLL loading cannot select.
- [Phase 03]: Require startup=A when present and require startup to match startup_tier when both manifest fields are present.
- [Phase 03]: Retain the existing deterministic golden; complete-object identity and repeatability are not hardware-accuracy evidence.
- [Phase 03]: Use FFF8 as fixed RAM and FFF9 as the full-byte MAPCTL latch; only guest writes to FFF9 are implemented.
- [Phase 03]: Unsupported mapped device, ROM, vector, and MAPCTL-read routes refuse explicitly; host register observation reports the latch without guest readback.
- [Phase 03]: Retain flat5 timing as an explicitly unqualified placeholder, independent of route decoding.
- [Phase 03]: 03-05: post-write watch returns at grant+1 after bus and CPU completion; host observation state remains separate from serialized guest state.
- [Phase 03]: 03-06: successful restore clears host watch state only after validation and guest-state apply; refusals preserve the whole instance.
- [Phase 03]: 03-06: preserve watch grant/grant+1 semantics; host observer state stays out of guest snapshots/digests, so no golden rebaseline was needed.
- [Phase 03]: 03-06: DET-03 unclassified probe remains a flagged assumption; repeatability checks do not auto-classify it.
- [Phase 03]: Harness resolves linked symbols and validates the fixed result schema before dependent pixel reads.
- [Phase 03]: Flat5 completion bounds remain PLACEHOLDER diagnostic checks, not hardware timing evidence.
- [Phase 03]: Runner and harness compare full accepted object identity and independently expected result/pixel values; retain the existing diag.result_controls inventory ID.
- [Phase 03]: The diagnostic consumer links only the public API and emits an explicitly labeled 8x8 SNAPSHOT plus Phase 6 placeholder silence.

### Pending Todos

None yet.

### Blockers/Concerns

No blockers. Open owner decisions that shape claims but do not stop work:

- O1 (baseline Lynx model/CPU profile): no baseline adopted; profiles are data (`none`, `lynx1`, `lynx2`); claims name profile `none` only.
- O6 (clean-room protocol): policy and exposure record stay DRAFT; no emulator source opened in Phases 1-2.
- O5 (licence): no licence file; all rights reserved until decided.
- O9 (raw video form if it affects LibFuturo): gates freezing the form in Phase 5.
- Phase 3 goal verification and software qualification pass. CON-01 remains globally open for the actual-frame and actual-audio consumer obligations already scheduled in Phases 5 and 6; the Phase 3 snapshot/silence slice is complete. Research flags remain for Phases 4-6.
- Carried follow-ups: bus tick stamp is plumbed but unobserved until a device consumes it (Phase 4); `cpu.vectors.malformed`/`complete_mode` are path-length fragile (about 170 chars); the history scan grows about 1.5 s per commit (candidate: scan each blob once).

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Technical follow-up | Investigate restore accepting a forged `cpu_ready_at` span of 255 ticks; inherited from `origin/main`, documented in Phase 3 code review, and outside Plan 03-09's change scope. | Open; schedule before relying on broader snapshot-validation claims; no owner risk acceptance inferred. | Phase 03 Plan 09 | v1 |

## Session Continuity

Last session: 2026-11-11T01:47:40.878Z
Stopped at: Phase 4 context gathered
Resume file: .planning/phases/04-timers-interrupts-and-first-public-suites/04-CONTEXT.md
