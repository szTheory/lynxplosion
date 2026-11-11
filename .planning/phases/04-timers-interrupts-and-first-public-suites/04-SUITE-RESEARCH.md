# Phase 4: Pinned Public Suite Research

**Research date:** 2026-11-10  
**Scope:** Exact pinned lynx-tests source, build/startup route, result evidence, and rights boundary. This is suite-source research; it does not settle timer hardware semantics, implement code, or run the suites.

## Recommendation to the planner

1. Make the Phase 4 acceptance set the pinned `cpu` and `timers` suites. Build temporary BLL images from the selected source files and the already pinned cc65 toolchain. The public upstream Makefiles build `.lnx` cartridge images by default; that is outside this phase’s `startup=A` direct-load evidence.
2. Keep upstream test bodies unchanged. First prove a small project-owned C runner can link the suite’s exported `run_tests` function, read the result symbol from the generated label file, and write a separate completion sentinel after the test function returns. The runner should omit TGI display setup. This wrapper approach is a recommendation, not a build already proved in this research. If it cannot link cleanly, a minimal, licensed edit limited to the upstream display runner is the fallback; record its exact diff and hash, and do not edit `tests.s`.
3. Generate expected bytes from the exact pinned C result tables and count metadata. Fail generation when the expected-table length, test offsets/counts, result-buffer size, or assembly writes drift. Preserve entries that accept multiple values. In particular, the timer suite has 16 meaningful results in an 18-byte buffer.
4. Record raw observations separately for each selected profile, using fresh machine state. Compare the CPU suite’s detected model to the selected profile. Do not describe any profile or suite as “passing” while owner decision O1 is open.
5. Require all three completion signals: the sentinel, a minimum guest-time threshold, and a maximum guest-time bound. Add the halted-before-first-instruction no-run control, a wrong-profile control, and a named timer fault. A zero-filled result block or a halted CPU alone is never completion evidence.
6. Use `timers2` as a source-level timer expectation and as an optional focused software regression if the scheduler implementation needs it. It is not part of Phase 4’s required public-suite acceptance list. If not run, list it as a named out-of-scope item; do not silently omit it. Phase 6’s wider suite inventory is tracked separately.

The custom wrapper, symbol reader, oracle generator, and guest-time bounds are project harness work, not new dependencies. Keep source archives, labels, BLL outputs, raw logs, and temporary build products outside the repository. Do not rerun the 2 GB CPU-vector lane for this phase.

## Project Constraints (from AGENTS.md)

- Keep source observations, measured results, recommendations, inference, and owner decisions distinct. The project instruction says: “Keep owner decisions, measured results, source observations, reports, inference, recommendations and unknowns distinct.” [VERIFIED: AGENTS.md:29-31]
- Record each fixture’s source, build recipe, identity, license, notices, and allowed use. “A hash establishes identity, not rights.” [VERIFIED: AGENTS.md:65-68]
- Use fresh isolated machine state, semantic guest progress, exact inventories, and meaningful negative controls; show missing or skipped evidence. [VERIFIED: AGENTS.md:72-75]
- Do not run hardware tests or publish artifacts under this phase authority. Physical hardware testing and publication are explicit owner stops. [VERIFIED: AGENTS.md:38-41; .planning/AUTONOMY.md:6-12]

## Phase boundary and owner decisions

The Phase 4 roadmap says: “The lynx-tests `timers` and `cpu` suites run headlessly from source-built tier-A images.” [VERIFIED: .planning/ROADMAP.md:183-190.] Its EVD requirements say:

> - [ ] **EVD-02**: A lynx-tests harness reads `g_results` by symbol from the build's label file, requires a completion sentinel and a guest-time minimum and bound, and compares against expectations generated from the pinned source with a count check
> - [ ] **EVD-03**: A no-run negative control (CPU halted before tests start) makes every suite fail in the harness
> - [ ] **EVD-04**: The harness asserts which model a suite detected and fails when it does not match the selected profile
> - [ ] **EVD-05**: Each device phase adds at least one named-fault control that is shown to fail the intended test
> - [ ] **EVD-06**: The lynx-tests `timers`, `cpu`, `page-mode`, `audio` and `audio2` suites run headlessly from pinned, source-built, startup-tier-A images with results captured and inventoried; any suite that cannot run that way is a visible, documented skip with its blocker

[VERIFIED: .planning/REQUIREMENTS.md:95-99.] EVD-06 is assigned to Phase 6, while EVD-02 through EVD-05 are assigned to Phase 4. [VERIFIED: .planning/REQUIREMENTS.md:223-226.]

> - **O1** | Baseline Lynx model/CPU profile for first claims (research split: Lynx I-style vs Lynx II) | Any suite-pass claim (CPU-02, EVD-06 claims)
> - **O5** | Project license | Any public artifact
> - **O6** | Clean-room protocol and recording of research GPL exposure | Finalizing FND-07

[VERIFIED: .planning/REQUIREMENTS.md:163-170.] EVD-06 names `timers`, `cpu`, `page-mode`, `audio` and `audio2`; this is later suite work, separate from Phase 4’s required two suites. [VERIFIED: .planning/REQUIREMENTS.md:99]

## Exact pin, source identity, and rights

The repository pin is the exact archive for commit `b583dc43d532d3c41563a6fa35420614fbcead89`, with version label `1.4.9`. The pin line is:

> lynx-tests 1.4.9 https://github.com/drhelius/lynx-tests/archive/b583dc43d532d3c41563a6fa35420614fbcead89.tar.gz a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e

[VERIFIED: pins.txt:12-13; archive SHA-256 independently matched the pinned value.] The pinned [commit](https://github.com/drhelius/lynx-tests/commit/b583dc43d532d3c41563a6fa35420614fbcead89) is dated 2026-08-13 [CITED: exact pinned commit]. Use that permalink and checksum in any recipe, never mutable `main` or a replacement commit. The archive listing has source, build files, README, and license; it contains no `.lnx`, `.rom`, `.bin`, or `.zip` payload. [VERIFIED: complete listing of exact pinned archive.]

The pinned root [LICENSE](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/LICENSE) identifies MIT and “Copyright (c) 2025 Ignacio Sanchez Gines.” [CITED: pinned LICENSE.] This supports use of the test source under its notice terms; it does not clear a public project artifact. The project’s draft clean-room policy says “Allowed sources. Epyx hardware documents, the lynx-tests suite as test source (MIT)” [VERIFIED: docs/policy/clean-room.md:16-19]. O5 remains an explicit stop for any public artifact. Keep the exact license and notices available with local generated output; do not commit or distribute the upstream source or BLL binaries under this phase.

The upstream [README](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/README.md) says the tests were made by analyzing actual hardware. [CITED: pinned README.] Treat that as the maintainer’s report. The source comments and C oracle are the upstream software expectations; this review did not independently verify hardware runs or the exact Lynx model used. No hardware testing is authorized here.

Repository provenance notes need reconciliation as part of the phase documentation work. The current line says:

> - **lynx-tests** (MIT, test source by the Gearlynx author): pinned in `pins.txt` by tag and SHA-256. Used as test source only; no built binary or source is committed.

[VERIFIED: docs/provenance.md:92-93.] The current pin is a commit, and the current suite maintainer is identified in the exact pinned LICENSE. The exposure record also says it read a scratch clone at commit `a920f01` dated 2026-09-20. [VERIFIED: docs/policy/exposure-record.md:22.] Update both records to identify this exact commit/date and the files actually read; keep the O6 draft banner. No Gearlynx, MAME, or other GPL emulator implementation was opened in this review.

## What the pinned source expects

Source links below are pinned to the exact commit. These expectations are useful for a generated software oracle; they are not primary hardware specifications.

| Suite | Source observations | Planning consequence |
|---|---|---|
| `cpu` | [main.c](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/cpu/main.c) declares `RESULT_COUNT 18`, `TEST_COUNT 8`, and `LYNX_MODEL_TEST 6`; [tests.s](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/cpu/tests.s) exports `_run_tests` and `_g_results`, reserving 18 bytes. The exact model tuples in main.c are `{ 0xFF, 0x00, 0x02, 0x02 }` for Lynx I and `{ 0xFE, 0x01, 0x01, 0x01 }` for Lynx II. | Preserve the 18-byte oracle and the two accepted model-specific tuples. The original C frontend accepts either tuple; the project harness must fail if the detected tuple does not match the selected profile. The pinned [README](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/README.md) says tests 1, 2, and 8 use Timer 6 IRQ behavior, so CPU IRQ sampling is part of this suite’s useful result. |
| `timers` | [main.c](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers/main.c) declares an 18-byte result array, with offsets `{ 0, 3, 5, 8, 12, 14, 15 }` and counts `{ 3, 2, 3, 4, 2, 1, 1 }`, summing to 16 meaningful results. [tests.s](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers/tests.s) reserves 18 bytes but writes indices 0 through 15. The source accepts alternatives `EXPECT(0x04, 0x05)` and `EXPECT(0x35, 0x36, 0x37)` for RESET_DONE observations. | Generate and count-check 16 expected values, not 18. Keep both accepted-value sets. Confirm result bounds from the `.sym` label and buffer before reading. The suite covers Timer 3/5, DONE, RESET_DONE, IRQs, reload, and Timer 3→Timer 5 cascade. |
| `timers2` | [main.c](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers2/main.c) and [tests.s](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers2/tests.s) define ten result bytes and ten debug bytes. A zero result means success in its original frontend; assembly records the last measured value separately in `g_debug_results`. Phase samples use delay counts `{ 0, 7, 15, 31 }`; `ValidatePhase` accepts `$A0-$FE` and explicitly does not require a single fixed phase. | If run, capture both result and debug arrays. Zero bytes by themselves cannot prove the suite ran. The source tests are useful for shared-phase and enable-order expectations, but they do not define a complete scheduler order or exact reset phase. |

For checked local values, the source excerpts are:

> #define RESULT_COUNT 18  
> #define TEST_COUNT 8  
> #define LYNX_MODEL_TEST 6  
> static const uint8_t k_test_offsets[TEST_COUNT] = { 0, 1, 2, 7, 9, 10, 12, 16 };  
> static const uint8_t k_test_counts[TEST_COUNT]  = { 1, 1, 5, 2, 1, 2, 4, 2 };  
> static const uint8_t k_lynx_i_results[4]  = { 0xFF, 0x00, 0x02, 0x02 };  
> static const uint8_t k_lynx_ii_results[4] = { 0xFE, 0x01, 0x01, 0x01 };

[CITED: pinned cpu/main.c, lines 7-10 and 56-60; pinned cpu/tests.s, lines 8-13.]

> #define RESULT_COUNT 18  
> #define TEST_COUNT 7  
> EXPECT(0x04, 0x05)  
> EXPECT(0x35, 0x36, 0x37)  
> static const uint8_t k_test_offsets[TEST_COUNT] = { 0, 3, 5, 8, 12, 14, 15 };  
> static const uint8_t k_test_counts[TEST_COUNT]  = { 3, 2, 3, 4, 2, 1, 1 };

[CITED: pinned timers/main.c, lines 7-30; pinned timers/tests.s, lines 8-13.]

> TEST_COUNT = 10  
> PHASE_SAMPLES = 4  
> ORDER_SAMPLES = 16  
> delay_counts: .byte 0, 7, 15, 31  
> ; Validate a phase sample without requiring one fixed reset phase  
> ; $A0-$FE proves that a 1 us source clock was observed before timeout

[CITED: pinned timers2/tests.s, lines 9-27 and 314-328.]

## Timer phase and ordering: source expectations versus hardware facts

The pinned `timers2` source **does** assert specific software-test expectations:

- Tests 1 and 2 exercise Timer 3 and Timer 6 enable order on the shared 4 µs selector: whichever timer is enabled first may interrupt alone, or both may be pending together; observing only the second-enabled timer is a test failure. [CITED: pinned timers2/tests.s, lines 331-398.]
- Tests 3 and 4 sample the first Timer 6 64 µs edge at several free-running phases and require rewriting Timer 6 CNT to preserve the shared source phase. [CITED: pinned timers2/tests.s, lines 401-467.]
- Test 5 changes the enabled Timer 6 selector from 5 to 6 and checks that the new selector samples the shared divider instead of restarting it. [CITED: pinned timers2/tests.s, lines 470-502.]
- Test 6 samples Timer 4 from the shared 64 µs source. [CITED: pinned timers2/tests.s, lines 505-533.]
- Test 10 checks a Timer 3→Timer 5 linked cascade. [CITED: pinned timers2/tests.s, lines 584-645.]

Those statements are the pinned test author’s expected behavior. The README describes the repository as hardware tests analyzed from actual hardware, but neither the test source nor this review provides a primary Epyx specification citation or a per-model measurement record for each assertion. Do not promote these comments to established hardware facts or claim an independent hardware validation.

The source does **not** settle the broader items the Phase 4 research flag leaves open:

- It accepts a broad free-running phase envelope and states that one fixed phase is not required; it does not specify a unique exact reset prescaler phase or establish that hardware reset phase is nondeterministic.
- It samples several phases but does not define the full sub-slot phase or every timer’s exact prescaler/slot phase after reset.
- It checks observable Timer 3/Timer 6 enable order, not a complete same-tick event ordering contract for the emulator scheduler.
- The `timers` RESET_DONE case expects four or five IRQs, clears RESET_DONE, executes three NOPs, then clears the final interrupt. It does not define an exact CTLA-write edge or same-tick order. [CITED: pinned timers/tests.s, lines 168-245.]
- The link-source selector-7 test configures Timer 5 as the Timer 3-linked timer. A scan of the complete pinned timer suite sources found no test that selects selector 7 on Timer 6. [CITED: pinned timers/tests.s and timers2/tests.s at commit b583dc43d532d3c41563a6fa35420614fbcead89.]
- These timer suites do not establish the slow timer/audio clock phase relationship; that remains with the hardware/spec research owner.

Keep the upstream oracle as a software regression expectation. Cluster A’s primary-document and hardware-claim work owns the unresolved normative questions.

## Startup route and evidence harness

The upstream [CPU Makefile](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/cpu/Makefile), [timer Makefile](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/timers/Makefile), and [shared build rules](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/shared/lynxcc65.mak) default to cartridge `.lnx` output. The upstream [build-all script](https://github.com/drhelius/lynx-tests/blob/b583dc43d532d3c41563a6fa35420614fbcead89/build-all.sh) builds the broad suite tree; avoid it for this bounded phase. The official [cc65 Lynx target documentation](https://cc65.github.io/doc/lynx.html) distinguishes cartridge output from BLL-compatible output.

Use the already pinned cc65 source and the project’s BLL direct-load route. The pinned `lynx-bll.cfg` contains a BLL header and starts main content at `$0400`; the pinned CRT executes before C `main`, initializes stack/register state, clears BSS, and writes Mikey/Suzy registers. The source says:

> HEADER: file = %O, start = $0000, size = $000a;
> MAIN:   file = %O, define = yes, start = $0400, size = $BC38 - __STACKSIZE__;
> lda     #$C
> sta     MAPCTL          ; $FFF9

[VERIFIED: build/pins/cc65/cfg/lynx-bll.cfg:8-12; build/pins/cc65/libsrc/lynx/crt0.s:45-56.] This is guest runtime initialization, not BIOS or cartridge boot. The roadmap’s exact route is “Every v1 run is `startup=A` (direct RAM load) and is never boot, cartridge or loader evidence.” [VERIFIED: .planning/ROADMAP.md:23-26.] Preserve the BLL input hash, loaded body hash, selected profile, cc65 pin, wrapper/source hashes, label-file identity, source license/notice, and oracle-generator identity in the fixture/run record. The suite image is not a bare test-body jump, and it must not be reported as cartridge boot evidence.

The required source oracle must:

- Parse the exact pinned expectations, model tuples, `TEST_COUNT`, offsets, counts, and assembly buffer extents.
- Preserve all alternatives in `EXPECT(...)`.
- Check the sum of per-test counts against the number of explicit expected results and the source buffer’s written range. For `timers`, explicitly report 16 meaningful results and two unused bytes.
- Fail closed if source shape or counts change; do not fall back to an old generated oracle or expected values copied from an emulator.
- Read `_g_results` from the generated label file, check symbol extent and RAM bounds, and capture raw bytes before interpreting them.

For the completion protocol, a separate sentinel should be initialized to a non-done value and written only after `_run_tests` returns. A custom wrapper avoids the original TGI drawing and permanent display loops without changing test assembly. The host should stop on the sentinel after its final guest tick, enforce a source-derived minimum, and enforce a fixed upper bound so the suite’s unbounded timer/IRQ polling loops cannot hang CI. The bound is a planning parameter to derive from a real run; no timing was measured in this research.

The no-run control must implement EVD-03’s actual condition: halt the CPU before the first test instruction, then prove the harness rejects both suites due to missing sentinel/results. The current public API says “FRAME and HALT remain reserved in this build”; therefore the no-run stop should be an internal harness seam or dedicated test-only machine setup, not a new product API just for this control. [VERIFIED: include/lynxplosion.h:42-52.] Also prove a selected-profile mismatch fails EVD-04 and a named DONE-gating fault fails its intended timer expectation while an unmodified positive control is accepted by the source oracle.

## Fast gate, extended evidence, and skips

- **Fast CI:** build only `cpu` and `timers` source inputs into temporary BLLs; run each with fresh machine state for the selected profile(s), and record raw output separately per profile. Since O1 is open, report observations and source-oracle comparison only, never a “suite passes” claim.
- **Extended timer evidence:** `timers2` has useful shared-phase expectations but is not required by the Phase 4 suite acceptance criteria. If the phase chooses to run it, make it an explicitly named manual or optional job, capture both arrays, and record its actual cost. Otherwise inventory it as not run in Phase 4 because it is outside the required suite set, with the source-research result retained here.
- **Later public suite inventory:** `page-mode`, `audio`, and `audio2` belong to EVD-06’s later suite work. Track them as out of phase in Phase 4, not as an accidental omission.
- **Timing evidence:** record queue, setup, build, suite runtime, and total CI wall time on the first real run. No wall-time estimate exists yet because no Phase 4 suite build or run was performed. Do not add caches, concurrency, or a broad matrix until this measurement shows a bottleneck.
- **Cost boundary:** use the Phase 2 CPU vector results already in the repository; do not rerun the 2 GB vector lane for timer-suite work. No new dependency is indicated.

## Owner gates and unresolved questions

| Gate | What can proceed now | What remains blocked |
|---|---|---|
| O1, selected model/CPU profile | Build and record raw results by explicitly selected profile; verify model detection against that selection. | Any claim that names a profile as passing. |
| O5, project license | Local temporary build and non-public evidence under the existing pinned-source working rule. | Public fixture, binary, or other project artifact. |
| O6, clean-room policy | Continue under the draft rule, record the exact files/source exposure, and keep emulator implementation sources closed. | Final policy adoption; this review does not decide it. |
| Physical hardware | No hardware action is needed for these software runs. | Any physical run or qualification. |

Open technical questions for the Phase 4 plan: prove the custom BLL wrapper/link setup from pinned sources; choose and document source-derived guest-time thresholds after measuring a real run; decide whether `timers2` earns a distinct optional check in Phase 4; and preserve the primary-spec and hardware observation questions in the Cluster A workstream.

## Work performed and confidence

The exact pinned archive was downloaded to a temporary directory and its SHA-256 matched `pins.txt`. Pinned suite source, root license, and build files were inspected. No source was extracted into the repository, built, or run; no tests, foreground apps, physical hardware, firmware, game content, or emulator implementation source was used. Wall time for the suites is unmeasured.

- **Pin and license identity: HIGH** — exact project pin and archive digest matched; root MIT license inspected at the same commit.
- **Suite data layouts and assertions: HIGH for what the source says** — exact pinned source and count tables reviewed; this is not a hardware-accuracy confidence rating.
- **BLL runner recommendation: MEDIUM** — it follows the project’s already measured BLL path and the pinned source exports, but the custom wrapper has not yet been built or linked.
- **Hardware interpretation: LOW / unresolved** — no independent hardware run or primary-spec comparison was performed in this suite-source review.
