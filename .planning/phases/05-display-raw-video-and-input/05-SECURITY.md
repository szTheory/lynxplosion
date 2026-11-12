---
phase: "05"
slug: display-raw-video-and-input
status: gaps_found
threats_open: 1
asvs_level: 1
block_on: high
register_authored_at_plan_time: true
created: "2026-11-12"
---

# Phase 05 — Security

Independent authored-threat mitigation review, plus affected-mitigation reviews after accepted fixes. This ASVS1 presence and focused boundary review is not an exhaustive vulnerability scan. The auditor made no file changes and ran no tests. Root records the returned structured verdict here.

## Trust Boundaries

| Boundary | Protection |
|---|---|
| Caller and guest bytes to machine | Span, policy and reachable-state validation before mutation |
| Guest observations to host evidence | Independent semantic assertions and exact source/configuration identity |
| Replay files and environment to consumer | Canonical bounded parsing, test-only hooks excluded from production, owned no-replace publication |
| Test scratch and generated evidence to checkout | Fresh invocation ownership, containment checks, rights manifests and privacy gates |

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation and independent evidence | Status |
|---|---|---|---|---|---|---|
| T-05-01 | Tampering | native buffer→machine | high | mitigate | preflight all spans before mutation; frame_driver sentinel and public-only checks; core/machine.c:669,701;CMakeLists.txt:87;tests/api/test_api.c:7382 | closed |
| T-05-02 | Repudiation | homebrew→frame evidence | medium | mitigate | independent known pen/palette/progress assertions and pinned original source recipe; tests/consumer/check_artifacts.c:806,844;tests/consumer/frame_driver.cmake:22,171 | closed |
| T-05-03 | Denial of service | timer→frame scheduler | high | mitigate | finite event cursors, positive bus costs and half-open budget; no second clock; core/machine.c:478,553,592;core/timer.c:187,228;core/video.c:161 | closed |
| T-05-SC | Tampering | pinned toolchain/source | high | mitigate | Reuse exact SHA256 pin/build-record/notice gates; no package install or new dependency. Unknown package requires legitimacy audit and blocking human gate before install.; pins.txt:1;scripts/fetch-pin.sh:146;scripts/check.sh:134;scripts/build-page-mode.sh:45,95;tests/consumer/frame_driver.cmake:22 | closed |
| T-05-04 | Tampering | guest geometry→capture arrays | high | mitigate | clamp stored rows to102, validate address arithmetic, guard row103 with canaries; core/video.c:161,202;tests/api/test_api.c:7025,7129 | closed |
| T-05-05 | Repudiation | palette observation→claims | medium | mitigate | label chosen row boundary unverified and compare asymmetric raw palette bytes; core/video.c:202;docs/video-input.md:35;tests/api/test_api.c:7180 | closed |
| T-05-06 | Denial of service | same-tick frame return→caller loop | high | mitigate | consume event cursors before return and assert bounded consumed0 behavior; core/timer.c:228;core/machine.c:553,732;tests/api/test_api.c:6964 | closed |
| T-05-07 | Tampering | bus master→CPU request | high | mitigate | retain posted request and assert exactly-once grant across ownership holds; core/machine.c:529,564,614,631;tests/api/test_api.c:6337,6349 | closed |
| T-05-08 | Denial of service | DMA/refresh deadlines→scheduler | high | mitigate | positive finite grant costs, bounded80-byte row and coalesced refresh cursor; core/video.c:126,161,185 | closed |
| T-05-09 | Repudiation | new policy→legacy evidence | high | mitigate | distinct policy identity and exact flat5 regression; forbid applying old pending exception to new env; core/machine.c:592;core/snapshot.c:600;tests/api/test_api.c:6764;tests/suites/test_public_suites.c:3506 | closed |
| T-05-10 | Tampering | caller spans→machine/output | high | mitigate | full-capacity pairwise span preflight and whole-buffer refusal matrix; core/machine.c:669,701;tests/api/test_api.c:7382,7616 | closed |
| T-05-11 | Information disclosure | old capture→new load | high | mitigate | reset/load clear input and staged video; sentinel and interleaved-instance controls; core/machine.c:104,173,293;core/video.c:50;tests/api/test_api.c:8804 | closed |
| T-05-12 | Denial of service | no-frame stop→host retry | medium | mitigate | explicit consumed0 time/device terminal rules and finite loop guards; core/machine.c:491;consumer/main.c:2077,2174;tests/det/test_det.c:1308 | closed |
| T-05-13 | Tampering | input mask→guest device | high | mitigate | unknown-bit refusal plus independent all512 bit/orientation table; core/input.c:11;tests/api/test_api.c:7941,7975,7713 | closed |
| T-05-14 | Repudiation | sample tick→recorded timeline | medium | mitigate | record actual grant/sample tick and exact adjacent-edge assertions; core/machine.c:445;tests/api/test_api.c:7880,8638,8684 | closed |
| T-05-15 | Elevation of privilege | partial Suzy support→unsupported engine | medium | mitigate | unsupported control bits/registers refuse before mutation; no fabricated sprite/math state; core/input.c:20,67;core/machine.c:445;tests/api/test_api.c:8064 | closed |
| T-05-16 | Tampering | snapshot bytes→live state | high | mitigate | validate full candidate ranges/cross-relations after hash before single apply; core/snapshot.c:568,620,381,705,712;tests/api/test_api.c:9055 | closed |
| T-05-17 | Information disclosure | snapshot/observation→host state | high | mitigate | serialize guest fields only; exclude pointers/watch/script and canonicalize unused bytes; core/snapshot.c:249,279,492,454;core/video.c:22,37;tests/det/test_det.c:1552 | closed |
| T-05-18 | Repudiation | digest equality→evidence | medium | mitigate | compare semantic progress and exact raw packets separately with sensitivity controls; tests/det/test_det.c:1321,1334,1423,1500 | closed |
| T-05-19 | Tampering | timeline bytes→guest input | high | mitigate | strict bounded full-file parse/hash before output and exact-tick driver; consumer/main.c:195,365,379,1980,2091 | closed |
| T-05-20 | Denial of service | replay/frame returns→host loop | high | mitigate | finite records/frame limits, sequence-progress guard and consumed0 terminal handling; consumer/main.c:30,379,2040,2077,2143,2174 | closed |
| T-05-21 | Repudiation | checkpoint cursor→replay identity | high | mitigate | bind full script digest plus explicit pre/post-edge cursor phase and current mask; consumer/main.c:1597,1622,1636,2093,2112;tests/consumer/frame_driver.cmake:611 | closed |
| T-05-22 | Tampering | host paths→published files | high | mitigate | exclusive stage ownership, alias/containment checks and outside-target sentinels; consumer/main.c:784,828,875,909,1215;tests/consumer/frame_driver.cmake:503,545 | closed |
| T-05-23 | Repudiation | stale artifacts→success | high | mitigate | fresh guest execution plus semantic progress and response diagnostics; consumer/main.c:1707,1952,2065;tests/consumer/frame_driver.cmake:332,344 | closed |
| T-05-24 | Information disclosure | failed host run→leftover output | medium | mitigate | only complete matching bundles publish; cleanup verifies invocation nonce before removing stage; consumer/main.c:846,868,892,1215;tests/consumer/frame_driver.cmake:375 | closed |
| T-05-25 | Repudiation | mutant build→claimed detection | high | mitigate | unique hash-changing anchor, successful build, exact intended assertion and fresh brackets; tests/video/fault_controls.cmake:148,202,250,283,303 | closed |
| T-05-26 | Tampering | fault scratch→checkout | high | mitigate | copy sources to nonce-owned contained paths and never edit production as mutation; tests/video/fault_controls.cmake:128,170,182,202,315 | closed |
| T-05-27 | Denial of service | fault execution→CI | medium | mitigate | finite guest bounds and measured per-case CTest timeouts; source faults are not broad reruns; tests/video/fault_controls.cmake:156,253,265,277;tests/api/tests.cmake:36,94;docs/video-faults.md:15 | closed |
| T-05-28 | Spoofing | page source→oracle identity | high | mitigate | verify pinned source/tool/wrapper/build/labels and derive exact6-row source alternatives; scripts/build-page-mode.sh:45;scripts/generate-page-mode-oracle.sh:71;tests/suites/test_public_suites.c:655,1480,4338 | closed |
| T-05-29 | Repudiation | runtime unavailable/skip→pass | high | mitigate | precise first-request blocker, matching declared skip and unrelated-stop negative control; tests/suites/test_public_suites.c:4404,4423,4458;tests/suites/run.cmake:1850;tests/suites/tests.cmake:47;tests/cmake/no_skip.cmake:17 | closed |
| T-05-30 | Tampering | new suite integration→legacy timer evidence | high | mitigate | preserve old wrappers/layout/descriptors and run existing oracle/symbol/build controls; scripts/build-page-mode.sh:1;scripts/generate-page-mode-oracle.sh:1;tests/suites/tests.cmake:1;tests/suites/test_public_suites.c:1641 | closed |
| T-05-31 | Repudiation | new env→legacy exception | high | mitigate | exact source/policy/layout/counter identity before pending admission, explicit new-env refusal control; tests/suites/test_public_suites.c:1641,1690,3506;tests/suites/run.cmake:560 | closed |
| T-05-32 | Tampering | oracle/counter→desired timing | high | mitigate | unchanged pinned expectation/layout, frozen independent policies and initial-vs-increment trace; tests/suites/test_public_suites.c:3308,3501,3953;tests/suites/run.cmake:589,603 | closed |
| T-05-33 | Spoofing | incomplete result→agreement | high | mitigate | full16-row extent/model/completion checks; separate raw execution and strict agreement statuses; tests/suites/test_public_suites.c:1932,3407,3555,3617;tests/suites/run.cmake:522 | closed |
| T-05-34 | Tampering | qualification commit→evidence | high | mitigate | bind exact reviewed source/pushed CI head after accepted fixes, verify bytes for metadata-only followup; .planning/phases/05-display-raw-video-and-input/05-QUALIFICATION.md:69;05-REVIEW-RESOLUTION.md:1;05-12-PLAN.md:96 | open — final qualification binding pending |
| T-05-35 | Information disclosure | generated evidence→public repo | high | mitigate | private raw logs stay temporary, normalize repo-relative evidence and hygiene before commit; scripts/hygiene.sh:41,132,200;.planning/AUTONOMY.md:1;05-QUALIFICATION.md:29 | closed |
| T-05-36 | Repudiation | green software gate→unsupported claim | high | mitigate | separate repeatability/raw agreement/skips/strict mismatch and keep global/owner gates open; docs/claims/skeleton-repeatability.claim:1;tests/suites/test_public_suites.c:4246,3505;README.md:38;05-QUALIFICATION.md:46 | closed |

## Reviewed source and repair deltas

Initial independent review: `013de47ff566ae9c997542ddfba6fe15ff93aebe`, 51 paths, fingerprint `6cea5aa9ed15a4dbe81aa888f9d97040e6cba5290d3244b658f464fc06b58321`. The complete path/hash ledger is `05-REVIEW-RESOLUTION.md`.

The initial four repairs changed six paths. Independent static delta review found cleanup preflight/ownership, exact handedness hash validation, pure route-aware opcode peeking and production legacy exclusion preserve affected mitigations. The final WR-04 two-path repair at `78592c38aeb9e2db5a6b0a1d952b8b2dd1cc8fe0` excludes the remaining interleaved/restore environment selectors from production. All 51 current hashes match fingerprint `e4fd27130097f8a23c8162403a58e933104bc7a35b160ff646767e402052c732`; 49 paths are unchanged since the preceding delta. No implementation gap was found. No risk was accepted or transferred.

WR-04 affects T-05-02 and T-05-19 through T-05-24: both reads are inside `LYNX_CONSUMER_TESTING`; production selectors stay zero; canonical parsing, exact-tick advancement, checkpoint validation and owned publication remain intact. Dedicated test modes require observable success markers. The production regression independently validates and compares default replay and each test-variable override, including every raw/rendered frame and manifest. See `consumer/main.c:1980`, `consumer/main.c:2022`, `consumer/main.c:2222`, `tests/consumer/frame_driver.cmake:66`, `tests/consumer/frame_driver.cmake:643`, and `tests/consumer/frame_driver.cmake:694`. Focused executable old/new proofs are recorded in `05-REVIEW-FIX.md`; the independent auditor inspected assertions and did not rerun them.

## Accepted Risks Log

No accepted risks. No transferred risks.

## Security Audit Trail

| Audit date | Scope | Total | Closed | Open | Result |
|---|---|---|---|---|---|
| 2026-11-12 | Initial authored 37-threat review | 37 | 36 | 1 | No implementation gaps; qualification binding pending |
| 2026-11-12 | Six repaired source/test paths | 37 | 36 | 1 | Affected mitigations preserved |
| 2026-11-12 | Final two-path replay-control repair | 37 | 36 | 1 | Affected mitigations preserved; 51 identities verified |

## Remaining qualification obligation

T-05-34 is high severity and blocks final phase advancement. It does not block starting qualification: the remaining action is to bind final reviewed source bytes to macOS, self-contained Linux and exact pushed-head hosted CI evidence. Metadata-only followups must be identified and product/script identity preserved. Generated final evidence must pass privacy gates again. Root owns final adjudication from the qualification records; no owner risk acceptance is requested.

## Sign-Off

- [x] All authored threats have mitigation dispositions.
- [x] No implementation gaps or accepted risks remain.
- [ ] Final environment and pushed-head evidence closes T-05-34.
- [ ] `threats_open: 0` and `status: verified` after qualification.

**Approval:** implementation mitigation review complete; final qualification pending.

## Mechanical qualification-preflight followup

At `ce2344851c7adebea900d590f34eda31eef25a1e`, four command-prefix empty `CDPATH` values are spelled explicitly for ShellCheck 0.11. Independent code review confirms semantic equivalence and unchanged containment/ownership/cleanup logic. Root reuses the security verdict for unchanged mitigations; no additional security scan is represented. Other 50 hashes are unchanged, and the updated full51 fingerprint is `b5568b960e8ce7f1c265f106e1d8dbc2e58455081497c8d69c1698742af10ed6`. ShellCheck and both manifest tests passed. Final qualification remains pending.
