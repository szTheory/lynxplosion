# Phase 4: Scheduler Integration - Research

**Researched:** 2026-11-10  
**Scope:** Core scheduler, CPU IRQ sampling, sleep and bus-grant seams, snapshot state, and determinism controls. Timer-register semantics and test-fixture rights are left to their parallel research clusters.  
**Confidence:** HIGH for current core/API facts; MEDIUM for the scheduler recommendation; unresolved for hardware-specific same-tick priority ranks.

## Summary

The current core already has the right CPU boundary for this phase: the CPU posts one request as data, and the machine grants, prices, performs, and completes that request. `guest_ticks` is the only machine clock. The current loop has no timer events or stalls and passes zero IRQ lines to CPU completion. The established half-open budget, refusal, post-write watch, and restore contracts are substantial constraints on the new loop. [VERIFIED: `core/cpu.h:117-161`, `core/internal.h:44-59`, `core/machine.c:390-464`, `include/lynxplosion.h:165-229`]

**Primary recommendation:** Keep event selection in the machine layer and use a bounded scan over each device's next candidate event, ordered by the required `(tick, priority, sequence)` key. Advance between events rather than stepping every master tick. Keep the CPU's pending request intact while the scheduler withholds a bus grant. Add the timer bank, sequence state, IRQ state, and any guest-visible sleep/arbitration state to snapshots, and tighten the inherited `cpu_ready_at` restore bound while changing snapshot validation. Do not add a heap or dependency without a measured need. The exact numeric priority ranks and sleep/wake behavior must follow the primary hardware findings; this note does not assign them. [ASSUMED]

## Phase Requirements

| ID | Description | Research Support |
|---|---|---|
| TIM-01 | “One master-tick clock and a scheduler with a total (tick, priority, sequence) order drive all devices” [VERIFIED: `.planning/REQUIREMENTS.md:73`] | Keep the existing machine clock and select eligible CPU/device candidates using the total key. |
| TIM-02 | “Mikey timers form one shared-prescaler bank with linking, DONE, reset-done and borrow semantics” [VERIFIED: `.planning/REQUIREMENTS.md:74`] | The machine scheduler should ask the timer bank for its next event; it must not create a separate timer clock. Exact timer state and edge rules are covered by the hardware research cluster. |
| TIM-03 | “Timer interrupts set pending bits separately from enables, and the CPU samples IRQs per the profile's rules” [VERIFIED: `.planning/REQUIREMENTS.md:75`] | Update timer pending state at its ordered event, derive the IRQ level from pending/enables, and pass the level at the CPU's existing poll point. |
| TIM-04 | “A minimal CPU-sleep and bus-request model exists so later Suzy arbitration does not require rework of the CPU or timers” [VERIFIED: `.planning/REQUIREMENTS.md:76`] | Add eligibility and grant gates around the posted CPU request; keep sleep and bus ownership in machine/scheduler state, not new CPU opcodes. |
| EVD-02 | “A lynx-tests harness reads `g_results` by symbol from the build's label file, requires a completion sentinel and a guest-time minimum and bound, and compares against expectations generated from the pinned source with a count check” [VERIFIED: `.planning/REQUIREMENTS.md:95`] | Scheduler must deliver deterministic guest progress and bounded guest time; harness/source work belongs to the fixture cluster. |
| EVD-03 | “A no-run negative control (CPU halted before tests start) makes every suite fail in the harness” [VERIFIED: `.planning/REQUIREMENTS.md:96`] | Implement the no-run case in a test seam or harness control; do not reinterpret the selected CPU profile or add WAI/STP semantics. |
| EVD-04 | “The harness asserts which model a suite detected and fails when it does not match the selected profile” [VERIFIED: `.planning/REQUIREMENTS.md:97`] | Preserve per-instance profile identity while introducing scheduler state. |
| EVD-05 | “Each device phase adds at least one named-fault control that is shown to fail the intended test” [VERIFIED: `.planning/REQUIREMENTS.md:98`] | Include at least one scheduler/timer fault whose failure identifies the intended timing or IRQ assertion. |

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|---|---|---|---|
| Guest-time event selection | API / Backend (emulator core) | Timer devices | The machine instance owns the clock and currently owns the bus loop. [VERIFIED: `core/internal.h:44-59`, `core/machine.c:390-464`] |
| Timer bank and IRQ source state | API / Backend (emulator core) | CPU request completion | Timer state owns pending/enables; the machine supplies the sampled line to the CPU. [VERIFIED: `.planning/REQUIREMENTS.md:73-76`, `core/machine.c:443`] |
| CPU sleep and bus request gating | API / Backend (emulator core) | CPU request data | The CPU already posts a request separately from the grant; arbitration can delay service without changing instruction microstate. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:60-68`, `core/cpu.h:117-161`] |
| Snapshot and digest state | API / Backend (emulator core) | Host observation | Guest scheduling state belongs in the snapshot; host watch configuration remains a separate observation. [VERIFIED: `core/internal.h:35-59`, `core/snapshot.c:35-42`] |
| Public suite results | Test harness | Core guest RAM | The harness reads guest-written results and checks profile/time; it does not own the machine clock. [VERIFIED: `.planning/REQUIREMENTS.md:95-98`] |

## Project Constraints (from AGENTS.md)

- Treat project-owned decisions and current source as authority; keep observations, measured results, inference, recommendations, and unknowns distinct. Recheck mutable claims before making them requirements. [VERIFIED: `AGENTS.md:3-6`, `AGENTS.md:29-31`]
- Keep deterministic emulation separate from host clocks, filesystems, threads, A/V presentation, and persistence policy; define instance ownership, guest-time bounds, reset/unload, errors, and refusal postconditions. [VERIFIED: `AGENTS.md:52-55`]
- Do not assume one universal Lynx profile. Preserve model/profile specificity and leave baseline selection open. [VERIFIED: `AGENTS.md:59-60`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:39-52`]
- Keep dependencies small and flat; prefer a small understandable helper over a new dependency when its lifetime and supply-chain cost exceed its benefit. [VERIFIED: `AGENTS.md:61-64`]
- Do not use, commit, or distribute rights-bearing BIOS, game, save, manual, overlay, or diagnostic content without documented permission; a hash does not establish rights. [VERIFIED: `AGENTS.md:65-68`]
- Use fresh isolated state, semantic guest progress, exact test inventories, meaningful negative controls, and visible skips. Keep the documented fast local gate as the PR gate; add CI only for a distinct recurring failure, measuring critical-path cost first. [VERIFIED: `AGENTS.md:72-82`]
- Optimize only a measured bottleneck, with outputs held constant. Preserve the current owner stops on copyrighted content, hardware testing, publication, licensing, and release authority. [VERIFIED: `AGENTS.md:38-41`, `AGENTS.md:83-90`]

## Standard Stack

| Component | Current use | Recommendation |
|---|---|---|
| C | The project compiles C17 with extensions disabled and warnings as errors. [VERIFIED: `CMakeLists.txt:1-14`] | Keep the scheduler and timer state as plain integer data in the core. |
| CMake / CTest | Existing build and exact test registration. [VERIFIED: `CMakeLists.txt:96-100`, `tests/api/tests.cmake:9-50`] | Extend the current targets and inventory. |
| External packages | None proposed for this scheduler work. [ASSUMED] | Do not install a general-purpose event library; use a small internal selector unless measurement changes the tradeoff. [ASSUMED] |

The project build source includes `set(CMAKE_C_STANDARD 17)`, `set(CMAKE_C_EXTENSIONS OFF)`, and `-Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror`. [VERIFIED: `CMakeLists.txt:1-14`]

**Installation:** None. No external package is recommended, so the package-legitimacy audit is not applicable to this cluster.

## Observed Baseline and Contracts

The machine instance stores CPU state, a cost model, `guest_ticks`, `cpu_ready_at`, and host-only watch observations. The source quotes `uint64_t guest_ticks;       /* the only clock */` and `uint64_t cpu_ready_at;      /* guest tick at which the CPU's next cycle may be granted */`. [VERIFIED: `core/internal.h:44-59`]

The CPU holds one pending request, and `lynx__cpu_complete` is the state transition after a request is served. The Phase 2 decision says “stalls, sleep and arbitration (RDY-like) are ‘the loop does not serve the request yet’,” and rejects a CPU bus vtable. [VERIFIED: `core/cpu.h:117-161`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:60-68`]

The current machine loop passes no interrupt lines; the exact source statement is `lynx__cpu_complete(&m->cpu, table, data, 0u); /* no interrupt lines before Phase 4 */`. CPU polling is attached to request flags, and the Phase 2 context requires devices to be caught up to the poll stamp before sampling. [VERIFIED: `core/machine.c:443`, `core/cpu.c:747-794`, `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:92-96`]

The profile policy explicitly says “On both models `$CB` and `$DB` are 1-byte NOPs that do not poll IRQ” and “Neither is a W65C02S: no WAI/STP exist.” [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:39-42`]

`lynx_advance` uses a half-open guest-time budget: events/actions at the target must remain for the next call, consumed ticks cannot exceed budget, and a cycle granted before the target may complete its bus action even if its duration ends after the target. The API source states “A cycle that starts before T is performed even if its duration extends past T; the next call resumes when it ends” and “A cycle due exactly at T is not started.” [VERIFIED: `core/machine.c:390-464`, `include/lynxplosion.h:165-204`]

Write-watch is a host observation. The public contract says a hit is returned only after “the bus write, CPU request completion and next-ready tick are committed”; it records the grant tick, and the source says “The return tick is grant+1”. Successful restore clears host watch state after guest state is applied, while a refused restore leaves the instance unchanged. [VERIFIED: `include/lynxplosion.h:206-229`, `core/snapshot.c:35-42`, `core/snapshot.c:347-368`]

The current snapshot format is `#define LYNX__SNAPSHOT_FORMAT 3u`. It serializes guest time and CPU-ready time. Restore currently uses `if (ready < ticks || ready - ticks > 255u) {`, and Phase 3 explicitly tracks an accepted forged 255-tick ready span as an inherited weakness. The current cost table quotes flat5 as `{{5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}}` and test_skew as `{{4, 6, 3, 10, 7, 9, 8, 11, 12, 13, 14}}`; therefore the present 255-tick allowance exceeds both current models' cycle prices. [VERIFIED: `core/internal.h:13-24`, `core/snapshot.c:24-33,331-345`, `core/bus.c:22-27`, `.planning/LESSONS.md:117-120`]

## Architecture Patterns

### Recommended event selection

Do not advance each device once per master tick. Keep each device's next event and stable sequence with its guest state, then scan candidates and choose the least event under the required total key. This is O(number of candidate sources) per dispatched event, independent of the elapsed tick gap. It avoids heap maintenance and stale timer entries while the device set is small; the tradeoff is linear selection cost as the number of independently scheduled devices grows. This is a design recommendation, not a measured performance result. [ASSUMED]

Every event gets its sequence when scheduled, not from the order in which the candidate scan happens to visit modules. Compare candidates lexicographically by `(tick, priority, sequence)`. Make priority assignment an explicit table and obtain the same-tick ranks from the primary hardware research; iteration order is not evidence. Store enough sequence state to recreate identical ordering after restore, and reject or stop before sequence wrap can change the order. [ASSUMED]

Treat CPU bus completion as a scheduler event at the bus grant/sample stamp. Before completing a request marked POLL, advance all device events that sort before that poll; after that, pass the derived IRQ level to `lynx__cpu_complete`. A timer edge that sorts after a poll must not be observed by that poll. The source contract requires device catch-up at the poll tick, but hardware evidence must settle the edge/write/poll priorities. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:92-96`; ASSUMED for the proposed ordering mechanism]

Keep eligibility separate from CPU instruction state. A CPU candidate is serviceable only when its cycle is ready, sleep is clear, and the bus grant is available. A blocked request remains byte-for-byte pending while the master clock advances through timer/device events. Releasing the gate must not replay an already-completed bus write or call CPU completion twice. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:60-68`; ASSUMED for the concrete gate shape]

Timer state owns the shared prescaler phase and link/cascade state. The scheduler asks for the next timer-bank event; it does not independently clock every timer or reconstruct the shared phase from host time. The hardware research cluster must define the event payload and ordering for underflow, cascade, DONE, reset-done, and IRQ effects. [VERIFIED: `.planning/REQUIREMENTS.md:73-76`; ASSUMED for the module seam]

~~~text
Candidate ordering: lexicographically compare (tick, priority, sequence).
CPU grant gate: cycle is ready AND sleep gate is open AND bus grant is available.
Advance rule: dispatch only events with tick < target; leave an event at target pending.
~~~

The key and half-open rule are verified requirements; the candidate types and gate fields above are proposed design language, not existing API names. [VERIFIED: `.planning/REQUIREMENTS.md:73`, `include/lynxplosion.h:169-178`; ASSUMED]

### Conceptual flow

~~~mermaid
flowchart TD
  H[Host supplies bounded guest tick budget] --> S[Machine scheduler computes exclusive target]
  S --> C[Select least eligible event by tick, priority, sequence]
  C -->|CPU grant| B[Perform posted bus request]
  B --> P{CPU request polls IRQ?}
  P -->|yes| I[Sample timer IRQ state after earlier same-tick events]
  P -->|no| R[Complete CPU request]
  I --> R
  C -->|timer event| T[Advance shared timer-bank state and pending IRQ bits]
  C -->|sleep or external bus request| W[Withhold CPU grant; retain request]
  T --> C
  W --> C
  R --> C
  C -->|no event before target| O[Return at target or documented stop]
~~~

This is a conceptual event flow; the numeric priority ranks and timer edge semantics remain source-backed decisions for the hardware cluster. [ASSUMED]

### Scheduler option comparison

| Option | Strength | Cost / failure mode | Disposition |
|---|---|---|---|
| Step every device every master tick | Simple local reasoning and direct tick traces. | Work scales with the entire requested budget, including idle spans; this can turn guest time into an unbounded per-tick loop. | Reject for production scheduling; keep only tiny test oracles if useful. [ASSUMED] |
| Bounded scan of next-event candidates | Small fixed implementation, no allocation, stable comparison, no stale queued events; snapshots store device state rather than a separate derived queue. | O(D) candidate comparisons per event; a large future device set may make this measurable. | Recommend now; measure before replacing it. [ASSUMED] |
| Binary min-heap | O(log D) insert/remove and scales better when many unrelated events are active. | Requires fixed capacity or allocation policy, stable sequence assignment, cancellation/generation handling for reloads, and heap serialization or exact reconstruction. A stale entry or restore-order difference can silently change hardware behavior. | Strong alternative if profiling shows candidate scans are a bottleneck; do not choose without that evidence. [ASSUMED] |

## Snapshot, Ownership, and Refusal Guidance

The existing format is build-bound and validates all data before applying it; preserve that ordering. If Phase 4 adds timer/prescaler, scheduler sequence, IRQ pending/enable, or sleep/arbitration state, bump to a new snapshot format (recommend the next version) and serialize every field needed to reproduce future event order. Do not serialize the host watch range or hit; keep clearing it only after successful restore. [VERIFIED: `core/snapshot.c:35-45`, `core/snapshot.c:237-368`; ASSUMED for the new layout/version]

Prefer recomputing candidate deadlines from validated guest device state instead of serializing a duplicate heap. Any sequence value that changes tie order is guest state and must round-trip. A refused restore must preserve the entire caller-owned machine byte-for-byte, including timer state and watch configuration. Reset and successful content load should return all scheduler/timer state to the documented power-on state; deinit should keep the existing caller-owns-buffer model. [VERIFIED: `core/machine.c:83-163`, `core/snapshot.c:237-368`; ASSUMED for added fields]

Tighten the inherited ready-span check while adding snapshot fields. The current acceptance condition allows a ready span as large as 255 ticks, while existing bus prices are much smaller and the lesson records an accepted forged snapshot. Keep bus wait/sleep deadlines in separate scheduler fields so they do not make `cpu_ready_at` appear reachable; validate the CPU-ready span against the reachable cost-model bound. Add a hash-correct malformed snapshot control that tries the old oversized span. [VERIFIED: `core/snapshot.c:325-345`, `.planning/LESSONS.md:117-120`; ASSUMED for the proposed bound test]

For guest-time arithmetic, retain the existing upper headroom and use checked arithmetic when forming event deadlines or sequence numbers. Validate candidate time is not earlier than the scheduler's last processed event except for an explicitly documented carry/edge rule. A corrupted restored deadline or wrapped sequence must not permit event order to move backward. [VERIFIED: `core/internal.h:31-33`, `core/machine.c:407-410`; ASSUMED for new scheduler state]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---|---|---|---|
| General event library | A new package, callback registry, or general-purpose scheduler abstraction | A private candidate comparator and bounded scan | The current C core forbids allocation and keeps plain per-instance state; no measured scale need supports a dependency. [VERIFIED: `core/machine.c:5-7`; ASSUMED recommendation] |
| CPU sleep or arbitration opcodes | New WAI/STP or reserved-opcode behavior | Machine-level grant gating around the existing pending request | Project profile policy says those instructions do not exist in this CPU contract; later bus masters should reuse the grant gate. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:39-42,60-68`] |
| Host-side event timing | Wall-clock callbacks, threads, or host timers | Integer guest timestamps owned by each machine instance | This preserves deterministic runs and independent machine state. [VERIFIED: `AGENTS.md:52-58`, `core/internal.h:44-59`] |

## Common Pitfalls

1. **Only advancing timers when the CPU is ready.** A CPU cycle can span a budget boundary and the CPU can be blocked; timer events still due before the budget target must run. Check split-budget equality while a bus cycle is in flight and while arbitration is blocking CPU grants. [VERIFIED: `include/lynxplosion.h:169-178`; ASSUMED test guidance]
2. **Processing an event at the exclusive target.** A request due exactly at T belongs to a later advance. Test zero budget and one-tick budgets around CPU-ready, timer-edge, sleep-release, and IRQ transitions. [VERIFIED: `include/lynxplosion.h:169-178`, `tests/api/test_api.c:1636-1671`; ASSUMED extensions]
3. **Using array traversal as priority.** Refactoring module order could change guest behavior. Test equal-tick collisions and make an intentional priority mutation fail the specific ordering assertion. Numeric rank values must come from primary device evidence. [VERIFIED: `.planning/REQUIREMENTS.md:73`; ASSUMED test guidance]
4. **Sampling IRQ too early.** Sampling on request post or before all earlier same-tick events violates the existing poll seam. Keep pending bits separate from enables, derive the line when polling, and compare with CPU scenario vectors under each supported profile. [VERIFIED: `core/cpu.c:625-633,747-794`, `core/machine.c:443`; ASSUMED test guidance]
5. **Conflating sleep with a stalled bus.** Sleep and arbitration can both prevent grants, but their release conditions may differ. Keep explicit state/reason and test that a pending request is neither dropped nor repeated; settle actual wake behavior from hardware evidence. [ASSUMED]
6. **Returning from a watch before same-tick events are drained.** Advancing the public guest tick to grant+1 while an event at the watched write's grant remains queued can make the next call process an earlier timestamp. Preserve the documented watch timestamp and return tick while draining all same-timestamp events in the established order before returning. [VERIFIED: `include/lynxplosion.h:206-224`; ASSUMED scheduler integration]
7. **Copying transient queue state into snapshots.** A heap plus timer-owned deadline fields can disagree after restore. Either derive candidates from canonical device state or validate and serialize one canonical queue representation, never two competing sources of truth. [ASSUMED]

## Code Examples

Use a single comparison rule for candidate ordering: first event tick, then the documented priority rank, then scheduling sequence. Give each candidate a stable sequence when it is created; never derive tie order from which module the scan visits first. Exact priority ranks are intentionally omitted until the primary hardware findings settle them. [VERIFIED: `.planning/REQUIREMENTS.md:73`; ASSUMED implementation detail]

Keep the request/grant separation explicit: the scheduler may defer a pending CPU request, but only a successful bus grant performs the bus action and calls CPU completion. At a poll request, calculate the IRQ line from current timer pending and enable state after earlier ordered events. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:60-68,92-96`, `core/machine.c:434-444`; ASSUMED for timer line composition]

## Validation Architecture

The existing regression framework is CTest with exact inventory registration. The local gate script says “The single local command and the CI gate: scripts/check.sh”. [VERIFIED: `CMakeLists.txt:96-100`, `tests/api/tests.cmake:9-50`, `scripts/check.sh:1-5`]

Existing public-path tests include the exact source entries “api.advance_budget” and “api.advance_slicing” [VERIFIED: `tests/api/tests.cmake:9-10`], “api.snapshot_refusal”, “api.snapshot_roundtrip”, and “api.snapshot_continue” [VERIFIED: `tests/api/tests.cmake:45-47`], and “api.watch_controls”, “api.watch_independent”, and “api.watch_post_write” [VERIFIED: `tests/api/tests.cmake:48-50`]. Extend these seams rather than creating a parallel runner.

| Requirement area | Required automated evidence |
|---|---|
| TIM-01 | Equal-tick candidate tests prove stable total order; edge tests at target−1, target, and target+1 prove half-open behavior; coarse, one-tick, and irregular budget slicing reach identical guest state and digest. [ASSUMED] |
| TIM-02 | Shared-prescaler phase, linking, DONE/reset-done/borrow tests and snapshot/restore continuation use the source-backed expectations from the timer research. [VERIFIED: `.planning/REQUIREMENTS.md:74`; ASSUMED test decomposition] |
| TIM-03 | Independently vary pending and enable bits; prove IRQ line and CPU behavior at the profile's poll point; include a same-tick timer/write/poll collision once source priorities are settled. [VERIFIED: `.planning/REQUIREMENTS.md:75`, `core/cpu.c:747-794`; ASSUMED tests] |
| TIM-04 | Block and resume a pending CPU request without duplicate bus effects; verify timers continue while CPU grants are blocked; test the named sleep/bus-request fault in the intended assertion. [VERIFIED: `.planning/REQUIREMENTS.md:76`; ASSUMED tests] |
| EVD-02..05 | Keep harness no-run, model mismatch, completion/time-bound, count, and named-fault tests in the suite harness workstream. [VERIFIED: `.planning/REQUIREMENTS.md:95-98`] |
| Snapshot | Save at timer edge, in-flight CPU cycle, pending IRQ, and blocked-request states; restore into a fresh matching instance; continue to a common guest target and compare every guest observation/digest; prove malformed, stale-version, or impossible scheduler state refuses byte-identically. [VERIFIED: `core/snapshot.c:237-368`; ASSUMED new cases] |
| Watch | Hit a guest write that collides with a timer edge; preserve the grant hit stamp, grant+1 return stamp, and post-write CPU continuation; rearm and compare with uninterrupted execution at the same final guest tick. [VERIFIED: `include/lynxplosion.h:206-229`; ASSUMED collision case] |

Use all supported profiles where the behavior is profile-dependent, without making a baseline or broad suite-pass claim before the baseline-profile decision is resolved. Keep scheduler tests deterministic, isolated, included in the exact inventory, and paired with a fault that fails for the expected reason. [VERIFIED: `AGENTS.md:59-60`, `AGENTS.md:72-75`, `.planning/REQUIREMENTS.md:17-19,97-98`]

## Security Domain

Project configuration sets `"security_enforcement": true`, so this research includes the security domain. [VERIFIED: `.planning/config.json:50`]

The current stable OWASP ASVS is version 5.0.0; it is defined for web applications and web services. Its current names place input validation and business logic under V2, memory/string/unmanaged handling and safe deserialization under V1, authentication under V6, sessions under V7, authorization under V8, and cryptography under V11. [CITED: [OWASP ASVS 5.0.0 overview](https://asvs.owasp.org/), [official ASVS 5.0.0 CSV](https://github.com/OWASP/ASVS/blob/v5.0.0/5.0/docs_en/OWASP_Application_Security_Verification_Standard_5.0.0_en.csv)]

This phase extends a local C library and guest machine, not a web application; apply ASVS input/deserialization concerns by analogy to API arguments and untrusted snapshot bytes. Authentication, web sessions, authorization, and transport controls are not present in this phase's core boundary. This scope classification is an inference from the local C API and project boundary. [VERIFIED: `CMakeLists.txt:1-7`, `include/lynxplosion.h:1-14`; ASSUMED applicability mapping]

| ASVS 5.0.0 category | Applies | Standard control for this phase |
|---|---|---|
| V1 Encoding and Sanitization | Yes, by analogy | Validate serialized timer, IRQ, deadline, and sequence values as untrusted data; reject impossible states before applying. |
| V2 Validation and Business Logic | Yes, by analogy | Validate guest-time arithmetic, candidate ordering, snapshot consistency, and API ranges. |
| V6 Authentication | No | The core adds no user identity or login boundary. |
| V7 Session Management | No | Machine instances are guest state, not authenticated web sessions. |
| V8 Authorization | No | The core adds no user/role access-control model. |
| V11 Cryptography | Limited | Reuse the existing payload digest for corruption detection; do not claim it authenticates a snapshot. The current snapshot layout lists a payload SHA-256 field and no key/signature field; treating it as integrity rather than authenticity is an inference. [VERIFIED: `core/snapshot.c:7-33,228-296`; ASSUMED security interpretation] |

| Threat pattern | STRIDE | Standard mitigation |
|---|---|---|
| Forged snapshot with impossible timer deadline, pending-bit combination, or sequence | Tampering | Validate every field and cross-field invariant before mutation; add a hash-correct invalid-state control. [VERIFIED: `core/snapshot.c:237-368`; ASSUMED new state] |
| Deadline or sequence arithmetic wraps | Tampering | Checked additions/subtractions, explicit headroom, and refusal before order or time can wrap. [VERIFIED: `core/internal.h:31-33`; ASSUMED scheduler fields] |
| Scheduler ties depend on source-file iteration order | Tampering | Total key comparison and a named fault that swaps a same-tick rank. [VERIFIED: `.planning/REQUIREMENTS.md:73`; ASSUMED control] |

## Environment and Dependencies

No new runtime, database, service, package, or host clock is required for the scheduler design. The existing project build/test toolchain and pinned suite fixtures remain in use; fixture/source and tool availability should be handled by the suite research cluster. No new package legitimacy audit is required for this cluster. [VERIFIED: `AGENTS.md:52-64`, `CMakeLists.txt:1-7`; ASSUMED scope]

## Assumptions Log

| # | Claim | Section | Risk if wrong |
|---|---|---|---|
| A1 | A bounded scan over next-event candidates is the simplest adequate event selector at current scale. | Summary, Architecture | If event counts are already high, selection overhead may become measurable; profile before changing. |
| A2 | Sequence numbers are assigned at scheduling time and form part of guest snapshot state. | Architecture, Snapshot | If the hardware/order model defines sequence differently, deterministic ordering or snapshot identity changes. |
| A3 | Sleep and bus arbitration can be represented as separate machine-level grant gates without new CPU opcodes. | Architecture | Incorrect wake semantics could delay IRQ response or alter CPU microstate. |
| A4 | Snapshot format should advance one version when timer/scheduler state is added. | Snapshot | A missed guest field could make restore diverge despite a valid digest. |
| A5 | The current `cpu_ready_at` 255-tick allowance should be narrowed to a reachable bus-cost bound in this snapshot change. | Snapshot | If future bus timing semantics allow longer cycle durations, validation must use the actual configured bound. |
| A6 | Watch returns should wait until all events at the watched grant tick are drained, preserving the existing grant+1 public return timestamp. | Pitfalls, Validation | If this changes the established watch contract or timer priority, watch/budget continuation can become order-dependent. |

## Open Questions

1. **What exact priorities order a timer edge, CPU register write, IRQ update, and CPU poll at the same guest tick?** The requirement demands a total order, but the exact numeric ranks are hardware semantics. Use the primary timer/IRQ sources and named suite evidence; do not choose ranks from implementation convenience. [VERIFIED: `.planning/REQUIREMENTS.md:73-75`; unresolved source evidence]
2. **What event does minimal sleep wait for, and what clears it?** Preserve the existing no-WAI/STP CPU profile. Choose an internal test seam or device-controlled sleep gate only after the timer/IRQ evidence states the wake condition. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:39-42,60-68`; unresolved hardware semantics]
3. **Does a bus request delay the grant only, or extend a cycle's duration/sample?** Keep arbitration delay separate from `cpu_ready_at` unless primary bus evidence says the bus cycle itself is lengthened. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:70-82`; unresolved future timing semantics]
4. **Which new guest scheduler fields must be visible through public register observations?** Existing project policy says “every snapshot field is readable through the public API and the observation digest covers all state”; preserve that completeness without leaking host watch state. [VERIFIED: `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:98-103`, `include/lynxplosion.h:69-82`, `core/snapshot.c:35-42`; API shape open]
5. **How much event work may occur at one timestamp, and what bound prevents sequence exhaustion?** Establish from the fixed device set and linked-event behavior; validate the bound during snapshot restore. [ASSUMED]

## Sources

### Primary local sources (HIGH confidence)

- `.planning/REQUIREMENTS.md:73-76,95-98` — exact timer/scheduler/IRQ/sleep and Phase 4 harness requirements; opened this session.
- `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-CONTEXT.md:39-42,60-96` — profile policy, posted-request CPU/bus seam, budget contract, and IRQ sample point; opened this session.
- `core/internal.h:13-59`, `core/cpu.h:117-161`, `core/machine.c:390-464` — current instance, pending request, clock, grant loop, and snapshot format; opened this session.
- `core/cpu.c:625-633,747-794`, `core/snapshot.c:35-45,237-368`, `include/lynxplosion.h:165-229` — IRQ poll logic, restore/refusal, budgets, and write-watch semantics; opened this session.
- `tests/api/tests.cmake:9-50`, `CMakeLists.txt:1-14,96-100`, `scripts/check.sh:1-5` — current regression registration, C17/CTest configuration, and local gate; opened this session.
- `AGENTS.md:38-90`, `.planning/LESSONS.md:117-120` — owner stops, deterministic-state policy, validation/CI guidance, and inherited snapshot weakness; opened this session.

### Official documentation (MEDIUM confidence)

- [OWASP ASVS 5.0.0 overview](https://asvs.owasp.org/) — current version and standard scope.
- [OWASP ASVS 5.0.0 official CSV](https://github.com/OWASP/ASVS/blob/v5.0.0/5.0/docs_en/OWASP_Application_Security_Verification_Standard_5.0.0_en.csv) — current chapter/category names.

## Metadata

**Confidence breakdown:**
- Existing code and public contracts: HIGH — source of truth files were opened and cited.
- Candidate-scan recommendation and test decomposition: MEDIUM — fit to present bounded core structure, but no benchmark has been run.
- Same-tick priority and sleep/wake behavior: LOW / unresolved — must follow the primary hardware and suite findings.

**Valid until:** Recheck hardware priority, sleep/wake, and bus timing before converting them into plan decisions; the local core contracts remain current until changed.
