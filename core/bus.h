/* The bus side of pricing: what one granted cycle costs. A pure function over
   const integer tables indexed by (cost model, region, cycle kind). The CPU
   never calls it and holds no cycle count or cost table; the machine loop asks
   for the price of the request the CPU posted and advances guest time by it.

   Cost model 0, flat5, is a PLACEHOLDER: five ticks everywhere. No claim may
   cite consumed ticks or cycle counts as Lynx timing evidence. Model 1,
   test_skew, is internal to the tests and the budget-split checks: its
   per-kind costs are distinct so a wrong price shows up as a wrong total. */
#ifndef LYNX_BUS_H
#define LYNX_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"

/* Cost model ids. An id at or above LYNX__COST_MODEL_COUNT is invalid. */
enum { LYNX__COST_FLAT5 = 0, LYNX__COST_TEST_SKEW = 1, LYNX__COST_MODEL_COUNT = 2 };

/* Address regions used for timing. flat5 still prices every CPU access as a
   PLACEHOLDER five ticks; route decoding is kept separate from timing. */
enum { LYNX__REGION_RAM = 0, LYNX__REGION_COUNT = 1 };

/* CPU-visible destination selected by MAPCTL. Unsupported destinations are
   kept distinct so execution and inspection cannot fall back to backing RAM. */
typedef enum lynx__bus_route {
    LYNX__BUS_ROUTE_RAM = 0,
    LYNX__BUS_ROUTE_SUZY = 1,
    LYNX__BUS_ROUTE_MIKEY = 2,
    LYNX__BUS_ROUTE_ROM = 3,
    LYNX__BUS_ROUTE_VECTOR = 4,
    LYNX__BUS_ROUTE_MAPCTL = 5
} lynx__bus_route;

/* The price of one cycle: ticks the cycle lasts and the offset inside it at
   which the data is sampled (0 in Phase 2). */
typedef struct lynx__bus_timing {
    uint8_t ticks;
    uint8_t sample;
} lynx__bus_timing;

/* Price of a cycle of the given kind in the given region under the given
   model. The caller must have checked lynx__bus_cost_model_valid first; an
   out-of-range argument is clamped to index 0 so a table is never read out of
   bounds, but the result then means nothing. */
lynx__bus_timing lynx__bus_timing_of(uint8_t model, uint8_t region, uint8_t kind);

/* Pure CPU-side decoder. MAPCTL bits 0–3 select RAM over the four windows;
   FFF8 and FFF9 are fixed exceptions. */
lynx__bus_route lynx__bus_decode(uint8_t mapctl, uint16_t addr);

/* The region an address belongs to. Timing stays flat5/PLACEHOLDER here. */
uint8_t lynx__bus_region_of(uint16_t addr);

/* Narrow VIDEO_POLICY1 page-mode candidate: only a same-page, mapped-RAM
   opcode fetch for the conservatively whitelisted NOP opcode can be priced at
   four ticks. page_latch is zero when no prior CPU RAM access is eligible and
   otherwise stores the previous 256-byte page plus one. */
bool lynx__bus_page_opcode_eligible(uint8_t mapctl, uint8_t page_latch,
                                    uint16_t addr, uint8_t kind, uint8_t opcode,
                                    uint32_t profile);

/* True for a cost model id this build defines. Takes the full-width value so a
   snapshot field or a setter argument is checked before it is narrowed. */
bool lynx__bus_cost_model_valid(uint32_t model);

#endif /* LYNX_BUS_H */
