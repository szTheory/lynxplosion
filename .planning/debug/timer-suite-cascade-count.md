---
status: diagnosis_complete
trigger: "Autonomous Phase 04-09 tracer: timer result 14 is 0C versus pinned source 0D on both profiles."
created: 2026-11-11
updated: 2026-11-11
---

## Current Focus

bug_class: bohrbug
known_pattern_candidate: A fixed instruction-budget oracle may depend on original CRT/video/page-mode environment.
hypothesis: Current flat5/no-DMA and zeroed-scratch runtime differs materially from original CRT/video/page-mode preconditions and unspecified initial t5_count; exact cause of13 remains unproven.
candidate_causes: [config: omitted CRT display-DMA precondition, code: unmodeled page-mode/refresh/video bus timing, environment: original measurement binary/layout not identified despite recovered release toolchain, data: unspecified initial zero-page t5_count or over-specific oracle]
and_gate: Possible; accurate page-mode speedups and video/refresh stalls may jointly determine the oracle's13. Exact contribution unresolved.
test: Bounded read-only provenance investigation complete; no product fix or new runtime execution performed.
expecting: Author map/measurement or independent source-backed bus model distinguishes runtime precondition from oracle error.
next_action: Parent preserves strict0D comparison and Task2 halt, carries blocked comparison into scoped video/bus planning with explicit initial-ZP contract, or obtains an explicit phase-scope decision to bring those prerequisites forward. No user action is required merely to retain the stop.

## Symptoms

- Expected: untouched pinned timer suite completes and all 16 meaningful results agree with its generated oracle; result 14 expects 0D.
- Actual: both profiles complete with guest probes A1/A2 and 15/16 accepted values; result 14 is 0C. Completion ticks are 359941 (lynx1) and 359916 (lynx2).
- Error: strict source comparison remains failing. Earlier wrapper IRQ storm is separately fixed only at the saved post-call continuation, preserving pending and in-suite frames.
- Timeline: first completed Phase 04-09 BLL timer runs; no prior completed timer comparison.
- Reproduction: build pinned suite and oracle, then run the existing lynx-test-suites timer CLI. Existing raw/comparison records are in build/check/scratch/suites/manual-timers/build. Temporary bus traces are named lynx1-timer-bus-trace.log and lynx2-timer-bus-trace.log in the temporary directory.

## Evidence

- Bounded continuation inspected all eight commits in pinned timers/tests.s path history and all six timers/main.c commits through the pin. Test6's $C0 loop, Timer3 $10/source2, zero-page INC and expected0D were introduced together in bf4253f (2025-09-26). Subsequent Test6 changes rename labels/comments, while preceding Test1 changes may shift link placement. No observed loop edit orphaned an older expectation. Historical binary geometry and oracle correctness remain unproven.
- GitHub release/workflow metadata establishes that exact-pin run31751695357 built and packaged 1.4.9, with named artifact and release-zip digests. The workflow clones mutable cc65 HEAD and uploads only *.lnx, not maps or measurement records. Direct CPU chapter retrieval succeeds despite browser failure; it expressly warns that page mode, video/refresh pauses and hardware latency make CPU-loop timing environment dependent.
- Exact-pin Linux job94618621837 logs expose cc65 BUILD_ID Git547d92358, resolved through primary GitHub metadata to547d923588d870aacf0b0016c67d0f6a92a70f83. Release job94618921785 verifies artifact9201182930 digest59ceee79867a15630b1d3d02a73529077a140677f44b91b409e50dba18386781 before packaging1.4.9. Release ZIP digest1d9d9de3e500643f763d6b1c9e1847fdce93d840990230b945983359d2b6f15c is metadata only; no binaries downloaded. Annotated tag1.4.9 dereferences to the exact source pin. This supersedes the earlier release-toolchain-unknown note, not the original measurement provenance gap.
- Retrieved recovered-release-toolchain and project-pin libsrc/lynx/crt0.s and cfg/lynx.cfg as text: both pairs are exactly identical. Logs show timers.sym generated with -Ln but packaging selects only *.lnx. No retained symbol map or Test6 hardware measurement was found in source/release/workflow records; a reproducible release build is not a measured hardware run.
- Canonical Epyx CPU and system-bus chapters are now accessible through the contents links/direct HTTP. CPU chapter distinguishes4-tick page-mode opcode reads,5-tick normal RAM and available-hardware cycles, address-dependent hardware waits, and video/refresh CPU pauses. Bus appendix orders Video > Refresh > CPU > Suzy and describes regularly enabled video/refresh requests pausing CPU. These characterize missing prerequisites; they provide no exact Test6 stall/phase calculation and no justification for constant INTSET padding.
- Additional data precondition: pinned Test6 references t5_count only at its ZEROPAGE declaration, INC and final LDA; no explicit initialization exists. Recovered CRT calls zerobss, which clears only __BSS_RUN__/__BSS_SIZE__; ZEROPAGE is separately allocated without file contents. Wrapper explicitly clears zero page. Original cartridge-loader/constructor/hardware entry state at that byte remains unestablished, so result14 cannot safely be equated with acknowledgment count absent that initial value. This is an alternative hypothesis, not proof of an initial1 or an upstream defect.

- Resumed diagnosis-only. No project skills or configured debugger agent skills found. No knowledge-base file exists; semantic memory tooling unavailable. SBFL skipped: no per-test coverage spectrum for this fixture. Deterministic source mismatch is classified Bohrbug; initial reproduction and differential link geometry are the useful routes (no prior passing suite commit for bisect).
- Pinned Test6 polls Timer5 pending 192 times, uses zero-page INC t5_count, Timer3 backup/count16 and selector2, and has one taken BEQ and one taken BNE in the no-IRQ path. Original Makefile links tests.o before main.o using pinned lynx.cfg, whose STARTUP/LOWCODE/ONCE precede CODE.
- Verified pinned tool/library binary hashes, then manually compiled unchanged pinned main.c and tests.s using original flags and original tests.o/main.o/library order plus lynx.cfg. Link only; no CRT, TGI, cartridge or upstream Makefile execution. Reference CODE0298, Test6=0401, poll0425, noIRQ0433, BNE0434. Wrapper CODE0273, Test6=03DC, poll0400, noIRQ040E, BNE040F. Both branch destinations share the page of the post-operand PC; both t5_count symbols use zero-page01. Thus original versus wrapper Test6 has identical cycle shape.
- Current core/bus.c has all cycles priced5, sample0, and all addresses mapped to the RAM timing region. Mapped hardware has no additional wait-state cost. No-IRQ iteration=14 CPU cycles; IRQ iteration=24, including zero-page INC5. Trace poll gaps70/120 ticks agree with these counts.
- Parsed both traces:192 INTSET reads and12 INTRST acknowledgments;179 poll gaps70 and12 gaps120; first read enable+20 and final read enable+13990. Formula20+191*70+12*50=13990 matches exactly. Expiry13=enable+d+14080 for first-edge delay d1..64. Independent phase enumeration yields12 for every phase; phase-only and5-tick sampling shifts cannot close91+ tick gap.
- Controlled temporary original-placement CODE0298/poll0425 returns0C on both profiles (only result14 mismatch). Deliberately synthetic crossing CODE0363/poll04F0 has BNE04FF→04F0 across page; independent pre-execution model predicts13, and both profiles return0D/all16 source-accepted. The synthetic control proves placement can force agreement but is not a legitimate wrapper fix because the actual original layout does not cross.
- Pinned cc65 CRT writes Timer0 BKUP/CTLA9E/18, Timer2 BKUP/CTLA68/1F, PBKUP29, DISPCTL0D and MAPCTL0C before main. Primary Epyx register appendix confirms video-DMA enable is DISPCTL bit0 and MAPCTL bit7 disables sequential timing. Original timers source never disables video; supporting timers2 explicitly disables it during measurements. Tier-A flat5/no-DMA execution therefore omits original runtime preconditions. Exact impact on13 unproven.
- Pinned README lacks Test6 per-profile measurement/binary/map/toolchain provenance; pinned CI clones mutable cc65 HEAD. No checked-in sym/map/lnx in source tree. Reconstructed geometry uses project-pinned toolchain and cannot establish historical author geometry. No release assets downloaded; no new CRT/TGI execution.
- Primary timer/audio chapters corroborate17-edge cadence and slot-limited timer/audio register access, but do not justify arbitrary INTSET wait states. Pinned timers2 Test9 expects identical aggregate RAM/INTSET/INTRST/SERCTL timing envelopes. Primary CPU/system-bus chapter URLs were unavailable in browser. Narrow report: .planning/debug/evidence/timer-suite-cascade-count.md.

- Timer3 backup/count 16, source2 divider64, reload interval1088; each expiry propagates to linked Timer5 without an observed loss.
- Lynx1 CTLA write279290, first decrement279296, expiry1=280320, expiry12=292288, last poll293280, result store293360, expiry13=293376.
- Lynx2 CTLA write279310, first decrement279360, expiry1=280384, expiry12=292352, last poll293300, result store293380, expiry13=293440.
- Test6 has 192 polls; a taken BNE or conditional BEQ that crosses a page can change total executed cycles after relocation.
- Phase-only explanation is not established: the first edge cannot precede enabling; moving the first edge earlier by at most6/50 ticks still appears insufficient to move expiry13 before the final polls (gaps96/140). Independently check this argument.
- Current uncommitted executor-owned changes: timer wrapper and manifest, build-suites.sh, test_public_suites.c. Do not revert or overwrite them. Task2 remains halted; no Plan09 commits.

## Eliminated

- hypothesis: Original cartridge loop incurs a taken-branch page-cross cycle that the wrapper removes.
  evidence: Manually reconstructed original link has poll0425, BEQ042A→0433, BNE0434→0425; wrapper poll0400, BEQ0405→040E, BNE040F→0400. Both same-page. Zero-page scratch addressing is identical.
  timestamp: 2026-11-11

- hypothesis: A different allowed64-tick source phase alone moves expiry13 into the final poll.
  evidence: Final poll enable+13990; earliest expiry13 enable+14081 (even edge at enable would be14080). Independent all64-phase enumeration gives12. Profiles' actual gaps96/140 agree exactly.
  timestamp: 2026-11-11

- Increasing the exploratory tick cap: source tests already return; cap and oracle remain unchanged.

## Resolution

root_cause: Exact normative cause remains unknown. Current12 is fully explained by14/24-cycle loop pricing under flat5 versus17-edge reload. Original CRT/video/page-mode and initial-counter preconditions differ or are unspecified; exact13 has not been derived for the measured original environment.
fix: none
verification: Prior four controlled headless runs and64-phase accounting retained; no new binaries executed. Bounded continuation checked all8 assembly/6 C path-history patches, exact release/tag/workflow metadata and retained logs, recovered cc65 revision, identical CRT/config text, canonical Epyx CPU/bus chapters and Test6/zerobss initialization contracts. No measurement binds initial counter/layout/bus state to13. Hardware accuracy unverified; strict original oracle remains unresolved.
files_changed: [.planning/debug/timer-suite-cascade-count.md, .planning/debug/evidence/timer-suite-cascade-count.md]; documentation only, executor modifications preserved

## Diagnosis Confidence and Next Decision

- Proven for the current model: flat five-tick CPU cycles without DMA produce twelve observed cascades for all sixty-four allowed source phases. Exact instruction and trace accounting agree on both profiles.
- Proven precondition difference: the reconstructed pinned-toolchain reference has the same Test6 cycle geometry and its CRT enables display DMA before the tests. Release-toolchain revision is now recovered and CRT/config match the project pin; original measurement binary placement remains unavailable. Test6 does not clear its zero-page count; original entry value is unknown, whereas the wrapper clears it.
- Not proven: why the historical source oracle expects thirteen, whether its counter started at zero, or whether independently modeled page-mode, refresh and display contention reproduce that value. This investigation does not establish hardware accuracy or an incorrect oracle.
- Least disruptive immediate disposition: preserve the strict failing comparison, retain the Task2 halt and leave source, oracle and timer policy unchanged. The parent owns any next bounded provenance research; no owner acknowledgment is needed merely to preserve this stop.
- Concrete implementation choice after this exhausted provenance pass: defer exact comparison to scoped Phase5 opcode page-mode, refresh/display bus grants and CRT-equivalent setup with an explicit initial-zero-page contract and independent traces/boundary/snapshot tests. Bringing those prerequisites into Phase4 needs an explicit phase-scope decision; it is not a timer-policy tweak. Either route retains the strict oracle and existing physical/firmware/baseline/publication gates. No new binary download/execution, dependency, pin change or hardware work is authorized here.
- Rejected remedies: padding the wrapper to create a branch crossing, tuning timer phase or cycle prices to the expected result, weakening the oracle, or treating synthetic agreement as hardware evidence. The synthetic link experiment is a falsification control only.
- No new implementation scope was started and no product fix was applied. No commits were made, as explicitly required for this diagnosis pass.
- Bounded diagnosis is complete; product mismatch remains unresolved and session remains unarchived. Parent acts next on scope/planning; preserving the stop requires no owner acknowledgment.
