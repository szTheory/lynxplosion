# Phase6 closeout

All12 plans are executed. Independent goal verification passes45/45 truths,
all eight phase requirements and the report fingerprint/status check. Final
native/GNU/hosted software qualification is at the source recorded in
06-QUALIFICATION.md. No human-only software UAT remains.

Global EVD-02 is still open: the strict timer source comparison disagrees at
row14 under both documented timing policies. The only skips are the two
page-mode SERCTL observations. Physical/model, rights, release/new-channel and
shared LibFuturo stops remain unchanged. The milestone audit is next; phase
completion does not mean the milestone definition of done is met.

The phase-complete command returned reference warnings. Actual inspection
resolves them without regenerating or committing guest assets:

- `audio/tests.s` and `audio2/tests.s` are pin-relative paths under
  `build/pins/lynx-tests`, and exist there with the verified pinned source.
- `sh scripts/check.sh` and `sh scripts/check-claims.sh` are commands, not file
  names; both underlying scripts exist and passed their registered gates.
- The old generated av_probe BLL was per-test scratch. Successful owned scratch
  cleanup is intentional; the original source/config, manifest and tested build
  recipe remain, and the final replay/benchmark builds and validates it afresh.

The milestone projection currently counts only the fresh Phase6 report, while
ROADMAP/STATE record six completed phases. Earlier verification fingerprints
become stale when later shared-core code changes; their recorded own-source
verdicts remain historical evidence. The milestone audit must combine those
records with current canonical regressions and independent integration. Do not
rerun completed plans or blindly rewrite historical fingerprints to satisfy
that projection. The v1 scope comes from the project roadmap/requirements;
missing SDK milestone-version metadata is not a new software release version.

README and PROJECT now describe the completed A/V checkpoint and link the
policies, replay, snapshot, fault and performance records. Historical placeholder
snapshot mode stays clearly labelled. Execution/API usage was not measured;
recorded handoff elapsed time is kept separate from actual build/test/CI costs.


Milestone audit and independent integration subsequently completed:53/54
requirements,6/6 flows, no new broken connection. EVD-02 remains the explicit
unmet comparison; the milestone stays open for an owner scope decision. All69
summaries now parse after four literal-preserving quoting repairs. Phase6's
current fingerprint/status was independently refreshed; historical phase hashes
remain historical. Repository followups are verified inREPO-POLICY.md.
