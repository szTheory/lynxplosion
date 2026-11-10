# Phase 2: CPU Profiles and Per-Opcode Vectors - Research

**Researched:** 2026-11-09
**Domain:** cycle-stepped 65C02-class CPU core in C17, data-defined Lynx I/II profiles, SingleStepTests vector verification, deterministic bus-cycle timing
**Confidence:** HIGH (design and data layouts verified against the pinned vectors and the repository); MEDIUM-LOW for Lynx hardware behaviour, which is unverified by design

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

### Profiles: ids, names, semantics (CPU-02)
- Public profile ids are append-only and are model profiles, not CPU-only: `0 none` (default),
  `1 lynx1`, `2 lynx2`. Ids live in snapshot headers and names in manifests and claims; neither
  is ever renumbered or renamed. `lynx_profile_name(id)` is a `switch` over string literals (a
  pointer table lands in `.data.rel.ro` and fails the nm writable-data gate on Linux).
- Profile is fixed at init per instance: no setter, reset keeps it, an unknown id is refused with
  `LYNX_ERR_ARGUMENT` and no write. The instance stores the id, never a table pointer.
- `none` means "what both Lynx tables agree on". An instruction runs under `none` only when its
  `lynx1` and `lynx2` table entries are identical. Otherwise advance stops with the new
  `LYNX_STOP_UNDEFINED = 4` **before the opcode, consuming 0 ticks and changing nothing**: the
  opcode is peeked at PC through the side-effect-free read path at the instruction boundary
  (idempotent: a repeated advance stops at the same place). A run that completes under `none` is,
  by the tables, identical under both Lynx profiles. `LYNX_STOP_HALT` is reserved again.
- `lynx1` follows the 65SC02 instruction set (no RMB/SMB/BBR/BBS; `$x7/$xF` are NOPs), `lynx2`
  follows the Rockwell R65C02 set (adds RMB/SMB/BBR/BBS). A test asserts the two tables differ in
  exactly the 32 `$x7/$xF` opcodes. On both models `$CB` and `$DB` are 1-byte NOPs that do not
  poll IRQ (hardware-informed: lynx-tests `cpu` Test 8). Neither is a W65C02S: no WAI/STP exist.
- Source honesty: Epyx calls the CPU "a 65C02 cell" whose instruction set and cycle counts are in
  a non-public VTI spec, so every per-opcode cycle count and behaviour is a stand-in from another
  vendor's datasheet, a hardware-informed test, or a software-model vector. Every expected value
  carries a tag (lynx-tests hardware / WDC / Rockwell / CMD datasheet / SingleStepTests model /
  inferred) and an `unverified` flag. The check prints counts per tag. Unverified for Lynx:
  every cycle count except `$03` (one fetch) and BBR/BBS 5/6/7 on lynx2; lynx1 `$x7/$xF` length
  and cycles (lynx-tests Test 7 gives the same result for 1-, 2- or 3-byte NOPs); `$CB/$DB`
  cycles; `$5C`; bus address of every dummy cycle; decimal-mode V flag, invalid BCD and extra
  cycle; RMW abs,X 6 vs 7 cycles; reset stack cycles and register values; SEI/PLP/RTI and
  taken-branch IRQ latency; any lynx1/lynx2 difference beyond the 32 bit-op opcodes.
- Profile data shape: one X-macro `.def` file expanded into a `const` integer-only `[2][256]`
  array of `{kind, mode, flags}`; bit numbers come from the opcode bits; no pointer or string
  tables in the core. Behaviour common to both profiles is plain CMOS code, not a runtime switch
  (D cleared on interrupt, JMP-indirect page-wrap fixed, RMW read-read-write, dummy read of the
  last instruction byte, BRK-then-IRQ, N/Z valid in decimal mode plus its extra cycle); these live
  in the test-side manifest as sourced constants.

### CPU structure and the bus-cycle interface (CPU-03)
- **[resolved] Cycle-stepped CPU that posts accesses as data.** The CPU never touches memory. It
  holds one pending request (address, data, kind, flags POLL/LAST/WRITE); the machine loop grants
  the bus, prices the cycle, performs it, stamps it, then calls `lynx__cpu_complete(cpu, data,
  irq_nmi_lines)`. No function pointers (passes the nm gate); the vector tests and the machine run
  the same compiled CPU; stalls, sleep and arbitration (RDY-like) are "the loop does not serve the
  request yet", so TIM-04 and Suzy arbitration need no CPU change. Rejected: CPU calls bus
  functions (cannot honour a budget mid-instruction; unspecified C evaluation order), a bus
  vtable, compile-time bus selection (tests would check code that does not ship).
- A guard: `core/cpu*.c` may include only `cpu.h`, enforced by a new include gate.
- Tick stamp = the absolute guest master tick (u64) at which the cycle is granted, plus duration
  and a sample offset (0 in Phase 2). The machine's `guest_ticks` is the only clock; the CPU has no
  time field and no cycle-count table.
- Cycle kinds: opcode, operand, dummy-stream, dummy, read, write, RMW-read, stack, pointer,
  vector. Cycle counts come from each addressing mode's sequence of bus accesses; expected counts
  live in tests with a source per value. Page mode, wait states, DMA and refresh go on the bus
  side later.
- Cost model: a pure function on the bus side over const integer tables indexed by (cost model,
  region, cycle kind). Phase 2 default model is `flat5`: 5 ticks everywhere, labelled
  PLACEHOLDER (Epyx: normal RAM/ROM and Mikey registers 5, page-mode opcode reads 4, Suzy reads
  9-15, audio DPRAM 5-20; all-5 makes straight-line code about 20% slow). No claim ever cites
  ticks as timing evidence. An internal `test_skew` model with unequal per-kind costs exists for
  success criterion 3 and is identity-bound in the snapshot.
- **[resolved] Budget semantics keep the Phase 1 invariant (consumed never exceeds the budget).**
  Half-open interval: advance to target T performs every bus action stamped before T and consumes
  exactly the budget unless it stops for UNDEFINED, FRAME, WATCH. A cycle granted before T is
  performed and the clock advances to T even if the cycle's duration extends past T;
  `cpu_ready_at` records when the next cycle may be granted. BUDGET stops may therefore fall
  mid-instruction. Splitting a run into small budgets gives identical state and stamps chain
  exactly (tested). (The API lens proposed instruction-boundary stops with overshoot; rejected: it
  breaks the verified Phase 1 property and cannot survive Suzy stalls in Phase 7.) A public
  single-step is not promised; the internal seam steps instructions for vectors.
- IRQ/NMI sampling: the IRQ line is sampled at the stamp of the cycle the CPU flags POLL, with
  devices already caught up to that tick. The CLI/SEI/PLP one-instruction delay and immediate RTI
  follow from where each cycle writes the I flag. The 1-byte 1-cycle `$x3/$xB` NOPs and `$CB/$DB`
  carry no POLL (fits lynx-tests Test 8). The NMOS taken-branch delay is not modelled. SEI/PLP/RTI
  latency is marked inferred. The poll position per opcode is profile data.

### Public API, registers, snapshot (CPU-02, CPU-04)
- Registers: keep PC, A, X, Y, S, P; add `LYNX_REG_CPU_STATE`, a bitfield of IRQ and latency
  latches, so every snapshot field is readable through the public API and the observation digest
  covers all state. P reads with bits 4 and 5 set (PHP semantics); vectors compare with mask
  `0xCF`. `lynx_read_memory` is documented as the raw RAM view until Phase 3 adds a CPU-mapped
  view. No public write functions for registers or memory (test seam below).
- Test seam stays internal and white-box (tests already include `core/internal.h`): a
  `lynx__cpu_step`/cycle seam with a caller-owned bus-cycle log; vectors set and restore only the
  RAM addresses they touch. One public-path test (snapshot restore, then a one-instruction budget)
  checks equality with the internal step.
- **Snapshot format 2.** Payload adds `cpu_state` and reserved bytes, the cost-model id,
  `cpu_ready_at`, and the CPU's position inside the instruction (about 12 bytes, validated on
  restore); `halted` is dropped and the skeleton-only "PC inside content" check is removed.
  Format 1 is refused with `LYNX_ERR_VERSION`. A compile-time assert ties payload length to the
  format number. A coverage test sets every field non-default, restores into a fresh instance and
  memcmps the whole buffer. New refusals: every cross-profile pair, format 3, bad P bits, undefined
  `cpu_state` bits. Snapshots stay same-build only.
- Runner: `--profile none|lynx1|lynx2` (default `none`; unknown exits 2). The manifest profile
  line comes from the core; an UNDEFINED stop writes the bundle, exits 0, and prints a stderr note
  naming the PC and opcode and suggesting `--profile`. `ALLOWED_PROFILES` for claims stays `none`.

### Vector sourcing, inventory and controls (CPU-01, CPU-05)
- Sources: `lynx1` uses SingleStepTests `synertek65c02/v1`; `lynx2` uses `rockwell65c02/v1`. Do
  not use wdc65c02, 6502 or nes6502. Pin commit `2f6980a2d95757486c7bee24355c360e40e2a224`
  (2025-05-10). Evidence tier T1 (software-model vectors, not hardware), startup_tier `none`
  (vectors inject CPU state; not tier A). Recheck the pin and licence before use.
- Asserted per vector: final PC, S, A, X, Y, the listed RAM values, and no write outside the
  listed addresses; P with mask `0xCF`. Cycle counts, bus traces, unlisted reads and ticks are
  NOT asserted from these vectors (their traces contain generator-specific dummy reads). A
  `--trace` diagnostic mode may exist.
- **[resolved] Exclusion rule.** Where hardware-informed Lynx evidence exists, the profile follows
  it and the SingleStepTests vectors for that opcode are excluded with reason
  `source-disagrees-with-hardware-informed-test` ($CB/$DB on both profiles, which the vectors give
  as 2- and 4-cycle ops of 1 and 2 bytes, against Test 8). Everywhere else the profile follows the
  source variant and every expectation lacking a Lynx source is labelled `unverified`. Compare
  tiers per opcode: everything-asserted, final-state-only (e.g. `$5C`, decimal-mode ADC/SBC, RMW
  abs,X), excluded. Exclusions come only from the profile table, reasons from a closed list, no
  "known failure" status; excluded rows are still parsed and counted.
- Fast gate uses a committed sample: the first 32 vectors of every opcode file for each variant,
  byte-for-byte as upstream, in one text container with `#file` headers, plus the upstream MIT
  LICENSE and a list of 512 per-file SHA-256 values: 16,384 vectors, about 6.4 MB of text (about
  1.2 MB in git). No `pins.txt` row (the check downloads every row and the archive is about 1 GB
  compressed, 5 GB unpacked). Record this third-party MIT test data in a fixture manifest,
  `docs/provenance.md`, the exposure record and THIRD-PARTY notices. The rights, hex/base64 and
  PII scans must still pass with no allow-list weakening (measured: 5.7 MB JSON passed in 0.42 s).
- Full lane: `scripts/vectors-full.sh`, outside `check.sh` and outside CTest. It downloads the 512
  pinned files over https into a temp directory outside `build/`, verifies each hash, rebuilds the
  sample and compares it byte-for-byte, then runs all 10,000 vectors per file. Run at Phase 2
  verification, on every pin bump and before any claim; a CI trigger only after measuring its
  time. The same lane diffs the two variants on the 178 shared opcodes (spike: record whether
  synertek and rockwell differ anywhere besides the 32 bit-op opcodes).
- Parser: a small purpose-built strict C reader in the test runner (strict about keys and number
  ranges, no allocation, a sample mode and a complete-file mode; checks that the byte at PC equals
  the file's opcode). Exit codes 0 pass, 1 assertion failure, 2 usage, 3 data or parse error, 4
  inventory mismatch. No JSON library, no awk converter, no Python in the check path. The data
  comes in two layouts (SBC and Rockwell xF files differ); the reader must handle both and the
  inventory counts catch a silent zero-vector parse.
- Inventory: one committed file per profile at `tests/inventory/vectors/<id>.txt`: header (source,
  commit, sample rule, sample hash, asserted, not asserted, not covered, tier T1), one row per
  opcode (status, count, decimal-vector count, reason), a totals line; compared byte-for-byte with
  the runner's output. Pass counts are checked, never committed.
- Own scenario vectors (what SingleStepTests cannot cover), each tagged by source and an
  unverified flag, comparing address/value/read-write/kind per bus cycle: CLI/SEI/PLP/RTI
  latency, the `$x3/$xB` block with IRQ pending, D in an IRQ, BRK, IRQ during BRK, NMI, reset
  sequence, bits 0-7, and the lynx-tests Test 8 replica.
- Negative controls (EVD-05 style): wrong profile in both directions (the set of failing opcodes
  must be exactly the 32 `$x7/$xF` opcodes); a no-step CPU (failures equal asserted vectors minus
  those whose final state equals the initial state); one corrupted expectation gives exactly one
  failure; malformed data exits 3; inventory drift exits 4; a changed sample byte fails its hash
  check; a mutated profile table (one opcode wired wrong) fails exactly that opcode; turning
  `$CB` into a 2-cycle polling op fails the Test 8 replica; an exclusion without a reason fails the
  inventory check; lints with controls for the pin list and the startup-tier vocabulary.
- Manifests/validation: fixture manifests and claims accept `startup_tier = none`, run manifests
  stay `A` only (small `validate-kv.sh` change with a control). No profile-naming claim in Phase
  2; evidence is recorded in the verification report. A later claim sets `content_sha256` to the
  hash of the vector inventory file and uses: software model, flat test bus, no timing,
  interrupt or memory-map claim.

### Re-baselining Phase 1 on purpose
- Remove the skeleton step and `LYNX__HALT_OPCODE` ($DB is a NOP on both models) and the budget
  helper. Tests built on a `0xDB` halt image move to a `none` trap image such as `07` (a bit-op
  opcode, so `none` stops there with UNDEFINED).
- Retire the 32-byte tracer as the golden and `api.skeleton` workload (as 65C02 code it hits `$1F`,
  BBR1, which differs by model, and a `JMP` through seeded RAM). Keep it for identity and loader
  tests, plus a short profile-divergence test whose values the test computes independently (none
  stops; lynx1 and lynx2 differ in PC).
- Two new fixtures: a never-ending loop program using only opcodes both profiles share and reading
  seeded RAM (the golden, snapshot and instance workload), and a short terminating program with a
  committed, hand-derived and cited trace table (registers, RAM, ticks). The pinned `ca65` (already
  built by the check) re-assembles the committed `.s` and must match the committed hex. No
  committed scripts in the check path.
- Re-baseline order: vectors green, then the hand-derived test green, then regenerate the golden
  with the reason in the commit. Rename `api.skeleton` to `api.cpu_program`; add
  `det.profile_agreement` (the loop program gives the same observation digest under none, lynx1,
  lynx2); `det.sensitivity` gains a cost-model flip. The new golden's evidence: identical digest on
  Apple clang and Linux gcc plus hand-derived register and tick values. Update the claim and the
  inventory hash once. Any later timing refinement re-baselines the golden the same way.

### CPU coding rules
- One bus access per statement (C evaluation order is unspecified; gcc and clang may differ).
  Unsigned arithmetic only; no `(int8_t)` branch casts; explicit `(uint8_t)` on assignments (gcc
  `-Wconversion`); tick math in `uint64_t`; `switch` dispatch; 256-entry tables static-asserted;
  no plain `char`; no pointer tables (Linux `.data.rel.ro` is reported by nm as `d`); no function
  pointers in the core.

### Claude's Discretion

- File and function names inside `core/` (e.g. `cpu.c`, `cpu.h`, `profile.def`, `bus.c`), the exact
  `cpu_state` bit assignments and snapshot byte layout, test names, the exact column layout of the
  inventory header, the sample-container format details, and the declared power-on value of S and
  reset register values (mark unverified).

### Deferred Ideas (OUT OF SCOPE)


- Real per-access costs (page mode, Suzy 9-15 ticks, audio DPRAM, cart read and window timing):
  Phase 3-4 data refinements through the cost-model tables, then re-baseline the golden.
- Sample point inside a cycle, Lynx dummy-cycle side effects (RMW on Suzy, RCART0 double read),
  NMI latency, polling when a stall falls inside an instruction: Phases 4-7.
- CPU-mapped memory view and MAPCTL (Phase 3); real IRQ sources (Phase 4); a CI trigger for the
  full vector lane (after measuring); any profile-naming claim (needs O1).
- Owner follow-ups: committing about 6.4 MB of MIT-licensed third-party test data with its notice
  is auto-followed under the rights policy (licence documented, manifested); the fallback is a
  range-request fetch of the first bytes of each file at check time.

</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CPU-01 | The 65C02-class CPU passes pinned per-opcode functional vectors for each defined profile, with an exact per-profile inventory | Sections 5 (runner, sample container, strict grammar, inventory), 6 (hand-checked cycle catalogue, decimal algorithm verified against all 1,152 ADC/SBC sample vectors), 9 (plans B and F) |
| CPU-02 | Lynx I and Lynx II CPU profiles exist as data (including RMB/SMB/BBR/BBS), selectable per instance, with neither adopted as baseline | Section 4.2 (X-macro `.def`, table shape, `none` = intersection), 4.6 (public API), 9 (plans A, D) |
| CPU-03 | All CPU memory accesses go through a tick-stamped bus-cycle interface so access costs can vary per access | Section 4 (cycle-stepped CPU, request-as-data, S2 budget loop, cost-model tables, TEST_SKEW), 8 (criterion-3 test) |
| CPU-04 | IRQ entry, CLI/SEI latency, WAI/STP and reserved-opcode NOP lengths follow the selected profile's documented behavior | Sections 4.4 (interrupt, BRK, reset, poll rules), 4.3 (NOP shapes), 5.7 (authored scenario vectors with source tags), 10 (validation) |
| CPU-05 | A named-fault control (wrong profile selected) fails the bit-operation vectors | Section 5.6 (controls; measured that the reverse control needs an address-hashed poison value), 10 |
</phase_requirements>

## Summary

Phase 2 replaces the Phase 1 skeleton step with a cycle-stepped 65C02-class CPU that posts one bus
request per cycle as data, runs under three data-defined profiles (`none` = agreement of the two
Lynx tables, `lynx1`, `lynx2`), and is proven by 16,384 committed SingleStepTests vectors plus
authored scenario vectors. This research verified the discuss decisions against the real data and
code, and produced the concrete shapes the planner needs. Everything below is consistent with the
locked CONTEXT decisions except the items in "Flagged Risks" (each with a recommendation).

Key results measured in this session (all from the pinned commit data, fetched by range request or
whole-file and deleted afterwards; no emulator source opened):

- The sample container and strict reader grammar are confirmed on real data: all 8,192 vectors per
  variant parse with one fixed key order (`name, initial, final, cycles`; `pc, s, a, x, y, p, ram`),
  at most 9 ram entries and 7 cycles per sampled vector; 9 synertek and 25 rockwell files use the
  "regenerated" physical layout (first line `[{`, 32 lines) and the rest the standard layout (first
  line `[`, 33 lines). The container is 3,201,335 bytes (synertek) and 3,229,999 bytes (rockwell).
- A cycle-template extraction over all 16,384 sampled vectors gives a data-backed bus-sequence
  catalogue (section 6.3). Synertek and rockwell templates differ in exactly the 32 `$x7/$xF`
  opcodes, so the discuss spike question is answered for the sample: no other shared-opcode
  divergence in cycle shape.
- Decimal-mode ADC and SBC (the largest software-model risk) were modelled and checked against all
  1,152 sampled ADC/SBC vectors (576 per variant) with zero mismatches (N, Z, C, V and A; P under mask `0xCF`).
  The exact algorithms are in section 6.5. The naive "Bruce Clark style" SBC formula fails 6% of
  decimal SBC vectors; the formula in 6.5 does not.
- BIT immediate (only Z), TSB/TRB, BRK, PHP, PLP, RTI, JSR, RTS, BBR/BBS and RMB/SMB final-state
  semantics were checked against the sample with zero mismatches. All `$x3/$xB`, `$x2`, `$44`,
  `$54/$D4/$F4`, `$5C/$DC/$FC` vectors are "pc += length, nothing else changes" (except `$DB`,
  excluded).
- The lens-03 proposal of a constant poison value for unlisted reads makes the reverse wrong-profile
  control (synertek data run under lynx2) pass vacuously for 8 of the 16 `$xF` opcodes, because the
  synertek BBx-slot vectors do not list the zero-page byte the real BBR/BBS reads. An
  address-hashed poison value fixes this (minimum 12 distinguishing vectors per opcode, measured).
- Check baseline: 47 s local on this host (the `hygiene` tests and the history scan dominate), 27 s
  in the latest CI run. The 6.4 MB sample passes the real rights and PII scans with no allow-list
  change. Estimated added gate cost: +4 to 6 s wall local, +3 to 5 s CI.
- The pinned `ca65`/`ld65` assemble raw binaries with a 2-line linker config; both proposed fixture
  programs were assembled and match the hand assembly byte for byte, and their hand-derived tick
  counts are in section 7.

**Primary recommendation:** build in this order, each plan green on its own: (A) CPU core and
profile tables with a white-box test bus and the new include gate; (B) the strict vector reader,
runner, sample, inventories, controls and full lane; (C) authored scenario vectors; (D0) the two
fixtures with their ca65 assembly gate; (D1) the machine switch (API, snapshot format 2, S2 budget
loop, cost model, runner flag) with the whole Phase 1 test re-baseline and the golden; (D2) the
public-path tests; (E) documentation, measurement and verification report. Section 9 gives the file
list for each.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Opcode decode, micro-sequence, flags, poll/latch logic | Core CPU (`core/cpu.c`, `core/cpu_profile.c`) | none | Pure function of CPU state and the data word of the last cycle; includes only `cpu.h`, so it cannot touch memory or time |
| Profile tables and the `none` intersection | Core CPU data (`core/cpu_profile.def`) | Machine (`lynx_profile_name`) | One X-macro source; integer-only `const` arrays |
| Grant, price, perform and stamp each bus cycle | Machine loop / bus side (`core/machine.c`, `core/bus.c`) | none | The machine owns the only clock (`guest_ticks`) and the only memory accessor; stalls and arbitration land here later |
| Cost model tables | Bus side (`core/bus.c`) | Tests (TEST_SKEW identity) | Data indexed by (model, region, cycle kind); the CPU holds no cycle table |
| Budget, stop reasons, UNDEFINED check | Public API (`lynx_advance`) | Machine loop | S2 half-open budget; UNDEFINED peeks the opcode through the side-effect-free path |
| Snapshot format 2, value validation | Core (`core/snapshot.c`) | Tests | Pending request is re-derived, not stored |
| Vector parse, assert, inventory, controls | Test executable (`tests/cpu/`) | CMake scripts, `scripts/vectors-full.sh` | Host side, stdio allowed, never in `core/` |
| Sample generation and full lane | Script (`scripts/vectors-full.sh`) | none | Network only here, outside `check.sh` and CTest |
| Fixture assembly cross-check | `scripts/check.sh` step plus a CMake compare | none | Network-free; uses the already pinned cc65 |
| Profile selection and stop note | Runner (`runner/`) | Core (`lynx_profile_name`) | Host policy; the core holds no message text |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C17, no extensions | existing | CPU, bus, tables | Project standard `CMAKE_C_STANDARD 17`, `CMAKE_C_EXTENSIONS OFF` [VERIFIED: CMakeLists.txt:4-6] |
| CMake + Ninja, CTest | 3.25 minimum; local 4.4.3, CI 3.31.6 | Build, tests, exact inventory | Existing gate machinery [VERIFIED: `cmake --version`, ninja 1.13.2 this session; CI version per LESSONS.md] |
| ca65 / ld65 from the pinned cc65 | cc65 commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2` | Re-assemble the two fixture programs | Already pinned and built by the check [VERIFIED: pins.txt row; `build/pins/cc65/bin/ca65` present] |
| `core/sha256.c` | existing | Sample identity hash inside the test runner | Already in the core; measured 0.23 s per 3.2 MB at -O0 with ASan+UBSan |
| SingleStepTests/65x02 data | commit `2f6980a2d95757486c7bee24355c360e40e2a224` (2025-05-10), MIT | Vector expectations | Pin re-checked this session: HEAD of `main` is still this commit, LICENSE is MIT "Copyright (c) 2024 Thomas Harte et al" (1,075 bytes, sha256 `5fb64d1f2e4d74fabc85c768e1f6ffa73b95dc93e217c55acfe7e4a89ba32b04`) [VERIFIED: GitHub API and raw fetch this session] |

### Supporting
None. No new dependency is needed: the strict JSON reader is purpose-built (about 250 lines), the
decimal and CMOS behaviours are code, and the scratch Python used for this research is not part of
the repository or the check path.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Purpose-built strict reader | jsmn or another tokenizer | Needs a notice and still needs a schema layer; rejected by the discuss lens and by "no new dependency" |
| Committed sample (6.4 MB) | Range-prefix fetch of 512 files at check time | Network in the gate; kept only as the documented fallback if the owner objects to third-party data in the repo |

**Installation:** none.

**Version verification:** the only external identities used are the cc65 pin (already verified by
`scripts/fetch-pin.sh` at SHA-256 `d3e4bfa5...5c8a1`) and the SingleStepTests commit above.

## Package Legitimacy Audit

No external package is installed by this phase (no npm, PyPI or crates dependency). The two
external sources are commit-pinned and hash-verified: cc65 (existing pin row) and SingleStepTests
(per-file SHA-256 list `fixtures/vectors/sst65x02/upstream.sha256`, section 5.1). The package
legitimacy seam was therefore not run. **Packages removed due to [SLOP]:** none. **Packages flagged
[SUS]:** none.

## 3. Measurements (source observations)

Tag key: [VERIFIED: x] read or measured this session; [CITED: lens N] stated by a discuss lens and
not re-measured; [INFERRED] derived from measurements; [ASSUMED] not verified.

### 3.1 Check and CI baseline

| Item | Value |
|------|-------|
| `sh scripts/check.sh`, local (Apple M5 Pro, cmake 4.4.3) | 47 s total: workflow 24 s, history 15 s, artifacts 7 s, shellcheck 1 s, others 0 s [VERIFIED: run this session] |
| Slowest tests (parallel, `jobs: 4`) | `hygiene.pii.self_test` 19.2 s, `hygiene.firmware_scan.self_test` 16.8 s, `hygiene.tree` 6.1 s, `hygiene.firmware_scan` 3.7 s, `hygiene.manifests.self_test` 3.1 s, `api.build_id.refresh` 2.7 s, `runner.cli` 2.4 s, `api.snapshot_refusal` 1.9 s; whole CTest 22.9 s for 57 tests [VERIFIED] |
| CI, latest `main` run (ubuntu-24.04) | job 31 s; check.sh total 27 s: pins 1 s, cc65 build 9 s, shellcheck 1 s, workflow 9 s (CTest 6.6 s), artifacts 2 s, history 4 s [VERIFIED: `gh run view`] |
| CI job durations, last 8 `main` runs | 11 to 43 s; the last five green runs 25 to 43 s (33, 35, 25, 34, 43) [VERIFIED: `gh run list`] |
| Scan cost with the 6.4 MB sample (scratch clone, real scripts) | `scan-binaries.sh --source` exit 0 (2.0 s vs 2.5 s baseline, noise), `hygiene.sh --tree` exit 0 (4.6 s vs 6.0 s), `hygiene.sh --history` exit 0 (10.9 s vs 11.2 s). Delta is inside run-to-run noise (about 1.5 s); no false positive from the hex-512, base64-200, home-path, email or credential rules [VERIFIED] |
| Pack growth | scratch clone after `git gc`: 1.85 MiB total pack including the sample (was 153 KiB loose before gc) [VERIFIED] |
| SHA-256 of a 3.2 MB sample under -O0 + ASan + UBSan | 0.225 s (synertek), 0.242 s (rockwell); a byte scan of the same file 0.035 s [VERIFIED: scratch benchmark of `core/sha256.c`] |

Budget (the user's targets: well under about 60 s local, about 45 s CI): the local gate is already
47 s on this host, so Phase 2 has about 13 s of headroom locally and 18 s on CI. Estimated added
cost [INFERRED from the measurements above]:

| Addition | Local wall (jobs 4) | CI wall |
|----------|--------------------|---------|
| 12 to 14 vector-runner invocations, each hashing a 3.2 MB sample (about 0.25 s) and parsing and executing 8,192 vectors (about 0.3 to 0.6 s) | +2 to 3 s | +3 to 4 s (gcc x86 runner about 2 to 3 times slower than the M5) |
| New unit tests (tables, seam, cycles, scenarios, cost model, snapshot coverage, S2 properties) | +1 s | +1 s |
| Scans over the sample, new fixtures and docs | +0.5 to 1.5 s | +0.3 s |
| ca65 re-assembly step (two programs) | +0.2 s | +0.2 s |
| **Total** | **+4 to 6 s** (about 51-53 s) | **+4 to 6 s** (about 31-33 s) |

Keep each control test cheap: build mini-samples (a few `#file` sections) for controls that do not
need the real sample; only the two positive runs and the sample-identity control need the full 3.2
MB. If the budget is exceeded after landing, the CI efficiency review should look first at the
hygiene self-tests (they are the critical path and are not Phase 2 work).

### 3.2 Vector data facts (commit `2f6980a2d95757486c7bee24355c360e40e2a224`)

[VERIFIED: range fetch of the first 40,000 bytes of all 512 files, two whole-file downloads, one LICENSE and README fetch, all into a scratch directory outside the repository, deleted afterwards]

- raw.githubusercontent.com serves the files at the commit SHA and honours HTTP Range (HTTP 206);
  512 requests of 40 KB with 16 parallel streams took 7 s with no rate-limit response. A whole
  3.1 MB file downloaded in 0.42 s and a 3.5 MB file in 0.52 s (single stream, about 7 MB/s).
- Two physical layouts, both one object per line (verified on all 512 files):
  - standard: line 1 is `[`; object lines start `{ "name": "<op> <b1> <b2>", "initial": { "pc": ...`
    (a space after each `{`), end `},`; the last object line ends `}` and a final line is `]` with no
    trailing newline; 10,001 lines.
  - regenerated: line 1 is `[{ "name": "<op> <n>", "initial": {"pc": ...` (no space after `{`),
    the last line ends `}]` with no trailing newline; 9,999 newlines (10,000 objects).
  - regenerated files: synertek `e1 e5 e9 ed f1 f2 f5 f9 fd` (9) and rockwell `0f 1f 2f 3f 4f 5f
    6f 7f 8f 9f af bf cf df ef ff e1 e5 e9 ed f1 f2 f5 f9 fd` (25). The sample uses `L = 33` lines
    for a standard file and `L = 32` for a regenerated one: that is the first 32 objects in both.
- All 16,384 sampled objects parse with one key order: `name`, `initial`, `final`, `cycles`;
  state keys `pc, s, a, x, y, p, ram`. Maximum ram entries 9, maximum cycles 7 in the sample (the
  full lane needs a larger bound; use 16). `initial.ram` always contains the opcode byte at `pc`
  and the name's first token equals the file opcode (checked on all 16,384). No duplicate
  `initial.ram` addresses. `final.ram` addresses differ from `initial.ram` addresses in 766
  (synertek) and 767 (rockwell) sampled vectors, so `nowrite` must use the union of both lists.
- P convention: bit 5 is set in every sampled `initial.p`; bit 4 is clear in 7,904 synertek and
  7,392 rockwell vectors and set in 288 and 800 (the regenerated files). The `0xCF` mask makes this
  irrelevant to the gate; PHP/BRK pushed bytes are still compared exactly through RAM.
- Whole-file hash recheck: `synertek65c02/v1/e9.json` sha256
  `3a33b31a76638a83748484cbd3ba8c5a70f04bdbb778b0ff87e2a375455b1b23` (3,118,032 bytes, 10,000
  objects); its first 32 lines equal the sample prefix byte for byte.
- Cross-variant: for the same opcode the two variants use different random initial states (0 of
  8,192 equal), so shared-opcode agreement cannot be diffed vector by vector. The question is
  answered through the wrong-profile controls and the cycle-template comparison (section 6.3):
  templates differ only for the 32 `$x7/$xF` opcodes.

## 4. CPU core design

### 4.1 Files, types, functions

New core files (all added to `LYNX_CORE_SOURCES` in CMakeLists.txt, which builds both `lynx_core`
and the uninstrumented twin `lynx_core_plain`): `core/cpu.h`, `core/cpu.c`, `core/cpu_profile.def`,
`core/cpu_profile.c`, `core/bus.h`, `core/bus.c`. Only `cpu.h` is visible to `cpu*.c`.

```c
/* core/cpu.h: includes <stdint.h> and <stdbool.h> only */
enum lynx__ck {                 /* cycle kind, chosen by the CPU */
    LYNX__CK_OPCODE = 0,        /* opcode fetch at PC */
    LYNX__CK_OPERAND,           /* instruction-stream byte after the opcode */
    LYNX__CK_DUMMY_STREAM,      /* internal cycle re-reading the stream (PC or a last instruction byte) */
    LYNX__CK_DUMMY,             /* internal cycle at another address */
    LYNX__CK_READ, LYNX__CK_WRITE,
    LYNX__CK_RMW_READ,          /* second read of a read-modify-write */
    LYNX__CK_STACK_READ, LYNX__CK_STACK_WRITE,
    LYNX__CK_POINTER,           /* indirect pointer byte, also JMP (abs) pointer */
    LYNX__CK_VECTOR,            /* interrupt, BRK and reset vector byte */
    LYNX__CK_COUNT              /* 11 */
};
enum { LYNX__CF_WRITE = 1u, LYNX__CF_POLL = 2u, LYNX__CF_LAST = 4u };
enum { LYNX__LINE_IRQ = 1u, LYNX__LINE_NMI = 2u };
/* latch bits of lynx__cpu.flags (also the low byte of LYNX_REG_CPU_STATE) */
enum { LYNX__CS_IRQ_TAKE = 1u, LYNX__CS_NMI_TAKE = 2u, LYNX__CS_NMI_PENDING = 4u,
       LYNX__CS_NMI_PREV = 8u, LYNX__CS_SEQ_MASK = 0x30u /* 0 none, 1 IRQ, 2 NMI, 3 RESET */ };

typedef struct lynx__cpu_decode { uint8_t kind, mode, flags, vec; } lynx__cpu_decode; /* 4 bytes */

typedef struct lynx__cpu {      /* plain data, no pointers, no padding surprises (all fields <= 16 bits) */
    uint16_t pc, ea;
    uint8_t a, x, y, s, p;      /* p always keeps bits 4 and 5 set (PHP form); IRQ/NMI entry pushes it with bit 4 cleared */
    uint8_t profile;            /* 0 none, 1 lynx1, 2 lynx2 */
    uint8_t opcode, step, tmp;  /* position inside the instruction; step 0 = at a boundary; at a boundary opcode, ea and tmp are 0 */
    uint8_t flags;              /* LYNX__CS_* */
    uint16_t req_addr;          /* pending request: derived state, re-posted after a snapshot restore */
    uint8_t req_data, req_kind, req_flags;
} lynx__cpu;

extern const lynx__cpu_decode lynx__cpu_decode_table[2][256];       /* [0] = lynx1 row set, [1] = lynx2 */
const lynx__cpu_decode *lynx__cpu_table(uint32_t profile);          /* none -> the lynx1 rows */
bool lynx__cpu_opcode_defined(uint32_t profile, uint8_t opcode);    /* none: both rows equal; else true */
void lynx__cpu_init(lynx__cpu *c, uint32_t profile);                /* declared power-on values, boundary at pc 0 */
void lynx__cpu_start_at(lynx__cpu *c, const lynx__cpu_decode *t, uint16_t pc);    /* boundary at pc, posts the fetch */
void lynx__cpu_start_reset(lynx__cpu *c, const lynx__cpu_decode *t);              /* internal seam: 7-cycle reset sequence */
void lynx__cpu_complete(lynx__cpu *c, const lynx__cpu_decode *t, uint8_t data, uint8_t lines);
bool lynx__cpu_at_boundary(const lynx__cpu *c);
bool lynx__cpu_state_valid(const lynx__cpu *c, const lynx__cpu_decode *t);        /* snapshot restore */
void lynx__cpu_repost(lynx__cpu *c, const lynx__cpu_decode *t);                   /* recompute req_* from state; pure */
```

- `lynx__cpu_complete` is the only state transition: it applies `data` (ignored for writes) to the
  step that posted the current request, samples `lines` at this cycle if the request carried
  `LYNX__CF_POLL`, advances `step`, and posts the next request through the same `repost` function
  that a snapshot restore uses. One bus access per call, so C evaluation order cannot matter. The
  CPU does not read memory, time or the machine.
- Pass the table as a `const lynx__cpu_decode *` argument (never stored in the instance): production
  passes `lynx__cpu_table(c->profile)`, the mutant-profile control passes a test-local copy with
  one row changed. This keeps the instance pointer-free and needs no `#ifdef` or link-time swap.
  [RECOMMENDED; the CONTEXT only requires that the mutation be possible without core changes.]
- Dispatch is `switch (mode)` then `switch (step)` for the sequence, and `switch (kind)` for the
  operation; no function pointers, no pointer tables. At fetch completion `opcode = data`, the row
  is read once, and the row's `mode` selects the sequence for the following steps.
- `lynx__cpu_state_valid` accepts a state iff: `profile <= 2`; `p & 0x30 == 0x30`; `flags` has no
  bit outside `0x3F`; `step == 0` implies `opcode == 0 && ea == 0 && tmp == 0`; `step` is below the
  longest step count of the row's mode (a per-mode `const` table of 8-bit lengths); the row of
  `opcode` is defined for the profile when `step > 0` and the sequence is not an interrupt entry.
  Canonical zeros at a boundary keep whole-buffer equality between machines that reach a boundary
  by different paths.

### 4.2 Profile table (CPU-02)

`core/cpu_profile.def` is the single source. Two macros (this removes 224 duplicated rows):

```c
/* OP(opcode, KIND, MODE, FLAGS, VEC)                      row shared by both profiles */
/* OPX(opcode, K1, M1, F1, V1, K2, M2, F2, V2)             row that differs; exactly 32 of them */
OP(0x69, K_ADC, M_IMM, F_DEC_EXTRA, 0)
OPX(0x07, K_NOP, M_NOP_ZP,  F_UNV_SHAPE, V_TIER_STATE,   K_RMB, M_ZP_RMW, 0, 0)
OPX(0x0F, K_NOP, M_NOP_ABS3, F_UNV_SHAPE, V_TIER_STATE,  K_BBR, M_ZPREL, 0, 0)
OP(0xCB, K_NOP, M_NOP1, F_NO_POLL | F_UNV_CYCLES, V_EXCL_HW)
```

`cpu_profile.c` expands it twice (once per macro form) into the `[2][256]` array and a second pass
into `static_assert`s that every opcode `0x00..0xFF` appears exactly once (a duplicate or missing
row is a compile error; do this with a 256-entry `const uint8_t` count table built by the macro, or
by designated initialisers, which reject duplicates with `-Woverride-init` as an error).

- `kind` is the operation, `mode` selects the bus sequence (section 6.2). Bit numbers for
  RMB/SMB/BBR/BBS come from opcode bits 4-6 and bit 7 selects set/branch-if-set; no table column.
- `flags`: `F_NO_POLL` (the instruction carries no POLL cycle), `F_DEC_EXTRA` (ADC/SBC: one extra
  cycle when D is set), `F_BITOP_SLOT` (member of the 32 profile-dependent opcodes), `F_UNV_CYCLES`,
  `F_UNV_SHAPE` (length or cycle sequence not sourced for Lynx). `_UNV_` bits never change
  execution; the tests enumerate unverified expectations from them.
- `vec` (vector policy, read only by the test runner and the inventory): `V_TIER_STATE`
  (final-state-only label), `V_EXCL_HW` (excluded with reason
  `source-disagrees-with-hardware-informed-test`). The core never reads `vec`; it lives in the same
  table because CONTEXT requires that exclusions come only from the profile table.
- **`none`**: `lynx__cpu_opcode_defined(0, op)` is `memcmp(&t[0][op], &t[1][op], 4) == 0`
  computed on demand (no third table). `lynx__cpu_table(0)` returns the lynx1 rows; they are
  equal to the lynx2 rows for every defined opcode by construction.
- A test asserts the set of opcodes whose two rows differ is exactly `{x7, xF : x = 0..F}` (32).
- `lynx_profile_name(uint32_t)`: a `switch` returning the literals `"none"`, `"lynx1"`, `"lynx2"`,
  `NULL` otherwise. A pointer table would land in `.data.rel.ro` and fail the nm gate on Linux
  [CITED: lenses 1, 2; mechanism INFERRED, not reproduced on Linux this session].
- Public ids: `LYNX_PROFILE_NONE = 0`, `LYNX_PROFILE_LYNX1 = 1`, `LYNX_PROFILE_LYNX2 = 2`. Init
  accepts exactly these; anything else is `LYNX_ERR_ARGUMENT` with no write (the current check is
  `cfg->profile != LYNX_PROFILE_NONE` [VERIFIED: core/machine.c, in `lynx_machine_init`]).

### 4.3 Operation semantics and shapes (verified against the sample)

All verified rows below: zero mismatches over the 32 sampled vectors of each opcode in both variants
unless stated [VERIFIED: scratch checks this session].

- Flags and registers: N and Z from results; BIT immediate (`$89`) changes only Z; BIT zp/abs
  /zp,X/abs,X set N and V from the memory byte and Z from `A & m` (standard; not separately
  sampled); TSB/TRB set Z from `A & m` and write `m | A` / `m & ~A`; CMP/CPX/CPY standard;
  INC A/DEC A (`$1A/$3A`), BRA, PHX/PHY/PLX/PLY, STZ, `(zp)` modes are the CMOS additions.
- PHP and BRK push `p | 0x30`; IRQ/NMI push `(p | 0x20) & 0xEF`; PLP and RTI load `p = (v | 0x30)`;
  the vector comparison masks `0xCF` so bits 4 and 5 never matter to the gate.
- BRK: pushes `pc + 2` high then low, then `p | 0x30`, sets I, clears D, loads `$FFFE/$FFFF`; 7
  cycles. JSR pushes `pc + 2` (the address of the last operand byte). RTS pulls and adds 1. RTI
  pulls P, PCL, PCH. JMP `(abs)` reads the high byte at `abs + 1` with a full 16-bit increment (no
  page wrap): 32 of 32 sampled vectors, but none sits at `$xxFF` (see risk R9), so add directed
  authored vectors for `$xxFF`.
- RMB/SMB (Rockwell): `zp ^= bit` via read, dummy read, write; flags unchanged; BBR/BBS: branch on
  bit clear/set, flags unchanged, offset is signed, target `pc + 3 + offset`.
- Reserved NOPs: `$x3` and `$xB` are 1-byte; `$02 $22 $42 $62 $82 $C2 $E2` 2-byte (immediate-like,
  2 cycles); `$44` 2-byte (zp read, 3 cycles); `$54 $D4 $F4` 2-byte (zp,X read, 4 cycles); `$5C`
  3-byte (4 cycles in the vectors); `$DC $FC` 3-byte (4 cycles). All leave registers, flags and RAM
  unchanged and advance PC by their length.
- **Profile-dependent shapes.** lynx2 = the Rockwell set (RMB, SMB, BBR, BBS). lynx1 follows the
  synertek vectors (CONTEXT: "everywhere else the profile follows the source variant"): `$x7` with
  even high nibble is a 2-byte NOP with a zero-page read (3 cycles); `$x7` with odd high nibble a
  2-byte NOP with a zero-page,X read (4 cycles); `$xF` with even high nibble a 3-byte NOP of 3
  cycles with no data read; `$xF` with odd high nibble a 3-byte NOP of 4 cycles (last operand byte
  re-read) [VERIFIED: synertek templates, section 6.3]. All of these carry `F_UNV_SHAPE`: CMD's
  G65SC02 table says 1 byte and 1 cycle [CITED: lens 1], so the lynx1 length is unverified (risk R3).
- **`$CB` and `$DB`** (both profiles): the same shape as `$x3`: 1 byte, 1 cycle, no POLL, flags
  `F_NO_POLL | F_UNV_CYCLES`. Source: lynx-tests `cpu` Test 8 (hardware-informed) [CITED: lens 1];
  the synertek/rockwell vectors (`$CB` 1 byte 2 cycles, `$DB` 2 bytes 4 cycles) are excluded with
  `V_EXCL_HW`.
- No WAI, no STP, no HALT: `LYNX_STOP_HALT` stays reserved.

### 4.4 Interrupts, BRK, reset and poll rules (CPU-04)

These have no SingleStepTests coverage; each expected value gets a source tag in the authored
scenario vectors (section 5.7). Poll and latch rules are the planner's contract:

- `LYNX__CF_POLL` is set on the last cycle of every instruction path except the F_NO_POLL family and
  the interrupt/reset sequences. At a POLL cycle, at its grant stamp, with devices already caught up:
  `if (lines & IRQ) && !(p & I)` then set `IRQ_TAKE`; `if NMI_PENDING` then set `NMI_TAKE`.
  The I flag tested is the value before this cycle's own update, so CLI, SEI and PLP (which write I
  in their last cycle) behave as: CLI and PLP clearing I delay the IRQ by one instruction; SEI with
  an IRQ already pending still takes it after SEI; RTI (which writes I in cycle 4 of 6) is
  immediate. [INFERRED from the Rockwell pin text quoted by lens 1 and the nesdev interrupt prose
  quoted by lens 2; only CLI is hardware-informed (lynx-tests Test 1).]
- NMI is edge triggered: at every `complete`, `if (lines & NMI) && !(flags & NMI_PREV)` sets
  `NMI_PENDING`; `NMI_PREV` follows the line level. NMI has priority over IRQ at the boundary.
- At a boundary (step 0, next fetch to post): if `NMI_TAKE` start the NMI entry; else if `IRQ_TAKE`
  start the IRQ entry; else post the opcode fetch. Starting an entry clears the taken latch (and
  `NMI_PENDING` for NMI).
- Entry sequence (IRQ, NMI), 7 cycles: C1 `DUMMY_STREAM` read at PC (PC not incremented); C2 same;
  C3 `STACK_WRITE` PCH; C4 PCL; C5 P with bit 4 clear and bit 5 set, then set I and clear D; C6
  `VECTOR` low; C7 `VECTOR` high; vectors `$FFFE/$FFFF` (IRQ) and `$FFFA/$FFFB` (NMI). No POLL
  cycle. BRK is the same sequence with C2 an `OPERAND` read, PC advanced, B set in the pushed P.
  "BRK then IRQ" (an IRQ pending during BRK) needs no special case: BRK sets I, so the IRQ waits
  until RTI or CLI; NMI hijacking is not modelled (unverified).
- Reset sequence (internal seam only; the machine's power-on does not execute it): C1, C2
  `DUMMY_STREAM` at PC; C3, C4, C5 `STACK_READ` at S, S-1, S-2 (reads chosen, see unverified list),
  S decreases by 3; C6, C7 `VECTOR` `$FFFC/$FFFD`; I set, D clear; A, X, Y unchanged.
- `F_NO_POLL` on `$x3`, `$xB`, `$CB`, `$DB` reproduces lynx-tests Test 8: after CLI with an IRQ
  pending, a run of 1-cycle NOPs takes no IRQ; the IRQ is taken after the first 2-cycle instruction
  that follows (the Test 8 return address is the byte after the `INX`), and a control block of
  `$EA` NOPs takes it after the first `$EA` [CITED: lens 1].
- Declared power-on values (Claude's discretion, all unverified, record in the profile document):
  A = X = Y = 0, S = `0xFD` (a reset sequence from S = 0 leaves `0xFD`), P read as `0x34` (I set, D
  clear, bits 4 and 5 set), PC = load address (loader) or the reset vector (reset seam), no latch
  set, `step` 0.
- Unverified list to carry into the profile document and the inventory header (superset of the
  CONTEXT list): every cycle count except `$03` and Lynx II BBR/BBS 5/6/7; lynx1 `$x7/$xF` shape;
  `$CB/$DB` cycles; `$5C` length and cycles; the bus address of every dummy cycle (including the
  decimal extra cycle, RMW second read, branch-taken and page-cross dummies, `JMP (abs,X)` fourth
  cycle); decimal V flag, invalid BCD and extra-cycle timing; RMW abs,X 6 versus 7; reset stack
  access kind and register values; SEI/PLP/RTI and taken-branch IRQ latency; NMI hijack; any
  lynx1/lynx2 difference beyond the 32 opcodes.

### 4.5 Machine loop and S2 budget (CPU-03)

```text
machine fields added: lynx__cpu cpu; uint64_t cpu_ready_at; uint8_t cost_model;
                      (removed: pc, a, x, y, s, p, halted)
lynx_advance(m, budget, &consumed, &stop):
  *consumed = 0; *stop = BUDGET; refuse ARGUMENT / STATE as in Phase 1 (needs loaded content)
  room   = (UINT64_MAX - LYNX__TICK_HEADROOM) - guest_ticks   (0 if guest_ticks is already above)
  target = guest_ticks + min(budget, room)                    // HEADROOM = 0x10000, > any cycle cost
  start  = guest_ticks
  while guest_ticks < target:
    if cpu_ready_at > guest_ticks:                  // inside a cycle's duration, or stalled
        guest_ticks = min(target, cpu_ready_at); continue
    if lynx__cpu_at_boundary(cpu) and the next request is an opcode fetch   // no interrupt entry pending
        and !lynx__cpu_opcode_defined(profile, peek(m, cpu.req_addr)):
        *stop = UNDEFINED; break                   // guest_ticks == cpu_ready_at here: the previous cycle has elapsed
    g = guest_ticks                                // Phase 2: lynx__bus_grant returns g (no stalls yet)
    t = lynx__bus_timing_of(m->cost_model, region_of(req_addr), req_kind)   // pure; {ticks, sample=0}
    data = lynx__bus_access(m, &req, g + t.sample) // the only memory access; writes land in raw RAM
    lynx__cpu_complete(&cpu, table, data, 0)       // lines = 0 until Phase 4
    cpu_ready_at = g + t.ticks
  *consumed = guest_ticks - start
```

- Half-open rule: every bus action stamped before `target` is performed; a cycle granted before
  `target` is performed even if its duration extends past it; the next call resumes at
  `cpu_ready_at`. `consumed == budget` unless UNDEFINED (or the headroom clamp) stops the run, and
  `consumed <= budget` always. BUDGET stops may fall mid-instruction; registers then show the state
  after the last performed cycle (PC = next byte to fetch, not the instruction start).
- UNDEFINED stops at an instruction boundary after the previous cycle's duration has elapsed
  (`guest_ticks == cpu_ready_at`), consumes nothing further and changes nothing; a repeated call
  stops at the same place with `consumed == 0` (idempotent). If `guest_ticks == target` at a
  boundary the loop exits with BUDGET and the next call reports UNDEFINED. Only `none` ever stops
  this way. [Rule is the planner's to lock; see open question OQ5.]
- Invariants (tested): `guest_ticks <= cpu_ready_at <= guest_ticks + max cycle ticks`; slicing
  independence (any split of a span into budgets gives byte-identical instance bytes at the same
  guest time; stamps chain `stamp[i+1] = stamp[i] + ticks[i]`); a cycle whose end would pass
  `UINT64_MAX` is never started (`LYNX__TICK_HEADROOM`). The Phase 1 white-box test that sets
  `guest_ticks = UINT64_MAX - 5` gets new expected values (nothing runs; consumed 0; no wrap).
- `peek` is `ram[addr]` in Phase 2 and the side-effect-free mapped read in Phase 3.
- Resetting: `lynx_reset` returns the whole instance to the fresh state, including `cost_model`
  (default `flat5`); tests set `TEST_SKEW` through `lynx__machine_set_cost_model(m, id)` after reset
  or load.

### 4.6 Public API, registers, snapshot format 2

Public header changes (all additive): `LYNX_PROFILE_LYNX1 = 1`, `LYNX_PROFILE_LYNX2 = 2`,
`LYNX_STOP_UNDEFINED = 4` (HALT 3 stays, documented reserved), `lynx_profile_name`, register ids
`LYNX_REG_CPU_STATE = 6`, `LYNX_REG_CPU_LATCH = 7`, `LYNX_REG_COUNT = 8` (see risk R4: the CONTEXT
asks for one id; two are needed for the "every snapshot field is publicly readable" invariant),
`lynx_read_memory` documented as the raw RAM view, the PROVISIONAL banner rewritten (the CPU is real
but unqualified; the raw RAM loader is still provisional until Phase 3).

Register encoding [RECOMMENDED]: `P` reads with bits 4 and 5 set. `CPU_STATE = flags | step << 8 |
opcode << 16 | (cpu_ready_at - guest_ticks) << 24`; `CPU_LATCH = ea | tmp << 16 | cost_model << 24`.
The delta fits 8 bits because every cycle costs at most 255 ticks (cost tables are `uint8_t`).

Snapshot format 2 (payload 65,568 bytes; header layout unchanged, 236 bytes, `format_version = 2`):

```
 0 pc u16 | 2 a | 3 x | 4 y | 5 s | 6 p (bits 4,5 set) | 7 cpu flags | 8 opcode | 9 step
10 ea u16 | 12 tmp | 13 cost_model | 14 reserved u16 (zero) | 16 guest_ticks u64
24 cpu_ready_at u64 | 32 ram[65536]            P_END = 65568
```
`LYNX__SNAPSHOT_FORMAT 2u`, `LYNX__SNAPSHOT_PAYLOAD_LEN 65568u`, and
`_Static_assert(LYNX__SNAPSHOT_FORMAT != 2 || LYNX__SNAPSHOT_PAYLOAD_LEN == 65568u, "...")` ties
the layout to the format number. Format 1 returns `LYNX_ERR_SNAPSHOT_VERSION` (the version check
already runs before the build-id check [VERIFIED: core/snapshot.c restore step 4]). The skeleton
`halted` byte and the "PC inside content" check (restore step 10, [VERIFIED: core/snapshot.c
`payload[P_HALTED] > 1u || pc < m->content_addr ...`]) are removed. New value checks after the
payload hash: `lynx__cpu_state_valid`, reserved bytes zero, `cost_model` equal to the instance's,
`guest_ticks <= cpu_ready_at <= guest_ticks + 255`. Apply stays one no-fail step: copy fields, then
`lynx__cpu_repost`. The pending request is never stored. Snapshots stay same-build only.

## 5. Vector runner (CPU-01, CPU-05)

### 5.1 Pin record and the 512-file hash list

- `fixtures/vectors/sst65x02/upstream.sha256`: a header naming the repository URL and the 40-hex
  commit, then 512 lines `<64-hex>  <variant>/v1/<op>.json`, `LC_ALL=C` sorted. No `pins.txt` row
  (the pins step downloads every row; the full archive is about 1 GB compressed [CITED: lens 3]).
- Reproducible production: `scripts/vectors-full.sh pin` downloads each file with
  `curl -fsSL --proto '=https' --proto-redir '=https' --max-filesize 8000000` from
  `https://raw.githubusercontent.com/SingleStepTests/65x02/<commit>/<variant>/v1/<op>.json` into a
  `mktemp -d` directory outside the repository and outside `build/` (EXIT trap removes it), names
  validated as `[0-9a-f]{2}`, hashes with `sha256sum` or `shasum -a 256` (the `sha_of` idiom in
  check.sh), and writes the sorted list. Measured: one file takes 0.4 to 0.5 s single stream, so
  512 files take about 4 minutes sequentially and about 1 minute with `xargs -P 8` [INFERRED from
  the two measured downloads]. Any download or hash failure aborts the whole run.
- Because the full lane must re-derive the sample byte for byte, the sample generator is committed
  with it. Recommended: one POSIX-sh script `scripts/vectors-full.sh` with three modes: `pin`
  (writes `upstream.sha256`), `sample` (writes both `.sample` containers from verified downloads),
  `run` (verify every file against the list, rebuild the sample and `cmp` it with the committed
  one, then run the runner in complete mode over all 10,000 vectors per file for both profiles,
  print both results files and their SHA-256 values). It must pass shellcheck 0.9.0 and must not
  appear under `tests/` (the no-network scan covers `tests/**`, `cmake/`, `CMakeLists.txt` only
  [VERIFIED: tests/cmake/no_network.cmake file globs]).
- Sample generation is therefore one run of `scripts/vectors-full.sh pin` then `sample`; the
  container logic is `head -n L` per file, with `L = 33` when line 1 is exactly `[` and `L = 32`
  otherwise (a trivial loop that reproduced both containers here). Fail closed if a file's first
  line is neither `[` nor starts with `[{ "name"`.

### 5.2 Sample container (v1) and repository placement

```
#sst65x02-sample v1
#source https://github.com/SingleStepTests/65x02 <40-hex commit> synertek65c02/v1
#rule first 32 objects of each file, upstream bytes verbatim, files in opcode order
#file 00.json lines 33
[
{ "name": "00 ...", ... },
...32 object lines...
#file 01.json lines 33
...
```
- A line starting `#` is framing; JSON lines never start with `#`. `lines L` gives the exact number
  of upstream lines (bytes verbatim, each terminated by `\n`, the last one still ending in its
  upstream `,`). 256 sections per file in opcode order, no gaps.
- Files: `fixtures/vectors/sst65x02/synertek65c02.sample` (3,201,335 bytes in the scratch build),
  `rockwell65c02.sample` (3,229,999), `LICENSE` (upstream MIT text verbatim), `upstream.sha256`.
  Add `.gitattributes` with `fixtures/vectors/sst65x02/* -text` so no client converts line endings
  (a conversion would change the SHA-256).
- Two manifests (check-manifests requires every fixture file to be named by a manifest `files` key
  and every `sha256` to be unique): `fixtures/manifests/sst65x02-synertek65c02.manifest` and
  `...-rockwell65c02.manifest`, keys `source revision recipe sha256 licence notices allowed_use
  startup_tier profile evidence_tier files`, with `startup_tier = none`, `evidence_tier = T1`,
  `profile = lynx1` / `lynx2`, `sha256` = the sample's SHA-256, `files` = the sample, LICENSE and
  `upstream.sha256`. `validate-kv.sh` currently hard-codes `startup_tier` to `A`
  [VERIFIED: scripts/validate-kv.sh `if (key == "startup_tier" && value != "A")`]; add a
  `--startup-tiers "A none"` option (default `A`), pass `A none` from `check-manifests.sh` and
  `check-claims.sh`, keep run manifests on `A`, and add controls: `none` accepted in a fixture
  manifest and refused in a run manifest (`tests/det/manifest_controls.sh`), and a seeded wrong value
  refused in `tests/gates/manifests_controls.sh`. Update `docs/provenance.md` ("Third-party data
  committed with notice"), `docs/policy/exposure-record.md` (one SingleStepTests row: README,
  LICENSE and data read, generator not read) and the claim template comment (a vector claim binds
  `content_sha256` to the hash of the vector inventory file).
- Scan evidence (section 3.1): no allow-list change is needed. The fixture manifests also hold the
  sample SHA-256, which exempts the files from the text rules by design.

### 5.3 Strict reader grammar (lives in `tests/cpu/`, never in `core/`)

Whitespace-insensitive, key-order-fixed, allocation-free, iterative, about 250 lines. Verified by
scanning all 16,384 sampled objects; any deviation exits 3.

```
sample    := "#sst65x02-sample v1" NL  source-line NL  rule-line NL  section{256}
section   := "#file " HEX2 ".json lines " DEC NL  body        (body = exactly DEC lines)
stream    := ws "[" ws object ( ws "," ws object )* ws ","? ws          (prefix mode: trailing "," allowed, no "]")
file      := ws "[" ws object ( ws "," ws object )* ws "]" ws           (complete mode: exactly 10000 objects, "]" required)
object    := "{" ws '"name"' ws ":" ws string ws "," ws '"initial"' ws ":" ws state ws ","
             ws '"final"' ws ":" ws state ws "," ws '"cycles"' ws ":" ws cycles ws "}"
state     := "{" ws '"pc"' : num , '"s"' : num , '"a"' : num , '"x"' : num , '"y"' : num , '"p"' : num ,
             '"ram"' : "[" [ pair ( "," pair )* ] "]" "}"            (pc <= 65535; others <= 255; pair = "[" num "," num "]", addr <= 65535)
cycles    := "[" [ trip ( "," trip )* ] "]"   trip := "[" num "," num "," ("\"read\""|"\"write\"") "]"
num       := "0" | [1-9][0-9]*                 (no sign, fraction, exponent or leading zero; range-checked)
string    := '"' [ -~ except '"' and '\\' ]* '"'
```
Rejected as exit 3 with file, opcode and vector ordinal in the message: unknown, missing or
duplicate keys; out-of-range numbers; more than 64 ram entries or 16 cycles; a direction other than
`read`/`write`; a duplicate `initial.ram` address; `initial.ram[pc] != file opcode`; the name's
first token not equal to the opcode; a section whose object count or line count differs from its
header; trailing bytes; a sample whose SHA-256 differs from the manifest's (checked before any
parsing, with `core/sha256.c`). Excluded rows are still parsed and counted.

### 5.4 What is asserted

Per vector, after loading `initial` into a flat 64 KiB test bus and running one instruction (until
`lynx__cpu_at_boundary`): `pc, s, a, x, y` exactly; `p & 0xCF`; every `final.ram` address holds its
listed value; **no write** to an address outside the union of the `initial.ram` and `final.ram`
lists. Not asserted: cycle count, bus trace, unlisted reads, ticks (kept as a `--trace` diagnostic
that prints the first difference and is never a gate). The test bus answers unlisted reads with
`poison(addr)`.

**Poison must be address-hashed, not constant** [VERIFIED: measured on the sample]: a constant
`0xA5` makes the reverse control (synertek vectors under lynx2) produce zero distinguishing
vectors for BBR 0, 2, 5, 7 and BBS 1, 3, 4, 6 (8 of 16 `$xF` opcodes), because the synertek `$xF`
vectors do not list the zero-page byte that lynx2's BBR/BBS reads. `poison(a) = (uint8_t)(((uint32_t)a
* 167u + 13u) >> 3)` gives at least 12 distinguishing vectors per `$xF` opcode in the sample; the
other direction (rockwell vectors under lynx1) has at least 11 for all 32 opcodes. Restore only the
addresses a vector touched (listed plus written) back to `poison`, not the whole 64 KiB.

Vectors whose final state equals the initial state apart from PC (BBx with offset that lands on the
same PC, NOPs) are counted as `identity`; the no-step control needs that count.

### 5.5 Per-profile inventory

`tests/inventory/vectors/lynx1.txt` and `lynx2.txt`, `LC_ALL=C` sorted rows, tab separated, compared
byte for byte with the plan the runner derives from the sample and the profile table
(`--expect-plan FILE`; a difference exits 4 and names the first differing line). Results are
checked, never committed.

```
# vector inventory: op<TAB>status<TAB>tier<TAB>vectors<TAB>decimal<TAB>reason
# profile = lynx1
# source = SingleStepTests/65x02 2f6980a2d95757486c7bee24355c360e40e2a224 synertek65c02/v1
# sample = first 32 objects of each of 256 files, upstream bytes verbatim
# sample_sha256 = <sha256 of fixtures/vectors/sst65x02/synertek65c02.sample>
# asserted = pc s a x y, p & 0xcf, final listed ram, no write outside listed addresses
# not_asserted = cycle count, bus trace, unlisted reads, ticks
# not_covered = irq/nmi/reset entry, cli/sei latency, jmp (abs) at xxff, lynx memory map, hardware timing
# evidence_tier = T1 (software-model vectors, not hardware)
00	run	full	32	0	-
69	run	state	32	21	cycles-unsourced-decimal
...
cb	excluded	-	32	0	source-disagrees-with-hardware-informed-test
# totals = files 256 run 254 excluded 2 vectors 8192 asserted 8128 excluded_vectors 64
```
- `status` is `run` or `excluded`; `tier` is `full` or `state` for `run` rows (from `V_TIER_STATE`).
  Because the gate asserts the same classes for both tiers, `tier` is a label that decides whether
  the optional `--trace` diagnostic may compare the cycle list and how a future claim words the
  coverage (open question OQ4). `decimal` counts vectors with D set in `initial.p`, only for ADC and
  SBC rows (0 elsewhere).
- Closed reason vocabulary (anything else is exit 4): `-`, `source-disagrees-with-hardware-informed-test`
  (excluded `$CB`, `$DB`), `cycles-unsourced-decimal` (the 18 ADC/SBC opcodes), `cycles-conflict-sources`
  (RMW abs,X shifts `$1E $3E $5E $7E`; `$5C`), `shape-unsourced-for-lynx1` (lynx1 `$x7/$xF`). An
  excluded row without a reason, a reason outside the list, or an exclusion that the profile table
  does not carry, fails the plan comparison (control in 5.6).
- Gate: every `run` row has `passed == asserted == vectors`; every `excluded` row has
  `asserted == 0` and `parsed == vectors`; totals equal the row sums and are non-zero; all 256
  opcodes appear exactly once. Pass counts never enter the committed file.
- Evidence tier T1, startup tier `none` (vectors inject CPU state; not tier A). No claim naming a
  profile is written in Phase 2 (`ALLOWED_PROFILES` stays `none`); the verification report records
  per profile: sample SHA-256, commit, expected-inventory SHA-256, results-file SHA-256, totals,
  full-lane date and totals.

### 5.6 Controls (each a CTest `.self_test`; faulty inputs built at test time, never committed)

All use `tests/cmake/expect_fail.cmake` (exact exit code plus message regex). Exit codes: 0 pass, 1
assertion failure, 2 usage, 3 data or parse error, 4 inventory mismatch.

| Control | Fault | Required result |
|---------|-------|-----------------|
| `cpu.vectors.wrong_profile` | rockwell sample under lynx1 | exit 1; failing opcode set is exactly the 32 `$x7/$xF` (parse `--results`); each has at least 1 failure |
| `cpu.vectors.wrong_profile_reverse` | synertek sample under lynx2, poison hashed | exit 1; same exact set |
| `cpu.vectors.no_step` | runner flag `--fault no-step` (loads state, never calls the CPU) | failures = asserted - identity |
| `cpu.vectors.corrupt_expect` | mini-sample with one `final.a` changed | exit 1, exactly one failure naming opcode, ordinal and field; variants: a `final.ram` value; an injected stray write (`--fault stray-write`, `nowrite` class) |
| `cpu.vectors.malformed` | truncated object, unknown key, duplicate key, missing `final`, `pc` 70000, value 256, negative number, PC byte not the opcode, object split across lines, wrong `lines` count | each exit 3 naming file and vector; none may report "0 vectors, pass" |
| `cpu.vectors.inventory_drift` | one vector removed (31 vs 32), duplicated `#file`, missing opcode row, excluded row without reason, reason outside the list, exclusion in the table missing from the file, stale exclusion in the file, wrong totals | each exit 4 (3 for sample-side faults) with a specific message |
| `cpu.vectors.sample_identity` | one byte of the sample changed | exit 3 "sha256 mismatch" before any parse |
| `cpu.vectors.mutant_profile` | test-local table copy with `$69` decoding as SBC | failing set exactly `{69}` |
| `policy.vectors_pin` | broken `upstream.sha256` (http URL, 63-hex digest, 255 rows, bad path) | rejected; lint in the style of `tests/cmake/pins.cmake` |

Mini-samples (a few `#file` sections) are cut from the real sample by a `tests/cpu/*.sh` helper
(awk, shellcheck 0.9.0 clean) and paired with their own plans and a fresh SHA-256, so only the two
positive runs and the sample-identity control touch the full 3.2 MB.

### 5.7 Authored scenario vectors (CPU-04), compared per bus cycle

Run through the internal cycle seam with a caller-owned log (address, data, read/write, kind per
cycle) and per-cycle IRQ/NMI line levels; each expected value carries a source tag from the closed
set `LT-HW` (lynx-tests hardware-informed), `WDC`, `RKW`, `CMD`, `SST-MODEL`, `INFERRED`, plus an
`unverified` flag; the runner prints counts per tag and per profile. Minimum set (CONTEXT list):
CLI/SEI/PLP/RTI latency; the `$x3/$xB` block with IRQ pending (Test 8 replica and its `$EA`
control); D cleared in an IRQ and restored by RTI (Test 2); BRK return address and pushed P;
IRQ during BRK; NMI edge; reset sequence; RMB/SMB/BBR/BBS bits 0 to 7 on lynx2; lynx1 `$x7/$xF`
shapes with an operand byte that is itself a visible instruction (e.g. `$E8`); and the directed edges
the sample cannot reach: `JMP ($xxFF)`, zero-page wrap of `(zp,X)`, `zp,X`, `(zp)` pointer at `$FF`,
stack wrap at `$0100/$01FF`, branch and BBR at page ends, index carry into page `$FF`, decimal results
at `$99`. Two controls: turning `$CB` into a 2-cycle polling op fails the Test 8 replica; a
wrong-profile run fails the lynx2 bit-op scenarios. (The Test 8 replica checks the CPU rule only; the
full lynx-tests `cpu` suite needs Mikey timers, a later phase.)

### 5.8 Runtime estimate (16,384 vectors under ASan + UBSan, Debug -O0)

Per runner invocation on this host: SHA-256 of the sample 0.23 s, parse about 0.1 to 0.3 s, execute
8,192 vectors of about 4 cycles each about 0.1 to 0.3 s: **about 0.5 to 0.9 s per variant** [INFERRED
from the measured hash and scan times]. Both positive runs about 1.5 s total; the gcc x86 CI runner
perhaps 2 to 3 times that. The full lane runs a release build (5.12 M vectors, estimated 10 to 30 s plus
the download) and one ASan+UBSan run per pin bump [CITED: lens 3 for the lane shape; estimate INFERRED].

## 6. Bus-cycle catalogue and decimal mode (source for the executor)

### 6.1 Method

A throwaway scratch script (not committed) extracted, for every opcode, the bus-address pattern of
all 32 sampled vectors of each variant and matched each cycle's address to symbolic labels computed
from the initial state (PC+k, operand bytes, `zp+X`, effective address, stack slots, vectors). The
result is a verified description of the SingleStepTests generator's traces. These dummy-cycle
addresses are generator choices, not Lynx facts: adopt them so a `--trace` diagnostic can match, but
mark every dummy address unverified. `P0` below is the address of the opcode byte, `P1 = P0+1` etc.

### 6.2 Sequences by mode group (cycle kind in brackets; "applies" = the cycle whose data completes the operation)

| Mode group (opcodes, rockwell numbering) | Cycles | Sequence |
|------------------------------------------|--------|----------|
| Implied / accumulator (e.g. `18 38 58 78 88 8A 98 9A A8 AA B8 BA C8 CA D8 E8 EA F8 0A 1A 2A 3A 4A 6A`) | 2 | OPCODE@P0; DUMMY_STREAM read@P1 (applies) |
| Immediate (`09 29 49 69 89 A0 A2 A9 C0 C2 C9 E0 E2 E9`) and 2-byte reserved NOPs (`02 22 42 62 82 C2 E2`) | 2 | OPCODE@P0; OPERAND@P1 (applies) |
| 1-byte NOPs (`$x3 $xB` incl. `CB DB` in our profiles) | 1 | OPCODE@P0 (no POLL) |
| Zero page read / write (`05 24 25 44 45 A4 A5 A6 C4 C5 E4`; `64 84 85 86`) | 3 | OPCODE; OPERAND@P1; READ or WRITE@zp |
| Zero page RMW (`04 06 26 46 66 C6 E6`, RMB/SMB) | 5 | OPCODE; OPERAND; READ@zp; RMW_READ@zp; WRITE@zp |
| Zero page,X/Y read (`15 34 35 54 55 B4 B5 D4 D5 F4`; `B6`) | 4 | OPCODE; OPERAND; DUMMY read@zp (unindexed); READ@(zp+X or Y)&FF |
| Zero page,X/Y write (`74 94 95 96`) | 4 | same, final cycle WRITE |
| Zero page,X RMW (`16 36 56 76 D6 F6`) | 6 | OPCODE; OPERAND; DUMMY@zp; READ@zpx; RMW_READ@zpx; WRITE@zpx |
| Absolute read / write (`0D 2C 2D 4D AC AD AE CC CD EC`; `8C 8D 8E 9C`) | 4 | OPCODE; OPERAND P1; OPERAND P2; READ or WRITE@abs |
| Absolute RMW (`0C 0E 1C 2E 4E 6E CE EE`) | 6 | OPCODE; OPERAND; OPERAND; READ; RMW_READ; WRITE (@abs) |
| Absolute,X/Y read (`19 39 59 B9 BE D9`; `1D 3C 3D 5D BC BD DD`) | 4, or 5 on page cross | OPCODE; OPERAND; OPERAND; [cross: DUMMY_STREAM read@P2]; READ@EA |
| Absolute,X/Y write (`99 9D 9E`) | 5 | OPCODE; OPERAND; OPERAND; DUMMY_STREAM@P2 (always); WRITE@EA |
| Absolute,X RMW shifts (`1E 3E 5E 7E`) | 6, or 7 on cross | OPCODE; OPERAND; OPERAND; [cross: DUMMY@P2]; READ@EA; RMW_READ@EA; WRITE@EA |
| Absolute,X RMW inc/dec (`DE FE`) | 7 | as above, DUMMY@P2 always |
| `(zp,X)` read / write (`01 21 41 61 A1 C1 E1`; `81`) | 6 | OPCODE; OPERAND; DUMMY@zp; POINTER@zpx; POINTER@zpx+1 (8-bit wrap); READ or WRITE@ea |
| `(zp),Y` read (`11 31 51 71 B1 D1 F1`) | 5, or 6 on cross | OPCODE; OPERAND; POINTER@zp; POINTER@zp+1 (wrap); [cross: DUMMY_STREAM@P1]; READ@EA |
| `(zp),Y` write (`91`) | 6 | as above, DUMMY@P1 always, WRITE@EA |
| `(zp)` read / write (`12 32 52 72 B2 D2 F2`; `92`) | 5 | OPCODE; OPERAND; POINTER@zp; POINTER@zp+1; READ or WRITE@ea |
| Relative (`10 30 50 70 90 B0 D0 F0`) | 2 / 3 taken / 4 taken across a page | OPCODE; OPERAND@P1; [taken: DUMMY_STREAM@P2]; [cross: DUMMY@((P2 & FF00) or (target & FF))] |
| BRA (`80`) | 3, or 4 on cross | as taken branch |
| Zero-page relative, lynx2 BBR/BBS (`0F 1F ... FF`) | 5 / 6 taken / 7 taken across | OPCODE; OPERAND zp@P1; READ@zp; DUMMY read@zp; OPERAND offset@P2; [taken: DUMMY_STREAM@P3]; [cross: DUMMY@P3] |
| JMP abs (`4C`) | 3 | OPCODE; OPERAND; OPERAND (PC = abs) |
| JMP (abs) (`6C`) | 6 | OPCODE; OPERAND; OPERAND; POINTER@abs (PCL); POINTER@abs+1 (PCH, 16-bit); DUMMY@abs+1 |
| JMP (abs,X) (`7C`) | 6 | OPCODE; OPERAND; OPERAND; DUMMY@P1; POINTER@abs+X; POINTER@abs+X+1 |
| JSR (`20`) | 6 | OPCODE; OPERAND P1; DUMMY@stack top; STACK_WRITE@S (PCH of P0+2); STACK_WRITE@S-1 (PCL); OPERAND P2 (PC = abs) |
| PHA PHP PHX PHY (`48 08 DA 5A`) | 3 | OPCODE; DUMMY_STREAM@P1; STACK_WRITE@S |
| PLA PLP PLX PLY (`68 28 FA 7A`) | 4 | OPCODE; DUMMY@P1; DUMMY (stack read)@S; STACK_READ@S+1 |
| RTS (`60`) | 6 | OPCODE; DUMMY@P1; DUMMY@S; STACK_READ@S+1 (PCL); STACK_READ@S+2 (PCH); DUMMY read at the pulled address (then PC + 1) |
| RTI (`40`) | 6 | OPCODE; DUMMY@P1; DUMMY@S; STACK_READ@S+1 (P); S+2 (PCL); S+3 (PCH) |
| BRK (`00`) | 7 | OPCODE; OPERAND@P1; STACK_WRITE PCH (S); PCL (S-1); P (S-2); VECTOR `$FFFE`; VECTOR `$FFFF` |
| `$5C`, `$DC`, `$FC`, lynx1 `$1F $3F ... $FF` | 4 | OPCODE; OPERAND; OPERAND; DUMMY@P2 (sheets say `$5C` is 8 cycles: excluded from cycle comparison, tier `state`) |
| lynx1 `$x7` even high nibble | 3 | as zero page read, no effect |
| lynx1 `$x7` odd high nibble | 4 | as zero page,X read, no effect |
| lynx1 `$0F $2F ... $CF` (even high nibble) | 3 | OPCODE; OPERAND; OPERAND (no data read) |

Decimal ADC/SBC with D set: one extra cycle. For memory operand modes the generator re-reads the
effective address (`65 75 6D 7D 79 61 71 72` and the SBC counterparts, e.g. `zp zp`, `abs abs`,
`(zp)+Y (zp)+Y`); for immediate it reads an arbitrary fixed address (`$0056` for ADC, `$0000` for
SBC), so choose: extra DUMMY read at the same address as the data read, and at P1 for immediate.
All unverified for Lynx.

### 6.3 Verification summary

Cycle counts per opcode from the sample (rockwell): every opcode in 6.2 has the listed count and
count variants; synertek equals rockwell for all 224 other opcodes and differs only on the 32
`$x7/$xF` [VERIFIED: template comparison over 16,384 vectors]. Notable observed counts:
`1E/3E/5E/7E` 6 or 7, `DE/FE` 7, `6C` 6, `7C` 6, `5C/DC/FC` 4, `0F`-family 5/6/7, `69/E9/65/E5/6D/ED`
2-6 depending on D and crossing.

### 6.4 Binary ADC/SBC

`ADC`: `r = a + m + c`; `C = r > 0xFF`; `V = (~(a ^ m) & (a ^ r) & 0x80) != 0`. `SBC`: `m' = m ^ 0xFF`,
then as ADC. N and Z from the 8-bit result. All unsigned arithmetic in `uint32_t`.

### 6.5 Decimal ADC/SBC (D set) [VERIFIED: 0 mismatches over 288 ADC and 288 SBC sampled vectors, both variants, N Z C V A under mask 0xCF]

```
ADC (D set), c = carry in:
  al = (a & 0x0F) + (m & 0x0F) + c
  if (al >= 0x0A) al = ((al + 6) & 0x0F) + 0x10
  ap = (a & 0xF0) + (m & 0xF0) + al              /* unsigned */
  V  = ((~(a ^ m)) & (a ^ (ap & 0xFF)) & 0x80) != 0   /* from the sum BEFORE the high-nibble adjust */
  if (ap >= 0xA0) ap += 0x60
  C  = ap >= 0x100;  result = ap & 0xFF;  N, Z from result
SBC (D set):
  r   = a - m - (1 - c)                          /* signed 32-bit arithmetic is fine here */
  C   = r >= 0;  V = ((a ^ m) & (a ^ r) & 0x80) != 0       /* the binary flags */
  res = r & 0xFF
  if (((a & 0x0F) - (m & 0x0F) - (1 - c)) < 0) res = (res - 6) & 0xFF
  if (r < 0)                                   res = (res - 0x60) & 0xFF
  N, Z from res
```
The alternative published SBC form with `((al - 6) & 0x0F) - 0x10` mismatches 9 of 152 (synertek) and
12 of 145 (rockwell) decimal SBC vectors; do not use it. Invalid BCD digits are whatever these
formulas produce (the vectors assert them; they are software-model values, unverified for Lynx,
hence the `state` tier and the `decimal` count).

## 7. Fixture programs and independently derived expectations

Both programs were assembled with the pinned ca65/ld65 (`ca65 --cpu 65SC02`, config below) and the
bytes equal the hand assembly. They use only opcodes both profiles define identically, so they run
unchanged under `none`, `lynx1`, `lynx2` (verified against the opcode-pair table: no `$x7/$xF`).

Linker config, committed as `fixtures/cpu/raw-0200.cfg` (the check step assembles with
`ca65 --cpu 65SC02 -o X.o X.s` then `ld65 -C raw-0200.cfg -o X.bin X.o`, in `env -i` like the probe
step; the output has no header and no padding, so it has exactly the program's length) [VERIFIED:
produced 35 and 46 bytes here]:

```
MEMORY { RAM: start = $0200, size = $0100, type = ro, file = %O; }
SEGMENTS { CODE: load = RAM, type = ro; }
```

### 7.1 Terminating program T (`fixtures/cpu/sum-trap.s`, load $0200, RAM policy zero, profile none)

```
a2 05 a9 00 86 10 18 65 10 ca d0 f8 8d 00 03 20 20 02 a2 ff bd 01 02 48 a9 00 68 07 ea ea ea ea e6 11 60
(35 bytes; SHA-256 of the bytes 6ae9bada8fb004f5756821c0e756267d2a2e44e6f68928fe27984906acc00f33)
```
Source: `LDX #5; LDA #0; loop: STX $10; CLC; ADC $10; DEX; BNE loop; STA $0300; JSR sub; LDX #$FF;
LDA $0201,X; PHA; LDA #0; PLA; .byte $07; .byte $EA x4; sub: INC $11; RTS`. The `$07` is a bit-op
slot, so `none` stops there with UNDEFINED. Power-on values per 4.4 (S = `$FD`, P reads `$34`).
Cycle counts are the datasheet/vector counts (SST rockwell templates agree for each: LDX#/LDA# 2,
STX zp 3, CLC 2, ADC zp 3, DEX 2, BNE 3 taken / 2 not, STA abs 4, JSR 6, INC zp 5, RTS 6,
LDA abs,X 5 on the page crossing, PHA 3, PLA 4), 5 ticks per cycle (`flat5`, PLACEHOLDER).

| PC | Instruction | Cycles | Cumulative cycles = ticks | Result |
|----|-------------|--------|---------------------------|--------|
| 0200 | LDX #$05 | 2 | 2 = 10 | X=05 |
| 0202 | LDA #$00 | 2 | 4 = 20 | A=00 |
| 0204 | STX $10 | 3 | 7 = 35 | [10]=05 |
| 0206 | CLC | 2 | 9 = 45 | C=0 |
| 0207 | ADC $10 | 3 | 12 = 60 | A=05 |
| 0209 | DEX | 2 | 14 = 70 | X=04 |
| 020A | BNE $0204 (taken) | 3 | 17 = 85 | |
| | iteration 2 (X=4) | 13 | 30 = 150 | A=09, X=03, [10]=04 |
| | iteration 3 (X=3) | 13 | 43 = 215 | A=0C, X=02, [10]=03 |
| | iteration 4 (X=2) | 13 | 56 = 280 | A=0E, X=01, [10]=02 |
| | iteration 5 (X=1): STX 3, CLC 2, ADC 3, DEX 2, BNE not taken 2 | 12 | 68 = 340 | A=0F, X=00, [10]=01, PC=020C |
| 020C | STA $0300 | 4 | 72 = 360 | [0300]=0F |
| 020F | JSR $0220 | 6 | 78 = 390 | push 02 at $01FD, 11 at $01FC; S=FB |
| 0220 | INC $11 | 5 | 83 = 415 | [11]=01 |
| 0222 | RTS | 6 | 89 = 445 | S=FD, PC=0212 |
| 0212 | LDX #$FF | 2 | 91 = 455 | X=FF |
| 0214 | LDA $0201,X | 5 | 96 = 480 | EA=$0300 (carry from $01+$FF), A=0F |
| 0217 | PHA | 3 | 99 = 495 | [01FD]=0F (overwrites the 02), S=FC |
| 0218 | LDA #$00 | 2 | 101 = 505 | A=00 |
| 021A | PLA | 4 | 105 = 525 | A=0F, S=FD |
| 021B | `07` | stop | | UNDEFINED, consumed total 525, PC=021B |

Expected final state under `none` with a budget of at least 525: `consumed = 525`,
`stop = UNDEFINED`, PC `$021B`, A `$0F`, X `$FF`, Y `$00`, S `$FD`, P reads `$34` (N=0, Z=0, C=0,
V=0, D=0, I=1), RAM: `$0010=01`, `$0011=01`, `$0300=0F`, `$01FC=11`, `$01FD=0F`, everything else
zero except the loaded image. A second `advance` returns UNDEFINED with consumed 0 and an identical
buffer. S2 edge cases on this program: `advance(524)` grants all 105 cycles (the last at tick 520),
consumes exactly 524 and reports BUDGET with the registers already final (one cycle is still in
flight until tick 525); a following `advance(1)` consumes 1 tick (BUDGET) and the next call reports
UNDEFINED with consumed 0. `advance(520)` stops before the last `PLA` cycle: A is still 0 and S
still `$FC`, PC is already `$021B`.

### 7.2 Never-ending loop L (`fixtures/cpu/churn-loop.s`, load $0200)

```
a2 00 a0 08 a9 00 85 40 a9 05 85 41 bd 00 04 45 30 71 40 9d 00 06 2a 48 20 2b 02 68 85 30 e8 d0 eb e6 31 80 e7 ea ea ea ea ea ea 04 32 60
(46 bytes; SHA-256 of the bytes dcee7216a88ff2ffaf3e4a2794c3369676fc47493d710f26cd8bbadd19745b77)
```
Source: prelude `LDX #0; LDY #8; LDA #0; STA $40; LDA #5; STA $41` (14 cycles) then loop head `$020C`:
`LDA $0400,X; EOR $30; ADC ($40),Y; STA $0600,X; ROL A; PHA; JSR $022B; PLA; STA $30; INX; BNE
loop; INC $31; BRA loop`, subroutine `$022B: TSB $32; RTS`. Control flow depends only on X, so
the cycles per iteration are constant: 4+3+5+5+2+3+6+(5+6)+4+3+2+3 = 51 cycles = **255 ticks** for
every iteration with X from 0 to 254 (BNE taken), after a **70-tick** prelude. Pointer `($40),Y` is
`$0500 + 8`, so no page crossing; `LDA $0400,X` and `STA $0600,X` never cross (low byte `$00`). It
reads seeded RAM (`$0400+X`, `$0508`, `$30`, `$32`), writes RAM page 6, zero page and the stack, and
never touches D, so no decimal path is involved.

Hand-derived checks (RAM policy zero, none): after `advance(325)` (prelude 70 + one iteration 255):
consumed 325, PC `$020C`, X=1, Y=8, A=0, S=`$FD`, P reads `$34` (the last flag write is `INX` with
X=1, so N=0 and Z=0; C stays 0); RAM `$01FD=00`, `$01FC=02`, `$01FB=1A`, `$0600=00`, `$0041=05`,
`$0040=00`. After `70 + 255*k` ticks: PC `$020C`, X=k. The golden workload (seeded RAM, 4096 ticks)
ends mid-instruction after 15 full iterations (`70 + 255*15 = 3895`, then 201 ticks of the 16th);
chunked 512-tick runs still sum to 4096 and equal the single call (slicing independence).
For seeded RAM the test may re-implement the loop arithmetic in 10 lines of C using its own
SplitMix64 (the existing `ref_splitmix64`), which is a second implementation of the program, not
of the CPU.

Sensitivity: flipping the data byte `08` in `a0 08` (Y) changes which seeded byte `ADC ($40),Y` reads;
the `det.sensitivity` content flip must use this (not an opcode byte), so the digest changes for the
intended reason.

### 7.3 Profile divergence on the retained tracer image (zero RAM policy, independent values)

Image `03 0a 11 18 1f 26 2d ...` (32 bytes at `$0200`). Under `none`: `03` (1 cycle), `0A` ASL A (2),
`11 18` ORA ($18),Y (5; pointer `$0018`=`$0000`, Y=0, reads `$0000`) = 8 cycles = **40 ticks**; then
`$1F` is a bit-op slot: UNDEFINED at PC `$0204`, consumed 40, A=0, X=0, Y=0, Z=1, S=`$FD`.
Single steps after that: **lynx1** `1F 26 2D` is a 3-byte NOP of 4 cycles: `advance(60)` leaves PC
`$0207`; **lynx2** `1F` is BBR1 `$26`,+`$2D`: `RAM[$26]` is 0, bit 1 clear, so the branch is taken: 6
cycles, `advance(70)` leaves PC `$0207 + $2D = $0234`. Same bytes, three documented outcomes. (The
lens-4 draft expected `$0205` for lynx1 because it assumed a 1-byte NOP; the CONTEXT rule gives
`$0207`: risk R3.) Also assert that `none` stops with consumed 0 at each of the 32 bit-op opcodes
placed first in an image, and runs the other 224 without UB under ASan.

## 8. Cost model and success criterion 3

`core/bus.c`: `static const uint8_t lynx__cost[LYNX__COST_MODEL_COUNT][LYNX__REGION_COUNT][LYNX__CK_COUNT]`
with `LYNX__REGION_COUNT == 1` in Phase 2. Model 0 `flat5` is 5 in every slot and is labelled
PLACEHOLDER in code and docs (page mode, device waits and stalls are not modelled; no claim cites
ticks). Model 1 `test_skew` uses distinct nonzero values per kind, for example opcode 4, operand 6,
dummy_stream 3, dummy 10, read 7, write 9, rmw_read 8, stack_read 11, stack_write 12, pointer 13,
vector 14. It is internal, set through `lynx__machine_set_cost_model`, stored in the snapshot payload
and refused on mismatch.

Tests (public-path plus seam): (1) run T under `flat5` and `test_skew` to UNDEFINED: registers and
RAM equal; the consumed-tick difference equals the sum over the CPU-seam bus log of
(skew[kind] - 5), where the test holds its own copy of the skew values; (2) the same run on the seam
with the flat test bus logs every cycle and the replay of the logged writes onto the initial RAM
equals the final RAM (catches a write that bypassed the bus; the include gate catches reads); (3)
named-fault controls: a cost function that ignores the table (the constant-5 anti-pattern) fails
(1); a swapped cycle order in one mode fails the scenario vectors; `<=` in the loop fails the
exact-budget test; (4) trace ring (optional): an internal fixed-size ring of
`{stamp, addr, data, kind|rw, ticks}`, off by default and excluded from snapshot and digests, with a
test that on/off does not change any digest.

## 9. Work breakdown: what changes in Phase 1 code and tests

### 9.1 Impact matrix (every use found by grep of the repository; none omitted)

Triggers: (a) remove the skeleton step, `LYNX__HALT_OPCODE`, `lynx__budget_steps`; (b) new stop
semantics (UNDEFINED = 4, HALT reserved, S2 budget); (c) snapshot format 2; (d) new registers; (e)
golden re-baseline; (f) claim and inventory hash.

| File | Change | Trigger |
|------|--------|---------|
| `include/lynxplosion.h` (lines 37-66 hold the stop, profile and register enums; the advance and snapshot comments say "version 1", "budget smaller than one machine step consumes nothing", "PROVISIONAL ... skeleton") | add `LYNX_STOP_UNDEFINED = 4`, `LYNX_PROFILE_LYNX1/LYNX2`, `LYNX_REG_CPU_STATE/CPU_LATCH`, `LYNX_REG_COUNT = 8`, `lynx_profile_name`; rewrite the advance, banner, snapshot and `lynx_read_memory` text | b, c, d |
| `core/internal.h` (lines 11-24: `LYNX__SNAPSHOT_FORMAT 1u`, `LYNX__SNAPSHOT_PAYLOAD_LEN 65552u`, `LYNX__TICKS_PER_STEP 4u`, `LYNX__HALT_OPCODE 0xDBu`; struct fields `pc a x y s p halted`; `lynx__skeleton_step`, `lynx__budget_steps`) | remove the skeleton items and `halted`; add `lynx__cpu cpu`, `cpu_ready_at`, `cost_model`, `LYNX__TICK_HEADROOM`, format 2 and payload 65568, `lynx__machine_set_cost_model` | a, c |
| `core/machine.c` | delete `lynx__skeleton_step`, `lynx__budget_steps`; init accepts profile 0..2; `lynx__power_on` calls `lynx__cpu_init`; the loader starts the CPU at the load address; S2 `lynx_advance`; `lynx_read_register` ids 6 and 7; `lynx_profile_name` | a, b, d |
| `core/snapshot.c` | layout (4.6), offsets (`P_HALTED` removed), value checks, `repost` on apply, comment block | c |
| `CMakeLists.txt` | add `core/cpu.c core/cpu_profile.c core/bus.c` to `LYNX_CORE_SOURCES` (feeds both `lynx_core` and the plain twin) | new |
| `runner/bundle.c` | `profile_text` (currently `v == LYNX_PROFILE_NONE ? "none" : NULL`) uses `lynx_profile_name`; `stop_text` adds `"undefined"`; `BUNDLE_REGS_LEN` derives from `LYNX_REG_COUNT` (automatic) | b, d |
| `runner/main.c` | `--profile none|lynx1|lynx2` (default none, unknown exits 2 with usage), usage line, UNDEFINED note on stderr built from public reads, exit 0 | b |
| `tests/header/header.c` | static asserts for `LYNX_STOP_UNDEFINED == 4`, `LYNX_REG_COUNT == 8` (currently `== 6`), profile ids; function-pointer table gains `lynx_profile_name` | b, d |
| `tests/api/test_api.c` | see 9.2 | a, b, c, d |
| `tests/det/test_det.c` | `copy[i]->a ^= 1u` becomes `copy[i]->cpu.a`; add flips of every new state field; `instance_*` helpers unchanged (chunked advance still sums to 4096) | a, d |
| `tests/det/tests.cmake`, `golden.cmake`, `two_runs.cmake`, `sensitivity.cmake`, `manifest_fields.cmake`, `runner_cli.cmake` | content paths and variable names (`LYNX_SKELETON_*`); `two_runs.cmake` line 19 pins the tracer hash `ab5f8b5c...` (becomes the loop image hash); `sensitivity.cmake` flips `03 0a` (becomes a data-byte flip); `manifest_fields.cmake` (c) uses a `db` halting image: becomes `ea 07` (consumed 10, stop `undefined`); `runner_cli.cmake` lines 67-78 use `db` and expect `halt`: becomes content `07` (consumed 0, `undefined`) | a, b, e |
| `tests/golden/skeleton.digest` | rename to `cpu-loop.digest`, rewrite header text and regenerate command (add `--profile none`), new digest | e |
| `fixtures/skeleton/*`, `fixtures/manifests/skeleton-image.manifest` | keep (identity and loader tests); add `fixtures/cpu/*` and their manifests | new |
| `tests/inventory/fast.txt` | rows per plan (9.3); `api.skeleton` becomes `api.cpu_program` | f |
| `docs/claims/skeleton-repeatability.claim` | `inventory_sha256` must be refreshed by every plan that edits `fast.txt` (`docs.claim_template` fails on a stale value [VERIFIED: scripts/check-claims.sh stale check]); at plan D1 re-point claim text, `content_sha256` (hash of the loop image bytes) and evidence names (keep the profile `none`) | f |
| `README.md`, `scripts/validate-kv.sh`, `scripts/check-manifests.sh`, `scripts/check-claims.sh`, `tests/det/manifest_controls.sh`, `tests/gates/manifests_controls.sh`, `docs/*` | profile text; `startup_tier` option; controls; provenance and exposure rows | new |

### 9.2 `tests/api/test_api.c`, case by case

Cases that call `lynx_advance`/`advance_ticks` with the tracer image today (grep): `skeleton` (1),
`advance_budget` (18), `inspect_pure` (3), `instances_independent` (2), `load_refusal_unchanged`
(1), `reset_power_on` (4), `snapshot_roundtrip` (13), `snapshot_refusal` (3 through `snap_instance`
and `advance_ticks`). Under `none` the tracer stops with UNDEFINED at `$0204`, so every one that
needs a running machine moves to the loop program L; cases that only load (identity, load
refusals, snapshot identity) keep the tracer.

| Case | New content |
|------|-------------|
| `skeleton` -> `cpu_program` | rename in `tests.cmake`, `fast.txt`, case table; hand-derived expectations of 7.1 (T, trace table) and 7.2 (L after 325 ticks); defaults check stays |
| `advance_budget` | budget 0 consumes 0; S2 semantics (`advance(1)` performs one cycle, consumed 1, `PC` after the fetch); consumed == budget except UNDEFINED/clamp; the 8-chunk and sweep slicing property with whole-buffer memcmp; `07` trap image for the `UINT64_MAX` budget (consumed 0, UNDEFINED, repeated, buffer unchanged); near-limit case with the headroom rule; the wrap image becomes a top-of-memory `NOP x13; JMP $FFF0` loop (image `ea` x13 then `4c f0 ff`); the removed `lynx__budget_steps` assertions go |
| `reset_power_on` | declared CPU values (S=`$FD`, P reads `$34`, A=X=Y=0), profile kept, halt image replaced by the `07` trap (reset clears content: a following `advance` refuses with STATE until a load) |
| `snapshot_roundtrip` | loop L; the "halt kept through a snapshot" block (lines around 991-1002) becomes "UNDEFINED state and a mid-instruction state survive a snapshot"; add the coverage test (every CPU field non-default, restore into a fresh instance, whole-buffer memcmp, continue both and compare again) |
| `snapshot_refusal` | the edit at `pay + 7` (`halted`) becomes the `cpu flags` byte with undefined bits; delete "PC outside content" and add the positive case (a PC outside the content range restores); new cases: format 1 and 3 give VERSION, every cross-profile pair MISMATCH, bad P bits, bad reserved bytes, `step`/`opcode` out of range, `cost_model` mismatch (each with a recomputed payload hash so the value check refuses, not the hash) |
| `instances_independent` | loop L, plus a lynx1 and a lynx2 instance interleaved with a `none` instance, each equal to its solo run |
| `init_refusal` | profile 3 and `0xFFFFFFFF` refused with the buffer untouched; 1 and 2 accepted; mutating the caller's config after init has no effect |
| `inspect_pure` | the new ids, `LYNX_REG_COUNT` still RANGE |
| `identity`, `load_refusal_unchanged`, `build_id`, `sha256` | unchanged except advance calls |

### 9.3 Dependency-ordered plan sequence (each plan green at its boundary)

Rules for every plan: `fast.txt` edits and the claim's `inventory_sha256` update go in the same
commit (`sh scripts/check-claims.sh --inventory-sha256`); new `-P` CMake scripts start with
`cmake_minimum_required(VERSION 3.25)` and use `cmake_policy(VERSION 3.25)` when they use `IN_LIST`;
new `.sh` files must pass shellcheck 0.9.0; findings are printed with `message(NOTICE)` then a
`FATAL_ERROR`; run `scripts/check.sh` in an `ubuntu:24.04` container before pushing (LESSONS.md).

| Plan | Scope | Files (approx.) | New tests (inventory rows) |
|------|-------|-----------------|----------------------------|
| A. CPU core and profiles, white-box | `core/cpu.h`, `cpu.c`, `cpu_profile.def`, `cpu_profile.c`; CMake source list; a flat-bus seam in `tests/cpu/`; `tests/cmake/cpu_includes.cmake` (the new include gate: `core/cpu*.c` may include only `cpu.h`, `cpu_profile.def` and stdint/stddef/stdbool/string) with a self-test; the machine is untouched (still the skeleton) | 9 | `core.cpu_includes`, `core.cpu_includes.self_test`, `cpu.tables` (256 rows both, differ-set exactly 32, none-intersection), `cpu.tables.self_test` (mutated differ-set), `cpu.cycles` (hand-authored cycle lists for one opcode per group of 6.2) |
| B. Vector runner, sample, inventories, controls, full lane | `tests/cpu/{sst_reader.c,.h,test_cpu_vectors.c,tests.cmake}`, sample files, LICENSE, `upstream.sha256`, `.gitattributes`, two manifests, two inventories, `scripts/vectors-full.sh`, `validate-kv.sh` option and controls, pin lint, provenance and exposure rows | 18 | `cpu.vectors.lynx1`, `cpu.vectors.lynx2`, the eight `.self_test` controls of 5.6, `policy.vectors_pin`, `policy.vectors_pin.self_test` |
| C. Authored scenario vectors | `tests/cpu/scenarios*.c` (sources and unverified tags, counts per tag), Test 8 replica and its control | 4 | `cpu.scenarios.lynx1`, `cpu.scenarios.lynx2`, `cpu.scenarios.test8.self_test` |
| D0. Fixtures and the ca65 gate | `fixtures/cpu/{sum-trap.s,sum-trap.hex,churn-loop.s,churn-loop.hex,raw-0200.cfg}`, two manifests, `scripts/check.sh` step `cpu-fixtures` after `probe` (assemble into `build/toolchain/`), `tests/cmake/cpu_fixtures.cmake` (compare each assembled `.bin` with its hex, compare the C arrays) | 10 | `fixtures.cpu_asm`, `fixtures.cpu_asm.self_test` (hex copy with one byte changed fails) |
| D1. Machine switch | header, `internal.h`, `machine.c`, `bus.c`, `snapshot.c`, `bundle.c`, `main.c`, every Phase 1 test of 9.1/9.2, new golden, claim re-point; the CPU path becomes the machine path | 25+ (split by task; plan boundary is what must be green) | `api.skeleton` -> `api.cpu_program` plus the renamed golden and claim; no new rows beyond D2's |
| D2. Public-path tests | `api.advance_slicing`, `api.profile_select`, `api.cost_model` (criterion 3), `api.snapshot_coverage` (or inside roundtrip), `api.seam_agreement` (restore a vector state, `advance` the seam's cycle count, compare to the internal step; one vector per opcode per profile), `det.profile_agreement`, `cpu.profile_divergence` (7.3) | 6 | those names |
| E. Documentation, measurement, verification | README Profiles section, `docs/` profile provenance document (every non-vector-covered entry cites a source or says unverified), SKELETON.md pointer line, full-lane run, CI efficiency review, verification report | 6 | none |

Keep D1 as small as the repository allows: do the mechanical test re-base task by task (header,
core, snapshot, runner, api tests, det tests, golden, claim) and run `scripts/check.sh` at the end.
If D1 proves too large for one plan, split it as D1a (core, header, runner and their direct unit
tests on the loop fixture) and D1b (det tests, golden, claim), but both halves must be green at
their boundary: the golden regeneration and the claim re-point stay in the same plan as the
machine switch, because a disabled or skipped golden gate is not allowed (`policy.no_skip`).

Re-baseline order inside D1 (CONTEXT): vectors already green (plan B), hand-derived program test
green, then regenerate the golden on Apple clang, confirm the same digest on Linux gcc (container or
CI), record the reason in the commit message, update the claim once.

## Architecture Patterns

### System architecture (data flow)

```text
 lynx_advance(budget) ─► clamp target ─► loop while guest_ticks < target
                                │
        cpu_ready_at > now? ──yes─► now = min(target, ready_at)
                                │ no
        at boundary + profile none + opcode differs between tables? ──yes─► stop UNDEFINED (consumed so far)
                                │ no
            req (addr, kind, flags) from lynx__cpu ─► bus_grant ─► bus_timing_of(cost_model, region, kind)
                                │                                         │
                    bus_access(raw RAM view) ◄────────── stamp = grant tick, ready_at = stamp + ticks
                                │ data
            lynx__cpu_complete(cpu, table, data, lines) ─► next req (or boundary)
 vector runner:  flat 64 KiB array + log  ─► same lynx__cpu_complete loop, one instruction at a time
```

### Recommended project structure

```
core/    cpu.h cpu.c cpu_profile.def cpu_profile.c bus.h bus.c machine.c snapshot.c internal.h
tests/cpu/   sst_reader.c sst_reader.h test_cpu_vectors.c test_cpu_scenarios.c cases.sh tests.cmake
tests/inventory/vectors/   lynx1.txt lynx2.txt
fixtures/vectors/sst65x02/ synertek65c02.sample rockwell65c02.sample LICENSE upstream.sha256
fixtures/cpu/   sum-trap.{s,hex} churn-loop.{s,hex} raw-0200.cfg
scripts/vectors-full.sh   (pin | sample | run)
```

### Don't hand-roll

| Problem | Don't build | Use instead | Why |
|---------|-------------|-------------|-----|
| SHA-256 of the sample | a second implementation | `core/sha256.c` | Already tested against four FIPS vectors |
| Hex content decoding in the runner | a new parser | `bundle_read_hex` | Existing, strict |
| Negative-control plumbing | WILL_FAIL | `tests/cmake/expect_fail.cmake` | Exact code plus regex |
| Inventory comparison | custom diff | `cmake -E compare_files` or the runner's byte compare | Exact |
| SplitMix64 for seeded expectations | borrowing the core's | the test's own `ref_splitmix64` | Independence (existing) |
| Assembling fixtures | a Python assembler | the pinned `ca65`/`ld65` | Already built by the check |

**Key insight:** the strict JSON reader and the opcode switch are the only new code worth writing
by hand; both are small and are checked by 16,384 vectors, so a library would add a dependency for
no coverage gain.

### Anti-patterns
- A cycle table the CPU sleeps on (cycle counts must emerge from the micro-sequence).
- Two bus accesses in one C expression (unspecified order; gcc and clang differ).
- Storing a table pointer or request pointer in the instance (breaks memcmp oracles).
- Using the vectors' traces as a gate (generator-specific dummy addresses).
- Constant poison for unlisted reads (section 5.4).

## Common Pitfalls

1. **C undefined behaviour in the opcode switch.** Keep all CPU arithmetic unsigned and widen to
   `uint32_t` before sums or shifts; no `(int8_t)` casts for branch offsets (implementation-defined
   before C23): `target = (uint16_t)(pc + off - ((off & 0x80u) ? 0x100u : 0u))`. UBSan is on with
   `-fno-sanitize-recover=all`, so a signed overflow fails the test run.
2. **gcc vs clang.** One bus access per statement; gcc `-Wconversion` warns on `a += m` for
   `uint8_t`: write `a = (uint8_t)(a + m)`. CI-only failure class in Phase 1 (LESSONS.md); run the
   container before pushing.
3. **`-Wshadow`/`-Werror` with X-macros.** Parameter names in the macro expansion must not shadow
   locals; give macro-local variables a `lynx__` prefix.
4. **nm gates on new core files.** `core.nm_globals` rejects `B b D d C`; `static const` integer
   tables are `.rodata`. A dense `switch (mode)` may compile to a jump table: on x86-64 gcc PIC the
   table holds relative offsets in `.rodata` (expected to pass) but a toolchain that emits absolute
   addresses puts it in `.data.rel.ro`, which nm reports as `d` [ASSUMED; not reproduced here]. If
   the Linux leg fails, split the switch or add `-fno-jump-tables` to the plain twin only after
   recording why. No `const char *` tables anywhere (`lynx_profile_name` is a `switch`).
5. **`core.nm_undefined`.** Only `memcpy memmove memset memcmp` (and `bzero` on Darwin) may be
   undefined; initialising a struct with `= {0}` may call `memset` (allowed). Do not use `memchr`.
6. **Include allow-list.** `core/cpu*.c` may include only `cpu.h` (plus `cpu_profile.def`); the
   existing `core.includes` scan also requires every quoted include to exist under the root.
   `cpu.h` must not include `internal.h`.
7. **`no_float`.** The scan reads `*.c` and `*.h`: avoid decimal-point numbers and `e` exponents in
   comments of core files.
8. **shellcheck 0.9.0 on CI vs 0.11 locally** (SC2317, SC2015 differ): keep helper functions called
   directly, avoid `A && B || C`; `vectors-full.sh` and any `tests/cpu/*.sh` are linted by the
   `shellcheck` step because it globs `scripts/*.sh` and `tests/*/*.sh`.
9. **CMake `IN_LIST` in `-P` scripts** needs `cmake_policy(VERSION 3.25)` (CI cmake 3.31.6, local
   4.4.3). Avoid `string(REGEX ...)` on multi-megabyte strings (slow): use `string(FIND)` and
   `string(SUBSTRING)` for the sample, or do the cutting in sh/awk.
10. **A full-width budget on a non-terminating program never returns.** Under `lynx1`/`lynx2`
    nothing stops; every maximum-budget test (runner_cli, manifest_fields, advance_budget) must use
    a `none` trap image (`07`, or `ea 07`).
11. **Sample integrity.** `.gitattributes -text`; the sample hash is checked before parsing; a
    regenerated-layout file with a `[{` first line is a different physical layout, not a corrupt one
    (9 plus 25 files).
12. **Observation digest coverage.** `obs.regs.hex` loops over `LYNX_REG_COUNT`; adding ids changes
    the digest, so the golden and every committed digest change in the same plan.
13. **Idempotent UNDEFINED.** The check must not advance time or mutate state (whole-buffer memcmp
    before and after); the in-flight cycle's duration is consumed first, once.
14. **State canonical form.** Zero `opcode`, `ea`, `tmp` at a boundary; otherwise two machines that
    are architecturally equal differ in memcmp and snapshot equality tests fail intermittently.
15. **Stack wrap and zero-page wrap** need explicit 8-bit casts; address arithmetic that wraps at
    16 bits uses `(uint16_t)`.

## 10. Validation Architecture (Nyquist)

`workflow.nyquist_validation` is true in `.planning/config.json` (verified this session), so this section applies.

### Test framework
| Property | Value |
|----------|-------|
| Framework | CTest (CMake 3.25+), C test executables with `tests/check.h`, CMake `-P` scripts, POSIX sh controls; the exact-inventory gate (`tests/inventory/fast.txt`) and JUnit completeness (`check_junit.cmake`) |
| Config | `CMakePresets.json` (`check` preset: Debug, ASan+UBSan on all test targets, `jobs: 4`), `tests/tests.cmake` includes |
| Quick run | `ctest --preset check -R '^cpu\.'` after `cmake --workflow --preset check` (or `build/check/tests/lynx-test-cpu <case>`) |
| Full suite | `sh scripts/check.sh` (local, CI) |
| Not in check | `sh scripts/vectors-full.sh run` (network, about 2 GB): Phase 2 verification, every pin bump, before any claim |

### Phase requirements to test map
| Req | Behaviour | Test (IDs) | Negative control |
|-----|-----------|-----------|------------------|
| CPU-01 | final state equals SingleStepTests expectations under lynx1 and lynx2, exact inventory | `cpu.vectors.lynx1`, `cpu.vectors.lynx2` (each compares `tests/inventory/vectors/<id>.txt` byte for byte, then asserts); full lane in verification | `cpu.vectors.no_step`, `.corrupt_expect`, `.malformed`, `.inventory_drift`, `.sample_identity`, `.mutant_profile`, `policy.vectors_pin.self_test` |
| CPU-02 | profiles are data; tables differ in exactly the 32 `$x7/$xF`; selectable per instance; unknown id refused; `none` stops on those 32 | `cpu.tables`, `api.profile_select`, `api.init_refusal`, `cpu.profile_divergence`, `det.profile_agreement` | `cpu.tables.self_test`; `cpu.vectors.mutant_profile` |
| CPU-03 | every CPU access is a tick-stamped bus cycle; a non-default cost model changes consumed ticks but not state | `cpu.cycles`, `api.cost_model`, `api.advance_slicing`, `api.advance_budget`, `core.cpu_includes` | constant-cost fault fails `api.cost_model`; `core.cpu_includes.self_test`; `<=` mutation fails `api.advance_budget` (re-run the Phase 1 seeded mutation and record it) |
| CPU-04 | IRQ entry, CLI/SEI/PLP/RTI latency, `$x3/$xB/$CB/$DB` NOP block, reserved NOP lengths, BRK, NMI, reset follow the profile, each with source tag or unverified flag | `cpu.scenarios.lynx1`, `cpu.scenarios.lynx2` (print counts per tag) | `cpu.scenarios.test8.self_test` (`$CB` as polling op fails) |
| CPU-05 | wrong profile fails exactly the bit-op vectors | `cpu.vectors.wrong_profile`, `cpu.vectors.wrong_profile_reverse` | the controls are the evidence; `cpu.vectors.no_step` shows they exercise the CPU |

### Phase 1 properties that must keep holding
| Property | Test | Note |
|----------|------|------|
| Determinism: two runs, two instances, golden, sensitivity | `det.two_runs`, `det.two_instances`, `det.golden` (+ `.self_test`), `det.sensitivity`, `det.sensitivity_state` | new workload L; golden regenerated once; `det.sensitivity` flips a data byte; `det.sensitivity_state` flips every new CPU field |
| Refusals leave the buffer byte-identical | `api.init_refusal`, `api.load_refusal_unchanged`, `api.snapshot_refusal` | UNDEFINED stop also leaves it identical |
| Snapshot round trip and refusals | `api.snapshot_roundtrip`, `api.snapshot_refusal` | format 2, coverage test, cross-profile pairs |
| Budget never exceeded | `api.advance_budget` | consumed <= budget always |
| Boundaries | `core.includes`, `core.no_float`, `core.nm_globals`, `core.nm_undefined`, `api.abi_symbols`, `api.header_selfcontained` | all with their `.self_test` controls; `lynx_profile_name` declared and defined |
| Rights and privacy | `hygiene.firmware_scan`, `hygiene.tree`, `hygiene.manifests` (+ self-tests), the `artifacts` and `history` steps | measured clean with the sample (3.1) |
| Manifests, claims, docs | `hygiene.manifests`, `docs.claim_template`, `docs.policy`, `docs.provenance`, `det.manifest_fields` | `startup_tier = none` accepted only in fixture manifests |
| Supply chain | `policy.pins`, `fetch_pin.mismatch.self_test`, `policy.no_network` | no pin row and no network token under `tests/` |

### Sampling rate
- Per task commit: `ctest --preset check -R '^(cpu|core|api)\.'` (about 5 to 10 s).
- Per plan: `sh scripts/check.sh`.
- Phase gate: `sh scripts/check.sh` green, `scripts/vectors-full.sh run` green for both profiles
  (all 10,000 vectors per file, 5.12 M vectors), results files and SHA-256 values recorded in the
  verification report, plus the same golden digest on Linux (container run or CI).

### Wave 0 gaps (all new)
- `tests/cpu/` (reader, runner, scenarios, helper sh), `tests/inventory/vectors/*.txt`,
  `tests/cmake/cpu_includes.cmake`, `tests/cmake/cpu_fixtures.cmake`, `fixtures/vectors/**`,
  `fixtures/cpu/**`, `scripts/vectors-full.sh`; no test framework install is needed.

### Zero human UAT
Every requirement has an automated test and a named negative control. Items that stay open by
design (O1 baseline, hardware accuracy, Lynx cycle timing) are recorded as unverified in the
inventory header and the profile document, never as passed; no owner review is part of acceptance.

## Security Domain

`security_enforcement` is true in config (ASVS level 1, block on high).

| ASVS category | Applies | Control |
|---------------|---------|---------|
| V2 Authentication / V3 Session | no | no accounts or sessions |
| V4 Access control | no | none |
| V5 Input validation | yes | strict, allocation-free JSON reader with fixed bounds and fail-closed exit 3; sample identity by SHA-256 before parsing; `lynx_snapshot_restore` value checks extended |
| V6 Cryptography | partly | SHA-256 only for identity, reuse `core/sha256.c`; never hand-roll anything else |
| V10/V14 Supply chain and configuration | yes | commit-pinned, per-file SHA-256, https-only downloads with `--proto '=https' --proto-redir '=https' --max-filesize`, no archive extraction, mktemp directory removed by an EXIT trap |

| Threat pattern | STRIDE | Mitigation |
|----------------|--------|------------|
| Malformed or hostile vector file overflows a fixed buffer | Tampering / DoS | bounds on ram (64) and cycles (16), numeric range checks, exit 3, run once under ASan+UBSan on the full lane |
| Upstream force-push or file swap | Tampering | per-file SHA-256 list; the fast gate uses the committed sample |
| Path traversal through a downloaded name | Tampering | names validated as two hex digits, no extraction |
| Copyrighted or rights-bearing bytes enter the repo | Info disclosure / legal | the sample is MIT test data with its notice; hygiene and firmware scans stay un-weakened; no BIOS, ROM or emulator source is read (clean-room policy) |
| Snapshot with forged CPU state | Tampering | `lynx__cpu_state_valid` plus canonical-form and range checks after the payload hash and before any write |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | An opcode `switch` compiles to a `.rodata` jump table (or none) on Linux gcc 13 PIC, so the nm globals gate passes | Pitfall 4 | The Linux CI leg fails; fix by restructuring the switch |
| A2 | Per-variant runner time is 0.5 to 0.9 s on the dev host and 2 to 3 times that on CI | 3.1, 5.8 | The gate budget is exceeded; mitigate with mini-samples and fewer full-sample controls |
| A3 | `flat5` all-5 costs and every dummy-cycle address are acceptable placeholders (nothing asserts them) | 4.4, 6 | A later phase re-baselines the golden |
| A4 | S = `$FD` and P reads `$34` at power-on | 4.4 | Hand-derived fixture values change; no hardware claim is affected |
| A5 | The SST generator's decimal and invalid-BCD behaviour is a software model, not Lynx | 6.5 | Already labelled unverified |
| A6 | Full lane wall time is about 1 to 2 minutes of download plus 10 to 30 s of execution | 5.1, 5.8 | Only affects when it is scheduled |
| A7 | NMI hijack and taken-branch IRQ delay are not modelled | 4.4 | Unverified list covers it |
| A8 | The CMD G65SC02 table (`$x7/$xF` as 1 byte, 1 cycle) is the datasheet alternative for lynx1 | 4.3 | Only if the planner chooses the lens-1 alternative (OQ1) |

## Open Questions (each with a recommendation; the planner marks them RESOLVED)

1. **OQ1: lynx1 `$x7/$xF` shape.** CONTEXT says follow the source variant; lens 1 preferred 1-byte
   NOPs from the CMD table and lens 4 hand-decoded the tracer assuming 1 byte. Recommendation:
   follow CONTEXT (synertek vector shapes: `$x7` 2-byte, `$xF` 3-byte), flag `F_UNV_SHAPE`, tier
   `state`, reason `shape-unsourced-for-lynx1`. Test 7 cannot tell the difference. Switching later is a
   table edit plus one inventory row change.
2. **OQ2: one or two new registers.** CONTEXT adds `LYNX_REG_CPU_STATE` only. One 32-bit value
   cannot carry the latches, step, opcode, `ea`, `tmp`, ready delta and cost model. Recommendation: two
   ids (`CPU_STATE`, `CPU_LATCH`), `LYNX_REG_COUNT = 8`, so `det.sensitivity_state` can flip every
   field through the public API.
3. **OQ3: power-on values.** Recommendation: A=X=Y=0, S=`$FD`, P reads `$34`, all latches clear; mark unverified.
4. **OQ4: `tier` label.** The gate asserts identical classes for `full` and `state`; the tier only
   selects `--trace` comparability and claim wording. Recommendation: keep it (cheap, CONTEXT
   asks for it), and state this in the inventory header so it is not mistaken for a stronger
   assertion.
5. **OQ5: UNDEFINED versus in-flight time.** Recommendation: the rule in 4.5 (advance to
   `cpu_ready_at`, then stop; BUDGET wins when `guest_ticks == target`).
6. **OQ6: poison.** Recommendation: address-hashed, formula in 5.4; keep the formula in the
   inventory header (`poison = hash167`) so a change is reviewed.
7. **OQ7: table argument versus profile index.** Recommendation: pass `const lynx__cpu_decode *` to
   `complete` (needed for the mutant control without core changes).
8. **OQ8: sample sections.** Recommendation: freeze the container header in 5.2 and the grammar in
   5.3 in the plan, including `lines L` per section.
9. **OQ9: CI trigger for the full lane.** Recommendation: none in Phase 2; measure once at
   verification (`time sh scripts/vectors-full.sh run`), record, decide at the CI efficiency review.
10. **OQ10: directed edge vectors.** The sample contains no `JMP ($xxFF)`. Recommendation: author
    them (5.7) rather than increasing N; the full lane finds sample escapes.
11. **OQ11: `api.seam_agreement` size.** Recommendation: one vector per opcode per profile (512
    restores of 64 KiB, well under 1 s).
12. **OQ12: reset sequence stack cycles.** Recommendation: reads, marked chosen and unverified.
13. **OQ13: where the ca65 compare runs.** Recommendation: the assembly happens in a new `check.sh`
    step after `probe` (needs the pinned toolchain, like `toolchain.probe`); the CTest compare reads
    `build/toolchain/*.bin`, and a missing file prints "run scripts/check.sh" like `toolchain_probe.cmake`.
14. **OQ14: `$5C` cycles.** Sheets say 8, vectors say 4. Recommendation: 4 (vectors) with `state` tier and
    `cycles-conflict-sources`; nothing asserts it.

## Flagged Risks

| # | Risk | Source | Recommendation |
|---|------|--------|----------------|
| R1 | Local gate already 47 s on this host (CI 27 s); Phase 2 adds 4 to 6 s | 3.1 | Cheap controls on mini-samples; measure after each plan; the `hygiene` self-tests (19 s) are the critical path and a CI-efficiency item |
| R2 | Lens 3 proposed a constant poison; it makes the reverse wrong-profile control vacuous for 8 of 16 `$xF` opcodes | 5.4, measured | Address-hashed poison, a parse of `--results` in the control |
| R3 | Contradiction between sources on lynx1 `$x7/$xF`: CMD says 1 byte, vectors say 2 and 3; lens 4's tracer example assumed 1 byte (PC `$0205`), CONTEXT's rule gives `$0207` | 4.3, 7.3 | OQ1 |
| R4 | CONTEXT asks for one register; the invariant needs two | 4.6 | OQ2 |
| R5 | CONTEXT: "no committed scripts in the check path"; the ca65 gate and `.cfg` are committed and a `check.sh` step runs ca65 | 7 | Interpreted as "no generator/Python/Node in the check path"; the assembly is a `check.sh` step plus a compare, like the existing probe |
| R6 | CONTEXT's "about 12 bytes" of snapshot state is 17 bytes plus `guest_ticks`, `cpu_ready_at` | 4.6 | Payload 65,568; documented |
| R7 | Every plan touching `fast.txt` makes `docs.claim_template` fail until the claim hash is refreshed | 9.3 | Same-commit refresh rule |
| R8 | Linux-only failure classes (jump table in `.data.rel.ro`, shellcheck 0.9.0, gcc `-Wconversion`, cmake 3.31 `IN_LIST`) | Pitfalls | Run the `ubuntu:24.04` container before pushing (Docker client is installed here; daemon state not checked) |
| R9 | The sample does not reach rare edges (`JMP ($xxFF)` appears 0 times; 1/256 incidence is caught with about 12% probability) | 4.3, 5.7 | Directed authored vectors; mandatory full lane |
| R10 | `LYNX_STOP_UNDEFINED` plus S2 change a documented Phase 1 contract (consumed may be below budget only for stops; mid-instruction BUDGET stops) | 4.5 | Rewrite the header text; slicing-independence tests; CONTEXT already resolved this |
| R11 | Hardware-informed evidence is thin: only CLI latency, `$03` fetch, BBR/BBS 5/6/7, Test 7/8 shapes | CONTEXT | Everything else labelled unverified; no profile claim |
| R12 | The sample commit adds 1.85 MiB to history and a one-off cost to every `hygiene --history` | 3.1 | Accepted; note in the CI efficiency review; each pin bump adds another |
| R13 | `-O0` ASan sha256 of 3.2 MB costs 0.23 s per invocation; 12 invocations on CI may add 3 to 4 s | 3.1 | Mini-samples for controls, one hash per process |
| R14 | An operation list with 70 kinds and 20 sequence groups is a large `switch`; a typo in one row is caught only by vectors | 4.2 | The vectors cover all 256 rows; the mutant control proves attribution |

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| cc65 pinned build (`ca65`, `ld65`) | fixture assembly gate | yes (cached) | commit `eebf54af...` | `scripts/check.sh` builds it (9 s on CI) |
| cmake, ninja | build and tests | yes | 4.4.3, 1.13.2 (CI 3.31.6) | none needed |
| shellcheck | `check.sh` step | yes | 0.11.0 local (CI 0.9.0) | container |
| Apple clang | local build | yes | 21.0.0 | CI uses gcc 13 |
| Docker client | Linux container run | client present (daemon not checked) | 29.5.2 | CI |
| curl, gh | full lane, CI inspection | yes | curl 8.7.1 | none |
| Python 3 | scratch research only | yes | 3.14.4 | not used in the repository |

Missing dependencies with no fallback: none.

## Sources

### Primary (HIGH confidence)
- This repository, read this session: `include/lynxplosion.h`, `core/internal.h`, `core/machine.c`,
  `core/snapshot.c`, `runner/main.c`, `runner/bundle.c`, `tests/api/test_api.c`, `tests/det/*`,
  `tests/cmake/*`, `tests/gates/*`, `scripts/check.sh`, `scripts/validate-kv.sh`,
  `scripts/check-manifests.sh`, `scripts/check-claims.sh`, `CMakeLists.txt`, `CMakePresets.json`,
  `pins.txt`, `.github/workflows/ci.yml`, `docs/*`, `fixtures/*`.
- SingleStepTests/65x02 at commit `2f6980a2d95757486c7bee24355c360e40e2a224`: LICENSE, the
  synertek README, and the vector data (512 prefix fetches and two whole files), read as data only;
  the generator was not opened.
- GitHub API and `gh run list/view` for the pin's HEAD and the CI timings.
- The pinned cc65 `ca65` and `ld65`, run locally on the two fixture programs.

### Secondary (MEDIUM)
- The four discuss lenses (`discuss/01..04`), which cite the WDC W65C02S, Rockwell R65C02, CMD
  G65SC02 datasheets, the Epyx Lynx specification pages, lynx-tests (MIT) and the nesdev interrupt
  page; their claims are [CITED: lens N] above and were not re-read from the datasheets in this session.

### Tertiary (LOW)
- Runtime estimates for CI and for the full lane (inferred).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, nothing new is added and the pins were rechecked.
- Architecture: HIGH for the CPU/bus/budget design (consistent with CONTEXT, checked against the
  existing code), MEDIUM for the exact register split (OQ2).
- Vector runner and data layouts: HIGH, measured on the pinned data.
- Decimal, bus sequences and flag semantics: HIGH against the vectors, LOW against real Lynx
  hardware (unverified by design).
- Pitfalls: MEDIUM (Linux-only items are inferred).

**Research date:** 2026-11-09
**Valid until:** until the SingleStepTests pin or the CONTEXT decisions change (the upstream repository was last pushed 2025-05-10).
