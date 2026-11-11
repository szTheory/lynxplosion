# Phase 4: Timers, Interrupts and First Public Suites — Pattern Map

**Mapped:** 2026-11-10
**Files analyzed:** 29 targets or proposed file groups
**Analogs found:** 28 / 29 (including role-matches)

Proposed paths below are suggestions for new files, not existing analogs. Every named analog was verified with `git ls-files`.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `core/scheduler.c` (if split out; proposed) | utility | event-driven | `core/machine.c` | role-match |
| `core/timer.c` (proposed) | model | event-driven, transform | none | none |
| `core/machine.c` | service/controller | event-driven, request-response | itself, lines 328-480 | exact |
| `core/internal.h` | model | event-driven | itself, lines 13-70 | exact |
| `core/cpu.c` | component | request-response | itself, lines 590-633, 747-794 | exact |
| `core/cpu.h` | model/interface | request-response | itself, lines 117-161 | exact |
| `core/bus.c` | utility | transform, request-response | itself, lines 33-54 | exact |
| `core/bus.h` | interface | request-response | itself, lines 49-58 | exact |
| `core/snapshot.c` | service | memory serialization | itself, lines 237-369 | exact |
| `include/lynxplosion.h` | interface | request-response | itself, lines 169-250, 288-317 | exact |
| `tests/api/test_api.c` | test | request-response | itself, `case_map_matrix`, `case_snapshot_continue` | exact |
| `tests/det/test_det.c` | test | transform, event-driven | itself, `watch_snapshot_agreement` | exact |
| `tests/timers/test_timer_bank.c` (proposed) | test | transform, event-driven | `tests/api/test_api.c`, `case_map_matrix` | role-match |
| `fixtures/suites/cpu-wrapper.s` (proposed) | test fixture | event-driven | `fixtures/diagnostic/first.s` | role-match |
| `fixtures/suites/timers-wrapper.s` (proposed) | test fixture | event-driven | `fixtures/diagnostic/first.s` | role-match |
| `scripts/check.sh` | utility/build | batch, file-I/O | itself, `run_diagnostic_fixtures` | exact |
| `fixtures/manifests/cpu-tests.manifest` (proposed) | config/manifest | batch | `fixtures/manifests/first-diagnostic.manifest` | role-match |
| `fixtures/manifests/timer-tests.manifest` (proposed) | config/manifest | batch | `fixtures/manifests/first-diagnostic.manifest` | role-match |
| `tests/suites/test_public_suites.c` (proposed) | test | batch, request-response | `tests/diag/test_diag.c` | role-match |
| `tests/suites/tests.cmake` (proposed) | config/test registration | batch | `tests/diag/tests.cmake` | role-match |
| `tests/api/tests.cmake` | config/test registration | batch | itself, lines 1-51, 112-134 | exact |
| `tests/det/tests.cmake` | config/test registration | batch | itself, lines 51-106 | exact |
| `tests/tests.cmake` | config/test registration | batch | itself, lines 1-14 | exact |
| `tests/inventory/fast.txt` | config/inventory | batch | `tests/gates/inventory.cmake` | role-match |
| `docs/claims/skeleton-repeatability.claim` | evidence metadata | batch | `scripts/check-claims.sh` | role-match |
| `docs/memory-startup.md` | documentation | request-response | itself, lines 36-50, 58-90 | exact |
| `docs/provenance.md` | documentation | batch | itself, lines 67-95 | exact |
| `docs/policy/exposure-record.md` | documentation | batch | itself, lines 15-42 | exact |
| `pins.txt` | config/source pin | file-I/O | itself, lines 1-13 | exact |

## Pattern Assignments

### CPU requests and IRQ polling — `core/cpu.c`, `core/cpu.h`

Keep the CPU as a cycle-stepped producer of one pending bus request. Preserve and repost that request until the machine grants and completes it. Add IRQ sampling to the existing completion/POLL seam; do not move timer state into the CPU.

**Source:** `core/cpu.h` lines 117-128, 151-161; `core/cpu.c` lines 590-610, 625-633, 747-794.

```c
void lynx__cpu_repost(lynx__cpu *c, const lynx__cpu_decode *t)
{
    uint8_t mode;
    uint8_t slot;
    if (c->step == 0u && seq_of(c) == SEQ_NONE) {
        c->req_addr = c->pc;
        c->req_kind = LYNX__CK_OPCODE;
        c->req_data = 0u;
        c->req_flags = 0u;
        return;
    }
    mode = mode_of(c, t);
    slot = slot_of(c);
    post_uop(c, t, lynx__script[mode][slot]);
    if (next_slot(c, t, mode, slot) == 0u) {
        c->req_flags = (uint8_t)(c->req_flags | LYNX__CF_LAST);
        if (seq_of(c) == SEQ_NONE && mode != LYNX__M_BRK &&
            (t[c->opcode].flags & LYNX__F_NO_POLL) == 0u) {
            c->req_flags = (uint8_t)(c->req_flags | LYNX__CF_POLL);
        }
    }
}
```

`lynx__cpu_complete` polls the supplied lines only when the pending request carries `LYNX__CF_POLL`; its poll helper already applies the CPU I-mask to acceptance. Reuse that seam and retain the posted request across withheld grants.

### Machine clock, grant loop and watch observation — `core/machine.c`, `core/internal.h`, `include/lynxplosion.h`

The current machine owns guest ticks, ready time, pending CPU state and host-only watch state. Use `core/machine.c:393-480` as the clock/grant integration point and `:328-389` for routed bus availability/access. If scheduling is split into a new module, keep machine ownership and the public advance contract in `machine.c`.

**Public budget contract** (`include/lynxplosion.h`, lines 169-180):

```c
/* Budgets are half-open intervals of guest time: advancing to T = start +
   budget performs every bus cycle that can start before T. A cycle that
   starts before T is performed even if its duration extends past T; the next
   call resumes when it ends. A cycle due exactly at T is not started. */
```

Host watches remain outside guest state. The API contract at `include/lynxplosion.h:217-230` and implementation at `core/machine.c:176-222,445-459` are the model for committing the write/request/ready-time transition before reporting a watch hit. Do not add a public halt/debug entry point for the negative control.

### Device routing — `core/bus.c`, `core/bus.h`

Extend the existing pure route decoder and its machine access switch for newly implemented mapped registers. Keep decode side-effect-free; preserve the existing unavailable path for all other mapped targets. `tests/api/test_api.c:535-582` exercises the decoder through CPU accesses over all MAPCTL values and checks that an unavailable vector request remains pending.

**Decoder interface** (`core/bus.h`, lines 49-54):

```c
lynx__bus_route lynx__bus_decode(uint8_t mapctl, uint16_t addr);
```

### Snapshot continuation — `core/snapshot.c`

Serialize new guest-owned timer, IRQ, and request state with the existing versioned layout. Derive next-event candidates from serialized device state unless stable sequence state is truly required; keep all validation before writes to the instance.

**Existing validation/apply boundary** (`core/snapshot.c`, lines 298-369):

```c
if (!lynx__cpu_state_valid(&cand, table)) {
    return LYNX_ERR_SNAPSHOT_INVALID;
}
/* Validate every remaining field and cross-field/time bound here. */
/* Apply: one step, field by field, no failure path. */
m->cpu.pc = cand.pc;
...
(void)lynx_watch_clear(m);
```

Use `tests/api/test_api.c:2296-2371` as the continuation/refusal pattern: save the same guest state, restore into a fresh matching instance, continue to a common tick, compare guest observations, and prove refusal leaves the instance unchanged. The watch comparison shows how host-only observations stay out of snapshots.

For cross-path equality, `tests/det/test_det.c:314-548` runs uninterrupted, sliced, watched and restored instances to one final tick, then compares public RAM/register/time/identity/snapshot state and the observation digest. Extend that harness for timer state only when the public-observation comparison remains meaningful; retain the dedicated API refusal tests for byte-identical rejection.

### Timer model and scheduler — proposed `core/timer.c`, optional `core/scheduler.c`

There is no existing timer bank or next-event scheduler to copy. The nearest boundaries are the pure bus decoder (`core/bus.c:33-54`), CPU request model (`core/cpu.h:117-161`) and machine grant loop (`core/machine.c:393-480`). Keep the new timer bank instance-owned and deterministic; integrate it through the existing machine loop. Treat stable same-tick ordering, shared phase, bounded catch-up and unsupported-source behavior as explicit new policy, not inferred properties of an existing helper.

### Timer, IRQ and continuation tests — proposed `tests/timers/test_timer_bank.c`; existing `tests/api/test_api.c`

Use fresh machine instances, direct before/at/after boundary checks, exact guest ticks, and refused-operation byte comparisons. `case_map_matrix` is a useful mapped-access shape; `case_snapshot_continue` is the integration shape. Keep pure bank tests separate from CPU-visible mapped-register tests.

### Source suites and completion harness — proposed wrappers and `tests/suites/test_public_suites.c`

`tests/diag/test_diag.c` is the closest validator. Its symbol parser rejects malformed/duplicate symbols and out-of-bounds or overlapping ranges (`:137-240`). Its completion loop requires progress, a symbol-bounded result block, a guest-written done byte, and fixed min/max ticks (`:658-706`); `:1007-1041` demonstrates the no-run and timeout controls. Reuse the independent host-side validation structure, but use suite-specific extents and expectations generated from the pinned source. No existing pattern proves a guest-executed model probe for these suites; add the small source-audited wrapper/probe required by the phase.

The original diagnostic fixture is a BLL-shaped, project-authored guest program with exported labels and a final done write (`fixtures/diagnostic/first.s:18-33,87-114,124-131`). Its linker layout (`fixtures/diagnostic/bll.cfg:1-12`) and manifest (`fixtures/manifests/first-diagnostic.manifest:1-11`) show how source, recipe, object hash, rights and allowed use are recorded. They do not establish that the upstream suite source links untouched; keep that as the first suite-build proof.

### Pinned fixture build — `scripts/check.sh`, proposed suite manifests

Copy the `run_diagnostic_fixtures` pattern at `scripts/check.sh:347-417`: use pinned `ca65`/`ld65` in a clean environment, build into ignored temporary/build storage, retain only allowed original metadata, validate nonempty BLL and `-Ln` output, verify required labels, and compare source/object identity. The build is already part of the configured local gate; do not introduce a separate networked test path.

### CTest registration, inventory and claim hash — `tests/*.cmake`, `tests/inventory/fast.txt`, existing claim

Mirror the executable and named-test registration style in `tests/api/tests.cmake:1-7,45-50`; `tests/tests.cmake:1-14` uses `include()` so registrations enter the top-level exact inventory. Pair new positive tests with intended-failure controls using `tests/api/tests.cmake:112-134` and `tests/gates/inventory.cmake:6-14` as models. `scripts/check-claims.sh:72-80,146-150` hashes exact `fast.txt` bytes and rejects a stale claim. If test names change the inventory, refresh the existing claim hash only after adding the final test set; this does not itself justify a new accuracy/pass claim.

### Evidence and source records — docs and pin metadata

Extend the concise source/recipe/right record style already present in `docs/provenance.md:67-95`, `docs/policy/exposure-record.md:15-42`, `docs/memory-startup.md:36-50`, and the fixture manifest. Preserve the exact existing commit/archive pin in `pins.txt:12-13`; record the actual upstream notice and fixture recipe, and keep source identity distinct from permission. Do not claim physical hardware accuracy from a software-suite match.

## Shared Patterns

- **Guest state ownership:** `core/internal.h` owns machine state; keep host watches and presentation outside snapshots and guest digests.
- **Request and refusal:** leave an ungranted/unavailable CPU request pending; use the existing `LYNX_STOP_UNAVAILABLE`/whole-instance refusal style.
- **Bounded progress:** preserve half-open guest budgets and explicit progress/max-bound checks in the suite harness.
- **Independent suite evidence:** validate actual linker symbols and guest-written completion/model evidence before reading or comparing result bytes; retain named negative controls.
- **Reproducible fixtures:** use the pinned cc65 build, temporary outputs, exact manifest identity and existing CTest/inventory gate.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `core/timer.c` (proposed) | model | event-driven | No timer bank or shared-prescaler state exists. Use `core/bus.c` only for pure decode style and `core/machine.c` for integration boundaries. |

## Metadata

**Analog search scope:** `core/`, `include/`, `tests/api/`, `tests/diag/`, `tests/gates/`, `tests/cmake/`, `fixtures/diagnostic/`, `fixtures/manifests/`, `scripts/`, and relevant `docs/`.
**Files scanned:** 29 tracked or proposed targets; all existing analog paths named above are tracked source.
**Pattern extraction date:** 2026-11-10
