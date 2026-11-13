# Phase 7: Suzy Math and Register Mirrors - Context

**Gathered:** 2026-11-13
**Status:** Ready for planning

<domain>
## Phase Boundary

Add deterministic Suzy arithmetic and CPU-visible register/mirror behavior to
the existing core. Deliver register and arithmetic correctness, guest-timed
completion, reset/snapshot continuation, and source-built math/memio evidence.
This extends the current open roadmap; it does not close or archive v1.

The owner selected this recommendation with "follow ur recs" after the inline
progress review. Required planning, implementation, builds, reviews, fixes and
bounded qualification are authorized for this phase. No repeated acknowledgment
is needed to continue those steps.
</domain>

<decisions>
## Implementation Decisions

### Scope and evidence
- **D-01:** Deliver CPU-visible registers/mirrors and byte-write side effects;
  multiply/divide, signed arithmetic, accumulation and flags; deterministic
  busy/completion; reset and save/restore during operations.
- **D-02:** Use independent arithmetic vectors plus the existing MIT pinned
  `math` and `memio` inputs. Preserve exact test inventories, completed guest
  progress and meaningful negative controls. Reuse existing builders and public
  machine APIs; no new dependency or toolchain is needed.
- **D-03:** Keep arithmetic correctness and timing comparison separate. Record
  all source diagnostic results, including timing rows, and report strict
  comparison failures honestly. Do not tune a latency or weaken an expectation
  to obtain a source pass. Whole-suite SUZ-02 stays deferred until its complete
  original math/memio agreement is established; this phase advances its scoped
  arithmetic/register prerequisites without claiming that broader obligation.
- **D-04:** Existing EVD-02 remains open. Physical testing is parked under the
  no-equipment default. Do not restart timer sweeps, ask for a model/loader,
  infer hardware accuracy or perform hardware work.

### Architecture and compatibility
- **D-05:** Keep machine state/event progress deterministic and instance-owned.
  No host time, filesystem, threads, physical input or presentation in the core.
  Keep math progress distinct from later sprite bus ownership.
- **D-06:** Preserve current mapped input behavior and unsupported-address
  refusal proofs. A new supported address requires auditing old mixed-span
  refusal fixtures rather than deleting their failure class.
- **D-07:** Apply the existing explicit snapshot-version/refusal policy to new
  state; update all independent readers/build recipes and ABI assertions at the
  first core/state format change. Verify a mid-operation snapshot and refusal
  before widening consumer behavior. No published external contract is selected
  or changed by this phase.

### Execution and efficiency
- **D-08:** Start with one complete register-to-operation-to-result tracer;
  verify it before expanding arithmetic cases or fixture harnesses. Prefer a
  small number of cohesive vertical plans over per-file plans.
- **D-09:** Reuse prior research, source pins and completed qualification.
  Reconcile stale workflow projections from evidence rather than rerunning
  completed phases. Each handoff scans only its new history; final canonical
  owns the full history scan. Check both installed ShellCheck versions before
  the final source push. Keep generated artifacts private and nonce-owned.
- **D-10:** Keep sprites/painter, Suzy bus-master handoff/SDONEACK, UART,
  cartridge/boot, EEPROM, physical qualification and release outside this phase.
  None is a prerequisite merely because it shares the Suzy address window.
- **D-11:** Use a private instance-owned math device and one completion event,
  independent of bus ownership. Compose math SPRSYS bits with existing input
  orientation in one machine adapter; public mapped inspection is pure.
- **D-12:** Follow original primary register rules and pinned signed quirks.
  Keep low-byte paired clearing distinct from explicit upper-port command/sign
  actions. Use the first-party carry explanation without opening its linked
  emulator implementation. Preserve and document all evidence limitations.
- **D-13:** Select the research's explicit software policies: documented tick
  formulas interpreted as guest master ticks, mathematical unsigned remainder
  where silicon defects are unspecified, refused non-reentrant math writes,
  latched operation inputs/modes and completion-time publication. These remain
  unverified hardware interpretations; never tune them to pass timing rows.
- **D-14:** The first tracer owns snapshot v8, digest-domain/golden evidence,
  independent readers, copied core source lists and changed refusal fixtures.
  Re-establish active hashes through real evidence. Deeper arithmetic and
  fixture expansion follow only after its focused and early broad checks.

### Agent discretion
Research and recommend module boundaries, operation semantics, explicit
software latency policy where hardware evidence is incomplete, fixture startup
and independent vector coverage. Record primary sources and unknowns. The
scope above is locked; implementation recommendations are synthesized after
the bounded hardware and integration/adversarial research fan-out.

The fan-out is complete: the integration advisory is independent; the initial
typed hardware researcher did not return an artifact and was interrupted.
The orchestrator completed bounded primary research and recorded that deviation
in `07-RESEARCH.md`; the planner/checker must challenge its remaining ambiguities.
</decisions>

<canonical_refs>
## Canonical References
- `AGENTS.md` and `.planning/AUTONOMY.md` — authority, stops and efficiency.
- `.planning/phases/07-suzy-math-and-register-mirrors/07-RESEARCH.md` and
  `07-INTEGRATION-ADVICE.md` in the same directory — primary semantics,
  selected software policies and integration/adversarial evidence.
- `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`,
  `.planning/STATE.md`, `.planning/LESSONS.md` — project scope and prior evidence.
- `.planning/debug/evidence/timer-no-equipment-evidence.md` — parked timer gap.
- `.planning/debug/evidence/timer-focused-bus-accounting.md` — exclusive grants.
- `include/lynxplosion.h`, `core/internal.h`, `core/machine.c`, `core/snapshot.c`,
  `core/bus.c`, `core/input.c` — public/private machine and mapped state seams.
- `scripts/build-suites.sh`, `tests/suites/run.cmake`, `tests/suites/tests.cmake`,
  `tests/suites/test_public_suites.c` — existing fixture producer/consumer patterns.
- `build/pins/lynx-tests/math/tests.s`, `build/pins/lynx-tests/math/main.c`,
  `build/pins/lynx-tests/memio/tests.s`, `build/pins/lynx-tests/memio/main.c`,
  `pins.txt`, `fixtures/suites/NOTICE` — pinned source and rights boundaries.
</canonical_refs>

<code_context>
## Existing Code Insights
The Suzy CPU route currently admits mapped input registers only. Timer/audio
and video provide instance-owned scheduled-device patterns; snapshot readers
and private copied-core fault builders must follow source-list changes.
The pinned math suite includes a timing test and VBlank waits; its whole result
must not be equated with pure arithmetic coverage. Memio also exercises
existing palette behavior and math-command aliases.
</code_context>

<deferred>
## Deferred Ideas
Full SUZ-02 hardware-informed timing agreement remains a later obligation if
not established here. Existing timer source disagreement, all owner rights and
release decisions, and the unscheduled later roadmap remain open.
</deferred>
