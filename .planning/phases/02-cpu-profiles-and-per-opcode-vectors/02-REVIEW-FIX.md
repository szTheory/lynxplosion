---
phase: 02-cpu-profiles-and-per-opcode-vectors
fixed_at: 2026-11-10T00:00:00Z
review_path: .planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-REVIEW.md
iteration: 1
findings_in_scope: 16
fixed: 11
skipped: 5
status: partial
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-11-10
**Source review:** .planning/phases/02-cpu-profiles-and-per-opcode-vectors/02-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in the review: 16 (0 critical, 5 warning, 11 info)
- Requested scope: WR-01 to WR-05 and IN-01, IN-04, IN-05, IN-06, IN-09, IN-10 (11 findings)
- Fixed: 11 (IN-01 partly: see its entry)
- Skipped: 5 (IN-02, IN-03, IN-07, IN-08, IN-11, outside the requested scope by the caller's choice)

Every fix to a validation or gate script has a seeded negative control that was run
against the old code first and failed there, then passed after the fix. The fixes were
made and the checks run in the main checkout (`workflow.use_worktrees` is false), so
the numbers below are reproducible from the tree.

## Verification

- Host (macOS, Apple clang, shellcheck 0.11.0): `sh scripts/check.sh`
  exits 0, "100% tests passed, 0 tests failed out of 94", "94 tests accounted for once
  each (0 declared skips)". Before the fixes the same command gave 93 of 93. The one new
  test is `cpu.vectors.lane_script`; the other controls extend existing tests.
- Linux (ubuntu:24.04 container, gcc, cmake 3.28.3, shellcheck 0.9.0): a clone of the
  final commit, `sh scripts/check.sh` exits 0, "100% tests passed, 0 tests failed out of
  94", junit accounting clean. The container built its own cc65 pin (a host `build/pins`
  copy holds macOS binaries and was not used).
- `shellcheck -s sh` is clean on every touched shell script (0.11.0 on the host, 0.9.0
  in the container via the check's own shellcheck gate).
- The golden digest was not touched: no reachable run changed behaviour (the validity
  sweep still accepts all 47,725 real CPU states, and `det.golden` passes).
- Not run: the 2 GB network lane (`scripts/vectors-full.sh run`). The recorded results
  from the Phase 2 run (`build/vectors-full/*.results`) satisfy the new IN-05 rule.

## Fixed Issues

### WR-01: `lynx__cpu_state_valid` accepts the unreachable `IRQ_TAKE` and `NMI_TAKE` latch bits

**Status:** fixed: requires human verification (validation logic)
**Files modified:** `core/cpu.c`, `tests/cpu/test_cpu.c`, `tests/api/test_api.c`
**Commit:** 4414026 (shared with WR-02: both are in one function and the tests interleave)
**Applied fix:** `flags` may hold only `NMI_PENDING | NMI_PREV | SEQ_MASK` (0x3C). The
old boundary tests that called 0x0F and 0x3F legal were corrected to 0x0C and 0x3C.
**Seeded negative control:** forged states in `check_state_forgeries` (cpu.cycles) with
flags 1, 2 and 3 at a boundary, mid-instruction and inside an IRQ entry sequence, and
forged snapshot payloads (flags 0x01, 0x02, 0x03 on a mid-instruction blob and on a
taken-branch blob) with the payload SHA-256 recomputed so only the field check can
refuse (api.snapshot_refusal). Against the old code the API control printed "snapshot
restore status 0, want 6" for each forged payload and the CPU control failed its
refusals; against the new code both pass, and the control states themselves are asserted
accepted first.

### WR-02: `lynx__cpu_state_valid` accepts a `step` pointing at a conditional micro-op real execution would have skipped

**Status:** fixed: requires human verification (validation logic)
**Files modified:** `core/cpu.c`, `tests/cpu/test_cpu.c`, `tests/api/test_api.c`
**Commit:** 4414026
**Applied fix:** a new `pending_reached` helper (also used for entry and reset
sequences). The pending slot must pass `uop_active`; at a page-cross dummy `tmp` must be
1; `ea >> 8` must be 0 before the branch taken cycle and 1 before the branch cross cycle.
The review's other suggestion (any `tmp` above 1 at a non-conditional slot) was not
applied because `tmp` carries data at most slots; only the slots the review named are
constrained.
**Seeded negative control:** in cpu.cycles, from real states of a taken page-crossing
BNE, an indexed LDA with and without a crossing, and a decimal ADC: Z set at the taken
and cross slots, `ea` high byte 0, 1 early, 2 and 3, `tmp` 0/2/0xFF at the page-cross
dummy, D clear at the decimal extra cycle, all refused. In api.snapshot_refusal, a real
snapshot of a taken BNE with the cross cycle pending restores (control), and the same
blob with Z set, `ea` 0x0010, 0x0210 and 0x0310 (SHA recomputed) is refused with the
instance untouched. Old code accepted all of these. The full validity sweep (all
opcodes, all profiles, no lines, IRQ held, NMI held) still accepts every real state.

### WR-03: Restore accepts `guest_ticks` above the headroom limit

**Status:** fixed: requires human verification (validation logic, API contract)
**Files modified:** `core/snapshot.c`, `include/lynxplosion.h`, `tests/api/test_api.c`
**Commit:** 85871bf
**Applied fix:** restore refuses `ticks > UINT64_MAX - 65,536` as
`LYNX_ERR_SNAPSHOT_INVALID`, instance untouched, snapshot format number unchanged. The
review's second suggestion (bound `ready` the same way) was not applied: a cycle granted
just below the limit ends up to 254 ticks past it, so that state is reachable and must
restore; the existing span check already bounds `ready`. No new status or stop reason (it
would change the ABI and the reachable limit behaviour). The header now documents the
signal at the limit: `LYNX_OK`, `LYNX_STOP_BUDGET`, `*consumed` below the budget and 0
once the limit is reached, and tells host loops to treat a BUDGET return with consumed 0
and a non-zero budget as the end of guest time; it also says restore refuses states above
the limit.
**Seeded negative control:** payloads with ticks (and ready) at limit + 1, 2, 255 and
0x10000, SHA recomputed, must be refused with the instance unchanged (failed against the
old code); a state at the limit with ready = limit + 254 restores, and `lynx_advance`
then returns OK, BUDGET, consumed 0 with guest time unchanged (the documented behaviour,
tested).

### WR-04: `vectors-full.sh run` can test a stale runner binary

**Files modified:** `scripts/vectors-full.sh`, `tests/cpu/vector_controls.sh`,
`tests/cpu/tests.cmake`, `tests/inventory/fast.txt`,
`docs/claims/skeleton-repeatability.claim`
**Commit:** b8ee5b8
**Applied fix:** a `build_runner` function always configures and builds the check preset
(incremental) before the lanes; a failed build is fatal. A network-free `build` mode
exposes the same step. `tests/inventory/fast.txt` gained `cpu.vectors.lane_script`
(sorted, LC_ALL=C) and the claim's `inventory_sha256` was refreshed with
`sh scripts/check-claims.sh --inventory-sha256` in the same commit.
**Seeded negative control:** new test `cpu.vectors.lane_script` (case in
`tests/cpu/vector_controls.sh`). A copy of the script in a fake repository with a stale
runner and a fake `cmake` on PATH must call `--preset check` and `--build --preset check`
and replace the runner; a failing cmake must end with exit 1; and an awk check requires
the run branch to call `build_runner` unconditionally before its first lane. Against the
old script the control failed (the build mode did not exist and the build was guarded).

### WR-05: `validate-kv.sh` expands `$tiers` unquoted

**Files modified:** `scripts/validate-kv.sh`, `tests/det/manifest_controls.sh`,
`tests/gates/manifests_controls.sh`, `tests/gates/claims_controls.sh`
**Commit:** b0360ba (shared with IN-09: same script and same control files)
**Applied fix:** a tier list containing `*`, `?` or `[` is a usage error (exit 2).
**Seeded negative control:** in `det.manifest_fields.self_test`, `hygiene.manifests.self_test`
and `docs.claim_template.self_test`: tier lists `A*`, `*`, `A?`, `[A]` and `A [x` run in
a directory that holds letters-only file names must exit 2 (the old validator exited 1,
having expanded the list against that directory). Files are built at run time.

### IN-01: `core/cpu.h` include comment

**Status:** partly fixed
**Files modified:** `core/cpu.h`
**Commit:** cb87f99
**Applied fix:** the comment now lists the four headers the `core.cpu_includes` gate
allows. The review also called the `<stddef.h>` include in `core/cpu.c` unused; it is
not (`lynx__cpu_init` uses `NULL`), so it stays.

### IN-04: results file `cp` without an existence check

**Files modified:** `scripts/vectors-full.sh`, `tests/cpu/vector_controls.sh`
**Commit:** 57aa562
**Applied fix:** `run_same` and `run_cross` test the results file, print the runner exit
status and set `lane_status=1` instead of dying under `set -e`.
**Seeded negative control:** in `cpu.vectors.lane_script`, the real `run_same` cut out of
the script and driven against a fake runner that exits without results must report the
failure and leave `lane_status=1` (the old script died in `cp`).

### IN-05: cross-pairing check compares only the opcode set

**Files modified:** `scripts/vectors-full.sh`, `tests/cpu/vector_controls.sh`
**Commit:** 73e2b36
**Applied fix:** every failing row of a cross pairing must also have passed + failed =
10,000. The per-opcode failure counts are data dependent (about half of the 10,000
vectors fail in the recorded runs), so no fixed count is asserted. The recorded Phase 2
results satisfy the rule.
**Seeded negative control:** the real `run_cross` against a fake runner whose results
have the right 32 opcodes failing but one row short (4,000 + 5,000) must set
`lane_status=1`; the same run with whole rows stays 0 (control). The old script passed
the short row.

### IN-06: `check_record` counts a blank line as a row

**Files modified:** `scripts/vectors-full.sh`, `tests/cpu/vector_controls.sh`
**Commit:** 4be8f62
**Applied fix:** the awk counts `NF && $1 !~ /^#/`.
**Seeded negative control:** a fake-root record of 511 rows plus a blank line must be
refused with "the pin record has 511 rows, not 512" (the old script counted 512 and went
on), and 512 rows plus a blank line passes the count (the run then stops at the fake
root's missing LICENSE).

### IN-09: `--required ""` accepted

**Files modified:** `scripts/validate-kv.sh`, `tests/det/manifest_controls.sh`,
`tests/gates/manifests_controls.sh`, `tests/gates/claims_controls.sh`
**Commit:** b0360ba
**Applied fix:** an empty or all-blank `--required` list is a usage error (exit 2).
**Seeded negative control:** in the same three controls, `--required ""` and
`--required " "` on an empty file must exit 2 (the old validator exited 0 for the empty
file).

### IN-10: `memcmp` over a struct after struct assignment

**Files modified:** `tests/cpu/test_cpu_vectors.c`
**Commit:** e7ccd8a
**Applied fix:** the saved CPU copy in `probe_undefined` is made with `memcpy`, so the
padding the `memcmp` reads is copied too.
**Seeded control (manual, not committed):** perturbing a CPU field after the copy made
`cpu.vectors.none` fail, so the comparison still bites; unchanged, all 14 cpu.vectors
tests pass.

## Skipped Issues

These are outside the scope requested for this pass (the review's cheap Info items only);
none was attempted.

### IN-02: `lynx__cpu_start_reset` clears `NMI_PREV`

**File:** `core/cpu.c:809-817`
**Reason:** skipped by scope; unreachable in Phase 2, preserve before Phase 4.
**Original issue:** reset clears the NMI edge detector.

### IN-03: sample mode accepts a last object without a trailing comma

**File:** `tests/cpu/sst_reader.c:640-655`
**Reason:** skipped by scope; covered by the byte-for-byte container compare.
**Original issue:** the sample-mode reader is lenient about the last object's comma.

### IN-07: `curl --max-filesize` not enforced without Content-Length

**File:** `scripts/vectors-full.sh:145`
**Reason:** skipped by scope; the SHA-256 check against the pin record already rejects an
oversized or altered download, so the post-download size check is hardening only.
**Original issue:** the size cap relies on a Content-Length header.

### IN-08: `pin` overwrites an existing `upstream.sha256`

**File:** `scripts/vectors-full.sh:297-298`
**Reason:** skipped by scope; a pin change is an explicit, reviewed action.
**Original issue:** trust on first use.

### IN-11: `mktemp -d` directories not covered by the EXIT trap

**File:** `scripts/check.sh:153,268`
**Reason:** skipped by scope; `scripts/check.sh` is the gate itself.
**Original issue:** an interrupted run can leave scratch directories.

---

_Fixed: 2026-11-10_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
