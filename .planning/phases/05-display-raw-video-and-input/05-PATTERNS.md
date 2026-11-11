# Phase 5: Display, Raw Video and Input - Pattern Map

**Mapped:** 2026-11-11
**Files analyzed:** 13 proposed or implied files
**Analogs found:** 13 / 13 assigned (11 exact/role matches; 2 partial matches)

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `core/video.c` (new) | model | event-driven | `core/timer.c` | role-match |
| `core/input.c` (new) | model | request-response | `core/machine.c`, `core/bus.c` | partial |
| `core/machine.c` (modify) | controller | event-driven | `core/machine.c` | exact |
| `core/bus.c` (modify if routing needs it) | utility | transform | `core/bus.c` | exact |
| `core/snapshot.c` (modify) | utility | transform | `core/snapshot.c` | exact |
| `include/lynxplosion.h` (modify) | utility | request-response | `include/lynxplosion.h` | exact |
| `fixtures/diagnostic/input_probe.s` (new; inferred name) | test | event-driven | `fixtures/diagnostic/first.s` | role-match |
| `fixtures/manifests/input-probe.manifest` (new; inferred name) | config | file-I/O | `fixtures/manifests/first-diagnostic.manifest` | role-match |
| `consumer/main.c` (modify) | utility | file-I/O | `consumer/main.c` | exact |
| `tests/api/test_api.c` (modify) | test | request-response | `tests/api/test_api.c` | exact |
| `tests/api/tests.cmake` (modify) | test | batch | `tests/api/tests.cmake` | exact |
| `fixtures/suites/page-mode-wrapper.s` (new; inferred name) | test | event-driven | `fixtures/suites/timers-wrapper.s` | role-match |
| `scripts/build-suites.sh` (modify) | utility | batch | `scripts/build-suites.sh` | exact |

All analog paths above were confirmed by `git ls-files`; no gitignored mirror paths are used.

## Pattern Assignments

### `core/video.c` (model, event-driven)

**Analog:** `core/timer.c` (role-match; timed instance-owned event source, not video behavior).

The timer module derives deadlines from machine guest time, finds the earliest eligible event and applies a bounded event step. Its current phase boundary is explicit:

```c
bool lynx__timer_next(const lynx__timer_bank *bank, uint64_t now, uint64_t *tick)
{
    ...
    const uint64_t candidate = now + delta;
    if (!found || candidate < earliest) {
        found = true;
        earliest = candidate;
    }
    ...
    *tick = earliest;
}
```

Source: `core/timer.c:183-221`; event application is separately bounded in `core/timer.c:224-247`. Keep display/refresh state per machine, expose its next event and apply it at a guest tick. Reuse the scheduler shape, not timer constants or presumed video cadence/costs.

### `core/input.c` (model, request-response)

**Analog:** `core/machine.c` bus request path and `core/bus.c` route decoder (partial; there is no existing guest-readable Suzy input-device model).

`core/bus.c` makes address routing a pure decode from the address and MAPCTL:

```c
if (addr >= 0xFC00u && addr <= 0xFCFFu) {
    return (mapctl & 0x01u) != 0u ? LYNX__BUS_ROUTE_RAM : LYNX__BUS_ROUTE_SUZY;
}
```

Source: `core/bus.c:33-53`. The machine samples a successful bus access at `grant + price.sample` and only then completes the CPU request (`core/machine.c:511-538`). Keep logical levels as copied machine state and answer JOYSTICK/SWITCHES through that route at the documented bus sample tick. Keep timeline parsing, event scripts and controller discovery outside the core. MAPCTL `0C` preserves the Suzy/Mikey overlays; `0F` routes those addresses to RAM and is only a negative setup control.

### `core/machine.c` (controller, event-driven)

**Analog:** existing event loop in `core/machine.c` (exact integration point).

The loop merges timer events and ready CPU grants against one exclusive target. It orders same-tick work and jumps to the target only when no eligible event remains:

```c
while (m->guest_ticks < target) {
    lynx__machine_event next = {target, UINT8_MAX, UINT8_MAX};
    ...
    if (lynx__timer_next(&m->timer_bank, m->guest_ticks, &timer_tick) &&
        timer_tick < target) { ... }
    ...
    if (!has_event) {
        machine_set_guest_tick(m, target);
        break;
    }
    if (next.priority == (uint8_t)LYNX__MACHINE_EVENT_TIMER_PRIORITY) {
        machine_set_guest_tick(m, next.tick);
        lynx__timer_apply(&m->timer_bank, next.tick);
        continue;
    }
}
```

Source: `core/machine.c:471-506`. Add display/refresh candidates to this arbitration and define their priority and no-replay behavior. Preserve the half-open `next.tick < target` rule, retained CPU request while held, and the completed-event-before-watch/frame-stop contract. Do not add a second scheduler in `video.c`.

### `core/bus.c` (utility, transform)

**Analog:** its own pure route and price functions (exact local style).

```c
lynx__bus_route lynx__bus_decode(uint8_t mapctl, uint16_t addr)
{
    if (addr == 0xFFF8u) return LYNX__BUS_ROUTE_RAM;
    if (addr == 0xFFF9u) return LYNX__BUS_ROUTE_MAPCTL;
    ...
}
```

Source: `core/bus.c:33-53`; pricing is a separate pure function at `core/bus.c:56-65`. Keep routing, timing price and access side effects separate. The current `flat5` timing table is explicitly a placeholder (`core/bus.c:10-21`); do not copy it as display/refresh evidence or tune it to make timer oracle bytes pass.

### `core/snapshot.c` (utility, transform)

**Analog:** field-by-field little-endian save, validate-then-apply restore (exact).

Save validates machine, loaded content, capacity and disjoint caller output before writing; state is encoded field by field rather than by struct copy:

```c
if (cap < SNAPSHOT_TOTAL) return LYNX_ERR_CAPACITY;
if (!ranges_disjoint(buf, SNAPSHOT_TOTAL, m, sizeof *m)) return LYNX_ERR_ARGUMENT;
...
put_u64(payload + P_TICKS, m->guest_ticks);
memcpy(payload + P_RAM, m->ram, LYNX_ADDRESS_SPACE);
```

Source: `core/snapshot.c:255-287`. Restore checks exact length, identity, payload hash and candidate field validity before its single apply section (`core/snapshot.c:329-388`, `439-488`). Add all in-progress DMA, row/palette/tick, and current-input fields required for identical continuation to format 5; validate candidates before changing the machine and retain refusal atomicity. Do not serialize host pointers or the host replay script.

### `include/lynxplosion.h` (utility, request-response)

**Analog:** existing caller-owned public inspection, advance and snapshot API contracts (exact).

```c
lynx_status lynx_advance(lynx_machine *m, uint64_t budget, uint64_t *consumed,
                         lynx_stop_reason *stop);
```

Source: `include/lynxplosion.h:204-205`. Document new frame-stop and input calls beside this API using the same precise rules: caller supplies buffers, capacities/spans/overlap are checked before guest progress, no pointer is retained, success and stop metadata are defined, and refusals leave instance and output untouched. Snapshot documentation currently describes format 4 (`include/lynxplosion.h:289-317`); update it to format 5 when the serialized state changes.

### `fixtures/diagnostic/input_probe.s` (test, event-driven)

**Analog:** `fixtures/diagnostic/first.s` (original tier-A guest fixture).

The existing fixture enters as a direct BS93 payload and explicitly enables its required MAPCTL RAM windows:

```asm
start:
        ; Tier A enters with MAPCTL zero.
        lda #$0C
        sta $FFF9
```

It uses named exported result/data symbols and a final completion write (`fixtures/diagnostic/first.s:18-22`, `110-130`). Build the new guest from original project source with a schema/versioned result block. Explicitly initialize MAPCTL, orientation and display/timer state, advance semantic progress, respond visibly to recorded logical input, and leave a clear done/error result. Preserve a stable loop after completion; use the public consumer to prove observation.

### `fixtures/manifests/input-probe.manifest` (config, file-I/O)

**Analog:** `fixtures/manifests/first-diagnostic.manifest` (role-match).

```text
source = fixtures/diagnostic/first.s (project-authored)
recipe = pinned ca65 ... and ld65 ...
sha256 = ...
licence = project source all rights reserved (O5 open)
allowed_use = original diagnostic fixture test content
evidence_tier = R
```

Source: `fixtures/manifests/first-diagnostic.manifest:1-11`. Record source, exact pinned toolchain/recipe, object identity, rights/notices, allowed use and evidence tier for each newly built fixture. A digest binds identity; it does not establish accuracy or rights.

### `consumer/main.c` (utility, file-I/O)

**Analog:** existing independent public-only consumer (exact extension point).

The consumer allocates aligned caller-owned machine storage, initializes through the public API, loads strict BLL and checks accepted identity (`consumer/main.c:260-302`). Its current driver slices bounded advances and checks guest progress before inspecting fixture symbols through public reads:

```c
if (lynx_advance(machine, budget, &consumed, &stop) != LYNX_OK || consumed > budget ||
    lynx_guest_ticks(machine, &after) != LYNX_OK) { ... }
...
if (lynx_read_memory(machine, where->result, result, sizeof result) != LYNX_OK) { ... }
```

Source: `consumer/main.c:327-365`. Add a distinct frame/replay mode that uses only `include/lynxplosion.h`, handles every intermediate frame before applying a ticked input edge, validates returned frame metadata/raw bytes independently, and writes PPM through existing output publication. Keep the Phase 3 snapshot/placeholder-silence mode intact. Do not open core internals in this executable.

### `tests/api/test_api.c` and `tests/api/tests.cmake` (test, request-response / batch)

**Analog:** named API cases plus exact CTest registration (exact).

The C test binary registers named cases in `cases[]` and dispatches by case name (`tests/api/test_api.c:6211-6309`). Add narrowly named assertions for frame edge/target, sentinel-preserving refusal, slice/uninterrupted/watch/restore equality, reset/load, multi-instance, input mask/edge/orientation and fault outcomes. The 6,309-line C file is a large-file analog; only the case table/targeted case blocks should guide the additions.

CTest lists each contract case explicitly and groups persistent faults around a clean positive plus exact expected failure text (`tests/api/tests.cmake:9-27`, `70-124`). Extend the exact inventory and no-skip/JUnit gates with semantic names. Each named mutant must fail its intended assertion after successful build; generic nonzero, timeout or compile failure is not a detected behavior fault.

### `fixtures/suites/page-mode-wrapper.s` (test, event-driven)

**Analog:** `fixtures/suites/timers-wrapper.s` (role-match; wrapper around unchanged pinned test source).

The wrapper includes the pinned suite, exports exact symbols and places a strict BLL entry around it (`fixtures/suites/timers-wrapper.s:1-27`). It initializes tier-A MAPCTL, marks progress/completion and leaves an idle loop after `_run_tests` (`fixtures/suites/timers-wrapper.s:28-82`). Follow that source-derived wrapper approach for `page-mode/tests.s`: keep the pinned source/oracle unchanged, export the result/completion symbols, and retain source/build/label identity checks. Do not copy the timer wrapper's offsets or interrupt special case without deriving their page-mode need.

### `scripts/build-suites.sh` (utility, batch)

**Analog:** existing pinned suite builder (exact extension point).

```sh
build_suite() {
  suite=$1
  wrapper="$ROOT/fixtures/suites/$suite-wrapper.s"
  ... ca65 ... -o "$OUT/$suite.o" "$wrapper"
  ... ld65 ... -Ln "$OUT/$suite.lbl" -o "$OUT/$suite.bll" "$OUT/$suite.o"
}

build_suite cpu
build_suite timers
```

Source: `scripts/build-suites.sh:131-150`. Reuse its pinned tool/hash, isolated-output, source-before/after, notice and build-record controls when adding the page-mode lane. Keep an explicit blocker-specific skip visible if wrapper preconditions make execution impossible; do not treat O1 alone as an execution blocker.

## Shared Patterns

### Guest-time events and bus access

**Sources:** `core/machine.c:471-538`, `core/timer.c:183-247`, `core/bus.c:33-65`
**Apply to:** `core/video.c`, `core/input.c`, `core/machine.c`, `core/bus.c`.

Machine guest ticks own event time. The scheduler arbitrates all device deadlines and CPU grants against the same half-open budget; bus access occurs at the selected grant/sample tick. Device code must not use host clocks, callbacks or an independent per-tick loop. Keep video/refresh costs and palette boundary as named, versioned software policy where research has no hardware value.

### Caller-owned output and atomic refusal

**Sources:** `include/lynxplosion.h:188-251`, `core/snapshot.c:255-267`, `core/machine.c:188-200`
**Apply to:** new frame API and `consumer/main.c`.

Follow the existing preflight/ownership contract: validate capacity, arithmetic, address spans and machine/output overlap before guest progress; publish payload only when the contract's completion condition succeeds; do not retain pointers. State and output sentinels must remain unchanged on refusal.

### Snapshot continuation

**Sources:** `core/snapshot.c:255-322`, `329-488`; `include/lynxplosion.h:289-320`
**Apply to:** `core/snapshot.c`, `include/lynxplosion.h`, snapshot API cases.

Canonical byte encoding, format identity, complete pre-validation and a no-failure apply step are current practice. Format 5 must carry every in-progress capture/DMA/line/palette and logical input field needed to reproduce the next frame; script/cursor remain host-owned.

### Fixture provenance and independent observation

**Sources:** `fixtures/diagnostic/first.s:1-11`, `fixtures/manifests/first-diagnostic.manifest:1-11`, `consumer/main.c:327-395`, `scripts/build-suites.sh:114-177`
**Apply to:** homebrew, manifest, consumer, suite harness and test inventory.

Use original source, pinned build recipe, linked labels, semantic guest progress and independently checked raw outputs. Preserve strict source identity and notice controls. Same-core output digests demonstrate repeatability only, not hardware accuracy.

## No Analog Found

| File | Closest partial analog | Missing behavior |
|---|---|---|
| `core/input.c` | `core/machine.c` bus grant plus `core/bus.c` device-route decode | No existing Suzy JOYSTICK/SWITCHES logical-level model or input orientation field. Treat the match as wiring/style only; follow phase evidence for masks, sample tick and explicit uncertainty labels. |
| `core/video.c` | `core/timer.c` event-source pattern | No existing raw frame, display DMA, row palette or capture buffer implementation. Use phase contract values and policies; do not infer them from timer behavior. |

## Metadata

**Analog search scope:** `core/`, `include/`, `fixtures/diagnostic/`, `fixtures/manifests/`, `fixtures/suites/`, `consumer/`, `tests/api/`, `scripts/`.
**Files scanned:** 12 tracked candidate analog files.
**Pattern extraction date:** 2026-11-11.
