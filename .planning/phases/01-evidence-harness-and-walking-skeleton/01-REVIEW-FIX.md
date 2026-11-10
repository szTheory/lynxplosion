---
phase: 01-evidence-harness-and-walking-skeleton
fixed_at: 2026-11-09T00:00:00Z
review_path: .planning/phases/01-evidence-harness-and-walking-skeleton/01-REVIEW.md
iteration: 1
findings_in_scope: 13
fixed: 13
skipped: 0
status: all_fixed
---

# Phase 1: Code Review Fix Report

**Fixed at:** 2026-11-09
**Source review:** `.planning/phases/01-evidence-harness-and-walking-skeleton/01-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 13 (CR-01, WR-01..WR-10, plus the two Info findings the caller
  pulled in: IN-06 and IN-03)
- Fixed: 13 (WR-06 and IN-06 with documented limits, see below)
- Skipped: 0 in scope; the seven remaining Info findings stay deferred (listed at the end)

Every gate fix comes with a seeded negative control; each control was shown to fail
against the old behaviour before the fix was kept. Work was done in the main checkout
(`workflow.use_worktrees` is `false`). Nothing was pushed.

## Open item: history rewrite needed for `check.sh` to exit 0

`sh scripts/check.sh` passes every step except `history` on this tree. The
stricter `hygiene.sh --history` (CR-01) now flags one commit: `aac9f2f` (`docs(01): add
code review report`), because the review report quoted the reproduction strings of
CR-01 literally (a personal-looking address and a home path next to allowed tokens). That
is the gate working: those strings are exactly what the policy forbids in a public
history. The tree is clean (the strings were replaced by placeholders in
the CR-01 commit), but the blob in the old commit cannot be changed without rewriting
history. The permission system blocked my attempt to amend `aac9f2f` (a history rewrite), so
this needs an explicit decision before the push, because `aac9f2f` is unpushed (the
branch is ahead of `origin/main`) and CI runs the same history step.

A rewrite is straightforward while it is unpushed: amend `aac9f2f` so the report holds
the placeholders (`<local-part>@<provider>.com`, `/Users/<name>/y`, and "a longer name
that ends in `git@github.com`" in place of the decoy), then rebase the fix commits
on top; the first fix commit (`fe0cfd2`) changes the same report lines and will need
that one hunk resolved in favour of the amended text. With `aac9f2f` replaced, the
history scan covers the rest cleanly: `hygiene.sh --history b8826e0` exits 0 (everything
before the report) and `hygiene.sh --history aac9f2f..HEAD` exits 0 (all the fix commits).

## Fixed Issues

### CR-01: hygiene.sh allow-list is applied per line

**Files modified:** `scripts/hygiene.sh`, `tests/hygiene/pii_control.sh`,
`.planning/phases/01-evidence-harness-and-walking-skeleton/01-REVIEW.md`
**Commit:** fe0cfd2
**Status:** fixed: requires human verification (pattern logic)
**Applied fix:** `leaks()` now tests only the matches (`grep -o`) and an allowed form must
be the whole match (`^(allowed)$`), so an allowed token cannot hide a leak on the same line
or inside a longer name; `EMAIL_OK` forms are whole-token. The history scan uses
`git grep -o` with the same `unallowed()` test. Reproduced first: a same-line leak passed
(`same_line_email` control: expected exit 1, got 0).
**Control:** `tests/hygiene/pii_control.sh` (`hygiene.pii.self_test`): same-line email and
home path in both orders, an allowed token as the suffix or prefix of a longer name, and the
same cases as a committed blob and as a commit message in history.
The review report's own quoted strings were replaced by placeholders in this commit.

### WR-01: hygiene.sh --history fails open when git fails

**Files modified:** `scripts/hygiene.sh`, `tests/hygiene/pii_control.sh`
**Commit:** 02d4cb1
**Applied fix:** the commit list is read into a file and a failing `rev-list` exits 1;
`git grep` status above 1, an unreadable message and an unreadable identity are findings;
an empty selection is reported visibly. Reproduced first: `--history bogusrev` exited 0.
**Control:** `pii_control.sh`: a bogus revision, and a `git` wrapper on `PATH` that fails
`grep` and `log`.

### WR-02: check-manifests.sh traps INT/TERM but does not exit

**Files modified:** `scripts/check-manifests.sh`, `tests/gates/manifests_controls.sh`
**Commit:** 3b11c3c
**Applied fix:** `trap '...' EXIT` plus `trap 'exit 1' INT TERM`.
**Control:** `manifests_controls.sh` case 16 (`signal_exit`): a validator stand-in signals the
running check and returns success; the check must exit non-zero, print no success line and
leave its scratch directory empty. Against the old trap it exited 0 with a garbage success
line.

### WR-03: coverage check exempts everything under fixtures/manifests/

**Files modified:** `scripts/check-manifests.sh`, `tests/gates/manifests_controls.sh`
**Commit:** 735cbae
**Applied fix:** only `*.manifest` files directly in `fixtures/manifests/` are exempt; any
other file there, or a nested manifest, is an uncovered fixture.
**Control:** `manifests_controls.sh` cases 15 (`manifests_dir_other_file`,
`manifests_dir_nested_manifest`); the first returned exit 0 on the old code.

### WR-04: check.sh step() runs each function under `if !`

**Files modified:** `scripts/check.sh`, `tests/gates/check_controls.sh` (new),
`tests/gates/supply.cmake`, `tests/inventory/fast.txt`
**Commit:** 8d35dd8
**Status:** fixed: requires human verification (control flow)
**Applied fix:** steps run bare and an EXIT trap prints `check: step NAME failed`.
Experiment first (dash, bash and the macOS `/bin/sh`): under `if !` a failing `false` inside a
function is ignored and the next command runs; run bare, `set -e` ends the shell. `identities`
captures each tool's output with an explicit failure (a missing cc, ninja or shellcheck no
longer prints an empty value and passes); `getconf` falls back to `-j1` with a visible note;
the probe checks its output file and hashes explicitly.
**Control:** new test `hygiene.check_steps.self_test` (`tests/gates/check_controls.sh`): the step
machinery block of `check.sh` is run as written against a step that fails before its last
command, and the real `check.sh` is run with a failing `cc` and must stop at `identities`.
The test is registered in `supply.cmake` with a sorted row in `fast.txt`.

### WR-05: fetch-pin.sh does not validate DEST, URL scheme or the row

**Files modified:** `scripts/fetch-pin.sh`, `tests/gates/fetch_pin_control.sh`
**Commits:** 7226a4f, 9b13b38 (the second keeps the control clean on shellcheck 0.9.0)
**Applied fix:** NAME `[A-Za-z0-9._-]+`; digest 64 lowercase hex; row of exactly four fields;
URL `https://` only; DEST refuses `''`, `/`, `.`, `..`, `*/.`, `*/..`, a leading `~`, and, as
resolved, the working directory, the home directory or a parent of either; the transfer uses
`--proto`, `--proto-redir '=https'` and `--` before the URL.
A `file://` URL is accepted only when `LYNX_FETCH_PIN_ALLOW_FILE=1`, which only the offline
control sets (the existing control uses `file://` payloads); the pins step never does.
**Control:** `fetch_pin_control.sh` cases 8-12: scheme refusals (file without the variable,
http, ftp, a leading dash), bad digest and name, eight DEST refusals with the protected files
verified to survive, and a recording stand-in for the transfer tool (its name is assembled
from fragments) that checks the https-only options and their order. The old script fails case 8.

### WR-06: the rights gate cannot see firmware stored as text

**Files modified:** `scripts/scan-binaries.sh`, `tests/hygiene/scan_control.sh`,
`docs/policy/clean-room.md`
**Commit:** 91f7cb5
**Status:** fixed (implemented, with documented limits): requires human verification
**Applied fix:** two conservative text-side rules in every zone (source tree, other build
paths, pin zone), waived only by the file's SHA-256 being a manifest value: a text file that
is hex digit pairs, whitespace and `#` comments decoding to exactly 512 bytes (the
`lynx-run --content` syntax), and a run of 200 or more base64 characters on one line. One awk
pass handles both (an ERE interval was 3x slower in some greps), so the artifacts scan stays
near its old time. The real tree passes with no allow-list change: the manifested skeleton
image is exempt through its manifest, and the cc65 and lynx-tests pin trees, the claims and the
planning documents have no hit. The threshold is 200 because POSIX ERE intervals cap at 255.
**Not covered, documented in `docs/policy/clean-room.md`:** an image split over many short
base64 lines, a C array of byte values, other encodings, and content inside compressed archives.
One existing control fixture (a 511-character single-word "512-byte text file", whose point is
that size alone does not make text a binary) became a spaced-words line, because the old line
is a base64-like run; its intent is unchanged.
**Control:** `scan_control.sh` text-side cases: hex images at several widths, comments, blank
lines and CRLF (rejected), 511 and 513 bytes and a digest line (accepted), manifested exemption
(accepted, then rejected again once the manifest is removed), the 199/200 run threshold, prose,
the build tree, and the pin zone where a file on the archive list is still rejected. The old
script fails at `text_hex512`.

### WR-07: bundle_write can leave a mixed bundle with a stale digests.txt

**Files modified:** `runner/bundle.c`, `runner/bundle.h`, `tests/det/runner_cli.cmake`
**Commit:** 7ff690b
**Applied fix:** `digests.txt` and `run.manifest` are removed before anything is written, and
every bundle file is removed and then created exclusively (`"wbx"`), so a symlink planted at a
name is replaced, never followed. (An existing output directory is still accepted: the tests
re-run into one.)
**Control:** `runner_cli.cmake` (`runner.cli`): a failed rewrite over an existing bundle (an
output name made a non-empty directory) must leave no `digests.txt`; symlinked `obs.ram.hex`
and `digests.txt` must be replaced with the link target unchanged. The old code left the stale
`digests.txt`.

### WR-08: the build identity is computed at configure time only

**Files modified:** `CMakeLists.txt`, `cmake/BuildId.cmake` (new), `tests/api/tests.cmake`,
`tests/api/build_id_refresh.sh` (new), `tests/inventory/fast.txt`
**Commit:** d53d481
**Status:** fixed: requires human verification
**Applied fix:** the identity logic moved to `cmake/BuildId.cmake`, which adds HEAD, the
current branch ref, `packed-refs` and the index to `CMAKE_CONFIGURE_DEPENDS`. Paths come from
`git rev-parse --git-path`, so a linked worktree (where `.git` is a file) works. Verified on
the real tree: the runner manifest said `git=91f7cb5...+dirty`, then after the later commit a
plain `cmake --build build/check` gave `git=2db5f17...` with no flag.
**Known limit:** a tracked file edited but neither staged nor committed does not touch any
dependency, so `+dirty` then updates at the next reconfigure; `scripts/check.sh` always
reconfigures.
**Control:** new test `api.build_id.refresh` builds a tiny project that includes the real
module in a temporary repository outside the checkout (Ninja, because a make that rounds file
times to the second can miss a same-second rebuild): an empty commit, a staged change
(`+dirty`), a clearing commit and a commit in a linked worktree, each followed by a plain
build. Without the dependency lines it fails at the first commit.

### WR-09: hygiene.sh writes a predictable temp file name

**Files modified:** `scripts/hygiene.sh`, `tests/hygiene/pii_control.sh`
**Commit:** fea6376
**Applied fix:** `mktemp -d` scratch directory, `EXIT` trap removes it, `INT`/`TERM` exit 1.
**Control:** `pii_control.sh` with a `git` wrapper that records the scratch area while the scan
lists files: it fails on the old `hygiene-list.*` name, on a missing private directory and on a
leftover file.

### WR-10: hygiene.sh tree mode has blind spots

**Files modified:** `scripts/hygiene.sh`, `tests/hygiene/pii_control.sh`
**Commit:** 8e1f70e
**Applied fix:** a symlink is scanned by its target text (tree: `readlink`; history: the blob of
each link a commit adds or changes, because `git grep` does not read symlinks); the script is no
longer excluded (its own pattern text passes); file names are scanned in both modes and the
finding does not print the name; file reads use `cat --` so a name starting with a dash is read
(it was reported as unreadable).
**Control:** `pii_control.sh`: a symlink target in tree and history, a harmless link, the script
scanned itself (a seeded home path in a copy of `hygiene.sh`), and a dashed-home file name in
tree and history.

### IN-06: narrow credential and home-path patterns

**Files modified:** `scripts/hygiene.sh`, `tests/hygiene/pii_control.sh`
**Commits:** 286a047, 77f5711
**Applied fix:** added Anthropic and generic `sk-` keys (a generic key must not be the tail of a
word), npm tokens, JSON web tokens, and the `C:/Users/` and JSON-escaped `C:\\Users\\` forms.
`/root/` was added and then dropped: it flagged an ordinary `$work/root/` directory name in a
test script, and root's home is not a person's; a control records that it is deliberately not a
pattern.
**Control:** `pii_control.sh`: each shape built from fragments, and a word that merely contains
`sk-`.

### IN-03: bundle_write and manifest_text do not check for NULL

**Files modified:** `runner/bundle.c`, `tests/det/test_det.c`
**Commit:** 913c93f
**Applied fix:** `bundle_write` and `bundle_observe` refuse a NULL machine or run.
**Control:** `det.two_instances` calls both with NULL and requires a refusal; the old code aborted
there.

### Settled inventory claim

**Files modified:** `docs/claims/skeleton-repeatability.claim`
**Commit:** 2db5f17
`inventory_sha256` regenerated with `sh scripts/check-claims.sh --inventory-sha256` after the
inventory settled (two tests added: `hygiene.check_steps.self_test`, `api.build_id.refresh`).

## Skipped Issues

None of the in-scope findings were skipped. Deferred by the caller's scope, unchanged:

- IN-01 `runner/main.c:191`: unreachable `load_addr > UINT32_MAX`.
- IN-02 `runner/bundle.h`, `runner/main.c`: `content_name` set but never read.
- IN-04 `core/machine.c`: stale boundary comment.
- IN-05 `tests/cmake/abi.cmake`: weak-undefined symbols and header-comment matches.
- IN-07 `scripts/scan-binaries.sh`: source mode scans the working tree, not the index.
- IN-08 `scripts/check-manifests.sh`: `$root` interpolated into sed and find patterns.
- IN-09 `scripts/scan-binaries.sh`: toolchain-output zone trusts magic bytes (design as stated).

## Verification

**Where it ran:** the dev host (macOS arm64, shellcheck 0.11.0, ugrep as `grep`) in the main
checkout, and an `ubuntu:24.04` container (`--platform linux/arm64`, gcc 13.3, shellcheck
0.9.0, GNU userland) against a fresh clone of the final commit, installed the way the CI image is.

| Run | Result |
| --- | --- |
| `sh scripts/check.sh`, dev host | every step passes (57 of 57 tests; `check_junit`: 57 accounted for once each, 0 skips) except `history`, which fails only on `aac9f2f` (see the open item) |
| same, Ubuntu 24.04 arm64 container | the same: shellcheck 0.9.0 clean over `scripts/*.sh` and `tests/*/*.sh`, 57 of 57 tests, artifacts scan clean, `history` fails only on `aac9f2f` |
| `hygiene.sh --history b8826e0` / `aac9f2f..HEAD` | both exit 0 |

The Linux run found one Linux-only issue: shellcheck 0.9.0 reports SC2015 for an `A && B || C`
in the new fetch-pin control (0.11 does not); fixed in 9b13b38. An earlier run in the amd64
(emulated) image reported sporadic `git grep failed` findings that did not reproduce natively or
on re-run; the fail-closed history scan reported them where the old one would have swallowed
them, and the native arm64 run is the faithful one.

**Wall time, `sh scripts/check.sh` on the dev host (warm cc65 cache):**

| | Total | workflow (ctest) | artifacts | hygiene label |
| --- | --- | --- | --- | --- |
| before | 21.6 s (21 s) | 9 s | 5 s | 19.7 s*proc (6 tests) |
| after | 24.8 s (the `history` step run to its failing end) | 11 s | 5 s | 27.2 s*proc (7 tests) |

About 3 s (15%) slower, spent in the new controls (`pii_control.sh` has many more cases, plus the
check-steps, build-id and signal controls) and the history scan's extra per-commit work (file
names, symlinks). The artifacts step is unchanged because the text-side rule runs as one awk pass.

---

_Fixed: 2026-11-09_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
