---
phase: 07-suzy-math-and-register-mirrors
audited_source: 3b50de6 (initial audit at 612c1e4; T-07-03 re-audited after b287750)
audited: 2026-11-14
asvs_level: 1
block_on: high
threats_total: 17
threats_closed: 17
threats_open: 0
verdict: SECURED
auditor: gsd-security-auditor (read-only; no source modified, no check gate run)
---

# Phase 07 security audit: Suzy math and register mirrors

## Scope and method

This audit checked every threat ID in the `<threat_model>` blocks of plans
07-01 to 07-04 (T-07-01..T-07-16, plus the shared T-07-SC) against the code at
HEAD `612c1e4`. No SUMMARY has a `## Threat Flags` section, so there are no
executor flags to map. Verification is at ASVS L1 (is the control present in
the file named by the plan?). Where a control was present but a concrete bypass
was found and reproduced, the threat is marked OPEN.

The REVIEW repairs were checked statically against the source:

- **CR-01 holds.** `tests/suites/test_public_suites.c:818` now reads
  `span == 0u || start >= meaningful || span > meaningful - start`, so the
  addition can no longer wrap before `covered[suite][start]` is indexed
  (lines 830-834). `tests/suites/run.cmake:2519-2531` adds a regression
  control. It seeds `start=4294967295 span=1`, fails if the seed was not
  applied, and requires exit 1 with the named "malformed group row"
  diagnostic (`expect_failure`, run.cmake:200-207).
- **WR-01 holds.** When the completion sentinel fires, the observer reads PC
  and S (test_public_suites.c:4352-4358). It requires PC to equal the linked
  `suite_idle` and S to equal `$FF`. `source_symbol_layout` (4010-4049) checks
  the linked JSR/RTS, the post-return stores and the idle-loop bytes, with
  bounded offsets. The independent reader re-derives these values from the BLL
  and labels (4408-4450). The fixed `guest_return=` claim has been removed.
  The controls `wrong_runtime_pc` and `missing_runtime_stack` run at
  run.cmake:2606/2640.
- **WR-02 holds.** run.cmake:2695-2851 now has eight strict subcases. Two
  independently change timing bytes 14 and 15. Six rehashed hostile records
  cover omitted timing, timing relabelled as reserved, a missing strict
  verdict, a stale count, a dropped mismatch row and a scoped pass promoted to
  a whole-suite pass. A clean positive record brackets each case. Each mutation
  fails closed: a no-op mutation leaves a valid record, which makes
  `expect_failure` abort. The exact case list is compared at 2844-2846. The
  strict reader recomputes everything from the raw bytes and the oracle
  (test_public_suites.c:4525-4609, 4701).

## Per-threat verification

| Threat ID | Category | Severity | Disposition | Status | Evidence / gap |
|---|---|---|---|---|---|
| T-07-01 | Tampering | high | mitigate | CLOSED | SPRSYS validation is done once, before any mutation. `core/machine.c:392-406`: mask `0xCD` rejects `0x32` before `lynx__input_write` or the mode changes. `core/input.c:63-70`. Shared decode for opcode peek, availability, bus access and public peek: machine.c:420-451, 475-476, 561-589, 1215-1230. Busy and command refusal before any write: `core/suzy.c:187-201`. Holes FC30..3F and FC70..7F: suzy.c:124-135. Tests: `tests/api/test_api.c:8890-8906` (`unsupported-sprsys-control-refuses-before-device-mutation`); test_api.c:1583-1600 (FDFF..FE00 span under MAPCTL=02 leaves caller bytes unchanged; FC30/FE00/FFF9/FFFA unavailable); `tests/suzy/test_suzy.c:698-699` (FC30/FC70 refusal with no hidden-RAM fallback). |
| T-07-02 | Tampering | high | mitigate | CLOSED | 88-byte fieldwise record: suzy.c:344-396, `core/internal.h:38`, static assert `core/snapshot.c:125`. Full candidate validation (enums, flag ranges, idle fields zero, latches redundant with backing, deadline equals start plus duration within headroom, start <= ticks <= deadline): suzy.c:298-342. Validation before any apply: snapshot.c:860-864, then apply at 866-889. Tests: test_suzy.c:984-1027 (seven rehashed forged candidates, each checked byte-for-byte against the untouched instance); test_suzy.c:1033-1053 (snapshot at the deadline restores); test_api.c:3618-3640 (formats 1-7 refused). |
| T-07-03 | Denial of service | high | mitigate | CLOSED (repaired b287750) | **Original defect, at 612c1e4:** the arithmetic sample-only prefix was bounded by `next.tick` before the Suzy candidate was considered. A coarse advance could step past a pending deadline and leave the operation busy forever with an unrestorable snapshot. I reproduced this under `VIDEO_POLICY1`. **Repair:** the Suzy candidate is now selected at `core/machine.c:698-711`, before the CPU event (712-725) and before the sample prefix bound (726-739, `before_tick = next.tick`). Because of that, `sampled_through < before_tick <= suzy deadline`. If a sample falls on the deadline tick, it still runs first (priority 4 < 5), as it did before the fix. Selection uses the strict total order `lynx__machine_event_before` (tick, priority, sequence; machine.c:597-606), so the result does not depend on the order candidates are inserted. All public advance entry points (machine.c:930, 1002, 1085) share this one loop. Every other guest-tick change moves either to `next.tick` (788, 802, 808, 818, 829, 834), to `target` when nothing is pending before it (753), or to `grant+1` after a watch (918). A Suzy deadline at or before the grant wins over the CPU (priority 5 < 6), and a later deadline is at least `grant+1`, so none of these can pass a pending deadline. Snapshot restore (snapshot.c:880) requires `ticks <= deadline` (suzy.c:338). **Regression:** `suzy.coarse_deadline` (test_suzy.c:1404-1464; tests.cmake:14; fast.txt:250) sweeps 0-400 NOPs under both policies and asserts completion and own-snapshot restore. My scratch repro now finds 0 stuck cases under either policy (it found 5 before the fix). The coordinator reports this test red before the fix and green after, and 3b50de6 passes native canonical 271/271 and the GNU ASan/UBSan focused groups 142/142. I took those runs from the coordinator's report; I did not rerun them. |
| T-07-04 | Repudiation | medium | mitigate | CLOSED | Digest domain `lynxplosion.guest-state.v4` with the Suzy record: snapshot.c:319, 357, 372. Golden rebound in commit `b1b67ac`. Sensitivity: `tests/det/test_det.c:1042-1073` (every latch and flag). Inventory: `tests/inventory/fast.txt:161-162, 249-259`. Copied recipes include `core/suzy.c`: `tests/{timers,audio,video}/fault_controls.cmake` (146/236/234) and `CMakeLists.txt:34`. Independent diag reader: `tests/diag/test_diag.c:23-25`. Native/GNU equality is recorded in 07-01-SUMMARY:46, 163. The slicing dependence under `VIDEO_POLICY1` found in T-07-03 is repaired, and `suzy.coarse_deadline` is now in the inventory that the claim was rebound to in 3b50de6. |
| T-07-05 | Tampering | high | mitigate | CLOSED | Unsigned arithmetic only: product `uint32*uint32`, negation `0u - product`, `uint64` accumulator sum, divide-by-zero guarded (suzy.c:259-281); sign conversion at suzy.c:93-106. Independent `uint64` model and hand-checkable sign, overflow and divide vectors: test_suzy.c:441-620. |
| T-07-06 | Denial of service | high | mitigate | CLOSED | Busy writes refused through both views before any state change: suzy.c:187-195; test_suzy.c:1297-1370. Reset and load cancel the operation: test_suzy.c:1239-1290. Half-open target and zero-budget behaviour: test_suzy.c:740-789. Headroom: test_suzy.c:1379-1398. The lost-completion bypass is tracked under T-07-03; a host reset or load still recovers. |
| T-07-07 | Tampering | high | mitigate | CLOSED | Rehashed forged candidates are refused without any change to the instance (test_suzy.c:984-1027). A snapshot at the deadline restores (1033-1053). Continuation suffix matches: test_suzy.c:1107-1137 (`suzy.snapshot.continuation-suffix`). Apply is all-or-nothing: snapshot.c:866-889. |
| T-07-08 | Repudiation | medium | mitigate | CLOSED | `tests/suzy/fault_controls.cmake:94-173, 207-214`: source manifest SHA, binary SHA plus inode before and after, clean positive runs bracketing the mutant, a nonce-named owner file, and identity checks on the copied non-mutant files. |
| T-07-09 | Tampering | high | mitigate | CLOSED | `scripts/build-suites.sh`: math and memio `tests.s`/`main.c` are pin-verified before and after the build; a change during the build is refused. `--verify` checks every wrapper, source, main, object and label record. `scripts/generate-suite-oracle.sh:329-354`: canonical tree, pin marker, and per-file pin hashes including LICENSE and the math/memio files. `tests/gates/math-script-controls.sh:59-178`: 14 shape, identity, tool, notice and reserved-range negatives with an exact case manifest. |
| T-07-10 | Spoofing | high | mitigate | CLOSED | Fresh public-API machine, build identity verified before the run, progress, sentinel and runtime PC/S check: test_public_suites.c:4217-4372 (4352-4358). The independent record reader recomputes the return boundary (4408-4450). Controls: no-run, replaced prior outputs, missing raw byte, wrong profile, lowered bound, missing progress, wrong runtime PC, missing stack (run.cmake:2606, 2640). |
| T-07-11 | Repudiation | high | mitigate | CLOSED | All 18 raw bytes, an independent strict evaluator that writes every mismatch row and recomputes count and verdict (test_public_suites.c:4525-4609, 4701), and the WR-02 hostile-record controls (run.cmake:238-330, 2695-2851). |
| T-07-12 | Denial of service | medium | mitigate | CLOSED | Finite guest window: `SOURCE_TICK_MAX=1000000` (test_public_suites.c:41-42, 4306-4313). Host CTest TIMEOUT 180/240: `tests/suites/tests.cmake:57-64`. Lowered-bound and no-completion controls are in run.cmake:2606. |
| T-07-13 | Tampering | high | mitigate | CLOSED | `cleanup_suite_run` (run.cmake:31-84): no symlink ancestors, canonical real path, prefix containment, 48-hex nonce, owner marker revalidated. It runs only on success (run.cmake:3962-3966); a FATAL failure leaves the work directory in place. Script controls use a `mktemp -d` directory under the canonical scratch root (math-script-controls.sh:4-8). |
| T-07-14 | Tampering | high | mitigate | CLOSED | The independent review exists (07-REVIEW.md). Fixes for CR-01 (0fe0734), WR-01 (7848e48) and WR-02 (612c1e4) are in the owning files and verified statically (see above). The new high defect found by this audit (T-07-03) is repaired in b287750 with an owning regression test. The claim was rebound in 3b50de6. Post-fix evidence at 3b50de6, as reported by the coordinator: native `check.sh` 271/271, artifacts and history clean; GNU gcc 13.3 `-O2 -g -UNDEBUG` ASan/UBSan focused groups 142/142; ShellCheck 0.9 clean. No high or critical review or security finding remains unresolved. Remaining bookkeeping: 07-REVIEW.md frontmatter should link these fix commits. |
| T-07-15 | Spoofing | high | mitigate | CLOSED | Exact-source evidence at 3b50de6: native canonical 271/271, GNU focused groups 142/142 with ShellCheck 0.9, and successful hosted CI run 43612316517 at that exact pushed SHA, recorded in 07-QUALIFICATION.md. |
| T-07-16 | Repudiation | high | mitigate | CLOSED (current claim surface) | `docs/public-suites.md:30-83` keeps raw bytes, scoped and strict counts, and states that this is not physical accuracy or SUZ-02. Run records hard-code `hardware_accuracy=unverified` and `profile_qualification=unresolved`, and the reader enforces them (test_public_suites.c:4619). `.planning/REQUIREMENTS.md` keeps EVD-02, SUZ-02 and SMR-01..04 pending. Re-check when 07-QUALIFICATION.md is written. |
| T-07-SC | Tampering | high | mitigate | CLOSED | `git diff 152f784..612c1e4` shows no change to `pins.txt`, `.github/`, or any package or lock manifest. No new install or repin. The cc65 and lynx-tests pins are unchanged. |

## Unregistered flags

None. No SUMMARY declares `## Threat Flags`.

## Informational (non-blocking, not threat closures)

- `tests/gates/math-script-controls.sh:156-158` edits the canonical build-tree
  math oracle in place and never restores it. Restoration depends on
  run.cmake:2490-2495 regenerating the oracle afterwards. If the script is
  interrupted, a corrupted oracle is left behind. Later validators fail closed,
  so this is a hygiene issue, not a bypass.
- The CR-01 regression control links against a hard-coded
  `${ROOT}/build/check/liblynx_core.a` (run.cmake:2512) instead of the
  active build directory's library. A different binary dir could link a stale
  core.
- `generate-suite-oracle.sh` accepts `--include-audio` and
  `--include-math-memio` together and silently emits only the math/memio form.

## Unresolved items

None blocking. T-07-15 closed after hosted CI run 43612316517 passed at the
exact pushed source (07-QUALIFICATION.md). The two non-blocking observations
above (hard-coded `build/check/liblynx_core.a` in the CR-01 control; combined
oracle-generator flags) are recorded for later cleanup.

This audit makes no hardware-accuracy claim. It does not close SUZ-02 or
EVD-02 and gives no release authority.
