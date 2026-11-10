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

static void run_tracer(void)
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
}

static int case_tracer(void)
{
    run_tracer();
    CHECK_DONE();
}

/* ---- cycles --------------------------------------------------------------- */

/* One expected bus cycle: address, write flag, cycle kind. */
typedef struct xc {
    uint16_t addr;
    uint8_t w, kind;
} xc;
/* A memory value to set before the run, or to check after it. */
typedef struct mv {
    uint16_t addr;
    uint8_t val, used;
} mv;

#define R(a, k) {(a), 0u, LYNX__CK_##k}
#define W(a, k) {(a), 1u, LYNX__CK_##k}
#define END {0u, 0u, 0xFFu}
#define M(a, v) {(a), (v), 1u}

/* One case: a program at pc (0x0200 when 0), start registers (S 0xFD and P
   0x34 when 0), memory to set, the hand-authored per-cycle list, and the state
   expected afterwards. src tags where the expected list comes from:
     SST-MODEL  the bus-sequence template of the SingleStepTests software model
                (a stand-in from another vendor's part, unverified for the Lynx)
     WDC / RKW  the W65C02S or Rockwell R65C02 datasheet cycle table
   Every dummy-cycle address is a model choice, unverified for the Lynx. */
typedef struct cycase {
    const char *group, *src;
    uint8_t profile;
    uint16_t pc;
    uint8_t prog[4];
    uint8_t a, x, y, s, p;
    mv mem[5];
    xc exp[10];
    uint16_t fpc;
    int fs;
    mv chk[3];
} cycase;

static unsigned g_tag_sst, g_tag_wdc, g_tag_rkw, g_tag_inf;

static void count_tag(const char *src)
{
    if (strcmp(src, "SST-MODEL") == 0) {
        g_tag_sst++;
    } else if (strcmp(src, "WDC") == 0) {
        g_tag_wdc++;
    } else if (strcmp(src, "RKW") == 0) {
        g_tag_rkw++;
    } else {
        g_tag_inf++;
    }
}

/* Runs one instruction of a case and compares the log cycle by cycle. */
static void run_cycase(const cycase *k)
{
    lynx__cpu c;
    const lynx__cpu_decode *t = lynx__cpu_table(k->profile);
    uint16_t pc = k->pc != 0u ? k->pc : 0x0200u;
    unsigned i;
    unsigned n = 0;
    unsigned before = (unsigned)check_failures;

    flat_bus_clear(&g_bus);
    for (i = 0; i < 4u; i++) {
        g_bus.mem[(uint16_t)(pc + i)] = k->prog[i];
    }
    for (i = 0; i < 5u; i++) {
        if (k->mem[i].used != 0u) {
            g_bus.mem[k->mem[i].addr] = k->mem[i].val;
        }
    }
    lynx__cpu_init(&c, k->profile);
    c.a = k->a;
    c.x = k->x;
    c.y = k->y;
    c.s = k->s != 0u ? k->s : 0xFDu;
    c.p = k->p != 0u ? k->p : 0x34u;
    lynx__cpu_start_at(&c, t, pc);
    if (!flat_bus_step_instruction(&g_bus, &c, t)) {
        fprintf(stderr, "cycles: %s: cycle bound exceeded\n", k->group);
        check_failures++;
        return;
    }
    while (n < 10u && k->exp[n].kind != 0xFFu) {
        n++;
    }
    if (g_bus.nlog != n) {
        fprintf(stderr, "cycles: %s: %u cycles, expected %u\n", k->group, g_bus.nlog, n);
        check_failures++;
    }
    for (i = 0; i < n && i < g_bus.nlog; i++) {
        const flat_entry *e = &g_bus.log[i];
        if (e->addr != k->exp[i].addr || e->write != k->exp[i].w || e->kind != k->exp[i].kind) {
            fprintf(stderr, "cycles: %s: cycle %u is %04X %s kind %u, expected %04X %s kind %u\n",
                    k->group, i + 1u, e->addr, e->write != 0u ? "write" : "read", e->kind,
                    k->exp[i].addr, k->exp[i].w != 0u ? "write" : "read", k->exp[i].kind);
            check_failures++;
        }
    }
    /* LAST rides the final cycle. The opcode fetch of a 1-cycle NOP cannot say so
       when it is posted (the opcode is not known yet), so it carries neither flag. */
    if (g_bus.nlog > 1u) {
        CHECK((g_bus.log[g_bus.nlog - 1u].flags & LYNX__CF_LAST) != 0u);
    }
    if (k->fpc != 0u && c.pc != k->fpc) {
        fprintf(stderr, "cycles: %s: pc %04X, expected %04X\n", k->group, c.pc, k->fpc);
        check_failures++;
    }
    if (k->fs >= 0 && c.s != (uint8_t)k->fs) {
        fprintf(stderr, "cycles: %s: s %02X, expected %02X\n", k->group, c.s, (unsigned)k->fs);
        check_failures++;
    }
    for (i = 0; i < 3u; i++) {
        if (k->chk[i].used != 0u && g_bus.mem[k->chk[i].addr] != k->chk[i].val) {
            fprintf(stderr, "cycles: %s: memory %04X is %02X, expected %02X\n", k->group,
                    k->chk[i].addr, g_bus.mem[k->chk[i].addr], k->chk[i].val);
            check_failures++;
        }
    }
    CHECK(lynx__cpu_state_valid(&c, t));
    CHECK(lynx__cpu_at_boundary(&c));
    count_tag(k->src);
    printf("cycles: %-40s %-9s %u cycles %s\n", k->group, k->src, n,
           (unsigned)check_failures == before ? "ok" : "FAILED");
}

static const cycase cycases[] = {
    /* implied and accumulator: INX */
    {.group = "implied", .src = "SST-MODEL", .profile = 2, .prog = {0xE8},
     .exp = {R(0x200, OPCODE), R(0x201, DUMMY_STREAM), END}, .fpc = 0x201, .fs = -1},
    /* immediate: LDA #$12, and the 2-byte reserved NOP $02 */
    {.group = "immediate", .src = "SST-MODEL", .profile = 2, .prog = {0xA9, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), END}, .fpc = 0x202, .fs = -1},
    {.group = "2-byte reserved NOP", .src = "SST-MODEL", .profile = 1, .prog = {0x02, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), END}, .fpc = 0x202, .fs = -1},
    /* 1-byte NOP $03 (one fetch, the only hardware-informed count) */
    {.group = "1-byte NOP", .src = "SST-MODEL", .profile = 2, .prog = {0x03},
     .exp = {R(0x200, OPCODE), END}, .fpc = 0x201, .fs = -1},
    /* zero page */
    {.group = "zero page read", .src = "SST-MODEL", .profile = 2, .prog = {0xA5, 0x34},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), END}, .fpc = 0x202, .fs = -1},
    {.group = "zero page write", .src = "SST-MODEL", .profile = 2, .prog = {0x85, 0x34}, .a = 0x5A,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), W(0x34, WRITE), END}, .fpc = 0x202, .fs = -1,
     .chk = {M(0x34, 0x5A)}},
    {.group = "zero page RMW", .src = "SST-MODEL", .profile = 2, .prog = {0xE6, 0x34},
     .mem = {M(0x34, 0x41)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), R(0x34, RMW_READ),
             W(0x34, WRITE), END},
     .fpc = 0x202, .fs = -1, .chk = {M(0x34, 0x42)}},
    /* zero page indexed */
    {.group = "zero page,X read", .src = "SST-MODEL", .profile = 2, .prog = {0xB5, 0x34}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x44, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "zero page,X read, 8-bit wrap", .src = "SST-MODEL", .profile = 2, .prog = {0xB5, 0x34},
     .x = 0xF0, .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x24, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "zero page,X write", .src = "SST-MODEL", .profile = 2, .prog = {0x95, 0x34}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), W(0x44, WRITE), END},
     .fpc = 0x202, .fs = -1},
    {.group = "zero page,Y read", .src = "SST-MODEL", .profile = 2, .prog = {0xB6, 0x34}, .y = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x44, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "zero page,Y write", .src = "SST-MODEL", .profile = 2, .prog = {0x96, 0x34}, .y = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), W(0x44, WRITE), END},
     .fpc = 0x202, .fs = -1},
    {.group = "zero page,X RMW", .src = "SST-MODEL", .profile = 2, .prog = {0xF6, 0x34}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x44, READ),
             R(0x44, RMW_READ), W(0x44, WRITE), END},
     .fpc = 0x202, .fs = -1, .chk = {M(0x44, 0x01)}},
    /* absolute */
    {.group = "absolute read", .src = "SST-MODEL", .profile = 2, .prog = {0xAD, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x1234, READ), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute write", .src = "SST-MODEL", .profile = 2, .prog = {0x8D, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), W(0x1234, WRITE), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute RMW", .src = "SST-MODEL", .profile = 2, .prog = {0xEE, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x1234, READ),
             R(0x1234, RMW_READ), W(0x1234, WRITE), END},
     .fpc = 0x203, .fs = -1, .chk = {M(0x1234, 0x01)}},
    /* absolute indexed */
    {.group = "absolute,X read, no page cross", .src = "SST-MODEL", .profile = 2,
     .prog = {0xBD, 0x34, 0x12}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x1244, READ), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,X read, page cross", .src = "SST-MODEL", .profile = 2,
     .prog = {0xBD, 0x34, 0x12}, .x = 0xF0,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY_STREAM),
             R(0x1324, READ), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,Y read, no page cross", .src = "SST-MODEL", .profile = 2,
     .prog = {0xB9, 0x34, 0x12}, .y = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x1244, READ), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,Y read, page cross", .src = "SST-MODEL", .profile = 2,
     .prog = {0xB9, 0x34, 0x12}, .y = 0xF0,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY_STREAM),
             R(0x1324, READ), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,X write", .src = "SST-MODEL", .profile = 2, .prog = {0x9D, 0x34, 0x12},
     .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY_STREAM),
             W(0x1244, WRITE), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,Y write", .src = "SST-MODEL", .profile = 2, .prog = {0x99, 0x34, 0x12},
     .y = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY_STREAM),
             W(0x1244, WRITE), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,X RMW shift, no page cross", .src = "SST-MODEL", .profile = 2,
     .prog = {0x1E, 0x34, 0x12}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x1244, READ),
             R(0x1244, RMW_READ), W(0x1244, WRITE), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,X RMW shift, page cross", .src = "SST-MODEL", .profile = 2,
     .prog = {0x1E, 0x34, 0x12}, .x = 0xF0,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY),
             R(0x1324, READ), R(0x1324, RMW_READ), W(0x1324, WRITE), END},
     .fpc = 0x203, .fs = -1},
    {.group = "absolute,X RMW inc/dec", .src = "SST-MODEL", .profile = 2,
     .prog = {0xFE, 0x34, 0x12}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY),
             R(0x1244, READ), R(0x1244, RMW_READ), W(0x1244, WRITE), END},
     .fpc = 0x203, .fs = -1},
    /* (zp,X) */
    {.group = "(zp,X) read", .src = "SST-MODEL", .profile = 2, .prog = {0xA1, 0x34}, .x = 0x10,
     .mem = {M(0x44, 0x78), M(0x45, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x44, POINTER),
             R(0x45, POINTER), R(0x5678, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "(zp,X) write", .src = "SST-MODEL", .profile = 2, .prog = {0x81, 0x34}, .x = 0x10,
     .mem = {M(0x44, 0x78), M(0x45, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x44, POINTER),
             R(0x45, POINTER), W(0x5678, WRITE), END},
     .fpc = 0x202, .fs = -1},
    {.group = "(zp,X) pointer wraps at $FF", .src = "SST-MODEL", .profile = 2, .prog = {0xA1, 0xF0},
     .x = 0x0F, .mem = {M(0xFF, 0x78), M(0x00, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0xF0, DUMMY), R(0xFF, POINTER),
             R(0x00, POINTER), R(0x5678, READ), END},
     .fpc = 0x202, .fs = -1},
    /* (zp),Y */
    {.group = "(zp),Y read, no page cross", .src = "SST-MODEL", .profile = 2, .prog = {0xB1, 0x34},
     .y = 0x10, .mem = {M(0x34, 0x78), M(0x35, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, POINTER), R(0x35, POINTER),
             R(0x5688, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "(zp),Y read, page cross", .src = "SST-MODEL", .profile = 2, .prog = {0xB1, 0x34},
     .y = 0x20, .mem = {M(0x34, 0xF0), M(0x35, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, POINTER), R(0x35, POINTER),
             R(0x201, DUMMY_STREAM), R(0x5710, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "(zp),Y pointer wraps at $FF", .src = "SST-MODEL", .profile = 2, .prog = {0xB1, 0xFF},
     .y = 0x01, .mem = {M(0xFF, 0x78), M(0x00, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0xFF, POINTER), R(0x00, POINTER),
             R(0x5679, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "(zp),Y write", .src = "SST-MODEL", .profile = 2, .prog = {0x91, 0x34}, .y = 0x10,
     .mem = {M(0x34, 0x78), M(0x35, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, POINTER), R(0x35, POINTER),
             R(0x201, DUMMY_STREAM), W(0x5688, WRITE), END},
     .fpc = 0x202, .fs = -1},
    /* (zp) */
    {.group = "(zp) read", .src = "SST-MODEL", .profile = 2, .prog = {0xB2, 0x34},
     .mem = {M(0x34, 0x78), M(0x35, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, POINTER), R(0x35, POINTER),
             R(0x5678, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "(zp) write", .src = "SST-MODEL", .profile = 2, .prog = {0x92, 0x34},
     .mem = {M(0x34, 0x78), M(0x35, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, POINTER), R(0x35, POINTER),
             W(0x5678, WRITE), END},
     .fpc = 0x202, .fs = -1},
    /* relative branches (Z is clear in the default P 0x34) */
    {.group = "relative, not taken", .src = "SST-MODEL", .profile = 2, .prog = {0xF0, 0x05},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), END}, .fpc = 0x202, .fs = -1},
    {.group = "relative, taken", .src = "SST-MODEL", .profile = 2, .prog = {0xD0, 0x05},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, DUMMY_STREAM), END},
     .fpc = 0x207, .fs = -1},
    {.group = "relative, taken backwards", .src = "SST-MODEL", .profile = 2, .prog = {0xD0, 0xFE},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, DUMMY_STREAM), END},
     .fpc = 0x200, .fs = -1},
    {.group = "relative, taken across a page", .src = "SST-MODEL", .profile = 2, .pc = 0x02FD,
     .prog = {0xD0, 0x05},
     .exp = {R(0x2FD, OPCODE), R(0x2FE, OPERAND), R(0x2FF, DUMMY_STREAM), R(0x204, DUMMY), END},
     .fpc = 0x304, .fs = -1},
    {.group = "relative, backwards across a page", .src = "SST-MODEL", .profile = 2, .pc = 0x0300,
     .prog = {0xD0, 0xFC},
     .exp = {R(0x300, OPCODE), R(0x301, OPERAND), R(0x302, DUMMY_STREAM), R(0x3FE, DUMMY), END},
     .fpc = 0x2FE, .fs = -1},
    {.group = "BRA", .src = "SST-MODEL", .profile = 2, .prog = {0x80, 0x05},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, DUMMY_STREAM), END},
     .fpc = 0x207, .fs = -1},
    {.group = "BRA across a page", .src = "SST-MODEL", .profile = 2, .pc = 0x02FD, .prog = {0x80, 0x05},
     .exp = {R(0x2FD, OPCODE), R(0x2FE, OPERAND), R(0x2FF, DUMMY_STREAM), R(0x204, DUMMY), END},
     .fpc = 0x304, .fs = -1},
    /* lynx2 BBR and BBS (zero page byte 0x34 holds the tested bits) */
    {.group = "BBR not taken", .src = "RKW", .profile = 2, .prog = {0x0F, 0x34, 0x05},
     .mem = {M(0x34, 0x01)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), R(0x34, DUMMY),
             R(0x202, OPERAND), END},
     .fpc = 0x203, .fs = -1},
    {.group = "BBR taken", .src = "RKW", .profile = 2, .prog = {0x0F, 0x34, 0x05},
     .mem = {M(0x34, 0x00)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), R(0x34, DUMMY),
             R(0x202, OPERAND), R(0x203, DUMMY_STREAM), END},
     .fpc = 0x208, .fs = -1},
    {.group = "BBR taken across a page", .src = "RKW", .profile = 2, .pc = 0x02FC,
     .prog = {0x0F, 0x34, 0x05}, .mem = {M(0x34, 0x00)},
     .exp = {R(0x2FC, OPCODE), R(0x2FD, OPERAND), R(0x34, READ), R(0x34, DUMMY),
             R(0x2FE, OPERAND), R(0x2FF, DUMMY_STREAM), R(0x2FF, DUMMY), END},
     .fpc = 0x304, .fs = -1},
    {.group = "BBS taken", .src = "RKW", .profile = 2, .prog = {0x8F, 0x34, 0x05},
     .mem = {M(0x34, 0x01)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), R(0x34, DUMMY),
             R(0x202, OPERAND), R(0x203, DUMMY_STREAM), END},
     .fpc = 0x208, .fs = -1},
    {.group = "RMB zero page", .src = "RKW", .profile = 2, .prog = {0x07, 0x34},
     .mem = {M(0x34, 0xFF)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), R(0x34, RMW_READ),
             W(0x34, WRITE), END},
     .fpc = 0x202, .fs = -1, .chk = {M(0x34, 0xFE)}},
    /* jumps and calls */
    {.group = "JMP absolute", .src = "SST-MODEL", .profile = 2, .prog = {0x4C, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), END}, .fpc = 0x1234, .fs = -1},
    {.group = "JMP (absolute)", .src = "SST-MODEL", .profile = 2, .prog = {0x6C, 0x34, 0x12},
     .mem = {M(0x1234, 0x78), M(0x1235, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x1234, POINTER),
             R(0x1235, POINTER), R(0x1235, DUMMY), END},
     .fpc = 0x5678, .fs = -1},
    {.group = "JMP (absolute) at $xxFF, no page wrap", .src = "SST-MODEL", .profile = 2,
     .pc = 0x0400, .prog = {0x6C, 0xFF, 0x02},
     .mem = {M(0x02FF, 0x78), M(0x0300, 0x56), M(0x0200, 0x99)},
     .exp = {R(0x400, OPCODE), R(0x401, OPERAND), R(0x402, OPERAND), R(0x2FF, POINTER),
             R(0x300, POINTER), R(0x300, DUMMY), END},
     .fpc = 0x5678, .fs = -1},
    {.group = "JMP (absolute,X)", .src = "SST-MODEL", .profile = 2, .prog = {0x7C, 0x34, 0x12},
     .x = 0x10, .mem = {M(0x1244, 0x78), M(0x1245, 0x56)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x201, DUMMY),
             R(0x1244, POINTER), R(0x1245, POINTER), END},
     .fpc = 0x5678, .fs = -1},
    {.group = "JSR", .src = "SST-MODEL", .profile = 2, .prog = {0x20, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x1FD, DUMMY), W(0x1FD, STACK_WRITE),
             W(0x1FC, STACK_WRITE), R(0x202, OPERAND), END},
     .fpc = 0x1234, .fs = 0xFB, .chk = {M(0x1FD, 0x02), M(0x1FC, 0x02)}},
    {.group = "PHA", .src = "SST-MODEL", .profile = 2, .prog = {0x48}, .a = 0x5A,
     .exp = {R(0x200, OPCODE), R(0x201, DUMMY_STREAM), W(0x1FD, STACK_WRITE), END},
     .fpc = 0x201, .fs = 0xFC, .chk = {M(0x1FD, 0x5A)}},
    {.group = "PLA", .src = "SST-MODEL", .profile = 2, .prog = {0x68}, .s = 0xFC,
     .mem = {M(0x1FD, 0x5A)},
     .exp = {R(0x200, OPCODE), R(0x201, DUMMY), R(0x1FC, DUMMY), R(0x1FD, STACK_READ), END},
     .fpc = 0x201, .fs = 0xFD},
    {.group = "RTS", .src = "SST-MODEL", .profile = 2, .prog = {0x60}, .s = 0xFB,
     .mem = {M(0x1FC, 0x33), M(0x1FD, 0x12)},
     .exp = {R(0x200, OPCODE), R(0x201, DUMMY), R(0x1FB, DUMMY), R(0x1FC, STACK_READ),
             R(0x1FD, STACK_READ), R(0x1233, DUMMY), END},
     .fpc = 0x1234, .fs = 0xFD},
    {.group = "RTI", .src = "SST-MODEL", .profile = 2, .prog = {0x40}, .s = 0xFA,
     .mem = {M(0x1FB, 0x30), M(0x1FC, 0x34), M(0x1FD, 0x12)},
     .exp = {R(0x200, OPCODE), R(0x201, DUMMY), R(0x1FA, DUMMY), R(0x1FB, STACK_READ),
             R(0x1FC, STACK_READ), R(0x1FD, STACK_READ), END},
     .fpc = 0x1234, .fs = 0xFD},
    {.group = "BRK", .src = "SST-MODEL", .profile = 2, .prog = {0x00, 0xEA},
     .mem = {M(0xFFFE, 0x00), M(0xFFFF, 0x80)},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), W(0x1FD, STACK_WRITE), W(0x1FC, STACK_WRITE),
             W(0x1FB, STACK_WRITE), R(0xFFFE, VECTOR), R(0xFFFF, VECTOR), END},
     .fpc = 0x8000, .fs = 0xFA, .chk = {M(0x1FD, 0x02), M(0x1FC, 0x02), M(0x1FB, 0x34 | 0x10)}},
    {.group = "$5C, 3-byte NOP of 4 cycles", .src = "INFERRED", .profile = 2,
     .prog = {0x5C, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY), END},
     .fpc = 0x203, .fs = -1},
    /* lynx1 bit-operation slots are NOPs of the synertek shapes */
    {.group = "lynx1 $07 even (zero page NOP)", .src = "INFERRED", .profile = 1, .prog = {0x07, 0x34},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), END}, .fpc = 0x202, .fs = -1},
    {.group = "lynx1 $17 odd (zero page,X NOP)", .src = "INFERRED", .profile = 1,
     .prog = {0x17, 0x34}, .x = 0x10,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, DUMMY), R(0x44, READ), END},
     .fpc = 0x202, .fs = -1},
    {.group = "lynx1 $0F even (3-byte NOP, 3 cycles)", .src = "INFERRED", .profile = 1,
     .prog = {0x0F, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), END}, .fpc = 0x203, .fs = -1},
    {.group = "lynx1 $1F odd (3-byte NOP, 4 cycles)", .src = "INFERRED", .profile = 1,
     .prog = {0x1F, 0x34, 0x12},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x202, OPERAND), R(0x202, DUMMY), END},
     .fpc = 0x203, .fs = -1},
    /* decimal extra cycle on the data address, and at the immediate byte */
    {.group = "decimal ADC immediate, extra cycle", .src = "INFERRED", .profile = 2,
     .prog = {0x69, 0x01}, .a = 0x99, .p = 0x38,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x201, DUMMY), END}, .fpc = 0x202, .fs = -1},
    {.group = "decimal SBC zero page, extra cycle", .src = "INFERRED", .profile = 2,
     .prog = {0xE5, 0x34}, .p = 0x39,
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), R(0x34, DUMMY), END},
     .fpc = 0x202, .fs = -1},
    {.group = "binary ADC zero page, no extra cycle", .src = "WDC", .profile = 2,
     .prog = {0x65, 0x34},
     .exp = {R(0x200, OPCODE), R(0x201, OPERAND), R(0x34, READ), END}, .fpc = 0x202, .fs = -1},
};

/* Runs one instruction from an explicit state and returns the CPU. */
static lynx__cpu run_one(uint8_t profile, const uint8_t *prog, unsigned n, uint8_t a, uint8_t p)
{
    lynx__cpu c;
    const lynx__cpu_decode *t = lynx__cpu_table(profile);
    flat_bus_clear(&g_bus);
    flat_bus_load(&g_bus, 0x0200u, prog, n);
    lynx__cpu_init(&c, profile);
    c.a = a;
    c.p = p;
    lynx__cpu_start_at(&c, t, 0x0200u);
    CHECK(flat_bus_step_instruction(&g_bus, &c, t));
    return c;
}

static void check_decimal(void)
{
    static const uint8_t adc01[2] = {0x69u, 0x01u};
    static const uint8_t sbc01[2] = {0xE9u, 0x01u};
    static const uint8_t adc7f[2] = {0x69u, 0x01u};
    lynx__cpu c;

    /* ADC #$01, A=$99, C=0, D=1: A=$00, C=1, 3 cycles. */
    c = run_one(2u, adc01, 2u, 0x99u, 0x38u);
    CHECK_EQ_HEX(c.a, 0x00u);
    CHECK((c.p & 0x01u) != 0u);
    CHECK_EQ_U64(g_bus.nlog, 3u);
    /* SBC #$01, A=$00, C=1, D=1: A=$99, C=0. */
    c = run_one(2u, sbc01, 2u, 0x00u, 0x39u);
    CHECK_EQ_HEX(c.a, 0x99u);
    CHECK((c.p & 0x01u) == 0u);
    CHECK_EQ_U64(g_bus.nlog, 3u);
    /* Binary ADC $7F + $01 sets V and N, clears C and Z, 2 cycles. */
    c = run_one(2u, adc7f, 2u, 0x7Fu, 0x30u);
    CHECK_EQ_HEX(c.a, 0x80u);
    CHECK((c.p & 0x40u) != 0u);
    CHECK((c.p & 0x80u) != 0u);
    CHECK((c.p & 0x03u) == 0u);
    CHECK_EQ_U64(g_bus.nlog, 2u);
    printf("cycles: decimal and binary ADC/SBC checked\n");
}

static int case_cycles(void)
{
    unsigned i;
    unsigned count = (unsigned)(sizeof cycases / sizeof cycases[0]);
    run_tracer();
    for (i = 0; i < count; i++) {
        run_cycase(&cycases[i]);
    }
    check_decimal();
    printf("cycles: %u cases; tags SST-MODEL %u, WDC %u, RKW %u, INFERRED %u (all unverified for the Lynx)\n",
           count, g_tag_sst, g_tag_wdc, g_tag_rkw, g_tag_inf);
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
    if (strcmp(argv[1], "cycles") == 0) {
        return case_cycles();
    }
    fprintf(stderr, "lynx-test-cpu: unknown case '%s'\n", argv[1]);
    return 2;
}
