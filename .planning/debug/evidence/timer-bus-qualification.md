# Focused bus repair qualification

Date:2026-11-13. Reviewed source: `7db8ef610535512279806830b5d5122100446b47`.
Scope: finish each CPU bus interval before video/refresh ownership; preserve
snapshot round-trips for requests waiting inside that interval and at exact
release. The strict timer oracle remains unchanged and failing.

## Evidence

- Canonical native command: `sh scripts/check.sh`, exit0, 214.78s.
  Configure started on the clean tracked source above, AppleClang21/Darwin arm64.
  CTest128.86s;246 rows accounted for once,244 pass and the same two declared
  page-mode SERCTL skips. No failure or extra/missing inventory row.
- Artifact scan:6380 files; rights/fixture, deterministic boundary/import,
  privacy/tree, JUnit and full-history checks pass. Debug instrumentation is
  absent from product/tests. Diagnostic outputs were preserved privately only
  after identical27-entry byte/type/mode inventories and owner-marker checks.
- Independent standard review is clean after its snapshot finding and the
  final fixture adjustment; see [review](../TIMER-BUS-REVIEW.md). Tests first
  demonstrated actual bus overlap and snapshot refusal against their preceding
  implementations. Current regression and hostile-state controls pass.
- The first canonical attempt at646b3ba exited8 after the full inventory found
  one stale DMA fault fixture; it is retained as failed evidence, not counted
  as qualification. The corrected actual mutant and clean brackets pass.
- Direct strict profile runs continue to fail only timer row14: legacy12,
  video15, source13. All15 other meaningful rows agree. Completion observations
  and bounded counterfactuals are in [the investigation](timer-focused-bus-accounting.md).

Raw commands, exit records, traces, source-copy hashes and failed/successful
logs are retained in the private focused-investigation temporary bundle.
Historical Phase6 GNU/native/CI and goal fingerprints retain their original
source identities. This followup uses current native canonical qualification
and the Ubuntu/GCC canonical push gate for its two compiler environments;
it adds no duplicate local GNU full gate or new CI job.

## Cost and process review

The successful native gate spends130s in configure/build/test,78s in history,
5s in artifacts and1s each in shellcheck/CPU fixtures (rounded step clocks).
The history gate remains required. No speculative cache, extra matrix or
concurrency change is justified by this small repair. The failed attempt
cost147.76s and led to a concrete lesson: include owning fault-control prefixes
in affected selection, not just `api.*`. Its repeated final gate follows the
actual source/test correction; no broad rerun is needed for later prose.

One final metadata commit records these results before one push. Hosted status
must be read from the exact pushed metadata HEAD and is reported at handoff,
not guessed here. The canonical [main push gate](https://github.com/szTheory/lynxplosion/actions/workflows/ci.yml)
runs the same full inventory under Ubuntu/GCC. No release/tag is authorized.

## Disposition

The software diagnosis and reviewed repair are qualified locally. EVD-02 and
milestone closure remain pending. The next evidence step is the
[proposed hardware study](timer-next-evidence-protocol.md), contingent on owner
hardware details and explicit physical-testing approval. No such execution has
started. Original firmware/content, release and other owner stops persist.
