---
phase: 01-evidence-harness-and-walking-skeleton
plan: 07
subsystem: snapshot
tags: [c17, snapshot, sha256, validate-then-apply, refusal-oracle, memcmp]
status: complete

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "Plan 01-02 in-core SHA-256 and build id; plan 01-06 host API contract and shared test helpers"
provides:
  - "lynx_snapshot_size, lynx_snapshot_save, lynx_snapshot_restore (snapshot format version 1, LYNXSNAP header)"
  - "api.snapshot_roundtrip and api.snapshot_refusal"
  - "Inventory of 29 tests"
affects: [01-08, 01-11, 01-13]

requirements-completed: [API-07]

actuals:
  tokens: 21000
  tasks: 2
  commits: 2

plan_head_before: 5922d7eeb39e959958c431db193150daaf09c14c
plan_head_after: d7c0aa06725c039a49ea31dbf8dac7ff906e72d5
commits: 2

tech-stack:
  added: []
  patterns:
    - "Validate-then-apply: ten ordered checks (arguments, exact length, magic, version, header length, build id, identity, payload length, payload hash, value ranges) then one non-failing field-by-field apply"
    - "Identity comes from the instance, never the blob; the blob only has to match it"
    - "Little-endian field-by-field serialisation, never a struct copy, so padding and host byte order never reach the blob"
    - "White-box refusal tests re-hash a corrupted payload so value validation is reached past the hash check"

key-files:
  created:
    - core/snapshot.c
  modified:
    - include/lynxplosion.h
    - core/internal.h
    - CMakeLists.txt
    - tests/api/test_api.c
    - tests/api/tests.cmake
    - tests/header/header.c
    - tests/inventory/fast.txt

key-decisions:
  - "Snapshot format version 1: 236-byte header, 65552-byte payload, 65788 bytes in total, constant for a build"
  - "lynx_snapshot_size needs only an initialised machine; save and restore additionally need loaded content (STATE otherwise)"
  - "A refused save writes nothing to the output buffer and nothing to *written"
  - "Overlap of the blob buffer with the instance is LYNX_ERR_ARGUMENT, checked after capacity for save and before the length check for restore"
  - "Reversibility: restore needs the identical build id, so no old snapshot is ever accepted and the format may change in any later build without migration (rated reversible)"

patterns-established:
  - "Later state added to the machine (CPU, bus, devices) must be added to both the payload and the apply step, with a version bump"

coverage:
  - id: D1
    description: "A snapshot saved mid-run restores into the same instance to a byte-identical buffer and continues to the same buffer as an uninterrupted run; restored into a second fresh instance with the same config and content it continues to the same bytes"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_roundtrip (ctest api.snapshot_roundtrip)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every header byte flipped is refused with the field's own status (INVALID, VERSION or MISMATCH) and the instance buffer stays byte-identical"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_refusal (ctest api.snapshot_refusal)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Payload flips at the first byte, the last byte and every 251st byte are refused with LYNX_ERR_SNAPSHOT_INVALID, instance unchanged"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_refusal (ctest api.snapshot_refusal)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Truncated, extended and zero-length blobs, version 2, and snapshots from another seed, content byte, RAM policy, address or length are refused before any state changes"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_refusal (ctest api.snapshot_refusal)"
        status: pass
    human_judgment: false
  - id: D5
    description: "A blob with a valid hash but halted 2 (or 0xFF) or a PC outside the content range is refused, so value validation runs after the hash check and before the apply"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_refusal (ctest api.snapshot_refusal)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Saving into a buffer one byte too small returns LYNX_ERR_CAPACITY and writes nothing; save and size leave the instance byte-identical; overlapping buffers, NULLs, no content and a deinitialised machine are refused"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_roundtrip (ctest api.snapshot_roundtrip)"
        status: pass
    human_judgment: false
  - id: D7
    description: "An image ending exactly at 0x10000 (16 bytes at 0xFFF0) saved at PC 0xFFF7 and at 0xFFFF restores and continues to the same buffer as an uninterrupted run; the PC range check uses 32-bit arithmetic"
    requirement: API-07
    verification:
      - kind: unit
        ref: "tests/api/test_api.c#case_snapshot_roundtrip, #case_snapshot_refusal (ctest api.snapshot_roundtrip, api.snapshot_refusal)"
        status: pass
    human_judgment: false

duration: 5min
completed: 2026-11-09
---

# Phase 1 Plan 07: Snapshot save and restore Summary

**Versioned `LYNXSNAP` snapshots (identity header plus SHA-256-protected payload) with a ten-step validate-then-apply restore, proven by whole-buffer `memcmp` across every header byte, sampled payload bytes, wrong lengths and versions, mismatched identities and valid-hash bad values.**

## Performance

- **Duration:** about 5 min (first task commit ledger base at 18:15, last commit 18:20, local time; no start timestamp was captured, so this is read from commit times)
- **Tasks:** 2 of 2
- **Files:** 1 created, 7 modified
- **`api.snapshot_refusal` under ASan+UBSan:** 0.91 s to 0.99 s wall (assumption A4 held; stride 251 was kept, no widening needed). `api.snapshot_roundtrip`: 0.09 s to 0.11 s.
- **Full check:** `sh scripts/check.sh` exits 0 in 4 s total; 29 tests accounted for once each in junit.

## Accomplishments

- `core/snapshot.c` (new, in `LYNX_CORE_SOURCES` so `lynx_core` and `lynx_core_plain` both get it) with the three public functions. Layout: header 236 bytes (`LYNXSNAP`, format 1, header length, 128-byte NUL-padded build id, profile, policy, seed, content SHA-256, content address and length, payload length, payload SHA-256), payload 65552 bytes (pc, a, x, y, s, p, halted, guest ticks, 64 KiB RAM). Layout is documented at the top of `core/snapshot.c` and in the header comment; constants live in `core/internal.h`. Static assertions tie the offset enums to `LYNX__SNAPSHOT_HEADER_LEN` and `LYNX__SNAPSHOT_PAYLOAD_LEN`.
- Save: checks NULL, magic, has-content, capacity, then disjointness (subtraction form on `uintptr_t`) before the first write. Restore: arguments, state, overlap, exact length, magic, version, header length, full 128-byte build id, identity (profile, policy, seed, content SHA-256, address, length), payload length, recomputed payload hash, then value ranges (`halted` 0 or 1; PC inside the content range in 32-bit arithmetic), and only then the single apply step.
- `api.snapshot_roundtrip`: same-instance restore equals the buffer at save time and continues to the uninterrupted final buffer; a second fresh instance (same config, same content) continues to the same bytes; saving twice gives identical bytes; `cap = size - 1` gives `LYNX_ERR_CAPACITY` with the output left all `0xA5`; NULL arguments; overlap in three positions (inside, reaching into the start, reaching into the end); no-content and deinitialised machine (STATE); top of address space at PC `0xFFF7` (restore then wraps to `0xFFF0` after the sixteenth step) and at PC `0xFFFF`; a halted machine keeps its halt.
- `api.snapshot_refusal`: all 236 header bytes flipped, asserting the per-field status (magic and header length and payload fields INVALID, version VERSION, build id through content fields MISMATCH); payload offsets 0, 251, 502 ... plus the last byte INVALID; lengths `size-1`, `size+1`, 0 and header-only; version 2; five other identities (seed, content byte, RAM policy, address, length); white-box re-hashed blobs with halted 2 and 0xFF and PCs `0x01FF`, `0x0220`, `0x0000`, `0xFFFF`; top-of-space instance with PCs `0xFFEF`, `0x0000`, `0x0200` refused and `0xFFFF` accepted; accepted-restore controls prove the oracle and the re-hash helper can see a change.
- Mutation checks (not committed): skipping the halted check, applying a RAM byte before the hash check, and comparing only 100 of the 128 build-id bytes each make `api.snapshot_refusal` fail; a 16-bit `content_addr + content_len` range check makes both snapshot tests fail (the roundtrip top-of-space case catches it). A truncated 16-bit subtraction mutation survived, but it is an equivalent mutation (the first PC comparison already rejects those values).
- `tests/header/header.c` signature table gained the three functions; `api.abi_symbols`, `api.header_selfcontained`, `core.includes`, `core.no_float`, `core.nm_undefined` and `core.nm_globals` still pass. The reserved snapshot status comments in the header were replaced by their real meanings.

## Task Commits

1. **Task 1: snapshot save, size and roundtrip restore** - `c1df18f` (feat)
2. **Task 2: exhaustive snapshot refusal** - `d7c0aa0` (test)

TDD note: the tests were written together with the implementation and passed on the first run; the non-vacuity of the refusal oracle was confirmed by the four mutations above, each reverted byte-for-byte before committing.

## Deviations from Plan

None - plan executed exactly as written, with two clarifications:

- The plan's `<verify>` fails_when wording says "100% tests passed, 0 tests failed out of N"; CTest prints "100% tests passed out of N" (already recorded in earlier summaries), so exit status and the printed counts were used.
- Both tasks' code was written before the first commit. To keep one commit per task, the Task 1 commit was built from the tree with the refusal case, its helpers, its CMake row and its inventory row temporarily removed, verified green, committed, then the full files were restored for Task 2.

**Total deviations:** 0 auto-fixed. **Impact:** none.

## Issues Encountered

None.

## Known Stubs

None. The snapshot payload covers exactly the provisional skeleton state (registers, halt flag, guest time, RAM). Phase 2 and later state must be added to the payload and the apply step with a format version bump; the golden digest planned for 01-08 does not depend on snapshots.

## Threat Flags

None new. T-01-11 (tampering with a restore blob): exact length, magic, version, header length, full build id, identity, payload hash and value-range validation all precede a non-failing apply, covered by the exhaustive header loop and sampled payload loop under non-recoverable ASan+UBSan. T-01-12 (identity spoofing): identity is read from the instance and only compared with the blob. T-01-13 (save buffer): capacity and disjointness are checked before the first write, and a refused save writes nothing.

## Notes for the next plan

- Snapshot size is constant, 65788 bytes, and equal for every instance of a build. Hosts may size a buffer once.
- The build id contains the git revision and a `+dirty` flag, so a snapshot taken from a dirty build restores only into the same dirty build (same string); two clean builds of the same commit, compiler and system exchange snapshots. Within one process this always holds.
- Restore requires loaded content in the target instance (STATE otherwise) and the same content bytes, address and length; the host loads the content first, then restores. `lynx_reset` clears content, so reload before restoring after a reset.
- The refusal test is about 1 s under ASan, well inside its 60 s timeout.
- `api.inspect_pure` does not cover the snapshot calls; their purity is asserted in `api.snapshot_roundtrip` (size and save leave the buffer byte-identical).

## Self-Check: PASSED

`core/snapshot.c` and `01-07-SUMMARY.md` exist; commits `c1df18f` and `d7c0aa0` are ancestors of HEAD; `git rev-list --count` from the ledger base gives 2; `sh scripts/check.sh` exits 0.
