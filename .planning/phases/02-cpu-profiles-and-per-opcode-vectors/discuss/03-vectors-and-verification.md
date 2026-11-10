# Phase 2 discuss, lens 3: vector sourcing, supply chain, verification and negative controls

Date: 2026-11-09. Lens: verification / DevOps / security-licensing.
Scope: CPU-01, CPU-05 and ROADMAP Phase 2 success criterion 1. This lens touches
CPU-03 (bus interface) and CPU-04 (reserved opcodes, WAI/STP) only where vector
data constrains them. It does not decide O1 (baseline model).

Labels: [SO] source observation (URL, commit, file) or a measurement made here;
[INF] inference; [UNK] unknown; [REC] recommendation. Owner decisions are quoted
from AUTONOMY.md / CONTEXT files and marked "owner".

Clean-room note: only SingleStepTests repository metadata, README files, LICENSE
and vector DATA were read, plus lynx-tests (MIT, allowed test source) `cpu/tests.s`
and `cpu/main.c` from the existing pin. The vector generator (an emulator) was not
opened. No GPL emulator source was opened. Klaus Dormann's GPL-3.0 tests are
mentioned only as prior art. All downloads went to a temporary scratch directory
outside the repository. That directory was deleted afterwards.

---------------------------------------------------------------------------

## 1. Measured facts (2026-11-09)

Repository: https://github.com/SingleStepTests/65x02

- [SO] Licence MIT, "Copyright (c) 2024 Thomas Harte et al" (LICENSE at the pin).
- [SO] HEAD of `main` = `2f6980a2d95757486c7bee24355c360e40e2a224`, committed
  2025-05-10 ("Merge pull request #17"). `pushed_at` 2025-05-10T14:49:00Z. The
  previous commit is b9cfa8b ("Clarified expectations regarding `ram` accesses").
  The repository looks finished and quiet: the last push was 18 months ago.
- [SO] GitHub API `size` 997,358 KB (packed git). Top-level directories: `6502`,
  `nes6502`, `rockwell65c02`, `synertek65c02`, `wdc65c02`. Each holds `README.md`
  and `v1/`. There is no `65c02` directory. STACK.md called it "`65c02`-family";
  the real names are the three above.
- [SO] Per variant, `v1/` has 256 files named `<op>.json`:
  - synertek65c02: 998,787,850 bytes in total.
  - rockwell65c02: 1,007,859,848 bytes.
  - wdc65c02: 1,000,495,993 bytes. `cb.json` and `db.json` are 0 bytes, because
    WAI and STP are "omitted here as they're incompatible with this style of
    testing" (wdc README).
  - File sizes run from 3.0 MB (`03.json`) to 5.4 MB (`00.json`). Each file holds
    10,000 vectors.
- [SO] Variant READMEs at the pin:
  - synertek65c02: "Sometimes called the 65SC02". It adds PHX/PLX/PHY/PLY, STZ,
    TRB, TSB and (zp) to the 6502. It has no RMB/SMB/BBR/BBS and no WAI/STP.
  - rockwell65c02: the same set plus BBR, BBS, RMB and SMB. It has no WAI/STP.
  - wdc65c02: the Rockwell set plus STP/WAI, which are omitted from the data.
- [SO] Methodology (top-level README): the vectors come from "an implementation
  that conforms to all available documentation, official and third-party; passes
  all other published test sets; and has been verified by usage in an emulated
  machine". This is software-model evidence, not a hardware capture.
- [SO] Vector schema (variant READMEs and data): `name` (string, "can be
  ignored"), `initial` and `final`, and `cycles`.
  - `initial` and `final` each hold `pc`, `s`, `a`, `x`, `y`, `p` and `ram`. `ram`
    is a list of `[address, value]`.
  - `cycles` is a list of `[address, value, "read"|"write"]`.
  - The READMEs say: "All tests assume the entire 64KiB memory space is RAM", and
    "Any memory address not included in a test's `ram` lists must not be accessed
    during that test".
  - The data contains no interrupt, reset, tick or timing field beyond the
    per-cycle bus list.
- [SO measured] The data uses two physical layouts. Both put one JSON object per
  line, but whitespace differs:
  - Standard layout: a first line that is just `[`, then `{ "name": ...},` lines,
    then `}]` at the end. That is 10,001 lines, with spaces after `{` and `[`.
  - Regenerated layout: the first line starts `[{ "name": "e9 1", ...`, there is
    no space after `{"`, and names are `"<op> <ordinal>"`. That is 9,999 lines.
  - Regenerated files: the SBC files `e1 e5 e9 ed f1 f2 f5 f9 fd` in BOTH variants,
    plus the rockwell BBR/BBS files `0f 1f ... ff` (all 16 `xF`). This was found
    by an 80-byte HTTP range read of the head of every file in both variants.
- [SO measured] The P register convention is not consistent across files:
  - Standard-layout files: every `p` has bit 5 set and bit 4 (B) clear. This was
    checked on all 20,000 values each of `a9`, `58` and `00` (synertek/rockwell).
  - Regenerated files: every `p` has bit 4 SET. This was checked on all 20,000
    values each of rockwell `0f`, `8f`, `e9`, `ff` and synertek `e9`.
  - PLP (`synertek 28.json`): the pulled stack byte has a random B in 4,817 of
    10,000 vectors and bit 5 clear in 5,109. Final `p` always has B=0 and bit 5=1.
    So in standard files the generator forces B=0 and bit 5=1, while regenerated
    files carry B=1.
  - [INF] A CPU with one consistent P model cannot match the full P byte in both
    file families. B and bit 5 are not physical flags; they are visible only in
    pushed stack bytes.
- [SO measured] The bus traces contain generator-specific dummy reads:
  - Decimal-mode ADC #imm (`69.json`) adds an extra read of zero-page `$0056` in
    both D=1 vectors inspected.
  - Decimal-mode SBC #imm (`e9.json`, regenerated) adds an extra read of `$0000`.
  - [INF] These addresses differ between two related instructions, so they come
    from the generator's own choices, not from hardware.
- [SO measured] Per-opcode behaviour that matters for the profiles (first vector,
  plus checks over all 10,000):
  - synertek `07` and `87` (RMB0/SMB0 slots): 2-byte NOP with a zero-page read and
    3 bus cycles. In all 10,000 vectors no memory changes and PC moves by 2.
  - synertek `0f` and `8f` (BBR0/BBS0 slots): 3-byte NOP, 3 cycles, PC +3 in all
    10,000 vectors.
  - rockwell `07`: RMB0. Memory changes in 4,988 of 10,000 vectors (16 of the
    first 32): when bit 0 was already clear, RMB leaves memory unchanged.
  - rockwell `0f`: BBR0. PC moves by 3 (not taken) in 5,024 of 10,000 vectors and
    13 of the first 32 branch.
  - synertek and rockwell `cb`: 1-byte NOP, 2 cycles. `db`: 2-byte NOP, 4 cycles
    (zp,X read).
  - `03` and `0b`: 1-byte NOP, 1 cycle. `02`: 2-byte NOP, 2 cycles. `44`: 2-byte
    NOP, 3 cycles.
  - `5c`, `dc` and `fc`: 3-byte NOP, 4 cycles, in both variants. The WDC
    documentation gives `5c` 8 cycles. Cycles are not asserted here (section 2.3).
- [SO measured] Decimal coverage in a first-N sample (D flag in `initial.p`):
  - `69`: 21 of the first 32 (synertek), 18 of 32 (rockwell), about 4,991 and 5,050
    of 10,000.
  - `e9`: 19 of 32 (synertek) and 15 of 32 (rockwell).
  - Upstream file order is random, so a first-N prefix is an unbiased random sample.
- [SO] lynx-tests at the pin `b583dc4` (`cpu/tests.s` lines 633-720,
  `cpu/main.c` `k_expected_results`): Test 7 places `$EA` (NOP) as the operand of
  `$07`/`$87` and `$18` (CLC) as the BBR/BBS offset. The results are the same
  whether Lynx I treats `$x7`/`$xF` as a 1-, 2- or (for BBx) 3-byte NOP.
  [INF] So lynx-tests does NOT establish the Lynx I NOP length for these opcodes.
- [SO measured] raw.githubusercontent.com serves files at a commit SHA and honours
  HTTP Range (206). A plain download of one 4.7 MB file took 0.9 s on the dev host
  (about 5 MB/s per stream); `--compressed` was slower (3.3 s). The full commit
  archive would contain all five variants (about 5 GB unpacked, roughly 1 GB
  compressed going by the packed-git size).
- [SO measured] Cost probes, on the dev host:
  - C tokenizer: a throwaway C tokenizer read 190 MB (50 files, 500,000 objects)
    in 0.20 s at -O2 and 0.27 s under ASan+UBSan.
  - awk converter: a throwaway awk JSON-to-line converter took about 0.5 s per
    10,000-vector file, about 2 minutes per variant.
  - `scripts/scan-binaries.sh --source` passed a 5.7 MB verbatim-JSON sample in a
    temporary git repo in 0.42 s, with no false positive (section 4.3).
  - `scripts/hygiene.sh --tree` passed the same sample in 1.16 s, and `--history`
    in 1.27 s.
- [SO measured] Sample size: the first 32 objects average about 12.2 KB per file
  over the 50 files probed. From the upstream average of about 390 bytes per
  vector, 2 variants x 256 files x 32 vectors comes to about 6.4 MB of verbatim
  JSON. gzip reduces JSON to about 18% (`69.json`: 640,557 of 3,486,422 bytes), so
  this is roughly 1.2 MB packed in git. The current `.git` is 4.3 MB.
- [SO measured, lesson] My own first `grep` for `"initial": {"pc"` matched 0 of
  10,000 lines in a standard-layout file, because those files write `{ "pc"`. A
  line- or regex-based extractor that returns zero silently is exactly the
  vacuous-pass fault in section 7.

---------------------------------------------------------------------------

## 2. (a) Directory-to-profile mapping, assertions and exclusions

### 2.1 Mapping

[REC] The **Lynx I-style** profile (65SC02-class, no RMB/SMB/BBx) uses
`synertek65c02/v1`. The **Lynx II-style** profile (Rockwell bit operations) uses
`rockwell65c02/v1`. One source directory per profile, stated in the inventory
header.

- Basis: [SO] the README definitions above match the profile definitions in
  REQUIREMENTS CPU-02 and SUMMARY ("RMB, SMB, BBR and BBS act as NOPs on Lynx I
  and execute on Lynx II", from lynx-tests Test 7 expectations).
- Not used:
  - `wdc65c02`: it adds WAI/STP (their data is empty) and is otherwise the
    Rockwell set. Nothing indicates that a Lynx is WDC.
  - `6502` and `nes6502`: the NMOS instruction set, wrong for this core.
- Caveats [INF/UNK]:
  - Neither variant is a Lynx. They are the closest documented models.
  - Lynx II's RMB/SMB/BBR/BBS are hardware-corroborated for bit 0 only (lynx-tests
    Test 7). Rockwell vectors for bits 1-7 are inferred to apply (PITFALLS P6:
    "do not extrapolate from bit 0").
  - On Lynx I, SST Synertek models `$x7` as a 2-byte NOP and `$xF` as a 3-byte NOP.
    Lynx I's real lengths are [UNK], since lynx-tests is length-agnostic (section 1).
  - If the CPU-semantics lens adopts a different Lynx I NOP length, those 32 rows
    become reasoned exclusions mechanically (section 2.4). The data does not
    decide the CPU semantics.
- O1 is untouched: both profiles run, and neither is named baseline.

### 2.2 What a vector asserts and what we assert

The upstream data asserts:

- final PC, S, A, X, Y and P;
- final values at the listed RAM addresses;
- the per-cycle bus list (address, value, direction), and with it the cycle count;
- implicitly, "no access outside the listed addresses".

[REC] What Lynxplosion gates on (assertion classes, named in every inventory header):

1. `regs`: final PC, S, A, X and Y exactly. P is compared under mask `0xCF`, so
   B (bit 4) and bit 5 are ignored. Reason: the P convention measured in section 1
   differs between file families, and B and bit 5 are not flag state. Their
   architectural effect, the byte pushed by PHP/BRK, is still asserted exactly
   through RAM.
2. `ram`: the final value at every address listed in `final.ram`.
3. `nowrite`: no bus write to an address outside the union of the `initial` and
   `final` RAM lists. Without it, a stray write would pass the `ram` class
   unseen.

[REC] Not asserted (gated off, stated in the header as `not_asserted`):

- Cycle count and bus trace. Their dummy-read addresses are generator choices
  (`$0056` versus `$0000` for decimal ADC versus SBC). The Lynx core's
  microsequence is unsourced; decimal-mode and reserved-opcode cycle counts are
  "unsourced" (SUMMARY item 7). Tick cost and page mode are a separate dimension
  that the data never encodes.
- Unlisted reads. The `$0056`/`$0000` dummy reads would make a "no unlisted
  access" rule test the generator's microsequence. The test bus answers an
  unlisted read with a fixed poison value, so a CPU that actually uses such a
  read still fails `regs`/`ram`.

Correction to earlier research [INF]:

- STACK/SUMMARY said the traces "assume standard 65C02 timing, which Lynx page-mode
  does not share". In fact the trace lists one bus access per CPU cycle. Page mode
  changes the tick cost of an access, not the access sequence.
- The real reason not to gate on the trace is the one above: it encodes an
  unverified microsequence, including generator artefacts.
- [REC] The runner offers a diagnostic `--trace` mode that compares accesses and
  counts and prints the first difference. It is never part of the gate or of any
  claim.
- The per-opcode access-count agreement may be printed as information. It is
  never "passed".

Not covered at all by single-step data:

- IRQ/NMI/RESET entry, CLI/SEI latency, WAI/STP as real instructions, and any
  Lynx memory map (the vectors run on a flat 64 KB test bus).
- The inventory header says so (`not_covered`), so nobody infers CPU-04 or MEM
  coverage from these rows.
- CPU-04 needs project-authored tests that cite sources (CPU lens).

### 2.3 Data-sanity rules (fail closed; all data errors, never "skip")

- [REC] For every vector, `initial.ram` holds the PC address, and its value equals
  the file's opcode. The `name` string's first token also equals the opcode.
  - Cost: none. It catches a misplaced or renamed file and a wrong directory more
    cheaply than any semantic check.
- [REC] Reject the data if:
  - an `initial.ram` address appears twice;
  - any value is out of range (address or PC above 65535, byte above 255);
  - a number has a sign, a fraction, an exponent or leading zeros;
  - a key is unknown, missing or duplicated;
  - a `ram` list has more than 64 entries (the largest seen is 9, for BRK);
  - a `cycles` direction is not `read` or `write`.
- [REC] Excluded rows are still PARSED and COUNTED. Exclusion means "not asserted",
  never "not read". A corrupted excluded section still fails.

### 2.4 Exclusions: how each one becomes a visible, reasoned row

[REC] Exclusions are never a hand-written skip list in the runner. They are
derived from the profile table:

- A profile entry may carry `diverges_from_source = <reason code> <citation>`.
- The runner excludes exactly those opcodes from that profile's run.
- The committed expected inventory (section 2.5) must list the same rows with the
  same reason code. Otherwise the inventory check fails.
- Adding or removing a divergence therefore always shows in a reviewed diff of the
  expected inventory.

Closed vocabulary of reason codes (a code not in this list is a data error):

- `profile-diverges`: the profile defines behaviour the source variant does not
  model. Example: Lynx I `$x7`/`$xF` if the CPU lens chooses 1-byte NOPs;
  `$CB`/`$DB` if a profile gives them non-NOP semantics. The citation names the
  profile-table entry and its source, or "unverified".
- `source-empty`: the upstream file has no vectors (wdc `cb`/`db`; not reachable
  with the recommended mapping, kept for completeness).
- `untestable-single-step`: the instruction cannot complete in one step (WAI/STP
  as real instructions). Not reachable with Rockwell/Synertek data.

There is NO "known failure" or xfail status. A vector either passes or belongs to
an excluded row with a reason (xfail lists rot).

Expected exclusions under the recommended mapping, if the CPU lens adopts the SST
NOP models for `$x3/$xB/$x2/$44/$54/$D4/$F4/$5C/$DC/$FC/$CB/$DB` and, for Lynx I,
`$x7/$xF`:

- Zero rows excluded for either profile. All 256 rows are run, and every
  "unverified" expectation carries that label in the profile table (CPU-04: "each
  expected value cites its source or is marked unverified").
- [INF] Zero exclusions is a valid, honest outcome, provided the header states what
  is not asserted and not covered.

Decimal mode:

- [REC] Decimal-mode vectors are included in the 18 ADC/SBC files.
- The inventory reports a per-row `decimal` subcount, the vectors with D=1 in
  `initial.p`. A claim can then name them, or exclude them later, without
  re-running.
- The values they assert (V flag, results for invalid BCD digits) are Rockwell or
  Synertek software-model values, not Lynx evidence [UNK]. lynx-tests Test 3
  checks only valid-BCD results and C/Z/N.

### 2.5 Exact per-profile inventory format

[REC] Committed expected file `tests/inventory/vectors/<profile-id>.txt`:

- LC_ALL=C sorted rows, `#` header lines, tab-separated, in the `fast.txt` style.
- The runner writes the same "plan" section, plus a results section, to
  `build/check/vectors/<profile-id>.inventory` (text).
- A CTest test compares the plan byte-for-byte with the expected file (exit 4 on
  any difference) and checks the results section.

Example header:

    # vector inventory: op<TAB>source<TAB>status<TAB>vectors<TAB>decimal<TAB>reason
    # profile = lynx-ii-style
    # source = SingleStepTests/65x02 2f6980a2d95757486c7bee24355c360e40e2a224 rockwell65c02/v1
    # sample = first 32 vectors of each opcode file, upstream order, bytes verbatim
    # sample_sha256 = <sha256 of fixtures/vectors/sst65x02/rockwell65c02.sample>
    # asserted = regs(pc s a x y, p&0xcf) ram(final listed) nowrite(outside listed)
    # not_asserted = cycle count, bus trace, unlisted reads, ticks
    # not_covered = irq/nmi/reset entry, cli/sei latency, wai/stp, lynx memory map
    # evidence_tier = T1 (software-model vector expectation; not hardware)

Example rows and totals:

    00	rockwell65c02/v1/00.json	run	32	0	-
    07	rockwell65c02/v1/07.json	run	32	0	-
    69	rockwell65c02/v1/69.json	run	32	18	-
    ...
    # totals = files 256 run 256 excluded 0 vectors 8192 asserted 8192 excluded_vectors 0

Results section, written by the runner only and checked, never committed:

    # result	op	asserted	passed	failed	identity
    # result_totals = asserted 8192 passed 8192 failed 0 identity <n>

- `identity` counts vectors whose final state equals their initial state apart from
  PC; see the no-step control in section 5.
- Gate: every `run` row has `passed == vectors`. Every `excluded` row has
  `asserted == 0`, a non-empty reason code and a parsed count equal to its planned
  count. The totals line equals the sum of the rows.
- The totals must be non-zero, and every one of the 256 opcodes must appear exactly
  once.

---------------------------------------------------------------------------

## 3. (b) Size and cost strategy

Options compared:

| Option | Fast-gate cost | Network in gate | Honesty | Verdict |
|---|---|---|---|---|
| Full dataset in the gate (2 x 1 GB per run; the archive is about 5 GB unpacked) | download about 2 GB per CI run; rights scan of 2 GB in build/ (minutes) | yes, heavy | full | reject |
| Fetch a prefix per file at check time (512 HTTP Range requests, per-prefix SHA-256 pins) | about 3-10 s with parallel requests; flaky; new fetch path | yes, 512 requests | sample | fallback only |
| Commit a verbatim first-N sample, plus a full lane on explicit request | under 1 s run, about +1-2 s of scans | none | sample, labelled; full lane for the full claim | recommend |
| Commit a converted compact sample (2.5x denser) | same | none | needs a trusted converter; provenance is indirect | reject (section 4.4) |
| Edge-enriched (coverage-guided) selection | same | none | biased by our own model; needs a selection algorithm | defer; revisit only if the full lane finds sample escapes |

[REC] The fast gate runs a committed, verbatim, first-N sample with N = 32 per
opcode file, for both variants:

- Totals: 2 x 256 x 32 = 16,384 vectors, about 6.4 MB of text and about 1.2 MB
  packed.
- Cost: dominated by the two text scans (hygiene tree and rights scan, each about
  1 s per 6 MB measured), not by the runner. Parsing plus execution is well under
  1 s even under ASan (0.27 s for 190 MB of tokenizing).
- This stays proportionate to the 23-29 s CI budget. Measure again after it lands,
  per AGENTS.
- [INF] Detection power of N = 32:
  - a bug that affects 10% of an opcode's random vectors is caught with
    probability about 97% (1 - 0.9^32);
  - 1% incidence: about 28%;
  - 1/256 incidence: about 12%.
  - The full lane (10,000 per opcode) catches 1% incidence essentially always.
  - This is why the full lane is mandatory at phase verification and before any
    claim. The sample is a regression tripwire, not the proof.
- N = 64 doubles the size (12.8 MB) for 1%-incidence detection of about 47%. That
  is not worth it while the full lane exists.

[REC] Full lane: `scripts/vectors-full.sh`, POSIX sh, outside check.sh and outside
CTest (the no-network rule):

1. Downloads the 512 pinned files over https from
   `raw.githubusercontent.com/SingleStepTests/65x02/<commit>/<variant>/v1/<op>.json`
   into a mktemp directory OUTSIDE the repository and outside `build/`. The
   artifacts step would otherwise rights-scan 2 GB.
2. Verifies each file against a committed per-file SHA-256 list.
3. Re-derives the sample and `cmp`s it with the committed sample. This proves
   provenance byte-for-byte.
4. Runs the same runner in "complete" mode (10,000 per file; closing `]` required)
   against plan files with counts of 10,000.
5. Prints both per-profile result inventories with their SHA-256 values, then
   deletes the directory unless `--keep DIR` is given.

When to run it:

- Required in Phase 2 verification, on every pin bump, and before any CPU-profile
  claim.
- [REC] Measure it on the dev host and once on CI through `workflow_dispatch`
  before adding any trigger. Estimate: about 2 GB of downloads (minutes at 5 MB/s
  per stream; parallelise 8-16 ways) plus about 10-20 s of run.
- A path-filtered CI lane (CPU core files, `fixtures/vectors/**`, the runner) is
  justified only if it is measured and catches a failure class the sample misses
  (AGENTS: "add a CI check only when it catches a distinct recurring failure at a
  useful cost").

What claim each supports (wording in section 6):

- Sample: "agrees on the first 32 vectors of each of the 256 opcode files".
- Full lane: "agrees on all 10,000 vectors of each of the 256 files at commit X".
- Both: functional final state only, on a flat test bus, against a software model.

---------------------------------------------------------------------------

## 4. (c) Supply chain: acquisition, storage, rights scan, licence, parser

### 4.1 Pinning

[REC] Do NOT add a SingleStepTests row to `pins.txt`:

- The check's pins step downloads every row, and fetch-pin.sh handles only
  commit archives. That would mean about 1 GB compressed or 5 GB unpacked per run.

[REC] Pin record, under `fixtures/vectors/sst65x02/`:

- `upstream.sha256`, text:
  - a header naming the repository URL and the 40-hex commit;
  - then 512 lines of the form `<sha256> <variant>/v1/<op>.json`, LC_ALL=C sorted.
- It is produced once at pin time by `scripts/vectors-sample.sh --pin`.
- A CMake lint in the style of `pins.cmake`, with a self-test, checks:
  - https URL; 40-hex commit; 64-hex digests;
  - exactly 256 rows per used variant; paths matching
    `^(rockwell|synertek)65c02/v1/[0-9a-f]{2}\.json$`; no duplicates.
- A pin bump is a dedicated change that regenerates `upstream.sha256`, both samples,
  both manifests and both expected inventories. This mirrors the Phase 1 pin-bump
  rule.

FND-02 ("CPU vectors fetched from a pinned revision and verified by SHA-256"):

- The fetch happens in the pin, regeneration and full lanes, each verified per file.
- The fast gate uses the committed derived sample, whose identity is checked against
  its manifest (FND-03).
- [INF] That satisfies FND-02's intent. Say so in the verification report, so the
  word "fetched" is not read as "fetched by check.sh".

Download hardening (vectors-full.sh / vectors-sample.sh, adapted from
fetch-pin.sh's idioms):

- `curl -fsSL --proto '=https' --proto-redir '=https' --max-filesize 8000000`
  (no upstream file exceeds 5.5 MB);
- names validated as `[0-9a-f]{2}`;
- verify before use; no archive extraction, so no tar path-traversal surface;
- a temporary directory, with an EXIT trap that removes it;
- any mismatch fails the whole run, never one file.

### 4.2 Storage layout (all text)

- `fixtures/vectors/sst65x02/synertek65c02.sample` and `.../rockwell65c02.sample`.
  Container format v1:

      #sst65x02-sample v1
      #source https://github.com/SingleStepTests/65x02 2f6980a2d95757486c7bee24355c360e40e2a224 synertek65c02/v1
      #rule first 32 objects of each file; upstream lines 1..L verbatim
      #file 00.json upstream_sha256 <64hex> lines <L> prefix_sha256 <64hex>
      <upstream lines 1..L, byte-identical>
      #file 01.json ...

  - JSON lines never start with `#`, so the framing is unambiguous.
  - L is 33 for the standard layout and 32 for the regenerated one. The sampler
    computes L by counting lines with exactly one `"name"` key. It fails closed if
    any object spans lines or a line holds two objects, so an upstream reformat
    cannot be truncated silently.
- `fixtures/vectors/sst65x02/LICENSE`: the upstream MIT text, verbatim.
- `fixtures/vectors/sst65x02/upstream.sha256` (section 4.1).
- `.gitattributes`: `fixtures/vectors/sst65x02/* -text`, so no EOL conversion can
  change bytes and therefore SHA-256 on any client. Optionally add
  `linguist-generated`. Diffs stay visible.

### 4.3 How the rights and hygiene scans treat it

- [SO measured] The verbatim-JSON sample passes `scan-binaries.sh --source`:
  - It is ASCII with no control bytes and valid UTF-8, so it counts as text.
  - The hex-512 rule needs a file made only of hex digit pairs, whitespace and `#`
    comments. JSON brackets, commas and quotes rule that out.
  - The 200-character base64-run rule finds no run: numbers are separated by `, `
    and names contain spaces.
  - The 64-hex digests in headers are 64 characters, well under 200.
- [SO measured] It passes `hygiene.sh --tree` and `--history` (no email, home path
  or credential pattern).
- [REC] Nevertheless, add the sample files' SHA-256 to their manifests, as
  check-manifests requires anyway.
  - That also exempts them from the text-side rules by design, so a future rule
    change cannot break them silently.
  - The firmware intent is unaffected: they are not 512-byte non-text files.
- [REC] Keep full-lane downloads out of `build/`. If they ever land there, the
  artifacts step must still treat them as unlisted, never exempt them by path.
- Cost: one large commit makes every `hygiene.sh --history` run re-read about
  6.4 MB (about 1.3 s, measured). Each pin bump adds another such commit. Accept
  this, and note it in the CI efficiency review.

### 4.4 Licence and notices

- [REC] Two manifests: `fixtures/manifests/sst65x02-synertek65c02.manifest` and
  `...-rockwell65c02.manifest`. For rockwell:

      source = https://github.com/SingleStepTests/65x02 rockwell65c02/v1, first 32 vectors of each of 256 files, verbatim
      revision = 2f6980a2d95757486c7bee24355c360e40e2a224
      recipe = sh scripts/vectors-sample.sh 32 rockwell65c02 over files verified against fixtures/vectors/sst65x02/upstream.sha256
      sha256 = <sha256 of rockwell65c02.sample>
      licence = MIT, Copyright (c) 2024 Thomas Harte et al
      notices = fixtures/vectors/sst65x02/LICENSE must accompany any copy
      allowed_use = CPU functional test expectations (software model, T1); not hardware evidence
      startup_tier = none
      profile = lynx-ii-style (candidate; O1 open)
      evidence_tier = T1
      files = fixtures/vectors/sst65x02/rockwell65c02.sample fixtures/vectors/sst65x02/LICENSE fixtures/vectors/sst65x02/upstream.sha256

- [REC] `startup_tier = none`:
  - Vectors inject CPU state into a flat test bus. They are not startup tier A
    (direct RAM load into the Lynx machine with a preset state).
  - `validate-kv.sh` currently allows only `A`.
  - Add a caller-selected set: fixture manifests and claims accept `A none`; run
    manifests stay `A` only.
  - Add a negative control: `none` in a run manifest is refused.
  - Labelling the vectors `A` would be a false label.
- [REC] `docs/provenance.md`:
  - Add a section "Third-party data committed with notice", naming SingleStepTests
    (MIT), the commit, the files and the LICENSE path.
  - Add a row to `docs/policy/exposure-record.md`: "SingleStepTests/65x02: README,
    LICENSE and vector data read; generator implementation not read."
  - [REC] Implementers do not read the generator either, even though it is not GPL.
    Keeping it unread keeps the vectors an independent oracle, which is the point of
    the check.
- Rights posture [INF]:
  - MIT permits redistribution with the notice. The data is not firmware or game
    content and not a release.
  - STACK.md already recommended "commit only a reviewed sample".
  - This is routine under AUTONOMY: no owner stop is listed for permissive
    third-party test data with notices. The repository is public by owner decision.
  - If the owner prefers no third-party data in the repository, use the Range-prefix
    fetch fallback from section 3.

### 4.5 Parser strategy (no JSON dependency, no Python)

Options:

- (P1) An awk converter to a compact line format, plus a tiny C line parser.
  - Fails: the converter becomes trusted code. Its bugs, such as dropping a
    register or misreading the regenerated layout (exactly what my measurement grep
    did), would not show, because the full lane would re-run the same converter.
  - It is 2.5x denser, but provenance becomes "our transform of upstream" rather
    than "upstream bytes".
- (P2) A purpose-built, schema-strict C reader of the upstream JSON, used for both
  the verbatim sample and the full files. [REC]
  - One code path for both lanes.
  - The sample is byte-identical to upstream, so provenance is a `cmp`.
  - No converter has to be trusted.
- (P3) Copy-paste a general JSON tokenizer, such as jsmn (MIT).
  - It needs a notice and still needs a strict schema layer on top.
  - A purpose-built reader for this one schema is about the same size and fails
    closed more naturally.
  - Reject, per "another copy-paste is better than another dependency, and no
    copy-paste is better than either".

P2 shape [REC]:

- It lives with the test runner (for example `tests/cpu/sst_reader.c`), not in
  `core/`. It uses stdio, which the core must not, and runs in about 250 lines.
- Hand-written iterative scanner:
  - no recursion, no allocation (fixed-size vector struct: ram lists of at most 64
    entries, cycles of at most 16);
  - whitespace-insensitive and key-order-insensitive;
  - it rejects unknown, duplicate or missing keys, out-of-range and malformed
    numbers, oversize lists, trailing garbage and an object count different from the
    planned count.
- Modes: `prefix` (the sample container: an exact count N, then an optional `,`
  before the next `#file` or EOF) and `complete` (an exact count of 10,000 and a
  closing `]`).
- Failure modes and mitigations:
  - A parser bug that accepts too much: the malformed-input controls in section 5,
    and the full lane run once under ASan+UBSan.
  - A parser bug that drops vectors: the exact counts in the inventory.
  - A layout change upstream: it fails closed at regeneration, before anything is
    committed.
- Exit codes:
  - 0 all asserted vectors pass;
  - 1 assertion failure(s);
  - 2 usage error;
  - 3 data or parse error;
  - 4 inventory mismatch.
  - Distinct codes let the existing `tests/cmake/expect_fail.cmake` (CODE + REGEX)
    assert the right failure class in every control.
- Sample identity: before parsing, the runner computes the sample file's SHA-256
  with the core's own `sha256.c` and compares it with the manifest `sha256 =`
  value. On a mismatch it exits 3 and parses nothing.

---------------------------------------------------------------------------

## 5. (d) Negative controls (each one a CTest self-test in fast.txt)

All controls are offline. Fault inputs are generated at test time into the
per-test scratch directory, from the committed sample. Nothing faulty is
committed, the same trick Phase 1 uses for the no-network fixture.

1. **CPU-05, wrong profile (named fault)** `cpu.vectors.wrong_profile.self_test`
   - Run the Lynx II-style plan (rockwell sample) with the Lynx I-style profile
     selected.
   - Required: exit 1, and the set of opcodes with at least one failure equals
     EXACTLY the 32 bit-operation opcodes (`$x7`, `$xF`). Each of them has at
     least one failure, and no other opcode fails.
   - This proves detection and localization: the profiles differ only where the
     data says they do.
   - [SO measured] This is robust: in the first 32 vectors, 16 rockwell RMB0
     vectors change memory and 13 BBR0 vectors branch. That holds whatever Lynx I
     NOP length the CPU lens picks.
   - Do not pin an exact failure count: it depends on the NOP-length model, so it
     would be brittle with no added detection power.
2. **Reverse wrong profile** `cpu.vectors.wrong_profile_reverse.self_test`
   - Run the synertek plan with the Lynx II-style profile.
   - Required: the failing set is exactly the `$x7`/`$xF` opcodes. For each,
     at least one fails: about half for RMB/SMB, where the bit was set; most for
     BBx, where PC differs.
3. **No step (no-run)** `cpu.vectors.no_step.self_test`
   - Runner fault flag `--fault no-step`: load the state, do not call the CPU,
     compare.
   - Required: failures == asserted - identity. `identity` counts vectors whose
     expected final state equals the initial state including PC; for example BBR
     with offset -3 has a PC delta of 0, as seen in the rockwell `0f` data.
   - This proves that the comparison exercises the CPU, and catches a harness that
     copies `final` into the machine. The corrupted-expectation control cannot catch
     that.
4. **Corrupted expectation** `cpu.vectors.corrupt_expect.self_test`
   - Copy one opcode section and change one `final` value, for example `a` of
     vector 1 of `69`.
   - Required: exit 1 with exactly one failure, naming that opcode, ordinal and
     field. This proves that every asserted field is compared.
   - Variants, in one test or several: a changed `final.ram` value (`ram` class);
     an injected write outside the listed addresses (`nowrite` class, via a test-bus
     fault flag).
5. **Malformed data** `cpu.vectors.malformed.self_test`
   - Cases: truncated object; unknown key; duplicate key; missing `final`;
     `pc` 70000; value 256; a negative number; a vector whose PC byte is not the
     file opcode; an object split across lines in prefix mode.
   - Required: each case exits 3 with a message naming the file and the vector.
     None may yield "0 vectors, pass".
6. **Inventory drift** `cpu.vectors.inventory_drift.self_test`
   - Cases, each against a modified copy of the expected inventory or the sample:
     a section with one vector removed (count 31 versus 32); a duplicated `#file`
     section; a missing opcode row; an excluded row without a reason; a reason code
     outside the vocabulary; an exclusion in the profile table missing from the
     expected inventory; a stale exclusion in the expected file; a wrong totals line.
   - Required: each case exits 4 (or 3 for sample-side faults) with a specific
     message.
7. **Sample identity** `cpu.vectors.sample_identity.self_test`
   - A sample copy with one byte changed against the real manifest.
   - Required: exit 3 before any parsing ("sha256 mismatch").
8. **Mutant profile (data mutation)** `cpu.vectors.mutant_profile.self_test`
   - A test-only copy of the Lynx II-style profile table in which one opcode decodes
     to a wrong operation (for example `$69` mapped to the SBC handler). It is built
     in the test, never in `core/`.
   - Required: the failing set is exactly `{69}`.
   - Data-defined profiles (CPU-02) make this a free mutation test. It proves that
     a single wrong handler is caught and attributed. No `#ifdef FAULT` code goes
     into the core.
9. **Pin-list lint control** `policy.vectors_pin.self_test`
   - A broken `upstream.sha256` (http URL, 63-hex digest, 255 rows, bad path) is
     rejected.
10. **Startup-tier vocabulary control** (section 4.4)
    - `startup_tier = none` is accepted in a fixture manifest and refused in a run
      manifest.

Optional, CPU lens [REC]:

- `cpu.vectors.access_cost` runs the same sample on a test bus with a non-default
  per-access tick cost (for example odd addresses cost 7).
- It asserts identical `regs`/`ram` results and total ticks equal to the sum of the
  per-access costs, which differs from the default. That is ROADMAP Phase 2
  criterion 3 at almost no extra cost.

---------------------------------------------------------------------------

## 6. (e) Evidence tier, wording and claim binding

- Tier [REC]: **T1, software-model vector expectation**.
  - SUMMARY: "T1 spec or vector expectation"; FEATURES: "per-instruction vectors".
  - It is never T2 (hardware-informed test), never T5 (hardware) and never R.
  - The runner's same-core digests are not involved.
- Wording that avoids overclaiming [REC]. Inventory, verification report and any
  future claim say:

  > Under profile <id>, on a flat 64 KB test bus, the CPU's final registers (P
  > compared with bits 4-5 masked) and the listed RAM agree with the
  > SingleStepTests/65x02 <variant> software model for <n> of <n> asserted vectors
  > (<sample rule | all 10,000 per file>, commit 2f6980a). The vectors come from a
  > documentation-conforming implementation verified in an emulated machine, not
  > from hardware. This says nothing about cycle counts, bus microsequence, page
  > mode, tick timing, interrupts, the Lynx memory map, or which variant matches a
  > real Lynx I or Lynx II.

  Banned phrasings: "passes the 65C02 tests"; "cycle-accurate"; "Lynx II CPU
  verified"; "all opcodes verified on hardware".
- Claim files [REC]:
  - No `docs/claims/*.claim` naming a profile is written in Phase 2. O1 is open,
    and `check-claims.sh` rejects any profile other than `none`, which is the owner
    gate in ROADMAP Phase 2.
  - Phase 2 records evidence in the verification report: per profile, the sample
    SHA-256, upstream commit, expected-inventory SHA-256, results-file SHA-256,
    totals, the full-lane run (date, totals, results SHA-256), evidence tier T1 and
    startup tier `none`.
- When O1 is decided, the claim binds as follows:
  - `profile = <id>`;
  - `content_sha256 =` SHA-256 of `tests/inventory/vectors/<id>.txt`. That file
    binds the sample hash, upstream commit, rule, assertion classes and
    exclusions, so the binding is transitive and any change makes the claim stale;
  - `startup_tier = none`; `evidence_tier = T1`;
  - `inventory_sha256 =` the fast.txt hash, as in the template;
  - `evidence = cpu.vectors.<id>` plus the full-lane results file and its hash.
  - [REC] Update the claim template comment to say that, for vector claims,
    `content_sha256` is the vector-inventory hash.

---------------------------------------------------------------------------

## 7. (f) Adversarial pass: ways the harness could pass vacuously

| Failure mode | How it would pass silently | Guard |
|---|---|---|
| Zero vectors run (empty file, glob miss, parser returns 0) | "0 failures" | Planned counts per row are non-zero; the totals line is checked; an exact object count per section; exit 4 on any row with asserted < planned |
| Regex or line extractor misses a layout (I did this: a 0-of-10,000 match) | Filtered set is empty or partial | P2 tokenizer, whitespace-insensitive; per-section exact counts; the two layouts both appear in the committed sample, so the parser is tested on both every run |
| Wrong directory pinned (wdc for rockwell, or the variants swapped) | Most opcodes are identical | Header source line checked against the manifest; per-file SHA-256; swap: the wrong-profile controls fail on exactly the bit-op set; wdc: `cb`/`db` empty means a count mismatch |
| Misplaced or renamed file (`07.json` holds `87` data) | Plausible NOP or bit-op results | `initial.ram[pc] == file opcode` and the name prefix check |
| Silent skip on parse failure | Bad section dropped, the rest pass | Any data error ends the whole run with exit 3; no per-file continue |
| Exclusion creep (an awkward failure "excluded") | Fewer asserted vectors | Exclusions come only from the profile table with closed-vocabulary codes; the expected inventory diff is reviewed; totals report excluded counts; a phase-review item: any exclusion change needs a citation |
| Golden refresh from runner output | Expected inventory regenerated to match a regression | The expected inventory is derived from the sample and the profile table only, never from results; the plan section holds no pass counts |
| Sample misses the interesting opcodes or cases | Rare-case bugs escape | The rule covers every one of the 256 files; the full lane is mandatory before claims; detection power stated in section 3 |
| Identity vectors (final == initial) | A do-nothing CPU passes them | `identity` count reported per row; no-step control requires failures == asserted - identity |
| A harness that copies `final` into the machine | Everything passes, even corrupted expectations | No-step control and wrong-profile control (both would pass under such a harness, and must fail) |
| P-register convention mismatch "fixed" by loosening all of P | Real flag bugs hidden | Mask only bits 4-5 (`0xCF`), with the basis measured in section 1; pushed P bytes still exact in RAM |
| Stray write outside the listed addresses | Final-state compare does not see it | `nowrite` class plus its corrupted-write control |
| Trace gating that tests the generator, not the CPU | Corners the CPU design (`$0056` vs `$0000` dummy reads) | Trace is diagnostic only; unlisted reads are not gated |
| Upstream force-push or repository deletion | Full lane cannot run | Per-file SHA-256 means it fails loudly; the fast gate is unaffected (committed sample); a pin bump is a dedicated change |
| EOL conversion on a client | Sample SHA-256 mismatch | `.gitattributes -text`; sample-identity control |
| Full-lane files left in `build/` | Artifacts scan of 2 GB (minutes) or temptation to exempt by path | Full lane uses mktemp outside the repository; no path exemptions ever |
| ASan or UBSan slowing a 2 GB full lane | Lane abandoned as too slow | Full lane in a release build, plus one ASan run per pin bump; the gate sample always under sanitizers |
| Reading the generator source | Vectors stop being an independent oracle | Exposure-record row; implementers do not open it |

Residual risks [INF]:

- Both profiles' expectations are one author's software model. Systematic modelling
  errors shared by the generator and our reading of the same documentation are
  invisible here. They are caught only by T2 (lynx-tests `cpu`, Phase 4) and,
  owner-gated, by T5 hardware.

---------------------------------------------------------------------------

## 8. Recommendations (for the orchestrator to lock)

1. Map Lynx I-style to `synertek65c02/v1` and Lynx II-style to `rockwell65c02/v1`
   at commit `2f6980a2d95757486c7bee24355c360e40e2a224`. Do not use wdc, 6502 or
   nes6502.
2. Gate on `regs` (P masked `0xCF`), `ram` (final listed) and `nowrite`. Do not
   assert cycle count, bus trace, unlisted reads or ticks; `--trace` is
   diagnostic only.
3. The fast gate uses a committed, verbatim, first-32-per-file sample per variant
   (16,384 vectors, about 6.4 MB of text) in a `#file`-framed container, plus the
   upstream MIT LICENSE and a per-file `upstream.sha256` pin list. No pins.txt row.
4. Full lane `scripts/vectors-full.sh`, outside check.sh and CTest:
   - per-file https download into mktemp, SHA-256 verified;
   - re-derive the sample and `cmp`;
   - run all 10,000 per file;
   - mandatory at Phase 2 verification, pin bumps and before claims;
   - a CI trigger only after measurement.
5. One purpose-built, schema-strict, allocation-free C reader in the test runner,
   with prefix and complete modes, distinct exit codes 0-4 and data-sanity rules
   (opcode at PC, ranges, keys). No JSON dependency, no converter, no Python.
6. Exclusions come only from profile-table `diverges_from_source` entries with a
   closed reason vocabulary. There is no xfail. Excluded rows are still parsed and
   counted.
7. Exact per-profile inventory at `tests/inventory/vectors/<id>.txt` (format in
   section 2.5), compared byte-for-byte; results are checked but never committed.
8. Controls: wrong profile, both directions (failing set == exactly `$x7/$xF`);
   no-step (failures == asserted - identity); corrupted expectation (exactly one
   failure); malformed data (exit 3); inventory drift (exit 4); sample identity;
   mutant profile (failing set == `{op}`); pin-list lint; startup-tier vocabulary.
9. Manifests use `evidence_tier = T1` and `startup_tier = none` (extend
   validate-kv per caller, with a control); add rows to provenance and the exposure
   record.
10. No profile-naming claim in Phase 2 (O1). Record the evidence in the
    verification report. A future claim binds `content_sha256` to the vector
    inventory hash and uses the section 6 wording.

---------------------------------------------------------------------------

## 9. Unknowns and handoffs

- [UNK] Lynx I `$x7/$xF` NOP length and cycles (lynx-tests is length-agnostic).
  [UNK] `$CB/$DB` and other reserved-opcode behaviour on either Lynx.
  - Owner of the decision: CPU-semantics lens (CPU-04).
  - Effect here: these rows run with an "unverified" label, or become
    `profile-diverges` exclusions.
- [UNK] Decimal-mode V flag and invalid-BCD results on Lynx hardware. Vectors
  assert one software model; the per-row decimal subcount keeps this separable.
- [UNK] Whether the Rockwell and Synertek SST sets differ anywhere beyond the bit
  operations. The READMEs say no; it was not verified vector by vector. The full lane
  under both profiles with one shared CPU semantics will show it.
- [UNK] Whether every regenerated file (SBC family, rockwell `xF`) uses B=1. 5 of
  25 were verified; the `0xCF` mask makes this irrelevant to the gate.
- [UNK] Full-lane wall time on a CI runner (download throughput from
  raw.githubusercontent, rate limits for 512 unauthenticated requests).
  Measure before adding any trigger.
- Handoff to the CPU and bus lenses: the test bus needs:
  - a flat 64 KB view seeded from `initial.ram`;
  - a poison value for unlisted reads;
  - write logging for `nowrite`;
  - a per-access cost hook for the CPU-03 cost-sensitivity check;
  - a profile table that can be copied and mutated in tests.
- Owner questions: none required. One note for the batch, auto-followed: the
  repository will carry a small MIT-licensed third-party test-data sample with its
  notice. The fallback, if the owner objects, is a per-prefix range fetch.
