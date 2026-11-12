# Phase 5 Independent Review Ledger

**Gate:** `PARENT_REVIEW_REQUIRED`  
**State:** preparation complete; independent verdicts and accepted fixes are
pending. Plan 05-12 Task 2 has not started. Root owns review, findings,
dispositions, fixes, bounded retests, and Task 2 resumption.

## Exact source scope for independent review

The review scope is the complete sorted non-`.planning` path set from
`git diff 1d9c9356f1a880dd949b4560fcee918d4073f14 --`, with the tracked
working-tree README preparation edit included. It contains all 51 changed
product, test, fixture, manifest, documentation, inventory, and script paths
below, including the Phase 5 manifest validator and exact no-skip changes.
No source path was inferred from a summary's `key-files` list. The planning
artifacts and project context are provided separately below.

The implementation HEAD at scope capture was
`0ac23d1260ae1e3ddd1a1d5f2a9c4a815a9a071f`. The table records SHA-256 of each
path's current working-tree bytes. The scope fingerprint is SHA-256 over the
UTF-8 concatenation of the sorted rows as `path<TAB>sha256<LF>`; it is
`6cea5aa9ed15a4dbe81aa888f9d97040e6cba5290d3244b658f464fc06b58321`.
This binds the path set and its contents, not a Git commit object. Root should
refresh it if a review fix changes any listed source path.

| Repo-relative path | SHA-256 |
|---|---|
| `CMakeLists.txt` | `288e7f26ade4e02c202ee1cc94795a219d8d44592965e1cb26e1c4d3fdea5505` |
| `README.md` | `ecfaac2e130c26685ef3e3a874ee29fd60195f62614b0c13d160602c2afd2b84` |
| `consumer/main.c` | `77ba351b36417ef8831daad869fa42fcc83d33d3aed5c78490aeb9cb40f9f6ec` |
| `core/bus.c` | `cc390844278e5da51f321eb15768d11acf744095494a48355d88996d2ad23499` |
| `core/bus.h` | `d9325a1f8950aa6157bc3531444ef4907baa3d7dda10c7be0ecba7be06b383f9` |
| `core/input.c` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` |
| `core/internal.h` | `de7fa5699e6d666a36b3cf7211bdb6de725a31770fb7804e51865597d04c92d4` |
| `core/machine.c` | `b9e37062e2899aa7b5a6a5b6480cefe276f50229452d4a956a7a96648d567625` |
| `core/snapshot.c` | `faf02f1dd704aa011a9d8efdcd58036fda2f654b029ad2a3406a60dbaf350190` |
| `core/timer.c` | `79f6d0a44e07606e44b299fa1e7de512ac067e56b7bde425f2666792e600f5e7` |
| `core/video.c` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` |
| `docs/bus-video-policy.md` | `f5a746209dc6a3691a9ed1cffade427e3dff3761f8b8c004cc870381c63ce3f5` |
| `docs/claims/skeleton-repeatability.claim` | `1d4e01a83dd11261d6c0c25bb1fad6bf9b4f19dfc64866c833b4386a6d828112` |
| `docs/input-replay.md` | `6717540eb03f368eb5ed5dc39b509f46f3e89d31d20952a22e6ec246ef2469aa` |
| `docs/public-suites.md` | `939659f21511521b32b2fa242a4a4b9c27bef302f87e7028c698a4f9472ed408` |
| `docs/snapshot-video.md` | `12d7739c76fa759be8865f26791a014e001ea5eebd03acd73093a718fa627279` |
| `docs/timer-video-revisit.md` | `9d704be54f379715ce9d30583fc25feb6cf1eecddfa139357ace6eeb1ae131b1` |
| `docs/video-faults.md` | `3e9119e1c550ce6a6d39d063d52cb191db8b124b0011008ec4978d8fea8cabfd` |
| `docs/video-input.md` | `292257f43b1302ba4c363ae3e11d8368e4a86a99d82e75f45f63be8aea2f7992` |
| `fixtures/diagnostic/input_probe.cfg` | `64daaf42011ef8fc5f49307225ecf8ec082b5fc52cf2c06048f1a52592b730f9` |
| `fixtures/diagnostic/input_probe.s` | `e2ed6a86f63c1eafcd2f45cefe2706d6cd4081469c34bbcc38c6de2bf8b2e2ca` |
| `fixtures/diagnostic/input_probe_tracer.s` | `2ce520b03e375e92893ae570f7ab7139a9a07d92b5f23037ab4470fcfa6a1a60` |
| `fixtures/manifests/input-probe.manifest` | `ea439d8ed75be0a72eca3be4755bfb2e64c30b94eb45d73ef6e7694bd6d7f9f3` |
| `fixtures/manifests/page-mode-tests.manifest` | `54ef001c06b44b620350941ed246e9bf409baa8cd7f56865f41f7163708c4ce3` |
| `fixtures/suites/page-mode-wrapper.s` | `8e8d403eb5b58db1914d3e105a2c5da4ebada05c8aa2a74c7953507e00e4d18d` |
| `include/lynxplosion.h` | `fbcb8c0f1bb3029764ec2def40bcae4acb9e177789e0f97113758564224fcb64` |
| `runner/bundle.c` | `cab3084119db30256d2fda8f72f19abda31ca4c70a37f611a216472376b17f84` |
| `runner/bundle.h` | `835d3a6447a72e73f5418a8ecca4fdd58318393b5ca1c2b0ce1705e25055b384` |
| `scripts/build-page-mode.sh` | `163a5ba66568eda1e04b452a35280810ec19218fffccf6fdfe413f6b1b317af7` |
| `scripts/check-manifests.sh` | `a58d335c4ed6f93b862cded220520acbfa0105749a2a33ee357b4c4814ce7a5c` |
| `scripts/generate-page-mode-oracle.sh` | `faa03ce3f05904259d1fd99d98145a277bc2b027a00a7151f8724d6254bb6fea` |
| `scripts/validate-kv.sh` | `cc433c0ea38dc4c18008bfd13182cb47daa49730b819d2d2f093da3282121bdc` |
| `tests/api/test_api.c` | `fdae740881c8846bcee5c235396bfe0016b4f47fd312d718be03b794cef14575` |
| `tests/api/tests.cmake` | `65b614d39f8d165f0f36e33b4506cabdd21e46a779bc67c9ecaccfc922dadcbe` |
| `tests/cmake/no_skip.cmake` | `1376aedc883bea80d3c8b5ec30fbcd8d2ed31bb7feb80e371243cc0b852f2422` |
| `tests/consumer/check_artifacts.c` | `5286f81877812d0908f442c3c1267f0f5e8c3e436ca32c482e7bc3a586c877ea` |
| `tests/consumer/frame_driver.cmake` | `a432f97ecd7d1fbfeea2227d006db57bf662e630d9a7c1718d7e86a14eb1702b` |
| `tests/consumer/tests.cmake` | `594c67f0696e7e22dd022baedaf1926ea93673f9e37450c5cd498716be72330e` |
| `tests/det/test_det.c` | `6920ab74afc6953e2c3aa307ac003a92afd018d8aa2a192c71e12607a6cfe614` |
| `tests/det/tests.cmake` | `89efffb31182482791468700aa4e3310431857137238daf75d6dcfbc29d34af0` |
| `tests/det/two_runs.cmake` | `7d06b313d5eec3c75685eef95d6383033976498fbb7a4eff0f67cb1bc5c145f7` |
| `tests/gates/manifests_controls.sh` | `e52b7f97ffce6577684b7c8d5fcf168e4a3c1620cf8e6101f3a573f9b0f22c29` |
| `tests/gates/page-mode-script-controls.sh` | `9b695af078a67244cd33283a71ddd4d41c358f39cc00cbc25cbbfd9bd5dad182` |
| `tests/golden/skeleton.digest` | `593caf1ffaacb184fec795c8cb77090690eee5769e2e7d67f31b9b43577e9fb4` |
| `tests/header/header.c` | `ad623103c552bc996c83ce5eb9f07d03496260b890b6cd90fceb52e9865a5325` |
| `tests/inventory/fast.txt` | `1e74578b2e13db45953c6fd02e4793f0e04623f4d4b3d7158b04a69049bd9c5d` |
| `tests/inventory/skips.txt` | `aa17c79afa50cebf430280306ff519c44931c82788e3812083944ee71d31f431` |
| `tests/suites/run.cmake` | `f85ec78c6aeb76b563bd4f2d7cae55385e9887e123800fed3c6b8e3fa5af4938` |
| `tests/suites/test_public_suites.c` | `ba14e2fd153edc215fcfeb8225b2b14e7e20fd11034504bffada2142993c02ef` |
| `tests/suites/tests.cmake` | `987c58c87c8b9148ca3f912676d4a32589d3c45f1c7fe02def68f6c09633e9ba` |
| `tests/video/fault_controls.cmake` | `7aa2da12bcbc24834bb98d9aa5949ee7e9b35432da354eaa948518e029e62905` |

## Planning context supplied separately

The source review should be read alongside the approved task and current
planning record, which are not part of the product-source hash set:

- `05-12-PLAN.md`, `05-11-SUMMARY.md`, `05-CONTEXT.md`, `05-PATTERNS.md`,
  `05-DISPLAY-RESEARCH.md`, `05-VIDEO-CONTRACT-RESEARCH.md`, and
  `05-INPUT-EVIDENCE-RESEARCH.md`.
- `05-SOURCE-AUDIT.md` provides the phase goal, source mapping, all twelve
  decisions, requirements, edges, and authored threat scope.
- `05-VALIDATION.md` maps each of the 24 phase tasks to exact test names and
  the current observed status; prior task evidence is linked to each summary.
- Project authority and unresolved decisions remain in `AGENTS.md`,
  `.planning/AUTONOMY.md`, `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`,
  `.planning/ROADMAP.md`, `.planning/STATE.md`, and `.planning/LESSONS.md`.
- `05-QUALIFICATION.md`, `docs/public-suites.md`,
  `docs/timer-video-revisit.md`, and `docs/claims/skeleton-repeatability.claim`
  separate the current software observations from strict disagreement,
  repeatability, declared skips, and remaining hardware/owner boundaries.

## Required independent reviews

All three verdicts are pending; this executor's focused checks are not a
substitute for them.

| Review | Required scope | Current disposition |
|---|---|---|
| Code review | All 51 hashed paths, with emphasis on `core/machine.c`, `core/video.c`, `core/input.c`, `core/snapshot.c`, `consumer/main.c`, `runner/bundle.c`, test oracles, and validator/no-skip controls | pending root review |
| Security and threat review | Caller-buffer and snapshot validation, replay input/path/output handling, owned scratch and cleanup, pinned source/tool/manifests, and generated-evidence privacy | pending root review |
| Goal and coverage review | Phase goal, all requirements, D-01 through D-12, research edges, authored threat mitigations, exact observations, skips, and limits | pending root review |

UI review is not applicable: this is a native/headless C API phase and has no
frontend evidence. The source audit and validation map are planning context,
not proof of review completion. No independent verdict has been recorded yet.

## Goal, requirement, and decision coverage

The project-authored `input_probe` homebrew and independent consumer produce
repeatable raw frames under recorded logical input and check guest progress,
input samples, pens, palettes, line ticks, and rendered output. API refusal,
snapshot continuation, row/frame boundaries, bus behavior, input orientation,
and named video/input fault controls have focused evidence in the linked
summaries. The validation map records those tests individually; their pass
status is implementation evidence, not a blanket accuracy claim.

| Area | Current evidence and disposition |
|---|---|
| VID-01 / D-01 | Programmable timer-driven line/frame events and half-open frame stops have named API tests. First-edge and frame phase are project policy; no physical cadence is claimed. |
| VID-02 / D-02, D-10 | DMA arbitration, CPU page continuation, and fault controls have named tests. Page-mode source/build/oracle identity is present, but both profile runs stop at `$FD8C` before results; the declared skips remain. |
| VID-03 / D-03 | Raw pens and per-row palette snapshots have independent checks. The palette snapshot boundary and DMA/refresh schedule remain unverified software policy. |
| VID-04 / VID-05 / D-04 | Caller-owned bounded frames, refusal postconditions, stop boundaries, reset/load, and format-5 continuation have tests. Host presentation remains outside the core. |
| INP-01 / API-06 / D-06, D-07 | Logical masks, handedness, exact guest-tick replay, parser bounds, and retained CPU-read sampling have focused tests. Physical edge polarity and disconnected cartridge-line values remain unqualified. |
| HB-02 / D-08 | The original source-built homebrew changes its observed frame under the recorded input; the consumer uses the public API and checks raw and rendered outputs independently. |
| CON-01 | Fresh, sliced, watched, restored, and multi-instance video/input observations are recorded. Real-audio integration remains a Phase 6 obligation, so CON-01 is not globally complete. |
| EVD-02 / D-10, D-11 | Legacy D-10 remains exactly index 14 `0c` versus source `0d` under flat5/no-DMA/counter-zero. VIDEO_POLICY1 independently reports `0f` versus `0d`, 15/16 rows agreeing, strict mismatch, and no pending descriptor. Global EVD-02 stays open. |
| EVD-05 / D-09 | Three video and five input copied-source semantic faults pass their exact intended assertions with clean brackets. The recurring requirement stays partial for Phase 6 audio faults. |
| D-05 / D-12 | The native Lynxplosion API is the scoped output; no LibFuturo files changed and O9 remains open for a future shared sidecar need. Independent reviews and final-source macOS/Linux/exact-head CI are still required. |

No scoped requirement is marked complete by this handoff. The plan's
prohibitions remain active: no firmware or commercial content, physical
qualification, LibFuturo change, release, or additional publication channel.
O1, O5, O6, and O9 remain owner decisions where applicable.

## Authored high-severity threat dispositions

| Threat | Required mitigation | Evidence now | Remaining work |
|---|---|---|---|
| T-05-34 — tampering between review and qualification | Bind reviewed source, evidence commit, pushed head, and hosted CI to exact identities | The exact 51-path content manifest and source-scope fingerprint are recorded above; local focused C121 passed | Root reviews/fixes, then Task 2 must bind the final reviewed and qualified head |
| T-05-35 — generated evidence exposes private paths or artifacts | Keep raw logs private, normalize committed evidence, run tree hygiene before evidence commit | C121 tree hygiene passed; raw C121 output stayed in the managed tool session and was not committed | Guarded preparation commit reruns tree hygiene and diff checks; Task 2 must repeat after any generated evidence is written |
| T-05-36 — green software checks become unsupported claims | Separate repeatability, raw source agreement, skips, strict mismatch, and owner/hardware gates | Claim profile remains `none`, tier `R`; current inventory digest matches; timer mismatch and page-mode skips are explicit | Independent threat review and final qualification must preserve these boundaries |
| T-05-SC — pinned source/toolchain tampering | Keep exact pins, manifests, notices, and no new package/dependency | Current manifest validation passed; earlier Task 05-10 records source/oracle/tool identities and hostility checks | Security review must verify the final evidence-to-source chain |

## Review resolution and continuation rule

No review finding has been accepted or dismissed in this preparation. Root
records each independent verdict and finding disposition here, applies
accepted fixes on the current authorized main checkout, reruns the affected
focused checks, and refreshes the source manifest if a listed path changes.
Task 2 may resume only after the required reviews and accepted fixes are
resolved. The final macOS, self-contained Linux, and hosted exact-head gates
belong to Task 2; no broad qualification or later phase is claimed here.
