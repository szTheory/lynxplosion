# Phase 5 Qualification Record

**Status:** Plan 05-12 final-source qualification passed. Product and test
source at `d3fb0024fe6a732b05f9b42122a18c110bed8ae2` was tested on macOS and
self-contained Ubuntu, pushed normally to `origin/main`, and passed the exact
head's hosted CI run. This records software behavior only; phase/security
closure remains with root.

## Reviewed source and inventory

- **Tested, pushed, and CI source:** `d3fb0024fe6a732b05f9b42122a18c110bed8ae2`
- **Source scope:** 55 reviewed product, test, script, fixture, and build paths;
  fingerprint `ea140006093038be023cdceeb67c4c1468576c940f0aa942b1b4b69bbcf1a1f3`.
  The same 55 hashes were verified in the host and independent Linux clone.
- **Linux Git history:** 354 commits, non-shallow, with no alternates outside
  the pinned Ubuntu 24.04 image.
- **Fast inventory and repeatability claim:** every final configure matched the
  sorted `tests/inventory/fast.txt` set. The claim check returned
  `1e74578b2e13db45953c6fd02e4793f0e04623f4d4b3d7158b04a69049bd9c5d`, matching
  `docs/claims/skeleton-repeatability.claim`.

The README qualification paragraph and planning records are documentation-only
followups written after these gates. The final pushed and CI-tested source stays
`d3fb0024fe6a732b05f9b42122a18c110bed8ae2`; no CI result is claimed for the
later metadata commit. The README's documentation replacement SHA-256 is
`629a416602d47780bee8fdfeb13b64ce9b5e1b9c3c30fab23846f76c2c91a510`. All 54
other paths in the tested 55-path map, including every product, test, fixture,
and script path, remain byte-identical.

## Final canonical gates

All three environments ran `sh scripts/check.sh` against the same source head.
Each gate accounted for all 202 registered CTest/JUnit rows exactly once: 200
tests passed, two declared page-mode observations were skipped, and none
failed. JUnit, artifact scanning, and full-history hygiene all passed.

| Environment | Toolchain and identity | CTest / JUnit | Artifacts | History / total |
|---|---|---|---|---|
| macOS local | macOS 26.6.2 arm64; AppleClang 21.0.0.21000101; CMake 4.4.3; Ninja 1.13.2; ShellCheck 0.11.0; Git 2.50.1 | 200 passed + 2 declared skips (202 JUnit rows); 0 failed; CTest 110.25s | 6,361 files scanned; the only accepted binary was exactly 512 bytes | 85s / 204s |
| Self-contained Linux | Pinned `ubuntu@sha256:19cad59657037cffa0ac9a48107019b768900ff1071140965827513ac866bd5e`; Ubuntu 24.04 arm64; GCC 13.3.0; CMake 3.28.3; Ninja 1.11.1; ShellCheck 0.9.0; Git 2.43.0 | 200 passed + 2 declared skips (202 JUnit rows); 0 failed; CTest 111.07s | 6,330 files scanned; the only accepted binary was exactly 512 bytes | 13s / 127s |
| Hosted CI | [Run 43268267738](https://github.com/szTheory/lynxplosion/actions/runs/43268267738), push event on `main`, exact head `d3fb0024fe6a732b05f9b42122a18c110bed8ae2`; Ubuntu 24.04.5 x64; GCC 13.3.0; CMake 3.31.6; Ninja 1.13.2; ShellCheck 0.9.0; Git 2.56.0 | 200 passed + 2 declared skips (202 JUnit rows); 0 failed; CTest 105.60s | 6,329 files scanned; the only accepted binary was exactly 512 bytes | 32s / 171s for the canonical command |

The exact skips in every environment were `suite.page_mode.observation.lynx1`
and `suite.page_mode.observation.lynx2`. Both stop at the unsupported `$06`
write to `$FD8C` before producing any result. Their raw observations are
missing; neither skip establishes profile agreement or qualification.

The final local raw logs are retained outside the repository and were not
committed. Their SHA-256 digests are `9c248689c8222002f37f347218a7d3d1595e9c0fe615f2bd32aec05b252a4a26`
(macOS) and `9dafb765f63d82b0c4277e24c712fae64f17050ebe7264e7dc6d81c2e160526a`
(Linux). Hosted evidence is available from the linked GitHub run.

## Hosted critical path

The push-triggered workflow used one Ubuntu job and one `scripts/check.sh`
step, with a full-history checkout (`fetch-depth: 0`). Queue time was about 2s,
runner setup was under 1s, checkout took 2s, and the command took 171s. Within that command, the pinned cc65
build took 19s, CMake configure about 0.7s, the native C build about 4.6s,
CTest 105.60s, JUnit checking 0s, artifact scanning 2s, and history hygiene
32s. The slowest CTests were `suite.timers.pending_controls` (58.70s),
`suite.timers.result_controls` (47.88s), and
`api.seam_agreement.none` (24.68s). There was no job matrix or duplicate broad
gate. These measurements do not support adding a cache, concurrency, or a
narrower test set: CTest is the main cost and the full-history scan is the
next-largest distinct check.

## Semantic outcomes and limits

- `LEGACY_FLAT5` remains the default. In its no-DMA, zero-counter D-10
  environment, the single admitted timer row is index 14: observed `0c` versus
  source `0d`.
- `VIDEO_POLICY1` has no pending descriptor. Both profiles retain the strict
  index-14 mismatch `0f` versus source `0d` and agree on the other 15 of 16
  rows. The strict source comparisons remain unresolved failures; green
  observation/control CTests do not turn those comparisons green.
- These results are software observations and repeatability evidence, not
  physical Lynx calibration. EVD-02 remains open. Recurring EVD-05 and CON-01
  remain partial for their Phase 6 audio obligations.
- O1, O5, O6, and O9 remain open. No BIOS or commercial content, physical
  testing, shared LibFuturo change, release, or broader publication authority
  is implied.

## Resolved qualification blockers

Earlier attempts at intermediate reviewed source heads are recorded as
failures, not green final gates. The macOS ShellCheck preflight and 12
test-harness failures, the stale successful-consumer scratch artifact finding,
Ubuntu ShellCheck 0.9 diagnostics, GCC 13 warning failures, and seven Ubuntu
consumer provenance refusals were fixed at their causes and independently
reviewed before the final gates above. See
[`05-REVIEW-RESOLUTION.md`](./05-REVIEW-RESOLUTION.md),
[`05-QUALIFICATION-REPAIRS.md`](./05-QUALIFICATION-REPAIRS.md),
[`05-CLEANUP-REPAIR.md`](./05-CLEANUP-REPAIR.md),
[`05-PORTABILITY-REPAIR.md`](./05-PORTABILITY-REPAIR.md),
[`05-GCC-REPAIR.md`](./05-GCC-REPAIR.md), and
[`05-PROVENANCE-REPAIR.md`](./05-PROVENANCE-REPAIR.md).

The qualification evidence now binds the exact reviewed, pushed, and CI-tested
source. Root retains the separate final security-ledger and phase-closeout
actions; this record does not advance the roadmap or close phase-wide
requirements.
