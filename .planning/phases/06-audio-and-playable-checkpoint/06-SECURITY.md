---
phase: "06"
slug: audio-and-playable-checkpoint
status: draft
threats_open: 1
asvs_level: 1
created: "2026-11-13"
source_head: e7bce5e8a830d9b13b10baa6af7dac54c52c7004
---

# Phase 6 — Security

Declared ASVS1 mitigations were independently inspected on frozen source, then the two demonstrated code-review defects and lifecycle coverage repair were independently rechecked. This is scoped repository/build evidence, not physical qualification or a forensic assertion about every historical command. No risk acceptance was used.

## Trust Boundaries

Caller/fixture/report inputs cross into bounded deterministic machine state or host-owned evidence. Caller-owned spans and guest state remain distinct from host clocks/files/publication. Private mutation builds remain separate from production.

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation evidence | Status |
|---|---|---|---|---|---|---|
| T-06-01 | Tampering / Denial of service | core/audio.c | high | mitigate | Original public guest response and independent bundle controls; consumer/main.c; fixtures/diagnostic/av_probe.s; tests/consumer/check_artifacts.c. | CLOSED |
| T-06-02 | Tampering / Denial of service | tests/api/test_api.c | high | mitigate | Format7 ABI and raw-reader assertions plus copied-source lists; core/internal.h; tests/header/header.c; API/CPU/diag readers and timer/video fault drivers. | CLOSED |
| T-06-SC | Tampering | Package/tool provenance | high | mitigate | Pinned/no-new-dependency provenance retained; static fixture symlink bypass repaired before validator reads with canonical containment and seeded controls. No arbitrary concurrent filesystem replacement guarantee is claimed. | CLOSED |
| T-06-03 | Tampering / Denial of service | fixtures/suites/audio-wrapper.s | high | mitigate | Pinned source/tool hashes, recipes and notices; scripts/build-suites.sh and audio manifests. | CLOSED |
| T-06-04 | Tampering / Denial of service | scripts/generate-suite-oracle.sh | high | mitigate | Exact source-only 18-row oracle and30/34 alternatives; generate-suite-oracle.sh and audio-script-controls.sh. | CLOSED |
| T-06-05 | Tampering / Denial of service | core/audio.c | high | mitigate | Signed channel/register/LFSR/integrator assertions; core/audio.c and tests/audio/test_audio.c. | CLOSED |
| T-06-06 | Tampering / Denial of service | core/audio.c | high | mitigate | Independent eight routing bits, bounded mixing and extrema; core/audio.c and authored stereo/mix cases. | CLOSED |
| T-06-07 | Tampering / Denial of service | core/timer.c | high | mitigate | Eight-visit worklist, checked68-bit debt and deferred continuation; core/timer.c; audio.links/ring_zero and det.audio_paths. | CLOSED |
| T-06-08 | Tampering / Denial of service | core/machine.c | high | mitigate | Post-success CPU grant watermark in core/machine.c:817; audio.order guest enable400→firstclock416, refusal/watch/strobe/restore controls. | CLOSED |
| T-06-09 | Tampering / Denial of service | include/lynxplosion.h | high | mitigate | Checked spans/overlap/refusal plus active lifecycle old-span checks; core/machine.c and audio.span_refusal/span_alias/stops. | CLOSED |
| T-06-10 | Tampering / Denial of service | include/lynxplosion.h | high | mitigate | Complete group frontier/resume and nondefault reset/load/deinit/reinit tests; audio.capacity/order/stops. | CLOSED |
| T-06-11 | Tampering / Denial of service | core/snapshot.c | high | mitigate | Fieldwise candidate validation before apply and malformed-state preservation; core/snapshot.c and audio.snapshot_refusal; new ordering restore continuation. | CLOSED |
| T-06-12 | Tampering / Denial of service | tests/det/test_det.c | high | mitigate | Complete stream/path/sensitivity comparisons including empty/single suffixes; tests/det/test_det.c. | CLOSED |
| T-06-13 | Tampering / Denial of service | consumer/main.c | high | mitigate | Canonical input and in-memory checkpoint identity/prefix before restore; consumer/main.c and av_edges/av_restore. | CLOSED |
| T-06-14 | Tampering / Denial of service | tests/consumer/check_artifacts.c | high | mitigate | Independent complete bundle semantics and named stale/silent/record mutations; check_artifacts.c and av_response_controls. | CLOSED |
| T-06-15 | Tampering / Denial of service | consumer/main.c | high | mitigate | Bounded canonical input, aliases/links/counts and unchanged sentinels; consumer/main.c and av_controls. | CLOSED |
| T-06-16 | Tampering / Denial of service | consumer/main.c | high | mitigate | Marker+inode staging and exclusive whole-directory publication; actual race/write/close/abort/foreign-stage controls. | CLOSED |
| T-06-17 | Tampering | tests/suites/test_public_suites.c | high | mitigate | Four fresh audio/audio2 source observations, independent model/progress/return/sentinel and strict18rows; suite harness. | CLOSED |
| T-06-18 | Tampering | tests/suites/test_public_suites.c | high | mitigate | Exact five-suite inventory, captured-record controls and two exact SERCTL skips; strict timer disagreements unchanged. | CLOSED |
| T-06-19 | Tampering | tests/audio/fault_controls.cmake | high | mitigate | Six successful private mutant builds, exact assertions and clean brackets; tests/audio/fault_controls.cmake. | CLOSED |
| T-06-20 | Tampering | tests/audio/fault_controls.cmake | high | mitigate | Both stereo-side, cadence399 and accepted-restored-ring-cursor suffix mutants; private copied sources only. | CLOSED |
| T-06-21 | Tampering | consumer/main.c | high | mitigate | Host-only CLOCK_MONOTONIC around public advance calls; five equal full13stream bundles; consumer.av_benchmark. | CLOSED |
| T-06-22 | Tampering | tests/consumer/av_driver.cmake | high | mitigate | Exactly five unique reports;28 named parser refusals and real no-run; separate timing categories in docs/performance.md. | CLOSED |
| T-06-23 | Tampering | .planning/phases/06-audio-and-playable-checkpoint/06-REVIEW.md | high | mitigate | Joined independent standard53-file review is clean after3checked repairs; coverage follow-up closes all3seams; focused native/GNU14-case batches and shell checks pass. | CLOSED |
| T-06-24 | Tampering | .planning/phases/06-audio-and-playable-checkpoint/06-QUALIFICATION.md | high | mitigate | PENDING: native/Linux final canonical gates, exact pushed HEAD hosted CI and critical-path inspection have not run. | OPEN |

## Accepted Risks Log

No accepted security risks. The inherited timer source disagreement, SERCTL blocker and open owner decisions remain explicit qualification limits, not newly accepted software/security defects.

## Prohibition Judgments

1. **Hardware/O1 claims:** verified within reviewed source and authored evidence. Records retain unverified hardware accuracy and unresolved model qualification. Repeatability/source agreement/fault sensitivity do not establish physical accuracy.
2. **Assets and clean-room boundary:** verified for the reviewed phase delta and recorded provenance. Inputs are project-authored or pinned notice-bearing diagnostics. The fixture coverage symlink bypass is now repaired and controlled. This does not certify all historical browser exposure; O5 and draft O6 remain open.
3. **Authority:** verified within the reviewed changes. No physical-test, release, new-channel or shared LibFuturo integration authority was added. Existing public-repository/checked-main authority is separate. O1/O5/O6/O9 and existing stops remain in force.

## Audit Trail

- Initial ASVS1 inspection found23declared mitigations present and2final-evidence obligations open. Code review subsequently demonstrated an ordering defect and a fixture symlink coverage bypass; those affected initial conclusions were superseded, not treated as exhaustive assurance.
- Repairs d40db8d,0daef49,e7bce5e have old-failure/fixed-pass evidence or explicitly labeled lifecycle coverage evidence. Native and GNU13.3 sanitizer-focused batches passed14cases; ShellCheck0.11/0.9 and POSIX syntax checks passed.
- Independent follow-ups confirm both repairs, lifecycle coverage, unchanged source identities and no new code defects. T-06-23 is closed by the joined review/fix/disposition evidence. T-06-24 remains OPEN until actual final gates and exact-HEAD CI evidence exist.

## Sign-Off

Implementation and review mitigations are verified at the repaired source. Final security/phase sign-off is pending T-06-24; `threats_open: 1` is intentional. Parent performs final qualification next; no user decision or risk acceptance is required.
