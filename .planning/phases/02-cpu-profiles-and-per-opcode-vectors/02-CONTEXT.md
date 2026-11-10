# Phase 2: CPU Profiles and Per-Opcode Vectors - Context

**Gathered:** 2026-11-09
**Status:** Ready for planning
**Method:** four parallel research lenses (CPU semantics, bus-cycle interface, vectors and
verification, API/snapshot/determinism), each from primary sources with no emulator source read
(clean-room draft). Full notes are kept in `discuss/01..04-*.md`. Conflicts between lenses were
resolved by the orchestrator and are marked **[resolved]** below. Per AUTONOMY.md the
recommendations are auto-followed; no owner decision (O1, O5, O6, O9) is taken here.

<domain>
## Phase Boundary

The 65C02-class CPU executes correctly under data-defined `lynx1` and `lynx2` profiles (plus the
default `none`), proven by pinned per-opcode functional vectors with an exact per-profile
inventory, without either profile adopted as baseline (O1 stays open; claims name profile `none`).
Every CPU memory access is a tick-stamped bus cycle so per-access cost can vary. Covers CPU-01..05.
Replaces the Phase 1 provisional skeleton step. Out of scope: memory map and MAPCTL (Phase 3),
timers and real IRQ sources (Phase 4), cartridge, boot, Suzy (later), any suite-pass claim.

</domain>

<decisions>
## Implementation Decisions

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

### Amendments from research (2026-11-09, orchestrator-accepted; refine CONTEXT estimates, no decision reopened)
- Registers: one 32-bit register cannot expose all CPU state, so add `LYNX_REG_CPU_LATCH` beside `LYNX_REG_CPU_STATE` (`LYNX_REG_COUNT` becomes 8) to keep "every snapshot field is readable through the public API".
- Snapshot format 2 carries about 17 bytes of new CPU state plus two 8-byte tick fields (the "about 12 bytes" above was an estimate); payload becomes 65,568 bytes. Layout stays Claude's discretion.
- "No committed scripts in the check path" means no generator or converter scripts. The ca65 fixture gate adds one `check.sh` step and a committed linker `.cfg` that work like the existing toolchain probe.
- Unlisted-read poison in the vector runner is address-hashed (`(a*167+13)>>3`, at least 12 distinguishing vectors per opcode), not a constant: a constant makes the reverse wrong-profile control pass vacuously for 8 of the 16 `$xF` opcodes. `final.ram` can list addresses absent from `initial.ram`, so the no-write check uses the union of both lists.
- Under the exclusion/tier rules the retained tracer diverges as lynx1 ends at `$0207` and lynx2 at `$0234` (research section 7.3; an earlier draft's `$0205` was wrong).
- Every plan that edits `tests/inventory/fast.txt` refreshes `docs/claims/skeleton-repeatability.claim`'s `inventory_sha256` in the same commit (the claim gate fails until it does).
- Decimal-mode ADC/SBC follow the algorithms verified in research section 6.5 (0 mismatches over 1,152 sampled vectors); do not use the common compact SBC formula (about 6% mismatch).
- lynx1 `$x7/$xF` shape follows the synertek vectors (2 and 3 bytes) per the exclusion rule and is marked unverified; the CMD datasheet says 1 byte.
- Plan sequence from research section 9.3 is the starting point (core and tables; vector runner, sample, inventories, controls, full lane; scenario vectors; fixtures plus the ca65 gate; machine switch with the Phase 1 re-baseline and golden; public-path tests; docs and verification).

### Amendments from plan check, iteration 1 (2026-11-09, orchestrator decisions)
- **Profile `none` is vector-checked (ROADMAP criterion 1 says every defined profile).** Add a `none` run and inventory `tests/inventory/vectors/none.txt` (test `cpu.vectors.none`): the 224 opcodes whose lynx1 and lynx2 rows are identical are defined under `none`; 222 run and pass (`$CB` and `$DB` are excluded by the table rule exactly as for the Lynx profiles) using the same sample data as lynx1; the 32 `$x7/$xF` bit-op opcodes are reported with the new closed-list reason `profile-none-stops-undefined` and are additionally asserted to stop with UNDEFINED, consuming 0 and changing nothing. The runner accepts `--profile none|lynx1|lynx2`.
- Shapes refined by the plans and accepted: the decode row adds a `vec` field (needed by the exclusion rule); cycle kinds are 11 (stack split) rather than 10; `lynx__cpu_complete` takes a profile-table argument.
- "178 shared opcodes" is corrected to 224 shared opcodes (research section 6.3, verified). The variant-diff spike is answered by the cross-pairing failing sets in the full lane (with identical shared rows and passing same-profile runs, a cross pair can fail only the 32 bit-op opcodes); record that caveat in the verification report.
- The awk mini-sample cutter used by a control in the check path cuts a committed text sample by file headers; it does not convert data. "No converter scripts in the check path" is read accordingly (research R5).
- Over-budget plan estimates (02-01 about 1.1x, 02-09 about 1.3x, both uncalibrated with sample_count 0) are accepted: executors get a fresh context, 02-09 is atomic by design (golden and claim re-baselined once), and an executor that nears its limit splits at a task commit boundary and records it.
- CPU-03's "the `<=` mutation fails api.advance_budget" is one-time evidence recorded in the SUMMARY (a persistent core mutant is not added); the constant-cost fault control is persisted as a `.self_test`.
- Pre-authorization record for this phase: the owner directed that the repository be public and pushed with `gh` (condition: no PII or credentials; recorded in `.planning/AUTONOMY.md` as the 2026-11-09 decision) and delegated autonomous operation, under which approval of a phase includes its planned implementation and required verification (AGENTS.md workflow rule; AGENTS.md itself does not authorize publication, the owner decisions do); plan 02-03 downloads per-file vector data over https and plan 02-11 downloads about 2 GB once for the full lane and pushes to origin main to watch CI. The reproducibility step uses one invocation with a shared work directory (no repeated 2 GB downloads).

### Claude's Discretion
- File and function names inside `core/` (e.g. `cpu.c`, `cpu.h`, `profile.def`, `bus.c`), the exact
  `cpu_state` bit assignments and snapshot byte layout, test names, the exact column layout of the
  inventory header, the sample-container format details, and the declared power-on value of S and
  reset register values (mark unverified).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- Phase 1 gates and conventions (read `01-*-SUMMARY.md`): to add a gate add one
  `tests/gates/<gate>.cmake` (real test + `*.self_test` via `expect_fail.cmake`), one include line
  and sorted rows in `tests/inventory/fast.txt`; `scripts/check.sh` steps; the fetch-pin pin-zone
  lists; the rights/PII scans; fixture manifests; the claim template bound to the inventory hash
  (`sh scripts/check-claims.sh --inventory-sha256` after the inventory settles).
- Core API surface in `include/lynxplosion.h`, `core/machine.c`, `core/snapshot.c`; tests include
  `core/internal.h`; shared helpers in `tests/api/test_api.c`; `det.*` gates and the runner bundle.
- CI portability lessons (`.planning/LESSONS.md`): any `-P` CMake script using `IN_LIST` needs
  `cmake_policy(VERSION 3.25)`; shellcheck 0.9.0 on CI; findings printed with `message(STATUS)`.

### Established Patterns
- Const integer tables only in the core; no writable globals, libc beyond `mem*`, no float, no
  host imports (enforced by `core.*` gates on an uninstrumented twin).
- Whole-buffer `memcmp` refusal oracle; negative control for every gate; exact-inventory; text-only
  bundles; golden is the build-independent observation digest.

### Integration Points
- `lynx_config.profile` (id), `lynx_advance`, `lynx_read_register`, snapshot payload/apply, runner
  CLI and manifest, the bundle observation digest, `tests/inventory`, `docs/claims`, `pins.txt`
  untouched (sample is committed), `scripts/validate-kv.sh`, the exposure record and provenance.

</code_context>

<specifics>
## Specific Ideas

- Exposure record additions (clean-room draft): the research read the MIT lynx-tests
  `lynx-page-mode.md` prose, which describes how an emulator models page mode and links to that
  emulator's source; the links were not followed. Also read: WDC W65C02S, Rockwell R65C02 and CMD
  G65SC02 datasheets, the Epyx Lynx spec chapters, the ca65 manual, the SingleStepTests READMEs and
  LICENSE and sampled vector data (not the generator), the nesdev interrupts page and a design
  article on cycle-stepped 6502 CPUs (prose only). No emulator source was opened.
- The `none` profile is the honest default: it makes repeatability claims possible while O1 is
  open without a hidden baseline.
- Verification is automated by default (AUTONOMY.md): the full vector lane and the profile-diff
  spike run as throwaway verification at phase verification.

</specifics>

<deferred>
## Deferred Ideas

- Real per-access costs (page mode, Suzy 9-15 ticks, audio DPRAM, cart read and window timing):
  Phase 3-4 data refinements through the cost-model tables, then re-baseline the golden.
- Sample point inside a cycle, Lynx dummy-cycle side effects (RMW on Suzy, RCART0 double read),
  NMI latency, polling when a stall falls inside an instruction: Phases 4-7.
- CPU-mapped memory view and MAPCTL (Phase 3); real IRQ sources (Phase 4); a CI trigger for the
  full vector lane (after measuring); any profile-naming claim (needs O1).
- Owner follow-ups: committing about 6.4 MB of MIT-licensed third-party test data with its notice
  is auto-followed under the rights policy (licence documented, manifested); the fallback is a
  range-request fetch of the first bytes of each file at check time.

</deferred>
