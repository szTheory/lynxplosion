/* Core-internal declarations. Not part of the public API.
   External names here start with lynx__ (two underscores). */
#ifndef LYNX_INTERNAL_H
#define LYNX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "lynxplosion.h"

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
