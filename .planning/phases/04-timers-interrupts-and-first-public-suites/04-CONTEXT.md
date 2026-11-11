# Phase 4: Timers, Interrupts and First Public Suites — Context

**Gathered:** 2026-11-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver TIM-01..04 and EVD-02..05: one guest-clock scheduler, a shared-prescaler Mikey timer bank and IRQ routing, a minimal CPU bus-request/sleep seam, snapshot continuation, and headless source-built `cpu`/`timers` observations with independent completion/model/count controls. Real video/DMA/input and audio remain Phases 5/6; neither cartridge boot nor firmware is in scope.
</domain>

<decisions>
## Implementation Decisions

Standing authority: the owner requested autonomous remaining-phase work and delegated discuss fan-out/synthesis/auto-follow in `.planning/AUTONOMY.md`. The decisions below are orchestrator recommendations selected under that authority, not new owner decisions. Tests and apps run headlessly/in the background; no focus activation is needed.

### Scheduler and event boundaries
- **D-01:** Use an instance-owned, bounded next-event scan, not a heap or per-master-tick loop. Order candidates by `(tick, priority, sequence)` with an explicit stable sequence rule; do not let traversal order silently break ties. Keep half-open advance budgets and no overshoot. Adopt device/timer catch-up before a same-tick CPU bus action, then that action's IRQ poll/CPU completion; identify this complete tie order as deterministic project policy where hardware evidence is incomplete. Handle cascades and all earlier-priority same-tick work before exposing a watch stop at grant+1. Preserve a posted CPU request while grants are withheld. Define overflow/exhaustion and idle behavior without unbounded loops.

### Timer bank and unsupported behavior
- **D-02:** Implement all eight timers' source-backed standalone divider/count/reload behavior on one free-running shared phase. Preserve phase across count/source/enable writes; do not restart an independent timer clock. Reset to a declared deterministic fixed-zero phase policy and bind the policy/version to run evidence; this is not a claim about physical reset phase. Implement documented owned links 0→2→4 and 1→3→5→7. Do not invent timer6's incoming link, an undocumented timer0 input, or audio3 feedback while audio is absent: enabled unsupported link modes use the established explicit unavailable/refusal contract. Leave future audio feedback as a named seam.
- **D-03:** DONE gates counting; reset-DONE is treated as held control, not a one-shot pulse, with write/edge behavior and inference labels explicit. Derive detailed borrow/reload/IRQ transitions from the primary register/variance notes and compare with the pinned timer oracle before committing implementation assumptions. Record any remaining tie/bit ambiguity as policy, never silently tune expected values to make a suite comparison agree. Timer4 expiry never substitutes for UART readiness; UART remains unsupported. Raw RAM and side-effect-free mapped inspection stay separate, and unimplemented Mikey/Suzy registers remain unavailable.

### IRQ and bus-request/sleep seams
- **D-04:** Timer interrupt enables gate new hardware-source latch events; IRQ level is driven by nonzero pending bits, not pending AND the current timer-enable mask. Disabling a source does not clear a pending bit. FD80 acknowledges selected bits and FD81 can force selected bits; pure reads show the same pending latch. CPU I masks acceptance, not the pending latch or wake request. Reuse the existing per-profile POLL semantics and source annotations, including CB/DB NOPs without WAI/STP. Test IRQ before/at/after a poll and while a cycle is in flight.
- **D-05:** Deliver the minimal request/grant/sleep model, with a private deterministic bus-hold/wake test seam as needed, without fabricating active Suzy. Primary documentation warns CPUSLEEP is broken without Suzy owning the bus. Do not claim working hardware sleep merely by pausing on FD91 with no Suzy; leave unsupported guest behavior explicit. The planner must choose and document the narrow guest-visible boundary after the wrapper spike shows actual required accesses. No new public debug API exists solely to halt a negative control.

### State, ownership and observation
- **D-06:** Extend the canonical guest snapshot for timer/shared-phase/IRQ/request state and any true pending-event sequence state, bumping its format under the existing same-build/version-refusal policy. Avoid two competing queue/deadline representations. Validate reachable values, cross-field/time bounds, overflow and unsupported states before any apply/observer clear. Tighten the inherited 255-tick CPU-ready allowance to the actual reachable cost bound, accounting for stalls separately rather than blessing an arbitrary span. Preserve whole-instance refusal, host-watch exclusion and successful-restore observer clearing. Require uninterrupted/sliced/watched/restored equality at common ticks. Declare any golden update in a plan and independently verify unchanged CPU/RAM/time observations first.

### Source suites and trustworthy completion
- **D-07:** Use only the existing exact lynx-tests commit/archive pin and existing pinned cc65. Fetch upstream inputs into temporary/build storage; commit original recipes/wrapper/generator/manifest/notices, not upstream source archives or BLL binaries. First prove an original tier-A wrapper can link untouched `cpu/tests.s` and `timers/tests.s` without display/CRT device setup. A minimal licensed display-runner adaptation is fallback only, with exact diff/identity; test logic is not edited. Preserve full BLL identity and actual linker symbols. A build/provenance tracer must precede depending on these fixtures.
- **D-08:** Generate fail-closed expectation/count records from the pinned C oracle and assembly extents. CPU has 18 results; timers reserves 18 bytes but has 16 meaningful results, with accepted alternatives preserved. Read actual symbols and verify bounds/duplicates/overlaps before RAM reads. Every suite needs guest-executed model evidence, not a copied host selection; add a small source-audited probe/wrapper if the suite lacks one. Run only explicitly selected lynx1/lynx2 suite profiles; `none` remains the overall default and is not silently reinterpreted as a detected model. Require a wrapper-written post-test sentinel, nonzero semantic progress, a source-reasoned minimum and bounded maximum guest time. First-run measurement qualifies bounds; it does not invent them or change upstream results. The no-run control really prevents test instructions, then fails completion; wrong-model, zero/unexecuted result, count drift, symbol corruption and DONE/reset-DONE faults each fail their intended assertion. Store raw observations separately from source-oracle comparisons and any hardware claim.

### Scope, rights and delivery
- **D-09:** Required public suites are `cpu` and `timers`. `timers2` is source evidence for phase/enable-order behavior; add a bounded focused check only if it catches a distinct needed failure, otherwise list it explicitly as out of this phase's required set. Page-mode/audio/audio2 remain later. O1 stays open and blocks profile/suite-pass claims; raw selected-profile observations and software oracle controls can proceed. O5 still blocks project licence/release decisions; existing owner authority for this public repository permits its normal original code/metadata commits, not a new artifact publication channel. Correct the suite memo's overbroad public-artifact wording using that recorded authority. O6 remains draft and no emulator implementation source is consulted. Reconcile provenance's incorrect tag description and exact MIT copyright/notice identity. No firmware, commercial content, physical tests, release, new dependency or full-data vector rerun absent a new relevant CPU concern. Keep the current fast gate as the one regression/local qualification command; run final macOS/Linux qualification and exact-head CI, measure critical-path work before changing caches/concurrency.

### Owner-approved timing comparison deferral

- **D-10 (owner, 2026-11-11):** The owner selected the recommendation in
  04-09-DISPOSITION.md with "follow ur recs". Finish the Phase 4 harness/control
  slice with only timers result offset 14 pending under the provisional
  flat5/no-DMA environment: observed 0C versus unchanged source 0D, on both
  profiles. Keep the strict comparison failing, with all sixteen rows and raw
  records intact. Distinguish completed observations from source agreement in
  commands, test names, records and reports. A Phase 4 observation gate may
  recognize exactly this documented pending row/environment; every other
  mismatch, missing result, identity/model/count/time error or new failure
  remains a gate failure. A generic nonzero exit or WILL_FAIL on a positive
  suite test is insufficient. Require a separate exact-diagnostic control for
  the known strict mismatch and independent timing proof for the provisional
  observed count. Carry exact agreement into Phase 5 with startup/counter and
  bus/video preconditions explicit; keep global EVD-02 open until its remaining
  agreement evidence is resolved. Do not tune phase, costs, layout or counter
  initialization to manufacture 0D. This lifts only the disposition stop;
  O1 and all rights/physical/release gates remain in force. Remaining execution,
  no-run, model, alternative-set and named-fault controls still must complete.

### Agent discretion

Concrete private module/field names, bounded event representation, exact named test IDs and plan slicing are implementation choices. Resolve technical uncertainties through the declared source/fixture experiments; escalate a materially new scope/rights/authority/cost choice, not routine builds, format changes or checks covered by this phase.
</decisions>

<canonical_refs>
## Canonical References

- `AGENTS.md`, `.planning/AUTONOMY.md`, `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`, `.planning/LESSONS.md` — current policy, scope and carried lessons.
- `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md` — fixed profiles, CPU requests/POLL/budget and evidence labels.
- `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-CONTEXT.md`, `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-09-SUMMARY.md`, `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-VERIFICATION.md` — BLL, mapped/unavailable, watch, identity and validated source baseline.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-TIMER-RESEARCH.md` — primary hardware source index and remaining uncertainty.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-SCHEDULER-RESEARCH.md` — current-core seams and failure modes; its pending/enables shorthand is superseded by D-04.
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-SUITE-RESEARCH.md` — exact source/rights/oracle observations; D-09 preserves the existing public-repository authority.
- `pins.txt`, `docs/provenance.md`, `docs/policy/clean-room.md`, `docs/policy/exposure-record.md`, `docs/memory-startup.md`, `fixtures/diagnostic/bll.cfg` — tool/source pins, provenance, supported startup and original fixture pattern.
</canonical_refs>

<code_context>
## Existing Code Insights

Reuse the cycle-stepped request/POLL seam in `core/cpu.h`/`core/cpu.c`, pure map decoder in `core/bus.c`, bounded loop/watch paths in `core/machine.c`, validate-before-apply serializer in `core/snapshot.c` and public `include/lynxplosion.h`. Extend the existing API/determinism/diagnostic CTest harness, exact inventory and claim hash. The first diagnostic/consumer remain regression consumers. No frontend is delivered.
</code_context>

<specifics>
## Specific Evidence and Limits

Primary Epyx documentation establishes separate pending acknowledgement/forcing and warns that full sleep depends on bus ownership. It does not provide the complete software tie order. The pinned source supplies software comparison expectations, not independent hardware qualification. In particular, broad timers2 phase envelopes do not choose a physical reset phase or timer6 link. The scheduler/request and wrapper traces must expose these distinctions in tests and documentation.
</specifics>

<deferred>
## Deferred Ideas

Real Suzy arbitration/sprites, display DMA/video/input, audio ring feedback/samples, UART/ComLynx, hardware timing calibration, baseline adoption and licence/release decisions remain their scheduled or owner-gated work. No outstanding todo was folded into this discussion.
</deferred>
