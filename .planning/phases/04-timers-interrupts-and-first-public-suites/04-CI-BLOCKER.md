# Phase 4 — external CI state blocker

Phase 4 remains in progress at Plan 12 Task 2. Implementation, reviews, fixes,
the independent D-10 gate, and full local qualification are complete. The
required hosted CI result is unavailable. No phase completion is claimed.

## Qualified and published source

- Product/scripts qualified on macOS and self-contained Ubuntu 24.04 arm64:
  `7616f6d1d12b98eac6cf4d4e979ac1c90f0e01ac`.
- Both canonical `sh scripts/check.sh` runs passed all 163 tests, exact JUnit,
  no skips, artifacts and history hygiene. Totals: macOS 112s, Ubuntu 75s.
- Final qualification ledger/lesson commit: `adc5247` (docs only).
- Reviewed pushed CI head: `f92084423d16e4d4d3420545d1f5131b47c7e461`
  (docs only after the tested implementation; existing origin/main channel).
- [Exact-head run 43096967098](https://github.com/szTheory/lynxplosion/actions/runs/43096967098),
  attempt 1, created 2026-11-11T20:35:41Z, remains queued with zero jobs and no
  conclusion. Fresh no-cache run/jobs APIs still show the creation timestamp
  as updated_at after both recovery attempts below.

See 04-QUALIFICATION.md for source identities, full outcomes and measured
failure/repair evidence. Current descriptor SHA-256:
`676b711e3e6cd4258fafb9191473cc531b8daee659dc10ee3a0767429764dacd`.

## External evidence and bounded recovery

GitHub declared critical multi-product incident `dvy0t3mvh1lg`; its exact
API-provided [incident shortlink](https://stspg.io/jdjlq5zjyl48) is the recorded
source. The outage temporally overlaps the queue; it is not proven to be its
sole cause.

| UTC time, 2026-11-11 | Observation |
| --- | --- |
|20:48:12|Incident created; Actions degradation update followed at 20:48:25|
|20:56:49|Multi-product investigation update|
|21:07:24–26|Mitigation/monitoring and Actions backlog-processing updates|
|21:21:32|GitHub reported the backlog fully processed and full service recovery|
|21:21:39|Incident resolved|
|After recovery|Fresh run/jobs responses still queued, attempt 1, zero jobs; public page has no scheduling or approval reason, pending deployments empty|

After the explicit backlog-drained update, root authorized one targeted
recovery, with no source changes or new push. The standard cancellation
command refused with `Cannot cancel a workflow run that is completed`.
Bounded fresh status checks still showed queued/zero jobs. Root then tried
the standard same-run rerun once; it refused with
`This workflow is already running`. Another fresh status/jobs check still
showed queued/zero jobs. No cancellation or rerun is confirmed; attempt remains 1.
No force cancellation, settings/billing/runner change, repeated retry or
substitute CI run was used. The GitHub service state is inconsistent.

The workflow skill's ci_monitor.cjs helper is absent from the installed skill
and project; the installed CLI was used with explicit before/after API checks.
That missing helper is not the blocker. No approval-review rejection occurred
for these CI operations; their refusals came from GitHub's run-state checks.

## Next actor and resume procedure

GitHub must first expose a usable, consistent hosted run/result. There is no
evidence-supported repository/source fix or required owner setting change.
No user confirmation is requested merely to preserve this gate.

On resume (`$gsd-autonomous --from 4`), query the existing exact-head run before
any new action. Do not repeat the failed cancellation/rerun blindly or publish
a metadata commit solely to create a substitute run. If jobs/results appear,
inspect their exact source, all test/JUnit/skip evidence and critical-path costs.
If it still has inconsistent state, preserve this blocker and report the
actual service evidence; do not weaken the required CI gate.

After CI succeeds: finish Plan 12 Task 2 ledger/04-12-SUMMARY and bookkeeping;
close T-04-32 with observed evidence, update coverage qualification, execute
the canonical verify-phase-goal lifecycle and independent verifier/covered
fingerprint, require the owning verification.status query to pass, then complete
Phase 4. Carry global EVD-02 and recurring EVD-05 open. Continue Phases 5–6 and
milestone lifecycle under the original autonomous request; O9 gates freezing
raw video form if it affects LibFuturo. Existing rights/physical/licence/release
and hardware-profile boundaries remain in force.

The exact timer index 14 observation remains 0c versus unchanged source 0d;
strict comparison still fails. This external blocker grants no source-suite
or hardware-accuracy claim. Source/control reviews remain recorded; no user
action is needed now. A later resume is required after GitHub recovers.

## Workspace preservation

The preexisting untracked `.planning/state.json` is not staged or deleted.
Raw logs and the reversible retained-scratch evidence stay in private temporary
storage. Source is already pushed; this blocker handoff may be a local
documentation-only commit ahead of origin/main. Do not confuse it with the
existing exact-head CI source or rerun local gates for unchanged documentation.

## Owner-requested recovery resume

The owner subsequently reported the outage over and requested another attempt.
A fresh official status response reports all systems operational, Actions
operational, and no incidents. Existing run 43096967098 still reports queued,
attempt 1, zero jobs; a new standard same-run retry is refused as already running.

Root is resuming the original autonomous workflow. The previously committed
handoff and this recovery record will be published through the same normal
origin/main channel, creating a fresh CI event with unchanged qualified
product/script source. This supersedes the earlier hold on using a new event
while service recovery was uncertain. Preserve the old run and its inconsistent
state as historical evidence; a fresh successful run must be accounted for
separately. No empty commit, gate waiver, new distribution channel or source
change is involved.
