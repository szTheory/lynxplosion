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

/* Address regions. Phase 2 has one: everything is RAM-priced. Page mode, Suzy,
   Mikey and audio memory regions arrive with the device phases. */
enum { LYNX__REGION_RAM = 0, LYNX__REGION_COUNT = 1 };

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

/* The region an address belongs to. Always LYNX__REGION_RAM in Phase 2. */
uint8_t lynx__bus_region_of(uint16_t addr);

/* True for a cost model id this build defines. Takes the full-width value so a
   snapshot field or a setter argument is checked before it is narrowed. */
bool lynx__bus_cost_model_valid(uint32_t model);

#endif /* LYNX_BUS_H */
