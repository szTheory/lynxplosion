---
phase: "03"
slug: memory-map-direct-load-startup-and-first-diagnostic
status: verified
threats_open: 0
asvs_level: 1
block_on: high
created: "2026-11-10"
---

# Phase 3 — Security

The initial and bounded delta audits verify 19 of 20 unique threats from source and control presence (ASVS L1). No blocking threats remain. Final local/Linux requalification and exact-head CI remain pending under the medium-severity delivery-evidence row. No owner risk acceptance is inferred.

## Trust Boundaries

| Boundary | Control |
|---|---|
| Host bytes to core | Validate BLL, snapshot, watch and buffer requests before mutation. |
| Guest state to evidence | Independent symbols, semantic result, pixels and identity validation. |
| Evidence to filesystem | Exclusive staging, checked writes, no-replace directory publication. |
| Toolchain to owned fixture | Existing pinned download/hash/build identity and source/text comparison. |
| Repository bytes to rights gate | Fail-closed text conversion and byte equality. |

## Threat Register

| Threat ID | Severity | Disposition | Component and evidence | Status |
|---|---|---|---|---|
| T-3-01 | high | mitigate | strict BLL input: `core/machine.c:224; tests/api/test_api.c:689` | closed |
| T-3-02 | high | mitigate | snapshot v3 identity: `core/snapshot.c:237,347` | closed |
| T-3-03 | high | mitigate | BLL ownership and aliases: `tests/api/test_api.c:584,654,689` | closed |
| T-3-04 | high | mitigate | runner binary input and metadata: `runner/bundle.c:196,467; runner/main.c:234` | closed |
| T-3-05 | high | mitigate | private vector adapter: `core/machine.c:271,283,337; core/snapshot.c:278` | closed |
| T-3-06 | medium | mitigate | normal startup and manifest migration: `tests/det/runner_cli.cmake:220; runner/bundle.c:219` | closed |
| T-3-07 | high | mitigate | CPU decoder: `core/bus.c:33; core/machine.c:340,364` | closed |
| T-3-08 | high | mitigate | raw/mapped inspect buffers: `core/machine.c:527,545` | closed |
| T-3-09 | medium | mitigate | map fault controls: `tests/api/test_api.c:3669; tests/api/tests.cmake` | closed |
| T-3-10 | high | mitigate | post-write watch bounds: `core/machine.c:176,217,439` | closed |
| T-3-11 | high | mitigate | restore/observer state: `core/snapshot.c:237,367; tests/api/test_api.c:2296` | closed |
| T-3-12 | medium | mitigate | watch/restore digest equality: `tests/det/test_det.c:513; tests/api/test_api.c:2328` | closed |
| T-3-13 | medium | mitigate | fixture identity and rights: `scripts/check.sh:353; fixtures/manifests/first-diagnostic.manifest` | closed |
| T-3-14 | high | mitigate | diagnostic symbols and completion: `tests/diag/test_diag.c:137,561,603,658` | closed |
| T-3-15 | medium | mitigate | runner result agreement: `tests/diag/test_diag.c:1655; tests/diag/tests.cmake:189` | closed |
| T-3-16 | high | mitigate | independent consumer input and encoding: `CMakeLists.txt:89; consumer/main.c:166,367,407,500` | closed |
| T-3-17 | high | mitigate | consumer filesystem publication: `consumer/main.c:523,572,584,634; tests/consumer/check_artifacts.c:819` | closed |
| T-3-18 | medium | mitigate | Delivery qualification: local/Linux evidence in 03-09-CHECKPOINT.md; exact final-head CI pending. | open — below high threshold (non-blocking) |
| T-3-SC | high | mitigate | Corrected current-delivery disposition: no new project dependency/install path; pins.txt, scripts/fetch-pin.sh:148 and scripts/check.sh:136,353. Verified by delta auditor. | closed |
| T-3-19 | high | mitigate | Newly registered scanner change: scripts/scan-binaries.sh:128 and tests/hygiene/scan_control.sh:187. Verified by delta auditor. | closed |

## Accepted Risks Log

No accepted risks. Nine repeated T-3-SC plan rows originally said `accept`, without an owner acceptance record. They now say `mitigate` and describe verified controls for the current delivery only. Prior-phase plan-authored acceptance is not treated as owner acceptance for this phase. Any changed dependency/install scope requires reassessment.

## Coverage and Qualification Limits

The delta audit verifies test-only barriers before manifest staging and before publication, with independent staged-file checks, exact-child SIGKILL/reaping, final-path absence and sibling preservation. The focused consumer tests pass and a premature-publication mutant fails its intended assertion, as recorded in 03-REVIEW-FIX.md and the validation audit. No test was executed by the security auditor. Green software checks do not establish hardware accuracy or grant rights, firmware, physical-testing, licence or release authority.

## Security Audit Trail

| Audit | Closed | Open | Evidence |
|---|---|---|---|
| Initial source audit | 17/19 | T-3-SC blocking; T-3-18 non-blocking | Typed security auditor, all nine plans and eight summaries plus plan 09 checkpoint. |
| Root disposition correction | pending delta audit | T-3-SC and newly registered T-3-19 blocking pending verification | No new risk accepted; scanner warning mapped explicitly. |
| Delta source audit | 19/20 | T-3-18 non-blocking | T-3-SC verified against all nine corrected rows and pinned-toolchain controls; T-3-19 verified against fail-closed iconv/byte comparison and invalid/long-valid UTF-8 controls; T-3-17 interruption gap resolved. No unmapped flag remains. |

## Sign-Off

- [x] Every threat has a supported disposition; no accepted risk inferred.
- [x] No high or critical threat remains open (`threats_open: 0`).
- [x] Scanner and test-only interruption surfaces are explicitly mapped.
- [ ] Final delivery evidence closes T-3-18 after required platform checks and exact-head CI.

Security implementation audit verified; overall phase completion remains pending delivery qualification.
