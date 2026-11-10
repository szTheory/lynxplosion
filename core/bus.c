/* Bus-side cycle pricing: const tables and three pure functions. No writable
   state, no floating point, no pointer tables. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus.h"

/* Ticks per cycle, indexed [cost model][region][cycle kind].

   Model 0, flat5, is a PLACEHOLDER. Every cycle costs 5 ticks. The vendor
   documentation the project has seen describes normal RAM/ROM and Mikey
   register accesses at 5, page-mode opcode reads at 4, Suzy reads at 9 to 15
   and audio DPRAM accesses at 5 to 20; none of that is modelled here, and all-5
   makes straight-line code run about 20% slow. No claim cites ticks as Lynx
   timing evidence.

   Model 1, test_skew, is internal to the tests: distinct non-zero costs per
   kind, in the order of enum lynx__ck (opcode, operand, dummy_stream, dummy,
   read, write, rmw_read, stack_read, stack_write, pointer, vector). */
static const uint8_t lynx__cost[LYNX__COST_MODEL_COUNT][LYNX__REGION_COUNT][LYNX__CK_COUNT] = {
    /* flat5 */
    {{5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}},
    /* test_skew */
    {{4, 6, 3, 10, 7, 9, 8, 11, 12, 13, 14}},
};

_Static_assert(sizeof lynx__cost == (size_t)LYNX__COST_MODEL_COUNT * LYNX__REGION_COUNT * LYNX__CK_COUNT,
               "cost table dimensions");
_Static_assert(LYNX__CK_COUNT == 11, "the cost tables list 11 cycle kinds");

lynx__bus_timing lynx__bus_timing_of(uint8_t model, uint8_t region, uint8_t kind)
{
    lynx__bus_timing t;
    uint8_t m = model < LYNX__COST_MODEL_COUNT ? model : 0u;
    uint8_t r = region < LYNX__REGION_COUNT ? region : 0u;
    uint8_t k = kind < LYNX__CK_COUNT ? kind : 0u;
    t.ticks = lynx__cost[m][r][k];
    t.sample = 0u;
    return t;
}

uint8_t lynx__bus_region_of(uint16_t addr)
{
    (void)addr;
    return LYNX__REGION_RAM;
}

bool lynx__bus_cost_model_valid(uint32_t model)
{
    return model < (uint32_t)LYNX__COST_MODEL_COUNT;
}
