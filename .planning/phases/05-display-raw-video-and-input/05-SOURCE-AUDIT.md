# Phase5 source coverage and planning preflight

Planning output only, 2026-11-11. No source change, data download, build or product test was performed. All new tests below are planned, not claimed present or green. Independent checker and root commit follow.

## Authority and discovery
Read project AGENTS/AUTONOMY/STATE/PROJECT/REQUIREMENTS/ROADMAP/LESSONS, current context, all three substantive research reports, patterns and validation, prior Phase4 disposition/qualification summary and Phase3 continuation summary. Existing-research path is sufficient: no redundant synthesis or canonical RESEARCH file required. Discovery level0 for established C17/CMake/CTest plus already-completed primary-source phase research; no new external library. Project skill directories have no indexed project skills; planner agent-skills query returned empty. Graphify is disabled, so no graph claims. Calibration factor1/sample_count0/confidence low; every estimate uses those values.

## Canonical MVP goal for root's scoped roadmap grammar repair
As a public API host, I want to receive timer-generated raw frames from an original homebrew responding to a recorded logical input timeline, so that I can render and reproduce its input-responsive frames.

Same approved outcome as the roadmap; no scope decision. Root updates only Phase5's goal grammar, plan count/list and later state bookkeeping.

## Multi-source coverage
| Source | ID | Feature | Plans | Status |
|---|---|---|---|---|
| GOAL | G-01 | Original homebrew produces timer-generated input-responsive raw frames through independent host | 01,02,05,07,08 | COVERED |
| GOAL | SC-01 | Programmable line/frame guest ticks and defined bounded frame stop | 01,02,04 | COVERED |
| GOAL | SC-02 | DMA ownership/cost/page closure, page observations/precise skip, strict timer revisit | 03,09,10,11 | COVERED |
| GOAL | SC-03 | Per-line16-entry12-bit palette/raw data/ticks separated from presentation | 02,04,07,09 | COVERED |
| GOAL | SC-04 | Logical input sampled at guest bus time | 05,07,09 | COVERED |
| GOAL | SC-05 | Original recorded homebrew rendered with repeatability-labelled raw digest | 01,07,08 | COVERED |
| REQ | VID-01 | Exact phase-scoped requirement | 01,02,12 | COVERED |
| REQ | VID-02 | Exact phase-scoped requirement | 01,03,09,10,11,12 | COVERED |
| REQ | VID-03 | Exact phase-scoped requirement | 01,02,09,12 | COVERED |
| REQ | VID-04 | Exact phase-scoped requirement | 01,02,04,06,07,12 | COVERED |
| REQ | VID-05 | Exact phase-scoped requirement | 01,02,04,06,07,12 | COVERED |
| REQ | INP-01 | Exact phase-scoped requirement | 01,05,06,07,09,12 | COVERED |
| REQ | API-06 | Exact phase-scoped requirement | 01,04,05,06,07,08,12 | COVERED |
| REQ | HB-02 | Exact phase-scoped requirement | 01,05,07,08,12 | COVERED |
| REQ | CON-01 | Real-frame consumer slice only; audio closure stays Phase6 | 01,07,08,12 | COVERED |
| REQ | EVD-05 | Phase5 DMA/palette/input faults only; audio closure stays Phase6 | 03,08,09,12 | COVERED |
| REQ | EVD-02 | Timer revisit; strict agreement/global obligation open if unresolved | 10,11,12 | COVERED |
| CONTEXT | D-01 | One timer scheduler, half-open target, programmable cadence, ordered no-replay frame/watch prefix | 01,02,04,05 | COVERED |
| CONTEXT | D-02 | Positive bounded video/refresh grants, priority, full-RAM DMA, priced readiness/page eligibility, labelled unverified costs | 01,03,09,10 | COVERED |
| CONTEXT | D-03 | 160x102 maximum, packed4bpp rows,32 raw palette bytes/row,102 uint64 ticks, explicit partial/blank/flip/address | 01,02,04,07 | COVERED |
| CONTEXT | D-04 | All-span preflight/refusal, completed-only publication, bounded accumulator/discard, complete format5/reset/load | 01,04,06 | COVERED |
| CONTEXT | D-05 | Native-only freeze, no shared LibFuturo mutation, O9/HOST-02 stay open | 01,12 | COVERED |
| CONTEXT | D-06 | Copied nine-bit current mask, bounded canonical host recording, exact tick0/edges, early frames, restored cursor phase | 01,04,05,06,07,08 | COVERED |
| CONTEXT | D-07 | Bus sample tick, all masks, primary Lefthand mappings, explicit cart/unsupported Suzy policy, MAPCTL0C/0F control, no automatic chord action | 01,05,07,09 | COVERED |
| CONTEXT | D-08 | Original source-built BS93, pinned recipe/symbols/provenance, public-only consumer, independent semantic PPM checks, old mode preserved | 01,07,08 | COVERED |
| CONTEXT | D-09 | DMA/palette/input/no-response faults, hostile buffers/snapshot/host/parser/path, continuation/isolation/inventory/rights/CI controls | 02,03,04,05,06,07,08,09,10,11,12 | COVERED |
| CONTEXT | D-10 | Unchanged page source build, complete six-row raw observation or precise runtime skip; legacy identities and strict mismatch preserved | 03,10,11,12 | COVERED |
| CONTEXT | D-11 | Frozen independent startup/counter/bus/video policies; unchanged16-row timer source oracle/layout; new env cannot inherit old exception | 03,09,11,12 | COVERED |
| CONTEXT | D-12 | Independent reviews/fixes before one final gate per macOS/self-contained Linux/exact-head CI, measured critical path, no irrelevant vector rerun | 12 | COVERED |
| RESEARCH | R-DIS-01 | Timer0 lines/Timer2 frame event;50/60/75 source configurations and first-edge policy separated | 01,02 | COVERED |
| RESEARCH | R-DIS-02 | Boundary minus1/exact/plus1, disabled/partial/blank captures, bounded current-tick frame return | 02,04 | COVERED |
| RESEARCH | R-DIS-03 | 80-byte rows/102-row cap, DISPADDR alignment/third-blank transfer and guest flip/address traversal | 01,02 | COVERED |
| RESEARCH | R-DIS-04 | Video>refresh>CPU exclusive ownership, full-RAM DMA, held request vs ready separation | 01,03 | COVERED |
| RESEARCH | R-DIS-05 | Unknown cadence/cost/burst/page-latch values explicitly versioned unverified software policy; no imported constants or suite tuning | 01,03,11 | COVERED |
| RESEARCH | R-DIS-06 | Source-eligible opcode-only4-tick opportunity, full-cycle data/write/crossing, non-CPU stream closure | 03 | COVERED |
| RESEARCH | R-DIS-07 | All16 asymmetric palette entries, GREEN/BLUERED raw order, per-line snapshot boundary/palette mutant; intra-line hardware unknown | 02,07,09 | COVERED |
| RESEARCH | R-RAW-01 | Caller-owned packed packet and102 tick entries (816 bytes), all capacity/metadata spans validated | 01,04 | COVERED |
| RESEARCH | R-RAW-02 | Only completed frame publishes; ordinary advance discards delivery while preserving guest evolution; no queue | 01,02,04 | COVERED |
| RESEARCH | R-RAW-03 | Complete ongoing format5 fieldwise continuation/host-pointer exclusion/old version refusal | 01,03,06 | COVERED |
| RESEARCH | R-RAW-04 | Native API only; later RGB24 adapter possible, actual shared sidecars/O9/HOST-02 deferred | 01,12 | COVERED |
| RESEARCH | R-INP-01 | All nine bits and primary guest Lefthand remap, no automatic chord orientation or invented sprite/math | 01,05 | COVERED |
| RESEARCH | R-INP-02 | Cart bits are not buttons; tier-A disconnected-zero-1 unverified policy, opposite direction levels valid | 05 | COVERED |
| RESEARCH | R-INP-03 | MAPCTL0C retains overlays;0F is negative setup control, orientation deliberately initialized | 01,05,07 | COVERED |
| RESEARCH | R-INP-04 | Host exact edge after exclusive advance, actual bus read sample incl held requests/earlier frames | 05,07 | COVERED |
| RESEARCH | R-INP-05 | Current mask guest-owned, script/cursor host-owned; explicit pre/post-edge restore convention and script digest | 06,07 | COVERED |
| RESEARCH | R-HB-01 | Original ca65/ld65 BS93 guest/config/manifest linked symbols and independently checked progress/input/frame content | 01,07 | COVERED |
| RESEARCH | R-HB-02 | Public-header-only frame/replay consumer, separate raw/presentation digests, PPM channel checks, preserved snapshot/silence | 01,07,08 | COVERED |
| RESEARCH | R-HB-03 | Parser/path/identity/publication/cleanup hostility, stale-good artifacts, no-run/no-response exact diagnostics | 08 | COVERED |
| RESEARCH | R-EVD-01 | Copied-source cost-zero/missing-request/palette-boundary and A/B/options/Pause/orientation/edge faults with successful builds/fresh brackets | 09 | COVERED |
| RESEARCH | R-EVD-02 | Source-derived page-mode wrapper6 results/oracle alternatives/count/model/bounds/source/symbol/completion; real blocker-specific skip | 10 | COVERED |
| RESEARCH | R-EVD-03 | Counter-initial vs acknowledged increments and CRT/setup differences; exact flat5/noDMA known mismatch isolated | 11 | COVERED |
| RESEARCH | R-EVD-04 | New environment identities, complete raw/strict result separation and rejected legacy-exception inheritance | 11 | COVERED |
| RESEARCH | R-QUAL-01 | Existing C17/CTest/sanitizers, no new dependency/framework, exact inventory/no-skip/JUnit, one gate/environment | 12 | COVERED |
| RESEARCH | R-QUAL-02 | Headless/background execution, source-focused evidence reuse, self-contained Linux/history and exact-head CI cost review | 12 | COVERED |

No scoped source item is silently omitted. Exclusions are the context's explicit deferred audio/Suzy sprites/cartridge/firmware/shared LibFuturo/hardware/licence/release items. Page-mode missing runtime prerequisites are an allowed visible skip with an exact blocker, not a planning omission. Strict nonagreement is reported honestly under the existing roadmap; no new environment receives a pending exception.

## Dependency graph and file coupling
- 05-01, wave1: needs existing Phase4 plus pinned tool records; creates original public-only frame/input tracer; checkpoints: none.
- 05-02, wave2: needs 05-01; creates programmable frames, row geometry and palette boundaries; checkpoints: none.
- 05-03, wave3: needs 05-02; creates exclusive dma/refresh grants and eligible page-mode fetches; checkpoints: none.
- 05-04, wave4: needs 05-03; creates atomic caller buffers and complete stop/reset behavior; checkpoints: none.
- 05-05, wave5: needs 05-04; creates all logical inputs, guest handedness and bus sample edges; checkpoints: none.
- 05-06, wave6: needs 05-05; creates format5 hostile restore and identical video continuation; checkpoints: none.
- 05-07, wave7: needs 05-06; creates bounded canonical replay and all input-responsive frames; checkpoints: none.
- 05-08, wave8: needs 05-07; creates consumer input/output hostility and no-response controls; checkpoints: none.
- 05-09, wave9: needs 05-08; creates independent dma, palette and input semantic mutants; checkpoints: none.
- 05-10, wave10: needs 05-09; creates unchanged page-mode source build and honest observation or blocker; checkpoints: none.
- 05-11, wave11: needs 05-10; creates independent timer preconditions and strict offset14 revisit; checkpoints: none.
- 05-12, wave12: needs 05-11; creates reviewed exact-source qualification and honest phase closeout; checkpoints: root-owned internal review/fix resume before final qualification.

All12 waves are sequential because the exact test inventory is checked during configure, and inventory/claim integration sidecars overlap. Avoiding that serial dependency would require an unnecessary inventory architecture change. Tasks authored/registered in the same task form its test/scaffold step; predecessor production paths exist before expansion. Tests must be written before behavior for tdd tasks, but missing test/build failure is never claimed as semantic RED. New sources/assertions are compiled before every owning command. Root authorized the one thin integrated tracer's atomic file-count exception. Other narrow slices can exceed five physical files only through co-located test/registration/inventory/claim/provenance/snapshot-serialization sidecars, whose omission would leave a broken integration; they are not extra subsystems. Estimates remain below the one-agent context target.

## Native contract and named policy guardrails
- Native-only packet:160 pixels,80 packed pen bytes/row, at most102 rows; pen8160, palette3264, ticks102 uint64=816 bytes. Never allocate816 tick entries. Raw palette order GREEN0..15 then BLUERED0..15.
- Standard frame ordinals: timer2 frame-end/co-occurring timer0 starts blank0, subsequent timer0 blank1, blank2/third-blank DISPADDR transfer, then visible0 at ordinal3 through visible101 at104; next frame end105. Source presets retain programmed divisors, no host fixed Hz.
- Numeric scheduling discretion is frozen before source comparisons: video-policy1 one RAM-byte/five-tick grant; refresh five ticks at256 guest phases with one coalesced pending request; page-stream-close-1 closes on any non-CPU grant. All unsourced schedule/latch choices are unverified software policy and never imported/tuned to suite bytes.
- Config default and exact existing lane remain legacy flat5/noDMA. Chosen timing policy is immutable identity/config and reset/load preserve that choice while clearing runtime state. New environment observations cannot use the existing legacy pending descriptor.
- Frame tie precedes same-tick CPU grant/watch; watched write atG returnsG+1 with endpoint work deferred. Budget0 changes no guest state; due current-tick FRAME may consume0 exactly once per completed sequence. Ordinary advancement discards delivery, never guest work.
- Timeline schema: exact ASCII header `LYNX-INPUT 1\n`, then decimal tick, one space, decimal mask, LF; no signs/leading zeros except0/extra whitespace or bytes. Maximum4096 records/128KiB, ticks strictly increasing and within guest headroom, mask0..511. Header-only means all released; tick0 applies after load. Validate whole file before any output creation. Hash canonical full bytes using static host-local project-owned SHA256 and independent known vectors.
- Host replay checkpoints explicitly mark pre-edgeT/cursor first>=T or post-edgeT/cursor first>T and bind full script digest/current mask. Core snapshots contain current mask, not script/cursor/pointers.
- Lefthand1: directions80/40/20/10; Lefthand0 reverses each pair. MAPCTL0C device fixtures,0F RAM negative. Cart-line-zero policy explicitly unverified. Other SPRSYS engine/math control requests remain unavailable; no auto Option2+Pause orientation.
- Public state digest introduced by06 provides canonical guest state through public API; runner never parses private format offsets. Consumer uses no private headers/symbols. Snapshot5 remains complete during each state-producing slice, with06 adding the exhaustive hostile-state proofs.

## Specless edge fallback: all17 surfaced items accounted for
| Requirement | Category | Disposition | Plans | Predicate / flagged scope |
|---|---|---|---|---|
| VID-01 | unclassified | flagged assumption | 01,02 | Programmable timing tests cover stated contract; classifier supplies no additional edge taxonomy. |
| VID-02 | unclassified | flagged assumption | 01,03,09,10 | Bus/page/fault/source checks cover stated contract; hardware schedule remains policy. |
| VID-03 | unclassified | flagged assumption | 02,09 | Palette semantics covered; physical intra-line latch remains unknown. |
| VID-04 | unclassified | flagged assumption | 01,02,04,06,07 | Raw/native span/continuation contract covered; no shared sidecar freeze. |
| VID-05 | unclassified | flagged assumption | 02,04 | Frame/no-frame/exact-edge stops covered. |
| INP-01 | unclassified | flagged assumption | 05,07,09 | Bit/orientation/sample/replay contract covered; physical edge/polarity gaps labelled. |
| API-06 | unclassified | flagged assumption | 04,05,07,08 | Setter/timeline/host hostility contract covered. |
| HB-02 | unclassified | flagged assumption | 01,07,08 | Original semantic response/rendering contract covered. |
| CON-01 | concurrency | resolved explicit | 06,07 | Common-tick fresh/sliced/watched/restored/interleaved frames agree; serialize calls on each machine. |
| EVD-02 | boundary | resolved explicit | 11 | Result extents0/1/15/16/17 and minimum/maximum ticks minus1/exact/plus1 are accepted/refused precisely. |
| EVD-02 | empty | resolved explicit | 11 | Empty/null/truncated source/result refuses before observation; genuine nonempty16-row positive required. |
| EVD-02 | encoding | resolved explicit | 11 | Exact canonical bytes and complete source alternatives define equality, no normalization. |
| EVD-02 | precision | resolved explicit | 11 | Checked integer ticks/counts never overflow or round; extreme boundary records refuse. |
| EVD-02 | concurrency | resolved explicit | 11 | Common-tick uninterrupted/sliced/watched new-environment records agree, per-instance calls serialized. |
| EVD-05 | adjacency | resolved explicit | 03,09 | T-1/T/T+1 DMA/sample/release cases and exclusive target distinguish equal versus adjacent work. |
| EVD-05 | empty | resolved explicit | 09 | No-active-DMA case does not pretend to prove cost, empty/unknown mutant selection refuses, clean active positive required. |
| EVD-05 | ordering | resolved explicit | 03,09 | Tie order is video>refresh>CPU under one bounded scheduler and intended assertions check it. |

Counts:17 surfaced=9 resolved explicit predicates carried in plan truths+8 flagged unclassified assumptions. No item auto-dismissed. No backstop invented for an unclassified item. The classifier assumptions remain visible in01 and here; they do not imply unknown source behavior was calibrated.

## Prohibition recall
Adversarial candidates covered fixed-Hz drift, fabricated LCD accuracy, hidden DMA costs, core rotation, controller discovery, forced timer agreement, silent skips, private API consumer use, firmware exposure and unauthorized shared/release actions. Routine correctness items moved to edge/tests; path traversal/overflow safety are canon security and mapped to threat review, not separately minted prohibitions. Two bespoke claim/authority guards survive; projected through the installed `projectProhibitions` serializer into12's must_haves.prohibitions as descriptor-less unresolved/flagged-unverified entries. No check_kind/target/rule or fixtures are fabricated. Later claim review may verify the negative claim guard while keeping calibration/baseline/rights decisions open; no redundant owner confirmation is needed just to preserve a stop.

## API / UI / schema applicability
No external API integration: this phase extends Lynxplosion's native C API and standalone local consumer, not an external SDK/service. No database/schema push applies. UI design generation/review is not applicable: native/headless, hasFrontendEvidence=false. A detector word match cannot authorize shared LibFuturo edits or a frontend.

## New paths and tracked-path preflight
These paths are deliberately new production/test/doc artifacts; all other implementation destinations refer to existing tracked files:
- `core/video.c`
- `core/input.c`
- `fixtures/diagnostic/input_probe.s`
- `fixtures/diagnostic/input_probe.cfg`
- `fixtures/manifests/input-probe.manifest`
- `tests/consumer/frame_driver.cmake`
- `docs/video-input.md`
- `docs/bus-video-policy.md`
- `docs/input-replay.md`
- `docs/snapshot-video.md`
- `tests/video/fault_controls.cmake`
- `docs/video-faults.md`
- `fixtures/suites/page-mode-wrapper.s`
- `scripts/build-page-mode.sh`
- `scripts/generate-page-mode-oracle.sh`
- `fixtures/manifests/page-mode-tests.manifest`
- `docs/timer-video-revisit.md`
- `.planning/phases/05-display-raw-video-and-input/05-QUALIFICATION.md`
- `.planning/phases/05-display-raw-video-and-input/05-REVIEW-RESOLUTION.md`

All referenced pinned build/pins sources are read-only research/build inputs under existing verified pins, not implementation destinations. PLAN/SUMMARY/qualification artifacts follow phase naming; parent owns commit/ROADMAP/STATE. Runtime binaries/images/logs remain ignored, temporary and invocation-owned.

The old pending descriptor hashes both existing build-suites.sh and generate-suite-oracle.sh. Plan10 therefore creates independent page-only scripts using small copied project-owned patterns and their own provenance/controls; it changes neither old recipe bytes nor the pending descriptor. This avoids invalidating exact legacy evidence while adding the new lane.

## Planning verification
All12 PLAN files passed installed frontmatter.validate and verify.plan-structure with2 tasks each and no warnings/errors before final semantic/path refinements; final rerun is recorded in the planner handoff. Every automated command has an immediate fails_when. Commands use known CMake presets/targets, exact CTest names with --no-tests=error, inherited manifest/hygiene/canonical check scripts, or the source builder extended in its task. Header gate is the real api.header_selfcontained. No product check is claimed performed at plan time.
