---
phase: 05-display-raw-video-and-input
task: 05-12
repaired_at: 2026-11-12T15:38:25Z
pre_repair_head: fe7afdd39fb498cfe9146b2d1263584e65a2bd21
repair_commit: 9290c743e67e9a8aabc60aff841c831c17186410
repairs: 4
focused_checks: passed
---

# Phase 05 Task 12 Qualification Harness Repairs

These are four harness and platform compatibility repairs found while preparing Task 12 qualification. They are separate from the five resolved code-review findings; those entries and their evidence remain in `05-REVIEW-FIX.md`. No core implementation, CPU oracle, or legacy 6502 source was changed.

**Repair commit:** `9290c743e67e9a8aabc60aff841c831c17186410`
**Pre-repair source head:** `fe7afdd39fb498cfe9146b2d1263584e65a2bd21`
**Old-run record:** `/private/tmp/lynx-phase5-task12-macos-final.log`

## Repairs

### CPU seam snapshot digest offset

**File:** `tests/cpu/test_cpu_vectors.c`
**Old/new SHA-256:** `eb166b626fe1e5d18d5d04a45defa61028d24224e9b594b910b4d69e9785bd7f` / `70bbc989dde386fba7dc15753869a27e367daf7227d95f9580c15467bee04f9e`

Format 5 stores the payload SHA-256 at bytes 220–251 and the timing-policy field at 252–255. The seam helper had derived the hash location as `header length - 32`, which writes at byte 224 and overwrites part of timing-policy identity. It now uses the format-defined hash offset and has compile-time layout checks. The file’s seam description now correctly names format 5.

The old log shows all three `api.seam_agreement` profiles failing at restore before the vectors ran. After repair, `api.seam_agreement.none`, `.lynx1`, and `.lynx2` pass: NONE executes 222 defined rows and 1,024 undefined-vector refusal checks with zero failures; Lynx 1 and Lynx 2 each execute 254 rows with zero failures.

### Diagnostic forged-snapshot layout

**File:** `tests/diag/test_diag.c`
**Old/new SHA-256:** `6f7f46db62370fa703dc96530d8c0dbdefdb89de635a929d39d3d9544d57ab16` / `99aa549f0f522e53bdf9961893dcdda5f912c411d50e026e49959e5dd0d20ebf`

The controls expected format 4 (252-byte header and 65,604-byte payload) after the core moved to format 5 (256-byte header and 77,948-byte payload). That stale size check stopped the forged-state and diagnostic-restore controls before their intended assertions. The test now uses the documented format-5 size, keeps the payload hash at byte 220 and RAM at payload offset 32, and checks the timing-policy word at byte 252. The payload reserved-byte and guest-result mutation checks remain unchanged.

`diag.result_controls`, `det.diagnostic_restore`, and `det.diagnostic_restore.self_test` pass. The diagnostic output confirms the result magic/schema/progress/error/done/pixel mutations and invalid-state controls still run.

### Timer fault mutant source list

**File:** `tests/timers/fault_controls.cmake`
**Old/new SHA-256:** `4d218dd30783066724ea5a78377d568718b004a1c38de19b8043af6175860371` / `eeefca41067bc328152f20698644c36e396094acc3138a40de8c03221852b030`

The synthetic mutant project copied all of `core/` but its generated CMake target omitted `core/input.c` and `core/video.c`, so it could not link the current machine implementation. Both sources are now included in the target. All five mutants now compile and fail their intended assertions with exit 1; the script also reports a clean API positive before and after each mutant:

- `done` → `timer.done-gates-counting`
- `irq` → `timer.irq.pending-independent-of-enable`
- `order` → `timer.order.timer-before-cpu-prefix`
- `phase` → `timer.phase.shared-divider-edge`
- `reset_done` → `timer.reset-done-held-clear`

### Darwin compiler stack-probe allowlist

**File:** `tests/cmake/abi.cmake`
**Old/new SHA-256:** `1924b6287292a6d1b8dc532b86607ed52c378fdd11aa519dd3525c352c3762d0` / `2cbf21b96f8c5258e327e62eea6d5f876b0ab6eb3173d30dec7344b567ee8e5e`

Apple clang emits raw `nm -P` symbol `___chkstk_darwin` for the large local video/snapshot buffer in `snapshot.c.o`. The ABI script’s CMake regex replacement removes every leading underscore, so the exact normalized symbol is `chkstk_darwin`. The allowlist now accepts only that name on Darwin. No general compiler-helper or libc allowance was added. `core.nm_undefined.self_test` still passes its libc-symbol refusal control.

## Verification

The focused targets `lynx-test-cpu-vectors`, `lynx-test-diag`, `lynx-test-api`, and `lynx_core_plain` built successfully. All 13 requested focused tests passed:

- `api.seam_agreement.none`, `.lynx1`, `.lynx2`
- `diag.result_controls`
- `det.diagnostic_restore`, `det.diagnostic_restore.self_test`
- `timer.fault.done`, `.irq`, `.order`, `.phase`, `.reset_done`
- `core.nm_undefined`, `core.nm_undefined.self_test`

The broad `check.sh` qualification and 2 GB CPU-vector runs were not run here. This report remains uncommitted for root to review and include with the qualification records.
