# Phase 6 Plan Check

## VERIFICATION PASSED

**Phase:** Audio and Playable Checkpoint  
**Plans checked:** 12  
**Status:** All plan checks passed; runtime evidence is pending execution.

### Goal and requirement coverage

The roadmap goal is a public API host replaying an original homebrew with input-responsive audio and video on one guest-tick timeline, producing reproducible A/V checkpoint evidence, the exact v1 suite inventory, and a throughput baseline without firmware. Each of the five roadmap success criteria has concrete planned tasks.

| Requirement | Plans | Coverage |
|---|---|---|
| AUD-01 | 03, 04, 09, 10, 12 | Four-channel behavior, eight stereo routes, source-suite observations, and semantic faults |
| AUD-02 | 01, 03–06, 10, 12 | Integer PCM/events, exact tick lattice, refusal/capacity behavior, snapshots, and core-purity checks |
| HB-03 | 01, 07, 08, 11, 12 | Original input-responsive A/V replay, safe publication, and benchmark workload |
| DET-05 | 01, 04–08, 10–12 | Separate guest-tick streams, full continuation comparisons, and repeatability labeling |
| EVD-06 | 02, 09, 12 | Pinned source builds, complete 18-row oracles, fresh observations, and exact five-suite inventory |
| PERF-01 | 11, 12 | Five equal-work, equal-output fresh runs with median/range and separate host/CI timing |
| CON-01 final real A/V consumer | 01, 05, 07, 08, 12 | Public-only consumer, real PCM/WAV and frames, validation, refusal, and atomic bundle publication |
| EVD-05 audio-fault closure | 10, 12 | Ten private semantic mutants with exact assertions and clean brackets |

### Plan structure and execution order

All 24 tasks have files, actions, automated verification command blocks, stated failing directions, and measurable done criteria. The declared graph is acyclic and wave-consistent: 06-01 and 06-02 run in Wave 1 with disjoint source-file ownership; 06-03 depends on both; the remaining plans execute serially through Wave 11. The Wave 1 source-suite builder uses separate scratch and does not share core build or registration files.

The approved 06-01 tracer/immediate-integration exception lists its touched paths and repairs snapshot readers and copied-core source lists alongside the first format change. Its estimate is 48,000 tokens against a 100,000-token smart-zone budget (48%, low confidence, zero completed-phase actuals); the other plan estimates are 20,000–31,000 tokens (20–31%, also low confidence). No estimate is over budget. The exception is not treated as a scope defect.

The plan-structure tool emitted a final-plan pipeline warning. Manual review shows the `|` in the GitHub Actions query is inside the single-quoted `--jq` expression, not a shell pipeline; the command does not mask an upstream `git` failure.

### Decision, architecture, and boundary checks

Plans honor the locked 40,000/1 Hz stereo S16 policy, 400-tick sample spacing, bounded caller-owned spans, separate event/audio/video/progress streams, immutable identity, fieldwise snapshot state, and host-owned replay cursor. They preserve the default `none` profile, `LEGACY_FLAT5`, opt-in `VIDEO_POLICY1`, strict timer disagreements, and only the existing two page-mode SERCTL skips. The source suites remain unchanged, use project wrappers and MIT provenance, and do not run their CRT mains. No task claims hardware accuracy or chooses an O1 profile.

The responsibility map is respected: deterministic behavior stays in the core/API, file parsing and publication stay in the public consumer, and evidence stays in test/build harnesses. The plan set includes every listed threat ID T-06-01 through T-06-24; the reserved T-06-SC entry is repeated as expected. The six classified edge predicates have explicit checks. Five unclassified edge assumptions and three descriptorless prohibitions remain explicitly surfaced for Plan 06-12's manual evidence-backed review; they are not treated as automatic passes. Deferred work—resampling, live presentation, UART/page-mode unblock, physical calibration, shared LibFuturo changes, optimization, and release—is excluded.

### Verification probes

The supplied failing-direction probe reports `ok` with no severity for all 26 automated commands. The supplied verify-path probe reports `not_applicable` for all 26 commands because their shell forms are unsupported; this is not evidence that command targets resolve, and no path-resolution pass is claimed. No builds, tests, containers, package operations, hardware checks, or runtime evidence were performed as part of this plan review.
