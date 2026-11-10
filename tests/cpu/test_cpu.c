/* CPU white-box tests: the profile tables and the cycle-stepped core on a flat
   test bus. Usage: lynx-test-cpu <case>. An unknown case exits 2. */

#include <stdio.h>
#include <string.h>

#include "check.h"
#include "cpu.h"
#include "cpu/flat_bus.h"

/* Static: the bus is 64 KiB plus the log, too big for the stack. */
static flat_bus g_bus;

/* ---- tables -------------------------------------------------------------- */

/* Checks one pair of table halves. Returns the number of failures, printing the
   differing opcodes; the message "differ set" names the failing check. */
static int check_tables(const lynx__cpu_decode *lo, const lynx__cpu_decode *hi, unsigned *differ_out,
                        unsigned *defined_out)
{
    unsigned op;
    unsigned differ = 0;
    unsigned defined = 0;
    unsigned excl = 0;
    int failures = 0;
    for (op = 0; op < 256u; op++) {
        bool differs = memcmp(&lo[op], &hi[op], sizeof lo[op]) != 0;
        bool expected = (op & 0x0Fu) == 7u || (op & 0x0Fu) == 0x0Fu;
        if (differs) {
            differ++;
        }
        if (!differs) {
            defined++;
        }
        if (differs != expected) {
            fprintf(stderr, "differ set: opcode %02X %s but expected %s\n", op,
                    differs ? "differs" : "is shared", expected ? "to differ" : "shared");
            failures++;
        }
        if (lo[op].vec == LYNX__V_EXCL_HW) {
            excl++;
        }
        if (hi[op].vec == LYNX__V_EXCL_HW) {
            excl++;
        }
    }
    if (differ != 32u) {
        fprintf(stderr, "differ set: size %u, expected 32\n", differ);
        failures++;
    }
    /* CB and DB: 1-byte NOP, no poll, hardware-exclusion vec, on both profiles. */
    {
        static const unsigned hw[2] = {0xCBu, 0xDBu};
        unsigned i;
        for (i = 0; i < 2u; i++) {
            const lynx__cpu_decode *row[2];
            unsigned h;
            row[0] = &lo[hw[i]];
            row[1] = &hi[hw[i]];
            for (h = 0; h < 2u; h++) {
                CHECK(row[h]->kind == LYNX__K_NOP);
                CHECK(row[h]->mode == LYNX__M_NOP1);
                CHECK((row[h]->flags & LYNX__F_NO_POLL) != 0u);
                CHECK(row[h]->vec == LYNX__V_EXCL_HW);
            }
        }
    }
    CHECK_EQ_U64(excl, 4u); /* two opcodes on two profiles, no other row */
    *differ_out = differ;
    *defined_out = defined;
    return failures;
}

static int case_tables(void)
{
    unsigned op;
    unsigned differ = 0;
    unsigned defined = 0;
    unsigned none_defined = 0;
    int failures;
    lynx__cpu c;

    failures = check_tables(lynx__cpu_decode_table[0], lynx__cpu_decode_table[1], &differ, &defined);
    check_failures += failures;

    for (op = 0; op < 256u; op++) {
        if (lynx__cpu_opcode_defined(0u, (uint8_t)op)) {
            none_defined++;
        }
        CHECK(lynx__cpu_opcode_defined(1u, (uint8_t)op));
        CHECK(lynx__cpu_opcode_defined(2u, (uint8_t)op));
        /* none is defined exactly where the two rows agree. */
        CHECK(lynx__cpu_opcode_defined(0u, (uint8_t)op) ==
              (memcmp(&lynx__cpu_decode_table[0][op], &lynx__cpu_decode_table[1][op],
                      sizeof lynx__cpu_decode_table[0][op]) == 0));
    }
    CHECK_EQ_U64(none_defined, 224u);
    CHECK(lynx__cpu_table(0u) == lynx__cpu_decode_table[0]);
    CHECK(lynx__cpu_table(1u) == lynx__cpu_decode_table[0]);
    CHECK(lynx__cpu_table(2u) == lynx__cpu_decode_table[1]);
    CHECK(lynx__cpu_table(3u) == NULL);

    /* A profile id above 2 is refused by the state check. */
    lynx__cpu_init(&c, 1u);
    CHECK(lynx__cpu_state_valid(&c, lynx__cpu_table(1u)));
    c.profile = 3u;
    CHECK(!lynx__cpu_state_valid(&c, lynx__cpu_table(1u)));

    printf("tables: differ-set size %u, defined-under-none count %u\n", differ, none_defined);
    CHECK_DONE();
}

/* ---- tracer -------------------------------------------------------------- */

static int case_tracer(void)
{
    static const uint8_t prog[4] = {0xA9u, 0x42u, 0xEAu, 0x03u};
    lynx__cpu c;
    const lynx__cpu_decode *t = lynx__cpu_table(2u);

    flat_bus_clear(&g_bus);
    flat_bus_load(&g_bus, 0x0200u, prog, 4u);
    lynx__cpu_init(&c, 2u);
    lynx__cpu_start_at(&c, t, 0x0200u);

    /* LDA #$42 */
    CHECK(c.req_addr == 0x0200u && c.req_kind == LYNX__CK_OPCODE);
    CHECK(flat_bus_step_instruction(&g_bus, &c, t));
    CHECK_EQ_HEX(c.a, 0x42u);
    CHECK_EQ_HEX(c.pc, 0x0202u);
    CHECK((c.p & 0x82u) == 0u); /* N and Z clear */
    CHECK_EQ_U64(g_bus.nlog, 2u);
    CHECK(g_bus.log[0].addr == 0x0200u && g_bus.log[0].kind == LYNX__CK_OPCODE);
    CHECK(g_bus.log[1].addr == 0x0201u && g_bus.log[1].kind == LYNX__CK_OPERAND);
    CHECK((g_bus.log[1].flags & LYNX__CF_POLL) != 0u);
    CHECK((g_bus.log[1].flags & LYNX__CF_LAST) != 0u);
    CHECK((g_bus.log[0].flags & LYNX__CF_POLL) == 0u);

    /* NOP $EA */
    CHECK(flat_bus_step_instruction(&g_bus, &c, t));
    CHECK_EQ_U64(g_bus.nlog, 4u);
    CHECK(g_bus.log[2].addr == 0x0202u && g_bus.log[2].kind == LYNX__CK_OPCODE);
    CHECK(g_bus.log[3].addr == 0x0203u && g_bus.log[3].kind == LYNX__CK_DUMMY_STREAM);
    CHECK((g_bus.log[3].flags & LYNX__CF_POLL) != 0u);
    CHECK_EQ_HEX(c.pc, 0x0203u);

    /* $03: one cycle, no poll */
    CHECK(flat_bus_step_instruction(&g_bus, &c, t));
    CHECK_EQ_U64(g_bus.nlog, 5u);
    CHECK(g_bus.log[4].addr == 0x0203u && g_bus.log[4].kind == LYNX__CK_OPCODE);
    CHECK((g_bus.log[4].flags & LYNX__CF_POLL) == 0u);
    CHECK_EQ_HEX(c.pc, 0x0204u);
    CHECK(lynx__cpu_at_boundary(&c));
    CHECK(!g_bus.overrun);
    CHECK(lynx__cpu_state_valid(&c, t));
    printf("tracer: 5 cycles, 3 instructions\n");
    CHECK_DONE();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: lynx-test-cpu <case>\n");
        return 2;
    }
    if (strcmp(argv[1], "tables") == 0) {
        return case_tables();
    }
    if (strcmp(argv[1], "tracer") == 0) {
        return case_tracer();
    }
    fprintf(stderr, "lynx-test-cpu: unknown case '%s'\n", argv[1]);
    return 2;
}
