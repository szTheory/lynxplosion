# Headless public CPU suite evidence

The `suite.cpu.lynx1` and `suite.cpu.lynx2` tests build the pinned `lynx-tests` sources, derive expected bytes from their checked-in `main.c` and `tests.s`, load the linked BS93 object through the public BLL API, and run the CPU suite in fresh machines. Each profile runs once with zeroed result/zero-page storage and once with a deterministic test-private seeded pattern. Raw guest bytes and their source-oracle comparisons are separate records. A match means those software observations agree for this build and selected profile input; it does not establish agreement with a physical Lynx or qualify a CPU hardware profile. The overall default profile remains `none`.

## Source oracle

`scripts/generate-suite-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/<owned-run>/oracle.tsv` accepts only the exact extracted pin, archive marker, and per-file source manifest. The generator records the archive and revision identities, SHA-256 values for CPU and timer source/assembly and the license, suite result count, assembly extent, meaningful result count, model tuples, result offsets and accepted byte sets. Production mode refuses a changed identity or source shape and removes any prior output before generating; it has no cached or stale-oracle fallback.

The oracle uses a strict versioned TSV grammar. CPU expectations contain 18 meaningful bytes and both Lynx model tuples. Timer expectations contain 16 meaningful bytes in the pinned 18-byte result extent; the two reserved bytes remain outside the meaningful range. Accepted alternatives are sets, and the timer source's two and three value alternatives are preserved exactly. The C reader checks the complete row inventory, ordering, byte values, alternatives and source identity against the fresh build record before the harness can compare guest results.

The private `--test-source-shape-only` option is used only by negative controls against temporary source copies. It tests grammar failures without changing production pin verification or the source identity chain used by normal runs.

## Linked symbols and bounded completion

The harness parses the actual ld65 label file. Each nonempty line must have exactly three fields in `al HEX NAME` form; a single leading dot on a name is accepted. Every name must be syntactically valid, every required label must occur once, and code, result, model, progress, probe and completion ranges are checked for address-space overflow and overlap before result RAM is read. There are no guessed addresses.

Completion uses guest ticks from the public API and a guest-written post-return sentinel. The original CPU wrapper seeds a source-level zero-page byte with `1` and executes RMB0; the probe byte and guest-written marker determine the observed model without consulting the host profile input. Separately, the four raw result bytes are matched against both source-defined model tuples to determine which tuple the guest produced. The harness requires probe, marker and tuple agreement, then compares that observation with the selected profile. The selected profile is only an independent comparison input. The wrapper requires the original suite to return, nonzero progress, and its sentinel before the harness reads results.

The minimum is 256 master ticks: the wrapper clears a full 256-byte zero-page range before invoking the suite, and each guest store consumes at least one master tick under the current scheduler. This is a conservative software-model floor, not a measured hardware cycle count. The finite exploratory maximum is 50,000,000 guest ticks. The pinned CPU suite completed at 26,696 ticks under `lynx1` and 26,841 under `lynx2`; each zeroed and seeded fresh instance produced the same profile-specific raw bytes, 18/18 source expectations matched, and progress was `8`. The observed probe/marker pairs were `01`/`a1` for `lynx1` and `00`/`a2` for `lynx2`. Relative to the source-work floor, the completion margins were 26,440 and 26,585 ticks; relative to the finite maximum, the remaining headroom was 49,973,304 and 49,973,159 ticks. Run records keep detected model, raw probe bytes, source tuple model, result counts, guest-time limits, observed ticks and both margins explicit. `suite.cpu.bound_controls` validates captured completion records at the exact accepted `256` and `50,000,000` boundaries and rejects `255`, `50,000,001`, and a value beyond the accepted integer range. Those are parser boundary controls, not runs claiming guest completion at those values. No host clock is used as guest time.

## Controls and execution

The registered tests `suite.oracle_tracer`, `suite.oracle_controls`, `suite.symbol_controls`, `suite.cpu.lynx1`, `suite.cpu.lynx2`, `suite.cpu.model_controls`, and `suite.cpu.bound_controls` run from isolated `build/check/scratch/suites/<test-id>` directories so parallel tests cannot share invocation output. The driver removes each invocation-owned directory after the run. `suite.cpu.model_controls` executes both real profile configurations, verifies a real Lynx I guest against an intentionally opposite selected expectation, then mutates captured run/raw records to test missing or corrupt probe bytes, probe/tuple disagreement, count drift and an oracle-shaped zero result block. These captured-record parser controls do not add production fault flags. `suite.cpu.bound_controls` mutates captured guest-time records at and around the accepted integer bounds; it does not manufacture guest-time progress. Oracle controls exercise malformed bytes, count and extent drift, lost alternatives, and stale source identity. Source-shape controls exercise CPU count drift, malformed and nonbyte tokens, timer alternative loss, overlapping offsets, extent confusion, and empty source. Symbol controls exercise missing, duplicate and malformed labels, empty input, completion/result overlap, and 16-bit extent overflow. Each negative control requires exit status 1 and its intended assertion or parser diagnostic; a generic timeout, compile error or other failure does not pass.

Run the focused evidence with:

```sh
cmake --preset check
cmake --build --preset check
ctest --preset check --no-tests=error --output-on-failure \
  -R '^suite\.(oracle_tracer|oracle_controls|symbol_controls|cpu\.(lynx1|lynx2|model_controls|bound_controls))$'
```

The pinned upstream sources and this project’s wrapper remain subject to their fixture manifests and notices. No firmware or commercial game content is used. These software controls establish parser, identity, bounds and repeatable guest-execution behavior for the pinned source; hardware accuracy and public profile qualification remain unresolved.
