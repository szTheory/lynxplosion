# Walking Skeleton — Lynxplosion

**Phase:** 1
**Generated:** 2026-11-09

## Capability Proven End-to-End

A developer runs `scripts/check.sh` on a clean checkout. It builds `lynx_core` with CMake. One C test creates a machine instance in caller-provided memory, loads a provisional RAM image, advances it by a tick budget and inspects it with exact expected values; the registered tests equal the committed inventory; GitHub Actions runs the same command on the public repository and goes green (plan 01-01). The core then reports content and build identity (plan 01-02), and the headless runner writes an artifact bundle whose digests are equal across two fresh runs (plan 01-03). Plans 01-01 and 01-03 together are the walking skeleton; plan 01-02 supplies the identity the bundle records.

## Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Language and standard | C17, `CMAKE_C_EXTENSIONS OFF`, `-pedantic-errors` | Owner decision O8 (2026-11-09) accepted the research default |
| Build, test and gate | CMake 3.25 floor, presets schema 6, one workflow preset `check` (configure, build, test), Ninja, CTest with JUnit output and `noTestsAction: error` | One command for laptop and CI. Schema 6 is the lowest schema with workflow presets (RESEARCH Pattern 1, F10) |
| Check driver | `scripts/check.sh`, POSIX sh, shellcheck-clean. Prints tool identities and per-step wall time | CONTEXT. The same script is the only CI step |
| Public API | `include/lynxplosion.h`, C ABI, opaque `lynx_machine`, prefix `lynx_` (public) and `lynx__` (internal externs). Status codes are `uint32_t` `lynx_status`. `lynx_config_default()` gives the default config | O8. Prefix chosen under Claude's discretion; `abi.cmake symbols` keys on it (RESEARCH open question 6) |
| Instance memory | Caller-provided. `lynx_machine_size()` and `lynx_machine_align()`, then `lynx_machine_init()` in place. The core never allocates | API-01. Whole-buffer `memcmp` becomes the oracle for refusal and inspect tests |
| Core boundary | `core/` includes only `stdint.h`, `stddef.h`, `stdbool.h`, `string.h`. It links only `memcpy`, `memmove`, `memset` and `memcmp` (plus per-platform helpers such as Darwin `bzero`). It has no writable globals, no floating point and no host clock, filesystem, threads or network | FND-05. Enforced by include/float scans and by `nm` on an uninstrumented twin `lynx_core_plain` |
| Time | `lynx_advance()` takes a `uint64_t` budget in 16 MHz master ticks. It returns ticks consumed (never more than the budget) and a stop reason: `BUDGET`, `FRAME`, `WATCH` or `HALT`. `FRAME` and `WATCH` are reserved until later phases | API-03, CONTEXT |
| Provisional machine | A skeleton step costs 4 master ticks and cycles the PC inside the loaded image. It mixes registers and writes one stack-page byte. Byte `0xDB` at the PC halts the machine. The PC advances with 32-bit arithmetic, so an image ending exactly at `0x10000` wraps back to its start. **This is provisional and is not a CPU.** Phase 2 replaces it | Gives digests real dependence on content, seed and budget, so determinism gates are not vacuous |
| Content | Provisional raw RAM image: bytes plus a load address. Phase 3 replaces it with the direct-load format. Fixtures are committed as hex text | API-02. Text fixtures keep the binary scan simple |
| RAM power-on | Policy 0 = fixed `0x00` (the default everywhere: `lynx_config_default`, the runner CLI, tests); policy 1 = SplitMix64 seeded fill (public domain, state held in the instance). The policy id and seed are part of run identity | API-04, CONTEXT |
| Identity | SHA-256 lives in `core/` (FIPS 180-4, adapted from muhtari, MIT, owner project). Build identity (git revision, dirty flag, compiler, system) is baked in at configure time | RESEARCH F4, Pattern 8 |
| Evidence bundle | The runner writes `run.manifest` (`key = value`), observation dumps as hex text, and a sorted `digests.txt`. The observation digest is build-independent and is checked against a committed golden on macOS clang and Linux gcc. The bundle digest includes build identity. Every digest is labelled "repeatability (R), not accuracy" | DET-01/02/04, RESEARCH Pattern 9 |
| Test scratch | Every file a test or configure-time fixture writes lives under `build/check/scratch/` (`LYNX_SCRATCH_DIR`); negative controls that seed binaries or fetch payloads use `mktemp -d` outside the repository and `build/` | Keeps toolchain-output paths distinct from test output for the rights scan |
| Rights scan zones | CONTEXT amendment 2026-11-09: non-text files pass only by fixture-manifest SHA-256, by a path and SHA-256 match in the pin zone against the archive list written by `fetch-pin.sh` or the cc65 build list `build/pins/cc65/.lynx-build-files` written after a successful build (amendment item 3, RC-1 resolution), or (ELF, Mach-O, `ar`, ninja state) by location in `build/check/` outside `scratch/`. The exact-512-byte rule applies everywhere | FND-04 |
| Exact inventory | `tests/inventory/fast.txt` (`name<TAB>labels`, sorted) must equal the registered tests at configure time. The JUnit output must show every name ran exactly once. Skips are allowed only when listed in `tests/inventory/skips.txt` | FND-06 |
| Negative controls | Every gate has a `*.self_test` that feeds it a built fault. `tests/cmake/expect_fail.cmake` requires an exact exit code and message | AGENTS.md "meaningful negative controls" |
| Directory layout | `include/`, `core/`, `runner/`, `tests/{api,det,abi,header,gates,hygiene,cmake,inventory,golden}`, `scripts/`, `fixtures/{manifests,skeleton,toolchain}`, `docs/{policy,claims}`, `cmake/`, `.github/workflows/` | CONTEXT layout. No empty placeholder directories |
| Deployment / CI | GitHub Actions on `ubuntu-24.04`. One job runs `scripts/check.sh`. Actions are pinned by commit SHA, with `permissions: contents: read` and `persist-credentials: false` | CONTEXT (repository is public, owner 2026-11-09) |
| Guest toolchain | cc65 pinned at commit `eebf54af…`, verified by SHA-256 before unpacking, built in two make invocations (`bin`, then `lib TARGETS=lynx`; CONTEXT "Corrections from research"). `fetch-pin.sh` records a per-file SHA-256 list of the verified archive. Reproducibility is proven by a linked probe `.lnx` hash, not by `lynx.lib` | FND-09, RESEARCH F1/F9 |

## Stack Touched in Phase 1

- [ ] Project scaffold: CMake presets, C17 flags, `-Werror`, non-recoverable ASan+UBSan, shellcheck, one check command
- [ ] Core: one real instance created in caller memory, a real load, a bounded advance and a side-effect-free inspect (the equivalent of a real read and write)
- [ ] Host surface: the headless runner drives the public API and writes a real artifact bundle
- [ ] Gates: exact inventory, core boundary, rights/binary scan, PII scan, pins, manifests, claims, each with a negative control
- [ ] Deployment: GitHub Actions runs `scripts/check.sh` on the public repository, and the run is inspected with `gh run view`

## Out of Scope (Deferred to Later Slices)

- The 65C02-class CPU and the Lynx I/II profiles (Phase 2). Phase 1 profile is `none`, because O1 is open.
- The MAPCTL memory map, the real direct-load format and the original diagnostic (Phase 3).
- Timers, IRQ and the public lynx-tests suites (Phase 4). Phase 1 only fetches and verifies the lynx-tests pin.
- Display, raw video and input timeline (Phase 5); audio (Phase 6).
- BIOS, boot ROM, HLE and `.lnx` cartridge boot (O2/O3/O4, not scheduled).
- Second CI lane or compiler-family cross-check lane; clang-format and clang-tidy gates (CONTEXT deferred).
- Project licence (O5 open: all rights reserved), releases and repo-setting changes (required-check ruleset, `sha_pinning_required`): these are owner decisions.

## Subsequent Slice Plan

Each later phase adds one vertical slice on this skeleton without changing its architectural decisions:

- Phase 2: the CPU replaces the provisional skeleton step behind the same `lynx_advance()`. Lynx I and Lynx II profiles are data; per-opcode vectors are pinned and inventoried.
- Phase 3: the MAPCTL memory map, startup tier A direct load replacing the provisional image, and an original cc65 diagnostic read through a RAM result block.
- Phase 4: the master-tick scheduler, timers and IRQ, and the `timers` and `cpu` suites built from the lynx-tests pin.
- Phase 5: timer-driven frames, raw video and the logical input timeline (`input_digest` stops being the empty-timeline value).
- Phase 6: audio, the playable checkpoint and a throughput baseline.
