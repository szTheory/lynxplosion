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

## Independent review handoff results

Reviewed source head: `013de47ff566ae9c997542ddfba6fe15ff93aebe`; the 51-path fingerprint above remains unchanged. The full standard-depth initial report is retained in `05-REVIEW-INITIAL.md` and current `05-REVIEW.md`. Coverage review found no missing/vacuous software tests; Task 12 final environment qualification remains pending. The independent ASVS1 authored-threat review found 36/37 mitigations present and one high-severity open lifecycle obligation, T-05-34, for final reviewed-source/pushed-head/CI binding. No risks were accepted. It permits collecting qualification evidence but blocks final phase advancement until that evidence exists. Its static mitigation-presence audit is not a claim of exhaustive vulnerability testing.

Root verified each cited code path and accepts all four code findings for repair before Task 2. The destructive manifest-control cleanup is pre-existing and outside the phase diff, but it is a concrete blocking safety defect in a modified, required gate. Its repair is explicitly included in the Plan 12 source scope.

| Finding | Disposition | Required proof |
|---|---|---|
| CR-01 cleanup before containment | accepted; fix pending | Refuse dangerous/stale/aliased WORK before mutation; prove on synthetic owned replicas, never actual source directories; preserve a legitimate successful control run |
| WR-01 backing-RAM opcode precheck | accepted; fix pending | Device/RAM disagreement in both defined/undefined directions uses the actual mapped byte without side effects; preserve private flat bus and RAM/page behavior |
| WR-02 handedness guest hash shape | accepted; fix pending | Both exact guest identity fields reject truncated/nonhex values while valid existing manifests and other unknown-key guards remain accepted/rejected correctly |
| WR-03 production legacy test switch | accepted; fix pending | Production replay ignores test-only legacy override; historical tracer remains a strict positive through the test-only target; ordinary production replay stays green |

Each accepted fix requires a focused regression that exposes the old behavior and passes the corrected behavior, with exact source provenance and isolated synthetic hazards. Root will re-review the repaired scope, refresh this source manifest and security/coverage evidence as needed, then explicitly resume Task 2. No broad qualification has started.

## Applied fixes awaiting independent delta verification

All four accepted findings have focused old/new regression proof and individual normal-hook commits. Source head after repairs is `ec38c34f233168c15a6049ef5768b1989fee6353`. The fixer report is `05-REVIEW-FIX.md`; final independent delta review remains pending, so Task 2 is still gated.

| Finding | Repair commit | Recorded verification |
|---|---|---|
| CR-01 | `620e7c2` | Exact old script deletes only a synthetic private marker; safe preflight/ownership guard controls and registered manifest self-test pass |
| WR-02 | `f61a82f` | Old validator accepts short/nonhex variants of both keys; all four corrected refusals and valid/unknown/incomplete manifest cases pass |
| WR-01 | `358874d` | Exact old machine fails mapped/RAM disagreement in both directions; mapped input/video/timer across all profiles, zero-tick refusal, private flat bus, page and profile regressions pass |
| WR-03 | `ec38c34` | Old production override diverts a valid timeline; corrected production outputs match with/without test env, historical test-only tracer remains strict, replay/snapshot/silence pass |

The fixer's generic human-verification flag on WR-01 is not treated as a claim that a human reviewed it or as a new owner decision. Project AUTONOMY requires automating software verification where possible. Root's independent source/logic review plus the old/new executable regression and final qualification will adjudicate this deterministic logic change; no rights, physical or subjective gate is being bypassed.

### Repaired source binding

The original 51-path table above remains the initial reviewed snapshot. Only the six replacement hashes below changed; every other source path/hash is identical. The complete repaired 51-path fingerprint is `4016d03fbd62cced5328c9d283878d087792cc1e91cc9e85cfaaa3f1a599ca6d`, computed with the same sorted `path<TAB>sha256<LF>` convention. No new or deleted product path was introduced by these fixes.

| Repaired path | Current SHA-256 |
|---|---|
| `consumer/main.c` | `a0cdfe145390dea8eb7854ed208ed2ee4341ebce86dbf2ac2814c604e15dabaa` |
| `core/machine.c` | `74dc654c2a5292680058d2f0d046015f847bdd6a934cd3b0194d2cd86b6faa7c` |
| `scripts/validate-kv.sh` | `82778e9233b9a77232ae094ccb297e744071e7f28a0c472f65c1084228ada811` |
| `tests/api/test_api.c` | `cb1732ba82501c544bdb714d4751b70233e2f0b9b99069f772de205b5ee43770` |
| `tests/consumer/frame_driver.cmake` | `8e63fccfe8f33ce8928a2fed8f2023eb991bb21871e017a2aeea872f80e10387` |
| `tests/gates/manifests_controls.sh` | `c62a8d1a94d05bd3fd258129609688a928d04423ce90029ee44895f17c6bde06` |

## Sibling replay-control repair

The first independent delta review resolved the original four findings and found WR-04: the sibling interleaved and restore test environment controls still reached production. Root accepted this same-boundary repair. Commit `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` gates both reads and retains explicit test-target mode assertions. The old production source refuses a valid five-event timeline under the restore variable; the corrected production baseline and all three override runs produce independently validated identical artifacts. Focused consumer checks passed; see `05-REVIEW-FIX.md`. Independent final two-path delta verification is pending.

Only `consumer/main.c` and `tests/consumer/frame_driver.cmake` changed since the six-path repaired binding; all other 49 scoped hashes are unchanged. The latest full 51-path fingerprint is `e4fd27130097f8a23c8162403a58e933104bc7a35b160ff646767e402052c732` using sorted `path<TAB>sha256<LF>`. Replacement hashes:

| Path | SHA-256 |
|---|---|
| `consumer/main.c` | `63abd1788e922697e4dba33d3e6a46f8952f4bd626b372faaa736f8e04bd5871` |
| `tests/consumer/frame_driver.cmake` | `8336afa4bbd5176e18ae101082f766a99c783b00f48cd084271a210bb92bfe1b` |

No new source path, inventory entry, dependency or authority is introduced. Broad qualification remains pending.

## Final independent repair verdicts

All three independent delta reviewers returned terminal results at source head `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0`: code review is clean with all five findings resolved; coverage found no missing/vacuous repair check; security found affected mitigations preserved with only T-05-34 final qualification binding open. The initial 51-path and subsequent six/two-path scope provenance are retained. No independent reviewer reran the recorded focused tests. Root authorizes Task 2 qualification after these report commits; this is an internal continuation, not phase completion.

## Qualification preflight repair

The first macOS canonical gate stopped at ShellCheck 0.11 SC1007 before CMake/tests. Four POSIX command-prefix empty assignments in the CR-01 helper used `CDPATH= cd`; commit `ce23448` spells the identical empty value explicitly as `CDPATH='' cd`. No cleanup or containment logic changed. ShellCheck, shell syntax and both registered manifest checks passed after the edit. An independent one-path mechanical delta check is pending before restarting the final gates. Only this scoped path changed; all other 50 hashes remain identical. The full 51-path fingerprint is `b5568b960e8ce7f1c265f106e1d8dbc2e58455081497c8d69c1698742af10ed6`; helper SHA-256 `f24b2b096b788a99e4992bb01528ddb851df9377dc90da27c5f9711c416ff5f2`. No Linux or hosted run started on the preflight-failing source.

The independent one-path review returned clean at `ce2344851c7adebea900d590f34eda31eef25a1e`: all four quoted empty assignments are semantically equivalent and containment, ownership, target selection and cleanup logic are unchanged. Prior security/coverage deltas remain applicable; the two focused manifest tests and ShellCheck passed. Task 2 can resume on this source after the report commit.

## Final-gate harness corrections

The macOS final-gate attempt at `fe7afdd39fb498cfe9146b2d1263584e65a2bd21` passed ShellCheck but failed 12/202 CTests before artifacts/history. Bounded diagnosis showed stale snapshot-layout assumptions in CPU seam/diagnostic controls, omitted input/video modules in timer-mutant builds, and a Darwin compiler stack-probe helper absent from the symbol allowlist. Commit `9290c743e67e9a8aabc60aff841c831c17186410` repairs four test-harness/platform files; all 13 affected focused checks then passed with semantic assertions intact. No emulator implementation/oracle changed. Details: `05-QUALIFICATION-REPAIRS.md`.

These four paths were unchanged since Phase4 and therefore expand the Phase5 reviewed source scope from51 to55. All original51 hashes remain exactly unchanged from the preceding binding. Current sorted55-path fingerprint: `d9cdad7a5c25079d5b7c38c1c09e7747f518ba0581720bc0cccb4e0f2a376c51`. Added scope hashes:

| Path | SHA-256 |
|---|---|
| `tests/cmake/abi.cmake` | `2cbf21b96f8c5258e327e62eea6d5f876b0ab6eb3173d30dec7344b567ee8e5e` |
| `tests/cpu/test_cpu_vectors.c` | `70bbc989dde386fba7dc15753869a27e367daf7227d95f9580c15467bee04f9e` |
| `tests/diag/test_diag.c` | `99aa549f0f522e53bdf9961893dcdda5f912c411d50e026e49959e5dd0d20ebf` |
| `tests/timers/fault_controls.cmake` | `eeefca41067bc328152f20698644c36e396094acc3138a40de8c03221852b030` |

Independent affected-scope review is pending. No Linux/hosted qualification or broad passing macOS result is claimed.

All three independent four-path delta reviews returned terminal results at `9290c743e67e9a8aabc60aff841c831c17186410`: code clean, coverage/control assertions preserved, security mitigations preserved; T-05-34 qualification remains open. Root also corrected three stale numeric values in the opening snapshot layout comment (format5/header256/payload77948). Everything after that comment is byte-identical, so executable behavior is unchanged; the intended layout values were independently checked in the harness review. Final55-path fingerprint before restarted qualification: `82cc7aa3d2f400da9b9ffa4cd221e2005b23d5c6b3e70e8f7fd6607fcbcab749`; replacement `core/snapshot.c` SHA-256 `e3d0c18748fec5847e3d2b45c4a972bc0d75ca3904f2a9f7b742cf768c68c5fd`. All other54 hashes unchanged.

## Consumer success-path scratch repair

The canonical macOS run at `d7d64c6` passed CTest/JUnit but failed the artifact gate because successful consumer tests retained forbidden scratch artifacts. Commit `3aaf004` introduces exclusive per-invocation work, canonical containment/owner checks and success-only cleanup across all top-level returns. Seven affected semantic cases passed; controlled failure preserves artifacts and unsafe WORK refusal preserves caller markers. See `05-CLEANUP-REPAIR.md`. Only frame_driver changed; other54 hashes unchanged. Latest55 fingerprint `69f6a655f0aeff73f8270cdb14deebb5775d3cee99b6cf7f9d6e0b2f5ac716c4`. Independent affected-scope review and final full qualification remain pending.

Independent code and security cleanup-delta reviews returned terminal clean/preserved verdicts at `3aaf004f08fb40d6aad6c9fc126aaa86fefd681f`. They verified fresh exclusive scratch, canonical/owner checks, success-only deletion across all returns and failure retention. They inspected focused evidence without rerunning tests. Root can resume Task2 after the report commit; artifact/history and final environment qualification remain pending.

## Portable shell guards and restored-tool inode

The Linux canonical preflight found15 ShellCheck0.9 SC2015 diagnostics in three shell files. Root replaced compound fallback forms with equivalent explicit conditional refusals. A focused control then exposed an exact private restored executable killed on its old inode while a byte-identical fresh inode ran. Commit `735cfa1` also restores that tool through a verified fresh file and atomic rename. Both lint versions and all affected post-fix controls pass; no oracle/production behavior changed. See `05-PORTABILITY-REPAIR.md` for differential proof and failed-run quarantine. Latest55 fingerprint `5de70b75e913919755c198ceef2a989ebd8c636908f37986fedbe17ebbd11dc8`; four script hashes changed, other51 unchanged. Independent affected review and new final-source environment gates remain pending.

Independent code and security reviews returned terminal clean/preserved verdicts at `735cfa169b0cfcf7f4687d746c87461dc1677e1f`. They confirmed inverse-condition equivalence, option bounds, existing/dangling symlink refusals, pinned hashes, ownership and exact negative diagnostics; verified private-tool restoration retains final positive brackets. The55 hashes match the latest fingerprint. Task2 may resume final-source qualification after this metadata commit; T-05-34 remains open.
