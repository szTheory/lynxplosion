# Timer Test6 timing adjudication

Diagnosis-only, 2026-11-11. No product/source/oracle edits, commits, new dependencies, CRT/TGI execution or hardware tests. Executor-owned modifications remain intact. Temporary listings, reference link output, controlled builds and raw logs are kept outside the repository; upstream assets are not included here.

## Established result

The current deterministic model correctly predicts the observed **12** acknowledgments. Neither ordinary CPU branch relocation nor a different initial64-tick prescaler phase can turn this execution into13. This establishes the software mechanism; it does **not** establish which physical behavior or upstream test precondition is responsible for the difference from the unchanged13 oracle.

### Exact instruction and timer accounting

Test6 uses Timer3 BKUP/CNT16, selector2 (64 master ticks), reload; Timer5 starts at0, linked to3, interrupt-enabled with held reset-DONE. A Timer3 reload therefore occurs every17 source edges,1088 ticks. The existing trace shows every reload reaching Timer5 and no missing acknowledgment within the loop.

Current `core/bus.c` prices all CPU requests at5 ticks, samples at grant (`sample=0`), and returns the RAM timing region for every address. There are no hardware-register wait states, RAM page-mode pricing or video bus holds in this run.

| No-IRQ iteration | CPU cycles |
| --- | ---: |
| LDA INTSET (AD81FD) | 4 |
| AND #20 (2920) | 2 |
| BEQ noIRQ, taken, same page (F007) | 3 |
| DEX (CA) | 2 |
| BNE poll, taken, same page (D0EF) | 3 |
| Total | 14 |

An IRQ iteration has BEQ not taken (2), then LDA immediate2, STA INTRST4 and INC zero-page5; total24 cycles. Thus consecutive polls differ by70 or120 ticks. Parsing both existing bus traces independently gives192 polls,12 acknowledgments,179 gaps70 and12 gaps120. The first INTSET read is20 ticks after the enable write; final read is13990 ticks after it:

`last_poll = enable +20 +191*70 +12*50 = enable +13990`.

Let `d` be the first enabled source-edge delay,1..64. Expiry13 is:

`expiry13 = enable + d +(13*17 -1)*64 = enable +d +14080`.

Even `d=0` would be90 ticks too late. A grant/end-of-cycle sampling shift of5 ticks cannot bridge that gap. Independent instruction-level enumeration of all64 first-edge offsets returns12 every time, with final poll13990. This does not change or validate the project's fixed-phase or same-tick hardware policy.

| Profile | Enable | First-edge delay | Final poll | Expiry13 | Gap |
| --- | ---: | ---: | ---: | ---: | ---: |
| lynx1 |279290|6|293280|293376|96|
| lynx2 |279310|50|293300|293440|140|

### Original link geometry versus controlled placements

Verified existing cc65/ca65/ld65/lynx.lib binaries against `.lynx-build-files`. Manually compiled unchanged pinned main.c and tests.s with the original Makefile's compiler/assembler flags. Linked `tests.o main.o lynx.lib` against pinned `lynx.cfg`; upstream Makefiles and resulting CRT/TGI/cart image were never executed. This reconstructs the original **build recipe using the project-pinned toolchain**, not the author's unidentified historical binary.

| Layout | CODE | Test6 | Poll | BEQ→target | BNE→target | t5_count |
| --- | --- | --- | --- | --- | --- | --- |
| Current wrapper |0273|03DC|0400|0405→040E|040F→0400|zero-page01|
| Original recipe / pinned tools |0298|0401|0425|042A→0433|0434→0425|zero-page01|
| Deliberate synthetic crossing |0363|04CC|04F0|04F5→04FE|04FF→04F0|zero-page01|

Both genuine layouts keep both branches within the page of the post-operand PC and retain the exact instruction bytes/addressing modes. This refutes the proposed branch-page-cross / absolute-versus-zero-page explanation for the original/current comparison.

Before controlled execution, independent timing predicted12 for the original placement and13 for a deliberately crossing BNE placement (an added5 ticks on each of191 taken BNEs; final poll14995 including the additional IRQ path). Temporary linker configurations changed CODE placement only; untouched wrapper and test source were reassembled. The existing harness observed:

| Placement | lynx1 result14 | lynx2 result14 | Source comparisons |
| --- | --- | --- | --- |
| Original CODE0298 |0C|0C|Only result14 disagrees on both|
| Synthetic CODE0363 |0D|0D|All16 agree on both|

Synthetic crossing is a falsification control, **not a remedy**. It shows why oracle agreement alone would not establish timing correctness. Neither source, padding, CPU pricing, timer policy nor expected values were changed in the project. Completion/probes remained present; these remain raw software observations with hardware accuracy and profile qualification unresolved.

## Original runtime preconditions differ

Pinned `cc65/libsrc/lynx/crt0.s` is linked before main. Its register/value tables initialize:

- Timer0 BKUP9E, CTLA18 (enabled/reloading, selector0).
- Timer2 BKUP68, CTLA1F (enabled/reloading, linked to0).
- PBKUP29 and DISPCTL0D.
- MAPCTL0C, which leaves sequential page-mode enabled in hardware.

The primary [Epyx register appendix](https://www.monlynx.de/lynx/hardware.html) identifies DISPCTL bit0 as video-DMA enable and MAPCTL bit7 as sequential-cycle disable. Therefore the original CRT requests video DMA before `main()` calls `run_tests()`, even though main's TGI installation occurs after the tests. The tier-A wrapper and current no-video core do not reproduce this bus-contention state. `timers/tests.s` resets timers3/5 only. Supporting pinned `timers2/tests.s` explicitly disables display/timer0/timer2 noise before timing and restores DISPCTL0D afterward. The runtime difference is directly supported; its **exact numerical effect on Test6 has not been measured or independently modeled**.

The primary [timer chapter](https://www.monlynx.de/lynx/lynx8.html) supports N+1 reload cadence and phase-dependent first use. The primary [audio chapter](https://www.monlynx.de/lynx/lynx7.html) describes shared timer/audio register slots and possible waits up to1 microsecond. That is insufficient to assign an arbitrary delay to **INTSET**, which is not a timer/audio bank register. Indeed the pinned `timers2` Test9 expects the same aggregate envelope for ordinary RAM, INTSET/INTRST and SERCTL accesses. Adding a constant Mikey delay merely to get13 is unjustified.

Pinned README reports real-hardware analysis but does not supply the exact Test6 measurement binary, toolchain revision, machine/profile, display phase or bus trace. Pinned CI clones cc65 mutable HEAD without selecting a revision. No checked-in `.sym`, `.map` or `.lnx` was found in the fetched source tree. The continuation below recovers the release build's toolchain from retained logs; this does not identify the author's original measurement environment. No release binary was downloaded.

Primary CPU/system-bus chapter URLs initially failed through direct browser open; the continuation below recovered both through contents links/direct HTTP. No substitute emulator implementation was consulted. The test-source video guide's introduction indicates mixed hardware and emulator-model provenance; its timing claims were not used to justify a core change.

## Bounded provenance continuation

Completed the authorized read-only source/history/metadata checks on 2026-11-11. No source-suite, toolchain, CRT, emulator or hardware execution; no binary downloads, pin changes, dependencies or commits. Only this report and the active debug record were updated.

### Expectation history

The exact-pin GitHub path histories contain [eight assembly commits](https://api.github.com/repos/drhelius/lynx-tests/commits?path=timers/tests.s&sha=b583dc43d532d3c41563a6fa35420614fbcead89&per_page=100) and [six C commits](https://api.github.com/repos/drhelius/lynx-tests/commits?path=timers/main.c&sha=b583dc43d532d3c41563a6fa35420614fbcead89&per_page=100). All listed patches were inspected.

- [bf4253f, 2025-09-26](https://github.com/drhelius/lynx-tests/commit/bf4253f7d37a14f978e7c3ab62beb40eb7ca44cc) introduces Test6 and its C expectation together: `$C0` polls, Timer3 backup/count `$10`, source2, zero-page `INC`, expected `$0D`.
- [ff4362e](https://github.com/drhelius/lynx-tests/commit/ff4362e63a505bdabf203a151ff52ca48e3d952b) renames labels and reorganizes shared build files/result reservation. [34f2ee9](https://github.com/drhelius/lynx-tests/commit/34f2ee93f399385bb26108cf4e66276bc54a3962) changes earlier Test1 and another expectation; preceding code can shift Test6 placement. [fa33577](https://github.com/drhelius/lynx-tests/commit/fa3357782357b19c3997639c2f2bdfdc214115cd) changes comments. No later Test6 loop instruction change was observed.
- This refutes the narrow hypothesis that `$0D` predates a subsequent edit of the loop count or Test6 instruction sequence. It does not establish that the introduced oracle was measured correctly, or that earlier-code changes preserved historical link geometry.

### Release build provenance recovered

The [annotated 1.4.9 tag](https://api.github.com/repos/drhelius/lynx-tests/git/tags/652007fb84a62be18cd82749d18696f767f7bdbf) resolves to the exact project source pin. [Run31751695357](https://github.com/drhelius/lynx-tests/actions/runs/31751695357) has successful Linux and Release jobs.

The [Linux job](https://github.com/drhelius/lynx-tests/actions/runs/31751695357/job/94618621837) logs compile cc65 with build identity `Git 547d92358`; the primary [cc65 commit lookup](https://api.github.com/repos/cc65/cc65/commits/547d92358) resolves this to `547d923588d870aacf0b0016c67d0f6a92a70f83`. This differs from the project pin. The log also records original flags, `tests.o main.o lynx.lib` order and `-Ln timers.sym` generation. The [workflow](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/.github/workflows/build.yml) archives only `.lnx` files; generated symbols are not retained in that artifact.

The [artifact metadata](https://api.github.com/repos/drhelius/lynx-tests/actions/runs/31751695357/artifacts) reports artifact9201182930 with SHA-256 `59ceee79867a15630b1d3d02a73529077a140677f44b91b409e50dba18386781`. The [Release job](https://github.com/drhelius/lynx-tests/actions/runs/31751695357/job/94618921785) verifies that digest before making the release ZIP. [Release metadata](https://github.com/drhelius/lynx-tests/releases/tag/1.4.9) reports ZIP SHA-256 `1d9d9de3e500643f763d6b1c9e1847fdce93d840990230b945983359d2b6f15c`. These are observed metadata/log links, not locally verified binary contents or hardware evidence. No assets were downloaded.

The recovered toolchain's [CRT](https://github.com/cc65/cc65/blob/547d923588d870aacf0b0016c67d0f6a92a70f83/libsrc/lynx/crt0.s) and [link configuration](https://github.com/cc65/cc65/blob/547d923588d870aacf0b0016c67d0f6a92a70f83/cfg/lynx.cfg) are text-identical to the project-pin versions. This strengthens the previously observed CRT precondition difference. It does not prove identical linked bytes or locate the Test6 hardware measurement. Source README/guide, release notes, workflow and commit comments supplied no per-Test6 measurement/map/initial-state record; the bounded timer issue search returned no issues.

### Primary bus prerequisites recovered

The canonical [Epyx CPU/ROM chapter](https://www.monlynx.de/lynx/lynx4.html) explicitly warns that instruction-count software timing depends on other hardware. Opcode page-mode reads can use4 ticks; normal RAM and available-hardware accesses use5. Page-mode eligibility depends on opcode and surrounding state; video/refresh can pause the CPU. Device-specific DTACK latency does not authorize arbitrary waits on INTSET.

The [Epyx system-bus appendix](https://www.monlynx.de/lynx/lynx11.html) defines exclusive ownership with priority Video, Refresh, CPU, then Suzy. Enabled video/refresh requests regularly pause CPU at the controller's appropriate transition. These establish real prerequisites absent from flat5/no-DMA, not a complete numerical stall schedule. Page-mode speedups and bus holds can oppose each other; neither chapter derives exactly13 for this loop.

### Additional initial-data precondition

The pinned [Test6 source](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers/tests.s) contains only three `t5_count` references: zero-page reservation, increment and final load. It never explicitly clears the counter. The recovered CRT calls [zerobss](https://github.com/cc65/cc65/blob/547d923588d870aacf0b0016c67d0f6a92a70f83/libsrc/common/zerobss.s), which clears the BSS interval, while the linker allocates zero page separately with no file data. The project wrapper clears all zero page.

This proves an unspecified source-level initial-data precondition, not its value on the author's machine: loader, constructors and entry RAM were not traced here. A result byte is the initial counter plus acknowledged events modulo256. Therefore historical `$0D` alone does not prove13 acknowledged events unless the initial counter was zero. Initial1 would be an additive explanation of a12-event result, but there is no evidence it was1. Seeding1, editing the suite, or treating this as a proven upstream bug would manufacture agreement.

### Disposition and confidence

**High confidence:** the present12-event mechanism; no later Test6-loop edit orphaning the expectation; recovered release build identity; source-level CRT/bus and scratch-state precondition gaps. **Unresolved:** original measured counter value, measured linked image/layout and real bus schedule; no confirmed normative root cause or safe product fix.

Keep the strict comparison visibly failing and Task2 halted. Least disruptive next step is to carry the exact comparison as blocked into scoped video/bus prerequisite planning, including an explicit initial-zero-page contract, rather than tune Phase4 timing. A decisive record must bind source, linked layout, startup scratch state and display/page-mode state to the observed acknowledgment count. The recovered release toolchain alone is insufficient.

The parent owns the scope decision: schedule source-backed opcode page-mode eligibility, refresh/display bus grants and original CRT-equivalent register/phase setup with independent traces and boundary/snapshot tests under Phase5, or explicitly replan those prerequisites into Phase4. No new implementation is authorized by this diagnosis. Physical measurement, firmware use, baseline adoption and publication retain their existing owner gates. No user acknowledgment is needed to preserve the stop; no author contact was sent.

## Unresolved cause and recommended next evidence

The leading candidate set spans **fixture/runtime preconditions**, **missing hardware bus behavior**, and **unspecified initial counter data**: a fixed instruction-budget source expectation is being applied to a flat5/no-DMA execution with deterministic zero-page initialization. The release toolchain is now identified, but original measured layout/startup state and an incorrect/over-specific oracle remain alternatives. There is no evidence for a dropped cascade, a16-edge reload requirement, a CPU instruction-cycle defect, or a phase-only fix.

Keep the strict0D comparison failing and Task2 halted. Do not adopt the synthetic placement or retune counters, phase, bus costs or expected values. The smallest next decisive evidence would be a provenance-matched author measurement/map or a source-backed independent CPU/page-mode/refresh/video-DMA timing model reproducing the original CRT state and explaining13. A focused upstream source record may establish those preconditions without running new hardware; ordinary source research needs no owner acknowledgment. Implementing real video arbitration moves into the already scheduled video/bus work and must be scoped by the parent workflow. Physical qualification and O1 remain owner-gated.

No safe product fix is confirmed by this diagnosis. The session remains unresolved and unarchived.
