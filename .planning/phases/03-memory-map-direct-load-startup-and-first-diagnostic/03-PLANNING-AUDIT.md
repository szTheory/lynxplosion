# Phase 3 planning source and dependency audit

Status: all in-scope source items COVERED by executable plans; implementation evidence remains pending.
The context source has no D-NN IDs. C01..C24 below are audit row labels only, not invented owner decisions.
Scope arbitration is 03-CONTEXT.md; research notes are evidence inputs, not independent authority.
No implementation, commit, push or external publication was performed by this planner.

## Dependency graph and ownership

| Plan | Needs | Creates / user capability | Wave |
|---|---|---|---|
| 03-01 | Existing CPU/public API/toolchain | Strict BLL tracer and versioned guest identity | 1 |
| 03-02 | 03-01 | BLL ownership proofs and strict runner startup | 2 |
| 03-03 | 03-02 | Complete caller migration and private flat-vector seam | 3 |
| 03-04 | 03-03 | MAPCTL routes and pure mapped inspection | 4 |
| 03-05 | 03-04 | Permanent map controls and post-write watch API | 5 |
| 03-06 | 03-05 | Watch isolation and equivalent snapshot continuation | 6 |
| 03-07 | 03-06 | Original source-built diagnostic and result oracle | 7 |
| 03-08 | 03-07 | Runner diagnostic agreement and independent public consumer | 8 |
| 03-09 | 03-08 | Atomic consumer artifacts and exact-head qualification | 9 |

Shared machine/header/tests/inventory files require sequential waves; there is no safe same-wave file independence.
Each plan has two tasks, at most five modified files per task, 6–10 actual task-listed modified files per plan, and runnable checks. Frontmatter file lists are the exact union of task file lists.
Calibration query: factor1, sample_count0, confidence low. No invented confidence.
Discovery Level0: no new dependency or external integration; existing pattern map plus settled Phase3 research supplies the novel hardware/container contract.
Graph context is disabled in project config; configured planner skills list is empty.
No package-manager installation is planned; existing cc65 pin/build remains the toolchain.
Public deterministic C ABI is not an external API/service integration; COVERAGE.md records this explicitly.

## Multi-source coverage

| SOURCE | ID | Feature / constraint | Plan | Status |
|---|---|---|---|---|
| GOAL | original diagnostic | Starts tierA on MAPCTL map; RAM pass/fail; labelled viewable snapshot | 01,02,04,05,07,08,09 | COVERED |
| GOAL | success1 | Original pinned source, reset provenance, strict result symbols and broken guest | 01,02,07,08 | COVERED |
| GOAL | success2 | CPU map and separate raw RAM | 04,05 | COVERED |
| GOAL | success3 | startup=A and absent firmware input | 01,02,03,04,05,08,09 | COVERED |
| GOAL | success4 | Completed-write watch and restored/split equality | 05,06,07,08 | COVERED |
| GOAL | success5 | Public consumer P3 SNAPSHOT and valid silence WAV | 08,09 | COVERED |
| REQ | MEM-01 | Four independent MAPCTL windows and pure/raw views | 01,02,04,05,07,08 | COVERED |
| REQ | MEM-02 | Strict tierA direct load, reset provenance | 01,02,03,07,08 | COVERED |
| REQ | MEM-03 | Every run/claim startup strategy | 02,03,07,08,09 | COVERED |
| REQ | MEM-04 | No firmware interface, unavailable mapped ROM | 01,02,03,04,05 | COVERED |
| REQ | EVD-01 | Original source diagnostic and symbol result/control | 07,08 | COVERED |
| REQ | HB-01 | Tick-stamped labelled viewable diagnostic snapshot | 08,09 | COVERED |
| REQ | API-08 | Nonintrusive completed-write watch | 05,06,07,08 | COVERED |
| REQ | DET-03 | Snapshot/slice/watch continuation equivalence | 01,02,03,05,06,07,08 | COVERED |
| REQ | CON-01 | Phase3 public consumer snapshot/silence writer portion | 08,09 | COVERED |
| RESEARCH | decoder | All16/boundaries/FFF8/FFF9/bit7; raw and mapped paths | 04,05 | COVERED |
| RESEARCH | unavailable policy | No fake ROM/device bank/readback/write-through claim | 04,05 | COVERED |
| RESEARCH | BLL | Measured10-byte format, total includes header, strict whole-object hash | 01,02,03,07,08 | COVERED |
| RESEARCH | responsibility | Core validation/guest fixture/host output remain separate | 01,02,04,05,07,08,09 | COVERED |
| RESEARCH | continuation | Post-write request state; host observer excluded; v3 validation | 01,02,05,06 | COVERED |
| RESEARCH | build | Pinned source build, -Ln labels, original source/hex identity and rights | 07,08 | COVERED |
| RESEARCH | output | Strict symbol/parser/result/pixels, P3 palette and temporary PCM WAV | 07,08,09 | COVERED |
| RESEARCH | validation | Exact inventories/claim hash, meaningful named controls, public boundary | 01,02,03,04,05,06,07,08,09 | COVERED |
| RESEARCH | delivery | Full local/Linux/exact-head CI, measured efficiency, no routine2GB rerun | 08,09 | COVERED |
| RESEARCH | conflict pass | Earlier host0C/pre-write/bodyhash/publicraw recommendations rejected by root context | 01,02,03,05,06,07,08 | COVERED |
| CONTEXT | C01 | Autonomy/runtime mappings/explicit authority stops | all | COVERED |
| CONTEXT | C02 | none default; no baseline/suite/accuracy/license/firmware/release/physical claims | all | COVERED |
| CONTEXT | C03 | Instance-owned four-window decoder and fixed special bytes | 04,05 | COVERED |
| CONTEXT | C04 | Raw RAM plus separate pure mapped inspect; whole output preflight | 04,05 | COVERED |
| CONTEXT | C05 | All16/boundaries/bit7, flat5 PLACEHOLDER | 04,05 | COVERED |
| CONTEXT | C06 | Absent ROM explicitly unavailable, no firmware input | 02,03,04,05 | COVERED |
| CONTEXT | C07 | Provisional device/readback/write-through policy visible | 04,05,08,09 | COVERED |
| CONTEXT | C08 | Canonical BS93 header/entry/nonempty/exact/no wrap | 01,02,07,08 | COVERED |
| CONTEXT | C09 | Replace public raw loader; private flatCPU adapter only | 02,03 | COVERED |
| CONTEXT | C10 | Core full object identity, separate payload, format/startup/entry | 01,02,03 | COVERED |
| CONTEXT | C11 | All refusals before mutation, instance input aliases | 01,02,04,05,06 | COVERED |
| CONTEXT | C12 | MAPCTL0 init/reset/load, guest0C, no injected DISPCTL/boot preset | 01,02,07,08 | COVERED |
| CONTEXT | C13 | Original assembly/config/header, pinned rebuild/-Ln, provenance/rights | 07,08 | COVERED |
| CONTEXT | C14 | Nonzero result magic/schema/progress/error/done, guest bounds and controls | 07,08 | COVERED |
| CONTEXT | C15 | Bounded no-callback watch, completed CPU write, exact tick/priority/rearm | 05,06 | COVERED |
| CONTEXT | C16 | Observer not guest; reset/load/restore rules/isolation | 05,06 | COVERED |
| CONTEXT | C17 | Bump/validate snapshot, public fields, split/mid restore equality | 01,02,05,06,07,08 | COVERED |
| CONTEXT | C18 | Independent public-header consumer, raw symbol result/pixels | 08,09 | COVERED |
| CONTEXT | C19 | P3 SNAPSHOT/tick/content/startup/profile/palette | 08,09 | COVERED |
| CONTEXT | C20 | Valid integer silence WAV, temporary binary output, placeholder label | 08,09 | COVERED |
| CONTEXT | C21 | Runner/consumer independent expected values; repeatability scope | 07,08,09 | COVERED |
| CONTEXT | C22 | Exact named tests and same-commit claim inventory hash | all | COVERED |
| CONTEXT | C23 | Targeted/full local/Linux/exact-head CI efficiency | 08,09 | COVERED |
| CONTEXT | C24 | Review/security/fixes bounded within approval; parent hooks only | 08,09 | COVERED |

Excluded by explicit phase scope: timer/IRQ/arbitration, calibrated page timing, DMA/real frames/logical input, actual audio, O9 video freeze, BIOS/HLE/cart boot, licence/release/hardware authority.
Partial CON-01 remains visible: real frame/audio stream portions belong to Phases5/6.

## UI truth lift

All six applicable artifact truths are plain must_haves.truths in 03-09: empty inputs/symbols/result/pixels refuse; completion precedes finalization; actionable unsuccessful errors; exact unscaled P3/metadata and labelled WAV; sample/chunk bounds; bounded complete metadata/errors.
No frontend dimensions create work.

## Probe fallback

03-EDGE-RESOLUTIONS.json preserves all14 original rows: nine classified edges have explicit plan predicates; five unclassified rows remain flagged assumptions with concrete tests/manual review.
No auto-dismissed edge, no silent drop, no backstop-only green.
Bespoke prohibition recall retained two transparency/rights constraints, serialized using canonical projectProhibitions from the installed probe-core library into 03-09 must_haves.prohibitions.
They remain descriptor-less unresolved/flagged-unverified; parent verification must not count them as mechanical green.
Normal input/overflow/ownership candidates were dropped as routine engineering. Generic input/filesystem security is canon and belongs to the existing parent security pass; no duplicate prohibition minted.

## Flagged assumptions requiring evidence review

- MEM-01 unclassified: matrix/public CPU route controls prove software decoding; unsupported read/write policy is provisional and hardware-unverified.
- MEM-04 unclassified: no firmware interface and unavailable ROM controls prove refusal scope, not rights provenance of arbitrary caller bytes.
- EVD-01 unclassified: no-run/broken/timeout/field-corruption and independent constants guard result meaning; no physical accuracy evidence.
- DET-03 unclassified: rehashed invalid-state controls and common-tick snapshot/watch/slice equality guard guest continuation; equality alone cannot prove accuracy.
- HB-01 unclassified: independent exact palette/sample parser plus nonzero pixels guard output meaning; not a hardware video contract.
These rows remain unresolved in the probe classification, with explicit tests planned; they do not reauthorize owner-gated actions.

## Delivery ownership

Planner writes only Phase3 plans/coverage/validation/listing. Parent performs plan review and commit, execution orchestration, code/security review and required fixes.
03-09's final task performs full local/Linux and exact-head CI qualification inside existing phase authority; no publication or release.

## Focused checker revision

All four issue records addressed: the 15-file plan was split; every plan now has at most10 real modified files and two retained tasks. Nine sequential waves preserve all18 original task scopes. Golden changes belong to 03-04 (new MAPCTL observation) and 03-06 (intentional guest-digest inputs only); 03-09 qualifies values locally/Linux/CI without changing them to silence failures. Each relevant plan declares tests/golden/skeleton.digest. Direct executable modes carry exact current/planned inventory IDs in their verify blocks. Startup manifest evidence uses det.manifest_fields and det.manifest_fields.self_test. No implementation or commit performed by the planner.

