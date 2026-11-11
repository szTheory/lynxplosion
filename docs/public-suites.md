# Headless public CPU suite evidence

The `suite.cpu.lynx1` test builds the pinned `lynx-tests` sources, derives the expected bytes from their checked-in `main.c` and `tests.s`, loads the linked BS93 object through the public BLL API, and runs the CPU suite in a fresh machine. Its output separates raw guest bytes from comparison with the source-derived software oracle. A match means those two observations agree for this build and profile input; it does not establish agreement with a physical Lynx or qualify a CPU hardware profile.

## Source oracle

`scripts/generate-suite-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/<owned-run>/oracle.tsv` accepts only the exact extracted pin, archive marker, and per-file source manifest. The generator records the archive and revision identities, SHA-256 values for CPU and timer source/assembly and the license, suite result count, assembly extent, meaningful result count, model tuples, result offsets and accepted byte sets. Production mode refuses a changed identity or source shape and removes any prior output before generating; it has no cached or stale-oracle fallback.

The oracle uses a strict versioned TSV grammar. CPU expectations contain 18 meaningful bytes and both Lynx model tuples. Timer expectations contain 16 meaningful bytes in the pinned 18-byte result extent; the two reserved bytes remain outside the meaningful range. Accepted alternatives are sets, and the timer source's two and three value alternatives are preserved exactly. The C reader checks the complete row inventory, ordering, byte values, alternatives and source identity against the fresh build record before the harness can compare guest results.

The private `--test-source-shape-only` option is used only by negative controls against temporary source copies. It tests grammar failures without changing production pin verification or the source identity chain used by normal runs.

## Linked symbols and bounded completion

The harness parses the actual ld65 label file. Each nonempty line must have exactly three fields in `al HEX NAME` form; a single leading dot on a name is accepted. Every name must be syntactically valid, every required label must occur once, and code, result, model, progress, probe and completion ranges are checked for address-space overflow and overlap before result RAM is read. There are no guessed addresses.

Completion uses guest ticks from the public API and a guest-written post-return sentinel. The original CPU wrapper's Lynx I probe stores `1` in a source-level zero-page byte and executes RMB0; the observed byte determines the guest-written model marker. The selected profile is only an independent comparison input. The wrapper requires the original suite to return, nonzero progress, and its sentinel before the harness reads results.

The minimum is 256 master ticks: the wrapper clears a full 256-byte zero-page range before invoking the suite, and each guest store consumes at least one master tick under the current scheduler. This is a conservative software-model floor, not a measured hardware cycle count. The finite exploratory maximum is 50,000,000 guest ticks. The observed pinned CPU/lynx1 run completed at 26,696 ticks with 18/18 source expectations matching, progress `8`, probe byte `01`, and model marker `a1`. Exact minimum/maximum boundary controls use integer tick records; values below 256, above 50,000,000, or overflowing the accepted integer type are rejected. No host clock is used as guest time.

## Controls and execution

The registered tests `suite.oracle_tracer`, `suite.oracle_controls`, `suite.symbol_controls`, and `suite.cpu.lynx1` run from isolated `build/check/scratch/suites/<mode>` directories. The driver removes each invocation-owned directory after the run. Oracle controls exercise malformed bytes, count and extent drift, lost alternatives, and stale source identity. Source-shape controls exercise CPU count drift, malformed and nonbyte tokens, timer alternative loss, overlapping offsets, extent confusion, and empty source. Symbol controls exercise missing, duplicate and malformed labels, empty input, completion/result overlap, and 16-bit extent overflow. Each negative control requires exit status 1 and its intended parser diagnostic; a generic failure does not pass.

Run the focused evidence with:

```sh
cmake --preset check
cmake --build --preset check
ctest --preset check --no-tests=error --output-on-failure \
  -R '^suite\.(oracle_tracer|oracle_controls|symbol_controls|cpu\.lynx1)$'
```

The pinned upstream sources and this project’s wrapper remain subject to their fixture manifests and notices. No firmware or commercial game content is used. These software controls establish parser, identity, bounds and repeatable guest-execution behavior for the pinned source; hardware accuracy and public profile qualification remain unresolved.
