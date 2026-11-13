---
milestone: "1"
audited: "2026-11-13T16:14:40.451621+00:00"
status: gaps_found
scores:
  requirements: "53/54"
  phases: "6/6"
  integration: "6/6"
  flows: "6/6"
gaps:
  requirements:
    - id: EVD-02
      status: unsatisfied
      phase: "4 and 5; global v1 obligation"
      verification_status: gaps_found
      evidence: "Strict timer row14 remains legacy0C/source0D and video-policy1 0F/source0D; current controls preserve the disagreement."
      claimed_by_plans:
        - ".planning/phases/04-timers-interrupts-and-first-public-suites/04-01-PLAN.md"
        - ".planning/phases/04-timers-interrupts-and-first-public-suites/04-07-PLAN.md"
        - ".planning/phases/04-timers-interrupts-and-first-public-suites/04-08-PLAN.md"
        - ".planning/phases/04-timers-interrupts-and-first-public-suites/04-09-PLAN.md"
        - ".planning/phases/04-timers-interrupts-and-first-public-suites/04-10-PLAN.md"
        - ".planning/phases/04-timers-interrupts-and-first-public-suites/04-12-PLAN.md"
        - ".planning/phases/05-display-raw-video-and-input/05-10-PLAN.md"
        - ".planning/phases/05-display-raw-video-and-input/05-11-PLAN.md"
        - ".planning/phases/05-display-raw-video-and-input/05-12-PLAN.md"
      completed_by_plans:
  integration: []
  flows: []
nyquist:
  compliant_phases: ["01", "02", "03", "04", "05", "06"]
  partial_phases: []
  not_validated_phases: []
  missing_phases: []
  overall: compliant
tech_debt:
  - phase: "02"
    items:
      - "Full-vector transfer post-size bound, pin-record overwrite guard and temporary-directory signal cleanup remain nonblocking hardening work."
      - "Sample parser final-object leniency is hash-bound; long-checkout-path control fragility has historical evidence but no new reproduction here."
  - phase: "cross-phase"
    items:
      - "SDK milestone projection counts only fresh fingerprints; preserve historical own-source verdicts and use current regression/integration rather than blind hash refreshes."
---

# v1 milestone audit — checkpoint qualified, milestone open

All six planned software phases and69 plans are executed. The independent
Phase6 verifier passes45/45 truths, and the independent integration audit finds
all six end-to-end flows connected with no new wiring blocker. The reviewed
implementation has native/GNU/hosted canonical qualification at the exact source
recorded inPhase6 qualification. This does **not** satisfy the unchanged full
milestone definition of done: EVD-02 remains open.

The scope is the roadmap's project v1 checkpoint. The SDK has no milestone
version/name marker, so `1` here denotes that existing v1 scope; it assigns no
software release version, tag or publication authority.

## The remaining decision, in plain language

The pinned timer diagnostic produces an interrupt acknowledgment count different from its unchanged
source expectation. The harness runs and observes it correctly, but strict
agreement remains a real failure: row14 is0C under legacy flat5 and0F under
video-policy1 against source0D. Phase4's approved exception closed its
observation/control slice; the Phase5 revisit and Phase6 work did not close the
global comparison requirement. All other15 timer rows agree.

The playable A/V checkpoint is implemented and repeatable, but a complete
public-suite agreement claim is unavailable. EVD-02 stays unchecked/Pending.
The recommended next action is a focused, evidence-led timer investigation,
reusing the existing traces, source pin and controls. Do not change the oracle,
add an exception or tune to0D without source-supported behavioral evidence.

The owner approved the recommended focused investigation on2026-11-13. It found
and repaired a CPU/video/refresh grant overlap; strict counts remain12/15 versus13.
See the [focused investigation](debug/evidence/timer-focused-bus-accounting.md).
This approval does not accept the unmet comparison as a milestone-scope change.
The milestone remains open automatically; no confirmation is needed
to keep the existing hardware/rights/release stops in place. No new phase,
archive cleanup, tag or release has been started.

## Three-source requirement reconciliation

All54 v1 definitions/traceability rows were cross-checked against canonical phase
VERIFICATION requirement tables and `requirements-completed` from all69 SUMMARY
frontmatters, extracted by the installed SDK. No requirement is orphaned. Shared
consumer/fault requirements are closed only after their final Phase6 evidence;
earlier partial slice verdicts are retained as history. Summary claims alone
never close a requirement; EVD-02 remains unmet despite slice-completion entries.

| Requirement | Assigned phase | Checkbox / traceability | Latest verification | Summary claim | Audit |
|---|---|---|---|---|---|
| FND-01 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-02 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-03 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-04 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-05 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-06 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-07 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-08 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| FND-09 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-01 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-02 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-03 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-04 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-05 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-06 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| API-07 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| API-08 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| DET-01 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| DET-02 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| DET-03 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| DET-04 | Phase 1 | checked / Complete | Phase1: SATISFIED | present | satisfied |
| DET-05 | Phase 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| CPU-01 | Phase 2 | checked / Complete | Phase2: SATISFIED | present | satisfied |
| CPU-02 | Phase 2 | checked / Complete | Phase2: SATISFIED | present | satisfied |
| CPU-03 | Phase 2 | checked / Complete | Phase2: SATISFIED | present | satisfied |
| CPU-04 | Phase 2 | checked / Complete | Phase2: SATISFIED | present | satisfied |
| CPU-05 | Phase 2 | checked / Complete | Phase2: SATISFIED | present | satisfied |
| MEM-01 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| MEM-02 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| MEM-03 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| MEM-04 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| TIM-01 | Phase 4 | checked / Complete | Phase4: SATISFIED | present | satisfied |
| TIM-02 | Phase 4 | checked / Complete | Phase4: SATISFIED | present | satisfied |
| TIM-03 | Phase 4 | checked / Complete | Phase4: SATISFIED | present | satisfied |
| TIM-04 | Phase 4 | checked / Complete | Phase4: SATISFIED | present | satisfied |
| VID-01 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| VID-02 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| VID-03 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| VID-04 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| VID-05 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| INP-01 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| AUD-01 | Phase 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| AUD-02 | Phase 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| EVD-01 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| EVD-02 | Phases 4 and 5 | unchecked / Pending | Phase5: OPEN globally | missing | unsatisfied |
| EVD-03 | Phase 4 | checked / Complete | Phase4: SATISFIED | present | satisfied |
| EVD-04 | Phase 4 | checked / Complete | Phase4: SATISFIED | present | satisfied |
| EVD-05 | Phases 4, 5 and 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| EVD-06 | Phase 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| HB-01 | Phase 3 | checked / Complete | Phase3: SATISFIED | present | satisfied |
| HB-02 | Phase 5 | checked / Complete | Phase5: SATISFIED | present | satisfied |
| HB-03 | Phase 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| CON-01 | Phases 3, 5, 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |
| PERF-01 | Phase 6 | checked / Complete | Phase6: SATISFIED | present | satisfied |

## Phase, integration and coverage evidence

| Phase | Recorded goal verification | Current evidence disposition |
|---|---|---|
| 1 | [5/5 roadmap success criteria verified; 18/18 requirements satisfied](phases/01-evidence-harness-and-walking-skeleton/01-VERIFICATION.md) | Passed at its recorded source; current registered regressions and integration remain qualified. |
| 2 | [4/4 roadmap success criteria verified (5/5 requirements satisfied)](phases/02-cpu-profiles-and-per-opcode-vectors/02-VERIFICATION.md) | Passed at its recorded source; current registered regressions and integration remain qualified. |
| 3 | [46/46 must-haves verified](phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-VERIFICATION.md) | Passed at its recorded source; current registered regressions and integration remain qualified. |
| 4 | [49/49 must-haves verified](phases/04-timers-interrupts-and-first-public-suites/04-VERIFICATION.md) | Passed at its recorded source; current registered regressions and integration remain qualified. |
| 5 | [44/44 must-haves verified](phases/05-display-raw-video-and-input/05-VERIFICATION.md) | Passed at its recorded source; current registered regressions and integration remain qualified. |
| 6 | [45/45 must-haves verified](phases/06-audio-and-playable-checkpoint/06-VERIFICATION.md) | Passed at its recorded source; current registered regressions and integration remain qualified. |

[Independent integration report](v1-INTEGRATION.md) traces the loader/CPU/bus,
logical input/A/V streams, format7 and in-memory checkpoint, independent bundle
validation/publication, pinned suites/inventory, and separated benchmark timing.
It maps every v1 requirement, with0 broken connections or orphaned audited C APIs.
No HTTP, frontend, live audio, physical device or persisted checkpoint-file
interface is required by this milestone.

All six coverage records use the current validated/compliant schema. Phase4's
legacy `complete` marker was normalized from its already-recorded final25-task
qualification, preserving its historical results and explicitly open global
comparison. Current native/GNU/hosted inventories each account for246 rows:
244 pass, exactly two declared page-mode skips, zero failures. Those two guests
stop at unsupported06→FD8C before results; EVD-06 explicitly permits a visible
blocker. They are missing observations, not suite or hardware passes.

Earlier phase fingerprints became stale as later shared core contracts changed;
SDK milestone-op therefore projects only one fresh phase. That is recorded as a
tooling projection limitation, not six missing implementations. Historical
verdicts/fingerprints are not blindly rewritten. The current Phase6 fingerprint
is fresh, and exact-source canonical regression plus independent integration
provide the current behavior evidence.

## Efficiency and metadata repairs applied

- Reused current research, warm executors and the existing self-contained Linux
  clone; disabled needless pattern mapping and stopped extra process-audit fanout.
- Kept the first complete runnable path ahead of required fault expansion,
  batched repairs and reused current qualification for regression/report prose.
- Removed repeated sensitivity interleaving while preserving complete logical
  inputs, profiles/handedness and every independent frame assertion. The older
  GNU case passed234.34s and timed out240.01s; the revised case passed138.69s
  inside final GNU qualification and12.97s in hosted source CI. These are observed
  harness costs, not controlled core speedups; limits were unchanged.
- Added per-environment artifact preflight after one avoidable363.46s test run
  reached old generated outputs. Only verified owned evidence was moved with
  complete matching inventories; no scanner exemption or unrelated cleanup.
- Repaired four YAML regex-reference quoting errors with exact literal command
  preservation. All69 summaries now parse. Added SDK summary extraction before
  future guarded commits; current Phase6 fingerprint was independently refreshed.
- Kept requirement Status cells machine-readable and details in definition notes;
  inspected per-ID write sets instead of treating a zero tool exit as full success.

Detailed costs, failures and uncertainty remain inPhase6
[qualification](phases/06-audio-and-playable-checkpoint/06-QUALIFICATION.md),
[efficiency review](phases/06-audio-and-playable-checkpoint/06-EFFICIENCY-REVIEW.md),
[lessons](LESSONS.md) and [autonomy defaults](AUTONOMY.md). No new cache,
concurrency setting, dependency or CI matrix was justified by measurement.

## Repository followups

There are no open PRs to triage. The previously recommended Actions full-SHA
policy is enabled. An active main required-check ruleset now binds `check` to
the observed GitHub Actions publisher, with a narrowly scoped audited owner-only
bypass preserving the explicit checked-main workflow. It does not require a new
PR workflow or waive local review and exact-source CI. See[repository policy](REPO-POLICY.md).

## Nonblocking debt and authority

The integration report triages historical NMI/reset-seam, parser, vector
resource/pin-overwrite, cleanup and long-path advisories. The bus timestamp is
now consumed. None exposed a new missing milestone connection; unresolved
hardening remains explicit rather than expanded into unplanned source work.

O1/O5/O6/O9, rights-bearing content, physical qualification, release/new-channel
and shared LibFuturo boundaries remain open. Software repeatability is not
hardware accuracy. This audit accepts no new risk, comparison exception or
milestone gap on the owner's behalf.
