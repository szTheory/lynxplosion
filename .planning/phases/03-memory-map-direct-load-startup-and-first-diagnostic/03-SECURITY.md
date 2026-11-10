---
phase: "03"
slug: memory-map-direct-load-startup-and-first-diagnostic
status: in-progress
threats_open: 2
asvs_level: 1
block_on: high
created: "2026-11-10"
---

# Phase 3 — Security

The initial auditor verified 17 of 19 unique planned threats from source and control presence (ASVS L1). Exact-head CI remains pending. Root corrected the unsupported template acceptance disposition and registered the scanner surface; those two rows await the delta audit. No owner risk acceptance is inferred.

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
| T-3-SC | high | mitigate | Corrected current-delivery disposition: no new project dependency/install path; pins.txt, scripts/fetch-pin.sh:148 and scripts/check.sh:136,353. Delta audit pending. | open |
| T-3-19 | high | mitigate | Newly registered scanner change: scripts/scan-binaries.sh:128 and tests/hygiene/scan_control.sh:187. Delta audit pending. | open |

## Accepted Risks Log

No accepted risks. Nine repeated T-3-SC plan rows originally said `accept`, without an owner acceptance record. They now say `mitigate` and describe verified controls for the current delivery only. Prior-phase plan-authored acceptance is not treated as owner acceptance for this phase. Any changed dependency/install scope requires reassessment.

## Coverage and Qualification Limits

The initial L1 source audit closes T-3-17's implementation mitigation, but does not establish the missing deterministic interruption experiment. The coverage audit records that gap in 03-VALIDATION.md; a focused fix remains required. No test was executed by the security auditor. Green software checks do not establish hardware accuracy or grant rights, firmware, physical-testing, licence or release authority.

## Security Audit Trail

| Audit | Closed | Open | Evidence |
|---|---|---|---|
| Initial source audit | 17/19 | T-3-SC blocking; T-3-18 non-blocking | Typed security auditor, all nine plans and eight summaries plus plan 09 checkpoint. |
| Root disposition correction | pending delta audit | T-3-SC and newly registered T-3-19 blocking pending verification | No new risk accepted; scanner warning mapped explicitly. |

## Sign-Off

Pending disposition/scanner delta audit, interruption control and final delivery evidence.
