/* Core-internal declarations. Not part of the public API.
   External names here start with lynx__ (two underscores). */
#ifndef LYNX_INTERNAL_H
#define LYNX_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus.h"
#include "cpu.h"
#include "lynxplosion.h"

/* Snapshot format version 3 (core/snapshot.c documents the layout). */
#define LYNX__SNAPSHOT_MAGIC "LYNXSNAP" /* 8 bytes, no terminating NUL in the blob */
#define LYNX__SNAPSHOT_MAGIC_LEN 8u
#define LYNX__SNAPSHOT_FORMAT 3u
#define LYNX__BUILD_ID_FIELD 128u
#define LYNX__SNAPSHOT_HEADER_LEN 252u  /* prior header + format/startup/entry/object identity */
#define LYNX__SNAPSHOT_PAYLOAD_LEN 65568u /* 32 bytes of CPU and clock state + 65536 RAM */

/* The payload layout belongs to the format number: a layout change without a
   format bump stops the build. */
_Static_assert(LYNX__SNAPSHOT_FORMAT != 3u || LYNX__SNAPSHOT_PAYLOAD_LEN == 65568u,
               "snapshot format 3 has a 65568-byte payload; bump the format to change it");

/* Build identity string baked at configure time (core/build_id.c.in). */
extern const char lynx__build_id[];

#define LYNX__MAGIC 0x4C594E58u

/* No bus cycle starts whose end could come within this many ticks of the
   64-bit guest-time limit. Larger than any cycle cost (costs are uint8_t). */
#define LYNX__TICK_HEADROOM 0x10000u

#define LYNX__TIMER_COUNT 8u
#define LYNX__TIMER_IMPLEMENTED_COUNT LYNX__TIMER_COUNT
#define LYNX__TIMER_PHASE_FIXED_ZERO_1 1u

typedef struct lynx__timer {
    uint8_t backup;
    uint8_t count;
    uint8_t control_a;
    uint8_t control_b;
} lynx__timer;

/* The last applied event timestamp distinguishes an edge pending exactly at
   guest_ticks from an edge already dispatched before a same-tick CPU action. */
typedef struct lynx__timer_bank {
    lynx__timer timers[LYNX__TIMER_COUNT];
    uint8_t pending;
    uint8_t phase_policy;
    uint8_t last_event_valid;
    uint64_t last_event_tick;
} lynx__timer_bank;

/* Private scheduler ordering contract. The comparator is shared with the
   scheduler-order test so traversal permutations exercise production logic. */
typedef struct lynx__machine_event {
    uint64_t tick;
    uint8_t priority;
    uint8_t sequence;
} lynx__machine_event;

enum {
    LYNX__MACHINE_EVENT_TIMER_PRIORITY = 0u,
    LYNX__MACHINE_EVENT_CPU_PRIORITY = 1u,
    LYNX__MACHINE_EVENT_SEQUENCE = 0u,
};

/* Host-side debugger observations are deliberately separate from guest
   machine state. Snapshot serialization and guest digests omit this object. */
typedef struct lynx__host_observation {
    uint32_t watch_armed;
    uint32_t watch_addr;
    uint32_t watch_len;
    lynx_watch_event watch_event;
} lynx__host_observation;

/* One machine instance. The caller's buffer is the only mutable state.
   Init zero-fills the whole struct, so padding bytes are always zero. */
struct lynx_machine {
    uint32_t magic;
    lynx_config cfg;
    uint8_t ram[LYNX_ADDRESS_SPACE];
    lynx__cpu cpu;
    uint8_t cost_model; /* LYNX__COST_*: prices every bus cycle; flat5 after power-on */
    uint32_t has_content, content_addr, content_len;
    uint32_t content_format, startup_tier, entry_addr, object_len;
    uint8_t content_sha256[32]; /* SHA-256 of the accepted image bytes; zero before a load */
    uint8_t mapctl; /* provisional byte latch; all written bits are retained */
    uint8_t cpu_bus_held; /* private deterministic grant gate; no public sleep claim */
    uint64_t guest_ticks;       /* the only clock */
    uint64_t cpu_ready_at;      /* guest tick at which the CPU's next cycle may be granted */
    lynx__timer_bank timer_bank;
    lynx__host_observation host_observation;
};

/* Applies the configured RAM power-on policy, puts the CPU in its declared
   power-on state, resets the cost model to flat5 and clears guest time and
   the content fields. Used by init and by load. */
void lynx__power_on(lynx_machine *m);

/* Test seam: select the cycle cost model of an initialised instance. An id
   this build does not define (or a NULL or uninitialised machine) is refused
   with LYNX_ERR_ARGUMENT and nothing is written. Reset and load return the
   model to flat5. */
lynx_status lynx__machine_set_cost_model(struct lynx_machine *m, uint32_t model);

void lynx__timer_init(lynx__timer_bank *bank);
bool lynx__timer_next(const lynx__timer_bank *bank, uint64_t now, uint64_t *tick);
void lynx__timer_apply(lynx__timer_bank *bank, uint64_t tick);
void lynx__timer_mark_tick(lynx__timer_bank *bank, uint64_t tick);
bool lynx__timer_available(uint16_t addr);
uint8_t lynx__timer_peek(const lynx__timer_bank *bank, uint16_t addr);
bool lynx__timer_write(lynx__timer_bank *bank, uint16_t addr, uint8_t value);
bool lynx__machine_event_before(lynx__machine_event a, lynx__machine_event b);

#endif /* LYNX_INTERNAL_H */
