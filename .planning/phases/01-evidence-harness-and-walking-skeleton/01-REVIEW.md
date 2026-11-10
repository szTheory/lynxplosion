---
phase: 01-evidence-harness-and-walking-skeleton
reviewed: 2026-11-09T00:00:00Z
depth: standard
files_reviewed: 25
files_reviewed_list:
  - core/machine.c
  - core/snapshot.c
  - core/internal.h
  - core/sha256.c
  - core/sha256.h
  - core/build_id.c.in
  - include/lynxplosion.h
  - runner/bundle.c
  - runner/bundle.h
  - runner/main.c
  - scripts/check.sh
  - scripts/fetch-pin.sh
  - scripts/hygiene.sh
  - scripts/scan-binaries.sh
  - scripts/validate-kv.sh
  - scripts/check-claims.sh
  - scripts/check-manifests.sh
  - CMakeLists.txt
  - CMakePresets.json
  - cmake/Tests.cmake
  - .github/workflows/ci.yml
  - tests/cmake/abi.cmake
  - tests/cmake/check_junit.cmake
  - tests/cmake/inventory_equal.cmake
  - tests/cmake/toolchain_probe.cmake
findings:
  critical: 1
  warning: 10
  info: 9
  total: 20
  necessity: 0
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-11-09
**Depth:** standard
**Files Reviewed:** 25
**Status:** issues_found

Source: the gsd-code-reviewer agent's report, saved by the orchestrator (the agent's rules forbid
writing report files). The C core (`machine.c`, `snapshot.c`, `sha256.c`, `internal.h`,
`lynxplosion.h`) held up: length and address arithmetic uses the subtraction form, snapshot restore
validates everything before the first write, no overflow or UB found. The runner's decimal/hex
parsing and hex reader are bounded. `ci.yml` is clean on permissions, SHA pinning,
`persist-credentials: false` and timeout (the checkout SHA vs the `v7.0.1` comment could not be
re-verified by the reviewer because the GitHub API was rate-limited). The `tests/cmake/*` files
(junit, inventory, probe) showed no defects. The defects are in the shell gates, a few bundle and
build-identity issues. Three were reproduced in scratch directories (CR-01, WR-01, WR-02).

## Critical Issues

### CR-01: hygiene.sh allow-list is applied per line, so an allowed token hides a leak on the same line

**File:** `scripts/hygiene.sh:45` (also 79-80, 124-127, 135-139)
**Issue:** `leaks()` is `grep -E "$1" | grep -v -E "$2" | grep -q .`. Any line containing an allowed
token (`git@github.com`, `noreply@anthropic.com`, `/home/runner`, `@example.com`) is dropped whole,
even if it also holds a real address or home path. Reproduced in a scratch repo: a tracked file with
`contact <local-part>@<provider>.com or git@github.com` and `/home/runner/x and /Users/<name>/y` made
`hygiene.sh --tree` exit 0 with no finding. The same flaw is in `--history` for blobs and commit
messages. The exemptions are also unanchored, for example `git@github.com` matches
a longer name that ends in `git@github.com`. For a public repo this gate is the last barrier before an irreversible leak and
the owner requires it to fail closed.
**Fix:** Remove the allowed matches from each line before testing, so only the remainder can trigger
a finding, e.g. `leaks() { grep -E -o -e "$1" | grep -v -x -E -e "$2" | grep -q .; }` (`-o` emits each
match alone, `-x` anchors the allow-list). Make `HOME_OK` and `EMAIL_OK` match a whole token. Add a
seeded control to `tests/hygiene/pii_control.sh` for the same-line case.

## Warnings

### WR-01: hygiene.sh --history fails open when git fails

**File:** `scripts/hygiene.sh:123-131`
**Issue:** `for c in $(git rev-list "$@")` is not subject to `set -e`, so a failing `rev-list` gives
an empty loop and exit 0 (reproduced: `hygiene.sh --history bogusrev` exits 0). Every
`git grep ... 2>/dev/null | grep -v ... | grep -q .` also discards git's own status and stderr.
**Fix:** `revs=$(git rev-list "$@") || { ...; exit 1; }`; treat `git grep` rc > 1 as a finding.

### WR-02: check-manifests.sh traps INT/TERM but does not exit, so it can finish with exit 0

**File:** `scripts/check-manifests.sh:52`
**Issue:** `trap 'rm -rf "$work"' EXIT INT TERM` deletes the work dir on a signal and the script
continues (reproduced with SIGTERM: success line printed, exit 0). `scan-binaries.sh:88` does this
correctly.
**Fix:** `trap 'rm -rf "$work"' EXIT` and `trap 'exit 1' INT TERM`.

### WR-03: check-manifests.sh coverage check exempts everything under fixtures/manifests/

**File:** `scripts/check-manifests.sh:118`
**Issue:** `! -path "$root/fixtures/manifests/*"` exempts any file there, e.g. `rom.bin`.
**Fix:** Exclude only `*.manifest` files and add a finding for any other file under that directory.

### WR-04: check.sh step() runs each function under `if !`, which disables `set -e` inside the function

**File:** `scripts/check.sh:18` (consequences at 28-31, 176)
**Issue:** `set -e` is ignored in a context whose status is tested, including `if ! "$@"`. Only the
last command of each `run_*` function (or an explicit `|| return 1`) can fail a step. E.g.
`jobs=$(getconf _NPROCESSORS_ONLN)` can fail silently leaving `-j` with no value; a missing `cc`,
`ninja` or `shellcheck` makes `identities` print empty values and still pass.
**Fix:** Call the step bare and report failure from an EXIT trap, or add `|| return 1` to every
fallible command and `jobs=$(getconf ...) || jobs=1`.

### WR-05: fetch-pin.sh does not validate DEST, URL scheme or the row before acting

**File:** `scripts/fetch-pin.sh:34-37, 80, 124`
**Issue:** `rm -rf "$dest"` is unconditional and only `""` and `/` are refused (`.`, `..`, `$HOME`
pass). `curl -fsSL` follows cross-scheme redirects and does not stop a URL starting with `-` being
read as an option; "https only" is documented but not enforced. The SHA-256 check protects
integrity but not these.
**Fix:** reject `''|/|.|..|*/.|*/..|~*` for DEST; require `https://`; use
`curl -fsSL --proto '=https' --proto-redir '=https' --retry 3 -o "$file" -- "$url"`; require a
64-char lowercase-hex digest and a `[A-Za-z0-9._-]+` name.

### WR-06: the rights gate cannot see firmware that is stored as text, although the runner accepts hex text

**File:** `scripts/scan-binaries.sh:159-167`
**Issue:** The 512-byte rule and manifest rule apply only to non-text files. A boot ROM committed as
`.hex`, base64 or a C array passes both scans; `lynx-run --content` loads exactly that hex format.
**Fix:** Add a text-side rule: flag a non-manifested text file whose hex-pair stream decodes to
exactly 512 bytes and flag long hex/base64 runs above a threshold unless the SHA-256 is manifested.
At minimum document the limitation.

### WR-07: bundle_write can leave a mixed bundle with a stale digests.txt

**File:** `runner/bundle.c:309-315`
**Issue:** Five files are written in sequence into a directory that may hold a previous bundle;
`digests.txt` is written last, so a mid-way failure leaves the old `digests.txt` beside new `obs.*`
files. `fopen(path, "wb")` also follows a symlink planted at `dir/<name>`.
**Fix:** Remove/truncate `digests.txt` and `run.manifest` first; refuse a non-empty `--out`
directory or use exclusive-create semantics.

### WR-08: the build identity is computed at configure time only

**File:** `CMakeLists.txt:31-57`
**Issue:** `git rev-parse HEAD`/`git status` run once in `execute_process`; a later commit followed
by a plain `cmake --build` keeps the stale revision and dirty flag in `lynx_build_id()` (goes into
run manifests and snapshot compatibility). `scripts/check.sh` is safe because the workflow
reconfigures.
**Fix:** add `.git/HEAD` and `.git/index` (real git dir) to `CMAKE_CONFIGURE_DEPENDS`, or regenerate
`build_id.c` from a custom target on every build.

### WR-09: hygiene.sh writes a predictable temp file name

**File:** `scripts/hygiene.sh:112-118`
**Issue:** `${TMPDIR:-/tmp}/hygiene-list.$$` follows a pre-planted symlink.
**Fix:** `mktemp` plus an EXIT trap.

### WR-10: hygiene.sh tree mode has blind spots that history mode does not

**File:** `scripts/hygiene.sh:66-71, 124-130`
**Issue:** `check_file` skips symlinks (a tracked symlink whose target text is `/Users/<name>/...`
passes tree mode); the script excludes itself from both modes; file names are never scanned.
**Fix:** scan `readlink` output; drop the self-exclusion or scan it with the pattern definitions
masked; run the patterns over `git ls-files` names.

## Info

- **IN-01** `runner/main.c:191`: `load_addr > UINT32_MAX` is unreachable (parse_hex caps at 8 digits). Drop the clause.
- **IN-02** `runner/bundle.h:20`, `runner/main.c:197`: `content_name` set but never read. Remove or use it.
- **IN-03** `runner/bundle.c:191, 277-283`: `bundle_write`/`manifest_text` dereference `run` without a NULL check although the header promises a refusal. Add `run == NULL` to the argument check.
- **IN-04** `core/machine.c:4-6`: stale boundary comment (snapshot.c also calls memcmp; abi.cmake allowlist adds memmove). Align them.
- **IN-05** `tests/cmake/abi.cmake:61-65, 84`: weak-undefined (`w`, `v`) symbols are not reported; the `lynx_...(` regex matches names in header comments. Treat `w`/`v` as undefined; strip comments before matching.
- **IN-06** `scripts/hygiene.sh:35, 42`: narrow patterns: no `sk-ant-`, `sk-`, npm or JWT shapes; `HOME_PATH` misses `/root/`, `C:/Users/`, JSON-escaped `C:\\Users\\`. Extend and add controls.
- **IN-07** `scripts/scan-binaries.sh:151, 243-245`: source mode scans the working tree, not the index (a deleted tracked file is skipped). Read via `git show ":$sf"` when absent.
- **IN-08** `scripts/check-manifests.sh:56, 70, 118-119`: `$root` is interpolated into sed/find patterns. Run `find` from inside `$root` with relative paths.
- **IN-09** `scripts/scan-binaries.sh:121-127, 169-172`: the toolchain-output zone accepts any file with ELF/Mach-O/ar magic. Design as stated; optionally require a parser to accept it.

---

_Reviewed: 2026-11-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
