---
phase: "07"
status: qualified
qualified: "2026-11-14"
source_head: 3b50de650226f392655c65660c94655c97ea8ea0
ci_run: 43612316517
inventory_sha256: f7d0671f82d03a829dad8ea5108679bc53086bebb14483415c1a4488297a962f
registered_tests: 271
passed_tests: 269
declared_skips: 2
failed_tests: 0
---

# Phase 7 — Software Qualification

The reviewed Suzy math/register-mirror software scope passes the canonical gate
on native macOS and hosted CI at exact source
`3b50de650226f392655c65660c94655c97ea8ea0`, and the owning groups pass on the
existing GNU/Linux clone at the same commit. This is scoped software evidence.
It does not close full SUZ-02, the open timer comparison (EVD-02), the physical
study, or any rights, model or release stop. Review/lifecycle commits after
this source are metadata successors; no local qualification of them is implied.

## Source and review identity

Review: `07-REVIEW.md` (CR-01, WR-01, WR-02) and the independent ASVS1 audit in
`07-SECURITY.md` (one further defect, T-07-03). Repair commits: `0fe0734`
(overflowing oracle ranges), `7848e48` (source guest return boundary),
`612c1e4` (eight hostile strict-record controls), `b287750` (coarse advance
could step past a pending Suzy deadline; regression `suzy.coarse_deadline`,
red before and green after the repair), `3b50de6` (repeatability claim rebound
to the inventory that now lists that test).

## Canonical results

| Environment | Identity | Result |
|---|---|---|
| Native macOS | Darwin 25.6.0 arm64; Apple clang 21.0.0; CMake 4.4.3; Ninja 1.13.2; ShellCheck 0.11.0; Debug; `sh scripts/check.sh` at the exact source | 271 accounted once each (269 pass, 2 declared page-mode skips, 0 fail); CTest 187s; artifacts 6395 files, no findings; history 129s; total 329s |
| Existing GNU clone | `lynx-phase5-task12-linux-reuse`, `/work/lynxplosion`; actual `git fetch` + `merge --ff-only` to the full SHA, clean tracked tree; GCC 13.3.0; CMake 3.28.3; ShellCheck 0.9.0; Release with explicit `-O2 -g -UNDEBUG`, assertions on, non-recovering ASan/UBSan | 142/142 pass in 35.4s: suzy, suite.math/memio, diag, det, api, audio, video, timers, input; `shellcheck -s sh scripts/*.sh tests/*/*.sh` clean |
| Hosted CI | [Run 43612316517](https://github.com/szTheory/lynxplosion/actions/runs/43612316517), push/main, exact source; Ubuntu 24.04; GCC 13.3.0; CMake 3.31.6; Ninja 1.13.2; ShellCheck 0.9.0; Git 2.56.1 | 271 accounted (2 declared skips), 0 fail; CTest 206s; artifacts 6340 files, no findings; history 50s; total 277s; job success |

Focused strict controls: `suite.math.strict_comparison_controls` passes with the
eight subcases (timing bytes 14 and 15, omitted timing, timing classified
reserved, missing strict verdict, stale mismatch count, dropped mismatch row,
scoped pass promoted to whole pass), each bracketed by a clean positive record.

GNU used the optimized Release configuration (the plan named RelWithDebInfo;
the effective flags `-O2 -g -UNDEBUG` are the same), in `build/check` because
the suite harness owns that scratch root. The full GNU Debug canonical run was
not repeated (known timeout; no limits changed). GNU ran the groups the
`core/machine.c` repair can affect, not the whole 271-test inventory; the
whole inventory is covered by the native and hosted runs.

## Environment notes

The first native gate failed only at the artifact scan on four stale
`run-<nonce>` scratch roots (112 files, all older than the session) retained
from earlier red-phase controls. They were moved outside `build/` after an
age check and equal before/after SHA-256 inventories; the scanner and its
policy were unchanged, and the second and later gates were clean. A later gate
failed once at `docs.claim_template` because `inventory_sha256` was stale after
`fast.txt` gained `suzy.coarse_deadline`; the claim was rebound and the gate
re-run clean. Both failed logs are retained separately from qualification.

## Limits

Policy, register and arithmetic results are software observations against
pinned sources. Strict timer/timing disagreements remain exactly as recorded
(unchanged source oracle, no fitting). Physical hardware, model baseline (O1),
rights, release and the next phase remain stops.
