/* The two profile tables, expanded from cpu_profile.def, and the profile
   queries. Integer-only const data; nothing here is writable. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "cpu.h"

/* Row flag names as the .def writes them. */
#define F_NO_POLL LYNX__F_NO_POLL
#define F_DEC_EXTRA LYNX__F_DEC_EXTRA
#define F_BITOP_SLOT LYNX__F_BITOP_SLOT
#define F_UNV_CYCLES LYNX__F_UNV_CYCLES
#define F_UNV_SHAPE LYNX__F_UNV_SHAPE

/* Expansion 1: the table. A designated initialiser written twice for the same
   index is an override warning, which -Werror turns into a compile error, so a
   duplicated opcode row cannot build. */
#define OP(lynx__o, lynx__k, lynx__m, lynx__f, lynx__v)                                            \
    [0][lynx__o] = {LYNX__K_##lynx__k, LYNX__M_##lynx__m, (lynx__f), LYNX__V_##lynx__v},            \
    [1][lynx__o] = {LYNX__K_##lynx__k, LYNX__M_##lynx__m, (lynx__f), LYNX__V_##lynx__v},
#define OPX(lynx__o, lynx__k1, lynx__m1, lynx__f1, lynx__v1, lynx__k2, lynx__m2, lynx__f2,         \
            lynx__v2)                                                                              \
    [0][lynx__o] = {LYNX__K_##lynx__k1, LYNX__M_##lynx__m1, (lynx__f1), LYNX__V_##lynx__v1},        \
    [1][lynx__o] = {LYNX__K_##lynx__k2, LYNX__M_##lynx__m2, (lynx__f2), LYNX__V_##lynx__v2},
const lynx__cpu_decode lynx__cpu_decode_table[2][256] = {
#include "cpu_profile.def"
};
#undef OP
#undef OPX

/* Expansion 2: row counts. Every opcode appears once (no override above), so
   256 rows in total means no opcode is missing; the shared and split counts
   pin the 224 and 32 split. */
#define OP(lynx__o, lynx__k, lynx__m, lynx__f, lynx__v) +1
#define OPX(lynx__o, lynx__k1, lynx__m1, lynx__f1, lynx__v1, lynx__k2, lynx__m2, lynx__f2,         \
            lynx__v2)                                                                              \
    +0
enum { LYNX__SHARED_ROWS = 0
#include "cpu_profile.def"
};
#undef OP
#undef OPX
#define OP(lynx__o, lynx__k, lynx__m, lynx__f, lynx__v) +0
#define OPX(lynx__o, lynx__k1, lynx__m1, lynx__f1, lynx__v1, lynx__k2, lynx__m2, lynx__f2,         \
            lynx__v2)                                                                              \
    +1
enum { LYNX__SPLIT_ROWS = 0
#include "cpu_profile.def"
};
#undef OP
#undef OPX

_Static_assert(LYNX__SHARED_ROWS == 224, "224 shared rows");
_Static_assert(LYNX__SPLIT_ROWS == 32, "32 profile-dependent rows");
_Static_assert(LYNX__SHARED_ROWS + LYNX__SPLIT_ROWS == 256, "every opcode has one row");
_Static_assert(sizeof(lynx__cpu_decode) == 4u, "a decode row is 4 bytes");
_Static_assert(LYNX__K_COUNT < 256 && LYNX__M_COUNT < 256, "kind and mode fit a byte");

const lynx__cpu_decode *lynx__cpu_table(uint32_t profile)
{
    if (profile <= 1u) {
        return lynx__cpu_decode_table[0];
    }
    if (profile == 2u) {
        return lynx__cpu_decode_table[1];
    }
    return NULL;
}

bool lynx__cpu_opcode_defined(uint32_t profile, uint8_t opcode)
{
    if (profile == 0u) {
        return memcmp(&lynx__cpu_decode_table[0][opcode], &lynx__cpu_decode_table[1][opcode],
                      sizeof(lynx__cpu_decode)) == 0;
    }
    return profile <= 2u;
}
