---
gsd_state_version: "1.0"
current_phase: 05
current_phase_name: Display, Raw Video and Input
current_plan: 10
status: executing
stopped_at: Completed 05-09-PLAN.md
last_updated: "2026-11-12T10:03:52.326Z"
last_activity: 2026-11-11
last_activity_desc: Phase 05 execution started
state_head: e6afbbe3658545cbbf1def252457162aa2dca4dd
progress:
  total_phases: 6
  completed_phases: 4
  total_plans: 57
  completed_plans: 54
  percent: 67
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-11)

**Core value:** Every compatibility claim is tied to an exact, named Lynx model/configuration and to reproducible evidence from a deterministic, observable machine.
**Current focus:** Phase 05 — Display, Raw Video and Input

## Current Position

Phase: 05 (Display, Raw Video and Input) — EXECUTING
Current Plan: 10
Total Plans in Phase: 12
Status: Ready to execute
Last activity: 2026-11-11 — Phase 05 execution started

Progress: [███████░░░] 67%

## Performance Metrics

**Velocity:**
- Total plans completed: 45
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 13 | - | - |
| 2 | 11 | - | - |
| 3 | 9 | - | - |
| 4 | 12 | - | - |

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
| Phase 04 P01 | 137 min | 3 tasks | 15 files |
| Phase 04 P02 | 25min | 2 tasks | 12 files |
| Phase 04 P03 | 38min | 2 tasks | 8 files |
| Phase 04 P04 | 46 min | 2 tasks | 7 files |
| Phase 04 P05 | 25min | 2 tasks | 9 files |
| Phase 04 P06 | 52m | 2 tasks | 14 files |
| Phase 04 P07 | 20 min | 2 tasks | 13 files |
| Phase 04 P08 | 30min | 2 tasks | 6 files |
| Phase 04 P09 | 74 min | 2 tasks | 15 files |
| Phase 04 P10 | 46min | 2 tasks | 6 files |
| Phase 04 P11 | 30min | 2 tasks | 6 files |
| Phase 04 P12 | 4h 14m | 2 tasks | 5 files |
| Phase 05 P01 | 55min | 2 tasks | 20 files |
| Phase 05 P02 | about 35 min; measured from first task commit through closeout | 2 tasks | 8 files |
| Phase 05 P03 | 35m | 2 tasks | 15 files |
| Phase 05 P04 | 28min | 2 tasks | 5 files |
| Phase 05 P05 | 38m | 2 tasks | 7 files |
| Phase 05 P06 | 61 min | 2 tasks | 19 files |
| Phase 05 P07 | 55 min | 2 tasks | 10 files |
| Phase 05 P08 | 18min | 2 tasks | 6 files |
| Phase 05 P09 | 41 min | 2 tasks | 6 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: v1 follows the six research slices; all v1 runs are startup tier A (direct RAM load), never boot evidence.
- [Roadmap]: No Lynx I/II baseline adopted (O1 open); both CPU profiles built as data; no suite-pass claim before O1.
- [Roadmap]: O8 accepted: C17 + CMake/Ninja/CTest + C ABI; preserve separate runtime model mappings.
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
- [Phase 04]: Build pinned CPU and timer suites only in owned scratch; preserve exact source hashes and notices.
- [Phase 04]: Accept only the source-derived first timer write as unavailable now or as the exact future post-write watch hit.
- [Phase 04]: Task 1 is fixture-only and has no meaningful RED; correct the plan TDD marker and do not fabricate failure evidence.
- [Phase 04]: Timer init/reset/load use deterministic timer-phase-fixed-zero-1; this is software policy, not calibration.
- [Phase 04]: Timer-bank events precede same-tick CPU bus actions and their poll/completion under timer-cpu-poll-order-1.
- [Phase 04]: Keep profile and hardware qualification, CTLB lower-bit lifetime, and timer-source agreement claims bounded; Plan 06 covers snapshot continuation only.
- [Phase 04]: CTLB bit 1 is a one-event borrow-in strobe and bit 3 sets or clears DONE; pinned guest behavior is comparison evidence, not hardware qualification.
- [Phase 04]: Only source-declared timer links 0→2→4 and 1→3→5→7 are enabled; unsupported timer 0, timer 1/audio 3, and timer 6 inputs refuse before mutation.
- [Phase 04]: CTLB low status-bit lifetime is explicit model policy until the next timer event because reviewed primary notes do not define stable read lifetime.
- [Phase 04]: Existing IRQ and scheduler behavior was already green, so Plan 4 adds permanent observations without fabricating RED evidence.
- [Phase 04]: Timer ordinals are fixed 0–7 and production coalesces the bank into one candidate; no dynamic sequence counter or exhaustion state exists.
- [Phase 04]: Timer-before-CPU ordering and fixed-zero timing remain software policy, not hardware timing or profile qualification.
- [Phase 04]: Keep bus ownership hold separate from priced-cycle readiness; normalize expired readiness to guest time and report zero remaining cost under hold.
- [Phase 04]: Derive wake_pending from timer pending independently of CPU I; CPU I still controls IRQ acceptance at POLL.
- [Phase 04]: Keep FD91 unavailable without active Suzy ownership; pinned public suite sources do not require CPUSLEEP.
- [Phase 04]: Plan 06 format 4 serializes timer/IRQ/hold state and the current-tick event-applied bit; wake and future timer candidates are derived, and prior cursor timestamps normalize away.
- [Phase 04]: Snapshot format 4 stores canonical timer/IRQ/hold fields and only the current-tick applied-event bit; wake, CPU request and future candidates are derived.
- [Phase 04]: Historical event timestamps normalize away with guest time; timer ordinals are fixed 0–7 and need no dynamic sequence counter.
- [Phase 04]: Keep the existing golden digest unchanged; golden and sensitivity controls pass, and snapshots are not digest input.
- [Phase 04]: 04-07: Source-oracle agreement is raw software evidence; O1 and named-profile qualification remain open.
- [Phase 04]: 04-07: The source-level RMB0 probe writes the guest model observation; profile selection remains an independent comparison input.
- [Phase 04]: 04-07: EVD-02 and EVD-04 remain globally open because the timer suite and guest-model taxonomy are unfinished.
- [Phase 04]: Detected CPU model requires agreement among the guest RMB0 probe and marker, pinned source tuple, and independently selected profile.
- [Phase 04]: CPU suite matches remain raw software-oracle evidence; O1 and hardware qualification stay open, and the overall default remains none.
- [Phase 04]: Use independent zeroed and deterministic seeded guest instances; add no public debug API or production fault flag.
- [Phase 04]: Plan 04-09 uses immutable two-profile observation baselines for captured-data timer faults, with fresh guest observations at batch boundaries and per-case positive/hash checks.
- [Phase 04]: D-10 remains limited to timer offset 14 at observed 0C versus unchanged source 0D; global EVD-02 stays open.
- [Phase 04]: Same-profile deterministic hashes cannot prove freshness; actual fresh-machine completion and watch checks must ignore prior output files.
- [Phase 04]: 04-11: Use per-CTest copied-core mutants with unique anchors, source/build hashes, exact API assertions, and clean API brackets.
- [Phase 04]: 04-11: Preserve D-10 index 14 at expected 0D versus observed 0C; global EVD-02 and recurring EVD-05 remain open for later phase evidence.
- [Phase 05]: LEGACY_FLAT5 remains the default; VIDEO_POLICY1 is explicit immutable unverified software timing policy.
- [Phase 05]: Raw native video exposes pens, per-row palettes and guest line ticks without host presentation in the core.
- [Phase 05]: The Phase 5 native C API does not change LibFuturo; O9/HOST-02 remains outside this plan.
- [Phase 05]: No run-identity change: another emulator appears only in the prohibition on importing constants.
- [Phase 05]: Incomplete DMA rows are omitted and cleared without rolling back consumed guest time.
- [Phase 05]: The effective normal display address is aligned down at the third-blank transfer; unsupported flip, color and two-bit modes refuse atomically.
- [Phase 05]: Per-row palette snapshots occur after timer work and before same-tick CPU stores; this is software policy, not hardware latch evidence.
- [Phase 05]: Baseline-green palette behavior is recorded as a positive regression, never as fabricated RED evidence.
- [Phase 05]: MAPCTL B7 set disables sequential cycles; only B7-clear candidates are considered.
- [Phase 05]: Keep page-mode eligibility limited to shared NOP opcode while primary CPU page-mode docs leave decode eligibility unspecified; this remains an unverified software policy.
- [Phase 05]: Serialize page continuation in existing format-5 payload byte 15, preserving payload size and exact-build compatibility; keep flat5 unchanged.
- [Phase 05]: Keep the existing frame/input implementation: the full transactional, stop-ordering and reset/load regression matrix passed without a production-code gap.
- [Phase 05]: Leave VID-04, VID-05 and API-06 unchecked until all sibling plans that declare them have summaries.
- [Phase 05]: All nine-bit logical masks remain valid, including opposite directions and simultaneous Option 2 plus Pause.
- [Phase 05]: Input is sampled at the actual CPU read grant with zero additional sample pricing; changing that point requires a new ordered contract.
- [Phase 05]: Disconnected cartridge lines are fixed zero only under an explicitly unverified software policy.
- [Phase 05]: Guest-state digests use a versioned fieldwise dynamic-state stream and exclude immutable identity, padding and host observations.
- [Phase 05]: The CPU-loop golden changes only because the runner observation schema now includes obs.state.hex; sensitivity controls prove the added stream is nonvacuous.
- [Phase 05]: VID-04, VID-05, INP-01 and API-06 remain unchecked because zero of four are ready while sibling Phase 05 obligations remain open.
- [Phase 05]: Preserve consumer.frame_tracer strict positive checks with a separate A-only guest fixture while expanding the nine-input replay diagnostic.
- [Phase 05]: During software CPU/DMA overlap, require exact input patterns only on settled frames and bound transitional pen/color values with raw-to-PPM checks; this is not hardware tearing qualification.
- [Phase 05]: Observe two final frame periods with a 32-frame bound so the final held input has a settled endpoint frame.
- [Phase 05]: Keep API-06, INP-01, HB-02, CON-01, VID-04 and VID-05 partial because the requirements ready-ids query reports remaining Phase 05 obligations.
- [Phase 05]: Preflight device/inode identity and symlink containment before creating output, so alternate spellings of one file cannot pass as distinct inputs.
- [Phase 05]: Remove staging only after checking the invocation marker and cached directory identity; publish the complete bundle with exclusive directory rename.
- [Phase 05]: Report the point of failure separately for result schema, guest progress, logical-input response, raw-frame evidence, and rendered PPM color.
- [Phase 05]: Keep Task 2 baseline-green evidence distinct from a comparative failure on the reconstructed pre-plan consumer.
- [Phase 05]: Keep all eight video and input faults as independent copied-source semantic mutations without production fault switches.
- [Phase 05]: Keep EVD-05 partial until the Phase 6 audio fault obligation is complete.

### Pending Todos

None yet.

### Blockers/Concerns

Phase 4 is complete and its owning verification status is passed. Local macOS, self-contained Ubuntu arm64 and hosted CI qualify the final product/script source baabaee. Formal evidence is in `04-VERIFICATION.md` and `04-QUALIFICATION.md`. Phase 5 discussion is next; Phase 6 has not started.

- Both timer runs complete with independent model evidence, but result offset 14 is 0C versus source 0D on both profiles (15/16 accepted). Exact current-model timing explains 12 for every phase; original display/page-mode and counter-initialization preconditions differ. Preserve the strict comparison failure and unchanged oracle while completing Phase 4; exact agreement remains pending Phase 5. No source/profile/hardware qualification is accepted. No owner action is needed for this approved continuation.
- Plan 09's data-only control matrices use per-invocation fresh observations on both profiles and immutable hash-checked baselines for private fault copies; every mutation retains positive baseline validation before/after and its named diagnostic.
- Phase 4's five named timer/IRQ mutants now fail their exact independent API assertions; CPU source agreement and D-10 timer observation gates pass. Global EVD-02 remains open for the unchanged index-14 `0D` versus `0C` row in Phase 5. EVD-05 is a recurring Phase 4-6 requirement: its Phase 4 slice is complete, while Phase 5 and Phase 6 device-fault evidence remains open.

Other open owner decisions remain in force:

- O1 (baseline Lynx model/CPU profile): no baseline adopted; profiles are data (`none`, `lynx1`, `lynx2`); claims name profile `none` only.
- O6 (clean-room protocol): policy and exposure record stay DRAFT; no emulator source opened in Phases 1-2.
- O5 (licence): no licence file; all rights reserved until decided.
- O9 (raw video form if it affects LibFuturo): gates freezing the form in Phase 5.
- Phase 3 goal verification and software qualification pass. CON-01 remains globally open for the actual-frame and actual-audio consumer obligations already scheduled in Phases 5 and 6; the Phase 3 snapshot/silence slice is complete. Research flags remain for Phases 4-6.
- Carried follow-ups: the timer bank now consumes bus tick stamps with ordering controls; `cpu.vectors.malformed`/`complete_mode` are path-length fragile (about 170 chars); the history scan grows about 1.5 s per commit (candidate: scan each blob once).
- Final marker validation rejects trailing pin data; its old behavior was reproduced before the fix. All final local/Linux/hosted gates pass. The outage-era queued run remains historical and unexecuted. Source qualification is authoritative in `04-QUALIFICATION.md`; metadata-only closeout needs an exact-head hosted run, without repeating unchanged local/Linux gates.


## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Technical follow-up | Investigate restore accepting a forged `cpu_ready_at` span of 255 ticks; inherited from `origin/main`, documented in Phase 3 code review, and outside Plan 03-09's change scope. | Resolved in Phase 4 Plan 06 with reachable residual bounds and hostile snapshot controls; see 04-06-SUMMARY.md. | Phase 03 Plan 09 | v1 |

## Session Continuity

Last session: 2026-11-12T10:03:51.938Z
Stopped at: Completed 05-09-PLAN.md
Resume file: None
