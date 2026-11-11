# Phase 4 prequalification coverage audit

**Audit date:** 2026-11-11
**Audited source:** `db7149eebde4c2a09f4cfc9c57cb414b4e89e3d9`
**Result:** 25/25 plan tasks mapped. No current product-behavior coverage gap found. Final qualification remains pending.

This is an audit of existing tests and their recorded results. It did not add or rerun behavioral tests. Owning Plan summaries contain the execution evidence; Plan 12 Task1 evidence is in [04-QUALIFICATION.md](04-QUALIFICATION.md). The only live queries were CTest discovery and the claim/inventory hash check.

## Task map

| Task | Requirements | Actual check and meaningful negative controls | Observed evidence |
|---|---|---|---|
| 04-01-T1 | EVD-02, EVD-04 | `suite.build`: original CPU and timer BLLs, linked symbols, strict load and bounded prefix on both explicit profiles. | Plan01 records 4/4 prefixes and the registered tracer pass. |
| 04-01-T2 | EVD-02, EVD-04 | `hygiene.manifests`, `hygiene.tree`: fixture identity/notice coverage and tree hygiene. | Plan01 records 9 manifests covering 18 fixtures and both checks passing. |
| 04-01-T3 | EVD-02, EVD-04 | `suite.build_controls`: missing tool/source/label, malformed BS93 length, changed assembly identity and unsafe/symlink output; positive builds bracket faults. | Plan01 records `suite.build` and `suite.build_controls` passing with named refusals. |
| 04-02-T1 | TIM-01..03 | `api.timer_tracer`: a guest programs timer0, reads mapped expiry/pending state and stores a result; mapped reads remain separate from raw RAM. | Plan02 records the direct tracer and affected 12-test regression selection passing. |
| 04-02-T2 | TIM-01..03 | `api.timer_standalone`, `api.timer_phase`, `api.timer_tracer`: eight counters, divider boundaries, reload/one-shot and shared phase across writes; unsupported routes retain explicit refusal. | Plan02 records all three timer API tests and the 12-test affected selection passing. |
| 04-03-T1 | TIM-02, TIM-03 | `api.timer_done_borrow`, `api.timer_link_tracer`: source-derived CTLB `$0A` borrow, linked 3→5 event on a collision, and held reset-DONE behavior. | Plan03 records the behavior RED at the intended mapped count/CTLB assertions, then passing guest cases. |
| 04-03-T2 | TIM-02, TIM-03 | `api.timer_links`, `api.timer_unavailable`, plus the two T1 IDs: every owned edge/chain and simultaneous expiry; unsupported timer0/timer6/audio3 sources, invalid writes and mapped-range refusal. | Plan03 records all 7 selected timer regressions passing, including boundary and refusal cases. |
| 04-04-T1 | TIM-01, TIM-03, EVD-05 | `api.irq_pending`, `api.irq_poll`: real handlers on both profiles; enables gate new latches, pending survives disable, INTSET/INTRST and I-mask/POLL behavior are observed. | Plan04 records guest handler/stack checks and the intended tick-boundary observations. |
| 04-04-T2 | TIM-01, TIM-03, EVD-05 | `api.scheduler_order`, `api.scheduler_limits`, `api.scheduler_watch`, plus IRQ tests: traversal permutations, target boundaries, zero budget, idle/headroom limits, timer/cascade-before-CPU prefix and grant+1 watch return. | Plan04 records the affected 16-test selection passing after its test-seam fixes. |
| 04-05-T1 | TIM-04, TIM-01, TIM-03 | `api.request_hold`: real posted store remains unchanged while time/timer IRQ progress; release grants the same request once. | Plan05 records semantic RED at the remaining-cost assertion and the direct guest test passing. |
| 04-05-T2 | TIM-04, TIM-01, TIM-03 | `api.request_hold`, `api.request_wake`, `api.request_boundary`: held/in-flight costs, masked/unmasked wake, collisions, per-instance isolation and explicit FD91 refusal. | Plan05 records the three tests and 16 targeted cross-module regressions passing; no Suzy/WAI/STP claim. |
| 04-06-T1 | TIM-01..04 | `api.timer_snapshot`, `api.snapshot_refusal`: format-4 timer/IRQ/request continuation; old format and hostile, rehashed state refuse atomically. | Plan06 records direct snapshot/refusal modes passing, including timer/link/time/readiness corruption. |
| 04-06-T2 | TIM-01..04 | `api.timer_snapshot_controls`, `det.timer_paths`, `det.golden`, `det.golden.self_test`: coarse/sliced/watched/restored paths compare independent RAM, mapped, register, request, time and snapshot observations at common ticks. | Plan06's first full CTest had 136/140 pass with four stale format consumers; after fixes the affected 9/9 retest passed. Canonical full check remains pending. |
| 04-07-T1 | EVD-02, EVD-04 | `suite.cpu.lynx1`, `suite.oracle_tracer`: pinned source build/oracle, linked symbols, real guest sentinel/progress/model and 18 CPU source rows. | Plan07 records 2/2 passing, including the bounded 18/18 CPU observation. |
| 04-07-T2 | EVD-02, EVD-04 | `suite.oracle_controls`, `suite.symbol_controls`, `suite.oracle_tracer`: malformed/stale source/oracle, source shape/count/grammar, missing/duplicate/overlapping/out-of-range symbols and time boundaries. | Plan07 records 4/4 passing with exact parser diagnostics. |
| 04-08-T1 | EVD-02, EVD-04, EVD-05, TIM-03 | `suite.cpu.lynx1`, `suite.cpu.lynx2`: fresh zeroed and seeded runs, independent guest probe/marker and source tuple, all 18 CPU comparisons. | Plan08 records 7/7 selected tests passing; both profiles match 18/18 and zeroed/seeded results agree. |
| 04-08-T2 | EVD-02, EVD-04, EVD-05, TIM-03 | `suite.cpu.model_controls`, `suite.cpu.bound_controls`: actual opposite-profile execution; missing/invalid probe, tuple disagreement, count/zero/progress/sentinel/identity faults; exact time floor/ceiling and overflow. | Plan08 records the exact control selection passing with clean positives bracketing the opposite-profile case. |
| 04-09-T1 | EVD-02, EVD-04, EVD-05, TIM-02, TIM-03 | `suite.timers.observation.lynx1/.lynx2`, `strict_comparison_controls`, `cascade_model`: full 16-row observations, exact strict exit1, independent linked 64-phase count proof. | Plan 09 and the Plan 12 ledger record both complete observations, 15/16 agreements per profile, exact pending row14 (`0c` vs unchanged `0d`), and 12 acknowledgements for all 64 phases. |
| 04-09-T2 | EVD-02, EVD-04, EVD-05, TIM-02, TIM-03 | `suite.timers.model_controls`, `result_controls`, `pending_controls`: actual wrong-profile run; captured alternatives/shape/time/result faults; malformed or stale pending descriptor/companion/proof/source identities. | Plan 09 records these control batches passing. Current source lists 49 result-control cases and 55 pending descriptor/companion/proof cases; details below. |
| 04-10-T1 | EVD-02..05, TIM-04 | `suite.cpu.no_run`, `suite.timers.no_run`: each suite/profile is held before the first instruction; CPU/request and all RAM are unchanged while guest time reaches the minimum, then the exact absent-execution refusal is required. Hold release must be followed by a fresh valid positive. | Plan 10 records the 2 CTests passing; together they execute four held suite/profile cases. |
| 04-10-T2 | EVD-02..05, TIM-04 | `suite.completion_controls`, `suite.selection_controls`: 8 actual omitted-sentinel/256-tick faults across both suites and profiles; empty, wrong, duplicate and incomplete selections; registration-to-inventory binding. | Plan 10 records the exact 8-case manifest, named refusals and clean positives before/after each fault; its targeted 9/9 suite selection passed. |
| 04-11-T1 | EVD-05, TIM-01..03 | `timer.fault.done`, `timer.fault.reset_done`: isolated source mutants; each variant must configure/build/link, then fail its named timer assertion; clean API test brackets each mutant. | Plan11 records both exact assertion detections and 5/5 mutant CTests passing. |
| 04-11-T2 | EVD-05, TIM-01..03 | `timer.fault.phase`, `timer.fault.order`, `timer.fault.irq`: isolated shared-phase, event-order and pending-mask mutants with exact assertions and clean API brackets. | Plan11 records successful build/runtime assertion detection for all three; all five mutant CTests passed. |
| 04-12-T1 | TIM-01..04, EVD-02..05 | `suite.timers.observation.lynx1/.lynx2`, `strict_comparison_controls`, `cascade_model`, `model_controls`, `result_controls`, `pending_controls`: audit the exact pending gate and its output inventory. | Task1 ledger records 7/7 passing at source `0c4ff91`. HEAD `db7149e` adds documentation only; no test was rerun in this audit. Summary12 is absent because Task2 has not resumed. |
| 04-12-T2 | TIM-01..04, EVD-02..05 | `sh scripts/check.sh`, then the same exact gate on self-contained Ubuntu 24.04 arm64 and exact reviewed-head CI after parent reviews/fixes. | **PLANNED/PENDING.** This is an explicit qualification obligation, not a missing product test and not a human-only UAT item. Root owns the checkpoint and resumption. |

## Hard-path audit details

### Source observations and exact D-10 boundary

- CPU positives retain exactly 18 raw source-order results per profile and compare all 18. The two profiles carry independent executed probes and matching source tuples; their guest tick bounds, completion sentinel, progress and object/label/oracle identities are recorded separately from hardware claims.
- Timer positives retain all 16 meaningful results inside the linked 18-byte extent, with two reserved bytes. Both explicit profiles have exactly 15 source agreements and the single pending row 14, source `0d` versus observed `0c`, under the descriptor-bound provisional `flat5`/5-tick, sample-offset-0, fixed-zero phase, no page-mode/video/refresh-DMA, zero-counter environment. The complete row table and descriptor identities are in [04-QUALIFICATION.md](04-QUALIFICATION.md).
- The strict timer CLI is still an exact exit-1 source mismatch. Its control first proves valid completed observations, then checks the precise diagnostic and all 16 comparison rows: 15 accepted and only `result 14 / 0c / EXPECT(0d)` mismatched. A successful observation control does not turn that source mismatch into agreement.
- The cascade control checks the linked pinned instructions and independently enumerates all 64 first-edge delays; each predicts 12 acknowledgements and final poll offset 13,990. The proof binds actual BLL/labels/raw/run/comparison/oracle/descriptor inputs and companion hashes. It explains the current bounded software observation, not the source's `0d` expectation or hardware behavior.
- Timer comparator controls accept every declared alternative (`04` or `05` at index 8; `35`, `36` or `37` at index 11), reject adjacent values outside those sets, reject index14 values `0b` and `0d`, and reject a second mismatch. These captured-data cases are labelled as comparator corpus evidence, not guest execution.

### Completion freshness, record controls and negative controls

- Each `suite.timers.result_controls` invocation builds positive Lynx1/Lynx2 baselines at entry and exit. Each individual mutation receives private copied files; baseline SHA-256 values and positive record validation run before and after every mutation. The current source enumerates 49 cases: one copied-profile identity refusal, nine alternative-set cases, 26 run-record field mutations, three timing-boundary mutations, one all-zero result block, two index14 mutations, one second mismatch, and six raw/comparison shape mutations.
- Each `suite.timers.pending_controls` invocation also captures both real profiles at entry and exit. Its 55 private-copy cases comprise 33 descriptor mutations, 20 companion mutations and two proof mutations. They cover malformed, missing, unknown, duplicate and missing-key JSON; stale schema/version; wrong field type/value; pin/source/tool/wrapper/oracle/build/environment/proof identities; artifact hashes; and corrupt/missing proof. Named validator diagnostics must precede any pending classification.
- The distinct companion contains hashes for raw results, run record, comparison, BLL, labels, oracle, descriptor and proof; it does not self-hash. Validation checks the acyclic identity chain against current files. Actual run paths clear prior outputs before starting; completion-fault controls seed hash-verified prior positive outputs, require the fresh run/watch path to clear them, assert no downstream evidence remains after refusal, and then require fresh positives.
- No-run controls exercise four cases (CPU/timers × both profiles). They arm a private hold before any guest tick or instruction; full CPU/request state and all RAM stay unchanged while exact minimum guest time advances. The validator must reject absent execution before the hold is released; the same fresh instance then must execute and complete.
- Completion controls exercise eight actual guest faults: omitted post-return sentinel and a 256-tick limit for each suite/profile. They prove actual guest execution reached the intended completion/time assertion, require no stale downstream records, and bracket every fault with valid positives. The completion-case manifest asserts exactly eight entries.
- Five semantic mutants use unique `scratch/timer-fault-{id}` roots and do not invoke nested CTest. Each isolated variant is configured and built before execution; only exit1 at the named runtime assertion counts. A clean API positive runs before and after each mutant. The recorded five-test CTest selection passed.

### Inventory and test-maintenance caveat

The read-only `ctest -N` discovery found 49 Phase4-related registered IDs across API, determinism, suite, hygiene and mutant tests. A direct comparison found every discovered ID exactly once in `tests/inventory/fast.txt`; `sh scripts/check-claims.sh --inventory-sha256` returned the bound digest `4c96587c3303c64aaa14874597cc173288eaf155dce9edca465aa3c3893034`.

**Actionable review finding F04-COV-1 — internal subcase inventory can shrink silently.** Current source enumerates 49 result-control cases and 55 pending descriptor/companion/proof cases, while the batch runner checks only `case_count >= 1`. Plan 09's historical result-control inventory records 48, so the current 48→49 change also needs reconciliation. The named cases are present and the recorded aggregate CTests passed; this finding is about missing/duplicate subcases not failing automatically, not an untested current product behavior. Root owns reconciling the count and adding an exact unique case-name manifest with missing/duplicate controls before final qualification. This is an actionable review fix, not manual-only evidence and not a new owner decision.

## Remaining qualification and global limits

- Plan 12 Task2 stays pending until root records full-phase code/security/coverage and UI-applicability outcomes, accepted fixes and targeted retests, and independent acceptance of the pending-gate verifier. Only then run the planned final local gate, Linux arm64 gate and exact-head CI. No broad final gate or CI was run during this audit.
- Global EVD-02 stays open for Phase5 startup/counter and bus/video preconditions and a repeat of the unchanged strict comparison. Do not tune the phase, costs, counter, wrapper or oracle to manufacture `0d`.
- Global EVD-05 remains open for later device-fault controls in Phases5/6. O1/model taxonomy, physical/hardware qualification, descriptor-less prohibitions, rights, licence, publication and release limits remain unresolved. No hardware, firmware, BIOS or commercial content claim follows from these software tests.
- The repository's untracked `.planning/state.json` was left untouched. No source, test, fixture, inventory, requirement, state or qualification-finalization change was made by this audit.

## Finding resolution after repair

F04-COV-1 / WR-01 is fixed at source `27145c8`. Exact unique ordered manifests lock
49 result-control and 61 pending-control names, and explicit missing/duplicate
self-controls fail with their intended diagnostics. Plan 09's48 was historical;
Plan 10 added the copied Lynx II profile case to reach49. The post-review pending
inventory grows55→61 through six output/identity cases. Current aggregate
controls pass; the original audit observations above describe the pre-fix tree.
No CTest IDs changed. Final full-inventory qualification is still pending.

## Final qualification completion

The prior pending Plan 12 obligation is fulfilled. All 25 mapped tasks now have
execution summaries; full macOS/Ubuntu gates and exact-head hosted CI pass the
complete 163-test inventory, exact JUnit/no skips, artifact and history gates.
See 04-12-SUMMARY.md and 04-QUALIFICATION.md. No additional implementation gaps
were found when closing the existing audit; the resolved exact-name warning
is not a human-only item. The initial prequalification task map above is dated
history. Global EVD-02/EVD-05 remain open for their scheduled later slices.

Final WR-02 adds two positive and five malformed pin-marker checks within the
existing oracle ID. They demonstrated old false acceptance and fixed refusal.
All163 inventory cases then passed on the same source `baabaee` in all three
qualification environments. No new registered ID or missing behavior was
introduced; qualification is complete under D-10, with global EVD scopes open.
