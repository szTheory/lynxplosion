# Phase 1: Evidence Harness and Walking Skeleton - Research

**Researched:** 2026-11-09
**Domain:** C17 emulator-core build/evidence harness (CMake presets, POSIX sh gates, supply-chain pins, rights/determinism boundary checks, public C API skeleton)
**Confidence:** HIGH for harness mechanics (each important mechanism was run on macOS arm64 and in an `ubuntu:24.04` amd64 container this session); MEDIUM for CI timing (no real CI run yet); LOW for anything about `lynx-tests` building under the pinned cc65 (execution of that third-party Makefile was blocked by the sandbox, so it is unverified).

**Provenance tags used:** `[VERIFIED: ...]` ran/read/fetched this session (command or file named); `[CITED: url]` official documentation read this session; `[ASSUMED]` training knowledge or inference, needs confirmation; `[INF]` my inference from observations; `[REC]` recommendation. Source observations, inference and recommendation are kept separate throughout.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Owner gates and authority**
- **O8 accepted (owner, 2026-11-09):** C17 (`CMAKE_C_EXTENSIONS OFF`, `-pedantic-errors`), CMake ≥3.25 with workflow presets, Ninja, CTest, and a C ABI with an opaque handle. Record this in PROJECT.md Key Decisions.
- **O6:** proceed with a draft clean-room policy (`docs/policy/clean-room.md`). Implementers do not open GPL emulator source (Gearlynx, MAME, Holani, Beetle). Research exposure is recorded per source: what was read, when, and by which research file. FND-07 stays marked "draft pending O6".
- **Repository is public (owner, 2026-11-09):** `github.com/szTheory/lynxplosion` was created and pushed during discuss. Conditions: no PII and no credentials. History was already scrubbed of local home paths. CI is GitHub Actions on `ubuntu-24.04`, running `scripts/check.sh` and nothing else, with actions pinned by commit SHA. FND-01 and FND-09 CI evidence comes from a real CI run in this phase; record CI wall time. No licence file is added: O5 stays open (all rights reserved until decided). A README must say so.
- `.planning/AUTONOMY.md` created (points to AGENTS.md; records the discuss, verify and CI defaults).

**Repository layout and check command**
- Layout: `include/lynxplosion.h` (public API), `core/` (machine; no host imports), `runner/` (headless runner, in-repo sha256, bundle writer), `tests/`, `scripts/`, `fixtures/manifests/`, `docs/policy/`. No empty placeholder directories.
- `scripts/check.sh`: POSIX sh, shellcheck-clean. Steps: print tool identities (compiler, cmake, ninja, cc65 pin); run `scripts/fetch-pin.sh` for each `pins.txt` row (SHA-256 verified before unpack); build cc65 into `build/pins/` cached by commit stamp (`make bin lib TARGETS=lynx`, wall time recorded); run `cmake --workflow --preset check`; run hygiene scans. Any missing pin fails loudly.
- Lint: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`; ASan+UBSan in the check preset; shellcheck on `scripts/*.sh`. Formatting gate deferred (clang-format is not on the host).
- Exact inventory: committed `tests/inventory/fast.txt` plus `tests/inventory/skips.txt` (each skip with a reason). A CMake script compares `ctest -N` and the JUnit output against them. A missing, extra or undeclared skip fails. Include a negative control showing that an extra or missing test fails the inventory check.

**Skeleton machine and API contract**
- Instance memory is caller-provided: `lynx_machine_size()` plus init in place. The core never calls malloc and links only `memcpy`, `memmove`, `memset` and `memcmp`. Instances share no mutable state (API-01).
- Time: advance takes a `uint64_t` budget in 16 MHz master ticks and returns ticks consumed plus a stop reason enum (`BUDGET`, `FRAME`, `WATCH`, `HALT`). `FRAME` and `WATCH` are defined but reserved until later phases. Advance never exceeds the budget (API-03).
- Content: Phase 1 uses a provisional raw RAM image (bytes plus a load address), documented as provisional; the Phase 3 direct-load format replaces it. Refusals (NULL, empty, oversize, past the end of the address space) return a status code and leave an existing session byte-identical (API-02).
- Reset and power-on: RAM power-on policy is fixed-0x00 by default, with an integer PRNG option (seeded). The policy id and seed are recorded in run identity. Reset returns to the declared power-on state (API-04).
- Inspect: register and memory reads are side-effect free; tests prove that state and guest time do not change (API-05).
- Snapshot: a versioned blob holding magic, format version, core build identity (git rev + compiler id baked at build time), profile id, content SHA-256, RAM policy and seed, then state. Every field is validated before any state is replaced, and a mismatch is refused with no state change (API-07).

**Evidence, manifests and determinism gates**
- Manifests: one line-oriented `key = value` file per fixture under `fixtures/manifests/`, parsed by sh/CMake (no Python or JSON in the check path). Required keys: source, revision, recipe, sha256, licence, notices, allowed_use, startup_tier, profile, evidence_tier. A validator rejects missing keys, with a negative control.
- Firmware/binary scan: fail on any binary (NUL or non-text) file not listed in a manifest, across tracked files, untracked files and build artifact dirs; also fail on any binary of exactly 512 bytes regardless of manifest. A test seeds a fake 512-byte file in an isolated copy and asserts rejection, then asserts a pass once it is removed (FND-04).
- Forbidden imports (FND-05): two layers. (a) A `#include` allowlist for `core/` (`stdint.h`, `stddef.h`, `stdbool.h`, `string.h`) plus a token scan for `float`/`double`. (b) An `nm` check that the core static library's undefined symbols are only `mem*` (plus sanitizer runtime symbols in sanitizer builds) and that there is no writable data/bss. Each layer has a negative-control fixture that must fail.
- PII/credential hygiene scan (owner condition for the public repo): fail on absolute home paths (`/Users/<name>`, `/home/<name>`), non-noreply email addresses and common credential patterns in tracked files, with a negative control.
- Artifact bundle: the runner writes `run.manifest` (profile, startup tier, content sha256, input digest, tick count, core build identity, RAM policy and seed), state dumps, and `digests.txt`. The bundle digest is the SHA-256 of the sorted `digests.txt`, with no timestamps or host paths. Gates: two fresh runs, and two instances in one process, give equal digests (DET-01/02/04). Same-core digests are labelled repeatability, not accuracy.
- Claim template (FND-08) binds profile, content identity, startup tier, evidence tier and test inventory.

### Claude's Discretion
- Exact function names and prefix (e.g. `lynx_` vs `lxp_`), status code values, internal file split inside `core/` and `runner/`.
- The exact wording of policy documents, kept plain and short.
- The integer PRNG choice for the RAM policy (any small, documented, integer-only generator).

### Deferred Ideas (OUT OF SCOPE)
- Second CI runner / compiler-family cross-check lane: add only after a distinct digest-mismatch class is demonstrated (later phase).
- clang-format formatting gate, clang-tidy: later, when pinned.
- Project licence (O5): owner decision; the public repo is all rights reserved until then.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| FND-01 | One local command = PR CI gate | Workflow preset on schema version 6 verified on CMake 3.25.3/3.28.3/4.4.3; check.sh step list; CI job skeleton with SHA-pinned actions (Architecture Patterns 1, 8; Code Examples) |
| FND-02 | Pinned sources, SHA-256 before use, mismatch fails | cc65 and lynx-tests archive hashes re-fetched and matched; muhtari `fetch-pin.sh` (67 lines) reusable; offline `file://` negative control (Pattern 2) |
| FND-03 | Fixture manifests with required keys | `key = value` awk validator; hash-based "listed in a manifest" check; evidence-tier vocabulary T1..T7/R from SUMMARY (Pattern 4) |
| FND-04 | Fail closed on firmware-like/unmanifested binary; seeded 512-byte negative control | Portable `tr`+`iconv` detector tested on macOS and Linux (dash); NUL-only detection misses ~13.5% of random 512-byte blobs; scan zones (Pattern 5) |
| FND-05 | Core has no host imports | Include allowlist + token scan (muhtari `no_float.cmake` 59 lines) + `nm -P` checks (muhtari `abi.cmake` 111 lines); measured platform divergences; plain companion library (Pattern 6) |
| FND-06 | Exact inventory | Three layers: configure-time registration equality, JUnit completeness, `noTestsAction=error`; `expect_fail.cmake` negative controls (Pattern 3) |
| FND-07 | Source-exposure record + draft clean-room policy | Exposure seed extracted from research files; template (Pattern 10) |
| FND-08 | Claim template | Same kv validator, claim bound to `sha256(tests/inventory/fast.txt)` (Pattern 4) |
| FND-09 | cc65 builds reproducibly from pinned commit on dev host and CI, wall time recorded | Built and timed here; two-step make recipe (race found); tool binaries and linked probe output byte-identical across builds and across macOS/Linux; `lynx.lib` itself is NOT byte-identical (Pattern 2, Pitfall 1) |
| API-01..05, API-07 | Instances, load refusal, bounded advance, reset/power-on, side-effect-free inspect, snapshot | Caller-provided memory makes whole-instance `memcmp` the refusal/inspect oracle; all-fields-before-write snapshot; SplitMix64 for RAM policy (Pattern 7) |
| DET-01, DET-02, DET-04 | Equal digests across runs/instances; run manifest | Observation digest vs bundle digest split; fields that must never enter digests; sensitivity controls against vacuous equality (Pattern 9) |
</phase_requirements>

## Summary

Phase 1 is mostly harness engineering, and every mechanism in CONTEXT.md is feasible with POSIX sh, CMake scripts and a tiny C core. The owner's `~/projects/muhtari` already contains MIT-licensed, owner-authored, small helpers for almost every gate (pin fetch, JUnit completeness, negative-control runner, include/float scan, `nm` link-surface checks, SHA-256, hygiene scan). Copying them, with renames, is cheaper than writing them and consistent with the "copy-paste over dependencies" rule. Two things are not reusable as-is: muhtari's NUL detection (`grep -I`) and its single-pipeline fail flag, both of which have gaps proven below.

The research surfaced concrete corrections and risks to CONTEXT.md that the planner must absorb (details in "Flagged Risks"): (1) `make bin lib TARGETS=lynx` fails when run as one parallel invocation (race, reproduced) and must be two invocations; (2) the cc65 `lynx.lib` is not byte-reproducible but the tools and every linked output are, so FND-09 must hash the tools and a linked probe, not the library; (3) NUL-only binary detection cannot catch ~13.5% of random 512-byte blobs, so the 512-byte rule needs a control-byte/UTF-8 test; (4) "build artifact dirs" contain legitimate compiler output, so the scan needs an explicit toolchain-output class and the runner should write dumps as hex text; (5) the `nm` "no writable data" check cannot run on sanitized objects (Mach-O sanitized objects gain `C`/`b`/`d` symbols), so a plain companion library is required; (6) macOS default fortify and `-O2` bzero lowering add undefined symbols the allowlist must handle; (7) UBSan must be non-recoverable or the gate cannot fail; (8) snapshot content identity needs SHA-256 inside the core, which CONTEXT places in `runner/`.

**Primary recommendation:** Build the harness first with self-tests for every gate (muhtari shapes, adapted), keep `scripts/check.sh` a thin 100-150 line POSIX sh driver that prints identities and per-step wall time, run all gates as CTest tests with an exact inventory so the existing inventory mechanism covers them, and split the digest into an observation digest (build-independent, golden-comparable across macOS clang and Linux gcc) and a bundle digest (includes build identity).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Deterministic machine state, advance, reset, inspect, snapshot | `core/` (C library, no host imports) | public header `include/` | Owns all behavior; must be free of clocks, FS, malloc, FP, globals |
| Instance memory ownership | Host (caller) | `core/` (size/align query, init in place) | Caller-provided buffers make instances independent and refusal postconditions byte-checkable |
| SHA-256 content/state identity | `core/` internal util (needed for snapshot header) | `runner/` consumes it | Snapshot content identity must be computed by the core, not trusted from the host (see Flagged Risk F4) |
| Artifact bundle writing, file I/O, digests.txt | `runner/` (host side) | — | Files, paths and printing are host concerns; core never touches FS |
| Pin fetch + SHA-256 verification | `scripts/fetch-pin.sh` | `pins.txt` | Supply-chain gate before any unpack |
| cc65 build, build stamp, wall time | `scripts/check.sh` | `build/pins/` | Toolchain is a guest-side tool, not a core dependency |
| Test registration, exact inventory, JUnit completeness | CMake (`cmake/*.cmake`, `tests/cmake/`) | `tests/inventory/*.txt` | CTest is the single registry; scripts are invoked as tests |
| Rights/PII/forbidden-import gates | sh + CMake scripts invoked by CTest and by `check.sh` post-step | CI | Same code path locally and in CI |
| CI gate | GitHub Actions job running `scripts/check.sh` | — | One definition shared by laptop and CI |

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| C17, `CMAKE_C_EXTENSIONS OFF`, `-pedantic-errors` | locked (O8) | core, runner, tests | Owner decision O8 |
| CMake | floor 3.25; dev host 4.4.3; ubuntu-24.04 apt 3.28.3; GitHub ubuntu-24.04 runner preinstalled 3.31.6 | configure/build/test/workflow | `[VERIFIED: cmake --version, apt-cache policy in ubuntu:24.04 container, actions/runner-images Ubuntu2404-Readme.md image 20261101.360.1]` |
| CMake presets `"version": 6` | schema 6 = CMake 3.25 | workflow, JUnit output | `[CITED: cmake.org/cmake/help/latest/manual/cmake-presets.7.html]` table: "6 \| 3.25"; Workflow Presets "added" in 6; `outputJUnitFile` "Added in presets version 6". `[VERIFIED: ran]` a version-6 preset with workflowPresets, `outputJUnitFile`, `execution.noTestsAction: error` passes on CMake 3.25.3 (Kitware tarball, SHA-256 checked against Kitware's SHA-256.txt), 3.28.3 and 4.4.3 |
| Ninja | dev 1.13.2; runner 1.13.2; ubuntu apt 1.11.1 | generator | `[VERIFIED]` runner README lists "Ninja 1.13.2"; no apt install needed on the runner |
| CTest | = CMake | runner, JUnit | bundled |
| gcc/clang | dev Apple clang 21.0.0 (arm64); runner default gcc 13.x (`4:13.2.0-7ubuntu1` apt pkg, GNU C 12.4/13.3/14.2 present), clang 16/17/18 | compilers | `[VERIFIED]` runner README |
| shellcheck | dev 0.11.0; runner 0.9.0-1 | lint | `[VERIFIED]`; version drift: pin `shellcheck -s sh` and print the version |
| cc65 | commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2`, Zlib | guest toolchain (Phase 1: build + probe only) | Pinned in STACK.md; archive hash re-verified (below) |
| lynx-tests | commit `b583dc43d532d3c41563a6fa35420614fbcead89` (tag 1.4.9), MIT | fetched and verified only in Phase 1 | Built in Phase 4 |

### Supporting (project-authored copies, no new dependencies)
| Helper | Source | Size | Use |
|--------|--------|------|-----|
| `fetch-pin.sh` | muhtari `scripts/fetch-pin.sh` | 67 lines | Verified pin fetch |
| `check_junit.cmake`, `expect_fail.cmake`, `no_skip.cmake`, `pins.cmake` | muhtari `tests/cmake/` | 58, 19, 38, 49 lines | Inventory, negative controls, no self-skip, pins.txt lint |
| `MuhtariTests.cmake` (registration == inventory) | muhtari `cmake/` | 69 lines | Configure-time exact inventory |
| `abi.cmake` (modes undefined/globals/symbols) | muhtari `tests/cmake/` | 111 lines | `nm -P` checks incl. public-name/header equality |
| `no_float.cmake` | muhtari `tests/cmake/` | 59 lines | float/double/`<math.h>`/literal scan |
| `hygiene.sh` | muhtari `scripts/` (adapted from nesturbator, MIT) | 178 lines | Home-path/email scan, `--tree`/`--staged`/`--history` |
| `sha256.c/.h` | muhtari `runner/` (FIPS 180-4, "Written from the standard") | 132 + 22 lines | Content/state identity; only `memcpy`/`memset` used |
| SplitMix64 | Vigna, public domain `[VERIFIED: fetched https://prng.di.unimi.it/splitmix64.c]` | ~6 lines | RAM power-on PRNG |

All muhtari files are MIT, `LICENSE`: "Copyright (c) 2026 muhtari contributors" `[VERIFIED: head -5 ~/projects/muhtari/LICENSE]`. Git authors of the files above: only `szTheory` (4 commits touching them); repo total 327+4 `szTheory`, 9 `Codex`, 10 release bot `[VERIFIED: git log]`. `[REC]` Put a one-line "Adapted from muhtari (MIT, owner project) at commit <sha>" header in each copy and keep the notice in `docs/` provenance; do not copy muhtari's `check_stop_policy` (it fails on this repo's `mode: yolo` config; muhtari-specific) or `firmware_scan.cmake` (223 lines, hashes known 7800/2600 ROMs; not applicable).

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `grep -I` binary detection (muhtari) | `tr`+`iconv` (recommended) | `grep -I` is NUL-only in the C locale on BSD and GNU grep (tested), so a 512-byte blob without NUL passes |
| JSON manifests | `key = value` (locked) | No parser dependency; awk validator |
| `actions/cache` for cc65 | no cache first | cc65 `make bin`+`lib` measured 5-20 s locally (below); measure on the runner before adding the action |
| Python helpers | POSIX sh + CMake scripts (locked) | Matches AUTONOMY |

**Installation:** none (no npm/pip/cargo). Dev host already has cmake, ninja, shellcheck, `docker` (for Linux experiments), `gh`. cc65 is built from the pinned archive.

**Version verification (measured this session):**
```bash
shasum -a 256 cc65.tar.gz   # d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1  (3,412,205 bytes)  matches STACK.md
shasum -a 256 lt.tar.gz     # a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e  (102,538 bytes)    matches STACK.md
```
`[VERIFIED: curl of https://github.com/cc65/cc65/archive/eebf54af545f4ae52f8489a2f6d4d5f93e7942d2.tar.gz twice (identical hash) and https://github.com/drhelius/lynx-tests/archive/b583dc43d532d3c41563a6fa35420614fbcead89.tar.gz]`. Note: the tag-name URL `.../archive/refs/tags/1.4.9.tar.gz` hashes differently (`13215d4d6ea76897ade6fe5fcd4baee859881c9ce0fa51ed719fc8fb9e96fd38`) because the top-level directory name differs. `[REC]` Use the commit-SHA URL form in `pins.txt` (the form CONTEXT and STACK.md already recorded) and never the tag URL. `[INF]` GitHub archive bytes are not contractually stable; if a hash ever drifts, the fallback identity is `git clone` + `git rev-parse HEAD`; make `fetch-pin.sh` print that remedy in its mismatch message.

## Package Legitimacy Audit

No external packages are installed from npm, PyPI or crates in this phase, so the `gsd-tools package-legitimacy` seam was not applicable. Third-party material is limited to two pinned source archives, verified by SHA-256 (table above) from the upstream repos named in STACK.md, plus runner-image tools. cc65 is Zlib, lynx-tests is MIT (STACK.md `[SO]`).

| Package | Registry | Verdict | Disposition |
|---------|----------|---------|-------------|
| (none) | — | — | — |

**Packages removed due to [SLOP] verdict:** none. **Packages flagged [SUS]:** none.

## Architecture Patterns

### System Architecture Diagram

```
 developer / CI job (ubuntu-24.04)
          |
          v
   scripts/check.sh  (POSIX sh; prints identities; per-step wall time)
          |
          +--1--> identities: cc, cmake, ninja, shellcheck, git, cc65 pin
          |
          +--2--> scripts/fetch-pin.sh  --(pins.txt row)--> curl -> tmp -> sha256 == row? --no--> FAIL
          |                                                          | yes
          |                                                          v
          |                                      unpack build/pins/{cc65,lynx-tests}
          |
          +--3--> cc65 stamp present? --no--> make -C build/pins/cc65 bin ; make ... lib TARGETS=lynx ; record seconds
          |                                         probe: cl65 -t lynx probe.c -> sha256 (compare to manifest value)
          |
          +--4--> shellcheck -s sh scripts/*.sh
          |
          +--5--> cmake --workflow --preset check
          |          configure (Ninja, Debug, ASan+UBSan non-recoverable, -Werror set)
          |             |-- registration == tests/inventory/fast.txt ? --no--> FAIL at configure
          |          build: lynx_core (sanitized)  lynx_core_plain (nm target)  runner  tests
          |          test (CTest, JUnit, noTestsAction=error):
          |             api.* det.* core.* hygiene.*.self_test  inventory.self_test ...
          |          <junit.xml>
          |
          +--6--> cmake -P check_junit.cmake (every inventory name ran once, none skipped; skips.txt declared)
          |
          +--7--> scripts/hygiene.sh --tree ; artifact-zone scan of build/ ; (--history if history available)
          v
       exit 0 / 1

 runner (inside tests): content bytes + profile + seed + budget
     -> core instance (caller-provided buffer) -> advance -> observation dumps (hex text)
     -> digests.txt (sorted) -> observation digest, bundle digest -> run.manifest
```

### Recommended Project Structure
```
include/lynxplosion.h         # public C API (self-contained, extern "C"-safe)
core/                         # machine; includes only stdint/stddef/stdbool/string
  sha256.c sha256.h           # internal util (also used by runner) -- see F4
  machine.c snapshot.c ...    # split is Claude's discretion
  build_id.c.in               # configure_file: git rev, dirty flag, compiler id, target
runner/                       # headless runner, bundle writer, run.manifest
tests/
  api/ det/ core/ hygiene/    # CTest executables and sh/CMake tests
  cmake/                      # check_junit, expect_fail, no_skip, abi, no_float, pins ... (copied)
  inventory/fast.txt skips.txt
scripts/                      # check.sh fetch-pin.sh hygiene.sh scan-binaries.sh validate-kv.sh
fixtures/manifests/*.manifest # key = value, one per fixture
docs/policy/clean-room.md  docs/claims/claim.template  docs/ (exposure record)
cmake/                        # Tests.cmake (registration==inventory), Build identity
pins.txt  CMakePresets.json  CMakeLists.txt  README.md (all rights reserved; O5 open)
.github/workflows/ci.yml
```
No placeholder directories (CONTEXT): create `fixtures/`, `docs/` etc. only when a file lands in them.

### Pattern 1: One workflow preset shared by laptop and CI
**What:** `cmake --workflow --preset check` = configure+build+test in one command. **When:** always; check.sh calls it. 
**Example** `[VERIFIED: ran on cmake 4.4.3, 3.28.3 and 3.25.3]`:
```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    { "name": "check", "generator": "Ninja", "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" } }
  ],
  "buildPresets": [ { "name": "check", "configurePreset": "check" } ],
  "testPresets": [
    { "name": "check", "configurePreset": "check",
      "output": { "outputOnFailure": true, "outputJUnitFile": "${sourceDir}/build/${presetName}/junit.xml" },
      "execution": { "noTestsAction": "error" } }
  ],
  "workflowPresets": [
    { "name": "check", "steps": [
      { "type": "configure", "name": "check" },
      { "type": "build", "name": "check" },
      { "type": "test", "name": "check" } ] }
  ]
}
```
Rules `[CITED: cmake-presets manual]`: first workflow step must be configure, later steps build/test/package, all with the same `configurePreset`. Do not copy muhtari's schema `"version": 11` (needs CMake 4.3); muhtari documents that a lower-CMake container broke on its newer schema (`pins.txt` comment). Use the lowest schema needed (6). `[VERIFIED]` A re-run of `--workflow` re-executes configure (the build id and git dirty flag therefore refresh on every check run).
JUnit content `[VERIFIED]`: `<testsuite name="(empty)" tests="2" ... hostname="" ... timestamp="...">` with `<testcase name="one" classname="one" ... status="run">`. It carries a timestamp and timings, so it is never an input to a digest.
**Floor coverage gap (risk, not a decision change):** CI preinstalls 3.31.6 and the dev host 4.4.3, so the 3.25 floor is not exercised by the gate; I verified it once with 3.25.3 in a container. `[REC]` document that one-off check; add a lane only if a floor break is ever observed (consistent with "add CI only for a distinct recurring failure").

### Pattern 2: Pins, fetch, and the cc65 build
**pins.txt row format (copy muhtari):** `name version url sha256`, https only, 64-hex digest, unique names (`pins.cmake`, 49 lines, rejects violations).
**fetch-pin.sh behavior (read):** downloads to `mktemp -d`, hashes with `sha256sum` or `shasum -a 256`, only then unpacks (`tar --strip-components=1`). `[VERIFIED: file read]` Mismatch message: `fetch-pin: $name: sha256 mismatch: pins.txt has $want, the download is $got`.
**Negative control without network:** `PINS=<tmp pins file with a file:// URL and a wrong digest> sh scripts/fetch-pin.sh NAME DEST` must exit 1 with `sha256 mismatch`, and must not create DEST (`curl` supports `file://`; `[ASSUMED]` verify in the plan; the muhtari script's `PINS` override variable exists `[VERIFIED: pins=${PINS:-...}]`).
**cc65 build recipe (measured):**
```sh
# fetch-pin.sh cc65 build/pins/cc65   (verified, stripped one level)
make -C build/pins/cc65 -j"$jobs" bin
make -C build/pins/cc65 -j"$jobs" lib TARGETS=lynx
export CC65_HOME=$PWD/build/pins/cc65   # required: compiled-in defaults point at /share/cc65
PATH=$CC65_HOME/bin:$PATH
```
Measured `[VERIFIED: ran]` (dev host arm64, 18 cores, GNU make 3.81 default; clean extraction each time):

| Command | Wall time |
|---------|-----------|
| `make -j8 bin` | 4.12 s |
| `make -j8 lib TARGETS=lynx` | 0.43 s |
| `make -j4 bin` / `make -j4 lib TARGETS=lynx` | 6.31 s / 0.50 s |
| serial `bin` + `lib TARGETS=lynx` (no `-j`) | 20.2 s |
| ubuntu:24.04 amd64 container under emulation (`-j4`, gcc 13.3, GNU make 4.3) | 45.9 s / 4.3 s (emulated; NOT representative of the runner) |

`getconf _NPROCESSORS_ONLN` works on macOS and Linux (`nproc` is not on macOS). The runner has 4 vCPU/16 GB for public repos `[CITED: docs.github.com .../reference/runners/github-hosted-runners]`, so expect roughly the `-j4` row; the first real CI run supplies the number to record.
**Race (CONTEXT recipe correction):** `make -j8 bin lib TARGETS=lynx` as ONE invocation failed (`make[2]: cc65: No such file or directory`, `*** [lib] Error 2`) because `libsrc/Makefile` evaluates `$(wildcard ../bin/cc65*)` at parse time, before the parallel `bin` goal has produced the tools. Two invocations in order fix it. `[VERIFIED: ran, log read]`
**Reproducibility results (FND-09 evidence design)** `[VERIFIED: ran]`:
- Two builds in different directories: `bin/ca65` and `bin/ld65` byte-identical (SHA-256 prefixes `a410d4f5a3bdcfb5`, `521d3c584ddfe881` both runs).
- `lib/lynx.lib` NOT identical (SHA-256 differ; 2,698 differing bytes, same size 1,702,086) — embedded build-time data. Do not hash the library.
- A tiny project-authored probe (`unsigned char g_results[4]; void main(void){ g_results[0]=1; for(;;){} }`, `cl65 -t lynx -Ln t.sym -o t.lnx t.c`, run with `env -i` and only `CC65_HOME` set) produced a byte-identical 594-byte `.lnx` from both toolchains, SHA-256 `e2fdd9812e79c7ba2a7c3b6f1328aedf61bf358b54aab7f35ae6fb45117804af`, **and the same bytes on Linux x86_64 (gcc 13.3) vs macOS arm64 (Apple clang 21)**; the `.sym` files were also identical, and the symbol line reads `al 00033F ._g_results`.
- `[REC]` FND-09 evidence = (a) archive SHA-256 verified, (b) wall time of the build printed, (c) probe `.lnx` SHA-256 printed and compared with the value recorded in the probe's manifest (`sha256 = ...`), identical on dev host and CI runner. Commit the probe source (own work) as a fixture with a manifest; its output lives in `build/` (see Pattern 5 for the hash-keyed allowlist). The expected hash must be recomputed from the committed source/flags by the plan's first task (value above is for my scratch source).
- Build stamp: write `build/pins/cc65/.stamp` (pin row sha256 + recipe version) only after both make steps and the probe succeed; skip rebuild only if stamp matches. Print the stored build seconds on a cache hit so FND-09 wall time is always visible; a clean CI checkout always rebuilds.
- A third-party build was NOT run for `lynx-tests`: invoking its Makefile (`cd cpu && make` with `CC65_HOME`) was blocked by the sandbox permission classifier. `[REC]` leave "does lynx-tests 1.4.9 build under this cc65" to the Phase 4 spike (STACK.md open item 2); Phase 1 only fetches and verifies it. I read `shared/lynxcc65.mak` and `cpu/Makefile`: tools are invoked by bare name from `PATH`; link uses `lynx.lib`; `-Ln cpu.sym`. `[VERIFIED: file read]`
- cc65's compile-time defaults are `-DCA65_INC="/share/cc65/asminc"` etc. (visible in the make log); without `CC65_HOME` the tools look in `/share/cc65`. `[VERIFIED: make log]`

### Pattern 3: Exact inventory in three layers + negative controls
1. **Configure time** (copy `MuhtariTests.cmake`, 69 lines): `cmake_language(DEFER DIRECTORY ... CALL check_inventory)` compares the set of registered tests (`get_property(DIRECTORY TESTS)` + labels) with `tests/inventory/fast.txt` (`name<TAB>labels`, sorted, `#` comments); configure FAILS on any difference or unsorted file.
2. **Run time** (`check_junit.cmake`, 58 lines): every selected inventory name appears exactly once in the JUnit file with `status="run"` and no `<failure|error|skipped>`; zero tests = failure. Combined with preset `noTestsAction: error`.
3. **Declared skips:** `skips.txt` = `name<TAB>reason`. Phase 1 has no skips: `[REC]` file exists with a header comment only, and the checker accepts a skip only if listed. `no_skip.cmake` (38 lines) forbids `SKIP_RETURN_CODE`/`SKIP_REGULAR_EXPRESSION`/`DISABLED` in CMake files so a test cannot self-skip silently.
**Negative controls** are CTest tests that run the checker on synthetic input and require an exact exit code and message via `expect_fail.cmake` (19 lines: `-DCMD -DCODE -DREGEX`; a bare `WILL_FAIL` would accept a crash or typo). Fixtures are small text files generated by the test or committed as text: a JUnit with a missing test, an extra test, a duplicate, and a `<skipped/>`; an inventory with an unsorted line. `[VERIFIED: files read]`
`ctest -N` is also available; `[REC]` rely on registration equality for the declared set and on JUnit for the executed set instead of parsing `ctest -N` text (CONTEXT mentions both; layering them costs nothing, and `ctest -N` text `Test #n: name` is parseable with `sed` if wanted).

### Pattern 4: Manifests, claims and the fixture identity model
- **Format:** `key = value`, `#` comments, exactly one ` = ` per line, keys `[a-z_]+`, no duplicate keys, no unknown keys, no empty values. Required keys (CONTEXT): `source revision recipe sha256 licence notices allowed_use startup_tier profile evidence_tier`. Validate in awk (POSIX): check `length($0)==64` and `$0 ~ /^[0-9a-f]+$/` for `sha256` (`[REC]` avoid `{n}` regex intervals in awk; old BWK awk may lack them — `[ASSUMED]`).
- **"Listed in a manifest" = SHA-256 of the file equals some manifest's `sha256`**, not path equality. This survives renames, works for build-tree outputs, and needs no `path` key. `[REC]` an optional informational `path` key is fine but must not gate.
- `[INF]` Phase 1 fixtures that need manifests: the provisional skeleton RAM image if committed as binary (or generate it as text in tests and commit none), and the cc65 toolchain probe (Pattern 2). `startup_tier = A`, `profile = none` (O1 open; do not name a Lynx I/II profile), `evidence_tier`: the SUMMARY vocabulary is T1 spec/vector, T2 public hardware-informed test, T3 original diagnostic with manifest, T4 independent emulator comparison, T5 measured hardware, T6 game/homebrew progress, T7 installed-host behaviour, R same-core repeatability `[VERIFIED: .planning/research/SUMMARY.md:33]`. The probe and skeleton image are `R`/`T3`-class; the validator should enforce an enumerated list. SUMMARY line 300 also lists an `independence` key not in CONTEXT's ten; mention to the planner as optional.
- **Claim template (FND-08):** same kv format and the same validator with a different required-key list: `claim profile content_sha256 startup_tier evidence_tier inventory_sha256`. `inventory_sha256` = SHA-256 of `tests/inventory/fast.txt`, recomputed by a test, so a claim is bound to the exact inventory. Negative control: a claim missing `profile` (or with a stale `inventory_sha256`) is rejected. Template text states that same-core digests are repeatability (`R`), not accuracy.

### Pattern 5: Rights/firmware/binary scan (FND-04)
**Measured facts that drive the design** `[VERIFIED: ran]`:
- On this host the interactive `grep` is a shell function wrapping ugrep 7.8.4 (scripts see `/usr/bin/grep`, BSD grep 2.6.0-FreeBSD); Linux has GNU grep 3.11. With `LC_ALL=C`, BSD and GNU `grep -I -q ''` both call a file binary only if it contains a NUL; a 512-byte file of bytes 0x80..0xFE and a file of C0 control bytes are "TEXT" to both. `[INF]` arithmetic: P(no NUL in 512 random bytes) = (255/256)^512 ≈ 13.5%, so a NUL-only test misses roughly one in seven random 512-byte blobs (muhtari's `hygiene.sh` relies on `grep -I`).
- `tr -d '\000-\010\013\016-\037\177' < f | wc -c` versus `wc -c < f` gives identical results on macOS tr and Linux tr (deletes NUL and C0 controls except TAB, LF, FF, CR; and DEL). A random 512-byte blob has controls with probability ≈ 1 - 1e-27.
- `iconv -f UTF-8 -t UTF-8 < f >/dev/null` exits non-zero for invalid UTF-8 on both macOS and Linux, and zero for valid UTF-8.
- This repo's own docs contain valid UTF-8 non-ASCII (14 tracked files, e.g. `.planning/research/ARCHITECTURE.md` 3,153 non-ASCII bytes), so "non-ASCII = binary" would false-positive.
- cc65 source tree `[VERIFIED]`: 4,288 files, 76 NUL-containing files (`*.ref`, `*.bin-ref`, `.hgr`, `.dhgr`, `.chr`, `.png`, `.pcx`, `.fnt`), exactly one 512-byte file (`libsrc/runtime/aslax7.s`, text), no binary of exactly 512 bytes; the built tree (5,987 files) again has only that one 512-byte file. lynx-tests source `[VERIFIED]`: 75 files, 0 binary, 0 of 512 bytes.

**Design `[REC]`** (prototype run on macOS `sh` and Ubuntu `dash`, both flagged NUL and no-NUL 512-byte blobs, passed clean text and UTF-8):
- `is_nontext(f)` = contains a control byte per the `tr` test, OR fails the `iconv` UTF-8 check.
- **Source zone:** `git ls-files -z -co --exclude-standard` (tracked + untracked-not-ignored), skip symlinks (`[ -L ]`) and directories, `-s` non-empty. Non-text files must have their SHA-256 listed in a manifest. Any file of exactly 512 bytes that is non-text fails regardless of manifest (CONTEXT rule).
- **Pin zone `build/pins/`:** excluded from the "unmanifested binary" rule by ONE explicit, visible, documented rule (pins are verified by archive SHA-256; the cc65 tree has 76 legitimate binary test refs). The exact-512-byte non-text rule still applies there, implemented cheaply with `find build/pins -type f -size 512c` (found 1 text file; no false positives measured). If a future pin bump produces a 512-byte compiler object, the check fails visibly and the PR must address it (no exemption mechanism in Phase 1).
- **Build-artifact zone `build/<preset>/` (and any other non-pin dir under `build/`):** CONTEXT says scan these too, but compiler/linker output (`.o`, `.a`, executables, `.ninja_deps`, dSYM) is binary and unmanifested. `[REC]` classify toolchain outputs by magic (`od -An -tx1 -N4 f`: ELF `7f 45 4c 46`, Mach-O `cf fa ed fe`/`ca fe ba be`, `!<arch>` `21 3c 61 72`, ninja `# ninjadeps` header) and require everything else non-text to be manifested. `[REC]` make the runner write bundle dumps as lowercase hex text lines (the digest is over the canonical bytes, not the file), so artifacts are text and the rule stays simple. This is a deliberate narrowing of "any binary in build artifact dirs" — flag F2 for the planner.
- **Run the artifact-zone scan as a `check.sh` step after the workflow** (tests may write files concurrently under `ctest -j`; scanning mid-run is racy). Keep source-zone scan and all self-tests inside CTest.
- **Negative control (isolated copy):** the self-test builds a temp tree with `mktemp -d` OUTSIDE the repo (so the artifact-zone scan never sees a seeded file), `git init -q`, copies the scan script, writes deterministic fakes — one with a NUL plus 511 `A` bytes, one with 512 bytes `128 + (i % 127)` and no NUL — and asserts rejection of each, then removes them and asserts pass. Deterministic fakes, never `/dev/urandom` (flaky); both variants are needed.
- **Pitfall in my prototype (and any `cmd | while read` loop):** a variable set inside a piped `while` loop does not survive the subshell, so the exit status stayed 0 while findings printed. Write findings to a temp file or count output lines, and `exit` from that.

### Pattern 6: Forbidden imports (FND-05) — measured behavior
**Layer (a)** include allowlist over `core/` and the public header (`stdint.h stddef.h stdbool.h string.h`; `[REC]` do not allow `assert.h`: it adds `__assert_rtn`/`__assert_fail`), plus muhtari `no_float.cmake` (flags `float`, `double`, `<math.h>`, hex-float and decimal literals, 59 lines).
**Layer (b)** `nm -P` link-surface checks via muhtari `abi.cmake` modes. Parser facts `[VERIFIED: abi.cmake read]`: `nm -P` prints `name type [value size]`; headers like `core.o:` do not match `^([^ ]+) ([A-Za-z])( |$)`; on Darwin one leading `_` is stripped; writable = types `B b D d C`; defined externals vs header declarations check by regex.
Measured on Apple clang 21 (arm64) and gcc 13.3 (ubuntu:24.04 amd64), source with `memset`, `memcpy`, a const `uint32_t` table:
- **Sanitized objects cannot be nm-checked.** Mach-O ASan/UBSan object: undefined `___asan_*`, `___ubsan_handle_*`, and added writable symbols (`____asan_globals_registered C 8`, `l___unnamed_1 d`, `ltmp3 b`). Linux sanitized: undefined `__asan_*`, `__ubsan_*`. `[REC]` Build a second, un-instrumented static library from the same sources (`lynx_core_plain`, tiny compile) and point `nm` at that. CONTEXT's "plus sanitizer runtime symbols in sanitizer builds" then becomes unnecessary; keep only as a documented fallback.
- **Undefined symbols beyond `mem*`:** macOS defaults add `___memcpy_chk`/`___memset_chk` (fortify) at `-O0`; `-O2` and even `-O0` with fortify off add `_bzero` for zeroing `memset` (`nm`, after `_` stripped: `bzero`). Ubuntu gcc default adds `__stack_chk_fail`, `__strcpy_chk` when arrays/`strcpy` appear. `[REC]` compile the plain core with `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector`; allowlist `memcpy memmove memset memcmp` plus per-platform helpers: Darwin `bzero __bzero`; Linux x86_64 `_GLOBAL_OFFSET_TABLE_` (muhtari lists these with measurement notes `[VERIFIED: abi.cmake:26-37 read]`; `_GLOBAL_OFFSET_TABLE_` did not appear in my samples, so keep it as owner precedent, not as my finding).
- **Pointer tables diverge by object format:** `static const char *const names[]` is `s` on Mach-O (passes) but `d` on ELF PIE (`.data.rel.ro`, fails the writable check); const integer tables and function-local `static const uint32_t H0[8]` are `s`/`r` on both. `[REC]` rule: integer const tables only in `core/` (muhtari states the same).
- A genuine mutable global shows as `b` (bss) locally even though `static` (`_counter b e0 0` and a duplicate local `ltmp2 b`), so one mutable static anywhere is caught.
- Negative controls: a fixture `.c` with `static uint32_t counter; ... ++counter`, one with `printf`/`time`/`malloc` calls, one with `float`, one with `#include <stdio.h>`; each built to its own tiny lib/file and fed to the same script via `expect_fail.cmake`.
- Linux `nm`/Darwin `nm`: both accept `-P`; llvm-nm on macOS is "compatible with GNU nm" `[VERIFIED: nm --version]`.

### Pattern 7: Skeleton machine and C ABI
`[REC]` unless marked.
- **Header:** `include/lynxplosion.h`, only `stdint.h stddef.h stdbool.h`, `extern "C"` guards, opaque `typedef struct lynx_machine lynx_machine;`. Add a test that compiles the header alone with `-pedantic-errors` (and, when a C++ compiler exists, `-fsyntax-only` as C++), and muhtari-style `abi.cmake symbols` equality: every declared function is defined, and no other public name (prefix `lynx_`; internals `lynx__`).
- **Instances:** `lynx_machine_size()` and an alignment query; `lynx_machine_init(void *mem, size_t size, ...)` validates NULL, size, alignment, zero-fills, returns a status. "Create/destroy" = init-in-place and a `deinit`/poison that is a no-op for state. The instance buffer is the only mutable state; the plain-lib `nm` check proves there is none elsewhere. Test buffers use `_Alignas(max_align_t)`.
- **Refusal and inspect oracle:** because memory is caller-owned, a test can `memcmp` the whole instance buffer before and after a refused load, a refused restore, or any inspect call; this is a byte-identical check, not a field-by-field one. Refusal inputs: NULL pointer, zero length, oversize, `addr+len` past 0x10000 (use overflow-safe form `len > 0x10000u - addr`, not `addr + len`), bad alignment, bad status of reserved enums.
- **Advance:** budget `uint64_t` master ticks, returns consumed and stop reason (`BUDGET`, `FRAME`, `WATCH`, `HALT`); consumed <= budget always. A skeleton needs a quantum or nothing observable evolves; `[REC]` a fixed 4-tick quantum (one 4 MHz cycle = 4 master ticks) with `consumed = (budget / 4) * 4`, so budgets 0..3 consume 0 and still report `BUDGET`; each quantum applies a small deterministic integer mix to state (and optionally one RAM byte at a derived address) so that digests depend on ticks, seed and content. Guard `UINT64_MAX`: no overflow in guest time (wrapping is a defined policy or a refusal; test it).
- **Reset/power-on:** policy id `0 = fixed 0x00`, `1 = seeded PRNG`; reset re-applies the declared policy and seed; policy id and seed are returned for run identity. SplitMix64 `[VERIFIED: fetched https://prng.di.unimi.it/splitmix64.c; public domain]`:
```c
/* written 2015 by S. Vigna, public domain; state is the caller-owned instance field */
static uint64_t splitmix64_next(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15u);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}
```
  (The original uses a `static` state variable; that is a writable global, so the copy must take the state by pointer.) Fill RAM from successive outputs, one byte per output or 8 bytes per output little-endian explicitly, and document which.
- **Snapshot:** header = magic, format version, build id, profile id, content SHA-256, RAM policy, seed, payload length, payload SHA-256, then payload. `[REC]` payload is a canonical field-by-field little-endian serialization (not a `memcpy` of structs: padding bytes and host endianness would otherwise leak into digests and blobs). Restore does a full validation pass over every header field AND the payload hash and value ranges before the first write, then an apply step that cannot fail; the test proves "refused => instance memory byte-identical" by `memcmp` of the whole buffer. Corruption test: flip each byte of the header exhaustively and a deterministic stride of the payload (every 251st byte plus first/last), each restore must refuse and leave the instance identical (exhaustive 64 KB payload flips with SHA-256 each are too slow under ASan; the stride is a measured-first compromise `[ASSUMED]`).
- **Content SHA-256 lives in the core (F4).** `[REC]` put `sha256.c/.h` under `core/` as an internal unit compiled into the library (uses only `memcpy`/`memset`; muhtari's `init` uses a function-local `static const uint32_t H0[8]`, `r`/`s` on both platforms) and let the runner use the same unit through an exported helper or by compiling the same file into the runner target. One copy, not two.
- **Test registry (proposed names):** `api.header_selfcontained`, `api.abi_symbols`, `api.instances_independent`, `api.load_refusal_unchanged`, `api.advance_budget`, `api.reset_power_on`, `api.inspect_pure`, `api.snapshot_roundtrip`, `api.snapshot_refusal`.

### Pattern 8: Build identity baked at configure time
`[REC]`, `[VERIFIED]` where noted.
- `configure_file(core/build_id.c.in ...)` fed by `execute_process(git rev-parse HEAD)`, a dirty flag from `git status --porcelain --untracked-files=no` (empty = clean), `CMAKE_C_COMPILER_ID` + `CMAKE_C_COMPILER_VERSION`, and `CMAKE_SYSTEM_NAME/PROCESSOR`. Fallback string `nogit` when git or `.git` is absent (tarball builds).
- Since `--workflow` re-runs configure each time `[VERIFIED]`, `check.sh` always sees a fresh rev; in an incremental IDE loop the id can go stale, which is acceptable because the id is identity for snapshot-compat and manifests, not for behaviour.
- Add `-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.` (and the binary dir) so absolute host paths do not reach objects; never use `__DATE__/__TIME__`.
- Snapshot restore compares the full build-id string; a snapshot from another build is refused (API-07: "same build").

### Pattern 9: Digest design (DET-01/02/04) — which fields enter which digest
`[REC]`:
- **Observation digest** (build-independent): SHA-256 of the sorted `obs.*` lines of `digests.txt`, where each line is `name<space>sha256` of a canonical observation: machine state serialization, RAM bytes, registers/time, ticks consumed, stop reason. Contains nothing about build identity, paths, time, flags or the host.
- **Bundle digest** (CONTEXT definition): SHA-256 of the whole sorted `digests.txt`, which also lists `run.manifest`'s own SHA-256. `run.manifest` carries build identity (git rev + compiler id), so the bundle digest legitimately differs across builds and is equal across two runs of the same build.
- `run.manifest` fields (DET-04): profile, startup tier, content sha256, input digest (use `sha256("")`, SHA-256 of the empty string, `e3b0c442...b855`, as the explicit value for an empty timeline), tick budget and ticks consumed, build identity, RAM policy and seed. Text `key = value`, same parser as manifests.
- Never enter any digest: timestamps, hostnames, absolute paths, JUnit content, wall time, sanitizer/compile flags, pointer values, struct padding, locale-dependent sort. `[REC]` the runner sorts names with byte comparison itself; the check can recompute with `LC_ALL=C sort`.
- **Gates:** (1) two fresh runs in two processes => equal observation AND bundle digests; (2) two instances in one process => equal to the separate runs; (3) **sensitivity controls** so equality is not vacuous: different seed, different content byte, different budget, and one flipped state bit each change the observation digest; (4) **golden cross-compiler value:** commit the observation digest of one fixed workload (`tests/golden/skeleton.digest`); it is computed on macOS Apple clang by the dev host and by gcc on the runner in the same `check.sh`, which proves compiler-independence without adding the deferred second CI lane. `[INF]` this is cheap and informative; it fails loudly the first time the skeleton gains behaviour (regenerate with a documented command and a justification in the commit). Flag to planner as an addition, not a CONTEXT contradiction.
- Label every digest output "repeatability (R), not accuracy" (requirement text).

### Pattern 10: Policy documents (FND-07)
- `docs/policy/clean-room.md`: draft pending O6, plain and short: rule (implementers do not open GPL emulator source), allowed sources (Epyx documents, lynx-tests as test source, cc65 sources, project-authored tests), what to do on accidental exposure (record it), how exposure is recorded. Status banner: "DRAFT pending owner decision O6".
- Exposure record seed `[VERIFIED: grep of .planning/research/*.md and SUMMARY.md:182-184,462]`: ARCHITECTURE.md line 14 states Gearlynx (GPL-3.0, commit `94068da`, 2026-11-04) and lynx-tests (commit `a920f01`) were read directly in a scratch clone; Handy/libretro-handy (zlib) and MAME (GPL-2.0+) via a summarizing fetch; Beetle "not read at source level"; STACK.md and FEATURES.md read Holani (GPL-3.0) and Gearlynx source (STACK lists `src/gearlynx_core.cpp`, `src/media.cpp`, README) and libretro-handy files (`lynx/system.cpp`, `rom.cpp`, `lynxdec.cpp`); PITFALLS P4 mandates the rule. Mention counts per file (Gearlynx 29/27/29/8 mentions in ARCHITECTURE/PITFALLS/FEATURES/STACK) only as a pointer; the planner's task is to produce the per-source table (what, when, which research file) from those files, not from memory. No emulator source is to be opened for this.

### Pattern 11: Hygiene scan (public repo)
- Adapt muhtari `hygiene.sh` (178 lines): `HOME_PATH='(/Users/|/home/)[A-Za-z0-9._-]+|[A-Za-z]:\\Users\\[A-Za-z0-9._-]+|-Users-[A-Za-z0-9._]+-'` with allowance `HOME_OK='(/Users/|/home/)runner'`; email pattern with allow-list `@users\.noreply\.github\.com|noreply@github\.com|noreply@anthropic\.com|@example\.(com|org|net)|git@github\.com` (this repo's history carries `Co-Authored-By: ... <noreply@anthropic.com>` lines `[VERIFIED: git log]`, so that allowance is needed for `--history`). `[VERIFIED: ran muhtari hygiene.sh --tree and --history here]`: zero home-path/email findings on the current tree and history; the only findings were muhtari's own GSD-mode policy, which must be dropped.
- **Add credential patterns** `[VERIFIED: ran identically on BSD grep via /bin/sh and GNU grep in ubuntu:24.04]`: `ghp_[A-Za-z0-9]{36}|github_pat_[A-Za-z0-9_]{22,}|gh[ousr]_[A-Za-z0-9]{36}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}`. Hits on a runtime-built `ghp_` token, an `AKIA` key and a PEM header; no hit on `ghp_short`, `~/projects/x`, `/usr/local/bin`.
- **Negative controls must build the fake token at test time (string concatenation, in an isolated temp tree), never commit it:** the repo has secret scanning and push protection enabled `[VERIFIED: gh api repos/szTheory/lynxplosion security_and_analysis]`, so a committed fake `ghp_...` or `AKIA...` could block the push or raise an alert. Also build the home-path control from parts so the script file and test source never contain a literal `/Users/<name>`; the pattern text itself is safe (`/Users/|` is not followed by a name character).
- Commit identity check (`--history`): authors/committers must be noreply. In CI use `fetch-depth: 0` (tiny repo) or the history scan sees one commit. `[REC]` run `--tree` always and `--history` when more than one commit is present.
- Scan reads files via `git ls-files -z` and an `IFS`-newline loop (muhtari handles names with newlines by failing closed) and skips symlinks.
- Interactive-shell trap: in the agent shell `grep` is a function around ugrep; verify scripts by running `sh script`, not by typing `grep` interactively.

### Pattern 12: CI (GitHub Actions)
**SHA pins verified** `[VERIFIED: gh api repos/<r>/commits/<tag>, releases/latest]`:

| Action | Latest release | Commit SHA |
|--------|----------------|-----------|
| `actions/checkout` | v7.0.1 (2026-07-20) | `3d3c42e5aac5ba805825da76410c181273ba90b1` (same as muhtari's pin) |
| `actions/cache` | v6.1.0 (2026-06-26) | `55cc8345863c7cc4c66a329aec7e433d2d1c52a9` (only if added later) |
| `actions/upload-artifact` | v7.0.2 (2026-10-07) | `cf430e030ddbb5b0abf93d22962f4752f3646cd9` (only if needed) |

**Skeleton** (muhtari ci.yml shapes: SHA pin with version comment, `permissions: contents: read`, `persist-credentials: false`, PR-only concurrency cancel, no trigger path filter):
```yaml
name: CI
on:
  pull_request:
  push:
    branches: [main]
permissions:
  contents: read
concurrency:
  group: ci-${{ github.workflow }}-${{ github.event.pull_request.number || github.sha }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}
jobs:
  check:
    runs-on: ubuntu-24.04
    timeout-minutes: 15
    steps:
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        with:
          persist-credentials: false
          fetch-depth: 0
      - run: scripts/check.sh
```
Use `pull_request` (never `pull_request_target`) so fork PRs get no secrets `[ASSUMED: standard GitHub behavior]`. Repo setting `sha_pinning_required` is currently `false` and `allowed_actions: all` `[VERIFIED: gh api .../actions/permissions]`; enabling the setting or a required-check ruleset is a repo-configuration change, so list as an optional owner-confirmed step, not part of the required plan.
**Runner facts** `[VERIFIED: Ubuntu2404-Readme.md image 20261101.360.1]`: Ubuntu 24.04.5, CMake 3.31.6, Ninja 1.13.2, Make 4.3, Git 2.56.0, shellcheck 0.9.0-1, clang 16/17/18 and clang-format/clang-tidy 16/17/18 present (helpful for the deferred formatting gate), no cc65 listed. 4 vCPU/16 GB for public repos `[CITED: docs.github.com runners reference]`. `nm`, `iconv`, `tr`, `sha256sum`, `curl`, `find` are base tools (`nm`/`iconv`/`tr`/`sha256sum` worked in the plain `ubuntu:24.04` image `[VERIFIED]`).
**Cache decision (measure first, AUTONOMY):** cc65 `make bin` + `lib` is 5-20 s here; an `actions/cache` restore plus a third-party action (trust surface) probably costs a similar amount. `[REC]` first CI run has no cache; `check.sh` prints per-step wall time (`date +%s`; `$SECONDS` is not POSIX) so the run log is the measurement. If later added: cache scope rule `[CITED: docs.github.com dependency-caching]` — a cache saved by a `pull_request` run is readable only by that PR; PR runs can read the default branch's caches, and only trusted triggers (`push`) write that scope, so a useful cache must be saved by the `push: main` run, and key it by the pin row hash (`cc65-<runner>-<sha256 of pins row + recipe version>`). A cache hit must still print the recorded build seconds and re-run the probe hash so FND-09 evidence survives.
**After first run:** `gh run view <id> --json jobs` and the log's step timings feed the efficiency review required by AUTONOMY; record outcome in the phase summary.

### Anti-Patterns to Avoid
- **One-invocation `make bin lib`** (races; Pitfall 1).
- **Hashing `lynx.lib` or the JUnit file** as evidence (not reproducible).
- **`grep -I` as the only binary test** (misses non-NUL blobs).
- **Running `nm` on sanitized objects** (adds writable symbols and runtime undefineds).
- **`memcpy` of structs as snapshot payload or digest input** (padding/endianness).
- **Committing fake secrets** for negative controls (push protection) or leaving a seeded 512-byte file where the artifact scan runs.
- **Skeleton whose state never changes** (all digest gates pass vacuously).
- **Copying muhtari's GSD stop-policy or `firmware_scan.cmake`.**

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SHA-256 | new implementation | muhtari `runner/sha256.c` (132 lines, FIPS 180-4, MIT, owner code) | Subtle; test against `shasum -a 256`, NIST `abc` and empty-string vectors `[ASSUMED]` |
| Pin fetch/verify | ad-hoc `curl` in check.sh | `scripts/fetch-pin.sh` (muhtari, 67 lines) | Hash-before-unpack, one-top-dir handling |
| Exact inventory | `ctest -N | diff` pipelines | `MuhtariTests.cmake` + `check_junit.cmake` | Handles labels, duplicates, skipped, empty runs |
| Negative-control runner | bare `WILL_FAIL` | `expect_fail.cmake` | Requires exact exit code and message |
| nm link-surface checks | new parser | `abi.cmake` | Handles Darwin underscore, platform helper allowlist, header equality |
| Float scan | regex in sh | `no_float.cmake` | Hex floats, literals, math.h handled |
| RAM power-on PRNG | LCG from memory | SplitMix64 (public domain, 4 lines) | Verified constants and permutation property |
| Test framework | Unity/cmocka | plain CTest executables with a small check header | Zero dependencies (STACK/AUTONOMY) |
| JSON/YAML parsing in check path | any parser | awk `key = value` validator | Locked decision |
| Secret scanning | custom entropy detector | fixed-prefix ERE list + GitHub push protection (already enabled) | Bounded false positives |

**Key insight:** every gate in this phase must be able to fail on purpose. The reusable value in muhtari is less the code than the habit: each check has a self-test feeding it a known fault.

## Runtime State Inventory

Not a rename/refactor/migration phase (greenfield; repo holds planning docs only). Omitted.

## Common Pitfalls

### Pitfall 1: cc65 `make bin lib TARGETS=lynx` race
**What goes wrong:** `lib` fails with `cc65: No such file or directory`. **Why:** `libsrc/Makefile` picks `../bin/cc65` at parse time via `$(wildcard ...)`; under `-j` the `bin` and `lib` goals run in parallel. **Avoid:** two sequential make invocations; set `CC65_HOME`. **Warning sign:** `Error 2` for `libwrk/lynx/_afailed.o` right after the `Using Makefile ... bin lib` banner. `[VERIFIED: reproduced]`

### Pitfall 2: Non-reproducible library hash
**What goes wrong:** FND-09 "reproducible" check hashes `lib/lynx.lib` and fails between two good builds. **Avoid:** hash tools and a linked probe (Pattern 2).

### Pitfall 3: Vacuous determinism
Equal digests mean nothing if state never changes. Include sensitivity controls and a golden value (Pattern 9).

### Pitfall 4: UBSan that does not fail
Default UBSan recovers and the process exits 0. `[VERIFIED: ran]` Apple clang: `-fsanitize=undefined` printed `runtime error: signed integer overflow` and exited 0; with `-fno-sanitize-recover=undefined` exit 134. **Avoid:** `-fsanitize=address,undefined -fno-sanitize-recover=all` in the check preset. macOS has no leak sanitizer (`detect_leaks is not supported on this platform`), so do not set `ASAN_OPTIONS=detect_leaks=1` there; Linux in Docker may need ptrace for LSan (CI runner does not). The core never allocates, so leaks only come from the runner/tests.

### Pitfall 5: Flaky or leaky negative controls
Random 512-byte blobs, fake secrets committed to git, or seeded files left in `build/` (see Pattern 5/11). Use deterministic fakes in `mktemp -d` outside the repo with trap cleanup.

### Pitfall 6: Pipeline subshell loses the failure flag
`find ... | while read` sets `fail=1` in a subshell. Observed in my scan prototype (findings printed, exit 0). Count lines or write a findings file.

### Pitfall 7: Compiler-dependent undefined symbols
Fortify/stack-protector/bzero lowering (Pattern 6). Fix with flags on the plain core and a per-platform allowlist; do not weaken to "anything starting with `mem`".

### Pitfall 8: Struct padding and endianness in digests
Zero the instance buffer on init and serialize field by field little-endian; ASan/UBSan do not detect padding differences, and MSan is unavailable on Apple clang.

### Pitfall 9: `size_t`/`uint64_t` overflow in address math
`addr + len > 0x10000` can wrap; use subtraction form. `advance` guest-time wraparound at `UINT64_MAX` must be tested.

### Pitfall 10: shellcheck version drift
Dev 0.11.0 vs runner 0.9.0; a directive valid in one can warn in the other. Print the version in check.sh; run `shellcheck -s sh`; keep scripts simple, no bashisms. `$SECONDS`, `local`, arrays and `[[` are not POSIX sh (libfuturo's 731-line `check.sh` is bash and Darwin-only: precedent for identity printing and bounded per-step timing, not for shape).

### Pitfall 11: JUnit and archives carry non-deterministic data
`timestamp`, `hostname`, per-test times (JUnit) and archive mtimes: never hashed. Pin hashes are of the archive, bundle digests are of canonical bytes.

### Pitfall 12: Tool identity drift on pin bumps
A new cc65 commit changes probe hash and may change 512-byte object coincidences; a pin bump is a dedicated PR that recomputes the archive SHA-256 and the probe hash (STACK.md decision-ownership table already says so).

## Code Examples

### Binary/non-text detection (portable, tested on macOS sh and Ubuntu dash)
```sh
# Source: prototype run this session (scratchpad/fw/scan.sh); POSIX sh
LC_ALL=C; export LC_ALL
is_ctl_binary() { [ "$(tr -d '\000-\010\013\016-\037\177' < "$1" | wc -c | tr -d ' ')" != "$(wc -c < "$1" | tr -d ' ')" ]; }
is_bad_utf8()   { ! iconv -f UTF-8 -t UTF-8 < "$1" >/dev/null 2>&1; }
is_nontext()    { is_ctl_binary "$1" || is_bad_utf8 "$1"; }
sha()           { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }
in_manifest()   { grep -s -q "^sha256 = $1\$" "$mdir"/*.manifest; }
```

### nm parsing for the plain core library
```sh
# Source: muhtari tests/cmake/abi.cmake behavior; nm -P: "name type [value size]"
nm -P build/check/liblynx_core_plain.a | awk 'NF >= 2 && $2 == "U" { print $1 }'          # undefined
nm -P build/check/liblynx_core_plain.a | awk 'NF >= 2 && $2 ~ /^[BbDdC]$/ { print $1 }'  # writable data
```
(CMake form: copy `abi.cmake` with `NM` from `CMAKE_NM`, allowlist `memcpy memmove memset memcmp` + Darwin `bzero __bzero` + Linux x86_64 `_GLOBAL_OFFSET_TABLE_`.)

### Sanitizer and lint flags (verified behavior on Apple clang; gcc 13 accepted the same ASan/UBSan spellings)
```cmake
target_compile_options(lynx_core PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror
  -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
target_link_options(lynx_core PUBLIC -fsanitize=address,undefined)
# plain twin, used only by nm checks
add_library(lynx_core_plain STATIC ${LYNX_CORE_SOURCES})
target_compile_options(lynx_core_plain PRIVATE -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector
  -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror)
```

### JUnit completeness and negative control
```cmake
# Source: muhtari tests/cmake/check_junit.cmake (58 lines), expect_fail.cmake (19 lines) [VERIFIED: read]
# CMD is a CMake list (semicolon-separated) run by expect_fail.cmake; it must exit 1 with matching text.
add_test(NAME inventory.missing_test.self_test
  COMMAND ${CMAKE_COMMAND}
    "-DCMD=${CMAKE_COMMAND};-DJUNIT=fixture-missing.xml;-DINVENTORY=fixture-inventory.txt;-P;check_junit.cmake"
    -DCODE=1 "-DREGEX=in the JUnit file 0 times" -P expect_fail.cmake)
# (shape adapted from muhtari; not run verbatim here -- the planner's first task should run it once)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Per-OS shell scripts for configure/build/test | CMake workflow presets (`cmake --workflow --preset`) | CMake 3.25, schema 6 | One command local/CI |
| Tag-based action pins | Commit SHA pins with version comment | owner portfolio practice | Supply-chain integrity |
| cc65 2.19 packaged | Pinned master commit built from source | STACK.md | 3,959 commits ahead of 2.19 (STACK.md) |
| `ubuntu-latest` | explicit `ubuntu-24.04` | runner README: `ubuntu-latest` currently maps to 24.04 while 26.04 exists | Explicit label avoids silent image change |

**Deprecated/outdated:** muhtari's CMake preset schema 11 (do not copy); `grep -I` NUL-only detection for the 512-byte rule.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `curl` accepts a `file://` URL so the fetch-pin mismatch negative control can run offline | Pattern 2 | Control needs a loopback server or committed tiny archive instead |
| A2 | BWK awk on older macOS may lack regex intervals; use length checks | Pattern 4 | None if avoided; harmless |
| A3 | SHA-256 of `abc` and empty string are standard test vectors; verify with `shasum` | Don't Hand-Roll | Wrong vector in test; caught on first run |
| A4 | Stride sampling of snapshot payload is fast enough and adequate under ASan | Pattern 7 | Too slow (reduce stride) or too weak (raise coverage) |
| A5 | `pull_request` (not `_target`) gives fork PRs no secrets | Pattern 12 | Misconfiguration risk is low but documented GitHub behavior should be re-read when writing the workflow |
| A6 | A 4-tick (4 MHz) quantum for the skeleton `advance` | Pattern 7 | Arbitrary; Phase 2 replaces it with real cycle costs; harmless if documented as provisional |
| A7 | `lynx-tests` 1.4.9 builds under the pinned cc65 | Pattern 2 | Phase 4 spike risk; not claimed here |
| A8 | `_GLOBAL_OFFSET_TABLE_` needed in the Linux allowlist | Pattern 6 | Not reproduced; if CI shows it undefined, the allowlist entry is already present; if absent, the entry is harmless |
| A9 | Applying the exact-512-byte rule across the whole pin zone stays false-positive free after pin bumps | Pattern 5 | A future pin bump may need a documented exemption |
| A10 | muhtari helpers may be copied into an all-rights-reserved repo (owner is sole author of the named files; repo has Codex/bot commits elsewhere) | Standard Stack | Licensing-notice question for the owner; low risk |

## Open Questions

1. **Where does SHA-256 live (F4)?** Core needs it for the snapshot content identity; CONTEXT places an "in-repo sha256" in `runner/`. Recommendation: one file in `core/` shared with the runner. Planner decision, low cost either way (also decide whether `lynx_sha256()` is a public utility).
2. **Artifact-zone rule (F2).** Accept "toolchain-output classes by magic, everything else must be text or manifested" and hex-text dumps? This narrows CONTEXT's wording slightly.
3. **Golden cross-compiler digest.** Add `tests/golden/skeleton.digest`? Recommended; it is not a second CI lane.
4. **Owner-level repo settings** (required check ruleset, `sha_pinning_required`): optional, needs the owner's call because it changes shared repo configuration.
5. **`lynx-tests` build under pinned cc65** remains unverified (blocked here); Phase 4 spike.
6. **Prefix and status codes** (`lynx_` vs `lxp_`): Claude's discretion; choose before writing the header because `abi.cmake symbols` mode keys on the prefix.
7. **CONTEXT/STACK says record O8 in PROJECT.md Key Decisions** — a docs task for the plan; PROJECT.md was not modified in this research.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| cc (Apple clang) | build | yes | 21.0.0 | — |
| cmake | presets/workflow | yes | 4.4.3 (floor 3.25 verified separately in container) | — |
| ninja | generator | yes | 1.13.2 | — |
| make | cc65 build | yes | GNU Make 3.81 default (worked) | — |
| shellcheck | lint | yes | 0.11.0 (runner 0.9.0) | — |
| git | build id, scans | yes | present | — |
| gh | run inspection | yes, authenticated as the owner account | present | — |
| curl, tr, wc, find, iconv, od, awk, sha256 (`shasum`/`sha256sum`) | scripts | yes (mac) / yes (Linux container) | — | — |
| docker | optional local Linux experiments | yes (daemon running; `ubuntu:24.04` image present) | 29.5.2 | CI is the Linux oracle |
| clang-format, clang-tidy | deferred gates | no on host; yes on runner (16-18) | — | deferred (CONTEXT) |
| cc65 | guest toolchain | not installed (by design: built from the pin) | commit eebf54a | — |
| libfuturo `scripts/check.sh` | precedent only | readable with the Read tool (Bash `cat` was denied in this sandbox); bash, Darwin/arm64-only, 731 lines | — | muhtari + this document |

**Missing dependencies with no fallback:** none. **Missing with fallback:** none blocking.

## Validation Architecture

Goal (AUTONOMY): zero human UAT. Every requirement maps to an automated test registered in CTest (so it is in the exact inventory) or a named `check.sh` step. Every gate has a negative control that must fail.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest (CMake 3.25+), plain C test executables with a tiny project-authored check header, sh/CMake script tests |
| Config file | `CMakePresets.json` (version 6), `CMakeLists.txt`, `tests/inventory/fast.txt`, `tests/inventory/skips.txt` |
| Quick run command | `cmake --workflow --preset check` (after the first configure, `ctest --preset check -R '^api\.'` for a slice) |
| Full suite command | `scripts/check.sh` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command (inside `ctest --preset check`) | Negative control | File Exists? |
|--------|----------|-----------|----------------------------------------------------|------------------|-------------|
| FND-01 | One command = CI gate | e2e | `scripts/check.sh` locally; CI job runs the same; `gh run view` shows green and step times | CI run on a seeded failing commit is not required; the controls below prove failure paths | Wave 0 |
| FND-02 | Hash mismatch fails, pin lint | script | `policy.pins`, `fetch_pin.mismatch.self_test` (`file://` URL, wrong digest => exit 1 + `sha256 mismatch`, DEST absent) | the mismatch test itself; `policy.pins.self_test` (http URL, short digest, duplicate name) | Wave 0 |
| FND-03 | Manifest keys present | script | `hygiene.manifests` (all `fixtures/manifests/*.manifest`), `hygiene.manifests.self_test` | manifest missing `licence`, unknown key, duplicate key, bad sha256, bad tier value => reject | Wave 0 |
| FND-04 | Firmware/binary fail-closed | script | `hygiene.firmware_scan` (source zone), post-workflow artifact-zone step in `check.sh`; `hygiene.firmware_scan.self_test` | seeded 512-byte NUL file, seeded 512-byte no-NUL file, unmanifested 100-byte binary => reject each; remove => pass; manifested 100-byte binary => pass; manifested 512-byte binary => still reject | Wave 0 |
| FND-05 | No host imports in core | script + nm | `core.includes`, `core.no_float`, `core.nm_undefined`, `core.nm_globals`, `api.abi_symbols`; `*.self_test` for each | fixtures: `#include <stdio.h>`, `float`, `static` mutable counter, `time()`/`malloc()` call => reject | Wave 0 |
| FND-06 | Exact inventory | CMake script | configure-time equality + `inventory.junit` post-step | `inventory.*.self_test`: missing test, extra test, duplicate, `<skipped/>`, unsorted inventory, undeclared skip => reject | Wave 0 |
| FND-07 | Policy + exposure record exist | script | `docs.policy` (files exist, contain `DRAFT pending O6` banner, exposure table has a row per named emulator) | table row removed => reject | Wave 0 |
| FND-08 | Claim template binds 5 fields | script | `docs.claim_template` (sample claim validates; `inventory_sha256` equals `sha256(fast.txt)`) | claim missing `profile`, stale inventory hash => reject | Wave 0 |
| FND-09 | cc65 builds from pin, wall time recorded | check.sh step | `check.sh` prints `cc65: built in Ns` and `probe sha256 = ...`; `toolchain.probe` ctest compares probe hash with its manifest | wrong expected hash in a temp manifest => reject | Wave 0 |
| API-01 | Independent instances | C test | `api.instances_independent` (two buffers; mutate one; other byte-identical; advance both) | covered by sensitivity: mutate => other unchanged is the assertion; nm globals check proves no shared state | Wave 0 |
| API-02 | Refused load leaves session unchanged | C test | `api.load_refusal_unchanged` (NULL, empty, oversize, past end, wrap) with whole-buffer `memcmp` | the tests are the controls; also run one accepted load to show mutation is detectable | Wave 0 |
| API-03 | Advance bounded | C test | `api.advance_budget` (0, 1..3, exact multiple, huge, `UINT64_MAX`); consumed <= budget, stop reason set | a deliberately over-consuming stub is not in product code; assertions cover boundaries | Wave 0 |
| API-04 | Reset to declared power-on | C test | `api.reset_power_on` (policy 0: all RAM 0x00; policy 1 seed S: reproducible RAM; reset after run == fresh init; seed and policy id reported) | different seed => different RAM | Wave 0 |
| API-05 | Inspect has no side effects | C test | `api.inspect_pure` (whole-instance `memcmp` and guest time unchanged across register/memory reads incl. out-of-range reads returning a status) | — | Wave 0 |
| API-07 | Snapshot restore + refusal | C test | `api.snapshot_roundtrip`, `api.snapshot_refusal` (flip header fields: magic, version, build id, profile, content sha, policy, seed, length, payload hash; stride payload flips; truncated blob; extra trailing bytes) each => status + instance `memcmp` identical | the flips are the controls | Wave 0 |
| DET-01 | Two fresh runs equal | e2e | `det.two_runs` (runner invoked twice as separate processes; compares observation and bundle digests) | `det.sensitivity` (seed/content/budget/state-bit change => digest differs) | Wave 0 |
| DET-02 | Two instances in one process equal | C test | `det.two_instances` compares to separate-run digests | same sensitivity controls | Wave 0 |
| DET-04 | Run manifest has required fields | script | `det.manifest_fields` (all keys present, no timestamps/absolute paths: scan manifest for `/`-rooted path and ISO dates) | manifest missing `core_build` => reject | Wave 0 |
| (cross-compiler) | Golden observation digest equal on macOS clang and Linux gcc | e2e | `det.golden` compares to `tests/golden/skeleton.digest` | edited golden => fails | Wave 0 (recommended) |

### Sampling Rate
- **Per task commit:** `ctest --preset check -R '<slice>'` (seconds) plus `shellcheck -s sh scripts/*.sh`.
- **Per wave merge:** `scripts/check.sh` (full gate; expected well under a minute locally, CI time to be measured).
- **Phase gate:** `scripts/check.sh` green locally and the same commit green on GitHub Actions (`gh run view`), wall times recorded, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `CMakeLists.txt`, `CMakePresets.json`, `cmake/Tests.cmake` (registration == inventory), `tests/cmake/{check_junit,expect_fail,no_skip,abi,no_float,pins}.cmake` (copy + rename)
- [ ] `scripts/{check.sh,fetch-pin.sh,hygiene.sh,scan-binaries.sh,validate-kv.sh}`, `pins.txt` (cc65, lynx-tests)
- [ ] `tests/inventory/fast.txt`, `skips.txt`; tiny check header for C tests
- [ ] `include/lynxplosion.h`, `core/*`, `runner/*`
- [ ] `.github/workflows/ci.yml`, `README.md` (all rights reserved; O5 open), `docs/policy/clean-room.md`, `docs/claims/claim.template`, manifests

## Security Domain

`security_enforcement` is enabled (absent = enabled; config shows `security_enforcement: true`, ASVS level 1).

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no (no users) | — |
| V3 Session Management | no | — |
| V4 Access Control | partial (CI token scope) | `permissions: contents: read`, `persist-credentials: false`, no secrets used |
| V5 Input Validation | yes | Public API refusals (NULL/length/alignment/overflow-safe address math), snapshot full-validation-before-write, manifest/pins/claim strict parsers |
| V6 Cryptography | partial | SHA-256 used for identity/integrity only (not secrecy); implement from FIPS 180-4 (muhtari copy) and test against known vectors; no custom crypto beyond hashing |
| V8/V14 Data protection, configuration, supply chain | yes | SHA-256-verified pins before unpack, commit-SHA action pins, PII/credential hygiene scan, fail-closed rights scan, no network access at test time |

### Known Threat Patterns
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Tampered or substituted source archive | Tampering | SHA-256 in `pins.txt` verified before unpack; fallback identity = git commit |
| Malicious or drifting GitHub Action | Tampering | Commit-SHA pins with version comment; read-only token |
| Hostile PR altering CI/scripts | Elevation | `pull_request` trigger, no secrets, `timeout-minutes`, concurrency cancel |
| Cache poisoning of toolchain binaries | Tampering | Default no cache; if added, key by pin hash, rely on main-scoped writes (only `push` writes that scope), re-run probe hash |
| Firmware/copyright content entering repo or artifacts | Info disclosure / legal | Hash-keyed manifest allowlist, 512-byte hard rule, text-only artifacts, fail-closed scans, seeded controls |
| PII/credential leak in public repo | Info disclosure | Home-path/email/token patterns, noreply identity check, push protection (already enabled), history scan |
| Memory-unsafe API misuse (OOB, overflow, UB) | Tampering/DoS | Overflow-safe checks, ASan+UBSan non-recoverable in the gate, snapshot validation before apply, corruption-flip tests |
| Shell injection via file names | Tampering | `git ls-files -z`, newline-IFS loops that fail closed, quote everything, `set -f` |
| Archive extraction path traversal | Tampering | Mitigated by hash-before-unpack of pinned content; `[ASSUMED]` acceptable since contents are fixed by hash |

## Flagged Risks and Corrections to CONTEXT.md (not overrides)

| # | CONTEXT item | Finding | Recommendation |
|---|--------------|---------|----------------|
| F1 | `make bin lib TARGETS=lynx` (one command) | Reproduced race; fails with `-j` | Two sequential invocations; keep wall time as the sum |
| F2 | Scan "build artifact dirs" for any unmanifested binary | Compiler outputs are binary; `build/pins` legitimately holds 76 binary test refs | Explicit pin-zone exclusion, magic-class allowance for toolchain outputs, hex-text dumps, artifact scan after the workflow |
| F3 | "binary (NUL or non-text)" detection | `grep -I` is NUL-only on BSD/GNU grep; ~13.5% of random 512-byte blobs have no NUL | `tr` control-byte test + `iconv` UTF-8 test |
| F4 | `runner/` holds sha256 | Snapshot header needs content SHA-256 computed in the core | One sha256 unit in `core/` used by runner too |
| F5 | nm check "plus sanitizer runtime symbols in sanitizer builds" and "no writable data/bss" | Sanitized objects gain writable symbols (Mach-O) and runtime undefineds | Plain companion library for nm checks |
| F6 | nm allowlist `mem*` only | Apple clang adds `___mem*_chk` (fortify) at -O0 and `bzero` lowering; Ubuntu gcc adds stack-protector/fortify helpers | Flags `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector` and per-platform allowlist |
| F7 | "ASan+UBSan in the check preset" | Default UBSan does not fail the process | `-fno-sanitize-recover=all` |
| F8 | Manifest keys (10) | SUMMARY line 300 also lists `independence`; no `path` key | Hash-keyed allowlist needs no path; consider `independence` optional |
| F9 | `FND-09` "reproducibly" | `lynx.lib` bytes differ between builds; tools and linked output are identical (also cross-host) | Define FND-09 evidence as tool hashes + probe `.lnx` hash |
| F10 | CI on one runner only, floor 3.25 | CI uses 3.31.6, dev 4.4.3, so the floor is unexercised by the gate | One-off verified (3.25.3); document, no new lane |

## Sources

### Primary (HIGH confidence)
- Local runs this session (macOS arm64, Apple clang 21.0.0, cmake 4.4.3, GNU make 3.81, `/usr/bin/grep` BSD 2.6.0; `ubuntu:24.04` amd64 container with gcc 13.3.0, cmake 3.28.3, Kitware cmake 3.25.3 tarball with checked SHA-256, GNU grep 3.11, GNU Make 4.3): cc65 and lynx-tests archive hashes; cc65 two-step build, timings, race, tool-binary and probe reproducibility incl. cross-OS; `nm -P` behavior; UBSan recover behavior; `tr`/`iconv`/`grep` behavior; credential regex behavior; workflow preset prototype.
- `https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html` (+ v3.25/v3.26/v3.28 pages: `workflowPresets`, `outputJUnitFile`, `noTestsAction` present at 3.25) and ctest manual (`--no-tests`).
- `https://raw.githubusercontent.com/actions/runner-images/main/images/ubuntu/Ubuntu2404-Readme.md` (image 20261101.360.1) and runner-images README (label table).
- `https://docs.github.com/en/actions/reference/runners/github-hosted-runners` (4 vCPU/16 GB public Linux) and `.../dependency-caching` (cache scoping, limits).
- `gh api` for action tags/SHAs, repository security settings and Actions permissions.
- `https://prng.di.unimi.it/splitmix64.c` (public domain SplitMix64).
- `~/projects/muhtari` (MIT; owner project) files read: `pins.txt`, `scripts/fetch-pin.sh`, `scripts/hygiene.sh`, `tests/cmake/{check_junit,expect_fail,no_skip,no_float,abi,pins,firmware_scan}.cmake`, `cmake/MuhtariTests.cmake`, `runner/sha256.{c,h}`, `CMakePresets.json`, `.github/workflows/ci.yml`, `tests/inventory/hygiene.txt`.
- `~/projects/libfuturo/scripts/check.sh` (first 80 lines and a grep of timing variables via Read/Bash): bash, Darwin/arm64-only, 731 lines.
- Project files: AGENTS.md, CONTEXT.md, REQUIREMENTS.md, STATE.md, AUTONOMY.md, ROADMAP.md, `.planning/research/STACK.md`, SUMMARY.md, `.planning/config.json`.

### Secondary (MEDIUM confidence)
- STACK.md/SUMMARY.md/ARCHITECTURE.md statements (dated project research) re-verified only where listed above.

### Tertiary (LOW confidence / unverified)
- `lynx-tests` building under the pinned cc65 (not run; blocked by sandbox).
- CI wall times (no run yet).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, tool versions and behaviors measured on both OS families.
- Architecture: HIGH for harness/gates, MEDIUM for skeleton details (provisional by design).
- Pitfalls: HIGH, most reproduced.
- CI timing and cache payoff: MEDIUM-LOW until a real run.

**Research date:** 2026-11-09
**Valid until:** 2026-12-09 (fast-moving: runner image contents, action SHAs, CMake 4.x, cc65 master; the pins themselves are immutable)
