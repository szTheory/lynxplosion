/* Core-internal declarations. Not part of the public API.
   External names here start with lynx__ (two underscores). */
#ifndef LYNX_INTERNAL_H
#define LYNX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "lynxplosion.h"

/* Snapshot format version 1 (core/snapshot.c documents the layout). */
#define LYNX__SNAPSHOT_MAGIC "LYNXSNAP" /* 8 bytes, no terminating NUL in the blob */
#define LYNX__SNAPSHOT_MAGIC_LEN 8u
#define LYNX__SNAPSHOT_FORMAT 1u
#define LYNX__BUILD_ID_FIELD 128u
#define LYNX__SNAPSHOT_HEADER_LEN 236u  /* 8+4+4+128+4+4+8+32+4+4+4+32 */
#define LYNX__SNAPSHOT_PAYLOAD_LEN 65552u /* 2+6+8+65536 */

/* Build identity string baked at configure time (core/build_id.c.in). */
extern const char lynx__build_id[];

#define LYNX__MAGIC 0x4C594E58u
#define LYNX__TICKS_PER_STEP 4u
#define LYNX__HALT_OPCODE 0xDBu

/* One machine instance. The caller's buffer is the only mutable state.
   Init zero-fills the whole struct, so padding bytes are always zero. */
struct lynx_machine {
    uint32_t magic;
    lynx_config cfg;
    uint8_t ram[LYNX_ADDRESS_SPACE];
    uint16_t pc;
    uint8_t a, x, y, s, p;
    uint8_t halted;
    uint32_t has_content, content_addr, content_len;
    uint8_t content_sha256[32]; /* SHA-256 of the accepted image bytes; zero before a load */
    uint64_t guest_ticks;
};

/* Applies the configured RAM power-on policy and clears registers, halt
   flag, guest time and the content fields. Used by init and by load. */
void lynx__power_on(lynx_machine *m);

/* One provisional machine step (replaced by the CPU in Phase 2). */
void lynx__skeleton_step(lynx_machine *m);

/* Number of whole steps a budget buys, with guest time kept below the
   64-bit limit. */
uint64_t lynx__budget_steps(uint64_t guest_ticks, uint64_t budget);

#endif /* LYNX_INTERNAL_H */
