# Headless public CPU, timer and page-mode suite evidence

The `suite.cpu.lynx1` and `suite.cpu.lynx2` tests build the pinned `lynx-tests` sources, derive expected bytes from their checked-in `main.c` and `tests.s`, load the linked BS93 object through the public BLL API, and run the CPU suite in fresh machines. Each profile runs once with zeroed result/zero-page storage and once with a deterministic test-private seeded pattern. Raw guest bytes and their source-oracle comparisons are separate records. A match means those software observations agree for this build and selected profile input; it does not establish agreement with a physical Lynx or qualify a CPU hardware profile. The overall default profile remains `none`.

## Source oracle

`scripts/generate-suite-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/<owned-run>/oracle.tsv` accepts only the exact extracted pin, archive marker, and per-file source manifest. The generator records the archive and revision identities, SHA-256 values for CPU and timer source/assembly and the license, suite result count, assembly extent, meaningful result count, model tuples, result offsets and accepted byte sets. Production mode refuses a changed identity or source shape, writes through an exclusively created mode-restricted temporary file in the validated output parent, and atomically publishes a regular output file; it has no cached or stale-oracle fallback.

The oracle uses a strict versioned TSV grammar. CPU expectations contain 18 meaningful bytes and both Lynx model tuples. Timer expectations contain 16 meaningful bytes in the pinned 18-byte result extent; the two reserved bytes remain outside the meaningful range. Accepted alternatives are sets, and the timer source's two and three value alternatives are preserved exactly. The C reader checks the complete row inventory, ordering, byte values, alternatives and source identity against the fresh build record before the harness can compare guest results.

## CPU observations

The two explicit profile runs complete with guest progress `8`, a guest-written
completion sentinel `a5`, and matching 18/18 source comparisons. Each profile
was run with zeroed and deterministic seeded result/zero-page storage; the two
runs produced the same bytes for that profile. The recorded result bytes are
listed in source-index order below. This is pinned-source software evidence;
O1 remains open and the bytes do not qualify hardware profiles.

| Profile | Guest ticks | Probe / marker | Raw results, indices 0–17 |
| --- | ---: | --- | --- |
| `lynx1` | 26,696 | `01` / `a1` | `00 00 52 06 01 02 80 02 30 bb 03 00 ff 00 02 02 00 00` |
| `lynx2` | 26,841 | `00` / `a2` | `00 00 52 06 01 02 80 02 30 bb 03 00 fe 01 01 01 00 00` |

The private `--test-source-shape-only` option is used only by negative controls against temporary source copies. It tests grammar failures without changing production pin verification or the source identity chain used by normal runs.

## Page-mode source build

`sh scripts/build-page-mode.sh --out build/check/scratch/<new-owned-output>` assembles the unchanged pinned `page-mode/tests.s` into a temporary tier-A BS93 object. The companion `sh scripts/generate-page-mode-oracle.sh --source build/pins/lynx-tests --out build/check/scratch/<new-oracle.tsv>` derives six expected bytes from pinned `main.c` and `tests.s`: `35 41 69 81 74|75 81`, with offsets `0..5`, one byte per test, and a six-byte result extent. Together with their artifact records, the builder and oracle bind the exact page-mode source, upstream notice, cc65 tools, wrapper, BLL layout, recipe, labels and object identities; existing CPU and timer outputs and descriptors remain independent.

The page wrapper keeps `WaitVBlank`, the MAPCTL accesses, Timer6 measurements and SERCTL trigger writes from upstream source. It configures the source-declared Timer0/Timer2 wait chain and uses the explicit unverified `VIDEO_POLICY1` policy in the host harness. Display DMA remains disabled. This source build establishes assembly and oracle provenance only; it does not claim runtime completion, expected-byte agreement, hardware accuracy or profile qualification.

## Page-mode runtime blocker

The registered suite.page_mode.observation.lynx1 and
suite.page_mode.observation.lynx2 cases each create a fresh BS93 instance,
probe the guest model independently, select VIDEO_POLICY1, and execute the
unchanged source until its first unsupported request. Both reach the initial
TriggerPulse before the first page-mode test body can write a result:
SERCTL address $FD8C, write value $06. The harness stops there and emits the
exact LYNX-SKIP: page-mode source requires unavailable SERCTL write at $FD8C
signal. These are declared CTest skips, with matching rows in
tests/inventory/skips.txt; the JUnit inventory gate rejects undeclared skips
and stale declarations.

| Profile | Guest ticks | Probe / marker | Progress / done | First unsupported request | Result evidence |
| --- | ---: | --- | --- | --- | --- |
| lynx1 | 267,429 | 01 / a1 | 1 / 00 | write $06 to $FD8C | 0 meaningful bytes; raw observation missing; comparison not run |
| lynx2 | 267,414 | 00 / a2 | 1 / 00 | write $06 to $FD8C | 0 meaningful bytes; raw observation missing; comparison not run |

The 40,000-tick minimum is a software-model bound derived from the wrapper's
Timer0 backup 158 and Timer2 backup 104 wait chain: it exceeds two complete
(158 + 1) × (104 + 1) timer periods. The finite maximum is 50,000,000 guest
ticks. The controls also hold a fresh machine for the minimum time before
releasing it, then prove that the same exact SERCTL request is reached. Timer
setup remains Timer0 backup/control 9e/18 and Timer2 68/1f; MAPCTL is 0c,
timing policy is VIDEO_POLICY1, and display DMA remains disabled. The MAPCTL
reads inside the six tests, their Timer6 measurements, test completion, and
all six result writes are not reached.

The exact source, build, tool, oracle and runtime-record identities were
validated before the invocation-owned scratch was removed:

| Artifact | SHA-256 |
| --- | --- |
| Pinned Lynx tests revision | b583dc43d532d3c41563a6fa35420614fbcead89 |
| page-mode/main.c | 4504f1c9ebe9fc0a414f245bcb45193b2a90e5025f912b23c9443624b95acce7 |
| page-mode/tests.s | 929d0f032a5f1cf36ff55b163abec388adb08159304e56cf25516c705e6410f4 |
| Upstream notice | 5473029e68513703f857ffcac8194be3edae89469a71143499b9579d9a3ec2e3 |
| cc65 ca65 | aae8906856e0a07e3eb138ed83fd52c1f20ae0b21c35000ce774c896f1785967 |
| cc65 ld65 | e74938752aabfe22609d7cf3009a525630f1d4dc279a53bad569360b42b3ffd6 |
| Page-mode BLL object | 63ba7bc2778a4d9f1f78de1307e21e91781109aa82ae102c3636bd080183f2c6 |
| Page-mode labels | ca6f3e4f695fe574e039a4489a630a86e5aebf20a259ea9301b65353d3a9825e |
| Six-row source oracle | 38f96b51601edab5e927f64e59b1bf2ca57a84fca6eaa68d84550099a1cf9d26 |
| Page-mode build record | 21ec422b3e905c588cc3ee20efa00eec6910e9ef9d0be2d1369aea05089056d4 |
| Wrapper | 8e8d403eb5b58db1914d3e105a2c5da4ebada05c8aa2a74c7953507e00e4d18d |
| BLL layout | f00dbf7ea1b9c47a1e20f658bb4b981149c7def2db886985c5b44f6b295422e6 |
| Fixture manifest | 54ef001c06b44b620350941ed246e9bf409baa8cd7f56865f41f7163708c4ce3 |
| Page-mode builder recipe | 163a5ba66568eda1e04b452a35280810ec19218fffccf6fdfe413f6b1b317af7 |
| Page-mode oracle recipe | faa03ce3f05904259d1fd99d98145a277bc2b027a00a7151f8724d6254bb6fea |
| Lynx I blocker record | dbf4288b48020c09a187878423d38972fef066bb2e301de86d02ba662aa258c1 |
| Lynx II blocker record | f0328a044b9d562e7ace6fcf4a0dd81d9f6aeab2335c2eda6c3c20306bfbbf5a |

Neither skip is a source-oracle agreement, raw frame, input response, hardware
result, or profile qualification. EVD-02 remains open.

## Timer observations and the pending source row

`observe timers PROFILE BUILDDIR ORACLE PENDING_DESCRIPTOR` executes a fresh guest for the requested profile, retains all 16 meaningful raw bytes and source comparisons, and writes a hash-bound pending record. `PROFILE` is exactly `lynx1` or `lynx2`; pass it as one argument without a shell pipe. Its successful observation is deliberately limited to 15 source agreements plus the exact D-10 pending row: result index 14 is `0c` while the unchanged pinned source expects `0d`. The strict `run timers PROFILE BUILDDIR ORACLE` command keeps that disagreement visible and exits 1 with `suite: timer source oracle comparison mismatch`; no accepted-value set or upstream source expectation is changed.

The timer wrapper executes its own A1/A2 model probe. The observed records preserve profile, probe and marker bytes, 16 meaningful results in the 18-byte extent, guest progress and sentinel, tick bounds, fixed phase, flat five-tick CPU cost, grant-time sampling, disabled page-mode/video and refresh DMA, and zero-initialized timer counter. A matching software observation does not establish hardware accuracy or profile qualification; the default profile remains `none` and O1 remains open.

| Profile | Guest ticks | Probe / marker | Progress / sentinel | Raw timer results, indices 0–15 | Observation status |
| --- | ---: | --- | --- | --- | --- |
| `lynx1` | 359,941 | `01` / `a1` | `8` / `a5` | `08 7f 00 08 00 01 08 00 04 08 00 36 00 01 0c 08` | Complete bounded software observation; 15/16 source rows agree; D-10 row 14 pending |
| `lynx2` | 359,916 | `00` / `a2` | `8` / `a5` | `08 7f 00 08 00 01 08 00 04 08 00 37 00 01 0c 08` | Complete bounded software observation; 15/16 source rows agree; D-10 row 14 pending |

The full 16-row per-profile comparison and pinned source/tool/build/proof
identities are recorded in the [Phase 4 qualification ledger](../.planning/phases/04-timers-interrupts-and-first-public-suites/04-QUALIFICATION.md).

`observe timers-video PROFILE BUILDDIR ORACLE` runs the same pinned timer guest
without a pending descriptor under the explicitly frozen VIDEO_POLICY1 setup
documented in [the Phase 5 revisit report](timer-video-revisit.md). Its strict
comparison returns success only for 16 source agreements and returns 1 for any
mismatch. The two profiles currently agree on 15 rows; each produces `0f` at
index 14 against the pinned `0d`, so both strict observations exit 1. This
different result does not inherit D-10's legacy `0c` exception. The current
model accepts only DISPCTL bit 0; the CRT's `0d` setting and hardware accuracy
remain unresolved.

`timer-cascade-model` verifies the linked Test6 setup and poll opcodes, operands, addressing modes and branch targets against the unchanged pinned assembly. It derives 14-cycle no-IRQ and 24-cycle IRQ paths, yielding 70/120 tick poll intervals, then independently enumerates all64 first-edge delays over the source-declared 192 polls. The executable proof reports12 acknowledgments and final poll offset13990 for every phase, and binds those results to the actual BLL, labels, raw bytes, run record, comparison, oracle and pending descriptor. This matches the current bounded guest result without modifying its timing or the oracle. The pending row is scheduled for Phase5 review; global EVD-02 remains open.

The exact fast inventory contains these ten timer tests. The two legacy
observations and the video-policy revisit are guest runs; the other seven are
strict, proof, model, record, descriptor, no-run or continuation controls.
They are not suite-pass claims. The
data-only `result_controls` batch emits exactly 49 unique named subcases. This
preserves the pre-fix inventory count; Plan 09 recorded 48 before Plan 10 added
the copied Lynx II profile case. The pending-controls batch now emits exactly
61 unique names: its prior 55 data-only cases plus one symlink-publication
control and five distinct source/object/label identity controls. Both batches
reject a missing name and a duplicate name through explicit manifest
self-controls; these mutations are not counted as guest executions.

| CTest ID | Role |
| --- | --- |
| `suite.timers.observation.lynx1` | Positive lynx1 timer observation |
| `suite.timers.observation.lynx2` | Positive lynx2 timer observation |
| `suite.timers.strict_comparison_controls` | Requires the unchanged strict mismatch and exact D-10 classification |
| `suite.timers.cascade_model` | Independent 64-phase software proof against linked source and actual guest result |
| `suite.timers.model_controls` | Rejects invalid/mismatched guest model evidence |
| `suite.timers.pending_controls` | Rejects stale/malformed descriptor, records, identities and proof |
| `suite.timers.result_controls` | Rejects result/count/time/completion faults and any second mismatch |
| `suite.timers.no_run` | Proves held guest execution is absent, then validates resumed execution |
| `suite.timers.video_revisit` | Captures both VIDEO_POLICY1 profiles and requires complete strict row diagnostics without a pending exception |
| `suite.timers.video_revisit_controls` | Validates independent video-policy identities, strict result records, descriptor refusal, bounds and common-tick continuation controls |

The strict controls require exit 1 and the exact source mismatch diagnostic
only after valid observations and proof; they also require exactly 16
comparison rows, 15 accepted rows and the sole index 14 mismatch
`0c`/`EXPECT(0d)`. The full Plan 12 Task 1 targeted integration gate and its
current result are recorded in the qualification ledger.

The timer controls `suite.timers.model_controls`, `suite.timers.result_controls`, and `suite.timers.pending_controls` also require exact named refusals. A wrong selected profile is an actual guest execution and is bracketed by fresh observations for both profiles. Captured-result, record, descriptor and companion mutations use private copies of an immutable baseline captured from fresh Lynx1/Lynx2 observations at the start of each control-test invocation; both baseline records are positively revalidated and their hashes checked before and after every individual mutation. Each batch ends with fresh observations for both profiles. The EXPECT alternative-set cases validate a separately labelled captured comparator corpus, not guest execution. `video_revisit_controls` similarly freezes both fresh profile baselines, checks exact source/tool/object/core/layout/policy identity and all 16 rows, rejects legacy-descriptor inheritance and malformed rows, and brackets the controls with new observations. Its manufactured agreeing comparison and accepted tick candidates are labelled as comparator/parser evidence, not guest execution. Its uninterrupted, sliced and watched runs must agree at common guest ticks. These controls do not accept another mismatch, alter D-10's pending row, or qualify the profiles.

The Timer2 suite is source-only at this stage. Page-mode timing, audio, and the later audio2 suite remain deferred for scoped future work; none of these source or software observations resolves physical, firmware, rights, publication or release stops.

## Linked symbols and bounded completion

The harness parses the actual ld65 label file. Each nonempty line must have exactly three fields in `al HEX NAME` form; a single leading dot on a name is accepted. Every name must be syntactically valid and every required label must occur once. Linked code must stay in the payload range `$0200-$61FF`; result bytes and progress/model/completion bytes must stay in BSS `$8000-$BFFF`; the model probe must stay in zero page `$0000-$00FF`. These ranges keep all suite data away from the BLL header and CPU stack at `$0100-$01FF`. The harness rejects control bytes that alias code, result data, or another control byte before reading guest results. There are no guessed addresses.

Completion uses guest ticks from the public API and a guest-written post-return sentinel. The original CPU wrapper seeds a source-level zero-page byte with `1` and executes RMB0; the probe byte and guest-written marker determine the observed model without consulting the host profile input. Separately, the four raw result bytes are matched against both source-defined model tuples to determine which tuple the guest produced. The harness requires probe, marker and tuple agreement, then compares that observation with the selected profile. The selected profile is only an independent comparison input. The wrapper requires the original suite to return, nonzero progress, and its sentinel before the harness reads results.

The minimum is 256 master ticks: the wrapper clears a full 256-byte zero-page range before invoking the suite, and each guest store consumes at least one master tick under the current scheduler. This is a conservative software-model floor, not a measured hardware cycle count. The finite exploratory maximum is 50,000,000 guest ticks. The pinned CPU suite completed at 26,696 ticks under `lynx1` and 26,841 under `lynx2`; each zeroed and seeded fresh instance produced the same profile-specific raw bytes, 18/18 source expectations matched, and progress was `8`. The observed probe/marker pairs were `01`/`a1` for `lynx1` and `00`/`a2` for `lynx2`. Relative to the source-work floor, the completion margins were 26,440 and 26,585 ticks; relative to the finite maximum, the remaining headroom was 49,973,304 and 49,973,159 ticks. Run records keep detected model, raw probe bytes, source tuple model, result counts, guest-time limits, observed ticks and both margins explicit. `suite.cpu.bound_controls` validates captured completion records at the exact accepted `256` and `50,000,000` boundaries and rejects `255`, `50,000,001`, and a value beyond the accepted integer range. Those are parser boundary controls, not runs claiming guest completion at those values. No host clock is used as guest time.

`suite.cpu.no_run` and `suite.timers.no_run` use the private core bus-hold seam on fresh machines for both explicit profiles. The harness arms the hold at the linked entry PC before the first wrapper instruction, snapshots the full CPU/request and RAM state, then advances guest time to at least 256 ticks. Unchanged CPU/request and RAM state proves the tests did not run and no guest write was granted. The ordinary completion validator then emits its exact absent-execution refusal before model or timer-pending checks. The harness releases the hold on that same instance and requires the normal positive evidence path to complete; each profile also has a separate fresh positive immediately before its held case. These controls are software execution evidence only.

`suite.completion_controls` runs eight actual guest fault cases: sentinel omission after the real tests return, and a 256-tick guest-time limit before completion, for CPU and timers under both profiles. The sentinel variant replaces only the final five-byte guest store with five NOPs, retaining linked addresses and untouched upstream `tests.s`; its wrapper, linked object, pinned tools and source inputs are recorded in invocation scratch. Every fault begins with hash-verified output files from a fresh positive capture in the output directory, then runs through a new machine and the normal completion watch. The harness requires the exact guest refusal, progress/sentinel/watch state and absence of downstream evidence before a fresh positive is run. This establishes that prior same-invocation output files cannot impersonate completion when the actual fresh-machine path does not complete. Hashes alone cannot distinguish two deterministic captures that share the same profile, inputs and build.

`suite.selection_controls` requires exact nonempty required-suite and control selections and rejects empty, unknown, duplicate and incomplete lists. It also checks that each registered required/control CTest ID occurs exactly once in `tests/inventory/fast.txt`. CPU and timer captured-record controls take one positive capture per profile into immutable invocation-owned baselines, verify each baseline hash and validate its positive record around each private mutation. The finite controls include missing/corrupt progress and sentinel evidence, zero result blocks, missing/stale model and source/object/label identities, count/time boundary failures, and copied Lynx II evidence presented as Lynx I. Copied timer raw rows fail the raw identity check before record or pending-row classification. Captured-record controls are parser evidence, not guest runs. No control resolves O1, profile qualification or the pending timer source row.

## Controls and execution

The registered tests run from isolated `build/check/scratch/suites/<test-id>` directories so parallel tests cannot share invocation output. The CMake driver creates a unique nonce-named run directory with a path-bound owner marker. After success it rechecks canonical containment, rejects symlink ancestors, and requires the exact path/nonce marker before removing that invocation directory. Failed runs retain their scratch for diagnosis. Harness output cleanup first validates the canonical owned directory and marker, then unlinks only known direct file entries (including symlinks themselves), without following symlink targets or removing directories. CPU model controls execute both real profiles, verify a real Lynx I guest against an intentionally opposite selected expectation, then mutate private records for probe, progress, sentinel, tuple, count, zero-result and identity checks. CPU bound controls mutate captured guest-time records at and around the accepted integer bounds; they do not manufacture guest-time progress. Timer result and pending controls use the same invocation-owned positive batching and retain D-10's exact pending descriptor. Oracle controls exercise malformed bytes, count and extent drift, lost alternatives, and stale source identity. Source-shape controls exercise CPU count drift, malformed and nonbyte tokens, timer alternative loss, overlapping offsets, extent confusion, and empty source. Symbol controls exercise missing, duplicate and malformed labels, empty input, completion/result overlap, and 16-bit extent overflow. Each negative control requires exit status 1 and its intended assertion or parser diagnostic; a generic timeout, compile error or other failure does not pass.

Run the focused evidence with:

```sh
cmake --preset check
cmake --build --preset check
ctest --preset check --no-tests=error --output-on-failure \
  -R '^suite\.(oracle_tracer|oracle_controls|symbol_controls|cpu\.(lynx1|lynx2|model_controls|bound_controls))$'
```

The page-mode slice runs both source-bound observations, its blocker/no-run and
record/parser controls, isolated builder/oracle source, tool, notice, marker,
shape and output-ownership hostility controls, and the existing oracle and
symbol controls:

~~~sh
cmake --preset check
cmake --build --preset check
ctest --preset check --no-tests=error --output-on-failure \
  -R '^suite\.(page_mode\.(observation\.(lynx1|lynx2)|controls)|oracle_controls|symbol_controls)$'
~~~

The timer observation/control slice can be rerun with:

```sh
cmake --preset check
cmake --build --preset check
ctest --preset check --no-tests=error --output-on-failure \
  -R '^suite\.timers\.(observation\.(lynx1|lynx2)|strict_comparison_controls|cascade_model|model_controls|result_controls|pending_controls|video_revisit|video_revisit_controls)$'
```

The pinned upstream sources and this project’s wrapper remain subject to their fixture manifests and notices. No firmware or commercial game content is used. These software controls establish parser, identity, bounds and repeatable guest-execution behavior for the pinned source; hardware accuracy and public profile qualification remain unresolved.
