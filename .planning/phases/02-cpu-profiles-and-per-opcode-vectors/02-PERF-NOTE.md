# Phase 2 performance note: the check gate after the vector data landed

Scope: `sh scripts/check.sh` went from about 38 s to about 76 s locally after plan 02-03
committed 6.4 MB of vector text under `fixtures/vectors/sst65x02/`. This note records the
profile, what changed, what was left as a candidate and the evidence that no finding class
was lost.

## Profile (what was actually slow)

- The dev host was heavily loaded during every measurement (load average 8 to 120, other
  tools running), so wall times vary by 2x between identical runs. All comparisons below
  alternate old and new scripts in the same session and use CPU seconds (user + sys, minimum
  of 2 to 3 rounds) as the primary number; wall seconds are shown for orientation only.
- `hygiene.sh --tree` read every file about ten times (`cat` to test readability, `wc`,
  `tr | wc`, `iconv`, then once per rule) and ran three regular-expression passes (`grep -E -o`
  with large alternations) over each file: about 0.8 s of grep per 3.2 MB sample, about 20
  forks per file.
- `hygiene.sh --history` ran three whole-tree `git grep -E -o` per commit. The samples are in
  every commit since 02-03, so each new commit adds about 1.5 s wall (1.8 CPU s) to that step,
  and this grows with every future commit.
- `scan-binaries.sh` read each text file three times in `is_nontext` (about 0.1 s per sample)
  plus one awk pass (0.37 s for both samples). Not the bottleneck.
- Not caused by the data: the `*.self_test` tests. They build small synthetic repositories
  (neither `pii_control.sh` nor `scan_control.sh` copies `fixtures/vectors/**`). The same
  `pii_control.sh` takes the same time on the pre-data commit (13 to 17 s when the host is
  loaded) as on HEAD. The reported 3 s to 30 s growth was host contention while the two
  real-tree scans ran beside them under `ctest -j4`, not data volume. Likewise the
  `artifacts` step scans `build/`, which holds no vector files; its growth is host load.

## What changed (commit `perf(02): read each file once ...`)

1. `scripts/hygiene.sh`
   - One read per file: the reader writes a private copy, every rule reads the copy.
   - `is_nontext`: control bytes are detected by `tr -d ... | cmp -s - file` (equal exactly
     when no control byte is present) instead of two `wc` passes; a cmp failure is a
     `cannot read file` finding (fail closed); `iconv` is unchanged.
   - Fixed-string prefilter (`lits.home`, `lits.email`, `lits.cred`, `lits.all` written to the
     scratch directory, one string per line). Every match of the three patterns contains one
     of those strings (`Users`, `/home/`, `@`, `ghp_`, `gho_`, `ghu_`, `ghs_`, `ghr_`,
     `github_pat_`, `AKIA`, `-----BEGIN `, `xox`, `AIza`, `sk-`, `npm_`, `eyJ`), so a file,
     name list, symlink text or message without any of them cannot hold a finding and the
     expensive expression is skipped; with a string present the full rule runs exactly as
     before (`file_leaks`, `file_has`).
   - `--history`: one `git grep -I -l -F -f lits.all` per commit lists the files holding such
     a string; only those files (as `:(literal)` pathspecs) go through the three expressions.
     A quoted name or more than 200 files falls back to the whole tree (the old scan). A failing
     listing reports the same `git grep failed: commit ...` finding, three times as before.
2. `scripts/scan-binaries.sh`: `is_nontext` uses the same `tr | cmp` pass.
3. `tests/hygiene/pii_control.sh`: six new cases seed the forms the prefilter list must keep
   (`AIza`, `gho_`, `ghu_`, `ghs_`, `ghr_`, dashed agent-tool project folder form). Previously only the
   regex covered them, so a dropped literal would have gone unnoticed.
4. New test `hygiene.large_text.self_test` (`tests/hygiene/large_text_control.sh`): the one
   control that carries the real multi-megabyte samples. It copies them into a temporary
   repository, asserts both real scanners accept them, appends a home path and an address to
   one sample, a credential and a 300-character base64 run to the other and a control byte to
   a third (truncated) copy, and asserts `hygiene.sh` and `scan-binaries.sh` report each by
   file name. The real gates (`hygiene.tree`, `hygiene.firmware_scan`, `artifacts`, `history`)
   still scan the real data; nothing is excluded anywhere. Inventory and
   `inventory_sha256` in `docs/claims/skeleton-repeatability.claim` were refreshed (63 tests).

## Before and after

CPU seconds (user + sys), minimum over alternating rounds, same session, same load. "Old" is
the scripts at `def21b1`. The self-test rows compare against the pre-data checkout of the same
tests. Wall seconds in brackets (noisy, load average 50 during this batch).

| step or test | old CPU (wall) | new CPU (wall) |
| --- | --- | --- |
| `hygiene.tree` | 17.1 (12.7) | 5.7 (5.2) |
| `hygiene.firmware_scan` (`--source`) | 6.6 (6.2) | 4.8 (4.3) |
| `artifacts` step (`--artifacts build --cmake-binary-dir build/check`) | 12.5 (10.5) | 8.2 (8.4) |
| `history` step (`--history`, 105 commits) | 52.7 (21.7) | 30.4 (20.8) |
| `hygiene.pii.self_test` | 13.1 (10.9) | 11.7 (13.0) |
| `hygiene.firmware_scan.self_test` | 12.1 (11.7) | 9.2 (9.3) |

A quieter pair of runs earlier in the session (CPU): `hygiene.tree` 7.4 -> 4.0, `--source`
3.1 -> 2.8, history 82.6 -> 47.9 and 59.0 -> 25.6.

`sh scripts/check.sh` end to end, same host, load 5 to 20: 80 s before, 54 s and 65 s after
(workflow 36 -> 23 to 32, artifacts 7, history 34 -> 23). The 45 s target could not be
confirmed on this host because other tools kept it saturated, and the `workflow` step
(the CTest run, parallel and contended) varies most with host load.
`hygiene.large_text.self_test` adds about 5 CPU s, in parallel with the other tests.

## Equivalence evidence

- Real tree, outputs captured before the change and compared after with `cmp`:
  `hygiene.sh --tree` (exit 0, empty), `hygiene.sh --history` (exit 0, empty),
  `scan-binaries.sh --source` (`169 files, 0 of exactly 512 bytes`) and
  `--artifacts build --cmake-binary-dir build/check` (`6423 files, 1 of exactly 512 bytes`)
  are byte-identical. These all pass, so they prove no false positive only.
- Seeded differential run (not committed): a scratch repository with about 20 seeded cases (home
  path in Unix, Windows and dashed forms, e-mail, every credential form, allowed forms, a
  binary, a Latin-1 file, an empty file, no trailing newline, symlinks, a base64 run, a
  3.2 MB sample with a leak appended, file names and commit messages carrying leaks, quoted
  and tab-bearing file names, commits adding and removing leaks) was scanned by the old and the
  new `hygiene.sh --tree`, `--history` and `scan-binaries.sh --source`: stdout, stderr and exit
  codes are identical (16 tree findings, 25 history findings, 3 rights findings).
- Self-tests, pass counts before and after: `hygiene.pii.self_test` 40 -> 46 (six cases added),
  `hygiene.firmware_scan.self_test` 35 -> 35, `hygiene.large_text.self_test` new, 5 checks.
  The whole check (`scripts/check.sh`, 63 tests plus every step) passes, shellcheck clean.
- Mutation controls (each mutant applied to a copy and run against the matching self-test, all
  failed as they must): drop `AIza` from the list; skip the prefilter result; truncate the
  candidate list to the first file; drop the credential rule from the history scan; scan only a
  `fixtures` subtree; a 1 MB size cut-off in `hygiene.sh`; a skipped `scan_text_files` in
  `scan-binaries.sh`.
- Linux: Ubuntu 24.04 container (arm64, GNU userland, shellcheck 0.9.0): shellcheck clean,
  `hygiene.sh --tree`, `--history`, `scan-binaries.sh --source` exit 0, `pii_control.sh` 46 ok,
  `scan_control.sh` 35 ok, `large_text_control.sh` 5 ok. The full `check.sh` was not run there
  (it fetches and builds the cc65 pin; unchanged by this work).

## Candidates not taken

- History blob de-duplication: cache the scan result per blob or tree id and scan a commit
  only for entries not seen in an earlier commit, instead of re-listing every file at every
  commit. It would make the `history` step cost proportional to changed bytes, not
  commits x tree size. Not done: it replaces `git grep -I` semantics (binary detection,
  symlinks, pathspec handling) with a re-implementation that needs its own proof, and the
  prefilter already removed the data-volume term (the samples cost a fixed `git grep -l -F` of
  about 0.02 s per commit now). Revisit if the step passes about 30 s of wall in CI.
- `artifacts` step: about 400 non-pin files each cost three forks (`wc`, `tr | cmp`, `iconv`)
  and the pin directory is hashed and awk-scanned each run. A single awk pass for the
  control-byte test is not portable across mawk, onetrue-awk and gawk NUL handling; not done.
- A dedicated skip for `fixtures/vectors/**` in any gate was rejected: it would remove a scanned
  surface. The new control proves the real data is still read to its last byte.
- Tiny repositories pay a little extra per history scan (one more `git grep`), visible in the
  self-tests that run `--history` about 15 times; the CPU totals above include that cost.
