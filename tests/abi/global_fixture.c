/* Negative control for core.nm_globals: one mutable file-scope integer,
   written by a function, which the check must report as writable data. */
#include <stdint.h>

uint32_t lynx__abi_global(uint32_t step);

static uint32_t counter;

uint32_t lynx__abi_global(uint32_t step)
{
    counter += step;
    return counter;
}
