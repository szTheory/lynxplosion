# Phase 5 Independent Goal and Coverage Audit

**Verdict:** software assertion coverage is sufficient to resume qualification after the other required reviews. This audit found no missing or vacuous software test that needs a source/test fix before Task 2. Final broad qualification is still pending, two page-mode profile runs are explicit skips with no source results, and the timer source comparisons still contain the recorded strict mismatch. This is not phase verification or a hardware-accuracy claim.

## Scope and method

The checkout matched the frozen review head named in the dispatch. I read the phase context, source audit, review ledger, qualification record, roadmap criteria, validation map, phase plans/summaries, registered test names, and the relevant API, consumer, bus/video, input, snapshot, source-suite, and mutation-control assertions. I used the prior focused verification records because the reviewed source is unchanged. I ran only `ctest --preset check -N` to inspect registration; I did not execute tests or change any implementation, test, inventory, shared validation, state, or cache file. This report is the only file created by this audit.

## Phase goal and success criteria

| Criterion | Status | Behavioral evidence and limit |
|---|---|---|
| Programmable Timer 0 line and Timer 2 frame timing; documented frame stop | **COVERED** | `api.video_cadence`, `api.frame_edges`, `api.frame_stops`, `api.scheduler_order`, and `api.scheduler_limits` assert exact line/frame ticks for three programmed presets, half-open exact-target behavior, deferred event handling, partial/blank frames, same-tick ordering, and no duplicate delivery. These establish the named software policy, not calibrated physical cadence. |
| Display DMA owns/prices bus work, closes page streams, named cost fault; page-mode evidence or visible skip; timer revisit | **COVERED with explicit evidence limits** | `api.video_bus`, `api.video_bus_edges`, `api.video_page_mode`, `api.cost_model` and `video.fault.{dma_cost_zero,dma_request_omitted}` assert grants, busy intervals, CPU hold/release, video/refresh priority, page-stream closure and exact semantic mutation failures. The pinned page-mode suite was built and identified, but both profile runs stop at the first unsupported `$06` write to `$FD8C` before the test body writes results. The two registered profile cases are the exact declared skips; skip/inventory controls pass. This satisfies the roadmap's visible-skip alternative, but there are no page-mode source result bytes. The separate timer revisit retains its strict mismatch described below. |
| Raw pens, per-line 16-entry palette state, ticks, and no core presentation transform | **COVERED as software contract** | `api.video_geometry`, `api.palette_boundary`, `consumer.frame_tracer`, `consumer.frame_replay`, and `check_artifacts.c` assert packed pen bytes, all palette entries in selected rows, line timestamps, row count, raw-to-PPM colors, and scan order. Palette changes are tested on either side of the chosen row boundary. The exact physical latch edge remains uncalibrated. |
| Logical input reflected at sampled guest time; no core controller discovery | **COVERED as software contract** | `api.input_bits`, `api.input_orientation`, `api.input_edges`, and consumer replay assertions check all nine logical bits, both handedness maps, JOYSTICK/SWITCHES values, exact grant-time reads, and edge positions at T-1/T/T+1. The input replay code has no host controller discovery path. Disconnected cartridge-line values and physical polarity remain explicitly unqualified. |
| Original direct-load homebrew responds to recording; consumer renders and repeats raw-frame observations | **COVERED as repeatability evidence** | The pinned source-built input probe runs through the public-only consumer. `consumer.frame_replay` checks semantic progress, sampled input, raw pen/palette/tick records and independently rendered PPM pixels for 23 frames across both software handedness settings and Lynx I/II profiles. Fresh and interleaved runs compare raw and rendered digests; a changed event is required to change the expected tail frames. Repeatability does not establish hardware accuracy. |

## Scoped requirements

| Requirement | Status | Assertion coverage and remaining limit |
|---|---|---|
| VID-01 | **COVERED** | `api.video_cadence`, `api.frame_edges`, `api.frame_stops`: programmable line/frame intervals, Timer 0/2 event, exclusive edge, and bounded frame return. No fixed host refresh or hardware calibration claim. |
| VID-02 | **COVERED for the software contract** | `api.video_bus`, `api.video_bus_edges`, `api.video_page_mode`, `api.cost_model`, `video.fault.*`; includes positive and empty-DMA controls, five-tick DMA/refresh ownership, page eligibility and stream close. Original page-mode source observations are the two declared `$FD8C` skips, not successful suite results. O1 remains open for any profile/compatibility claim. |
| VID-03 | **COVERED for the selected row-snapshot policy** | `api.palette_boundary`, `api.video_geometry`, `video.fault.palette_boundary`, and independent consumer artifact checks verify palette bytes and row-edge selection. Mid-line physical palette behavior remains unknown and is documented as policy. |
| VID-04 | **COVERED** | `api.video_geometry`, `api.frame_refusal`, `api.video_snapshot`, `api.video_snapshot_refusal`, `consumer.frame_replay`; checks caller-owned raw buffers, bounded dimensions, palette/tick sidecars, sentinels/canaries, and raw-to-rendered separation. O9 only remains relevant if a future shared LibFuturo sidecar is needed; no shared contract was changed here. |
| VID-05 | **COVERED** | `api.frame_edges`, `api.frame_stops`, `api.video_cadence`; exact event definition, deferred target event, no-frame output preservation, watch/tie ordering, and one-time delivery are asserted and documented. |
| INP-01 | **COVERED for logical input policy** | `api.input_bits`, `api.input_orientation`, `api.input_edges`, five input fault controls, `consumer.frame_replay`; actual guest bus reads and edge timing are checked. Hardware polarity and disconnected physical inputs are not qualified. |
| API-06 | **COVERED** | `api.input_refusal`, `api.input_edges`, `consumer.replay_edges`, `consumer.replay_restore`, `consumer.replay_controls`; invalid masks/timelines refuse, tick-zero starts released unless explicitly set, the setter does not advance guest time, and checkpoint tests validate both pre-edge and post-edge cursor/mask at the same tick. |
| HB-02 | **COVERED** | `consumer.frame_tracer`, `consumer.frame_replay`, `consumer.frame_response_controls`; the source-built guest's result/progress block, recorded A response, sampled registers, raw transition pixels and independently rendered output are checked. No-response and no-progress copies fail their intended semantic assertions. |
| CON-01 | **PARTIAL globally; Phase 5 frame slice covered** | Phase 5 public-only raw-frame consumer, fresh/sliced/watched/restored/interleaved and multi-instance observations are covered by `consumer.*`, `api.video_snapshot`, and determinism tests. Real-audio consumer closure is Phase 6 work. |
| EVD-02 | **PARTIAL globally; Phase 5 revisit performed** | `suite.timers.video_revisit`, observation cases, strict controls, result/pending/model controls check source identity, all 16 rows, completion/model/count/time boundaries, and reject a second mismatch or legacy-descriptor inheritance. Legacy flat5/no-DMA/counter-zero remains row 14 `0c` versus source `0d`; VIDEO_POLICY1 has 15/16 agreement and row 14 `0f` versus source `0d`, with no pending descriptor. Strict comparison remains failing. The page-mode source results remain missing because of the exact skip. |
| EVD-05 | **PARTIAL globally; Phase 5 slice covered** | Three video and five input copied-source mutations must compile, fail exactly one named semantic assertion with exit 1, and be bracketed by fresh clean positives. The recurring audio-fault obligation remains in Phase 6. |

The project requirements and global obligations must retain their current open/partial states. In particular, the test results do not close EVD-02, the audio portions of EVD-05 or CON-01, or owner decisions O1/O5/O6/O9.

## D-01 through D-12

| Decision | Status | Actual proof or disposition |
|---|---|---|
| D-01 — one scheduler, programmable frame cadence, half-open boundaries | **COVERED** | `api.video_cadence`, `api.frame_edges`, `api.frame_stops`, `api.scheduler_order`, `api.scheduler_limits`; exact target and later positive call are distinguished, including watch/frame ties and zero-progress repetition. |
| D-02 — bounded video/refresh grants, priority, full-RAM DMA, page eligibility, named costs | **COVERED as versioned software policy** | `api.video_bus`, `api.video_bus_edges`, `api.video_page_mode`, `api.cost_model`, `video.fault.*`; asserts exact grants/holds and that only the declared opcode subset receives page pricing. The chosen schedule and prices are not physical calibration. |
| D-03 — raw packed pens, palette rows, ticks, geometry and partial frame | **COVERED as versioned software policy** | `api.video_geometry`, `api.palette_boundary`, `api.video_cadence`, consumer artifact checks; checks 80-byte rows, 102-row cap, aligned/wrapping address, raw nibble order, palette ordering and exact selected ticks. |
| D-04 — bounded caller buffers, atomic refusal/publication, format-5 continuation, reset/load | **COVERED** | `api.frame_refusal`, `api.frame_stops`, `api.video_reset_load`, `api.video_snapshot`, `api.video_snapshot_refusal`; whole-instance/output postconditions and next-frame equality after reachable DMA/refresh/palette/input state restores are asserted. |
| D-05 — native-only freeze; no shared LibFuturo change | **COVERED for this phase boundary** | Native public-header and consumer checks plus the 51-path source scope show no LibFuturo source/contract change. O9 remains open for a future shared sidecar need; no approval is inferred. |
| D-06 — canonical replay, exact edge application, snapshot cursor convention | **COVERED** | `consumer.replay_edges`, `consumer.replay_restore`, `consumer.replay_controls`, `api.input_edges`; parser bounds/hostility, tick-zero, exact event tick, and pre-edge cursor=0/post-edge cursor=1 snapshot restore are asserted. |
| D-07 — sampled joystick/switch masks, handedness, explicit partial SPRSYS policy | **COVERED as software mapping** | `api.input_bits`, `api.input_orientation`, `input.fault.*`, `consumer.frame_replay`; real guest reads test both direction maps, aliases, switch/Pause, write/readback and unsupported mapped routes. Hardware electrical behavior is not claimed. |
| D-08 — original source-built guest and public-only semantic consumer | **COVERED** | `consumer.frame_tracer`, `consumer.frame_replay`, `consumer.frame_response_controls`, independent `check_artifacts.c` assertions and fixture manifest identity. Output hashes are labeled repeatability. |
| D-09 — semantic faults, refusals, continuation/isolation and host-input hostility | **COVERED** | Eight semantic source mutants, frame/snapshot/input refusal matrices, timeline parser/path/identity controls, replay restore and fresh/interleaved positives are registered. Mutation harnesses reject compile/runtime/sanitizer failures as false evidence. |
| D-10 — unchanged page-mode suite or precise visible skip | **PARTIAL evidence, with the roadmap's skip branch satisfied** | `C101` builds the unchanged source/oracle and validates identity; `suite.page_mode.controls` checks record/skip integrity. Both profile observations stop before result writes at `$FD8C` and are the only declared skips. Do not convert them into profile evidence or suite agreement. |
| D-11 — strict timer revisit with frozen inputs and no forced agreement | **COVERED as a revisit; agreement remains open** | `suite.timers.video_revisit` and `suite.timers.video_revisit_controls` prove zero counter initialization, frozen policy identities, all 16 source rows, exact mismatch set, and strict failure. Legacy exception does not transfer to VIDEO_POLICY1. |
| D-12 — independent reviews, fixes, final macOS/Linux/exact-head gate and CI efficiency review | **PENDING** | C121 focused Task 1 is recorded 9/9 plus claims/manifests/hygiene. Code/security/coverage verdict disposition and fixes precede Task 2. C122 `sh scripts/check.sh`, self-contained Linux, and exact-head hosted CI have not run. |

## Twenty-four planned task checks

The validation map's C011–C122 registry matches the CTest names inspected with `ctest --preset check -N`. Prior per-task focused runs are reused here; the statuses below are evidence from unchanged-source summaries/qualification, not new test executions. C121 was recorded as nine selected tests passing. C122 remains pending. C102's command passed its controls while its two profile observations were explicit skips.

| Task | Registered proof / command | Audit status |
|---|---|---|
| 05-01-1 | `C011`: `consumer.frame_tracer` | **PASS** — public-only tracer checks two frames and an A response. |
| 05-01-2 | `C012`: tracer, public API, snapshot PPM, silence WAV, self-contained header | **PASS** — five selected tests recorded. |
| 05-02-1 | `C021`: video cadence, frame edges, scheduler order/limits | **PASS** — exact timing and event assertions. |
| 05-02-2 | `C022`: geometry and palette boundary | **PASS** — raw rows, palette and line timestamps. |
| 05-03-1 | `C031`: video bus, bus edges, request hold/wake/boundary | **PASS** — bus ownership and exact release edges. |
| 05-03-2 | `C032`: page mode, bus, cost model and its control | **PASS** — software page pricing/stream-close assertion. |
| 05-04-1 | `C041`: frame and input refusal | **PASS** — rejected calls preserve whole instance/result/output sentinels. |
| 05-04-2 | `C042`: frame stops, reset/load, frame edges, power-on reset, unchanged load refusal | **PASS** — stop/output, reset and refusal postconditions. |
| 05-05-1 | `C051`: input bits, orientation and refusal | **PASS** — nine-bit mapping, both hands, invalid-mask atomicity. |
| 05-05-2 | `C052`: input edges/orientation and video edges | **PASS** — bus grant T-1/T/T+1 sample behavior. |
| 05-06-1 | `C061`: video snapshot/refusal, snapshot refusal, timer snapshot controls | **PASS** — versioned state acceptance/refusal and unchanged target. |
| 05-06-2 | `C062`: deterministic video paths/sensitivity/golden and header | **PASS** — same state repeats, input-sensitive state diverges. |
| 05-07-1 | `C071`: replay edges/restore and tracer | **PASS** — exact host event and checkpoint continuation. |
| 05-07-2 | `C072`: frame replay/tracer and prior snapshot/silence checks | **PASS** — recorded input and semantic frame results. |
| 05-08-1 | `C081`: replay, publication and refusal controls | **PASS** — hostile timeline/path/output refusals and cleanup controls. |
| 05-08-2 | `C082`: frame-response controls and replay | **PASS** — no-progress/no-response/schema mutants plus clean positives. |
| 05-09-1 | `C091`: DMA-cost, omitted-DMA and palette-boundary mutations | **PASS** — each fails its intended assertion, with clean brackets. |
| 05-09-2 | `C092`: A/B, options, Pause, orientation and edge-late mutations | **PASS** — five intended guest-read assertions, with clean brackets. |
| 05-10-1 | `C101`: page-mode source build/oracle, manifests and hygiene | **PASS** — source and oracle identities/control scripts. |
| 05-10-2 | `C102`: two profile observations plus page-mode/oracle/symbol controls | **PASS with two expected skips** — both profile cases stop at unsupported `$FD8C`; raw rows are missing and remain visibly inventoried. |
| 05-11-1 | `C111`: timer video revisit, two profile observations, strict controls and cascade | **PASS as strict observation** — 16 rows retained, row 14 mismatch visible; no pass/agreement claim. |
| 05-11-2 | `C112`: revisit, pending, result and model controls | **PASS** — exact mismatch set and descriptor/identity/shape/tick/continuation hostility. |
| 05-12-1 | `C121`: nine focused integration/control tests plus claim, manifest and tree checks | **PASS** — recorded as 9/9; this is not the final cross-platform gate. |
| 05-12-2 | `C122`: full `sh scripts/check.sh` and macOS/self-contained Linux/exact-head qualification | **PENDING** — must run on the final reviewed/fixed source. |

## Source-audit edge probes

The source audit records 17 surfaced rows: nine explicit predicates and eight items flagged because the classifier did not define a deeper edge taxonomy. The flagged rows are not new hidden requirements; no classifier taxonomy or hardware fact was invented to fill them.

| Probe | Status | Actual evidence |
|---|---|---|
| VID-01 — programmable cadence | **FLAGGED assumption; stated contract covered** | `api.video_cadence`, `api.frame_edges`, `api.frame_stops`. |
| VID-02 — DMA and page behavior | **FLAGGED assumption; stated contract covered** | `api.video_bus`, `api.video_bus_edges`, `api.video_page_mode`, DMA mutants; page suite profile skip remains explicit. |
| VID-03 — palette latch semantics | **FLAGGED assumption; policy covered** | `api.palette_boundary`, palette mutant; physical intra-line timing remains unknown. |
| VID-04 — raw observation shape | **FLAGGED assumption; API contract covered** | `api.video_geometry`, frame refusal, snapshot and consumer checks. |
| VID-05 — frame stop boundary | **FLAGGED assumption; stated contract covered** | `api.frame_edges`, `api.frame_stops`. |
| INP-01 — input mapping/sample behavior | **FLAGGED assumption; stated contract covered** | `api.input_bits`, orientation/edge tests, consumer replay and five input mutants. |
| API-06 — host recording | **FLAGGED assumption; stated contract covered** | replay edge/restore/parser/refusal controls. |
| HB-02 — guest response | **FLAGGED assumption; stated contract covered** | tracer/replay checker and no-response/no-progress mutations. |
| CON-01 — concurrency/isolation | **RESOLVED explicit predicate** | `det.video_paths`, `det.video_sensitivity`, replay fresh/interleaved runs and restored snapshots compare common-tick outputs; calls on one machine are serialized. Audio remains Phase 6. |
| EVD-02 — result/time boundaries | **RESOLVED explicit predicate** | `suite.timers.result_controls`, completion/time candidates and bounded exact suite runner assert row extents and accepted/refused limits. |
| EVD-02 — empty/missing evidence | **RESOLVED explicit predicate** | empty/null/truncated input refusal controls and required nonempty 16-row positive records. |
| EVD-02 — source encoding/equality | **RESOLVED explicit predicate** | strict byte/source-alternative comparison controls; no normalization of result bytes. |
| EVD-02 — integer precision | **RESOLVED explicit predicate** | checked tick/count candidates include exact, adjacent and overflow values in result controls. |
| EVD-02 — continuation | **RESOLVED explicit predicate** | `suite.timers.video_revisit_controls` compares fresh, sliced and watched observations at common ticks. |
| EVD-05 — adjacent DMA/input sample edges | **RESOLVED explicit predicate** | `api.video_bus_edges`, `api.input_edges`, DMA and input semantic mutations. |
| EVD-05 — empty DMA/control | **RESOLVED explicit predicate** | idle `api.video_bus_edges` proves no active request; cost mutations also require active-row failure and clean positives. |
| EVD-05 — grant ordering | **RESOLVED explicit predicate** | `api.video_bus`, `api.video_bus_edges` assert video then refresh before the retained CPU request at tied releases. |

## Negative controls and high-severity mitigation evidence

No reviewed negative control appeared vacuous. The DMA, palette and input mutation harness copies the relevant source into owned scratch, requires a unique anchor and changed hash, builds the mutant, rejects compile errors/timeouts/sanitizers as non-evidence, requires exit 1 and exactly its named assertion, then runs a fresh clean positive after the mutant. `consumer.frame_response_controls` similarly brackets no-progress, no-response and invalid-result-schema guests with positives. Replay/parser, frame refusal, snapshot semantic corruption, stale/missing skip declarations, and timer extra-mismatch/descriptor-inheritance cases have their own behavioral negatives.

| Threat | Evidence inspected | Remaining disposition |
|---|---|---|
| T-05-01 — caller buffer tampering/refusal | `api.frame_refusal`, `api.video_snapshot_refusal`, `consumer.refusal_controls`, `consumer.frame_publication_controls` | Current focused behavior is covered; final-source C122 still required. |
| T-05-02 — unsupported guest/output claims | independent raw artifact checker plus public-only source-built consumer; response and color controls | Repeatability only; no physical accuracy inference. |
| T-05-03 — unbounded/incorrect bus progress | `api.scheduler_limits`, `api.video_bus_edges`, `api.frame_stops`, `api.cost_model` and DMA mutants | Named timing policy only; physical cost remains unverified. |
| T-05-SC — pinned source/toolchain tampering | C101 identity/oracle build, fixture manifests, C121 manifest check, no new dependency/source | Current checks recorded; security review and final gate remain root-owned. |
| T-05-34 — review/qualification source drift | dispatch-bound frozen review scope; review-resolution ledger fingerprint; C121 on that source | **Pending:** refresh identity if fixes change scoped bytes, then qualify exact final source/head. |
| T-05-35 — generated/private evidence leakage | C121 tree hygiene and claims/manifests checks | **Pending:** repeat hygiene on final generated evidence and after any accepted fixes. |
| T-05-36 — green checks overstated as accuracy | claim remains profile `none`, tier R, repeatability; strict mismatch and exact skips remain recorded | **Pending:** preserve same distinctions in final qualification; owner/hardware gates stay open. |

## Findings and next action

1. **No missing or vacuous software test found.** No test or implementation fix is recommended by this coverage audit before Task 2. The code/security reviews may still find independently actionable issues.
2. **Page-mode results are unavailable, not silently green.** Both profiles stop at `$FD8C` before test results; the exact declared skips and skip controls satisfy the roadmap's allowed skip branch. Keep O1 open and do not claim source agreement.
3. **Timer disagreement is tested and remains open.** Legacy index 14 is `0c` versus source `0d`; VIDEO_POLICY1 is `0f` versus `0d`, with the other 15 rows matching. The new environment has no inherited pending descriptor, and strict comparison remains failing. Do not close EVD-02 or tune the implementation toward `0d` without new evidence.
4. **Task 2 is a qualification obligation, not a missing test file.** After root records all review verdicts and applies any accepted fixes, run C122 on the final reviewed source, including the required self-contained Linux and exact-head hosted evidence. Keep the final-source/path hygiene and source identity checks current. No user action or hardware UAT is needed for this software coverage result.

**Coverage counts:** 5/5 roadmap success criteria are covered under their stated software/skip contract; 8/11 phase requirement IDs have their Phase 5 slice covered and 3/11 remain globally partial (CON-01, EVD-02, EVD-05); 0 missing software test gaps found; 23/24 planned task checks have recorded successful invocations (C102 includes two declared page-mode skips), while C122 is pending. The phase remains unqualified and incomplete.
