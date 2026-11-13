# Focused timer and bus investigation

Date: 2026-11-13. Owner approved the recommended focused investigation after
the v1 audit; this is not acceptance of the missing strict comparison. The
starting source is `c893827bb80ee88222c70e04290c6f04838c609b`.

## Result and remaining gap

Found and repaired a real exclusive-bus violation: a pending video or refresh
request could begin inside the priced interval of an already granted CPU
cycle. Higher priority now selects the next owner after CPU release. The
strict timer disagreement remains: legacy counts12 and video-policy1 counts15
against source13 on both profiles. No oracle, exception, timer divider,
page-mode eligibility or DMA/refresh cost changed.

Row14 is a zero-page interrupt acknowledgment **counter**, not a timer status
register. Pinned Test6 polls Timer5 pending192 times, acknowledges each observed
pending bit, increments zero-page01, and stores that counter into result14.
The wrapper demonstrably initializes it to zero. Historical CRT entry state at
that byte is still unknown; see the earlier
[provenance investigation](timer-suite-cascade-count.md).

## Fast reproduction and hypotheses

The existing source-built guest, pin, wrapper, oracle and owned scratch protocol
were reused. No new toolchain, ROM, physical test or emulator implementation
source was used. Two fresh legacy runs reproduced the identical strict failure
in0.28–0.30 seconds; the video run took0.45 seconds. Commands after the standard
suite build/oracle setup:

```sh
build/check/tests/lynx-test-suites run timers lynx1 "$suite_build" "$oracle"
build/check/tests/lynx-test-suites observe timers-video lynx1 "$suite_build" "$oracle"
```

Both exit1 specifically with `timer source oracle comparison mismatch`; the
comparison record contains only `result 14 0c EXPECT(0d) source-mismatch` or
`result 14 0f EXPECT(0d) source-mismatch`. This explicit red loop, prior
instruction-budget minimization,64-phase enumeration and actual link-placement
controls were reused before forming further hypotheses.

Three ranked hypotheses were shown to the owner before probing:

1. NOP-only page-mode eligibility overprices the polling loop. Prediction:
   allowing all otherwise eligible opcode fetches reduces the video count.
2. Display/refresh ownership contributes excess stalls. Prediction: independently
   suppressing their grants separates the stall sources.
3. Historical startup preconditions remain necessary. Prediction: removing the
   identified model limitations does not establish the unchanged source13 by
   itself, and original initial-counter/layout/state evidence remains missing.

Private copied-source probes changed one factor at a time, with two combined
controls to distinguish interaction. Source guest bytes and expectations stayed
unchanged. These altered policies are **diagnostic counterfactuals**, not
qualified builds or candidate hardware policies; their inherited build identity
must not be used as qualification. Tagged tracing covered only the Test6 address
window and CPU/video/refresh grant boundaries. Private sources, hashes, raw logs,
parsed events, scripts and exit records are retained in the focused investigation
temporary bundle (`lynx-timer-focused-yiw4dxdu`).

## Quantified observations before repair

Each row below has192 polls. The interval begins with the successful Timer3
enable write and ends at the final INTSET read. Sum of posted CPU cycle prices
plus ownership delay equals the elapsed interval exactly in every row.

| Private policy | Acks | CPU price ticks | Ownership delay | Elapsed ticks | Video byte grants | Refresh grants |
|---|---:|---:|---:|---:|---:|---:|
| Unchanged video-policy1 |15|14140|2489|16629|480|61|
| All otherwise eligible opcode fetches |14|13329|2555|15884|480|59|
| Suppress display DMA |12|13990|55|14045|0|55|
| Suppress refresh |15|14140|2793|16933|560|0|
| All opcodes, suppress display DMA |12|13230|159|13389|0|53|
| All opcodes, suppress refresh |14|13315|2795|16110|560|0|

The all-opcode control saves761 priced ticks in its measured interval but also
changes the number of acknowledgments and the earlier guest/display phase.
Therefore these rows are not additive corrections to one fixed trace. In
particular removing refresh changes alignment enough to admit another DMA row;
its elapsed time grows. No constant tick padding follows from this table.
Page-mode expansion alone cannot close the comparison, and display ownership
dominates the modeled difference. No tested counterfactual independently
establishes source13.

An independent interval scan found57 overlaps inside the unchanged baseline's
enable-to-last-poll window. For example a CPU operand grant at283135 owns five
ticks through283140, yet refresh was granted at283136. Both private corrected
profile traces have zero overlapping grants across the traced address window;
both still count15. This is a directly demonstrated software invariant defect,
independent of the unresolved hardware schedule.

## Repair and regression

`machine_advance` clamps video and refresh candidates to the outstanding CPU
release tick before comparing candidates with each other, timers, samples,
the next CPU request and the half-open budget. Timer/audio work still proceeds
on time during ownership. No extra serialized state is needed: `cpu_ready_at`
already records the outstanding interval. Existing exact-build snapshot
compatibility applies; this corrects the documented policy rather than adopting
new hardware costs.

The regression extends existing `api.video_bus_edges`, without adding a CI job
or changing the registered inventory. A real NOP stream grants both five-tick
normal and four-tick eligible opcode cycles. On both profiles, video-only,
refresh-only and simultaneous requests arrive at every interior tick. Assertions
check no preemption before release, an exclusive target, full five-tick grants,
video-before-refresh ordering after release, and unchanged posted CPU work until
the owners finish. The neighboring video/refresh/CPU tie fixture now starts at
the actual preceding operand cycle's release, preserving its original tie proof.

The expanded regression was linked against the copied original scheduler:
exit1 with the intended `video.bus.cpu-cycle-not-preempted` assertion. With the
repair it exits0 (`ok 1 - api.video_bus_edges`). The initial affected group
passes16/16, including timer, input, frame-stop, page-mode and snapshot cases.
Required review and canonical qualification are recorded separately below.

Independent review then identified a newly reachable snapshot boundary: a
request delayed during a CPU cycle could be overdue at a public return or
exactly at CPU release, while the old video validator knew only device owners
as blockers. A new real JMP-loop regression reproduced restore refusal before
the validator repair. The validator now accepts that CPU-delay case only when
the pending request is less than five ticks before the recorded release and
no intervening video/refresh owner exists. It also rejects simultaneous future
CPU and device releases. No serialized field or format version changed.

The added `api.video_snapshot` regression uses the natural256-tick refresh and
real CPU cycles for refresh-only, DMA-only and combined requests. It saves and
restores both inside the CPU interval and at exact release, checks the first
device grant, and compares a500-tick coarse continuation against one-tick calls.
Freshly hashed hostile snapshots exercise the age bound and overlapping CPU/
device owners. The regression is red against the preceding validator at the
intended round-trip assertion and green after repair. The complete affected
group passes16/16 after these changes.

The first canonical run at646b3ba accounted for all246 registered cases and
found exactly one failure: the clean bracket of `video.fault.dma_cost_zero`
still started its synthetic DMA burst before the preceding operand released.
The same boundary adjustment preserves that control's original tie assertion.
After correction its actual compiled mutant and clean brackets pass; independent
review accepts the fixture repair. The reviewed followup source is
`7db8ef6`; final qualification is in [the qualification record](timer-bus-qualification.md).

Direct strict runs after the repair still exit1 with exactly one disagreement:

| Profile | Legacy completion/count | Video-policy1 completion/count | Source count |
|---|---|---|---|
| lynx1 |359941 /12|367529 /15|13|
| lynx2 |359916 /12|367504 /15|13|

All15 other meaningful timer rows agree. The changed video completion times
are observations from the repaired schedule, not blind replacements for the
historical Phase5 evidence.

## Source boundary and next decisive evidence

Rechecked Epyx's [CPU/ROM chapter](https://www.monlynx.de/lynx/lynx4.html),
[system bus appendix](https://www.monlynx.de/lynx/lynx11.html) and
[display chapter](https://www.monlynx.de/lynx/lynx5.html). They describe exclusive
ownership, controller-selected transfer points, opcode-only page-mode reads,
and the original normal-color setup. They still do not supply the complete
opcode decode exclusions or DMA/refresh cadence and grant-transition schedule.
The linked HTML contents do not include the bus state-machine appendix referred
to by the bus chapter. The current display setup also remains01 versus the
CRT's0D, whose additional mode bits are unsupported. These are constraints and
unknowns, not permission to choose constants that return13.

The next decisive input is an independently sourced bus/page-mode schedule or
an authorized hardware trace binding the Test6 binary, initial zero-page byte,
display state and CPU/video grants to its count. The existing physical-testing
stop remains in force. Repeating phase/layout/provenance searches or sweeping
DMA costs until13 appears would not supply that evidence. EVD-02 and milestone
closure remain open.

A [concrete next evidence proposal](timer-next-evidence-protocol.md) is prepared
for owner review. It does not start or authorize physical execution.
