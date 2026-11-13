# v1 Cross-Phase Integration Check

## Result

The six expected product flows are wired end to end. I found **0 broken cross-phase connections** and **0 orphaned exports among the audited shared C APIs**. Two software qualification limits remain visible: global EVD-02's strict timer comparison is incomplete, and both page-mode guest observations are declared skips because the guest stops at an unavailable SERCTL write. EVD-02 is a requirement warning, not an absent code connection; the page-mode skips satisfy EVD-06's documented-skip clause.

This is a native C library and headless CLI project. It has no HTTP API routes, login surface, or user-authentication layer to audit.

## Evidence Basis

- Current checkout is `f32b113ff2895990e7c2545cc638bbd74a4088f9`. The tracked diff from the qualified source is documentation and planning metadata only; executable source, tests, and build configuration match qualified source `bc943eecc7a498b4405a150961c036e6d911e60e`.
- All six canonical phase verification reports are present and passed at their recorded commits. Phase 6's current report is 45/45. Phase artifacts inspected for provided/consumed contracts include `03-08-SUMMARY`, `04-12-SUMMARY`, `05-08-SUMMARY`, and Phase 6 summaries `06-01`, `06-07`, `06-08`, `06-09`, `06-11`, and `06-12`.
- The prepared requirement inventory contains all 54 v1 IDs with no orphaned definitions; 53 are satisfied and EVD-02 remains pending.
- Existing canonical evidence is bound to the unchanged qualified source: native and GNU `scripts/check.sh` runs each accounted for 246 rows, with 244 passed, the two declared skips, and zero failures. The hosted source CI run 43448772929 succeeded. No gate was rerun for this audit.
- Raw evidence: `/private/tmp/lynx-06-canonical-native-u8elrdjn/{identity.json,gate.log,LastTest.log,summary.json}` and `/private/tmp/lynx-06-final-review-czwgzg_5/gnu-final/{gate.log,LastTest.log,summary.json}`; hosted job output is `/private/tmp/lynx-06-final-review-czwgzg_5/source-ci-job.log`.
- GSD's SDK milestone projection is stale (`milestone-op` reports a null version and one completed phase); this audit uses project v1's ROADMAP, requirement inventory, six phase reports, and current canonical evidence rather than treating the projection as missing implementation.

## Wiring Summary

| Measure | Result |
|---|---:|
| Expected end-to-end flows wired | 6 / 6 |
| Broken connections / BLOCKER findings | 0 |
| Requirement-level warnings | 1: EVD-02 strict comparison |
| Visible qualification limit | 2 page-mode skips, allowed and documented under EVD-06 |
| Orphaned audited shared exports | 0 |

## End-to-End Flows

| Flow | Trace through current code | Runtime evidence | Status |
|---|---|---|---|
| 1. Pinned/source-built content to guest execution, semantic result, and watch | `scripts/check.sh` builds the authored diagnostic with the pinned cc65 tools (`scripts/check.sh:347-388`) and the suite builder verifies pin/tool/source hashes (`scripts/build-suites.sh:82-117`). `lynx_load_bll` validates the complete BS93 object, hashes it, resets the instance, copies the payload, starts the CPU, and stores content/startup identity (`core/machine.c:270-322`). `runner/bundle.c` reads that identity for manifests (`:209-239`). The scheduler selects timer/device/CPU work on guest ticks (`core/machine.c:581-724`), timestamps bus access, completes CPU cycles, and commits a matching write before returning a watch event (`:752-839`). The diagnostic consumer reads the symbol-bounded RAM result and compares sliced, watched, and restored runs (`tests/diag/test_diag.c:603-617,653-699,1460-1505`). | Current canonical suite includes `diag.result_controls`, CPU profiles, timer observations, no-run controls, and watch/snapshot coverage; native/GNU logs both show CPU and timer observations passing. | **WIRED** |
| 2. Canonical logical input to guest response and separate raw A/V/progress streams | The consumer parses and hashes the canonical timeline, validates BLL and linked symbols, advances only to each event tick, and applies `lynx_set_input` at that exact time (`consumer/main.c:2028-2059,2073-2141,2160-2162,3284-3319,3435-3449`). The public setter updates instance-owned logical input (`core/machine.c:892-897`); `core/input.c:11-17,25-58` maps it when the CPU reads JOYSTICK/SWITCHES. `lynx_advance_av` returns raw pens, palette, line ticks, PCM and channel events (`core/machine.c:970-1027`); timer underflows drive frame/line boundaries and palette snapshots (`core/video.c:196-260`). The consumer validates guest progress/response, encodes progress and A/V streams, and binds them in the manifest (`consumer/main.c:3320-3350,3528-3604`). | `consumer.av_replay`, `consumer.av_response_controls`, `consumer.av_edges`, and current native/GNU logs passed. The independent checker accepts the complete replay output. | **WIRED** |
| 3. Format-7 continuation and in-memory host checkpoint | Restore checks format/build/content/policy identity and payload hash, validates CPU, timer, video, and audio candidates before the first mutation, then applies the complete state in a no-fail block (`core/snapshot.c:681-857`). The A/V sidecar is an in-memory host structure: it binds guest snapshot state to build/content/input identities, tick, cursor, phase, output prefix digest, counts, and frame/sample positions before restoring (`consumer/main.c:3079-3127,3418-3432`). Invalid checkpoints preserve guest time and published-prefix counts (`consumer/main.c:3421-3430`). | `audio.snapshot`, `audio.snapshot_refusal`, `det.audio_paths`, `consumer.av_restore`, and the complete replay/checkpoint path passed in native and GNU canonical runs. No host checkpoint file is required by the contract. | **WIRED** |
| 4. Public-only consumer to independent decoder and exclusive bundle publication | `consumer/main.c` includes the public API header only (`consumer/main.c:29`); `lynx-consume` links `lynx_core` (`CMakeLists.txt:89-94`). It writes 13 distinct output streams (`consumer/main.c:3593-3604`) into an owned sibling staging directory using exclusive file creation, verifies stage identity, and publishes by exclusive directory rename (`:2487-2573`). The separately linked `lynx-check-artifacts` parses produced files in `tests/consumer/av_driver.cmake:656-670`. Race, write/close faults, abort, foreign-stage, and replaced-inode controls exercise ownership and refusal (`tests/consumer/av_driver.cmake:1174-1359`). | `consumer.av_replay`, `consumer.av_restore`, and `consumer.av_publication_controls` passed on both platforms. The race case produced one checked winner and one refusal; injected publication faults left no partial destination or owned stage. | **WIRED** |
| 5. Pinned CPU/timer/page-mode/audio/audio2 sources to oracle, symbol reads, completion, inventory, and negative controls | `tests/suites/run.cmake:2167-2191` invokes the pinned suite builder and source-oracle generator, validates the 18 CPU and 16 meaningful timer expectations, then runs `test_public_suites`. The builder checks pinned archive markers, source hashes, and tool hashes (`scripts/build-suites.sh:82-117`); `scripts/generate-suite-oracle.sh:185-205,240-255` extracts source `g_results` extents and expectations with exact counts. The harness validates linked symbols, build/source identities, guest progress, model probe, sentinel, time bounds, and result bytes (`tests/suites/test_public_suites.c`; phase 4/6 verification key-link tables). CMake registers exact per-profile observations and negative controls (`tests/suites/tests.cmake:39-69`); `scripts/check.sh` and JUnit compare against the exact inventory. | CPU, timer, audio, and audio2 per-profile observations passed in both canonical runs; current audio/audio2 runs compare all 18 rows. The two page-mode observations are the only skips and remain separately inventoried. | **WIRED**, with the explicit page-mode limit below |
| 6. Fixed workload to separated host timing, complete stream identity, and core purity | The benchmark runs the same source-built guest, profile, fixed input, and build identity across five fresh public A/V replays. Host timing wraps only `lynx_advance_av`; frame encode, stream assembly, publication, and whole replay have separate intervals (`consumer/main.c:2376-2395,2436-2483,2640-2680,3532-3632`). The test compares all 13 output stream digests and the combined identity; `docs/performance.md:54-75` records the final-source capture. `core_includes`, `core.no_float`, `core.nm_undefined`, and `core.nm_globals` enforce that clock/filesystem/floating-point/host facilities stay outside machine code (`tests/cmake/core_includes.cmake`, `tests/cmake/no_float.cmake`, inventory rows 119-126). CI wall time remains separate from the benchmark. | Native `summary.json` records five fresh runs, 1,248,256 guest ticks, all 13 equal streams, and output SHA `4f8394b4587431b22180ca01d05a8631ada163fbebf77a18e5a95d4ac20759b4`; API median 280,126,000 ns, frame encode 139,186,000 ns, assembly 271,974,000 ns, publication 5,584,000 ns, full consumer 4,320,873,000 ns. Current native/GNU core-purity checks and benchmark controls passed. | **WIRED** |

## Warnings and Qualification Limits

### EVD-02: Strict timer source comparison remains open

This is a connected observation path with an unresolved value, not a missing harness link. The pinned source, builder, symbol-based `g_results` reader, completion/progress/time checks, source oracle, and comparison are all wired. The current logs show:

- Legacy flat-5 policy: timer row 14 is `0x0C` against source `0x0D` on both profiles. The owner-approved descriptor covers this one legacy observation/control row only.
- VIDEO_POLICY1: timer row 14 is `0x0F` against source `0x0D` on both profiles, with no pending descriptor. Strict comparison remains an actual failure.
- The remaining 15 of 16 meaningful timer rows agree. The controls ensure strict mismatch remains visible rather than being recast as a pass.

Evidence is in native `LastTest.log` rows around 675-682 and 1488-1597, GNU `LastTest.log` rows around 465-472 and 1212-1321, `docs/timer-source-index14-pending.json`, and `docs/public-suites.md`. Phase 4 closed only the approved observation/control slice; Phase 5 revisited but did not close global EVD-02. **Classification: WARNING; requirement status PARTIAL.** Do not claim full source agreement, a profile pass, or hardware accuracy from these observations.

### Page-mode observations: documented missing observation

`suite.page_mode.observation.lynx1` and `.lynx2` stop at the pinned guest's unsupported `$FD8C` SERCTL write before writing result rows. Both are in `tests/inventory/skips.txt`; `tests/suites/tests.cmake:56-59` requires the exact skip diagnostic and rejects undeclared/stale skips. The canonical gate records exactly these two skips and no failures. This honors EVD-06's visible-skip contract; it does not produce page-mode results or qualify page-mode behavior. **Classification: WARNING; EVD-06 remains WIRED with the declared blocker visible.**

### Historical Phase 2 integration advisories

These do not expose a current cross-phase disconnection; no speculative repair was made during this audit.

- The Phase 2 bus timestamp deferral is consumed now: CPU bus operations pass `grant + price.sample` into `bus_access` (`core/machine.c:789-801`), and the timer/video/audio scheduler consumes the common guest tick (`:581-724`).
- IN-02's `lynx__cpu_start_reset` still resets its edge flags (`core/cpu.c:809-816`), but public `lynx_reset` rebuilds the whole machine through `machine_setup` (`core/machine.c:192-201`); no current requirement or E2E path depends on preserving that test-seam state. Hardware NMI/reset behavior remains unqualified.
- IN-03's sample parser still accepts the final vector object without a trailing comma (`tests/cpu/sst_reader.c:640-655`); the checked-in sample path is hash-bound to the upstream pin. This is parser leniency, not a broken consumption path.
- IN-07's transfer size cap can depend on server headers, but accepted vector files are subsequently checked against the exact pinned SHA-256 record (`scripts/vectors-full.sh:145-175`). This remains resource-hardening debt, not evidence substitution.
- IN-08's explicit `pin` mode copies a newly generated record over `upstream.sha256` (`scripts/vectors-full.sh:311-335`); any resulting evidence changes still flow through the explicit pin operation and pinned-source checks.
- IN-11 remains a cleanup-hardening caveat: `check.sh` removes its temporary directories on ordinary paths but does not register both `pristine_tmp` and `fin_tmp` in the signal/exit trap (`scripts/check.sh:154-170,271-287`). It does not alter machine results or current gate status.
- I did not find the handoff's approximate 170-character vector-control fragility as a separate item in `02-REVIEW.md` or `02-REVIEW-FIX.md`; its status is unknown and it is not evidenced as a broken cross-phase path.

## Requirements Integration Map

Status is per requirement. Grouped rows share the same concrete path; “WIRED, local/self-contained” means the requirement's own policy or evidence contract is complete but is not a runtime API provided to a later phase.

| Requirement(s) | Integration path | Status | Issue |
|---|---|---|---|
| FND-01, FND-02, FND-03, FND-04, FND-05, FND-06, FND-09 | Pinned sources/manifests and core-boundary checks feed the exact local/CI command and all generated guest/test consumers. | WIRED | — |
| FND-07, FND-08 | Exposure record / draft clean-room policy and claim template are retained policy artifacts; no runtime consumer is required. | WIRED, local/self-contained | O6 and final claim policy remain owner decisions. |
| API-01, API-02, API-03, API-04, API-05, API-07 | Public instance/content/time/reset/inspect/snapshot APIs are used by diagnostic, suite, runner, and/or A/V consumers. | WIRED | — |
| API-06 | Canonical timeline → `lynx_set_input` at exact tick → CPU bus sample → guest response and applied-input stream. | WIRED | — |
| API-08 | Public write-watch → committed diagnostic RAM store → watch return tick and symbol read; snapshot continuation tests compare against uninterrupted execution. | WIRED | — |
| DET-01, DET-02, DET-04 | Per-instance identity and state observations → runner bundle and replay manifests; same-build runs and instances compare full digests. | WIRED | — |
| DET-03 | Snapshot restore → continued guest → equal final observation versus uninterrupted diagnostic run. | WIRED | — |
| DET-05 | A/V API → separate input, progress, raw video, PCM and event streams on guest ticks; benchmark binds all outputs. | WIRED | Repeatability only; no hardware-accuracy claim. |
| CPU-01, CPU-02, CPU-03, CPU-04, CPU-05 | Profile tables and cycle-stepped CPU → machine bus/scheduler → per-profile vectors and public-suite runs with wrong-profile controls. | WIRED | Lynx baseline choice O1 remains open; profiles are not hardware-qualified. |
| MEM-01, MEM-02, MEM-03, MEM-04 | MAPCTL/BLL tier-A public load and identity → source-built diagnostic, pinned suites, and A/V consumer; unsupported firmware path stays absent. | WIRED | No boot, cartridge, or firmware claim. |
| TIM-01, TIM-02, TIM-03, TIM-04 | Master-tick scheduler/timer bank → CPU IRQ, timer-linked video/audio, snapshot continuation, and pinned suite observations/controls. | WIRED | Timer policy is software evidence; see EVD-02. |
| VID-01, VID-02, VID-03, VID-04, VID-05 | Timer-driven video/DMA/palette → public frame/A/V buffers → independent raw/PPM consumer and response checker. | WIRED | Page-mode source observations remain the two declared skips. |
| INP-01 | Logical mask → mapped JOYSTICK/SWITCHES sampled by guest at the granted bus tick → changed progress/frame/audio. | WIRED | — |
| AUD-01, AUD-02 | Timer-linked channel state → bounded integer PCM/event spans → WAV/event/progress bundle and independent checker. | WIRED | Digital software observation only. |
| EVD-01 | Source-built project diagnostic → linked symbols → public RAM result/progress/watch reader and independent snapshot consumer. | WIRED | — |
| EVD-02 | Pinned builder/oracle → linked-symbol guest results → strict per-row comparator. | PARTIAL | Strict row 14 differs under both legacy and video policies; exact disposition above. |
| EVD-03, EVD-04, EVD-05 | No-run/model/fault controls run through suite and device-phase test harnesses; these are test obligations, not production APIs. | WIRED, evidence-only | Their outcomes pass; they have no downstream runtime consumer. |
| EVD-06 | Source/tool pin → builder/oracle → exact five-suite inventory with visible blocker records and exact CTest skips. | WIRED | Page-mode is explicitly unobserved at `$FD8C`; see warning above. |
| HB-01 | Diagnostic RAM pixels → public-only snapshot consumer → independently parsed PPM artifact. | WIRED | Snapshot label only, not a rendered hardware frame. |
| HB-02 | Original input-probe guest → canonical timeline → raw frame and PPM response checker. | WIRED | — |
| HB-03 | Original A/V guest → logical A input → changed guest marker and non-silent PCM/events → independently checked bundle. | WIRED | — |
| CON-01 | Public C API consumer → raw/encoded frames and WAV/event files → independent bundle decoder. | WIRED | Host checkpoint remains intentionally in memory. |
| PERF-01 | Same build/guest/input workload → five equal 13-stream output bundles; API, encode, assembly, publication, total-consumer, and CI measurements stay separate. | WIRED | Named-build software throughput only. |

**Requirements with no cross-phase product wiring by design:** FND-07 and FND-08 are policy/claim artifacts; EVD-03, EVD-04, and EVD-05 are negative/model/fault evidence; PERF-01 is a recorded measurement. They are self-contained obligations, not orphaned product exports. All other v1 requirements appear in the map and have a concrete consumer or gate path.

## Next Action

No source wiring fix is indicated by this audit. Keep EVD-02 visibly open and page-mode skips visible. Before making any strict suite-agreement claim, the next useful work is a bounded evidence investigation of row 14 under the approved timer policies; closing it requires source-supported evidence, not changing the expected value to match the emulator. No owner action is needed to preserve the current stop, and no firmware, physical test, release, or additional publication authority is implied.
