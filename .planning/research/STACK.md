# Stack Research

**Domain:** Accurate, deterministic, headless-testable Atari Lynx emulator core (C-ABI embeddable)
**Researched:** 2026-11-09 (all "observed" facts were rechecked on this date at the named primary source)
**Confidence:** HIGH on language/build/guest-toolchain/fixture facts; MEDIUM on boot-path feasibility (nothing here was executed); LOW on the "free boot ROM" provenance

**Claim labels used below:** `[SO]` source observation (read at the primary source on 2026-11-09, commit/tag named); `[MR]` maintainer report (a project's own statement, not independently verified); `[INF]` inference from observations; `[REC]` recommendation. Nothing in this file is an owner decision. Items an owner must decide are collected under "Decision ownership".

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **C17** (ISO C, `CMAKE_C_EXTENSIONS OFF`, `-pedantic-errors`) | C17 | Language of the machine core, runner and public header | `[SO]` Every sibling core in the owner's portfolio checked locally (nesturbator, muhtari, gabbaboy, genewhip) is `LANGUAGES C` with `CMAKE_C_STANDARD 17` (gabbaboy/genewhip/muhtari/nesturbator CMakeLists, read 2026-11-09), and libfuturo itself is a C17 SDK whose public surface is a C header. `[INF]` A C core plugs into LibFuturo/libretro-style hosts with zero glue, no runtime, and no second toolchain. Lynx hardware is integer-only (4 MHz-class 65C02, 16 MHz master clock, integer audio/blitter/math), so there is no floating-point or SIMD reason to leave C. **Confidence: HIGH** that C17 is the right default for this owner; **MEDIUM** that it beats Rust on pure memory-safety merit (see Alternatives). |
| **CMake** | floor `3.25` (`cmake_minimum_required(VERSION 3.25)`); dev host has 4.4.3; latest release 4.4.5 (2026-10-27) | Configure/build/test, presets, one workflow entrypoint | `[SO]` Same floor as libfuturo/nesturbator/muhtari/gabbaboy. `[REC]` 3.25 is where `workflowPresets` (`cmake --workflow --preset check`) became available, so one command is identical locally and in CI with no script drift (muhtari's documented approach, `.planning/milestones/v1.0-research/STACK.md`). **HIGH** |
| **Ninja** | 1.13.2 (latest, 2025-11-20; host has 1.13.2) | Build backend | `[SO]` Already on the dev host; fast, deterministic ordering. **HIGH** |
| **CTest** (bundled with CMake) | = CMake | Test runner, JUnit output, exact inventory | `[REC]` Plain CTest executables, no framework. Exact-inventory and skip/missing visibility are then enforced with a small CMake script comparing `ctest -N` / JUnit to a committed inventory file (muhtari's `tests/inventory/*.txt` + `check_junit.cmake` pattern, `[SO]` seen locally). **HIGH** |
| **Apple clang 21.0.0** (macOS dev) and the runner's default gcc/clang on **ubuntu-24.04** | host-observed | Compilers | `[SO]` Host: Apple clang 21.0.0 on macOS 26.6.2 arm64. `[SO]` GitHub: `ubuntu-latest` = Ubuntu 24.04 x64, `macos-latest` = macOS 26 arm64; macOS 14 images retire 2026-11-02 (actions/runner-images README). `[REC]` Pin explicit labels (`ubuntu-24.04`, `macos-15`), never `-latest`, so a runner-image change cannot silently change results. Two different compilers on two OS/arch pairs is also the cheapest determinism cross-check (see Patterns). **MEDIUM-HIGH** |
| **cc65** (guest toolchain) | **commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2`** (master, 2026-11-08), built from source; **not** V2.19 | Assemble/link guest fixtures (`ca65`, `cc65`, `ld65`, `cl65 -t lynx`) | See "Guest toolchain" below. License Zlib `[SO]`. **HIGH** that a pinned commit beats V2.19; **MEDIUM** that this exact commit is the best pin (chosen as current HEAD, not tested). |
| **lynx-tests** (fixture source, not a dependency of the core) | tag `1.4.9` = commit `b583dc43d532d3c41563a6fa35420614fbcead89` (2026-08-13); repo HEAD `a920f01164d37edff94939993fbc0551e5b7000b` (2026-09-20) differs from 1.4.9 only in `.github/FUNDING.yml` and one `.md` | Public hardware-informed test ROMs | See "Fixture sources" below. MIT `[SO]`. **HIGH** |

### Database

None. The core has no persistent store. `[REC]` Saves/EEPROM are exchanged as host-owned byte buffers tied to an explicit cartridge identity (a separate API/contract decision, not a stack choice).

### Infrastructure

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| GitHub Actions | runners `ubuntu-24.04`, `macos-15` (arm64) to start | Fast PR gate = the same `cmake --workflow --preset check` | `[SO]` Sibling pattern (muhtari ci.yml: every job runs `cmake --workflow --preset`, actions pinned by commit SHA with version comment). `[REC]` Start with 2 runners; add Windows/arm/big-endian lanes only when a distinct failure class justifies them and after measuring queue/setup/build/test time (AGENTS.md). |
| `pins.txt` + `scripts/fetch-pin.sh` | in-repo, ~60 lines of POSIX sh | SHA-256-verified fetch of every pinned external tool/fixture source | `[SO]` Precedent in `~/projects/muhtari` (`pins.txt` rows `name version url sha256`; `fetch-pin.sh` downloads to a temp dir, verifies SHA-256, only then unpacks). `[REC]` Copy the pattern (a small copied helper beats a new dependency, per AGENTS.md); do not adopt a package manager. |
| ASan + UBSan | compiler built-in | Memory/UB detection in the check lane | `[REC]` On by default in the local check; UB is the main way a C core silently becomes non-deterministic. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `libretro.h` (MIT, vendored single header) | pin to a libretro-common commit | libretro frontend wrapper | Only when a libretro wrapper is built (host lane). `[SO]` muhtari vendors only this header. Not needed by the core. |
| In-repo `sha256.c`, `crc32.c`, tiny PNG writer | project-authored, <300 lines each | Evidence hashing and frame dumps in the **runner** | `[SO]` muhtari's `runner/` carries exactly these instead of zlib/libpng/OpenSSL. Keep them in the runner, never in the core. |
| SingleStepTests `65x02` JSON vectors (MIT, last push 2025-05-10) | pin a commit; fetch with `fetch-pin.sh`, commit only a reviewed sample | One-instruction CPU state vectors (dirs: `6502`, `65c02`-family `wdc65c02`, `rockwell65c02`, `synertek65c02`, `nes6502`) | CPU component tests once the CPU profile is chosen. `[SO]` README: vectors are generated by an implementation "conforming to all available documentation" and verified in an emulated machine, i.e. not hardware. `[INF]` Use for functional state only; its per-cycle bus traces assume standard 65C02 timing, which Lynx page-mode fetch does not share. Lynx I reportedly lacks RMB/SMB/BBx (lynx-tests README `[MR]`), so exclude/qualify those opcodes. |

No other runtime or test library is recommended. `[REC]` Runtime dependency count for the core: **zero** (links only `memcpy/memmove/memset/memcmp`, muhtari's stated rule).

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `scripts/check.sh` | The single documented local command | Thin wrapper: verify toolchain presence, run `scripts/fetch-pin.sh` for pinned guest tools, then `cmake --workflow --preset check`. Mirrors `libfuturo/scripts/check.sh` `[SO]`. Must print compiler/CMake/SDK identities and fail loudly on missing pins. |
| `CMakePresets.json` | Presets: `check` (Debug + ASan/UBSan + all tests), `release-bench` (later), `hygiene` | One definition shared by laptop and CI. |
| clang-format (pin one version) | Formatting | `[SO]` muhtari pins clang-format 21; not installed on this host (`clang-tidy`, `cppcheck` also absent). `[REC]` Routine/reversible; start with compiler warnings (`-Wall -Wextra -Wconversion -Wshadow`, warnings-as-errors in the check preset) and add clang-tidy analyzer checks as advisory later. |
| `shellcheck` | Lint `scripts/*.sh` | `[SO]` Already on the host (Homebrew). |
| libFuzzer / Valgrind | Fuzz and instruction-count work | Defer. `[SO]` Apple clang ships no libFuzzer; Valgrind does not run on macOS arm64. Add on a Linux job only when a measured need exists. |

---

## Guest toolchain: cc65

All facts `[SO]` unless labelled, rechecked 2026-11-09.

**Revisions.**
- Latest tagged release: **V2.19, published 2020-11-20** (`gh api repos/cc65/cc65/releases`). The master branch is **3,959 commits ahead** of V2.19 (`compare/V2.19...eebf54a`). Master HEAD observed: `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2`, committer date 2026-11-08, license Zlib (GitHub API; `LICENSE` text is the standard zlib terms).
- Packaged copies are all V2.19: Homebrew `cc65 2.19` (bottled, sha256 of source `157b8051aed7f534e5093471e734e7a95e509c577324099c3c81324ed9d0de77`), Ubuntu jammy/noble `2.19-1`, resolute (26.04) `2.19-2build1` (packages.ubuntu.com).
- cc65 is **not installed** on the dev host (`which cl65 ca65` empty, 2026-11-09).

**Why not V2.19 / a distro package** `[REC]`, based on `[SO]`: `libsrc/lynx`, `include/lynx.h` and `cfg/lynx*.cfg` changed materially after V2.19 (e.g. 2022-03-21 "Clear hw palette and set AUDIN low at startup" 79c243c; 2022-02 TGI palette/colour fixes; 2024-01 `__BLOCKSIZE__` -> `__BANK0BLOCKSIZE__` config symbols; 2024-11 Suzy enums/math structs in `lynx.h`; 2022-12/2024-02 serial driver rework). Startup code and header layout are exactly the bytes a boot path and a test harness observe. Also, upstream `lynx-tests` CI builds **cc65 `master` unpinned** (`.github/workflows/build.yml`: `git clone https://github.com/cc65/cc65.git; make`), so V2.19 is not the toolchain its authors exercise. `[INF]` Whether `lynx-tests` even compiles on V2.19 is unverified; do not rely on it.

**Pin and install reproducibly (macOS arm64, Linux x64/arm64)** `[REC]`:
1. Add a `pins.txt` row: `cc65 eebf54af545f cc65/archive/eebf54af545f4ae52f8489a2f6d4d5f93e7942d2.tar.gz d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1` (SHA-256 computed by me on 2026-11-09 from `https://github.com/cc65/cc65/archive/eebf54a...tar.gz`, 3,412,205 bytes, HTTP 200). `[INF]` GitHub archive tarballs are not contractually byte-stable; if the hash ever drifts, re-derive from `git clone` + `git rev-parse HEAD == <40-hex>`, which is the real identity. Record both.
2. Build from the verified source tree with only `make` and the system C compiler. Sibling precedent `[SO]` (muhtari `CONFORMANCE.md` CF.06): `make bin` plus `make lib TARGETS=<target>` took ~4 s with `-j8` for the 7800 target on a pinned commit. The Lynx recipe (`TARGETS=lynx`) is **not executed or timed by me**; confirm in the first fixture task. `lynx-tests` README says `CC65_HOME` may be "the cc65 installation or source-tree root" with `$CC65_HOME/bin` on `PATH`, so an in-tree build with no `make install` is enough.
3. Never `apt install cc65` / `brew install cc65` in the check path. Export `CC65_HOME=<build-tree>/cc65` and prepend its `bin/` only inside the check script.
4. Run `cl65 --version`/`ca65 --version` and print the pinned commit in the check log so evidence names its toolchain.

**What `cl65 -t lynx` produces** `[SO]` (`cc65.github.io/doc/lynx.html`; `doc/lynx.sgml`; `cfg/lynx.cfg`; `libsrc/lynx/*` at `eebf54a`):
- Docs: `cl65 -t lynx -o game.lnx main.c` "will create a bootable cart image"; the cc65 loader "does not check the cart or show a title screen"; `lynx-bll.cfg` instead yields a **BLL download** binary. The doc page **does not state a CPU**. The `65SC02` claim in the bootstrap note comes from cc65's **source** (`src/common/target.c`: `{"lynx", CPU_65SC02, ...}`; `cpu.c` labels 65SC02 "the original CMOS instruction set", i.e. no Rockwell RMB/SMB/BBx). `lynx-tests` assembly uses `.setcpu "65C02"` (Rockwell-extended) in 19 files. So "Epyx says 65C02 / cc65 says 65SC02 / tests assume 65C02" is real and unresolved: **the baseline CPU profile must still be chosen from evidence**.
- Memory layout (docs): programs run with I/O and kernel enabled, usable `$0200`-`$C037`; collision buffer `$A058-$C037`, screen buffers `$C038-$E017`, `$E018-$FFF7`; vectors `$FFF8-$FFFF`; C stack grows down from `$C037` (`$A057` with collision). Block/ROM sizes: 512 B/128K (old games), **1024 B/256K (common homebrew, the default `__BANK0BLOCKSIZE__=$0400`)**, 2048 B/512K.
- `.lnx` image = **64-byte header** (`cfg/lynx.cfg` `HEADER size=$0040`; `libsrc/lynx/exehdr.s`): magic `LYNX`; bank-0 page size u16; bank-1 page size u16; version `1`; 32-byte cart name; 16-byte manufacturer (`"cc65 V"` + version digits); rotation byte (0/1 left/2 right); 5 spare bytes. Inspected the real `cpu.lnx` from the 1.4.9 release: bytes `4C59 4E58 0004 0000 0100 "Cart name…" "cc65 V2.13"…`. `[INF]` "V2.13" is cc65's hex-digit printing quirk for minor version 19 (0x13), so the string cannot distinguish V2.19 from master; **do not use the header string as toolchain evidence**.
- Boot structure (`libsrc/lynx/bootldr.s`, comments in the source): the first bank-0 bytes are a **51-byte encrypted, signed first-stage loader** (begins `ff b6 bb 82 d5 9f 48 cf…`; I confirmed these exact bytes at offset `$40` of the shipped `cpu.lnx`). The real 512-byte boot ROM reads and **decrypts** it, then runs it. Its plaintext (documented in the source) clears MAPCTL and the palette, sets ComLynx open-collector, sets IODIR/IODAT (AUDIN low), reads a 151-byte plaintext second-stage into `$FB68`, which reads the 8-byte directory entry and loads the main program (`$0200` for `lynx.cfg`) and jumps. `crt0.s` then initialises a table of Mikey/Suzy registers itself.
- `[INF]` Consequence: **a cc65-built `.lnx` is not runnable by a loader that only copies bank-0 bytes into RAM**. Something must perform the ROM's job: real ROM (rights-bearing), an HLE that decrypts/loads (below), a project-authored ROM, or a different image format (BLL direct-load).
- EEPROM helpers (`lynx_eeprom_*`, `eeprom46/66/86.s`) exist in the runtime; cartridge filesystem is "not yet supported" per docs (separate `lynx-cart-demo`).

**Other cc65-adjacent tools** (`lynxenc`, `make_lnx`, `sprpck`): not required by `lynx-tests` (its Makefiles call only `cc65/ca65/cl65` and `lynx.lib`; `sprpck` appears only in an unused `.bmp` rule and no `.bmp` is tracked). `[REC]` Do not add them until an original diagnostic needs sprite assets or a signed encrypted loader. `42Bastian/new_bll` (Apache-2.0, 2026-05-15) hosts `make_lnx`/`lynxenc` (needs OpenSSL) if ever required.

**Licensing of guest output** `[SO]`/`[INF]`: cc65 is Zlib; its runtime (`crt0`, `lynx.lib`, TGI static driver) is linked into every fixture, so each built ROM is "own source + Zlib runtime". Zlib requires no notice in binaries but forbids removing the notice from source redistributions; muhtari still records `MIT AND Zlib` per ROM in its manifest (`ASSET_POLICY.md`). Follow that.

---

## Fixture sources

### drhelius/lynx-tests (primary public hardware-informed suite)

All `[SO]`, clone at `…/scratchpad/research-clones/lynx-tests`, 2026-11-09.

| Item | Observation |
|------|-------------|
| HEAD | `a920f01164d37edff94939993fbc0551e5b7000b`, 2026-09-20, "Update lynx-video-timing.md". 57 commits, first 2025-10-13. |
| Latest release | `1.4.9` -> commit `b583dc43d532d3c41563a6fa35420614fbcead89`, 2026-08-13 ("New timers2 tests"). `git diff 1.4.9 HEAD`: `.github/FUNDING.yml` (+3) and `lynx-video-timing.md` only. |
| License | `LICENSE` is standard **MIT**: "Copyright (c) 2025 Ignacio Sanchez Gines", full grant/notice/warranty text present. Only one license file; **source files carry no per-file header or SPDX line** (checked `cpu/main.c`, `sprites2/main.c`). `[INF]` Under muhtari's own policy ("checked in both the LICENSE and the file's header") that is weaker than ideal; treat as MIT-by-repo-LICENSE and record that. |
| Suites | 19 directories with a Makefile: audio, audio2, cpu, math, memio, page-mode, refresh-rate, sdoneack, sprites1-5, timers, timers2, uart, uart2, uart3, uart4. `build-all.sh` lists **18** (omits `refresh-rate`); upstream CI builds every directory with a Makefile. `refresh-rate` is interactive (no pass/fail) and `uart4` needs two consoles and a ComLynx cable: **exclude both from any headless gate** (consistent with its README). |
| Build recipe | `cd <suite> && make` with `CC65_HOME` set; `shared/lynxcc65.mak` calls `cc65 -t lynx -O -Or -Cl -Os -g`, `ca65 -t lynx`, and links `cl65 -Ln <suite>.sym -t lynx -o <suite>.lnx … lynx.lib`. Static TGI driver (`tgi_static_stddrv`). Assembly includes `.setcpu "65C02"`. Needs only cc65 + POSIX make; **no** `lynxenc`, `make_lnx` or `sprpck` in the build path (the devcontainer installs them, the Makefiles do not use them). |
| Prebuilt binaries | **Not in the repository** (`.gitignore` excludes `*.lnx`). Release `1.4.9` has one asset `lynx-tests-1.4.9.zip` (87,872 B, SHA-256 `1d9d9de3e500643f763d6b1c9e1847fdce93d840990230b945983359d2b6f15c`, matches API digest; 19 `.lnx`). Built by upstream CI from **unpinned cc65 master**, so it is not tied to a known cc65 commit. `[REC]` Use the zip only as a *comparison target* ("do our pinned-toolchain bytes equal theirs?", result unknown), never as the committed or CI fixture. |
| BIOS needed? | The README says nothing about BIOS/boot ROM. `[INF]` From the `.lnx` structure above (encrypted first stage), **the suites need *some* boot path**; they do not themselves need a BIOS *image* beyond what the boot path provides. Whether the tests pass under an HLE/open boot path depends on the state the real ROM leaves behind and is **untested**. |
| How results are reported | Two ways, per README and `cpu/main.c`: (1) on screen via cc65 TGI text: abbreviated name + `PASS`/index or `LYNX I`/`LYNX II`, then raw diagnostic hex bytes; (2) **in guest RAM**: an exported BSS array `_g_results` (`cpu`: 18 bytes), mirrored in the `-Ln` label file (`<suite>.sym`, VICE-style) that the Makefile already emits. Per-byte expectations are C tables in `main.c` (`k_expected_results`; some bytes accept two values for Lynx I vs II). The program ends in `for(;;);` after `tgi_updatedisplay()` with **no completion flag** (inspected for `cpu` only; other suites share `shared/util.h` conventions but must be checked one by one). |
| Fixture-rights notes | `sprites2` test 8 embeds a 3-byte "Alpine Games protection" source row and SCB values (`sprites2/tests.s`). `[INF]` Plainly facts about hardware behavior, but name them in the fixture manifest for owner/rights awareness. |

`[REC]` Fixture handling: fetch the commit tarball by `pins.txt` row (`b583dc43d532…`, SHA-256 of `…/archive/b583dc43….tar.gz` computed 2026-11-09: `a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e`), build in the build tree with the pinned cc65, **do not commit the built `.lnx` or the lynx-tests source**, and keep a manifest row per suite (source, pin, recipe, SHA-256 of output, license `MIT AND Zlib`, allowed use). Read results by symbol address from `<suite>.sym` + an independently authored expectations file; do not scrape TGI pixels and do not parse the C tables as the oracle (that makes the fixture check itself). Detect completion by bounded guest time plus a "PC in a self-loop" settle check, and record that this is a harness heuristic. Source for `HEAD a920f01` would give SHA-256 `2a8c78d3625338f1b691d2261387dfeffd50027e2de8a99bd5c4fc35c8180fe7`; prefer the tagged commit.

### Other candidate sources (rights-checked at API/README level, 2026-11-09)

| Source | License | Use | Verdict `[REC]` |
|--------|---------|-----|-----------------|
| **Project-authored diagnostics** (own ca65/C sources built with pinned cc65) | project license (owner choice) + Zlib runtime | Independent expected results; boot/loader and API-contract probes | **Primary long-term fixture class.** Mirrors muhtari ("our own cc65 ROMs"). |
| SingleStepTests/65x02 | MIT | CPU vectors | Use after CPU profile chosen (see above). |
| `atarilynx/cc65-lynx-gametemplate` | CC0-1.0 (2024-09-07) | Scaffolding for a small playable/homebrew scenario | Safe reference for cc65 Lynx project layout. |
| `atarilynx/devcontainers(-features)` | MIT | Shows how others install cc65 | Reference only. |
| `42Bastian/new_bll` (BLL, lyxass, make_lnx, lynxenc, sprpck) | Apache-2.0 | BLL loader/tool source | Reference; fetch only if a BLL direct-load fixture needs it. |
| `atarilynx/handy-examples`, `handy-bootloaders`, `handy-encryption`, `handy-docs`, `emulator` | **no license file** (GitHub shows none) | n/a | **Do not use.** No license = all rights reserved; `handy-bootloaders` is the Epyx commercial loader source. |
| Klaus Dormann 6502/65C02 functional tests | **GPL-3.0** (GitHub API) | n/a | **Do not commit or vendor**; copyleft and not Lynx-specific. |
| Commercial ROMs, BIOS, homebrew with no stated license | n/a | n/a | Out of scope per owner policy. No downloads performed. |

No rights-clean, third-party, purpose-built Lynx homebrew *game* was found at a verified license in this pass (web search surfaced only a "hello world" with no verified license and cc65's `lynx-cart-demo` whose contrib licensing I did not verify). `[REC]` Plan the "playable/homebrew scenario" milestone item around an **original** cc65 demo (the CC0 template is the starting layout), not an external game.

---

## Boot / firmware consequences (feeds the roadmap's open "boot policy" decision)

| Fact | Label |
|------|-------|
| Real handheld has a 512-byte boot ROM; the cc65 `.lnx` first stage is encrypted and decrypted by it. | `[SO]` (`bootldr.s`, shipped `cpu.lnx` bytes) |
| Gearlynx @ `94068da` (2026-11-04): `RunToVBlank` returns a "no BIOS" screen when no BIOS is loaded; `LoadBiosData` rejects any buffer whose size is not 512 B but **accepts any 512-byte content** (CRC only sets a validity flag). README: "requires a BIOS… any BIOS… original md5 `fcd403db…` recommended." | `[SO]` |
| Handy (`libretro/libretro-handy` @ `bc55d46`, 2026-04-20): when the correct BIOS CRC is absent it logs "Using internal fallback" and uses an **HLE boot** (hooks at `$FE00/$FE19/$FE4A/$FF80`, clears RAM, decrypts the loader blocks with the Lynx public-key modulus, loads to `$200`, jumps). | `[SO]` |
| Beetle Lynx/Mednafen: "lynxboot.img … required" (libretro docs, Mednafen docs); no HLE found in `lynx/system.cpp`. | `[SO]` (absence is a grep result) |
| MAME `lynx.cpp`: `lynx.bin`/`lynxa.bin` both `BAD_DUMP`; `lynx2.bin` `NO_DUMP`; ComLynx "missing/imperfect" in its own TODO. | `[SO]` |
| Holani (Rust, GPL-3.0, `LLeny/holani`): embeds a "Free Lynx Boot Rom … Courtesy of bjoern.spruck.net/lynx" (512 B) and a BLL uLoader blob. Source comment gives no license; the cited page returned HTTP 404 on 2026-11-09. | `[SO]` for the embed; provenance/license **unknown (LOW)** |
| `[INF]` The claim "Gearlynx passes all lynx-tests" is therefore a **with-real-BIOS** claim; it says nothing about an HLE/open-ROM profile. | `[INF]` |
| `[INF]` Three technically plausible rights-clean paths exist, none validated: (A) a **BLL/direct-load lane** that builds the same fixture objects with `lynx-bll.cfg` and places the program in RAM with no boot ROM and no decryption (still dependent on handoff state; `crt0` self-initialises Mikey/Suzy); (B) a **project-authored 512-byte boot ROM** (original source, ca65, would need the decrypt math; Gearlynx's size-only check suggests such an image is mechanically acceptable to at least one core; the 7800 precedent is muhtari using CC0 OpenBIOS source built in CI); (C) **HLE of the ROM's documented behavior** (Handy-style). (B) and (C) must use the Lynx public modulus; whether publishing derived data is acceptable is a rights question for the owner. | `[INF]` / `[REC]` |
| `[REC]` Owner-supplied real BIOS: local-only, never in the repo or shared CI; usable for cross-checks against Gearlynx/MAME and for a boot-fidelity comparison. | `[REC]` |

---

## Installation

```bash
# Dev host (macOS arm64): already present per 2026-11-09 survey
#   Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2, git, shellcheck
# Missing and optional: clang-format (pin one version), clang-tidy

# One command, from the repo root (to be created in the first phase):
./scripts/check.sh
#   -> verifies compiler/cmake identities
#   -> scripts/fetch-pin.sh cc65 build/pins/cc65         (SHA-256 verified)
#   -> make -C build/pins/cc65 bin lib TARGETS=lynx -j    (recipe to be confirmed; not run in this research)
#   -> scripts/fetch-pin.sh lynx-tests build/pins/lynx-tests
#   -> cmake --workflow --preset check
```

No `npm`/`pip`/`cargo` packages. Python is not needed by the core or check; keep it out of the required path (host has 3.14.4 if a helper is ever wanted).

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| **C17** | **Rust** (1.99.1, 2026-10-16) | If the owner later values compile-time memory safety over portfolio uniformity. `[INF]` Costs: second toolchain and `cargo` supply chain in a "small, flat" tree; a C-ABI shim (`cdylib`/`staticlib` + generated header) must be kept in step with LibFuturo's source-level C contract; sibling cores, `fetch-pin`, CMake presets and the big-endian/wasm lanes are all C-shaped. A Rust Lynx core exists (Holani, GPL-3.0) as a behavior reference only. Strongest alternative; revisit only if C-specific UB defects prove costly despite sanitizers. |
| **C17** | **C++** (17/20) | If a large, genuinely object-heavy tool (debugger UI) is added; keep it out of the core. Gearlynx and Handy are C++. Exceptions/RTTI/STL iteration order/ABI make determinism and the C boundary harder, not easier. |
| **C17** | **Zig** (0.17.1; ziglang.org index lists 0.17.1, 0.17.0, 0.16.0; master 0.18.0-dev) | Not before 1.0. `[SO]` pre-1.0 with a release-to-release breaking cadence; build-repeatability and long-lived maintenance risk for a milestone-based project. |
| CMake + Ninja + CTest | Meson, Bazel, plain Makefiles | Only if all sibling projects migrate together. Mixed build systems break the shared `check.sh` shape and the LibFuturo source-consumption pattern. |
| cc65 pinned commit, built from source | cc65 V2.19 (brew/apt) | Quick local experiments only. Never in the evidence path. |
| cc65 (`ca65`/`ld65`) | `lyxass` (42Bastian), VASM/dasm | If a future original diagnostic needs BLL-native tooling; adds a tool and a license to track. |
| Plain CTest executables + in-repo check header | Unity, cmocka, Criterion, GoogleTest | Never needed; each is a dependency whose lifetime cost exceeds the benefit (muhtari runs 600+ tests this way `[SO]` quality.json floors). |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **`cc65` V2.19 from brew/apt** in CI or for fixtures | 2020 release, 3,959 commits behind; Lynx startup/headers changed since; upstream test CI builds master. | Pinned commit built from source via `pins.txt`. |
| **Unpinned `git clone cc65` / `latest` / `*-latest` runner labels** | Changes evidence silently; exactly what upstream `lynx-tests` CI does. | Commit SHA + SHA-256 pins; explicit runner labels. |
| **Gearlynx/MAME/Handy/Beetle code, copied or linked** | GPL-3.0 / GPL-2.0+ / GPL-2.0 (+zlib for Handy) code would constrain the project license; owner policy forbids copying. Handy's zlib terms *permit* reuse, but policy still says behavior reference only. | Read-only behavior cross-checks; document each difference as a finding. |
| **Committing the `lynx-tests` release `.lnx` zip or built `.lnx` outputs** | Third-party bytes with no per-file license header; cc65 version unknown; not source-built under our pins. | Build from pinned source in the build tree; manifest row per output. |
| **The "free boot ROM" embedded in Holani, or Handy's HLE tables/modulus code** | Provenance and license unverified (LOW); Holani is GPL-3.0; copying contradicts policy. | Project-authored boot path, or owner-supplied BIOS kept local. |
| **Real BIOS, commercial ROMs, Epyx loader sources (`handy-bootloaders`)** | Rights-bearing; owner policy. No download was performed. | Original diagnostics + source-built fixtures. |
| **`-ffast-math`, floating point in machine state, `time()`/`rand()`/`getenv`/stdio/threads/global mutable state inside the core** | Each breaks fresh-state determinism or the host/machine separation. | Integer-only core, instance struct, caller-provided buffers and logical input/time. |
| **UB-dependent code (signed overflow, uninitialised reads, strict-aliasing punning), `-march=native` in evidence builds** | Different optimisers/CPUs then yield different outputs. | UBSan in check lane; `-std=c17`, no `-march=native` in evidence builds. |
| **OCR or pixel-scraping TGI text as the pass/fail signal** | Brittle; couples test to cc65's font. | Read `_g_results` by symbol address; capture raw video separately as an independent observation. |
| **Klaus Dormann tests, any no-license repo** | GPL-3.0 / all rights reserved. | SingleStepTests (MIT) and own diagnostics. |
| **Python/Node in the required check path** | Extra runtime surface; none needed. | POSIX sh + CMake scripts (as siblings do). |

---

## Stack Patterns by Variant

**If the CPU/model baseline is not yet chosen (it is not):**
- Keep the CPU core behind a profile table (instruction set, interrupt-entry quirks, Lynx I vs II RMB/SMB/BBx behavior) and cut vectors by profile.
- Because `lynx-tests` `cpu` Test 6/7 accepts both Lynx I and II values and reports which one it saw, the suite informs rather than assumes the model.

**If a no-firmware CI lane is required (it is, by owner policy):**
- Start with (A) the BLL/direct-load fixture lane, labelled as *machine-behavior* evidence only (not boot fidelity); evaluate (B)/(C) as a separate, owner-approved milestone item.

**If a libretro/LibFuturo host is attached:**
- Keep the core free of host I/O; put the frontend in `libretro/` and the headless driver in `runner/` (muhtari layout `[SO]`). Public header `extern "C"`-safe, opaque instance handle, caller-owned buffers, no global state.
- LibFuturo is "experimental… no stable binary or plugin ABI is promised" `[SO]` (README); consume it as pinned source, not as a promised ABI.

**If determinism must be demonstrated (it must):**
- Run the same `check` preset on two compiler/arch pairs (Apple clang arm64 on `macos-15`, gcc or clang x86_64 on `ubuntu-24.04`) and compare machine-state and output hashes; a big-endian qemu lane exists in muhtari as a precedent but is a later, measured addition. Same-core hashes show repeatability only, not hardware accuracy.

**Proposed CI split (distinct failure classes, measure before caching):** (1) hygiene (format, shellcheck, inventory/manifest/license-policy checks); (2) build+test on 2 runners incl. fixture rebuild with the pinned cc65; (3) later: sanitizer/fuzz/soak/benchmark lanes.

---

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| CMake `>=3.25` | workflow presets, `cmake --workflow` | Ubuntu 24.04 apt CMake meets the floor `[INF]` (not rechecked); muhtari notes an `ubuntu:26.04` container's apt CMake was too old for its newer preset version, so keep presets at the lowest schema version needed. |
| cc65 `eebf54a` | `lynx-tests` `1.4.9`/`a920f01` | **Unverified**; first fixture task must build all 17 headless suites with this pair and diff output bytes against the release zip (expect size-equal at best; equality is not guaranteed because upstream built unpinned master). |
| cc65 V2.19 | `lynx-tests` | Unknown; not recommended. |
| `lynx.cfg` (cc65) | HLE/boot path | `__BANK0BLOCKSIZE__` (default `$0400`), directory at `$00CB`, main at `$0200`; boot path must follow whichever cfg the pinned commit ships. |
| macOS 26 dev host / macos-15 CI | Apple clang 21.x vs older Xcode clang on `macos-15` | `[INF]` Compiler versions differ; keep warnings-as-errors identical and avoid newest-only diagnostics. |

---

## Decision ownership

| Choice | Level | Note |
|--------|-------|------|
| C17, CMake/Ninja/CTest, `check.sh` + preset workflow, sanitizers, runner labels, formatting/lint tools, `pins.txt` mechanism | **Routine, reversible default** | Proceed autonomously; revisit with measured evidence. |
| cc65 pinned commit (bump policy), fixture pins (`lynx-tests` tag, SingleStepTests commit), manifest format | **Routine, reversible** | Pins change via dedicated PRs with recomputed hashes. |
| Language *change* away from the sibling-portfolio standard (e.g. Rust) | **Owner-level** (materially changes cost, toolchain and LibFuturo fit) | Not recommended now. |
| **Project license** (siblings are MIT; libfuturo Apache-2.0) | **Owner** | Not chosen. Using GPL reference code would pre-empt it, which is another reason not to copy. |
| Repository visibility / publication / release route | **Owner** | Explicit existing boundary. |
| Any use of a real BIOS (even local-only), and its handling in CI | **Owner** (rights, authority) | Local-only cross-check use should be requested as an explicit, narrow approval. |
| Adopting, vendoring or publishing the "free boot ROM", the Lynx public modulus, or Handy-derived HLE data; shipping a project-authored boot ROM | **Owner** (rights) | Needs provenance/license review first; my web check could not establish either for the "free" ROM. |
| Baseline model/CPU profile (Lynx I vs II; 65C02 vs 65SC02 semantics) and what claims are made | **Owner-approved evidence plan** | Research can recommend; claims name the exact profile. |
| Physical hardware qualification (refresh-rate, `uart4` two-console, Lynx I/II comparison) | **Owner** | Excluded from headless CI. |

---

## Open verification items for the first phases (not done in this research)

1. Build the pinned cc65 from source on macOS arm64 and ubuntu-24.04 with the exact `make` recipe; record wall time. (I did not run any third-party build script, by rule.)
2. Build all headless `lynx-tests` suites at `b583dc43…`; compare to the 1.4.9 release zip (size, bytes, `.sym` symbol addresses). Record whether `_g_results` is present in every suite's `.sym`.
3. For each suite, confirm completion behavior (self-loop address, or none) and which result bytes are model-dependent.
4. Decide and prototype the no-firmware lane: BLL/direct-load build of the same objects (`lynx-bll.cfg`), then check whether a suite's `_g_results` match expectations without the ROM-left state.
5. Confirm exact CMake preset schema version needed for `workflowPresets` on the 3.25 floor and on Ubuntu 24.04's apt CMake.
6. Run the owner-approved local-only BIOS cross-check, if the owner permits it, to see whether Gearlynx output matches the project core on each suite (evidence tier: independent emulator comparison, not hardware).

---

## Sources

- `~/projects/lynxplosion/.planning/PROJECT.md`, `AGENTS.md`, `.planning/bootstrap/EMULATOR-DNA.md` (owner policy and dated research input; rechecked claims noted above). HIGH.
- cc65 Lynx docs `https://cc65.github.io/doc/lynx.html` and `doc/lynx.sgml`, `cfg/lynx.cfg`, `cfg/lynx-bll.cfg`, `libsrc/lynx/{bootldr,exehdr,crt0}.s`, `src/common/target.c`, `src/common/cpu.c` at commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2` (2026-11-08); `gh api repos/cc65/cc65` releases/tags/compare/commits/license. HIGH.
- Homebrew formula `cc65` 2.19 (`Formula/c/cc65.rb`); `packages.ubuntu.com` cc65 search (jammy/noble `2.19-1`, resolute `2.19-2build1`). HIGH.
- `https://github.com/drhelius/lynx-tests` at `a920f01…` / tag `1.4.9` (`b583dc4…`): `LICENSE`, `README.md`, `build-all.sh`, `shared/lynxcc65.mak`, `.github/workflows/build.yml`, `.devcontainer/devcontainer.json`, `cpu/{Makefile,main.c,tests.s}`, `sprites2/tests.s`, `shared/util.h`; GitHub release `1.4.9` asset digest; release zip downloaded and listed (not executed). HIGH.
- Gearlynx `https://github.com/drhelius/Gearlynx` @ `94068da354f2…` (GPL-3.0; `src/gearlynx_core.cpp`, `src/media.cpp`, `README.md`, release 1.2.38 2026-11-04). HIGH for source observations; README accuracy claim is `[MR]`.
- `libretro/libretro-handy` @ `bc55d46…` (`lynx/system.cpp`, `rom.cpp`, `lynxdec.cpp`, `license.txt`); libretro docs for Handy and Beetle Lynx (BIOS "Required", zlib / zlib+GPLv2); `libretro/beetle-lynx-libretro` (GPL-2.0); Mednafen Lynx docs; MAME `src/mame/atari/lynx.cpp` (`license:GPL-2.0+`) and `LICENSE.md` (GPL-2.0+, majority BSD-3); `LLeny/holani` (GPL-3.0, README, `src/rom.rs`); `MiSTer-devel/AtariLynx_MiSTer` (GPL-2.0). HIGH for licenses and code facts; LOW for the "free boot ROM" provenance.
- `atarilynx` org repos and licenses; `42Bastian/new_bll` (Apache-2.0); `SingleStepTests/65x02` (MIT, README); `Klaus2m5/6502_65C02_functional_tests` (GPL-3.0). HIGH.
- Rust 1.99.1 (`rust-lang/rust` release, 2026-10-16); Zig index `ziglang.org/download/index.json`; CMake v4.4.5; Ninja v1.13.2; LLVM 23.1.5; `actions/runner-images` README runner labels. HIGH.
- Owner-portfolio precedents read locally: `~/projects/libfuturo` (README, `docs/development.md`, `scripts/check.sh`, `CMakeLists.txt`), `~/projects/muhtari` (`pins.txt`, `scripts/fetch-pin.sh`, `.planning/milestones/v1.0-research/STACK.md`, `.planning/preparation/CONFORMANCE.md`, `ASSET_POLICY.md`, `.github/workflows/ci.yml`, `quality.json`), nesturbator/gabbaboy/genewhip `CMakeLists.txt` and licenses. HIGH as observation of the owner's conventions; they are precedent, not requirements for Lynxplosion.

---
*Stack research for: deterministic, headless-testable Atari Lynx emulator core*
*Researched: 2026-11-09*
