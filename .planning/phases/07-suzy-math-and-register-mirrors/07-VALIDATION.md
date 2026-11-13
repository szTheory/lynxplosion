---
phase: 07
slug: suzy-math-and-register-mirrors
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-11-13
---

# Phase 7 validation strategy

This is planned coverage, not execution evidence. New checks are created in
their owning implementation task before its verification command runs. Existing
checks and source pins are reused. No physical UAT is included; equipment is
unavailable and that track stays parked. No acknowledgment is needed to execute
the approved software phase.

## Requirement and task proof map

| Requirement | Owner | Automated IDs and independent evidence | Threat ref | Planned timeout |
|---|---|---|---|---|
| SMR-01 | 07-01 task1, 07-02 task1 | `suzy.tracer`, `suzy.registers`; actual mapped guest vs pure CPU-memory peek; both windows, paired clearing vs explicit commands; `memory.map_matrix`, `api.memory_peek_pure`, `api.timer_unavailable`, `api.input_orientation`, `api.input_edges`; `suzy.fault.decoder` | T-07-01, T-07-05 | 60s behavioral; 360s copied fault |
| SMR-02 | 07-02 task1, 07-03 task1 | `suzy.arithmetic`, `suzy.flags`; independent uint64 model and hand-derived signed/zero/8000/stale-latch/overflow/divide0 vectors; `suite.math.observation.lynx1/lynx2` gates arithmetic0..13; `suite.memio.observation.lynx1/lynx2` gates register0..12; `suzy.fault.result` | T-07-05, T-07-09, T-07-11 | 60s behavioral; 180s suite observations; 360s fault |
| SMR-03 | 07-01 task1, 07-02 task2 | `suzy.snapshot_tracer`, `suzy.lifecycle`, `suzy.snapshot_refusal`, `det.suzy_paths`, `det.suzy_sensitivity`; coarse/sliced/instances, reset/load, target-before/on/after, same-tick CPU read, headroom/busy refusal; rehashed forged snapshots and complete suffix comparisons; `suzy.fault.completion`, `suzy.fault.snapshot` | T-07-02, T-07-03, T-07-06, T-07-07 | 60s behavioral; 360s copied faults |
| SMR-04 | 07-03 both tasks, 07-04 both tasks | Four source-built observations; `suite.math.controls`, `suite.memio.controls`, `suite.math.source_controls`, `suite.math.strict_comparison_controls`; source/meaningful/reserved shape and identity, fresh model/progress/watch/return, bounded completion, no-run/stale/wrong/missing rows, exact strict timing status, selection/subcase inventories; reviewed native full Debug canonical, final GNU owning optimized sanitizer/assertion-enabled groups, and exact-commit hosted full Debug CI | T-07-08..T-07-16 | 180s observation; 240s grouped controls; all existing case limits retained |

New registrations are owned as follows (all belong in exact sorted fast.txt):

- 07-01: `suzy.tracer`, `suzy.snapshot_tracer`, `det.suzy_sensitivity`.
- 07-02 task1: `suzy.registers`, `suzy.arithmetic`, `suzy.flags`.
- 07-02 task2: `suzy.lifecycle`, `suzy.snapshot_refusal`, `det.suzy_paths`,
  `suzy.fault.decoder`, `suzy.fault.result`, `suzy.fault.completion`,
  `suzy.fault.snapshot`.
- 07-03 task1: `suite.math.observation.lynx1`,
  `suite.math.observation.lynx2`, `suite.memio.observation.lynx1`,
  `suite.memio.observation.lynx2`.
- 07-03 task2: `suite.math.controls`, `suite.memio.controls`,
  `suite.math.source_controls`, `suite.math.strict_comparison_controls`.

There are 21 planned new fast IDs. Before accepting any focused run compare
CTest discovery with its full named expected selection, then use
`--no-tests=error`. Nonempty discovery alone does not establish every named
case exists. Configure's existing inventory comparison and canonical JUnit
completeness checks still own global exact membership and no-skip rules.
Control groups also validate exact subcase order/uniqueness/count.

## Executable verification commands

The exact owning commands live in each PLAN's `<verify>` block, with checkout
root as working directory. The established command pattern is:

`cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^(suzy\.(tracer|snapshot_tracer|registers|arithmetic|flags|lifecycle|snapshot_refusal)|det\.(suzy_paths|suzy_sensitivity))$'`

Source capture command:

`ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(math|memio)\.observation\.(lynx1|lynx2)$'`

Source/adversarial command:

`ctest --preset check --no-tests=error --output-on-failure -R '^suite\.(math\.(controls|source_controls|strict_comparison_controls)|memio\.controls|selection_controls|completion_controls|audio\.source_controls)$'`

Plan 01 task2 owns one early full registered inventory after the first broad
mapped/state/source-list change and focused fixes:

`cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure`

Run the single early full GNU inventory in the existing clone after actual
checked-commit git fetch/git merge --ff-only, exact HEAD and clean tracked-tree
verification. Inspect and start the named existing container only if stopped.
Use this explicit configuration; build/test presets consume it without
reconfiguring Debug:

`docker exec lynx-phase5-task12-linux-reuse sh -lc 'cd /work/lynxplosion && cmake --preset check -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O2 -g -UNDEBUG" && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure'`

Plan 01's full executable command additionally asserts the effective Ninja
compile commands contain O2, UNDEBUG and ASan+UBSan and do not define NDEBUG;
record cache and effective compile/link flags retaining non-recoverable
sanitizers and assertions. This optimization addresses demonstrated GNU Debug
timeouts without changing limits, sanitizers, assertions or the global preset.
No source overlays, new container or dependency install is authorized by it.

Subsequent diagnosed repairs repeat affected groups. Every GNU configure
retains the explicit RelWithDebInfo/-O2 -g -UNDEBUG override; never invoke GNU
scripts/check.sh or a plain check configure, which reset Debug. Do not repeat
the known GNU Debug full timeout or the early optimized full inventory. Plan 04
owns final full native Debug `sh scripts/check.sh`, final GNU owning Suzy/source/
reader/control groups with effective-flag assertions and ShellCheck0.9, and
successful full hosted Debug ci.yml at exact pushed HEAD. Its PLAN contains
the exact focused GNU/CI commands. Preserve all existing test/script limits;
measure both environments and diagnose attributable failures before bounded
retests. No foreground activation is needed. Broad registered/canonical checks
can exceed one minute and require background execution plus factual updates.

## First-change and evidence-binding invariants

The tracer owns snapshot8's complete88-byte canonical Suzy envelope, payload
78196, digest-domain4 and actual native/GNU golden re-establishment. Idle
pending fields normalize to zero. Equal-target deadlines remain reachable
because the established advance interval is half-open; pure peek and
zero-budget calls never finish an event. Snapshot semantic controls correct
the hash first, and all refusal comparisons cover every machine byte.

The first change also updates independent API/diagnostic readers, ABI/payload
assertions, normal/plain sources and the audio/timer/video copied-core lists.
Generic mixed-span refusal moves to FDFF..FE00 under MAPCTL02; Suzy holes
FC30/FC70 remain separate controls. JOYSTICK/SWITCHES and lefthand retain
their original input/grant owner; SPRSYS validation/composition occurs once.

Every inventory-owning task re-establishes `det.two_runs`,
`det.two_instances`, `det.golden`, `docs.claim_template` and
`docs.claim_template.self_test` before refreshing
`docs/claims/skeleton-repeatability.claim`. New digest input is proved
sensitive before accepting the new golden. Locate active helper identity and
mutation bindings together, then run their owning semantic checks before
changing any digest. Historical qualification reports remain unchanged.

## Source inventory and claim boundary

Math: all8 groups run, each2 bytes; 18-byte raw result, arithmetic meaningful
offsets0..13, timing-test status/diagnostic14..15, reserved16..17. Memio:
all3 groups run, sizes4/4/5 at0/4/8; raw18, meaningful0..12 and reserved13..17.
Unused source initializer entries do not redefine group coverage. Both CPU
profiles are independently selected and observed through the guest model probe.

After source expansion the exact required suite selection is14 IDs, covering
seven suite families on both profiles, and the control selection is12 IDs.
Existing page-mode skips and strict timer disagreements remain exact. All raw
math timing bytes and strict mismatches are written before verdict; separate
arithmetic/register acceptance never implies whole-source or hardware agreement.
Timing-control positives require an all-source-agreeing record; any real or
seeded mismatch remains a strict failure with exact row provenance. No fixed
mismatch allowlist, oracle edit, parameter fit or omission is permitted.

## Ownership, security and capability gates

Use the existing producer/consumer and nonce-owned scratch. Source and derived
object identity/rights/notice/recipe are recorded; no BIOS/commercial assets,
new package, GPL emulator code, physical test or release is in scope. Raw logs
are unique private files preserving exit/status/stage; mutable CTest logs are
supplementary only. Successful generated outputs clean up after canonical
containment/nonce/owner checks; failures remain attributable. Existing scanner
fields govern binary ownership, with preflight before final canonical runs.

Each automated command has its own sibling fails_when describing observable
nonzero exit/assertion/identity conditions. Plan 01 explicitly asserts native
ShellCheck0.11.0 and GNU0.9.0 identity; that command establishes identity only,
while Plan 03 and the final GNU command perform actual changed-script lint.

ASVS1 applies; each plan has concrete trust boundaries and unique T-07-01..16
mitigations, plus reserved T-07-SC. Unresolved high/critical software threats
block qualification. No npm/pip/cargo install is planned. Native ShellCheck0.11
and GNU ShellCheck0.9 both check changed scripts. Independent code/security/
integration review precedes final canonical qualification.

AI integration, UI design/review, external API coverage and live DB schema push
gates are N/A: a private C math device and source-test harness add none of those
surfaces. Assumption detector returned detected:false; no identity promotion is
needed. Snapshot/domain changes follow the already approved compatibility
policy and are costly/reversible, requiring no new owner decision.

Plan 03 has an explicit orchestrator-selected 16-file cohesive-seam exception:
producer/oracle/wrappers/rights/consumer/driver/inventory and claim binding form
one auditable executable evidence path. Splitting one file into a fifth plan
solely for a threshold would duplicate qualification without a complete slice;
all ownership remains visible. Plan 01 retains its equivalent state-format seam
exception. Four sequential plans remain the approved structure.

Full multi-source coverage audit is in 07-04-PLAN.md's `<source_audit>`;
GOAL, SMR-01..04, every scoped RESEARCH constraint and D-01..D-14 are planned.
SUZ-02 whole-suite agreement, EVD-02 closure, physical qualification and
rights/release decisions remain deferred. No planned test result is asserted
as observed. Keep compliance flags false until actual execution establishes
their corresponding evidence; there is no separate foundation-only Wave0.
