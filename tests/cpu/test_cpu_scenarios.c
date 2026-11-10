/* Authored scenario vectors for what SingleStepTests cannot cover (CPU-04): the
   interrupt entry, the one-instruction latency of CLI, SEI, PLP and RTI, BRK, NMI,
   the reset seam, the lynx-tests cpu Test 8 shape, bit operations, reserved NOP
   lengths and the page and wrap edges the pinned sample does not reach.

   Usage:
     lynx-test-cpu-scenarios <lynx1|lynx2> [--fault cb-poll]
     lynx-test-cpu-scenarios --scenario-profile <p> --run-profile <q>
   The second form runs only the bit-operation scenarios authored for p under
   profile q (the wrong-profile control). Exit 0 all passed, 1 a scenario failed,
   2 usage.

   Source honesty. Every scenario carries one source tag from a closed set and an
   unverified flag. A scenario that does not carry LT-HW is a stand-in from another
   vendor's datasheet or a software model and is always unverified for the Lynx:
     LT-HW      a lynx-tests hardware-informed result (cpu Test 1, 2, 5, 8 and the
                Lynx II BBR/BBS 5/6/7 cycle counts from the page-mode guide)
     WDC        W65C02S datasheet (tables 4-1, 6-4, 7-1 and the wrap rules)
     RKW        Rockwell R65C02 datasheet (matrix, table 4 and the interrupt text)
     CMD        CMD G65SC02 datasheet (table V)
     SST-MODEL  the SingleStepTests software model: its bus-cycle shapes and its
                dummy-cycle addresses are generator choices, not Lynx facts
     INFERRED   chosen here from the poll rule (SEI, PLP and RTI latency, NMI
                priority, reset stack accesses); no source measures them
   The tag names what the expected values of that scenario claim. A scenario whose
   per-cycle list mixes claims is split: the per-cycle list is SST-MODEL, and the
   architectural effect alone (final state, no cycle list) carries the stronger tag.
   Every expected value is derived from the cited rule and written out below; none
   is produced by running this CPU. The runner refuses a missing or unknown tag.

   NOT modelled, so not asserted: the NMOS taken-branch IRQ delay and the NMI
   hijack of an interrupt entry. */

#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "cpu/flat_bus.h"

static flat_bus g_bus;

/* ---- tags ----------------------------------------------------------------- */

enum { TAG_LTHW, TAG_WDC, TAG_RKW, TAG_CMD, TAG_SST, TAG_INF, TAG_COUNT };

static const char *const k_tag_names[TAG_COUNT] = {"LT-HW", "WDC", "RKW", "CMD", "SST-MODEL", "INFERRED"};

static int tag_index(const char *tag)
{
    int i;
    if (tag == NULL) {
        return -1;
    }
    for (i = 0; i < TAG_COUNT; i++) {
        if (strcmp(tag, k_tag_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static unsigned g_tag_count[TAG_COUNT];
static unsigned g_unverified;

/* ---- scenario data --------------------------------------------------------- */

#define SC_MAXCYC 20

/* One expected bus cycle: address, data, write flag, cycle kind. */
typedef struct xc {
    uint16_t addr;
    uint8_t data, w, kind;
} xc;
/* A memory value to set before the run, or to check after it. */
typedef struct mv {
    uint16_t addr;
    uint8_t val, used;
} mv;
/* From log cycle index from on, the interrupt lines have this level. */
typedef struct ls {
    uint8_t from, level, used;
} ls;
/* Expected final state; only the registers named in mask are compared. p is
   compared under pmask (0 means 0xCF: bits 4 and 5 never matter). */
typedef struct fin {
    uint8_t mask;
    uint16_t pc;
    uint8_t a, x, y, s, p, pmask;
} fin;

#define RD(a, d, k) {(a), (d), 0u, LYNX__CK_##k}
#define WR(a, d, k) {(a), (d), 1u, LYNX__CK_##k}
#define END {0u, 0u, 0u, 0xFFu}
#define M(a, v) {(a), (v), 1u}
#define L(f, l) {(f), (l), 1u}

enum { F_PC = 1, F_A = 2, F_X = 4, F_Y = 8, F_S = 16, F_P = 32 };
enum { SC_FULL = 0, SC_COUNT, SC_FINAL };
enum { P_LYNX1 = 1, P_LYNX2 = 2, P_BOTH = 3 };

/* mode SC_FULL     run ncyc cycles and compare the whole per-cycle list
   mode SC_COUNT    run one instruction; its cycle count must equal ncyc
   mode SC_FINAL    run one instruction and compare the final state only
   profiles         which profiles the scenario applies to
   bitop            a Lynx II bit-operation scenario (the wrong-profile control)
   reset            begin with the reset seam instead of an opcode fetch
   s 0 and p 0 mean the power-on values 0xFD and 0x34. */
typedef struct scen {
    const char *name, *tag;
    uint8_t unv, profiles, bitop, mode, reset;
    uint16_t pc;
    uint8_t prog[12];
    uint8_t a, x, y, s, p;
    mv mem[8];
    ls lines[4];
    unsigned ncyc;
    xc exp[SC_MAXCYC];
    fin f;
    mv chk[4];
} scen;

/* The standard seven-cycle interrupt entry at S = 0xFD: two dummy stream reads of
   the return address (data d), the pushes of PCH, PCL and P, and the vector. */
#define ENT_IRQ(ret, d, pch, pcl, pp)                                                              \
    RD(ret, d, DUMMY_STREAM), RD(ret, d, DUMMY_STREAM), WR(0x1FD, pch, STACK_WRITE),               \
        WR(0x1FC, pcl, STACK_WRITE), WR(0x1FB, pp, STACK_WRITE), RD(0xFFFE, 0x00, VECTOR),         \
        RD(0xFFFF, 0x80, VECTOR)
#define ENT_NMI(ret, d, pch, pcl, pp)                                                              \
    RD(ret, d, DUMMY_STREAM), RD(ret, d, DUMMY_STREAM), WR(0x1FD, pch, STACK_WRITE),               \
        WR(0x1FC, pcl, STACK_WRITE), WR(0x1FB, pp, STACK_WRITE), RD(0xFFFA, 0x00, VECTOR),         \
        RD(0xFFFB, 0x90, VECTOR)

/* Final state after an entry from S = 0xFD: pc at the vector target, S = 0xFA,
   I set and D clear. */
#define FIN_VEC(pc_, s_) {F_PC | F_S | F_P, (pc_), 0, 0, 0, (s_), 0x34, 0}

#define IRQ LYNX__LINE_IRQ
#define NMI LYNX__LINE_NMI

static const scen k_scen[] = {
    /* ---- latency and entry (INC zp after CLI is the shape of lynx-tests Test 1) ---- */
    {.name = "irq.cli.latency", .tag = "LT-HW", .unv = 0, .profiles = P_BOTH,
     .prog = {0x58, 0xE6, 0x10}, .mem = {M(0x10, 0x41)}, .lines = {L(0, IRQ)}, .ncyc = 14,
     .exp = {RD(0x200, 0x58, OPCODE), RD(0x201, 0xE6, DUMMY_STREAM),
             RD(0x201, 0xE6, OPCODE), RD(0x202, 0x10, OPERAND), RD(0x10, 0x41, READ),
             RD(0x10, 0x41, RMW_READ), WR(0x10, 0x42, WRITE),
             ENT_IRQ(0x203, 0x00, 0x02, 0x03, 0x20), END},
     .f = FIN_VEC(0x8000, 0xFA), .chk = {M(0x10, 0x42)}},
    {.name = "irq.sei.pending_taken", .tag = "INFERRED", .unv = 1, .profiles = P_BOTH,
     .prog = {0x78, 0xEA}, .p = 0x30, .lines = {L(0, IRQ)}, .ncyc = 9,
     .exp = {RD(0x200, 0x78, OPCODE), RD(0x201, 0xEA, DUMMY_STREAM),
             ENT_IRQ(0x201, 0xEA, 0x02, 0x01, 0x24), END},
     .f = FIN_VEC(0x8000, 0xFA)},
    {.name = "irq.plp.delay", .tag = "INFERRED", .unv = 1, .profiles = P_BOTH,
     .prog = {0x28, 0xEA, 0xEA}, .s = 0xFC, .p = 0x34, .mem = {M(0x1FD, 0x20)},
     .lines = {L(0, IRQ)}, .ncyc = 13,
     .exp = {RD(0x200, 0x28, OPCODE), RD(0x201, 0xEA, DUMMY), RD(0x1FC, 0x00, DUMMY),
             RD(0x1FD, 0x20, STACK_READ),
             RD(0x201, 0xEA, OPCODE), RD(0x202, 0xEA, DUMMY_STREAM),
             ENT_IRQ(0x202, 0xEA, 0x02, 0x02, 0x20), END},
     .f = FIN_VEC(0x8000, 0xFA)},
    {.name = "irq.rti.immediate", .tag = "INFERRED", .unv = 1, .profiles = P_BOTH,
     .prog = {0x40}, .s = 0xFA, .p = 0x34,
     .mem = {M(0x1FB, 0x20), M(0x1FC, 0x34), M(0x1FD, 0x12), M(0x1234, 0xEA)},
     .lines = {L(0, IRQ)}, .ncyc = 13,
     .exp = {RD(0x200, 0x40, OPCODE), RD(0x201, 0x00, DUMMY), RD(0x1FA, 0x00, DUMMY),
             RD(0x1FB, 0x20, STACK_READ), RD(0x1FC, 0x34, STACK_READ), RD(0x1FD, 0x12, STACK_READ),
             ENT_IRQ(0x1234, 0xEA, 0x12, 0x34, 0x20), END},
     .f = FIN_VEC(0x8000, 0xFA)},
    /* ---- D in an interrupt (lynx-tests Test 2 shape) ---- */
    {.name = "irq.entry.d_cleared", .tag = "LT-HW", .unv = 0, .profiles = P_BOTH,
     .prog = {0xEA}, .p = 0x38, .lines = {L(0, IRQ)}, .ncyc = 9,
     .exp = {RD(0x200, 0xEA, OPCODE), RD(0x201, 0x00, DUMMY_STREAM),
             ENT_IRQ(0x201, 0x00, 0x02, 0x01, 0x28), END},
     .f = FIN_VEC(0x8000, 0xFA)},
    {.name = "irq.rti.restores_d", .tag = "LT-HW", .unv = 0, .profiles = P_BOTH,
     .prog = {0xEA}, .p = 0x38, .mem = {M(0x8000, 0x40)}, .lines = {L(0, IRQ), L(2, 0)}, .ncyc = 15,
     .exp = {RD(0x200, 0xEA, OPCODE), RD(0x201, 0x00, DUMMY_STREAM),
             ENT_IRQ(0x201, 0x00, 0x02, 0x01, 0x28),
             RD(0x8000, 0x40, OPCODE), RD(0x8001, 0x00, DUMMY), RD(0x1FA, 0x00, DUMMY),
             RD(0x1FB, 0x28, STACK_READ), RD(0x1FC, 0x01, STACK_READ), RD(0x1FD, 0x02, STACK_READ),
             END},
     .f = {F_PC | F_S | F_P, 0x0201, 0, 0, 0, 0xFD, 0x38, 0}},
    /* ---- BRK ---- */
    {.name = "brk.frame", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .prog = {0x00, 0xEA}, .p = 0x38, .ncyc = 7,
     .exp = {RD(0x200, 0x00, OPCODE), RD(0x201, 0xEA, OPERAND),
             WR(0x1FD, 0x02, STACK_WRITE), WR(0x1FC, 0x02, STACK_WRITE), WR(0x1FB, 0x38, STACK_WRITE),
             RD(0xFFFE, 0x00, VECTOR), RD(0xFFFF, 0x80, VECTOR), END},
     .f = FIN_VEC(0x8000, 0xFA)},
    {.name = "brk.irq_waits_for_cli", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .prog = {0x00, 0xEA}, .p = 0x30,
     .mem = {M(0x8000, 0x58), M(0x8001, 0xEA), M(0x8002, 0xEA)}, .lines = {L(0, IRQ)}, .ncyc = 18,
     .exp = {RD(0x200, 0x00, OPCODE), RD(0x201, 0xEA, OPERAND),
             WR(0x1FD, 0x02, STACK_WRITE), WR(0x1FC, 0x02, STACK_WRITE), WR(0x1FB, 0x30, STACK_WRITE),
             RD(0xFFFE, 0x00, VECTOR), RD(0xFFFF, 0x80, VECTOR),
             RD(0x8000, 0x58, OPCODE), RD(0x8001, 0xEA, DUMMY_STREAM),
             RD(0x8001, 0xEA, OPCODE), RD(0x8002, 0xEA, DUMMY_STREAM),
             RD(0x8002, 0xEA, DUMMY_STREAM), RD(0x8002, 0xEA, DUMMY_STREAM),
             WR(0x1FA, 0x80, STACK_WRITE), WR(0x1F9, 0x02, STACK_WRITE), WR(0x1F8, 0x20, STACK_WRITE),
             RD(0xFFFE, 0x00, VECTOR), RD(0xFFFF, 0x80, VECTOR), END},
     .f = FIN_VEC(0x8000, 0xF7)},
    /* ---- NMI ---- */
    {.name = "nmi.edge_taken_once", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .prog = {0xEA, 0xEA}, .mem = {M(0x9000, 0xEA), M(0x9001, 0xEA)}, .lines = {L(1, NMI)}, .ncyc = 13,
     .exp = {RD(0x200, 0xEA, OPCODE), RD(0x201, 0xEA, DUMMY_STREAM),
             ENT_NMI(0x201, 0xEA, 0x02, 0x01, 0x24),
             RD(0x9000, 0xEA, OPCODE), RD(0x9001, 0xEA, DUMMY_STREAM),
             RD(0x9001, 0xEA, OPCODE), RD(0x9002, 0x00, DUMMY_STREAM), END},
     .f = FIN_VEC(0x9002, 0xFA)},
    {.name = "nmi.second_edge_taken", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .prog = {0xEA, 0xEA}, .mem = {M(0x9000, 0xEA), M(0x9001, 0xEA)},
     .lines = {L(1, NMI), L(9, 0), L(10, NMI)}, .ncyc = 18,
     .exp = {RD(0x200, 0xEA, OPCODE), RD(0x201, 0xEA, DUMMY_STREAM),
             ENT_NMI(0x201, 0xEA, 0x02, 0x01, 0x24),
             RD(0x9000, 0xEA, OPCODE), RD(0x9001, 0xEA, DUMMY_STREAM),
             RD(0x9001, 0xEA, DUMMY_STREAM), RD(0x9001, 0xEA, DUMMY_STREAM),
             WR(0x1FA, 0x90, STACK_WRITE), WR(0x1F9, 0x01, STACK_WRITE), WR(0x1F8, 0x24, STACK_WRITE),
             RD(0xFFFA, 0x00, VECTOR), RD(0xFFFB, 0x90, VECTOR), END},
     .f = FIN_VEC(0x9000, 0xF7)},
    {.name = "nmi.priority_over_irq", .tag = "INFERRED", .unv = 1, .profiles = P_BOTH,
     .prog = {0xEA}, .p = 0x30, .lines = {L(0, IRQ | NMI)}, .ncyc = 9,
     .exp = {RD(0x200, 0xEA, OPCODE), RD(0x201, 0x00, DUMMY_STREAM),
             ENT_NMI(0x201, 0x00, 0x02, 0x01, 0x20), END},
     .f = FIN_VEC(0x9000, 0xFA)},
    /* ---- reset seam ---- */
    {.name = "reset.seam", .tag = "INFERRED", .unv = 1, .profiles = P_BOTH, .reset = 1,
     .a = 1, .x = 2, .y = 3, .p = 0x38, .ncyc = 7,
     .exp = {RD(0x200, 0x00, DUMMY_STREAM), RD(0x200, 0x00, DUMMY_STREAM),
             RD(0x1FD, 0x00, STACK_READ), RD(0x1FC, 0x00, STACK_READ), RD(0x1FB, 0x00, STACK_READ),
             RD(0xFFFC, 0x00, VECTOR), RD(0xFFFD, 0xC0, VECTOR), END},
     .f = {F_PC | F_A | F_X | F_Y | F_S | F_P, 0xC000, 1, 2, 3, 0xFA, 0x34, 0}},
    /* ---- lynx-tests Test 8 replica: 1-cycle NOPs ($x3, $xB, $CB, $DB) do not poll ---- */
    {.name = "irq.test8.replica", .tag = "LT-HW", .unv = 0, .profiles = P_BOTH,
     .prog = {0x58, 0x03, 0x0B, 0xCB, 0xDB, 0x13, 0x1B, 0xE8, 0xEA}, .lines = {L(0, IRQ)}, .ncyc = 17,
     .exp = {RD(0x200, 0x58, OPCODE), RD(0x201, 0x03, DUMMY_STREAM),
             RD(0x201, 0x03, OPCODE), RD(0x202, 0x0B, OPCODE), RD(0x203, 0xCB, OPCODE),
             RD(0x204, 0xDB, OPCODE), RD(0x205, 0x13, OPCODE), RD(0x206, 0x1B, OPCODE),
             RD(0x207, 0xE8, OPCODE), RD(0x208, 0xEA, DUMMY_STREAM),
             ENT_IRQ(0x208, 0xEA, 0x02, 0x08, 0x20), END},
     .f = {F_PC | F_X | F_S | F_P, 0x8000, 0, 1, 0, 0xFA, 0x34, 0}},
    {.name = "irq.test8.control_ea", .tag = "LT-HW", .unv = 0, .profiles = P_BOTH,
     .prog = {0x58, 0xEA, 0xEA, 0xEA, 0xE8}, .lines = {L(0, IRQ)}, .ncyc = 11,
     .exp = {RD(0x200, 0x58, OPCODE), RD(0x201, 0xEA, DUMMY_STREAM),
             RD(0x201, 0xEA, OPCODE), RD(0x202, 0xEA, DUMMY_STREAM),
             ENT_IRQ(0x202, 0xEA, 0x02, 0x02, 0x20), END},
     .f = FIN_VEC(0x8000, 0xFA)},
    /* ---- no WAI and no STP: $CB and $DB are 1-byte, 1-cycle NOPs (CMD table V, 1 byte
       and 1 cycle; the no-poll rule is the Test 8 replica above) ---- */
    {.name = "nop.cb.one_byte_one_cycle", .tag = "CMD", .unv = 1, .profiles = P_BOTH,
     .prog = {0xCB, 0xE8}, .ncyc = 1, .exp = {RD(0x200, 0xCB, OPCODE), END},
     .f = {F_PC | F_X | F_S, 0x0201, 0, 0, 0, 0xFD, 0, 0}},
    {.name = "nop.db.one_byte_one_cycle", .tag = "CMD", .unv = 1, .profiles = P_BOTH,
     .prog = {0xDB, 0xE8}, .ncyc = 1, .exp = {RD(0x200, 0xDB, OPCODE), END},
     .f = {F_PC | F_X | F_S, 0x0201, 0, 0, 0, 0xFD, 0, 0}},
    /* ---- reserved NOP lengths follow the profile table (WDC table 7-1 sizes) ---- */
    {.name = "nop.02.two_bytes", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .prog = {0x02, 0xE8}, .ncyc = 2,
     .exp = {RD(0x200, 0x02, OPCODE), RD(0x201, 0xE8, OPERAND), END},
     .f = {F_PC | F_X, 0x0202, 0, 0, 0, 0, 0, 0}},
    {.name = "nop.44.two_bytes_zp_read", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .prog = {0x44, 0xE8}, .ncyc = 3,
     .exp = {RD(0x200, 0x44, OPCODE), RD(0x201, 0xE8, OPERAND), RD(0x00E8, 0x00, READ), END},
     .f = {F_PC | F_X, 0x0202, 0, 0, 0, 0, 0, 0}},
    {.name = "nop.54.two_bytes_zpx_read", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0x54, 0xE8}, .x = 0x10, .ncyc = 4,
     .exp = {RD(0x200, 0x54, OPCODE), RD(0x201, 0xE8, OPERAND), RD(0x00E8, 0x00, DUMMY),
             RD(0x00F8, 0x00, READ), END},
     .f = {F_PC | F_X, 0x0202, 0, 0x10, 0, 0, 0, 0}},
    {.name = "nop.5c.three_bytes", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0x5C, 0xE8, 0xE8}, .ncyc = 4,
     .exp = {RD(0x200, 0x5C, OPCODE), RD(0x201, 0xE8, OPERAND), RD(0x202, 0xE8, OPERAND),
             RD(0x202, 0xE8, DUMMY), END},
     .f = {F_PC | F_X, 0x0203, 0, 0, 0, 0, 0, 0}},
    {.name = "nop.dc.three_bytes", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0xDC, 0xE8, 0xE8}, .ncyc = 4,
     .exp = {RD(0x200, 0xDC, OPCODE), RD(0x201, 0xE8, OPERAND), RD(0x202, 0xE8, OPERAND),
             RD(0x202, 0xE8, DUMMY), END},
     .f = {F_PC | F_X, 0x0203, 0, 0, 0, 0, 0, 0}},
};

#define K_SCEN_COUNT ((unsigned)(sizeof k_scen / sizeof k_scen[0]))

/* ---- the runner ------------------------------------------------------------- */

static unsigned scen_expected_cycles(const scen *s)
{
    unsigned n = 0;
    while (n < SC_MAXCYC && s->exp[n].kind != 0xFFu) {
        n++;
    }
    return n;
}

/* A scenario with a missing or unknown tag, a stand-in claim not marked
   unverified, or a per-cycle list whose length is not ncyc is a data error. */
static int validate_scen(const scen *s)
{
    int bad = 0;
    int ti = tag_index(s->tag);
    if (s->name == NULL || s->name[0] == '\0') {
        fprintf(stderr, "scenario (unnamed): data error: no name\n");
        return 1;
    }
    if (ti < 0) {
        fprintf(stderr, "scenario %s: missing or unknown source tag '%s'\n", s->name,
                s->tag == NULL ? "(none)" : s->tag);
        bad = 1;
    } else if (ti != TAG_LTHW && s->unv == 0u) {
        fprintf(stderr, "scenario %s: tag %s is a stand-in and must be marked unverified\n", s->name,
                s->tag);
        bad = 1;
    }
    if (s->mode == SC_FULL && scen_expected_cycles(s) != s->ncyc) {
        fprintf(stderr, "scenario %s: data error: %u expected cycles, ncyc %u\n", s->name,
                scen_expected_cycles(s), s->ncyc);
        bad = 1;
    }
    return bad;
}

static void build_sched(const ls *l)
{
    unsigned i;
    unsigned k;
    for (i = 0; i < FLAT_LOG_MAX; i++) {
        uint8_t lvl = 0u;
        for (k = 0; k < 4u; k++) {
            if (l[k].used != 0u && l[k].from <= i) {
                lvl = l[k].level;
            }
        }
        g_bus.sched[i] = lvl;
    }
}

/* Runs one scenario under the table t; returns the number of mismatches. */
static int run_scen(const scen *s, uint8_t run_profile, const lynx__cpu_decode *t)
{
    lynx__cpu c;
    uint16_t pc = s->pc != 0u ? s->pc : 0x0200u;
    uint8_t pm = s->f.pmask != 0u ? s->f.pmask : 0xCFu;
    unsigned i;
    int fails = 0;

    flat_bus_clear(&g_bus);
    g_bus.mem[0xFFFEu] = 0x00u;
    g_bus.mem[0xFFFFu] = 0x80u;
    g_bus.mem[0xFFFAu] = 0x00u;
    g_bus.mem[0xFFFBu] = 0x90u;
    g_bus.mem[0xFFFCu] = 0x00u;
    g_bus.mem[0xFFFDu] = 0xC0u;
    for (i = 0; i < 12u; i++) {
        g_bus.mem[(uint16_t)(pc + i)] = s->prog[i];
    }
    for (i = 0; i < 8u; i++) {
        if (s->mem[i].used != 0u) {
            g_bus.mem[s->mem[i].addr] = s->mem[i].val;
        }
    }
    build_sched(s->lines);
    lynx__cpu_init(&c, run_profile);
    c.a = s->a;
    c.x = s->x;
    c.y = s->y;
    c.s = s->s != 0u ? s->s : 0xFDu;
    c.p = s->p != 0u ? s->p : 0x34u;
    lynx__cpu_start_at(&c, t, pc);
    if (s->reset != 0u) {
        lynx__cpu_start_reset(&c, t);
    }

    if (s->mode == SC_FULL) {
        flat_bus_run_cycles(&g_bus, &c, t, s->ncyc);
        for (i = 0; i < s->ncyc && i < g_bus.nlog; i++) {
            const flat_entry *e = &g_bus.log[i];
            const xc *x = &s->exp[i];
            if (e->addr != x->addr || e->data != x->data || e->write != x->w || e->kind != x->kind) {
                fprintf(stderr,
                        "scenario %s: mismatch cycle %u is %04X %02X %s kind %u, expected %04X %02X %s kind %u\n",
                        s->name, i + 1u, e->addr, e->data, e->write != 0u ? "write" : "read", e->kind,
                        x->addr, x->data, x->w != 0u ? "write" : "read", x->kind);
                fails++;
            }
        }
    } else {
        if (!flat_bus_step_instruction(&g_bus, &c, t)) {
            fprintf(stderr, "scenario %s: mismatch cycle bound exceeded\n", s->name);
            return 1;
        }
        if (s->mode == SC_COUNT && g_bus.nlog != s->ncyc) {
            fprintf(stderr, "scenario %s: mismatch %u cycles, expected %u\n", s->name, g_bus.nlog,
                    s->ncyc);
            fails++;
        }
    }
    if (!lynx__cpu_at_boundary(&c)) {
        fprintf(stderr, "scenario %s: mismatch not at an instruction boundary after the run\n", s->name);
        fails++;
    }
    if (!lynx__cpu_state_valid(&c, t)) {
        fprintf(stderr, "scenario %s: mismatch final state refused by lynx__cpu_state_valid\n", s->name);
        fails++;
    }
    if ((s->f.mask & F_PC) != 0u && c.pc != s->f.pc) {
        fprintf(stderr, "scenario %s: mismatch pc %04X, expected %04X\n", s->name, c.pc, s->f.pc);
        fails++;
    }
    if ((s->f.mask & F_A) != 0u && c.a != s->f.a) {
        fprintf(stderr, "scenario %s: mismatch a %02X, expected %02X\n", s->name, c.a, s->f.a);
        fails++;
    }
    if ((s->f.mask & F_X) != 0u && c.x != s->f.x) {
        fprintf(stderr, "scenario %s: mismatch x %02X, expected %02X\n", s->name, c.x, s->f.x);
        fails++;
    }
    if ((s->f.mask & F_Y) != 0u && c.y != s->f.y) {
        fprintf(stderr, "scenario %s: mismatch y %02X, expected %02X\n", s->name, c.y, s->f.y);
        fails++;
    }
    if ((s->f.mask & F_S) != 0u && c.s != s->f.s) {
        fprintf(stderr, "scenario %s: mismatch s %02X, expected %02X\n", s->name, c.s, s->f.s);
        fails++;
    }
    if ((s->f.mask & F_P) != 0u && (c.p & pm) != (s->f.p & pm)) {
        fprintf(stderr, "scenario %s: mismatch p %02X, expected %02X under mask %02X\n", s->name,
                (unsigned)(c.p & pm), (unsigned)(s->f.p & pm), pm);
        fails++;
    }
    for (i = 0; i < 4u; i++) {
        if (s->chk[i].used != 0u && g_bus.mem[s->chk[i].addr] != s->chk[i].val) {
            fprintf(stderr, "scenario %s: mismatch memory %04X is %02X, expected %02X\n", s->name,
                    s->chk[i].addr, g_bus.mem[s->chk[i].addr], s->chk[i].val);
            fails++;
        }
    }
    return fails;
}

static void account(const scen *s, int fails)
{
    int ti = tag_index(s->tag);
    if (ti >= 0) {
        g_tag_count[ti]++;
    }
    if (s->unv != 0u) {
        g_unverified++;
    }
    if (fails == 0) {
        printf("ok   %-40s %-9s%s\n", s->name, s->tag, s->unv != 0u ? " unverified" : "");
    }
}

/* The whole Test 8 block: after CLI with an IRQ pending, all 32 one-cycle NOPs
   ($03, $13 ... $F3 then $0B, $1B ... $FB, so $CB and $DB are inside) take no IRQ;
   the IRQ is taken after the INX that follows, and the pushed return address is the
   byte after the INX. Expected values come from the rule: 1 fetch per NOP, no poll.
   Tag LT-HW, verified (lynx-tests cpu Test 8). */
static const char k_block32_name[] = "irq.test8.block32";

static int run_block32(uint8_t run_profile, const lynx__cpu_decode *t)
{
    lynx__cpu c;
    unsigned i;
    int fails = 0;
    uint8_t ops[32];
    const flat_entry *e;

    for (i = 0; i < 16u; i++) {
        ops[i] = (uint8_t)(0x03u + 0x10u * i);
        ops[16u + i] = (uint8_t)(0x0Bu + 0x10u * i);
    }
    flat_bus_clear(&g_bus);
    g_bus.mem[0xFFFEu] = 0x00u;
    g_bus.mem[0xFFFFu] = 0x80u;
    g_bus.mem[0x0200u] = 0x58u;
    for (i = 0; i < 32u; i++) {
        g_bus.mem[0x0201u + i] = ops[i];
    }
    g_bus.mem[0x0221u] = 0xE8u;
    g_bus.mem[0x0222u] = 0xEAu;
    flat_bus_set_lines_all(&g_bus, LYNX__LINE_IRQ);
    lynx__cpu_init(&c, run_profile);
    lynx__cpu_start_at(&c, t, 0x0200u);
    flat_bus_run_cycles(&g_bus, &c, t, 2u + 32u + 2u + 7u);

    e = g_bus.log;
    if (e[0].addr != 0x0200u || e[0].data != 0x58u || e[1].addr != 0x0201u || e[1].kind != LYNX__CK_DUMMY_STREAM) {
        fprintf(stderr, "scenario %s: mismatch CLI cycles\n", k_block32_name);
        fails++;
    }
    for (i = 0; i < 32u; i++) {
        const flat_entry *n = &e[2u + i];
        if (n->addr != (uint16_t)(0x0201u + i) || n->data != ops[i] || n->kind != LYNX__CK_OPCODE ||
            n->write != 0u || (n->flags & LYNX__CF_POLL) != 0u) {
            fprintf(stderr, "scenario %s: mismatch NOP %02X at cycle %u is %04X kind %u, expected one fetch at %04X\n",
                    k_block32_name, ops[i], 3u + i, n->addr, n->kind, (unsigned)(0x0201u + i));
            fails++;
            break;
        }
    }
    if (fails == 0) {
        const flat_entry *x = &e[34];
        if (x[0].addr != 0x0221u || x[0].kind != LYNX__CK_OPCODE || x[1].addr != 0x0222u ||
            x[1].kind != LYNX__CK_DUMMY_STREAM || (x[1].flags & LYNX__CF_POLL) == 0u) {
            fprintf(stderr, "scenario %s: mismatch INX cycles\n", k_block32_name);
            fails++;
        }
        if (x[2].addr != 0x0222u || x[2].kind != LYNX__CK_DUMMY_STREAM || x[3].addr != 0x0222u ||
            x[4].addr != 0x01FDu || x[4].data != 0x02u || x[5].addr != 0x01FCu || x[5].data != 0x22u ||
            x[6].addr != 0x01FBu || x[6].data != 0x20u || x[7].addr != 0xFFFEu ||
            x[7].kind != LYNX__CK_VECTOR || x[8].addr != 0xFFFFu) {
            fprintf(stderr, "scenario %s: mismatch interrupt entry (return address 0222 expected)\n",
                    k_block32_name);
            fails++;
        }
    }
    if (c.pc != 0x8000u || c.x != 1u || c.s != 0xFAu || !lynx__cpu_at_boundary(&c)) {
        fprintf(stderr, "scenario %s: mismatch final pc %04X x %02X s %02X\n", k_block32_name, c.pc, c.x,
                c.s);
        fails++;
    }
    g_tag_count[TAG_LTHW]++;
    if (fails == 0) {
        printf("ok   %-40s %-9s\n", k_block32_name, "LT-HW");
    }
    return fails;
}

/* The control: row $CB replaced by the $EA row, so $CB becomes a 2-cycle
   instruction that polls on its last cycle. Test-local copy, no core change. */
static lynx__cpu_decode g_fault_table[256];

static int parse_profile(const char *name)
{
    if (strcmp(name, "lynx1") == 0) {
        return 1;
    }
    if (strcmp(name, "lynx2") == 0) {
        return 2;
    }
    return 0;
}

static int usage(void)
{
    fprintf(stderr,
            "usage: lynx-test-cpu-scenarios <lynx1|lynx2> [--fault cb-poll]\n"
            "       lynx-test-cpu-scenarios --scenario-profile <p> --run-profile <q>\n");
    return 2;
}

int main(int argc, char **argv)
{
    int sprof = 0;
    int rprof = 0;
    int positional = 0;
    int fault = 0;
    int i;
    unsigned k;
    unsigned ran = 0;
    int total_fails = 0;
    int bad = 0;
    const lynx__cpu_decode *t;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fault") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "cb-poll") != 0) {
                return usage();
            }
            fault = 1;
        } else if (strcmp(argv[i], "--scenario-profile") == 0 && i + 1 < argc) {
            i++;
            sprof = parse_profile(argv[i]);
            if (sprof == 0) {
                return usage();
            }
        } else if (strcmp(argv[i], "--run-profile") == 0 && i + 1 < argc) {
            i++;
            rprof = parse_profile(argv[i]);
            if (rprof == 0) {
                return usage();
            }
        } else if (argv[i][0] != '-' && positional == 0) {
            positional = parse_profile(argv[i]);
            if (positional == 0) {
                return usage();
            }
        } else {
            return usage();
        }
    }
    if (positional != 0) {
        if (sprof != 0 || rprof != 0) {
            return usage();
        }
        sprof = positional;
        rprof = positional;
    } else if (sprof == 0 || rprof == 0 || fault != 0) {
        return usage();
    }

    for (k = 0; k < K_SCEN_COUNT; k++) {
        bad += validate_scen(&k_scen[k]);
    }
    if (bad != 0) {
        fprintf(stderr, "scenarios: %d data errors, nothing run\n", bad);
        return 1;
    }

    t = lynx__cpu_table((uint32_t)rprof);
    if (fault != 0) {
        memcpy(g_fault_table, t, sizeof g_fault_table);
        g_fault_table[0xCBu] = g_fault_table[0xEAu];
        t = g_fault_table;
    }

    for (k = 0; k < K_SCEN_COUNT; k++) {
        const scen *s = &k_scen[k];
        int fails;
        if ((s->profiles & (unsigned)(1 << (sprof - 1))) == 0u) {
            continue;
        }
        if (sprof != rprof && s->bitop == 0u) {
            continue;
        }
        fails = run_scen(s, (uint8_t)rprof, t);
        account(s, fails);
        ran++;
        total_fails += fails;
    }
    if (sprof == rprof) {
        total_fails += run_block32((uint8_t)rprof, t);
        ran++;
    }

    printf("profile %s (scenarios authored for %s): %u scenarios run, %d mismatches\n",
           rprof == 1 ? "lynx1" : "lynx2", sprof == 1 ? "lynx1" : "lynx2", ran, total_fails);
    printf("tags: LT-HW %u WDC %u RKW %u CMD %u SST-MODEL %u INFERRED %u unverified %u\n",
           g_tag_count[TAG_LTHW], g_tag_count[TAG_WDC], g_tag_count[TAG_RKW], g_tag_count[TAG_CMD],
           g_tag_count[TAG_SST], g_tag_count[TAG_INF], g_unverified);
    return total_fails != 0 ? 1 : 0;
}
