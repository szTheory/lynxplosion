---
phase: 04
gate: D-10 timer result index 14 pending comparison
verified: 2026-11-11
status: passed
scope: bounded pending-gate acceptance only
head: ae353db
implementation_source: 27145c8
independent_selection: 4/4 CTests passed in 4.52s
---

# Phase 4 D-10 Pending-Gate Verification

## Verdict

**PASS for the bounded D-10 acceptance gate.** The implementation recognizes only the owner-approved timer observation (`0c` at result index 14 against the unchanged source expectation `0d`) in the declared `flat5`, fixed-zero-phase, no-video/no-refresh-DMA environment. The strict `run timers` comparison remains failing on that exact row. This report does not qualify Phase 4 or either hardware profile.

## Independent execution

Ran this nonempty selection against the current checkout:

```sh
ctest --test-dir build/check --no-tests=error --output-on-failure \
  -R '^suite\.timers\.(observation\.(lynx1|lynx2)|strict_comparison_controls|cascade_model)$'
```

Result: **4/4 passed** (`suite.timers.observation.lynx1`, `suite.timers.observation.lynx2`, `suite.timers.strict_comparison_controls`, and `suite.timers.cascade_model`). The observations rebuilt the pinned source suite and emitted fresh records in `build/check/scratch/suites/`.

## D-10 checks

| Check | Evidence | Result |
| --- | --- | --- |
| Observe and strict comparison are separate paths | `tests/suites/test_public_suites.c:3532-3539` dispatches `run timers` without a pending descriptor and `observe timers` with one. `tests/suites/run.cmake:1723-1758` requires strict execution to exit 1 with the exact mismatch diagnostic, then checks 16 comparison rows, 15 accepted rows, and only `result 14 / 0c / EXPECT(0d)`. | PASS |
| Both selected profiles retain all source rows | Fresh run evidence below; each has 16 raw rows and 16 comparison rows, 15 agreements, and the same single index-14 mismatch. Profile probes, progress, sentinel, and guest-time bounds are recorded and validated. | PASS |
| Pending exception is exact and fail-closed | `tests/suites/test_public_suites.c:1215-1313,1508-1724` fixes descriptor values and validates raw/comparison rows against the parsed source oracle. `tests/suites/run.cmake:1102-1149,1840-1849` mutates index 14 to `0b` and `0d`, adds a second mismatch, and removes/duplicates/adds rows; each must be rejected. | PASS |
| Descriptor and companion schemas reject stale or malformed evidence | The shared JSON parser rejects duplicate and unknown keys, malformed data, missing required fields, and wrong field types. Descriptor and companion validators require their exact schema/version, values, field sets, and live identities. `tests/suites/run.cmake:1862-1917` enumerates the descriptor and companion negative controls. | PASS |
| Data mutations cannot masquerade as guest execution | `observe timers` runs the guest, validates changed CPU state, bounded progress, model probe/marker, result extent and post-return sentinel before writing evidence. Captured-result cases mutate private baseline copies; `timer_batch_begin/case_begin/case_end/batch_end` preserve and revalidate fresh positive baselines. | PASS |
| Build and evidence identities are bound | The fresh records bind the pinned revision/archive, timer source and C oracle, wrapper, generated oracle, linked BLL, labels, selected profile and run records. Proof output binds the descriptor and observed files; the companion binds raw, run, comparison, BLL, labels, oracle, descriptor and proof. Companion validation regenerates the proof into an owned temporary and compares its hash. Descriptor does not embed companion/proof-output hashes, so this chain is acyclic. | PASS |
| Timing explanation derives from current pinned source and linked code | `tests/suites/test_public_suites.c:1767-2050` checks source setup, runner call, linked Test 6 bytes, loop opcodes/operands and branch targets against the actual BLL and label map before enumerating phases. Fresh proof results are summarized below. | PASS |

The exact ordered control manifests in `tests/suites/run.cmake:655-705` require **49 result controls** and **61 pending controls**, with exact ordering and uniqueness; missing-name and duplicate-name manifest self-controls are also present (`:715-742`). `04-REVIEW-FIX.md` records the 49-result batch passing, and `04-SECURITY.md` records the root's current-source selection passing the 61 pending controls along with observation/strict checks. I did not rerun those longer batches. This review checked the current inventory and implementation directly.

## Fresh guest records

Fresh artifacts are in `build/check/scratch/suites/suite.timers.observation.lynx1/run-8af186f3af23323f10430e099a5cc81bc6ef0a85ea9b99b3/build/` and `build/check/scratch/suites/suite.timers.observation.lynx2/run-ebef71d494b650df3cf8bdd0394d539b59b4d448b69ceade/build/`. Each directory contains that profile's `raw-results`, `comparison`, `run-record`, cascade proof, pending companion, built suite and label map. The strict-comparison outputs are under `build/check/scratch/suites/suite.timers.strict_comparison_controls/run-932e9221b55789c91512d3162f7f5cfb4ca1a5cc5cf64336/build/`; the independently regenerated proof outputs are under `build/check/scratch/suites/suite.timers.cascade_model/run-05b1b843e95aabe7d5305facfc23077667b13799006e07e2/build/`.

| Profile | Raw results, indices 0–15 | Guest ticks | Probe/marker | Agreements |
| --- | --- | ---: | --- | ---: |
| `lynx1` | `08 7f 00 08 00 01 08 00 04 08 00 36 00 01 0c 08` | 359,941 | `01/a1` | 15/16 |
| `lynx2` | `08 7f 00 08 00 01 08 00 04 08 00 37 00 01 0c 08` | 359,916 | `00/a2` | 15/16 |

Both fresh run records show progress `8`, completion sentinel `a5`, 16 meaningful results plus 2 reserved result bytes, a 256–50,000,000 tick window, `flat5` at 5 ticks, sample offset 0, fixed-zero phase, and page-mode/video and refresh DMA disabled. They state `source_comparison=separate-record`, `hardware_accuracy=unverified`, and `profile_qualification=unresolved`.

The unchanged pinned `timers/main.c` expects `0d` at index 14. The generated comparisons retain all 16 source rows; row 14 is exactly `0c / EXPECT(0d) / source-mismatch`, with all others accepted. Current live descriptor SHA-256 is `6ff40ce64e426f9d66a9aec5633b98dc2353c9b127753ab7cc1be5d2c96d628c`; the fresh companions reference that digest. The `67e5614a...` descriptor digest in `04-QUALIFICATION.md` is a Task 12 Task 1 historical observation, not the current descriptor identity.

## Independent timing proof

The pinned Test 6 setup uses Timer 3 reload value `0x10` with source selector 2, Timer 5 linked to Timer 3, and a 192-iteration poll loop. The source does not clear its zero-page `t5_count`; the original wrapper explicitly clears zero page before entering it. No source expectation, counter seed, timer phase, cost, code placement, or DMA behavior was tuned for agreement.

The fresh linked BLL and labels are bound by SHA-256 `d766a24031246ad166edc6b417e4d30a841e18dda5115eae201804847ef96c79` and `b1423949c03badc1158b5cacfb67af146cbe18228cc73f156b11dd548776efa3`. Test 6 resolves to `03dc`; its poll loop resolves to `0400`. The linked 17-byte loop is `ad 81 fd 29 20 f0 07 a9 20 8d 80 fd e6 01 ca d0 ef`; its BEQ and BNE targets remain on the same page. The validator also checks the exact 36-byte setup prefix, runner call target, result location, code bounds and unique loop occurrence.

The source/linked-code proof derives a 20-tick first poll, 14-cycle/70-tick no-IRQ intervals, 24-cycle/120-tick IRQ intervals, and a 1,088-tick Timer 3 reload period. All 64 first-edge delays produce 12 acknowledgments and final poll offset 13,990. The thirteenth expiry falls between 14,081 and 14,144 ticks, after that final poll. This explains the software observation in the declared environment; it does not explain the upstream `0d` measurement or assert hardware accuracy. The descriptor binds the proof report digest `80c8fbb7fd082ad8f0e5a909e249a243049c2315a9e3a72e8835b4e053f2f2af`.

## Remaining phase boundary

Plan 12 Task 2 macOS/Linux/exact-head CI qualification remains pending; it is neither missing implementation nor passed qualification in this review. `04-SECURITY.md` keeps final qualification threat T-04-32 open. Global EVD-02 remains open for Phase 5 bus/video prerequisites, recurring EVD-05 remains open, and O1/hardware profile qualification remain unresolved. The strict source mismatch remains visible. No phase-completion or hardware-accuracy claim follows from this bounded D-10 pass.
