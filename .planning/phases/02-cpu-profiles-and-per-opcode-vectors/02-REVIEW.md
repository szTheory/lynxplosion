---
phase: 02-cpu-profiles-and-per-opcode-vectors
reviewed: 2026-11-10T00:00:00Z
depth: standard
files_reviewed: 19
findings:
  critical: 0
  warning: 5
  info: 11
  total: 16
  necessity: 0
status: issues_found
---

# Phase 2: Code Review Report

Source: the gsd-code-reviewer agent's report, saved by the orchestrator (the agent may not write report
files). No unsafe memory access and no arithmetic UB found in the opcode/mode code, budget loop,
snapshot code or vector reader. Findings concern restore-validation completeness and script robustness.

## Critical Issues

None.

## Warnings

### WR-01: `lynx__cpu_state_valid` accepts the unreachable `IRQ_TAKE` and `NMI_TAKE` latch bits
**File:** `core/cpu.c:862`
**Issue:** `(c->flags & ~0x3Fu) != 0u` lets bits 0 and 1 through. `poll()` sets them only on a `CF_POLL` cycle and `finish()` consumes and clears them in the same `lynx__cpu_complete` call, so no reachable state has either bit set. A forged snapshot (payload SHA is not authenticated) can set one, and the next instruction end then takes an interrupt that never polled (IRQ with I set, or NMI with no edge).
**Fix:** accept only `LYNX__CS_NMI_PENDING | LYNX__CS_NMI_PREV | LYNX__CS_SEQ_MASK` in `flags`.

### WR-02: `lynx__cpu_state_valid` accepts a `step` pointing at a conditional micro-op real execution would have skipped
**File:** `core/cpu.c:875-887`
**Issue:** Validity is only `step <= max_slot(mode)`. Slots skipped at runtime (`U_X_S_C`/`U_X_D_C` when `tmp == 0`, `U_DECX_*` when D is clear, `U_BR_TAKEN` when not taken, `U_BR_CROSS_REL`/`U_BR_CROSS_ZP` when not taken or not crossing) are accepted: for example opcode `0xD0`, step 3, Z set passes and `repost` then posts `U_BR_CROSS_REL`, so `apply` sets `pc = br_target` for a not-taken branch. A `tmp` outside {0,1} at page-cross slots, or `ea` bits `0x100`/`0x200` outside the branch micro-ops, also pass. No memory-safety impact, but the machine reaches states and cycle counts execution never produces.
**Fix:** after computing the mode, compute the pending slot (step plus 1 when seq != NONE) and require `uop_active(c, t, lynx__script[mode][slot])`; require `tmp <= 1` at page-cross slots; require `ea >> 8` to be 0 at `U_BR_TAKEN` and 1 at `U_BR_CROSS_*`.

### WR-03: Restore accepts `guest_ticks` above the headroom limit; `lynx_advance` then reports success while doing nothing
**File:** `core/snapshot.c:311-315`, `core/machine.c:233-238`, `include/lynxplosion.h:152-173`
**Issue:** Restore checks only `ready >= ticks` and `ready - ticks <= 255`. A snapshot with ticks above `UINT64_MAX - 0x10000` is accepted although real execution never gets there; afterwards `room = 0` and every `lynx_advance` returns `LYNX_OK`, consumed 0, `LYNX_STOP_BUDGET`, with no distinct signal. A host loop `while (left) { advance(left); left -= consumed; }` spins forever.
**Fix:** reject `ticks > UINT64_MAX - LYNX__TICK_HEADROOM` (and bound `ready` the same way) on restore; give the limit its own signal (a `LYNX_ERR_RANGE` status or a new stop reason), or at least document that `consumed < budget` with `BUDGET` means the limit was hit.

### WR-04: `vectors-full.sh run` can test a stale runner binary
**File:** `scripts/vectors-full.sh:338-344`
**Issue:** The check preset is built only if the runner is missing, so an older or locally edited binary is used and the 2 GB lane's evidence is recorded against a build that is not HEAD.
**Fix:** always build (incremental and cheap): `(cd "$root" && cmake --preset check && cmake --build --preset check) || fail "the check preset did not build"`.

### WR-05: `validate-kv.sh` expands `$tiers` unquoted, so shell globbing can rewrite the tier list
**File:** `scripts/validate-kv.sh:79`
**Issue:** `for tier in $tiers` is subject to pathname expansion: a word such as `A*` or `*` becomes file names and letters-only names then pass as tiers, while awk later splits the unexpanded string (the shell and awk views disagree).
**Fix:** `set -f` before the loop, or reject glob characters first: `case $tiers in *[*?[]*) usage ;; esac`.

## Info

- IN-01 `core/cpu.h:7` comment says only stdint/stdbool are allowed; the gate also allows stddef.h and string.h and `cpu.c:13` includes an unused `<stddef.h>`. Fix the comment, drop the include.
- IN-02 `core/cpu.c:809-817`: `lynx__cpu_start_reset` clears `NMI_PREV`; unreachable in Phase 2, preserve it before Phase 4.
- IN-03 `tests/cpu/sst_reader.c:640-655`: sample mode accepts a last object without a trailing comma (covered by the byte-for-byte container compare).
- IN-04 `scripts/vectors-full.sh:229,247`: `cp` of the results file has no existence check under `set -e`; test `[ -f ... ]` and set `lane_status=1`.
- IN-05 `scripts/vectors-full.sh:268`: the cross-pairing check compares only the failing-opcode set, not the failure counts.
- IN-06 `scripts/vectors-full.sh:125`: `check_record` awk counts a blank line as a row; use `NF && $1 !~ /^#/`.
- IN-07 `scripts/vectors-full.sh:145`: `curl --max-filesize` is not enforced without Content-Length; add a post-download size check.
- IN-08 `scripts/vectors-full.sh:297-298`: `pin` silently overwrites an existing `upstream.sha256` (trust on first use).
- IN-09 `scripts/validate-kv.sh:73`: `--required ""` is accepted, so an empty file passes.
- IN-10 `tests/cpu/test_cpu_vectors.c:853,870`: `memcmp` over a struct with tail padding after struct assignment; compare field-wise or `memset` first.
- IN-11 `scripts/check.sh:153,268`: `mktemp -d` directories are not covered by the EXIT trap.

---

_Reviewed: 2026-11-10_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
