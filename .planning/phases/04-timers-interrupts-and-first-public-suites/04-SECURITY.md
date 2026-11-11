---
phase: "04"
slug: timers-interrupts-and-first-public-suites
status: draft
threats_open: 5
asvs_level: 1
created: "2026-11-11"
---

# Phase 4 — Security

Whole-phase authored-register audit, baseline `7b848d0` through reviewed
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
| T-04-02 | Elevation of privilege | Output paths | high | mitigate | Predictable `OUT.$$` oracle temporary follows planted symlink; harness cleanup lacks canonical ancestor containment; CMake cleanup uses lexical prefix | open |
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
| T-04-19 | Tampering | Text parser | high | mitigate | `read_text` omits NUL used by later strstr; exact-limit missing-token input reproduced ASan overread. Metadata revision[41] is incorrectly copied with capacity65 | open |
| T-04-20 | Elevation of privilege | Symbol extents | high | mitigate | Completion/progress/model/probe versus code overlap is unchecked; actual copied labels with done=entry accepted by validate-symbols | open |
| T-04-21 | Spoofing | Completion | high | mitigate | Guest post-return sentinel and probes; shared completion validation and bounded actual watches | closed |
| T-04-22 | Spoofing | CPU model | high | mitigate | Executed probe, independent source tuple, selection checks, actual opposite-profile controls | closed |
| T-04-23 | Tampering | CPU results | high | mitigate | Exact 18-row oracle, complete count/progress/time/sentinel checks and actual completion faults | closed |
| T-04-24 | Spoofing | Timer model | high | mitigate | Independent executed RMB0 probe and actual opposite-selection fault | closed |
| T-04-25 | Tampering | Timer results | high | mitigate | 16 meaningful /18 reserved, exact alternatives and adjacent rejection controls | closed |
| T-04-26 | Spoofing | No execution | high | mitigate | Hold before first instruction, CPU/RAM invariance, ordinary validator rejects absent execution; real sentinel/time faults | closed |
| T-04-27 | Tampering | Evidence identity | high | mitigate | Captured CPU validator omits build identity/object binding; edited oracle revision plus refreshed record hash accepted. Strict/no-run timers omit build object hash comparison | open |
| T-04-28 | Repudiation | Subcase inventory | medium | mitigate | Timer batch guard checks only nonzero count. Enforce unique exact names and missing/duplicate self-controls; current 49 result and55 pending cases are present | open — below high threshold |
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
- [ ] Four implementation boundaries repaired and re-audited.
- [ ] Exact subcase inventory hardened.
- [ ] Final qualification completed.
- [ ] `threats_open: 0` confirmed and status verified.

O1/profile qualification, rights/licence/release and physical/firmware gates
remain unresolved. Global EVD-02 remains open for Phase5 and recurring EVD-05
for Phases4–6. No hardware or suite-pass claim is authorized.

**Approval:** pending implementation fixes and final qualification.
