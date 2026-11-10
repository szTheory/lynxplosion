/* Core-internal declarations. Not part of the public API.
   External names here start with lynx__ (two underscores). */
#ifndef LYNX_INTERNAL_H
#define LYNX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "bus.h"
#include "cpu.h"
#include "lynxplosion.h"

/* Snapshot format version 2 (core/snapshot.c documents the layout). */
#define LYNX__SNAPSHOT_MAGIC "LYNXSNAP" /* 8 bytes, no terminating NUL in the blob */
#define LYNX__SNAPSHOT_MAGIC_LEN 8u
#define LYNX__SNAPSHOT_FORMAT 2u
#define LYNX__BUILD_ID_FIELD 128u
#define LYNX__SNAPSHOT_HEADER_LEN 236u  /* 8+4+4+128+4+4+8+32+4+4+4+32 */
#define LYNX__SNAPSHOT_PAYLOAD_LEN 65568u /* 32 bytes of CPU and clock state + 65536 RAM */

/* The payload layout belongs to the format number: a layout change without a
   format bump stops the build. */
_Static_assert(LYNX__SNAPSHOT_FORMAT != 2u || LYNX__SNAPSHOT_PAYLOAD_LEN == 65568u,
               "snapshot format 2 has a 65568-byte payload; bump the format to change it");

/* Build identity string baked at configure time (core/build_id.c.in). */
extern const char lynx__build_id[];

#define LYNX__MAGIC 0x4C594E58u

/* No bus cycle starts whose end could come within this many ticks of the
   64-bit guest-time limit. Larger than any cycle cost (costs are uint8_t). */
#define LYNX__TICK_HEADROOM 0x10000u

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
    uint64_t guest_ticks;       /* the only clock */
    uint64_t cpu_ready_at;      /* guest tick at which the CPU's next cycle may be granted */
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

#endif /* LYNX_INTERNAL_H */
