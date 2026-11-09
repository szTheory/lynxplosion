# Phase 1: Evidence Harness and Walking Skeleton - Pattern Map

**Mapped:** 2026-11-09
**Files analyzed:** 41 new files (the repository holds planning documents only, so nothing is modified except `.planning/PROJECT.md`)
**Analogs found:** 36 / 41 (15 exact, 15 role-match, 6 partial, 5 none)

## How to read this file

- Every analog lives in the owner's sibling project `~/projects/muhtari` (MIT, owner-authored), read at its commit `7fd0386`. All analog files were checked with `git ls-files` inside that repo: they are tracked source, none is a build mirror. Paths below are written `~/projects/muhtari/...` on purpose; never expand `~` into an absolute home path in any lynxplosion file (the hygiene scan rejects it).
- One further analog, `~/projects/libfuturo/scripts/check.sh`, is used for shape only (identity printing). It is bash and Darwin-only.
- **Copy rule:** "exact" means copy the file, rename, add the provenance header, change nothing else unless the adaptation list says so. Header to add to each copy: `Adapted from muhtari (MIT, owner project) at commit 7fd0386.` (RESEARCH A10 asks the owner to confirm the notice; keep it in `docs/` provenance too.)
- **Rename map:** `muhtari_` to the chosen public prefix (RESEARCH uses `lynx_`), `muhtari__` to `lynx__` (internal), `MUHTARI_` to `LYNX_`, `muhtari.h` to `lynxplosion.h`. The prefix must be fixed before copying `abi.cmake`, because its `symbols` mode keys on it (lines 90, 93, 94, 100).
- **Never copy:** `check_stop_policy` and `check_branch` in `hygiene.sh` (GSD-mode specifics; `check_stop_policy` fails on this repo's `mode: yolo` config), `tests/cmake/firmware_scan.cmake` (hashes known 7800/2600 ROMs), the MSVC/PPC/WASI lanes, `ci-msvc`/`be32`/`wasm32` presets, `hygiene.timeout_comments`, quality/perf/release machinery.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog (all under `~/projects/muhtari/`) | Match Quality |
|---|---|---|---|---|
| `CMakePresets.json` | config | batch | `CMakePresets.json` | role-match (schema 11 to 6, F1/F7) |
| `CMakeLists.txt` | config | batch | `CMakeLists.txt` | role-match |
| `cmake/Tests.cmake` | utility | batch (configure-time check) | `cmake/MuhtariTests.cmake` | exact |
| `core/build_id.c.in` + configure step | config | transform | `cmake/version.h.in` + `configure_file` in `CMakeLists.txt` | partial |
| `scripts/check.sh` | script (driver) | batch | libfuturo `scripts/check.sh` (shape) + `scripts/fetch-pin.sh` (idioms) | partial |
| `scripts/fetch-pin.sh` | script | file-I/O | `scripts/fetch-pin.sh` | exact |
| `pins.txt` | config | n/a | `pins.txt` | exact (format) |
| `scripts/hygiene.sh` | script | batch / file-I/O | `scripts/hygiene.sh` | role-match (adapted) |
| `scripts/scan-binaries.sh` | script | file-I/O | `scripts/hygiene.sh` `check_file`/`check_files`/`in_manifest` | partial |
| `scripts/validate-kv.sh` | script | transform | `scripts/hygiene.sh` `in_manifest` (awk idiom) | partial |
| `tests/cmake/check_junit.cmake` | utility | batch | `tests/cmake/check_junit.cmake` | exact (+ skips.txt) |
| `tests/cmake/expect_fail.cmake` | utility | batch | `tests/cmake/expect_fail.cmake` | exact |
| `tests/cmake/no_skip.cmake` | utility | batch | `tests/cmake/no_skip.cmake` | exact |
| `tests/cmake/no_float.cmake` | utility | batch | `tests/cmake/no_float.cmake` | exact |
| `tests/cmake/core_includes.cmake` | utility | batch | `tests/cmake/no_float.cmake` (file loop shape) | role-match |
| `tests/cmake/abi.cmake` | utility | batch | `tests/cmake/abi.cmake` | exact (renamed, helper rows trimmed) |
| `tests/cmake/pins.cmake` | utility | batch | `tests/cmake/pins.cmake` | exact |
| `tests/gates/tests.cmake` (policy, hygiene, manifest, claim, inventory self-tests) | test | batch | `tests/hygiene/tests.cmake` | role-match |
| `tests/abi/tests.cmake` | test | batch | `tests/abi/tests.cmake` | exact (adapted) |
| `tests/abi/*_fixture.c` (libc, global, float, include controls) | test fixture | n/a | `tests/abi/libc_fixture.c`, `global_fixture.c`, `nofp_fixture.c` | exact (libc, global), role-match (rest) |
| `tests/inventory/fast.txt`, `skips.txt` | config | n/a | `tests/inventory/hygiene.txt` | exact (format) |
| `tests/hygiene/scan_control.sh` | test | file-I/O | `tests/hygiene/history_control.sh` | role-match |
| `tests/check.h` | utility | n/a | `tests/check.h` | exact |
| `tests/header/header.c` | test | n/a | `tests/header/header.c` | exact |
| `tests/api/test_api.c` + `tests/api/tests.cmake` | test | request-response | `tests/core/test_core.c`, `tests/core/tests.cmake` | role-match |
| `tests/det/test_det.c` + `tests/det/tests.cmake` | test | batch | `tests/core/test_core.c`, `tests/core/tests.cmake` | role-match |
| `tests/golden/skeleton.digest` | fixture | n/a | none | none |
| `include/lynxplosion.h` | public header | request-response | `include/muhtari.h` | role-match (no allocator) |
| `core/machine.c` (+ `core/internal.h`) | core | request-response | `src/instance.c` | role-match |
| `core/snapshot.c` | core | transform | `src/state.c` (save/load, lines 1258-1326) | role-match |
| `core/sha256.c`, `core/sha256.h` | core util | transform | `runner/sha256.c`, `runner/sha256.h` | exact (relocated) |
| `runner/main.c` | runner | file-I/O | `runner/main.c` (hash printing, CLI style) | role-match |
| `runner/bundle.c` (run.manifest, digests.txt, bundle digest) | runner | file-I/O | none (print_hashes is the nearest idiom) | none |
| `.github/workflows/ci.yml` | config | batch | `.github/workflows/ci.yml` | role-match (one job) |
| `docs/policy/clean-room.md` | doc | n/a | `PROVENANCE.md` section 1 | role-match |
| `docs/policy/exposure-record.md` | doc | n/a | none | none |
| `docs/claims/claim.template` | doc/config | n/a | none | none |
| `fixtures/manifests/*.manifest` | fixture config | n/a | `tests/roms/manifest.txt` | role-match (tab table to key = value) |
| `fixtures/toolchain/probe.c` + manifest | fixture | n/a | none | none |
| `README.md` | doc | n/a | `README.md` | partial (tone only; add "all rights reserved, O5 open") |
| docs provenance notice for copied helpers | doc | n/a | `PROVENANCE.md` section 5 | partial |
| `.planning/PROJECT.md` (O8 Key Decision) | doc | n/a | self | partial |

## Adaptation index (RESEARCH flags to files)

| Flag | Lands in | One-line instruction |
|---|---|---|
| F1 two-step cc65 make | `scripts/check.sh` | `make -C build/pins/cc65 -j"$jobs" bin` then a second `make ... lib TARGETS=lynx`; no analog in muhtari |
| F2 artifact-zone scan | `scripts/scan-binaries.sh`, `check.sh` post-step | pin zone excluded by one visible rule; toolchain outputs allowed by magic; runner dumps are hex text |
| F3 binary detection | `scripts/scan-binaries.sh` | replace muhtari `grep -I -q ''` (NUL only, hygiene.sh line 70) with `tr` control-byte test plus `iconv` |
| F4 sha256 in core | `core/sha256.*`, `runner/` CMake | one copy in `core/`; muhtari already shares one file between core and runner (`CMakeLists.txt` lines 98-114) |
| F5 nm on a plain twin | `CMakeLists.txt`, `tests/abi/tests.cmake` | `lynx_core_plain` is the `-DLIB=` target of `abi.cmake`; the sanitized lib is never nm-checked |
| F6 fortify/bzero allowlist | `CMakeLists.txt`, `abi.cmake` | add `-D_FORTIFY_SOURCE=0`; keep the Darwin `bzero __bzero` and Linux GOT rows only |
| F7 non-recoverable UBSan | `CMakeLists.txt` | `-fno-sanitize-recover=all`; spelled in muhtari `asan` preset line 55 |
| F8 manifest keys | `scripts/validate-kv.sh` | ten CONTEXT keys required; `independence` optional |
| F9 FND-09 evidence | `scripts/check.sh`, `fixtures/toolchain/` | hash tools and a linked probe, never `lynx.lib` |
| F10 floor 3.25 | `CMakePresets.json` | `"version": 6`, `cmakeMinimumRequired` 3.25.0 |

## Pattern Assignments

### `CMakePresets.json` (config, batch)

**Analog:** `~/projects/muhtari/CMakePresets.json`

**Hidden base preset** (lines 5-10, copy):
```json
{ "name": "base", "hidden": true, "generator": "Ninja",
  "binaryDir": "${sourceDir}/build/${presetName}" }
```

**Test preset shape** (lines 159-168, keep `output` and `execution.noTestsAction`; drop the `jobs` key and the label filters unless a split is needed, RESEARCH verified the shorter form on 3.25.3):
```json
"output": { "outputOnFailure": true, "outputJUnitFile": "${sourceDir}/build/${presetName}/junit.xml" },
"execution": { "noTestsAction": "error" }
```

**Workflow preset** (lines 306-313): `{ "type": "configure" }, { "type": "build" }, { "type": "test" }` with the same name in each step.

**Sanitizer flags to reuse** (the `asan` preset, line 55): `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`.

**Adaptations:**
- Line 2-3: muhtari is `"version": 11`, `cmakeMinimumRequired` 4.3. Use `"version": 6` and 3.25.0 (F10). Keep one configure/build/test/workflow preset named `check`; do not copy the other 13 presets.
- Divergence from muhtari: muhtari sets sanitizer flags in the preset's `CMAKE_C_FLAGS` and excludes the `abi` label from its asan test preset (preset line 55; `filter.exclude.label` `^(abi|release)$`), running abi tests on a separate lane. Lynxplosion has one lane, so put the sanitizer flags on the `lynx_core` and test targets with `target_compile_options` (RESEARCH "Sanitizer and lint flags") and leave `lynx_core_plain` uninstrumented. Do NOT put them in `CMAKE_C_FLAGS`, or the plain twin is instrumented and F5 fails.

---

### `CMakeLists.txt` (config, batch)

**Analog:** `~/projects/muhtari/CMakeLists.txt`

**C standard** (lines 16-18, copy):
```cmake
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
```

**Warning function** (lines 25-32): a `muhtari_warnings(target)` function applying the flag set per target. Rename, and use the CONTEXT set: `-Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror` (muhtari uses `CMAKE_COMPILE_WARNING_AS_ERROR` from the preset; either works, the explicit `-Werror` is the CONTEXT wording).

**Core compile options** (lines 53-73, function `muhtari_core_options`; this is the pattern for `lynx_core_plain`):
```cmake
target_compile_options(${target} PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-fno-stack-protector -U_FORTIFY_SOURCE -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.>)
set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)
```
Add `-D_FORTIFY_SOURCE=0` (F6) and drop the MSVC and `MUHTARI_NOFP` branches.

**Core and plain twin from one source list:** muhtari builds `muhtari_objs` once and shares objects. For lynxplosion build two static libs from the same `LYNX_CORE_SOURCES` list (`lynx_core` instrumented, `lynx_core_plain` with the options above), see RESEARCH Code Examples.

**sha256 shared with the runner** (lines 108-114): muhtari uses `set_source_files_properties(... INCLUDE_DIRECTORIES ...)` so only the state source sees the sha header. With sha256 in `core/` (F4) the runner links `lynx_core` and calls the internal `lynx__sha256_*` names; `abi.cmake symbols` accepts any external name starting `lynx__`.

**Test registration** (lines 254-307): `enable_testing()`, then `include(cmake/Tests.cmake)`, then `include(tests/...)` files (never `add_subdirectory`, so the directory `TESTS` property holds every name; comment at `tests/tests.cmake` lines 1-6).
```cmake
if(LYNX_TESTS)
  enable_testing()
  include(cmake/Tests.cmake)
  include(tests/tests.cmake)
endif()
```
Find `sh` once: `find_program(LYNX_SH sh REQUIRED)` (line 226).

**Adaptations:** muhtari has `project(... LANGUAGES C)` plus `enable_language(CXX)` for a C++ header compile (line 123). Lynxplosion has no C++ compiler requirement in Phase 1; RESEARCH makes the C++ syntax check conditional on a compiler existing. Skip install/export/package code (lines 120+). Build identity: see the `core/build_id.c.in` entry.

---

### `cmake/Tests.cmake` (utility, batch)

**Analog:** `~/projects/muhtari/cmake/MuhtariTests.cmake` (69 lines, exact)

**Registration wrapper** (lines 13-24): `NAME`, `COMMAND`, `LABELS`, `TIMEOUT` required, then `add_test` and `set_tests_properties(... LABELS ... TIMEOUT ...)`. Rename `muhtari_add_test` to `lynx_add_test`; drop `MUHTARI_TIMEOUT_SCALE` and `COST` (one lane).

**Equality check** (lines 28-67), the core of FND-06 layer 1:
```cmake
get_property(names DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY TESTS)
foreach(name IN LISTS names)
  get_test_property(${name} LABELS labels)
  list(SORT labels)
  list(JOIN labels "," labels)
  list(APPEND registered "${name}\t${labels}")
endforeach()
...
if(NOT sorted STREQUAL listed)
  message(FATAL_ERROR "${file} is not sorted")
endif()
```
```cmake
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL muhtari__check_inventory)   # line 69
```

**Adaptations:** the inventory file is fixed at `tests/inventory/fast.txt` (drop `MUHTARI_INVENTORY`, line 29). `skips.txt` is not part of registration equality. Generate `fast.txt` with `LC_ALL=C sort` (tab sorts before `.`, so `x.y` precedes `x.y.self_test`, as in muhtari's `tests/inventory/hygiene.txt`).

---

### `core/build_id.c.in` and the configure step (config, transform)

**Analog:** `~/projects/muhtari/cmake/version.h.in` and `configure_file(cmake/version.h.in ${PROJECT_BINARY_DIR}/generated/muhtari_version.h @ONLY)` (`CMakeLists.txt` line 51).

**Pattern** (`version.h.in`, whole file is 9 lines): a template with `@VAR@` placeholders, configured into `${PROJECT_BINARY_DIR}/generated/` and put on a private include path of the core targets.

**Adaptations (no muhtari equivalent for these parts):** feed it git rev (`execute_process(git rev-parse HEAD)`), dirty flag, `CMAKE_C_COMPILER_ID`/`VERSION`, `CMAKE_SYSTEM_NAME/PROCESSOR`, fallback `nogit`; emit a `const char` string (a `static const char[]` or `const char *const` of integer/char data only, no writable data, so `abi.cmake globals` stays green). `--workflow` re-runs configure so the id refreshes each check run (RESEARCH Pattern 8). Never `__DATE__`/`__TIME__`.

---

### `scripts/check.sh` (script driver, batch)

**Analogs:** no muhtari equivalent (muhtari's job is split across `ci.yml` steps). Shape from `~/projects/libfuturo/scripts/check.sh` lines 29-35 (identity printing); idioms from `~/projects/muhtari/scripts/fetch-pin.sh` lines 13-14, 38-43.

**Identity printing** (libfuturo lines 29-35, convert to POSIX sh; no `[[`, no arrays, no `$SECONDS`, no `local`):
```bash
printf 'CMake: %s (%s)\n' "$(command -v cmake)" "$(cmake --version | head -n 1)"
printf 'GitHub runner image: ImageOS=%s ImageVersion=%s\n' "${ImageOS:-not-provided}" "${ImageVersion:-not-provided}"
```

**Skeleton assembled from those idioms (not an existing file):**
```sh
#!/bin/sh
set -eu
set -f
t0=$(date +%s)                         # per-step wall time with date +%s, not $SECONDS
step() { printf '== %s\n' "$1"; }
# 1 identities: cc, cmake, ninja, shellcheck -V, git, cc65 pin row
# 2 for each pins.txt row: sh scripts/fetch-pin.sh NAME build/pins/NAME
# 3 cc65: make -C ... bin ; make -C ... lib TARGETS=lynx   (two invocations, F1)
#   stamp = pin sha256 + recipe version, written only after both makes and the probe succeed;
#   on a cache hit print the stored seconds and re-run the probe hash
# 4 shellcheck -s sh scripts/*.sh
# 5 cmake --workflow --preset check
# 6 cmake -DJUNIT=build/check/junit.xml -DINVENTORY=tests/inventory/fast.txt -P tests/cmake/check_junit.cmake
# 7 sh scripts/hygiene.sh --tree ; sh scripts/scan-binaries.sh --artifacts build   (after the workflow, F2)
```
`jobs=$(getconf _NPROCESSORS_ONLN)` works on macOS and Linux. Exit non-zero on any failed step with a message naming the step. Run the artifact-zone scan after the workflow, never during it (concurrent test writes).

---

### `scripts/fetch-pin.sh` (script, file-I/O)

**Analog:** `~/projects/muhtari/scripts/fetch-pin.sh` (67 lines, exact)

**Strict mode and row lookup** (lines 13-24, 29-32):
```sh
set -eu
set -f
name=$1
dest=$2
pins=${PINS:-$(dirname "$0")/../pins.txt}
row=$(awk -v n="$name" '$1 == n { print; exit }' "$pins")
# shellcheck disable=SC2086 # split the row into its four fields
set -- $row
url=$3
want=$4
```

**Hash before use** (lines 34-47):
```sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
file=$tmp/${url##*/}
curl -fsSL --retry 3 -o "$file" "$url"
if command -v sha256sum >/dev/null 2>&1; then got=$(sha256sum "$file"); else got=$(shasum -a 256 "$file"); fi
got=${got%% *}
if [ "$got" != "$want" ]; then
  echo "fetch-pin: $name: sha256 mismatch: pins.txt has $want, the download is $got" >&2
  exit 1
fi
```
Only after this does line 49-51 `mkdir -p "$dest"` and `tar -xf "$file" -C "$dest" --strip-components=1` run, so DEST is never created on a mismatch.

**Adaptations:** both pins are `.tar.gz`; the `.zip` branch (lines 52-64) and `${CMAKE:-cmake}` can be dropped. Extend the mismatch message with the remedy from RESEARCH (fallback identity is `git clone` + `git rev-parse HEAD`). `curl` accepts `file://`, and muhtari's own control already relies on it (see `tests/hygiene/tests.cmake` lines 61-71), so RESEARCH assumption A1 has owner precedent. Pin rows use the commit-SHA archive URL, never the tag URL.

---

### `pins.txt` (config)

**Analog:** `~/projects/muhtari/pins.txt`

**Row format** (lines 1-2 header, rows from line 12): `name version url sha256`, one row per line, `#` comments.
```text
# Every pinned download, one row each: name version url sha256.
# scripts/fetch-pin.sh NAME DEST downloads a row and verifies its SHA-256 before use.
rcheevos 12.1.0 https://github.com/RetroAchievements/rcheevos/archive/6755915f2bdf6c11c83b3e0e68c4623a39c0dabe.tar.gz c8bb9f66...
```
The rcheevos row (line 50) is the precedent for a GitHub commit-archive URL.

**Adaptations:** two rows, `cc65 eebf54af545f4ae52f8489a2f6d4d5f93e7942d2 .../archive/<sha>.tar.gz d3e4bfa5...c8a1` and `lynx-tests b583dc43d532d3c41563a6fa35420614fbcead89 ... a70edf38...ab3e` (full digests are in RESEARCH "Version verification"; recompute once and compare). Keep muhtari's bump-policy comment style, shortened.

---

### `scripts/hygiene.sh` (script, batch / file-I/O)

**Analog:** `~/projects/muhtari/scripts/hygiene.sh` (178 lines)

**Locale and quoting** (lines 18-22): `LC_ALL=C; export LC_ALL` and `git() { command git -c core.quotePath=false "$@"; }`, then `cd "$(git rev-parse --show-toplevel)"`.

**Patterns** (lines 33-38; this is the PII layer):
```sh
HOME_PATH='(/Users/|/home/)[A-Za-z0-9._-]+|[A-Za-z]:\\Users\\[A-Za-z0-9._-]+|-Users-[A-Za-z0-9._]+-'
HOME_OK='(/Users/|/home/)runner'
EMAIL='[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z][A-Za-z]+'
EMAIL_OK='@users\.noreply\.github\.com|noreply@github\.com|noreply@anthropic\.com|@example\.(com|org|net)|git@github\.com'
```
**Leak test and file loop** (lines 46, 82-90). `check_files` iterates `for f in $1` with `IFS` set to a newline and `set -f`; it does NOT pipe into `while read`, so the `fail=1` set by `found` survives (RESEARCH Pitfall 6). Keep this shape.
```sh
leaks() { grep -I -E -e "$1" | grep -v -E -e "$2" | grep -q .; }
```
**Identity and history** (lines 105-110, 143-171): `check_identity` allows only `*@users.noreply.github.com | noreply@github.com`; `--history` loops `git rev-list`, uses `git grep` per commit and also scans commit messages with `git log -1 --format=%B`.

**Adaptations:**
- Delete `check_stop_policy` (92-103) and `check_branch` (112-120) and their call sites (lines 134-135, 141); delete `ROM_NAME`, the `.a78` magic block (62-66), `GPL_TEXT`/`CODE_PATH` (42-43, 75-77) and the ROM-name loop in `--history` (168-170).
- Delete `MANIFEST=tests/roms/manifest.txt` and `in_manifest` (lines 27, 49). Replace the NUL-only binary rule (lines 67-72, `grep -I -q ''`) by a call to `scripts/scan-binaries.sh` (F3, hash-keyed manifest lookup).
- Add credential patterns (RESEARCH Pattern 11): `ghp_[A-Za-z0-9]{36}|github_pat_[A-Za-z0-9_]{22,}|gh[ousr]_[A-Za-z0-9]{36}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}` through the same `leaks` helper with an empty allow-list (use a never-matching `$2`).
- Modes: keep `--tree` and `--history`; `--staged` only if a pre-commit hook is added. Run `--history` only when more than one commit exists; CI uses `fetch-depth: 0`.
- Keep `SELF=scripts/hygiene.sh` excluded (line 54). The pattern text does not match itself.
- Negative controls must build the fake token, home path and email from concatenated parts inside a `mktemp -d` tree outside the repo; never commit a literal fake secret (push protection is on).

---

### `scripts/scan-binaries.sh` (script, file-I/O)

**Analog (partial):** `~/projects/muhtari/scripts/hygiene.sh` `check_file` (lines 53-78) for the per-file structure; the detector itself comes from RESEARCH "Binary/non-text detection" because muhtari's `grep -I` rule is the thing F3 replaces.

**Structure to copy:** iterate `git ls-files -z --cached --others --exclude-standard | tr '\0' '\n'` (hygiene.sh line 140), skip symlinks and directories, read each file through one function, and emit findings that name the rule and the file only (hygiene.sh lines 6-10, 29).

**Core rule (RESEARCH Pattern 5 example):**
```sh
LC_ALL=C; export LC_ALL
is_ctl_binary() { [ "$(tr -d '\000-\010\013\016-\037\177' < "$1" | wc -c | tr -d ' ')" != "$(wc -c < "$1" | tr -d ' ')" ]; }
is_bad_utf8()   { ! iconv -f UTF-8 -t UTF-8 < "$1" >/dev/null 2>&1; }
in_manifest()   { grep -s -q "^sha256 = $1\$" "$mdir"/*.manifest; }   # hash-keyed, not path-keyed
```
A non-text file passes only if its SHA-256 equals a manifest `sha256`; a non-text file of exactly 512 bytes fails regardless (`wc -c`). Pin zone `build/pins/`: one visible exclusion plus `find build/pins -type f -size 512c` for the 512 rule. Artifact zone: toolchain output classes by magic (`od -An -tx1 -N4`), everything else must be text or manifested.

**Adaptations:** keep the findings in a temp file or count output lines and `exit` from that (the `cmd | while read` flag loss, Pitfall 6). Reuse the `sha()` helper idiom from `fetch-pin.sh` lines 38-43.

---

### `scripts/validate-kv.sh` (script, transform)

**Analog (partial):** the awk lookup idiom in `~/projects/muhtari/scripts/hygiene.sh` line 49 (`awk -v p="$1" '$1 == p { f = 1 } END { exit !f }' "$MANIFEST"`), and the field-checking style of `~/projects/muhtari/tests/cmake/pins.cmake` (below).

**Rules (RESEARCH Pattern 4):** `key = value`, `#` comments, exactly one ` = ` per line, key `[a-z_]+`, no duplicate, no unknown, no empty values, required keys given by a `-v required="..."` argument so the same script validates manifests and the claim template. `sha256` check by `length($0)==64` and a hex character class (no `{n}` intervals in awk, assumption A2); `evidence_tier` and `startup_tier` against enumerated lists (`T1..T7`, `R`; `A`).

**Adaptation:** none to copy verbatim; model the error text on `pins.cmake` lines 24, 31, 35, 39 (`line N: NAME: what is wrong`).

---

### `tests/cmake/check_junit.cmake` (utility, batch)

**Analog:** `~/projects/muhtari/tests/cmake/check_junit.cmake` (58 lines, exact)

**Core logic** (lines 14-30, 32-54): neutralise list syntax in the XML (`;` `[` `]`), split on `<testcase `, record each name, flag a case unless it matches `status="run"` or contains `<failure|error|skipped>`; then for each inventory line require the name exactly once; zero selected or zero ran is a failure.
```cmake
if(NOT case MATCHES "^[^>]*status=\"run\"" OR case MATCHES "<(failure|error|skipped)")
  string(APPEND bad "\n  ${name}: failed, errored or did not run")
```
**Adaptation (new, about 10 lines):** accept `-DSKIPS=tests/inventory/skips.txt` (`name<TAB>reason`); a `<skipped` testcase is acceptable only if its name is listed there with a non-empty reason. muhtari has no skip declaration (any skipped is bad). Phase 1 has no skips, so `skips.txt` holds a header comment only. Keep `-DLABELS`/`-DEXCLUDE` (lines 38-41) only if a label split is used; otherwise drop.

**Self-test fixtures** (generated at configure time, not committed): a JUnit missing a test, an extra test, a duplicate, a `<skipped/>`; an unsorted inventory; an undeclared skip. Wire each through `expect_fail.cmake` (see below). Run it once before relying on it: RESEARCH marks the command shape as adapted, not run verbatim.

---

### `tests/cmake/expect_fail.cmake` (utility, batch)

**Analog:** `~/projects/muhtari/tests/cmake/expect_fail.cmake` (19 lines, exact)

**Core** (lines 12-18):
```cmake
execute_process(COMMAND ${CMD} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc STREQUAL CODE OR NOT out MATCHES "${REGEX}")
  message(FATAL_ERROR "expect_fail: expected exit ${CODE} and output matching\n  ${REGEX}\ngot exit ${rc} and output\n${out}")
endif()
```
This is the single negative-control runner for every gate: exact exit code plus message regex, so a crash or typo cannot pass (a bare `WILL_FAIL` would). Copy verbatim.

---

### `tests/cmake/no_skip.cmake` (utility, batch)

**Analog:** `~/projects/muhtari/tests/cmake/no_skip.cmake` (38 lines, exact)

**Self-exclusion trick** (lines 11-13): the forbidden property names are built by concatenation so the file does not match itself.
```cmake
set(skip SKIP_)
set(dis DIS)
set(rules ${skip}RETURN_CODE ${skip}REGULAR_EXPRESSION ${dis}ABLED)
```
Scans `tests/**/*.cmake`, `cmake/*.cmake`, `CMakeLists.txt`. Copy verbatim; register `policy.no_skip` with `-DROOT=${CMAKE_SOURCE_DIR}`. Add a self-test on a fixture dir containing a forbidden property (muhtari does not have one for this script in the files read; the `hygiene.no_float.self_test` pattern below applies).

---

### `tests/cmake/no_float.cmake` (utility, batch) and `tests/cmake/core_includes.cmake` (utility, batch)

**Analog:** `~/projects/muhtari/tests/cmake/no_float.cmake` (59 lines, exact for float; shape for includes)

**Float scan core** (lines 37-47): a decimal/hex float literal, `float`/`double` token, `<math.h>` after hex integers and "section N.N" are removed.
```cmake
string(REGEX REPLACE "0[xX][0-9a-fA-F]+|section [0-9.]*[0-9]|SPDX-License-Identifier: [A-Za-z0-9 .+()-]*" "" s " ${line} ")
if(line MATCHES "0[xX][0-9a-fA-F]*\\.?[0-9a-fA-F]*[pP][+-]?[0-9]" OR s MATCHES "[^A-Za-z0-9_](float|double)[^A-Za-z0-9_]" ...)
```
Invoked as `cmake "-DROOTS=<core>;<include>" -P no_float.cmake` (hygiene tests.cmake lines 29-32; note `$<SEMICOLON>` between roots).

**Verified trap (checked this session on a scratch copy):** running this script over muhtari's `runner/sha256.c` fails on lines 1-2, the header comment "sections 4.1.2, 4.2.2, 5.1.1, 5.3.3 and 6.2", because the plural "sections" is not stripped and `4.1.2` looks like a decimal. muhtari scanned only `src/` and `include/`, never `runner/`. When `sha256.c` moves into `core/` (F4), reword that comment (for example "FIPS 180-4, section 4.1.2" per mention, or cite sections without dots). The same file otherwise compiles clean with `-std=c17 -pedantic-errors -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector`, and `nm -P` of that object lists only `_bzero` and `_memcpy` as undefined and no writable data. Comments in `core/` also must avoid the words float/double and decimal numbers like 1.5.

**`core_includes.cmake` (new, layer 5a):** same file loop as `no_float.cmake` lines 14-35 (`file(GLOB_RECURSE ... "${root}/*.c" "${root}/*.h")`, neutralise list characters, split on `\n`, report `file:line: text`, count scanned files, `FATAL_ERROR` listing all). Per line match `^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]`; allow `stdint.h stddef.h stdbool.h string.h`, plus quoted headers that exist inside `core/` or `include/lynxplosion.h`. Not allowed: `assert.h` (adds `__assert_*`), `stdio.h`, `stdlib.h`, `math.h`, `time.h`, `limits.h` unless the planner adds it deliberately.

**Negative controls (hygiene.no_float.self_test pattern, `tests/hygiene/tests.cmake` lines 33-43):**
```cmake
file(WRITE ${hygiene_fixture}/sample.c "int y = 0x1E5;\nx = 1.5;\ndouble d;\n#include <math.h>\n")
set(hygiene_cmd ${CMAKE_COMMAND} -DROOTS=${hygiene_fixture} -P .../no_float.cmake)
string(REPLACE ";" "$<SEMICOLON>" hygiene_cmd "${hygiene_cmd}")
lynx_add_test(NAME hygiene.no_float.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${hygiene_cmd}" -DCODE=1 "-DREGEX=sample.c:2: x = 1.5.*no_float: 3 lines" -P .../expect_fail.cmake
  LABELS hygiene TIMEOUT 30)
```
The fixture includes lines that only look like a float (`0x1E5`, `double_free`) so the control also proves no false positive. Add the same for `core_includes` (`#include <stdio.h>` fixture).

---

### `tests/cmake/abi.cmake` (utility, batch) and `tests/abi/tests.cmake` (test, batch)

**Analog:** `~/projects/muhtari/tests/cmake/abi.cmake` (111 lines) and `~/projects/muhtari/tests/abi/tests.cmake`

**Allowlist with per-platform helpers** (abi.cmake lines 41-49, rows at 27-40):
```cmake
set(allowed memcpy memmove memset memcmp)
foreach(row IN LISTS helpers)
  string(REGEX MATCH "^([^:]*):(.*)$" unused "${row}")
  if(SYSTEM MATCHES "${CMAKE_MATCH_1}")  ...  list(APPEND allowed ${names})
```
**nm parse** (lines 52-78): `nm -P` lines match `^([^ ]+) ([A-Za-z])( |$)`; on `Darwin/` strip one leading `_`; type `U` is undefined; types `B b D d C` are writable; `[A-Z]` types are external.

**Three modes** (lines 81-105): `undefined` (undefined minus defined minus allowed), `globals` (any writable symbol), `symbols` (declared `lynx_*(` in the header equal defined externals both ways; every other external must start `lynx__`).

**Wiring** (`tests/abi/tests.cmake` lines 12-30, 34-46):
```cmake
set(abi_args -DNM=${abi_nm} "-DSYSTEM=${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
foreach(mode IN ITEMS undefined globals symbols)
  lynx_add_test(NAME abi.${mode}
    COMMAND ${CMAKE_COMMAND} -DMODE=${mode} -DLIB=$<TARGET_FILE:lynx_core_plain> ${abi_args}
      -DHEADER=${PROJECT_SOURCE_DIR}/include/lynxplosion.h -P ${abi_scripts}/abi.cmake
    LABELS abi TIMEOUT 30)
endforeach()
```
Negative controls: fixture libs built with the core's own options (`target_compile_options(... ${abi_options})` from `get_target_property(abi_options ...)`, lines 14-23), one per fixture, each run through `expect_fail.cmake` with a regex such as `unexpected[^:]*symbol:.*abort` and `writable[^:]*symbol:.*counter` (lines 34-46). The regex tolerates a line break because CMake wraps long messages.

**Adaptations:**
- Point `LIB` at `lynx_core_plain` (F5), never at the sanitized library.
- Trim `helpers` to the lanes that exist: keep `^Darwin/:bzero __bzero` and `^Linux/(x86_64|amd64|AMD64)$:_GLOBAL_OFFSET_TABLE_` (A8: harmless if unused); drop the Windows `__ImageBase` and the PowerPC rows (lines 35-40).
- Rename the prefix regexes at lines 90, 93, 94, 100.
- Keep `-DNM` from `CMAKE_NM`; muhtari falls back to `find_program(llvm-nm nm)` when empty (lines 6-11), keep that fallback.
- Fixtures `tests/abi/libc_fixture.c` (calls `abort`) and `global_fixture.c` (a `static uint32_t counter` written by a function) copy exactly, renaming `muhtari__abi_*` to `lynx__abi_*`. Add two more controls in the same style: a fixture with `float`/`double` (feeds `no_float`) and one with `#include <stdio.h>` (feeds `core_includes`); `nofp_fixture.c` is built only on muhtari's GCC `-mgeneral-regs-only` lane and is not needed here.
- Header self-containment test: copy `tests/abi/tests.cmake` lines 127-148 (an `OBJECT EXCLUDE_FROM_ALL` library compiled alone, driven by `${CMAKE_COMMAND} --build ... --target`, `RESOURCE_LOCK build_tree`) with only the C17 half unless a C++ compiler is configured.

---

### `tests/cmake/pins.cmake` (utility, batch)

**Analog:** `~/projects/muhtari/tests/cmake/pins.cmake` (49 lines, exact)

Rules (lines 18-41): skip comments and blanks; exactly four fields via `separate_arguments(fields UNIX_COMMAND ...)`; unique names; `url MATCHES "^https://"`; digest `^[0-9a-f]+$` and length 64; at least one row. Collect findings as `line N: NAME: ...` and fail once with all of them. Copy verbatim.

**Negative controls** (`tests/hygiene/tests.cmake` lines 55-71): `policy.pins` runs the script on `pins.txt`; `policy.pins.self_test` runs `fetch-pin.sh` with `PINS=<fixture>` whose row is a `file://` payload with 64 zeros as digest, expecting exit 1 and `payload: sha256 mismatch`:
```cmake
file(WRITE ${hygiene_pins}/pins.txt "payload 1 file://${hygiene_pins}/payload.txt ${hygiene_zeros}\n")
set(hygiene_cmd ${CMAKE_COMMAND} -E env PINS=${hygiene_pins}/pins.txt ${LYNX_SH} .../scripts/fetch-pin.sh payload ${hygiene_pins}/dest)
```
**Additions required by RESEARCH:** also assert DEST does not exist afterwards (muhtari's control does not), and add a `policy.pins.self_test` family for an `http://` URL, a short digest and a duplicate name (each through `pins.cmake` and `expect_fail.cmake`).

---

### `tests/gates/tests.cmake` (test registration, batch)

**Analog:** `~/projects/muhtari/tests/hygiene/tests.cmake` (role-match; most of it is muhtari-specific and not to be copied)

Copy only the repeating unit: **real run, then a self test that feeds the same script a built fault and requires the exact failure.** Units to lift: `hygiene.tree` (lines 5-7, run `hygiene.sh --tree`), `policy.pins` and its self test (55-71), `hygiene.no_float` and its self test (29-43), and the "edit a copy, expect the named finding" pattern with `hygiene_softlist_edit` (lines 360-375) which stops configure if the edit changes nothing, so a control can never compare equal inputs. Lynxplosion's tests of this shape (names are proposals, register each in `fast.txt`): `hygiene.tree`, `hygiene.manifests`, `hygiene.manifests.self_test`, `hygiene.firmware_scan`, `hygiene.firmware_scan.self_test`, `hygiene.pii.self_test`, `core.includes`, `core.no_float`, `core.nm_undefined`, `core.nm_globals`, `inventory.*.self_test`, `docs.policy`, `docs.claim_template`, `toolchain.probe`, `policy.pins`, `policy.pins.self_test`, `policy.no_skip`.

Do not copy: `hygiene.format`, `hygiene.analyzer` (clang tooling deferred), `policy.title`, `ci.classify`, `policy.ci_required`, `policy.dependabot`, `policy.publish_*`, `policy.cartdb_*`, `policy.softlist_*`, `hygiene.timeout_comments`.

Because muhtari uses `string(REPLACE ";" "$<SEMICOLON>" cmd "${cmd}")` before passing `-DCMD=` (every self test above), keep that idiom exactly; without it CMake splits the command list at the wrong level.

---

### `tests/inventory/fast.txt` and `skips.txt` (config)

**Analog:** `~/projects/muhtari/tests/inventory/hygiene.txt`

**Format** (header lines 1-2, then rows):
```text
# Every test the hygiene preset registers: name<TAB>labels.
# Sorted; configure fails when this differs from what is registered.
hygiene.no_float	hygiene
policy.pins	hygiene,policy
```
Tab-separated, labels comma-joined and sorted, lines sorted, `#` comments. `skips.txt`: header comment only in Phase 1 (`name<TAB>reason` when used).

---

### `tests/hygiene/scan_control.sh` (test, file-I/O)

**Analog:** `~/projects/muhtari/tests/hygiene/history_control.sh`

**Isolated-repo control shape** (lines 5-14, 24-32):
```sh
set -eu
work=$2
rm -rf "$work"; mkdir -p "$work/..."; cd "$work"
git init -q .
git config user.name fixture   # identity set locally; use a noreply-style address
git add . ; git commit -q -m add
...
if sh "$hygiene" ... 2> err; then echo "history_control: an unlisted binary was accepted" >&2; exit 1; fi
grep -q 'binary file not in manifest' err
```
Pass the real script and a work directory as arguments, create a fresh repo, assert the pass case first, then the fail case with the named rule in stderr.

**Adaptations (FND-04):** build the work dir with `mktemp -d` OUTSIDE the repo so the artifact-zone scan never sees the seeded file, and trap-clean it. Seed two deterministic fakes (never `/dev/urandom`): a NUL byte plus 511 `A` bytes, and 512 bytes of `128 + (i % 127)` with no NUL. Assert rejection of each; remove them and assert pass; also assert a manifested 100-byte binary passes and a manifested 512-byte binary still fails. Same file hosts the PII/credential controls with tokens built from concatenated parts.

---

### `tests/check.h` (utility)

**Analog:** `~/projects/muhtari/tests/check.h` (48 lines, exact)

`CHECK(cond)`, `CHECK_EQ_U64`, `CHECK_EQ_HEX`, and `CHECK_DONE()` which ends `main` with exit status 0 or 1; a failed check prints `file:line` to stderr and continues so one run shows every failure (lines 1-4, 10, 46). Rename the include guard (`MUHTARI_CHECK_H`) only.

---

### `tests/header/header.c` (test, none)

**Analog:** `~/projects/muhtari/tests/header/header.c` (55 lines, exact pattern)

Includes only the public header, then proves its shape at compile time:
```c
#include "muhtari.h"
_Static_assert(sizeof(muhtari_status) == 4, "status is 32 bits");
static muhtari_status (*const create_fn)(const muhtari_config *, muhtari **) = muhtari_create;
```
Compiling the file is the test (`abi.header.c17`). For lynxplosion: static asserts on the status type width and stop-reason enum values, plus a function-pointer table pinning every declared signature, and a `(void)` use of each to avoid `-Wunused`. Built with the same flags as the core (`-pedantic-errors`); the `tests/abi/tests.cmake` lines 127-148 wiring applies.

---

### `tests/api/test_api.c`, `tests/det/test_det.c` and their `tests.cmake` (test, request-response / batch)

**Analog:** `~/projects/muhtari/tests/core/test_core.c` and `~/projects/muhtari/tests/core/tests.cmake`

**One executable, one case per CTest entry** (test_core.c lines 3698-3710):
```c
int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: muhtari-test-core <case>\n"); return 2; }
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        if (strcmp(argv[1], cases[i].name) == 0) { cases[i].run(); CHECK_DONE(); }
    }
    fprintf(stderr, "muhtari-test-core: unknown case %s\n", argv[1]);
    return 2;
}
```
**Registration loop** (`tests/core/tests.cmake` lines 5-22):
```cmake
add_executable(muhtari-test-core ${CMAKE_CURRENT_LIST_DIR}/test_core.c ...)
target_link_libraries(muhtari-test-core PRIVATE muhtari::muhtari)
set_target_properties(... RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
foreach(case IN ITEMS ...)
  muhtari_add_test(NAME core.${case} COMMAND muhtari-test-core ${case} LABELS core portable TIMEOUT 10)
endforeach()
```
An unknown case exits 2, so a typo in `tests.cmake` fails instead of passing.

**Refusal oracle (API-02/05/07):** whole-instance `memcmp` before and after. muhtari does this at `test_core.c` line 1244 (`CHECK(memcmp(m, before, sizeof *before) == 0);` after a bad-argument call) and for peers at 921. With caller-provided memory lynxplosion can `memcmp` the entire buffer (`_Alignas(max_align_t)` test buffers). Also run one accepted load to show a mutation is detectable.

**Adaptations:** register the proposed `api.*` and `det.*` names from RESEARCH Pattern 7/9 and add each to `fast.txt`. Corruption flip tests use exhaustive header flips plus a stride over the payload (assumption A4, measure first). `det.two_runs` launches the runner twice as separate processes through `${CMAKE_COMMAND} -P` or `sh` (muhtari's `tests/runner/tests.cmake` does similar; not read in depth, plan to write this one).

---

### `include/lynxplosion.h` (public header, request-response)

**Analog:** `~/projects/muhtari/include/muhtari.h`

**Header skeleton** (lines 1-14, 15-18, 38):
```c
#ifndef MUHTARI_H
#define MUHTARI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t muhtari_status;
enum { MUHTARI_STATUS_OK = 0, MUHTARI_STATUS_ERR_ARGUMENT = 1, ... };
typedef struct muhtari muhtari;            /* opaque handle, line 38 */
```
**Prototypes** are plain C with a status return and out-parameters, `NULL`-safe destroy (lines 79-86), for example `muhtari_status muhtari_create(const muhtari_config *cfg, muhtari **out);`.

**Adaptations (the CONTEXT contract differs from muhtari here):**
- Drop `muhtari_allocator` and `muhtari_create/destroy` (lines 40-49, 80-81): the core never calls malloc. Replace with `size_t lynx_machine_size(void)`, an alignment query, and `lynx_machine_init(void *mem, size_t size, ..., lynx_machine **out)` validating NULL, size and alignment.
- Add `#include <stdbool.h>` only if used; no other includes (the include allowlist).
- Status values and prefix are Claude's discretion; fix them before copying `abi.cmake`. Add a stop-reason enum `BUDGET, FRAME, WATCH, HALT` and `uint64_t` budget/consumed (API-03).
- muhtari's struct size-tag mechanism (`muhtari__check_size`, `src/instance.c` lines 20-43) is optional for Phase 1; adopt only if a config/versions struct is exposed. `muhtari_get_versions` (lines 45-66) is the pattern for a `lynx_get_versions` that returns the state-format version and build-id string.
- Keep `MUHTARI_API_VERSION`-style `_Static_assert` coverage in `tests/header/header.c`.

---

### `core/machine.c` and `core/internal.h` (core, request-response)

**Analog:** `~/projects/muhtari/src/instance.c`

**Argument checks first, status return, then initialise** (lines 69-100 in `muhtari_create`):
```c
if (cfg == NULL || out == NULL) { return MUHTARI_STATUS_ERR_ARGUMENT; }
...
memset(m, 0, sizeof *m);        /* zero whole struct: no padding leaks into digests (Pitfall 8) */
muhtari__power_on(m);           /* reset path = init path: one function applies the power-on policy */
```
Reset/power-on: `muhtari__power_on` (from line 102) is the precedent for "reset re-applies the declared policy" (API-04); in lynxplosion it applies policy id 0 (fixed 0x00) or the seeded SplitMix64 fill, and `lynx_machine_init` calls the same function.

**Overflow-safe address math** (`src/state.c` `state_buffer_disjoint`, line 1141 onward): `if (bytes > UINTPTR_MAX - start) return 0;`. Use the same subtraction form for load: `len > 0x10000u - addr`, never `addr + len`.

**Adaptations:** no allocator, no heap, no file-scope writable data (`abi.cmake globals` enforces); the instance buffer is the only mutable state. Serialize fields little-endian by hand, never `memcpy` a struct (Pitfall 8). SplitMix64 takes its state by pointer (the original uses a static, RESEARCH Pattern 7). Advance quantum, stop-reason, `UINT64_MAX` guard: no analog, per RESEARCH Pattern 7. Prefix internals `lynx__`.

---

### `core/snapshot.c` (core, transform)

**Analog:** `~/projects/muhtari/src/state.c` lines 1258-1326 (`muhtari_save_state`, `muhtari_load_state`) and the LE helpers at lines 130-146.

**Validate every field, then apply** (lines 1295-1326, condensed):
```c
if (m == NULL || buffer == NULL) return MUHTARI_STATUS_ERR_ARGUMENT;
size_t expected = muhtari_state_size(m);
if (expected == 0u || bytes != expected) return MUHTARI_STATUS_ERR_INVALID_STATE;
if (memcmp(input, magic, sizeof magic) != 0 || state_get_u32(input + 12u) != STATE_HEADER_BYTES || ...)
    return MUHTARI_STATUS_ERR_INVALID_STATE;
if (state_get_u32(input + 8u) != STATE_FORMAT ...) return MUHTARI_STATUS_ERR_UNSUPPORTED_STATE;
if (state_get_u32(input + 24u) != state_crc32(input, expected)) return MUHTARI_STATUS_ERR_INVALID_STATE;
if (memcmp(input + 28u, m->state_identity, sizeof m->state_identity) != 0) return MUHTARI_STATUS_ERR_STATE_MISMATCH;
if (state_walk(m, NULL, STATE_PUBLIC_VALIDATE, ...) == 0) return MUHTARI_STATUS_ERR_INVALID_STATE;
muhtari__state_payload_apply(m, input + STATE_HEADER_BYTES, payload);   /* cannot fail */
```
**LE writers** (lines 141-146, 130):
```c
static void state_put_u32(uint8_t *bytes, uint32_t value)
{ bytes[0] = (uint8_t)value; bytes[1] = (uint8_t)(value >> 8); bytes[2] = (uint8_t)(value >> 16); bytes[3] = (uint8_t)(value >> 24); }
```
Save side (lines 1258-1293): fixed 8-byte magic, then format, header length, total length, then payload written by one visitor; capacity and disjointness checked before any write.

**Adaptations:** muhtari's integrity field is CRC-32; lynxplosion's header carries payload SHA-256 (via `core/sha256`) plus magic, format version, core build id (the full string, compared whole), profile id, content SHA-256, RAM policy, seed, payload length, then payload. All checks complete before the single apply step; refusal leaves the instance buffer byte-identical (tested by whole-buffer `memcmp`). Distinct statuses per failure class are Claude's discretion but keep "unsupported version" separate from "invalid" as muhtari does.

---

### `core/sha256.c`, `core/sha256.h` (core util, transform)

**Analog:** `~/projects/muhtari/runner/sha256.c` (132 lines) and `sha256.h` (22 lines), exact, relocated to `core/` (F4).

**API** (header lines 8-20): `muhtari__sha256` struct (`h[8]`, `block[64]`, `used`, `total`), `_init`, `_update(s, data, len)`, `_final(s, digest[32])`, `_hex(digest, hex[65])` with lowercase hex and NUL.
**Constant tables** (c lines 9-20, 71-74): `static const uint32_t K[64]` and a function-local `static const uint32_t H0[8]`, both read-only (`r`/`s` in `nm`), so `abi.cmake globals` passes. The only libc use is `memcpy` and `memset` (lines 75, 89, 107, 111).

**Adaptations:** rename `muhtari__sha256*` to `lynx__sha256*`; header guard; reword the header comment at lines 1-2 (see the `no_float` trap above); keep the `Written from the standard` provenance line. Verify with NIST vectors (`abc`, empty string `e3b0c442...b855`, a 56-byte and a multi-block message) against `shasum -a 256` in a `core.sha256` test (assumption A3; muhtari has the empty-string vector only inside digest tests, not as a standalone core test). The runner either links `lynx_core` or compiles the same file; one copy.

---

### `runner/main.c` and `runner/bundle.c` (runner, file-I/O)

**Analog (role-match for CLI and hash printing):** `~/projects/muhtari/runner/main.c`

**Hash print idiom** (lines 193-198, 249-264):
```c
muhtari__sha256 s;
muhtari__sha256_init(&s);
muhtari__sha256_update(&s, p, n);
muhtari__sha256_final(&s, digest);
muhtari__sha256_hex(digest, hex);
printf("sha256%s %s\n", suffix, hex);
```
CLI style: a synopsis string, `muhtari-run: <reason>` prefixed stderr messages with a usage dump (line 124), an exit-status table (`muhtari__exit_statuses`, printed at line 133).

**Adaptations:** the runner's output is a bundle directory: `run.manifest` (`key = value`, same parser as fixture manifests), state dumps as lowercase hex text lines (digest is over canonical bytes, file is text, F2), and `digests.txt` (`name<space>sha256`, sorted by byte comparison inside the runner). Observation digest = SHA-256 of the sorted `obs.*` lines; bundle digest = SHA-256 of the whole sorted `digests.txt` (includes `run.manifest`'s hash, so build identity enters it). Never write timestamps, hostnames, absolute paths, wall time, flags, pointers or padding. Print every digest labelled "repeatability (R), not accuracy". No muhtari analog for the bundle writer or `run.manifest`.

---

### `.github/workflows/ci.yml` (config, batch)

**Analog:** `~/projects/muhtari/.github/workflows/ci.yml` (use lines 4-8, 44-46, 57-59, 77-82 only; the file is 1087 lines of muhtari-specific jobs)

**Excerpts to reuse:**
```yaml
permissions:
  contents: read
concurrency:
  group: ci-${{ github.workflow }}-${{ github.event.pull_request.number || github.sha }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}
...
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        with:
          persist-credentials: false
          fetch-depth: 0
```
The SHA-with-version-comment pin and the `persist-credentials: false` shape are the owner convention (RESEARCH verified `3d3c42e5...` as v7.0.1, the same SHA muhtari pins).

**Adaptations:** one job `check`, `runs-on: ubuntu-24.04` (muhtari uses `ubuntu-26.04`; CONTEXT fixes 24.04), `timeout-minutes: 15` with a trailing `# provisional` comment (muhtari convention, lines 70, 100), step `run: scripts/check.sh` and nothing else. Triggers `pull_request` and `push` to `main`; drop `workflow_dispatch`/`workflow_call` inputs, the `changes` classifier job, `actions: read`, `defaults.run.shell: bash`, artifact uploads and the ruleset roll-up. Never `pull_request_target`. Fetch pins happen inside `check.sh`, not as CI steps (muhtari does them as CI steps, lines 108-112; CONTEXT says `check.sh` and nothing else).

---

### `docs/policy/clean-room.md`, `docs/policy/exposure-record.md`, provenance notice (doc)

**Analog:** `~/projects/muhtari/PROVENANCE.md` section 1 "Rules" (role-match) and `ASSET_POLICY.md` "Never" list.

**Rule style to copy:** short bold-lead bullets, for example "Copyleft and unlicensed source stays closed. GPL/LGPL source ... is never read past its licence header, never pasted into an assistant's context", "AI assistance is disclosed in a co-author trailer or the PR description", "Firmware and game bytes never enter". Lynxplosion's rule (CONTEXT O6): implementers do not open Gearlynx, MAME, Holani or Beetle source; exposure is recorded per source (what, when, which research file). Status banner "DRAFT pending owner decision O6". `docs.policy` test checks the banner and one exposure row per named emulator.

**Exposure record:** no analog. Seed rows from the files RESEARCH names (`ARCHITECTURE.md` line 14, `STACK.md`, `FEATURES.md`, `SUMMARY.md` 182-184 and 462); build the table from those files, not from memory, and do not open any emulator source to do it.

**Provenance notice for copied helpers:** model on `PROVENANCE.md` section 5 (notices); list each copied muhtari file, the commit `7fd0386`, MIT, owner-authored.

---

### `fixtures/manifests/*.manifest` and `fixtures/toolchain/probe.c` (fixture config)

**Analog:** `~/projects/muhtari/tests/roms/manifest.txt` (role-match; format changes)

muhtari's manifest is one tab-separated table (`path<TAB>source<TAB>pin<TAB>licence<TAB>sha256<TAB>notice`, header comment lines 1-12), keyed by path. Lynxplosion's decision is different on two counts: one `key = value` file per fixture, and "listed in a manifest" means the file's SHA-256 equals some manifest `sha256` (path-independent, works for build-tree outputs). Required keys: `source revision recipe sha256 licence notices allowed_use startup_tier profile evidence_tier` (profile `none`, startup tier `A`, tier `R` or `T3`; `independence` optional).

**Probe (no analog):** commit the 3-line project-authored source (`unsigned char g_results[4]; void main(void){ g_results[0]=1; for(;;){} }`) with a manifest whose `sha256` is the expected `.lnx` hash. The hash in RESEARCH is for a scratch source; recompute it from the committed source and flags in the plan's first task.

---

### `README.md`, `.planning/PROJECT.md` (doc)

`~/projects/muhtari/README.md`: partial, tone only (plain, states what exists and what is not claimed, "No claim of universal ..."). Lynxplosion's README must state that the project is all rights reserved and that the licence decision (O5) is open, and must not claim compatibility. `.planning/PROJECT.md`: add the O8 Key Decision line (CONTEXT, RESEARCH open question 7).

---

## Shared Patterns

### Negative control for every gate
**Source:** `~/projects/muhtari/tests/cmake/expect_fail.cmake` lines 12-18, wiring at `~/projects/muhtari/tests/hygiene/tests.cmake` lines 37-43
**Apply to:** every gate (pins, manifests, claim, inventory, include allowlist, float, nm, firmware scan, PII, no_skip)
```cmake
string(REPLACE ";" "$<SEMICOLON>" hygiene_cmd "${hygiene_cmd}")
lynx_add_test(NAME <gate>.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${hygiene_cmd}" -DCODE=1 "-DREGEX=<exact finding>" -P .../expect_fail.cmake
  LABELS <label> TIMEOUT 30)
```
Exact exit code plus message regex; fixtures are built at configure or test time under the build dir or a `mktemp -d` tree outside the repo, with deterministic content.

### Fail-closed scan loop (no subshell flag loss)
**Source:** `~/projects/muhtari/scripts/hygiene.sh` lines 29, 82-90
**Apply to:** `hygiene.sh`, `scan-binaries.sh`, `validate-kv.sh`
```sh
found() { printf 'hygiene: %s: %s\n' "$1" "$2" >&2; fail=1; }
check_files() { old_ifs=$IFS; IFS='
'; set -f; for f in $1; do check_file "$f" "$2"; done; set +f; IFS=$old_ifs; }
...
exit "$fail"
```
A `for` over a newline list, never `cmd | while read`. A name that contains a newline splits into parts that name no file, and each part fails closed. Findings print the rule and the file, never the matched text.

### SHA-256 tool selection
**Source:** `~/projects/muhtari/scripts/fetch-pin.sh` lines 38-43
**Apply to:** `fetch-pin.sh`, `scan-binaries.sh`, `check.sh` (probe hash), any sh helper that hashes
```sh
if command -v sha256sum >/dev/null 2>&1; then got=$(sha256sum "$file"); else got=$(shasum -a 256 "$file"); fi
got=${got%% *}
```

### Core link surface and compile options
**Source:** `~/projects/muhtari/CMakeLists.txt` lines 53-73 and `tests/cmake/abi.cmake` lines 41-49
**Apply to:** `lynx_core_plain`, `lynx_core`, abi fixtures
Core links only `memcpy memmove memset memcmp` (plus Darwin `bzero`); no stack protector, no fortify, no build path in objects; read-only integer tables only; internals named `lynx__*`.

### Refusal leaves state byte-identical
**Source:** `~/projects/muhtari/tests/core/test_core.c` line 1244; `~/projects/muhtari/src/state.c` lines 1295-1326
**Apply to:** `core/machine.c`, `core/snapshot.c`, `tests/api/*`
Validate every input, return a status before the first write, then apply once; prove with a whole-buffer `memcmp`.

### Inventory-first test registration
**Source:** `~/projects/muhtari/cmake/MuhtariTests.cmake` lines 13-24, 28-69; `~/projects/muhtari/tests/tests.cmake` lines 1-6
**Apply to:** every CTest entry in every `tests/*/tests.cmake`
Register only through `lynx_add_test`; `include()` (never `add_subdirectory`) each test file; every new name is appended to `tests/inventory/fast.txt` in `LC_ALL=C sort` order in the same change, or configure fails.

### Public-repo hygiene in everything written
**Source:** `~/projects/muhtari/scripts/hygiene.sh` lines 33-38
**Apply to:** all new files, including this planning tree
Write home paths as `~/...`; no non-noreply email; no credential-shaped text; no fake secrets committed (build them from parts at test time).

---

## No Analog Found

Files with no close match (planner should use RESEARCH.md patterns):

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `runner/bundle.c` (run.manifest, digests.txt, observation and bundle digests) | runner | file-I/O | muhtari prints per-frame hashes (`runner/main.c` lines 249-264) but writes no bundle; use RESEARCH Pattern 9 |
| `tests/golden/skeleton.digest` | fixture | n/a | new in this repo (RESEARCH Pattern 9, recommended, not a second CI lane) |
| `docs/claims/claim.template` | doc/config | n/a | muhtari has `claims_wording.cmake` (not read) but no claim template; RESEARCH Pattern 4 defines it |
| `docs/policy/exposure-record.md` | doc | n/a | project-specific research-exposure table |
| `fixtures/toolchain/probe.c` + manifest | fixture | n/a | cc65 probe is new; hash recomputed in the plan's first task |
| cc65 two-step build, stamp and probe step inside `scripts/check.sh` | script | batch | muhtari fetches wasmtime and tools but builds no third-party compiler; use RESEARCH Pattern 2 |
| advance quantum, stop reasons, SplitMix64 RAM policy in `core/machine.c` | core | transform | muhtari's frame loop is hardware-specific; use RESEARCH Pattern 7 |

## Metadata

**Analog search scope:** `~/projects/muhtari` (`scripts/`, `tests/cmake/`, `tests/abi/`, `tests/hygiene/`, `tests/core/`, `tests/header/`, `tests/inventory/`, `cmake/`, `runner/`, `include/`, `src/instance.c`, `src/state.c`, `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml`, `pins.txt`, `PROVENANCE.md`, `ASSET_POLICY.md`, `README.md`); `~/projects/libfuturo/scripts/check.sh` (first 80 lines). The lynxplosion repo itself has no source.
**Files scanned:** about 40 muhtari files read or excerpted; 22 of the primary analog files confirmed tracked with `git ls-files` (the rest are tracked siblings in the same directories).
**Verified this session:** `no_float.cmake` flags muhtari `runner/sha256.c` header comment lines 1-2; `sha256.c` compiles clean under the lynxplosion flag set and `nm -P` shows only `bzero` and `memcpy` undefined with no writable data (scratch copies, nothing written in either repo).
**Not verified (carry as assumptions):** `check_junit.cmake` skip-declaration extension and the JUnit self-test command shape (adapted, not run verbatim); the `tests/runner/tests.cmake` multi-process pattern for `det.two_runs` (not read in depth).
**Pattern extraction date:** 2026-11-09
