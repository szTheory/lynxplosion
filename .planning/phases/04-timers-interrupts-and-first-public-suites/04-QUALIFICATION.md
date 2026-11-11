# Phase 4 software qualification ledger — Plan 12 Tasks 1–2

**Status:** Task 1 evidence, parent reviews/fixes, and independent pending-gate verification are recorded. The exact-source macOS and Ubuntu local gates passed at `7616f6d`; exact-head CI remains pending root review. This is not Phase 4 completion.

## Scope and decision boundary

This ledger records the approved D-10 Phase 4 observation/control slice. The
unchanged timer source comparison at index 14 remains pending: expected `0d`,
observed `0c` for both explicit software profile inputs. The strict timer
command remains failing, and global EVD-02 remains open for Phase 5 bus/video
evidence. This ledger does not claim a suite pass, hardware accuracy, profile
qualification, BIOS/cartridge boot, or source-oracle agreement for the pending
row.

The fixed-zero divider phase, timer-before-CPU same-tick ordering, flat-five
master-tick bus price, grant-time sampling, zero-initialized timer counter, and
disabled page-mode/video/refresh DMA are the recorded software test
environment. They are bound test inputs, not calibrated hardware settings.

## CPU software observations

The previously qualified Plan 08 runs remain the CPU evidence. Each fresh
explicit-profile guest completed with progress `8`, sentinel `a5`, and 18/18
source comparisons. Zeroed and deterministic seeded runs agreed within each
profile. The source-result bytes and completion records were captured under
`build/check/scratch/suites/early-lynx2/build/`; Plan 08's summary records the
complete run evidence.

| Profile | Guest ticks | Probe / marker | Raw source-order results, indices 0–17 | Status |
| --- | ---: | --- | --- | --- |
| `lynx1` | 26,696 | `01` / `a1` | `00 00 52 06 01 02 80 02 30 bb 03 00 ff 00 02 02 00 00` | Complete bounded software observation; 18/18 source expectations agree |
| `lynx2` | 26,841 | `00` / `a2` | `00 00 52 06 01 02 80 02 30 bb 03 00 fe 01 01 01 00 00` | Complete bounded software observation; 18/18 source expectations agree |

These results compare the pinned guest to its pinned software source oracle.
They do not resolve O1 or qualify either hardware profile.

## Timer source, build, and proof identities

The checked-in descriptor is
[`docs/timer-source-index14-pending.json`](../../../docs/timer-source-index14-pending.json).
Its SHA-256 is
`67e5614a922cd851a6dc30b4ee0ff241fe839d4641a32782bf26e61d46c3a2e2`. The
descriptor and positive observation path bind these current source, tool,
wrapper, build, oracle, and environment identities:

| Input | Bound identity |
| --- | --- |
| Pinned `lynx-tests` revision | `b583dc43d532d3c41563a6fa35420614fbcead89` |
| Pinned archive SHA-256 | `a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e` |
| Pinned cc65 archive SHA-256 | `d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1` |
| `timers/tests.s` SHA-256 | `dde10583ae050aba1dcd72f3e0b92cdf2d38d5037c4380c5e8864f2c81702e1f` |
| `timers/main.c` SHA-256 | `b89d4207e837fb4a85b1dcf6b62ffb56c8e62317292b02deeafc22ac02bada3a` |
| Timer wrapper SHA-256 | `fa0973d355eeba6311d9540f3d2ec7d429f6991e1e5641fb36ed7af6c2749e6c` |
| Timer suite build recipe SHA-256 | `a8da249ff0a121a64df8cd3eefe621fd2783515830c4f604db94a919a54735ae` |
| Oracle generator SHA-256 | `138a1075dfd962d918b7951ed0ddff2504a0bb046577c0f380fae6023125e483` |
| Generated CPU/timer oracle SHA-256 | `08b5a4f48caf0f92e7bc177d9f3a6ac8ff68d757b57b9bc346aad8bd4c698b4f` |
| Linked timer BLL SHA-256 | `d766a24031246ad166edc6b417e4d30a841e18dda5115eae201804847ef96c79` |
| Linked timer labels SHA-256 | `b1423949c03badc1158b5cacfb67af146cbe18228cc73f156b11dd548776efa3` |
| Independent cascade evidence SHA-256 | `80c8fbb7fd082ad8f0e5a909e249a243049c2315a9e3a72e8835b4e053f2f2af` |

The bound timer environment is `timer-phase-fixed-zero-1`,
`timer-cpu-poll-order-1`, `flat5` / 5 ticks, sample offset 0, page mode/video
disabled, refresh DMA disabled, and a zero-initialized counter. The records
identify source comparison separately from raw execution and label hardware
accuracy `unverified` and profile qualification `unresolved`.

## Timer observations and all source-comparison rows

Both fresh timer observations include 16 meaningful bytes in an 18-byte
linked result extent, 2 reserved bytes, matching guest model probe/marker,
progress `8`, and the post-return sentinel. Guest bounds are 256 through
50,000,000 ticks.

| Profile | Guest ticks | Probe / marker | Raw results, indices 0–15 | Run-record SHA-256 | Cascade-proof SHA-256 |
| --- | ---: | --- | --- | --- | --- |
| `lynx1` | 359,941 | `01` / `a1` | `08 7f 00 08 00 01 08 00 04 08 00 36 00 01 0c 08` | `9650aa87ee72d3b6a3e2bc1c12f76d5193095674555e42d52e5abcb850d10c2b` | `bc152d739e5c0660a95758a3823dfa1bf4ae262fdc8ca4757b5fb7875efd1a0a` |
| `lynx2` | 359,916 | `00` / `a2` | `08 7f 00 08 00 01 08 00 04 08 00 37 00 01 0c 08` | `c1baf6d1db1e12dc4b471abc535b0eff17da6d03b03a54d2d21fa04d4cd36e0b` | `d25369b647e4782ed226f259b329cb613bea87d3e156c5ea11adea2d1ee61fc8` |

The source expectations below come from the unchanged pinned oracle. All rows
are present in each comparison record; only index 14 is pending.

| Source index | Unchanged source set | Lynx1 actual | Lynx2 actual | Result |
| ---: | --- | --- | --- | --- |
| 0 | `08` | `08` | `08` | agree |
| 1 | `7f` | `7f` | `7f` | agree |
| 2 | `00` | `00` | `00` | agree |
| 3 | `08` | `08` | `08` | agree |
| 4 | `00` | `00` | `00` | agree |
| 5 | `01` | `01` | `01` | agree |
| 6 | `08` | `08` | `08` | agree |
| 7 | `00` | `00` | `00` | agree |
| 8 | `04`, `05` | `04` | `04` | agree |
| 9 | `08` | `08` | `08` | agree |
| 10 | `00` | `00` | `00` | agree |
| 11 | `35`, `36`, `37` | `36` | `37` | agree |
| 12 | `00` | `00` | `00` | agree |
| 13 | `01` | `01` | `01` | agree |
| 14 | `0d` | `0c` | `0c` | D-10 pending |
| 15 | `08` | `08` | `08` | agree |

Each profile therefore has 15 agreements and exactly one approved pending
comparison. A different index-14 value, a second disagreement, incomplete or
stale execution evidence, wrong model/count/time, or descriptor/identity drift
is a failure; none is covered by D-10.

## Independent fixed-loop proof

The executable `suite.timers.cascade_model` check verifies the unchanged
Test 6 setup and poll opcodes, operands, addressing modes, and branch targets.
It independently derives 14-cycle no-IRQ and 24-cycle IRQ paths, so the
source loop polls every 70 or 120 master ticks; the Timer 3 reload period is
1,088 ticks. Starting from each of the 64 possible first-edge delays, it
simulates the source-declared 192 polls. Every phase yields 12 acknowledgments
and final poll offset 13,990; the thirteenth expiry is later than that final
poll. This matches the actual guest's result `0c` in both profiles under the
bound flat-five/no-DMA model. It does not identify the physical cause of the
source's `0d` expectation.

The generated proof binds to the current linked BLL, labels, raw results, run
record, comparison, oracle, wrapper, and pending descriptor. The report
[`timer-suite-cascade-count.md`](../../debug/evidence/timer-suite-cascade-count.md)
records the independent timing derivation and unresolved upstream
preconditions. It is software proof, not hardware qualification. No pin,
assembly expectation, wrapper layout, zero-page counter initialization, bus
cost, phase policy, or production timing was tuned to change the comparison.

## Descriptor, records, CLI, and control inventory audit

Descriptor schema `lynxplosion.timer-source-pending` version 1 names D-10,
EVD-02, suite `timers`, source index 14, actual `0c`, expected set `0d`, both
profiles, and Phase 5 revisit. Its artifact identities bind the pinned
revision/archive, cc65 pin, both timer source files, source oracle, wrapper,
build recipe, oracle generator, proof report, and every recorded model and bus
environment field. The profile companion records use schema
`lynxplosion.timer-source-pending-record` version 1 and bind raw, comparison,
run record, BLL, labels, oracle, descriptor, and proof hashes. The independent
proof records carry all 64 phase rows and the same source/guest identities.

The normal `run timers PROFILE` command remains strict: it returns exit 1 with
`suite: timer source oracle comparison mismatch` after a valid completed guest
when the source comparison is the one known mismatch. The descriptor-bound
`observe timers ...` path can succeed only for the exact pending row with 15
agreements and complete, identity-matched guest evidence. A successful
observation is not a strict source pass.

The eight timer CTest registrations in `tests/suites/tests.cmake` are present
exactly once in `tests/inventory/fast.txt`:

| Role | Registered IDs |
| --- | --- |
| Positive guest observations | `suite.timers.observation.lynx1`, `suite.timers.observation.lynx2` |
| Strict comparison and independent proof | `suite.timers.strict_comparison_controls`, `suite.timers.cascade_model` |
| Model, pending, result, and no-run controls | `suite.timers.model_controls`, `suite.timers.pending_controls`, `suite.timers.result_controls`, `suite.timers.no_run` |

The inventory bytes hash to
`4c96587c3303c64aaa14874597cc173288eaf155dce9edca465aa3c3893034`; this is
the hash currently bound by
[`skeleton-repeatability.claim`](../../../docs/claims/skeleton-repeatability.claim).
That claim remains specifically about repeatability of the `none` CPU
churn-loop workload; timer test results do not alter its subject.

The seven-test Task 1 integration command was:

```sh
cmake --preset check
cmake --build --preset check
ctest --preset check --no-tests=error --output-on-failure \
  -R '^suite\.timers\.(observation\.(lynx1|lynx2)|strict_comparison_controls|cascade_model|model_controls|result_controls|pending_controls)$'
```

At source checkout `0c4ff91c25261644cbca2913af3e8f60c1d1cab6`, CMake
configured successfully with AppleClang on Darwin arm64, the build completed,
and all seven selected tests passed in 50 seconds wall time. The strict
mismatch control passed by observing the exact expected nonzero result and
diagnostic; that does not make the timer suite pass. The registered
`suite.timers.no_run` control was inspected in the inventory and retains its
prior Plan 10 evidence; it is not in this Task 1 regex. The full local check,
Linux 24.04 qualification, review fixes, independent pending-gate audit, and
CI against the final reviewed head have not yet run.

## Open evidence and owner gates

The parent review checkpoint is complete; see
[04-PARENT-CHECKPOINT.md](04-PARENT-CHECKPOINT.md). Implementation findings
are repaired, coverage and UI applicability are reconciled, and the bounded
[D-10 verifier](04-PENDING-GATE-VERIFICATION.md) passed. Plan 12 Task 2's
exact-source macOS and self-contained Ubuntu gates subsequently passed; exact-
head CI remains pending root review. The current live descriptor identity is
recorded in the final qualification section below. Hashes in the Task 1 tables
remain historical observations.

- Global EVD-02 remains open. Phase 5 must establish the relevant initial
  state, page-mode and bus/video prerequisites using independent evidence,
  then repeat the unchanged timer comparison. If the evidence does not justify
  `0d`, retain the mismatch; do not seed the counter or retune timing/layout.
- Recurring EVD-05 remains globally open for the future DMA/audio slices. The
  Phase 4 timer/IRQ observations do not close it.
- O1, O5, O6, O9 and the existing rights, physical-test, publication, and
  release gates remain unchanged. No firmware, BIOS, commercial content,
  hardware test, licence decision, or release is implied or authorized here.
- The plan's descriptor-less prohibitions remain unresolved/unverified until
  their required descriptor or owner evidence exists.
- Root completed the full-phase code/security/coverage review, UI applicability,
  accepted fixes and targeted retests, and independent pending-gate acceptance.
  Exact-head CI and any phase-completion record remain pending; passing local
  gates do not imply hardware/profile qualification.

## First Task 2 attempt and bounded repair

At `1764ad1` the canonical macOS command exited 1 at artifacts after 163/163 tests
and exact JUnit accounting with no skips. The workflow took 79s (CTest 78.48s);
the whole attempt took 349s, with about 266s spent in the artifact scan. There
were 1857 findings among 10217 scratch files (59MB): 542 current-run generated
outputs plus prior retained scratch. These include generated BLL/object files
and intentional nonproduction text fixtures. History hygiene did not run, so
this attempt does not qualify the source. Linux was not redundantly run on the
known failing source.

Root restored successful-run cleanup with a fresh canonical path/nonce-marker
check and explicit missing/stale/ancestor refusal controls. The four affected
CTest checks pass and leave no suite files; prior generated scratch was moved
reversibly to private temporary evidence. The artifact policy remains intact.
Renew final source qualification after this repair; do not reuse the failed
attempt as successful local evidence.

## Cross-platform shell portability repair

The renewed macOS gate at `5719e3a` passed completely:163 tests, exact JUnit,
no skips, artifact scan5s, history47s, total125s. This remains successful
historical evidence for that source. The self-contained Ubuntu24.04.5 arm64
checkout had266 commits, no shallow boundary or alternates, GCC13.3 and
ShellCheck0.9.0. Its gate stopped at ShellCheck before the suite workflow:
SC2015 rejected compound `A && B || refusal` shell expressions. The initial
container wrapper attempt stopped before clone/check due to a reporting-line
expansion error and is not a project qualification attempt.

Root replaced the nine compound expressions with explicit conditional
refusals, preserving their conditions/statuses and the strict lint gate.
Recipe/generator hashes in the pending descriptor and corresponding exact
mutation anchors are rebound to the changed script bytes. The oracle contents,
source pin, wrapper, source expectations, cost/phase/counter environment and
linked-code timing proof are unchanged. Syntax checks, local ShellCheck and
four affected build/oracle/both-profile tests passed in 4.91s. Full local and
Linux qualification was still required on this committed source; prior macOS
success did not substitute for that exact-source run. Results follow.

## Final exact-source qualification (Plan 12 Task 2 local gates)

Both canonical `sh scripts/check.sh` runs passed the reviewed source
`7616f6d1d12b98eac6cf4d4e979ac1c90f0e01ac`. The current pending descriptor is
`676b711e3e6cd4258fafb9191473cc531b8daee659dc10ee3a0767429764dacd` after the
portable recipe/oracle-generator identity rebind. The source oracle, timer
expectations, execution environment, and D-10 admission/proof behavior did not
change. The strict timer comparison still returns exit 1 for the exact index-14
`actual 0c / expected 0d` row in both profiles; the separate bounded observation
path remains the sole approved exception.

| Environment | Tool identities | Inventory and outcome | Measured `check.sh` steps |
| --- | --- | --- | --- |
| macOS Apple Silicon (`Darwin-arm64`) | Apple clang 21.0.0.21000101; CMake 4.4.3; Ninja 1.13.2; ShellCheck 0.11.0; Git 2.41.0 | 163/163 tests; JUnit accounted for all 163 exactly once; 0 declared skips; artifact scan passed at 6,346 files; history hygiene passed | identities 0s; pins 0s; cached cc65 0s (built in 2s); probe/fixtures 0s; ShellCheck 2s; CMake workflow 64s including 63.80s test time; JUnit 0s; artifacts 5s; history 41s; total 112s |
| Ubuntu 24.04.5 arm64 (`aarch64`) | GCC 13.3.0; CMake 3.28.3; Ninja 1.11.1; ShellCheck 0.9.0; Git 2.43.0; ca-certificates `20260816ubuntu0.24.04.2` | 163/163 tests; JUnit accounted for all 163 exactly once; 0 declared skips; artifact scan passed at 6,322 files; history hygiene passed. The container used a full clone: 267 commits, not shallow, no object alternates. | identities 0s; pins 2s; cc65 build 5s; probe/fixtures 0s; ShellCheck 2s; CMake workflow 54s including 52.90s test time; JUnit 0s; artifacts 2s; history 10s; total 75s |

The Linux run passed ShellCheck 0.9.0 after the nine compound-refusal
expressions were rewritten as explicit conditionals. The test command was run
in a self-contained `ubuntu:24.04` arm64 container cloned from the full source
history at the exact commit above. Container package installation and clone
setup occurred before `check.sh` and were not separately timed by the check
runner. Linux raw output is retained outside the repository at
`/private/tmp/lynxplosion-phase04-final-linux-7616f6d-retry.log`; macOS output
was streamed through the managed session and its summary is recorded above.

The earlier ShellCheck wrapper setup failures and the first artifact-retention
failure remain historical failure records above; neither substitutes for these
passing exact-source results. CI against this exact head has not run. Root
review of the final ledger and the existing `origin/main` push/CI handoff are
the next actions. EVD-02, EVD-05, O1, the strict timer mismatch, hardware/profile
qualification, rights, physical-test, publication, and release gates remain
open as described above.

## External hosted CI blocker

The reviewed `f920844` head is pushed through the existing origin/main channel.
Its exact run43096967098 remains queued with zero jobs after a declared GitHub
Actions outage and a public backlog-drained/resolved update. One cancellation
attempt was refused as already completed; one standard same-run rerun was
refused as already running. Fresh APIs still show queued/attempt1/no jobs.
No CI result exists to qualify or inspect. Plan12Task2 and phase completion
remain pending; full local results above remain valid for their exact source.
See [04-CI-BLOCKER.md](04-CI-BLOCKER.md) for official incident evidence, bounded
recovery and the resume procedure. No gate was waived and no replacement push
was used to bypass the inconsistent run.
