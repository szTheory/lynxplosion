---
phase: 02-cpu-profiles-and-per-opcode-vectors
verified: 2026-11-10T00:00:00Z
status: passed
score: 4/4 roadmap success criteria verified (5/5 requirements satisfied)
covered_files:
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-01-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-01-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-02-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-02-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-03-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-03-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-04-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-04-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-05-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-05-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-06-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-06-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-07-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-07-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-08-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-08-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-09-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-09-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-10-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-10-SUMMARY.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-11-PLAN.md"
  - ".planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-11-SUMMARY.md"
  - "core/bus.c"
  - "core/bus.h"
  - "core/cpu.c"
  - "core/cpu.h"
  - "core/cpu_profile.c"
  - "core/cpu_profile.def"
  - "core/machine.c"
  - "docs/cpu-profiles.md"
  - "scripts/check-claims.sh"
  - "scripts/vectors-full.sh"
  - "tests/api/test_api.c"
  - "tests/cpu/test_cpu_scenarios.c"
  - "tests/cpu/test_cpu_vectors.c"
  - "tests/golden/skeleton.digest"
  - "tests/inventory/vectors/lynx1.txt"
  - "tests/inventory/vectors/lynx2.txt"
  - "tests/inventory/vectors/none.txt"
covered_digest: "v4:sha256:5e51fb59775501a5b6055190714f1f09c023051769bf194168d1c2b3d5b75430"
behavior_unverified: 0
overrides_applied: 0
flagged_prohibitions:
  - statement: "No emulator source was opened (clean-room, O6 DRAFT)"
    tier: judgment
    status: "unverified-prohibition: human review recommended, pending owner decision O6"
advisory:
  - finding: "Two vector-control tests (cpu.vectors.malformed, cpu.vectors.complete_mode) fail when the repo sits under a very long path (about 170 characters), because a diagnostic line is truncated before the expected regex text. Not reproducible at the real path or on CI."
    category: other
    reason: "Found while running seeded faults in a scratch clone under a long scratchpad path. Environment fragility, not a goal gap."
    evidence_status: "reproduced in the scratch clone on an unmodified tree"
  - finding: "The tick stamp passed to bus_access in core/machine.c is plumbed (grant plus sample offset) but discarded; no Phase 2 test observes a per-cycle stamp, only the consumed-tick totals and chained budget slices."
    category: other
    reason: "By design (CONTEXT: no device observes the stamp before Phase 4). Revisit when the first device consumes it."
    evidence_status: "code read"
  - finding: "HEAD 4c91a83 is local and unpushed. It differs from the pushed, CI-green 8f1fd8f only in .planning/phases/02-*/02-VALIDATION.md."
    category: other
    reason: "Code tree is identical to the green run 42773404969 (check job success)."
    evidence_status: "git diff 8f1fd8f 4c91a83 -- . ':!.planning' is empty"
---

# Phase 2: CPU Profiles and Per-Opcode Vectors Verification Report

**Phase Goal:** The 65C02-class CPU executes correctly under data-defined Lynx I and Lynx II profiles, proven by pinned per-opcode vectors, without either profile being adopted as baseline.
**Verified:** 2026-11-10
**Status:** passed (with one flagged judgment-tier prohibition pending owner decision O6)
**Re-verification:** No, initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria are the contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Pinned per-opcode vectors run in the check for every defined profile, with an exact per-profile inventory (included, excluded, reason) | VERIFIED | Independent `sh scripts/check.sh` exit 0, "94 tests accounted for once each (0 declared skips)". `cpu.vectors.lynx1/lynx2/none` each print "plan matches the committed inventory". lynx1: 256 files, 254 run, 2 excluded, 8192 vectors, asserted 8128, passed 8128, failed 0. lynx2: identical counts. none: 222 run, 2 excluded, 32 undefined, asserted 7104, failed 0. Inventories `tests/inventory/vectors/{none,lynx1,lynx2}.txt` have 256 rows each; every exclusion carries a reason (`$CB`/`$DB`: `source-disagrees-with-hardware-informed-test`); state-tier rows carry reason labels (`shape-unsourced-for-lynx1` 32 on lynx1, `cycles-unsourced-decimal` 18, `cycles-conflict-sources` 5). `profile-none-stops-undefined` 32 rows on none. |
| 2 | A host selects the profile per instance; RMB/SMB/BBR/BBS follow the profile's data; the wrong-profile control fails the bit-op vectors | VERIFIED | `core/cpu_profile.def`: one row per opcode, 224 `OP` (shared) + 32 `OPX` (lynx1 half then lynx2 half), `_Static_assert` pins 224/32/256. `lynx_config.profile` validated in `lynx_machine_init`; reset keeps it (`machine_setup` copies cfg). `cpu.vectors.wrong_profile` and `wrong_profile_reverse` each report "failing set (32 opcodes): 07 0f 17 ... ff ... exactly the 32 bit-operation opcodes fail: ok". Own seeded fault (below) confirms a lynx2-only row change is caught by exactly the lynx2 tests. `api.profile_select`, `cpu.profile_divergence`, `det.profile_agreement` pass. |
| 3 | Every CPU memory access is a tick-stamped bus cycle; a non-default per-access cost changes consumed ticks without changing architectural results | VERIFIED (see advisory on stamp observability) | `core/cpu.c` has no RAM access and includes only `stdbool.h` and `stddef.h` (gate `core.cpu_includes`). The CPU posts `req_addr/req_data/req_kind/req_flags`; `lynx_advance` in `core/machine.c` prices (`lynx__bus_timing_of(cost_model, region, kind)`), performs via `bus_access(..., grant + sample)`, completes, and sets `cpu_ready_at`. `api.cost_model`: sum-trap is 105 cycles; flat5 525 ticks = 105*5; test_skew total equals the test's own price table summed over the seam log; registers, CPU position and 64 KiB RAM are `memcmp`-identical across models; RAM equals the replay of the logged writes. `api.advance_slicing` (chunked budgets equal one call) and `api.seam_agreement.{none,lynx1,lynx2}` (run_rows 222/254/254, failed 0) pass. |
| 4 | IRQ entry, CLI/SEI latency, WAI/STP, reserved-opcode NOP lengths follow the profile; each expected value cites a source or is marked unverified | VERIFIED | `cpu.scenarios.lynx1`: 78 scenarios, 0 mismatches; `cpu.scenarios.lynx2`: 99 scenarios, 0 mismatches. Tag counts lynx1 "LT-HW 7 WDC 18 RKW 0 CMD 2 SST-MODEL 45 INFERRED 6 unverified 71"; lynx2 "LT-HW 10 WDC 18 RKW 50 CMD 2 SST-MODEL 13 INFERRED 6 unverified 89". The runner refuses a missing or unknown tag; grep finds 0 scenarios with a non-LT-HW tag and `unv = 0`, and 0 LT-HW with `unv = 1`. WAI/STP: absent from both tables (`$CB`/`$DB` are 1-byte no-poll NOPs, `docs/cpu-profiles.md` states no WAI/STP, `LYNX_STOP_HALT` reserved); Test 8 replica `irq.test8.replica` and `irq.test8.block32` (LT-HW) pass, with `cpu.scenarios.test8.self_test` (fault `cb-poll` makes replica and block32 mismatch). Reserved-NOP lengths: `nop.*` and `lynx1.nop_xx` scenarios. Unverified facts are listed in `docs/cpu-profiles.md` ("Not covered by the vectors, and unverified for the Lynx"). |

**Score:** 4/4 truths verified, 0 present-but-behavior-unverified. No behavior-dependent truth rests on symbol presence: each has a named passing test (cost_model, advance_slicing, scenarios, vectors).

### Seeded-fault evidence (scratch clone of HEAD outside the repo, no repo file touched)

| Fault | Failed tests | Unaffected |
|---|---|---|
| `OP(0x09, ORA, IMM)` changed to AND (shared row) | `cpu.vectors.lynx1`, `lynx2`, `none` | scenarios, tables |
| lynx2 half of `$87` changed SMB to RMB (profile-specific row) | `cpu.vectors.lynx2`, `cpu.scenarios.lynx2`, `api.profile_select` | `cpu.vectors.lynx1`, `cpu.vectors.none`, `cpu.scenarios.lynx1`, `det.profile_agreement` |
| CPU polls interrupts after the micro-op instead of before (latency fault) | `cpu.scenarios.lynx1`, `lynx2`, `cpu.cycles` (vectors correctly do not assert interrupts) | `cpu.vectors.lynx1/lynx2/none` |
| `lynx_advance` prices every cycle with cost model 0 (cost model ignored) | `api.cost_model`, `api.advance_slicing` | all vector tests |

Each control caught the fault that matches its claim. (`cpu.vectors.malformed` and `complete_mode` also failed in that clone for the path-length reason in the advisory list, independent of the faults.)

### Required Artifacts

| Artifact | Status | Details |
|---|---|---|
| `core/cpu_profile.def`, `core/cpu_profile.c` | VERIFIED | data-defined tables, count asserts, `lynx__cpu_opcode_defined` implements the none rule |
| `core/cpu.c`, `core/cpu.h` | VERIFIED | 913-line cycle-stepped CPU, no memory or time access, include gate |
| `core/bus.c`, `core/bus.h` | VERIFIED | const cost tables, flat5 (PLACEHOLDER) and test_skew |
| `core/machine.c` | VERIFIED | budget loop with grant/price/perform/ready_at; profile per instance; UNDEFINED stop |
| `tests/inventory/vectors/*.txt` | VERIFIED | compared byte for byte to runner output (`cpu.vectors.inventory_drift`) |
| `tests/cpu/test_cpu_scenarios.c`, `test_cpu_vectors.c`, `tests/api/test_api.c` | VERIFIED | substantive, wired into 94-test inventory |
| `docs/cpu-profiles.md`, `scripts/vectors-full.sh` | VERIFIED | profile document with source tag table; full lane script read-verified |

### Key Link Verification

| From | To | Via | Status |
|---|---|---|---|
| `lynx_config.profile` | `lynx__cpu_table` | `lynx__cpu_init(m->cfg.profile)`, `lynx_advance` | WIRED |
| CPU request | tick-stamped cycle | `bus_access(..., grant + price.sample)`, `cpu_ready_at = grant + price.ticks` | WIRED (stamp consumed by no device yet) |
| cost model | consumed ticks | `lynx__bus_timing_of(m->cost_model, ...)`; seeded fault shows `api.cost_model` fails when ignored | WIRED |
| inventories | runner | `--expect-plan` | WIRED |
| golden digest | `det.golden`, `det.profile_agreement` | digest `506f79956f05bbb58bb2c6e759046836e5b40de0f499bb6af52bcf288e2bad3c` equals `tests/golden/skeleton.digest`; `det.profile_agreement` prints the same value under none, lynx1, lynx2 | WIRED |

### Behavioral Spot-Checks and Full-lane Consistency

| Check | Result | Status |
|---|---|---|
| `sh scripts/check.sh` | exit 0; 94/94 pass; total 69 s | PASS |
| `ctest -R '^cpu\.vectors\.'` | 14/14 pass | PASS |
| Full lane (02-11-SUMMARY, not rerun, 2 GB download) | none 222 files asserted 2,220,000; lynx1 254 files 2,540,000; lynx2 254 files 2,540,000; total 7,300,000 passed, 0 failed. File counts equal the committed inventories (222/254/254 run, 2 excluded each) and `x 10000` per file. Cross pairings fail 159,383 and 160,136 vectors on exactly the 32 `$x7/$xF` opcodes. `vectors-full.sh` run mode requires zero same-profile failures and the exact 32-opcode failing set on both cross runs (lines 19-24, 262-283, 378-383) | CONSISTENT |
| CI | run 42773404969 on pushed `8f1fd8f` (latest code-affecting push), job `check` success. Unpushed HEAD `4c91a83` differs only in a `.planning` doc | PASS |

### Requirements Coverage

| Requirement | Source Plans | Status | Evidence |
|---|---|---|---|
| CPU-01 | 02-02, 02-03, 02-04, 02-05, 02-11 | SATISFIED | Truth 1 |
| CPU-02 | 02-01, 02-04, 02-06, 02-07, 02-09, 02-10, 02-11 | SATISFIED | Truth 2 |
| CPU-03 | 02-01, 02-07, 02-08, 02-09, 02-10, 02-11 | SATISFIED | Truth 3 |
| CPU-04 | 02-01, 02-06, 02-09, 02-11 | SATISFIED | Truth 4 |
| CPU-05 | 02-05, 02-10, 02-11 | SATISFIED | wrong-profile and reverse controls, 32 opcodes exactly |

All five IDs appear in plan frontmatter and in REQUIREMENTS.md (marked complete, traceability rows Phase 2). No orphaned Phase 2 requirement.

### Owner-gate and Phase 1 Property Checks

- O1: `ALLOWED_PROFILES` defaults to `none` in `scripts/check-claims.sh`; `docs/claims/skeleton-repeatability.claim` says `profile = none`; `docs/cpu-profiles.md` states "No profile is a baseline". No claim names lynx1 or lynx2. PASSED.
- Phase 1 properties: `det.two_runs`, `det.two_instances`, `det.sensitivity`, `det.golden`, `api.load_refusal_unchanged`, `api.snapshot_refusal`, `api.snapshot_roundtrip`, `api.advance_budget`, `api.inspect_pure` all pass in the 94-test run.
- Open owner decisions O1, O5, O6, O9 are by design, not gaps.

### Anti-Patterns Found

None blocking. Grep for TBD, FIXME and XXX over `core include runner tests scripts docs fixtures/cpu` without a follow-up reference returned nothing. Review (02-REVIEW.md: 0 critical, 5 warnings) fixed per 02-REVIEW-FIX.md; Info items IN-02, IN-03, IN-07, IN-08 and IN-11 are deliberately deferred.

### Source-honesty note

Every per-opcode cycle count and the flat5 ticks are stand-ins from other vendors' datasheets, a hardware-informed test, or a software model; each expected value carries a tag from the closed set and an unverified flag, and the cost model is labelled PLACEHOLDER. Nothing here is a Lynx timing claim.

### Flagged Prohibition (judgment tier)

Clean-room prohibition (no emulator source opened) is judgment-tier, supported by exposure-record rows and SUMMARY attestations, not machine-checkable. Recorded as `unverified-prohibition: human review recommended`, pending owner decision O6 (clean-room record is a draft). Treated as in Phase 1: it does not change the status.

### Human Verification Required

None for phase goal achievement.

### Gaps Summary

No gaps. Three advisories are listed in the frontmatter: path-length fragility of two vector-control tests, the unobserved tick stamp (by design until a device consumes it), and the local-only docs commit `4c91a83`.

---

_Verified: 2026-11-10_
_Verifier: Claude (gsd-verifier)_
