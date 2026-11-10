# Phase 3: Memory Map, Direct-Load Startup and First Diagnostic - Pattern Map

**Mapped:** 2026-11-10 · **Analogs:** representative tracked sources checked below

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `core/bus.c`, `core/bus.h` | utility | request-response | `core/machine.c` bus seam | partial |
| `core/machine.c` | service | request-response | same | exact |
| `core/internal.h` | model | request-response | same | exact |
| `include/lynxplosion.h` | API | request-response | same | exact |
| `core/snapshot.c` | utility | transform | same | exact |
| `runner/main.c` | controller | file-I/O, request-response | same | exact |
| `runner/bundle.c`, `runner/bundle.h` | service | file-I/O | same | exact |
| `CMakeLists.txt` | config | batch | `lynx-run` target | role-match |
| independent consumer entry point | controller | file-I/O, request-response | `runner/main.c` | role-match |
| original ca65 diagnostic | fixture | transform | `fixtures/cpu/churn-loop.s` | role-match |
| diagnostic ld65 config / BLL header recipe | config / utility | transform | `fixtures/cpu/raw-0200.cfg`; no BLL analog | partial |
| diagnostic manifest | config | batch | `fixtures/manifests/cpu-churn-loop.manifest` | exact |
| `scripts/check.sh` | utility | batch | CPU fixture build step | role-match |
| API/map/watch/diagnostic tests | test | request-response, file-I/O | `tests/api/test_api.c`, `tests/det/runner_cli.cmake` | role-match |
| test registration and exact inventory | config | batch | `tests/api/tests.cmake`, `tests/inventory/fast.txt` | exact |
| claim/provenance/exposure docs; README if needed | utility | batch | corresponding current docs | role-match |

Fixture and consumer filenames remain planner choices. Keep generated binaries
in the build tree; current manifest and inventory gates need no changes unless
their contracts change.

## Pattern Assignments

### Core bus, API and loader

**Analog:** `core/machine.c:201-214,254-261`; `include/lynxplosion.h:183-211`.
One machine access seam performs a bus request, completes the CPU request, then
sets the next ready tick:

```c
const uint8_t data = bus_access(m, addr, m->cpu.req_data, m->cpu.req_flags,
                                grant + price.sample);
lynx__cpu_complete(&m->cpu, table, data, 0u);
m->cpu_ready_at = grant + price.ticks;
```

Route CPU reads/writes here; keep raw RAM inspection separate. A watch stops
after `lynx__cpu_complete`. No MAPCTL decoder exists (`core/bus.c:44-52` only
prices all addresses as RAM); `LYNX_STOP_WATCH` is reserved.

`core/machine.c:155-182` is a validate-before-power-on raw loader, not a BLL
analog. Retain preflight ordering but replace the public raw path; validate exact
container size, payload bounds and input alias before mutation. The core owns
content hashing (`core/machine.c:338-354`). `core/machine.c:311-323` checks a
whole read range before copying; preserve this for raw and mapped peeks.

### Snapshot and refusal tests

**Analog:** `core/snapshot.c:221-276,325-342`; `tests/api/test_api.c:516-558,1120-1175`.
Restore checks version, identity and payload before its no-failure apply section.
Tests prove refusal with whole-instance comparisons and prove continuation by
restoring mid-instruction, advancing, then byte-comparing with uninterrupted
execution. Bump format for new guest fields. Keep watch config/hits outside
guest digests and define restore/reset/load behavior separately.

### Runner, consumer and fixture

**Analogs:** `runner/main.c:216-244,264-270`; `runner/bundle.c:180-218,287-350`.
Runner owns file I/O, aligned allocation, public init/load/advance and cleanup;
the bundle derives identity from the core and labels `startup_tier = A`. Add
container/startup and payload address/length to its manifest; hash the full BLL
object. A consumer can copy the public API lifecycle, but must link only public
API and use independent diagnostic expectations. No PPM/WAV writer exists.

`fixtures/cpu/churn-loop.s:42-66` is the ca65 source/label pattern;
`fixtures/cpu/raw-0200.cfg:1-2` shows placement. `fixtures/manifests/cpu-churn-loop.manifest:1-11` records source/recipe/hash/rights/use; `scripts/check.sh:307-340` builds with pinned
ca65/ld65. No BLL header or symbol-addressed diagnostic analog exists; extend
the build step for BLL and `-Ln` output.

### Tests, inventory and claims

**Analogs:** `tests/api/tests.cmake:1-8,33-43`; `cmake/Tests.cmake:21-30,35-57`;
`tests/det/runner_cli.cmake:20-35,59-72`; `tests/det/test_det.c:432-496`.
Register each named test with command/labels/timeout and add it to sorted
`tests/inventory/fast.txt`; configure compares exact registered names/labels.
Runner fault controls check refusal output and no artifact creation. Diagnostic
checks need semantic progress and independent expected pixels; shared-core
digests establish repeatability only.

`scripts/check-claims.sh:10-20,72-80,130-149` binds every claim to the exact
inventory SHA-256. Refresh `docs/claims/skeleton-repeatability.claim` in the
same change as an inventory edit.

## No Analog Found

MAPCTL decoding, strict BS93/BLL parsing, absent-ROM status, completed-write
watch/rearm, original symbol-addressed diagnostic, BLL header recipe, and
public-consumer PPM/WAV output have no implementation analogs yet.

**Scope:** core, public header, runner, fixtures, tests, scripts, CMake, evidence
docs. Every existing analog above passed `git ls-files -- <path>` (tracked).
