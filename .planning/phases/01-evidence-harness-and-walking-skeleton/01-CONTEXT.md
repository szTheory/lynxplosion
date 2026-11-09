# Phase 1: Evidence Harness and Walking Skeleton - Context

**Gathered:** 2026-11-09
**Status:** Ready for planning

<domain>
## Phase Boundary

A developer runs one command (`scripts/check.sh`) that fetches pinned sources
with SHA-256 verification, builds the core, lints, and runs the full fast test
set against an exact inventory. The command enforces the rights boundary
(unmanifested or firmware-like binaries fail), the determinism boundary (no
host imports in the core) and the PII boundary. It proves that a skeleton
machine behind the public C API is repeatable across fresh runs and instances,
and that it refuses bad input with no side effects. The same command is the
PR CI gate on the public GitHub repository. Covers FND-01..09, API-01..05,
API-07, DET-01, DET-02, DET-04. Out of scope here: CPU (Phase 2), memory map
and real direct-load format (Phase 3), input timeline (Phase 5).

</domain>

<decisions>
## Implementation Decisions

### Owner gates and authority
- **O8 accepted (owner, 2026-11-09):** C17 (`CMAKE_C_EXTENSIONS OFF`,
  `-pedantic-errors`), CMake ≥3.25 with workflow presets, Ninja, CTest, and a
  C ABI with an opaque handle. Record this in PROJECT.md Key Decisions.
- **O6:** proceed with a draft clean-room policy
  (`docs/policy/clean-room.md`). Implementers do not open GPL emulator source
  (Gearlynx, MAME, Holani, Beetle). Research exposure is recorded per source:
  what was read, when, and by which research file. FND-07 stays marked
  "draft pending O6".
- **Repository is public (owner, 2026-11-09):** `github.com/szTheory/lynxplosion`
  was created and pushed during discuss. Conditions: no PII and no
  credentials. History was already scrubbed of local home paths. CI is GitHub
  Actions on `ubuntu-24.04`, running `scripts/check.sh` and nothing else, with
  actions pinned by commit SHA. FND-01 and FND-09 CI evidence comes from a
  real CI run in this phase; record CI wall time. No licence file is added:
  O5 stays open (all rights reserved until decided). A README must say so.
- `.planning/AUTONOMY.md` created (points to AGENTS.md; records the discuss,
  verify and CI defaults).

### Repository layout and check command
- Layout: `include/lynxplosion.h` (public API), `core/` (machine; no host
  imports; also holds the single in-repo SHA-256 unit, because the core
  hashes snapshot content and payload — amended 2026-11-09 per RESEARCH F4),
  `runner/` (headless runner and bundle writer; uses the core's SHA-256),
  `tests/`, `scripts/`, `fixtures/manifests/`, `docs/policy/`. No empty
  placeholder directories.
- `scripts/check.sh`: POSIX sh, shellcheck-clean. Steps: print tool
  identities (compiler, cmake, ninja, cc65 pin); run `scripts/fetch-pin.sh`
  for each `pins.txt` row (SHA-256 verified before unpack); build cc65 into
  `build/pins/` cached by commit stamp (`make bin lib TARGETS=lynx`, wall time
  recorded); run `cmake --workflow --preset check`; run hygiene scans. Any
  missing pin fails loudly.
- Lint: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`; ASan+UBSan
  in the check preset; shellcheck on `scripts/*.sh`. Formatting gate deferred
  (clang-format is not on the host).
- Exact inventory: committed `tests/inventory/fast.txt` plus
  `tests/inventory/skips.txt` (each skip with a reason). A CMake script
  compares `ctest -N` and the JUnit output against them. A missing, extra or
  undeclared skip fails. Include a negative control showing that an extra or
  missing test fails the inventory check.

### Skeleton machine and API contract
- Instance memory is caller-provided: `lynx_machine_size()` plus init in
  place. The core never calls malloc and links only `memcpy`, `memmove`,
  `memset` and `memcmp`. Instances share no mutable state (API-01).
- Time: advance takes a `uint64_t` budget in 16 MHz master ticks and returns
  ticks consumed plus a stop reason enum (`BUDGET`, `FRAME`, `WATCH`, `HALT`).
  `FRAME` and `WATCH` are defined but reserved until later phases. Advance
  never exceeds the budget (API-03).
- Content: Phase 1 uses a provisional raw RAM image (bytes plus a load
  address), documented as provisional; the Phase 3 direct-load format
  replaces it. Refusals (NULL, empty, oversize, past the end of the address
  space) return a status code and leave an existing session byte-identical
  (API-02).
- Reset and power-on: RAM power-on policy is fixed-0x00 by default, with an
  integer PRNG option (seeded). The policy id and seed are recorded in run
  identity. Reset returns to the declared power-on state (API-04).
- Inspect: register and memory reads are side-effect free; tests prove that
  state and guest time do not change (API-05).
- Snapshot: a versioned blob holding magic, format version, core build
  identity (git rev + compiler id baked at build time), profile id, content
  SHA-256, RAM policy and seed, then state. Every field is validated before
  any state is replaced, and a mismatch is refused with no state change
  (API-07).

### Evidence, manifests and determinism gates
- Manifests: one line-oriented `key = value` file per fixture under
  `fixtures/manifests/`, parsed by sh/CMake (no Python or JSON in the check
  path). Required keys: source, revision, recipe, sha256, licence, notices,
  allowed_use, startup_tier, profile, evidence_tier. A validator rejects
  missing keys, with a negative control.
- Firmware/binary scan: fail on any binary (NUL or non-text) file not listed
  in a manifest, across tracked files, untracked files and build artifact
  dirs; also fail on any binary of exactly 512 bytes regardless of manifest.
  A test seeds a fake 512-byte file in an isolated copy and asserts
  rejection, then asserts a pass once it is removed (FND-04).
- **Amendment (2026-11-09, plan-check B-1; keeps the rule's intent):** "listed in
  a manifest" is satisfied in exactly two derived ways, and nothing else is
  exempt. (1) Pin zone: a non-text file under `build/pins/<name>/` passes only if
  its SHA-256 appears in a per-file list that `fetch-pin.sh` writes from the
  SHA-256-verified archive at extraction time. Any file not on that list (added,
  modified or unpacked from elsewhere) fails. (2) Toolchain outputs: object,
  static-archive and executable formats (ELF, Mach-O, `ar`) and ninja/CMake state
  files pass only inside the CMake binary dir's own output paths, identified by
  magic bytes. The same magic anywhere else (source tree, `build/pins/`,
  bundles, fixtures) fails. The exact-512-byte rule applies everywhere with no
  exemption. Seeded controls: an unlisted non-512 binary under `build/pins/`,
  an ELF/Mach-O-magic file outside the toolchain output paths, and a 512-byte
  file inside each zone must all be rejected.
  (3) Pin build outputs (resolution of planner REVISION_CONFLICT RC-1, option
  A1): after both cc65 make runs and the probe succeed, the cc65 step writes a
  second derived list, `build/pins/cc65/.lynx-build-files` (SHA-256 + path of
  every file the pinned recipe added under `build/pins/cc65/`), stamped with
  the pin digest and recipe version. Pin-zone files pass only if they appear,
  path and hash matching, on the archive list or this build list. Any file
  added or modified after that point fails. The cache is reused only when the
  archive list, the build list and the stamp all agree.
- **Corrections from research (recorded, research-backed):** the cc65 recipe is
  two sequential make invocations (`make bin`, then `make lib TARGETS=lynx`),
  because the single invocation races (F1). Inventory equality is checked at
  configure time against registration, and the JUnit step covers the executed
  set (in place of `ctest -N`).
- RAM power-on policy default is fixed-0x00 everywhere (config default, runner
  CLI default, tests).
- Forbidden imports (FND-05): two layers. (a) A `#include` allowlist for
  `core/` (`stdint.h`, `stddef.h`, `stdbool.h`, `string.h`) plus a token scan
  for `float`/`double`. (b) An `nm` check that the core static library's
  undefined symbols are only `mem*` (plus sanitizer runtime symbols in
  sanitizer builds) and that there is no writable data/bss. Each layer has a
  negative-control fixture that must fail.
- PII/credential hygiene scan (owner condition for the public repo): fail on
  absolute home paths (`/Users/<name>`, `/home/<name>`), non-noreply email
  addresses and common credential patterns in tracked files, with a negative
  control.
- Artifact bundle: the runner writes `run.manifest` (profile, startup tier,
  content sha256, input digest, tick count, core build identity, RAM policy
  and seed), state dumps, and `digests.txt`. The bundle digest is the SHA-256
  of the sorted `digests.txt`, with no timestamps or host paths. Gates: two
  fresh runs, and two instances in one process, give equal digests
  (DET-01/02/04). Same-core digests are labelled repeatability, not accuracy.
- Claim template (FND-08) binds profile, content identity, startup tier,
  evidence tier and test inventory.

### Claude's Discretion
- Exact function names and prefix (e.g. `lynx_` vs `lxp_`), status code
  values, internal file split inside `core/` and `runner/`.
- The exact wording of policy documents, kept plain and short.
- The integer PRNG choice for the RAM policy (any small, documented,
  integer-only generator).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- No code yet: the repository holds planning documents only.
- Owner-portfolio precedent (read for patterns only, copy small helpers where
  useful, licence permitting): muhtari `pins.txt` + `scripts/fetch-pin.sh`,
  `tests/inventory` + JUnit check pattern, `runner/` in-repo sha256;
  libfuturo `scripts/check.sh` shape. Both are owner projects under
  `~/projects/`.

### Established Patterns
- Sibling cores use C17, CMake presets, `cmake --workflow --preset`, actions
  pinned by commit SHA.

### Integration Points
- `pins.txt`: cc65 `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2`
  (SHA-256 of archive recorded in `.planning/research/STACK.md`; verify);
  lynx-tests tag 1.4.9 `b583dc43…` (fetched and verified only in this phase;
  suites are built in Phase 4).
- GitHub remote `origin` = `https://github.com/szTheory/lynxplosion.git`.

</code_context>

<specifics>
## Specific Ideas

- "Another copy-paste is better than another dependency": zero runtime
  dependencies in the core; small project-authored helpers in the runner.
- Verification is automated by default (see `.planning/AUTONOMY.md`): aim for
  zero human UAT. CI-runner evidence comes from a real Actions run on the
  public repo, inspected with `gh run view`.
- After the first CI run, do the CI efficiency review (critical path, cc65
  build caching keyed by pin) and record the outcome.

</specifics>

<deferred>
## Deferred Ideas

- Second CI runner / compiler-family cross-check lane: add only after a
  distinct digest-mismatch class is demonstrated (later phase).
- clang-format formatting gate, clang-tidy: later, when pinned.
- Project licence (O5): owner decision; the public repo is all rights
  reserved until then.

</deferred>
