---
phase: "04"
slug: timers-interrupts-and-first-public-suites
status: draft
threats_open: 1
asvs_level: 1
created: "2026-11-11"
---

# Phase 4 — Security

Initial whole-phase authored-register audit, baseline `7b848d0` through reviewed
`db7149e`, including Plan 12 Task 1. Typed security auditor returned
`OPEN_THREATS`: 29 closed and six open of 35 unique authored threats. Five
open high threats block the configured high threshold; the medium inventory
finding is also scheduled for repair. Final qualification has not run.

## Trust Boundaries

| Boundary | Description | Data crossing |
| --- | --- | --- |
| Host inputs / evidence harness | Pinned sources, labels, JSON, oracle and captured records are untrusted until validated | Bounded text, hashes, addresses and result bytes |
| Harness / filesystem | Generated evidence belongs only in validated owned scratch | Output paths, temporary files and cleanup |
| Guest / deterministic core | Guest writes cannot bypass unsupported routes or mutate refused state | Timer registers, CPU requests and snapshots |
| Software observations / qualification | Repeatability and source comparisons cannot imply hardware accuracy | Claims, complete comparison rows and pending disposition |

## Threat Register

All authored dispositions are **mitigate**. References are repository-relative;
line locations in the initial audit describe the reviewed source and may move
when fixes land.

| Threat ID | Category | Component | Severity | Disposition | Mitigation / audit evidence | Status |
| --- | --- | --- | --- | --- | --- | --- |
| T-04-01 | Tampering | Source build | high | mitigate | `scripts/build-suites.sh`: pin/tool checks and unchanged upstream inputs before/after assembly; object gap tracked separately below | closed |
| T-04-02 | Elevation of privilege | Output paths | high | mitigate | Canonical strict scratch containment and path-bound owner markers precede C writes/cleanup; unique CMake run dirs avoid recursive deletion; exclusive temporary files/rename publish regular outputs. Root adds unlink-only cleanup, invalid-profile preflight and seeded old-remove proof; outside-target controls pass | closed |
| T-04-03 | Information disclosure | Rights/provenance | medium | mitigate | Complete upstream NOTICE, copied generated notices, provenance publication boundaries, hygiene checks; refresh exposure list for newly read main.c files | closed |
| T-04-04 | Denial of service | Scheduler | high | mitigate | `core/machine.c` subtraction-safe bounds, finite candidates, idle jump; `core/timer.c` bounded source scan and checked addition | closed |
| T-04-05 | Tampering | Guest routes | high | mitigate | Complete route preflight before unsupported access; pure mapped inspection range preflight | closed |
| T-04-06 | Tampering | Timer ownership | medium | mitigate | Instance-owned bank; init/reset/load initialization; two-instance API assertions | closed |
| T-04-07 | Tampering | DONE/reset-DONE | high | mitigate | Documented transition table, independent guest assertions and named semantic mutants | closed |
| T-04-08 | Denial of service | Links | high | mitigate | Increasing-index finite graph, bounded propagation, unsupported link enable refusal | closed |
| T-04-09 | Tampering | Refusal | medium | mitigate | CTLA checks before mutation; failed CPU action uncompleted; prefix/request/retry API controls | closed |
| T-04-10 | Tampering | IRQ | high | mitigate | Pending-based line, selected ack/force, both-profile guest IRQ cases | closed |
| T-04-11 | Tampering | Event order | high | mitigate | Timer-before-CPU comparator/drain, post-completion watch, independent expected prefixes | closed |
| T-04-12 | Denial of service | Time limits | medium | mitigate | Time headroom and fixed ordinals; exclusive-target/idle/limit assertions, no dynamic sequence counter | closed |
| T-04-13 | Tampering | Hold request | high | mitigate | Held CPU excluded, byte-identical posted request and unchanged RAM; release grants once | closed |
| T-04-14 | Denial of service | Hold progress | high | mitigate | Timers stay eligible during hold; idle jump; hold/wake/boundary cases | closed |
| T-04-15 | Tampering | Private hold | medium | mitigate | FD91 remains refused; private seam only; guest refusal/retry controls; no public halt/debug API | closed |
| T-04-16 | Tampering | Snapshot restore | high | mitigate | Full identity/hash/CPU/timer/time validation before apply; rehashed hostile inputs preserve instance and watch | closed |
| T-04-17 | Denial of service | Snapshot bounds | high | mitigate | Reachable residual limits, safe time headroom, legal links/status/cursor and hostile controls | closed |
| T-04-18 | Information disclosure | Snapshot observers | medium | mitigate | Explicit guest-only serialization; observer clear only after successful apply; refusal preservation controls | closed |
| T-04-19 | Tampering | Text parser | high | mitigate | Bounded allocation reserves limit+1 and appends NUL; embedded NUL/oversize still refuse. Metadata uses actual destination capacities and validates exact hex lengths before copying. Sanitized exact/zero/over-limit and long-revision controls pass | closed |
| T-04-20 | Elevation of privilege | Symbol extents | high | mitigate | Every control byte excludes code/results/other controls; linked payload, BSS, zero-page and stack limits are explicit. All code/result/pairwise aliases reject in symbol_controls | closed |
| T-04-21 | Spoofing | Completion | high | mitigate | Guest post-return sentinel and probes; shared completion validation and bounded actual watches | closed |
| T-04-22 | Spoofing | CPU model | high | mitigate | Executed probe, independent source tuple, selection checks, actual opposite-profile controls | closed |
| T-04-23 | Tampering | CPU results | high | mitigate | Exact 18-row oracle, complete count/progress/time/sentinel checks and actual completion faults | closed |
| T-04-24 | Spoofing | Timer model | high | mitigate | Independent executed RMB0 probe and actual opposite-selection fault | closed |
| T-04-25 | Tampering | Timer results | high | mitigate | 16 meaningful /18 reserved, exact alternatives and adjacent rejection controls | closed |
| T-04-26 | Spoofing | No execution | high | mitigate | Hold before first instruction, CPU/RAM invariance, ordinary validator rejects absent execution; real sentinel/time faults | closed |
| T-04-27 | Tampering | Evidence identity | high | mitigate | All run/captured/proof paths check pinned source revision/archive and actual source/manifest hashes; CPU and timer BLL/labels match build-record identities before cleanup. Self-consistent source edits and artifact edits reject; seeded outputs preserved | closed |
| T-04-28 | Repudiation | Subcase inventory | medium | mitigate | Exact ordered unique manifests enforce 49 result and 61 pending case names; missing/duplicate self-controls pass. Six added identity/output cases explain 55→61; historical 48→49 is Plan 10 copied-profile control | closed |
| T-04-29 | Tampering | Mutant isolation | high | mitigate | Private copied sources, unique anchors/directories, separate builds, production objects unchanged | closed |
| T-04-30 | Spoofing | Mutant detection | high | mitigate | Successful configure/build, exact intended assertion/status, timeout/sanitizer excluded, clean API brackets | closed |
| T-04-31 | Denial of service | Mutant cost | medium | mitigate | Targeted source list, finite build/runtime limits, CTest timeout and measured costs | closed |
| T-04-32 | Tampering | Final qualification | high | mitigate | Planned Plan12 Task2: exact final reviewed source and complete inventory on macOS, self-contained Ubuntu and exact-head CI; not an implementation defect | open |
| T-04-33 | Information disclosure | Privacy | high | mitigate | Canonical check invokes tree/history hygiene; committed evidence uses normalized paths | closed |
| T-04-34 | Spoofing | Claims | medium | mitigate | Runtime claim fields, strict mismatch versus observation distinction, explicit software limitations and qualification ledger | closed |
| T-04-SC | Tampering | Supply chain | high | mitigate | Archive verification before extraction; pristine/tool/source/stamp and pin-list checks | closed |

## Accepted Risks Log

No accepted risks. D-10 approves one explicitly pending source comparison;
it does not waive parser, filesystem, identity or qualification controls.

## Reproduction Evidence

The auditor used private temporary copies of existing inputs, without editing
project implementation or running broad qualification. The oracle symlink
escape exited successfully and overwrote an outside-scratch fixture. An
exact-limit non-NUL missing-token text triggered an ASan heap-buffer-overflow.
The symbol alias and altered captured CPU oracle revision were both accepted.
Private raw logs stay outside the repository; final fixes must add attributable
negative controls and retain the unchanged positive observations.

## Security Audit Trail

| Audit date | Threats total | Closed | Open | Run by |
| --- | --- | --- | --- | --- |
| 2026-11-11 | 35 | 29 | 6 (five high, one medium) | Typed gsd-security-auditor, whole-phase source db7149e |

## Sign-Off

- [x] All authored threats have dispositions.
- [x] No risk acceptance inferred.
- [x] Four implementation boundaries repaired and independently rechecked locally.
- [x] Exact subcase inventory hardened and negative controls passed.
- [ ] Final qualification completed.
- [ ] `threats_open: 0` confirmed and status verified.

O1/profile qualification, rights/licence/release and physical/firmware gates
remain unresolved. Global EVD-02 remains open for Phase5 and recurring EVD-05
for Phases4–6. No hardware or suite-pass claim is authorized.

**Approval:** implementation controls repaired; final qualification remains pending.

## Independent Delta Recheck

Root reviewed `272dd2b..27145c8`, including the source changes rather than only
fix-report claims. The delegated follow-up audit failed at automated content
screening before a review ran; root performed this read-only delta review
locally. This is not a second successful typed-auditor verdict.

The unchanged 29 closed threats retain their initial evidence; core timer,
request and snapshot implementations are unchanged by these fixes. All five
implementation/inventory findings above are now closed. T-04-32 alone remains
open: Plan 12 Task 2 must still qualify the final reviewed source on all three
required environments. There is no risk acceptance and no final SECURED claim.

For the code changes committed as `27145c8`, the sanitized suite target built successfully. Root's six-test
selection passed oracle/symbol controls, both timer profile observations,
strict-comparison controls and 61-case pending controls (60.72s wall). The two
CPU/tracer acceptance tests passed (1.40s wall). An isolated same-source
sanitized control replaced only unlink with the prior remove operation:
fixed code refused the directory output and preserved it; prior behavior
returned success and removed that directory. No production source was mutated
for the control. The oracle exact-limit/missing-token controls exercise the
same ASan-enabled text reader that previously overread.

The fixer separately recorded49-case result controls and CPU identity controls
passing; no test result was inferred from compilation alone. Existing source
and object checks precede cleanup, while proof/companion writes share the
owned atomic writer. Marker/path validation rejects canonical-ancestor
escapes; unlinking an owned symlink removes the entry without touching its
target. Active replacement by a party with write access to the same owned
scratch directory is not represented as an authenticated multiuser boundary.

| Audit date | Threats total | Closed | Open | Run by |
| --- | --- | --- | --- | --- |
| 2026-11-11 |35|34|1 high, planned final qualification|Root independent delta review, source `27145c8`|
