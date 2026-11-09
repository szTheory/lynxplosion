/* Negative control for core.nm_undefined: a library that calls into libc. The
   calls to time and malloc leave undefined symbols that no allowlist entry
   covers. The volatile pointer keeps the compiler from removing the
   allocation. This file lives outside core/, so it may include time.h and
   stdlib.h. */
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

uint32_t lynx__abi_libc(void);

uint32_t lynx__abi_libc(void)
{
    volatile unsigned char *p = malloc(1);
    uint32_t t = (uint32_t)time(0);
    if (p != 0) {
        p[0] = 1;
        t += p[0];
    }
    free((void *)p);
    return t;
}
