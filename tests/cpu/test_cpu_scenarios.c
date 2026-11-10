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
   sz               start with S = 0x00 (s 0 otherwise means the power-on 0xFD)
   p 0 means the power-on 0x34. */
typedef struct scen {
    const char *name, *tag;
    uint8_t unv, profiles, bitop, mode, reset, sz;
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
    /* ---- the Lynx II bit operations that need a fixed shape: page-crossing BBR/BBS
       (RKW matrix 3 5**: +2 when the branch crosses a page) and the BBR/BBS 5, 6, 7
       cycle counts, which are LT-HW-informed (page-mode guide: 23/28/33 ticks), run
       as counts only because the dummy-cycle addresses are model choices ---- */
    {.name = "bitop.bbr0.taken_cross_page", .tag = "RKW", .unv = 1, .profiles = P_LYNX2, .bitop = 1,
     .pc = 0x02FC, .prog = {0x0F, 0x34, 0x05}, .mem = {M(0x34, 0x00)}, .ncyc = 7,
     .exp = {RD(0x2FC, 0x0F, OPCODE), RD(0x2FD, 0x34, OPERAND), RD(0x34, 0x00, READ),
             RD(0x34, 0x00, DUMMY), RD(0x2FE, 0x05, OPERAND), RD(0x2FF, 0x00, DUMMY_STREAM),
             RD(0x2FF, 0x00, DUMMY), END},
     .f = {F_PC, 0x0304, 0, 0, 0, 0, 0, 0}},
    {.name = "bitop.bbs0.taken_cross_page", .tag = "RKW", .unv = 1, .profiles = P_LYNX2, .bitop = 1,
     .pc = 0x02FC, .prog = {0x8F, 0x34, 0x05}, .mem = {M(0x34, 0x01)}, .ncyc = 7,
     .exp = {RD(0x2FC, 0x8F, OPCODE), RD(0x2FD, 0x34, OPERAND), RD(0x34, 0x01, READ),
             RD(0x34, 0x01, DUMMY), RD(0x2FE, 0x05, OPERAND), RD(0x2FF, 0x00, DUMMY_STREAM),
             RD(0x2FF, 0x00, DUMMY), END},
     .f = {F_PC, 0x0304, 0, 0, 0, 0, 0, 0}},
    {.name = "bitop.bbr0.cycles_not_taken_5", .tag = "LT-HW", .unv = 0, .profiles = P_LYNX2, .bitop = 1,
     .mode = SC_COUNT, .prog = {0x0F, 0x34, 0x05}, .mem = {M(0x34, 0x01)}, .ncyc = 5,
     .f = {F_PC, 0x0203, 0, 0, 0, 0, 0, 0}},
    {.name = "bitop.bbr0.cycles_taken_6", .tag = "LT-HW", .unv = 0, .profiles = P_LYNX2, .bitop = 1,
     .mode = SC_COUNT, .prog = {0x0F, 0x34, 0x05}, .mem = {M(0x34, 0x00)}, .ncyc = 6,
     .f = {F_PC, 0x0208, 0, 0, 0, 0, 0, 0}},
    {.name = "bitop.bbr0.cycles_taken_cross_7", .tag = "LT-HW", .unv = 0, .profiles = P_LYNX2, .bitop = 1,
     .mode = SC_COUNT, .pc = 0x02FC, .prog = {0x0F, 0x34, 0x05}, .mem = {M(0x34, 0x00)}, .ncyc = 7,
     .f = {F_PC, 0x0304, 0, 0, 0, 0, 0, 0}},
    /* ---- directed edges the sample cannot reach (both profiles). Each edge has a
       per-cycle row (SST-MODEL: the dummy addresses are generator choices) and a
       final-state row that carries the architectural rule (WDC), so the stronger
       tag never covers a model choice ---- */
    {.name = "edge.jmp_ind_xxff.page_fixed", .tag = "LT-HW", .unv = 0, .profiles = P_BOTH,
     .mode = SC_FINAL, .pc = 0x0400, .prog = {0x6C, 0xFF, 0x02},
     .mem = {M(0x02FF, 0x78), M(0x0300, 0x56), M(0x0200, 0x99)},
     .f = {F_PC, 0x5678, 0, 0, 0, 0, 0, 0}},
    {.name = "edge.jmp_ind_xxff.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .pc = 0x0400, .prog = {0x6C, 0xFF, 0x02},
     .mem = {M(0x02FF, 0x78), M(0x0300, 0x56), M(0x0200, 0x99)}, .ncyc = 6,
     .exp = {RD(0x400, 0x6C, OPCODE), RD(0x401, 0xFF, OPERAND), RD(0x402, 0x02, OPERAND),
             RD(0x2FF, 0x78, POINTER), RD(0x300, 0x56, POINTER), RD(0x300, 0x56, DUMMY), END},
     .f = {F_PC, 0x5678, 0, 0, 0, 0, 0, 0}},
    {.name = "edge.izx_zp_wrap.final", .tag = "WDC", .unv = 1, .profiles = P_BOTH, .mode = SC_FINAL,
     .prog = {0xA1, 0xF0}, .x = 0x0F,
     .mem = {M(0x00FF, 0x78), M(0x0000, 0x56), M(0x0100, 0x11), M(0x5678, 0xAB)},
     .f = {F_PC | F_A, 0x0202, 0xAB, 0, 0, 0, 0, 0}},
    {.name = "edge.izx_zp_wrap.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0xA1, 0xF0}, .x = 0x0F,
     .mem = {M(0x00FF, 0x78), M(0x0000, 0x56), M(0x0100, 0x11), M(0x5678, 0xAB)}, .ncyc = 6,
     .exp = {RD(0x200, 0xA1, OPCODE), RD(0x201, 0xF0, OPERAND), RD(0x00F0, 0x00, DUMMY),
             RD(0x00FF, 0x78, POINTER), RD(0x0000, 0x56, POINTER), RD(0x5678, 0xAB, READ), END},
     .f = {F_PC | F_A, 0x0202, 0xAB, 0, 0, 0, 0, 0}},
    {.name = "edge.zpx_wrap.final", .tag = "WDC", .unv = 1, .profiles = P_BOTH, .mode = SC_FINAL,
     .prog = {0xB5, 0xF0}, .x = 0x20, .mem = {M(0x0010, 0x5A), M(0x0110, 0xEE)},
     .f = {F_PC | F_A, 0x0202, 0x5A, 0, 0, 0, 0, 0}},
    {.name = "edge.zpx_wrap.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0xB5, 0xF0}, .x = 0x20, .mem = {M(0x0010, 0x5A), M(0x0110, 0xEE)}, .ncyc = 4,
     .exp = {RD(0x200, 0xB5, OPCODE), RD(0x201, 0xF0, OPERAND), RD(0x00F0, 0x00, DUMMY),
             RD(0x0010, 0x5A, READ), END},
     .f = {F_PC | F_A, 0x0202, 0x5A, 0, 0, 0, 0, 0}},
    {.name = "edge.izp_ptr_ff.final", .tag = "WDC", .unv = 1, .profiles = P_BOTH, .mode = SC_FINAL,
     .prog = {0xB2, 0xFF},
     .mem = {M(0x00FF, 0x78), M(0x0000, 0x56), M(0x0100, 0x11), M(0x5678, 0x3C)},
     .f = {F_PC | F_A, 0x0202, 0x3C, 0, 0, 0, 0, 0}},
    {.name = "edge.izp_ptr_ff.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0xB2, 0xFF},
     .mem = {M(0x00FF, 0x78), M(0x0000, 0x56), M(0x0100, 0x11), M(0x5678, 0x3C)}, .ncyc = 5,
     .exp = {RD(0x200, 0xB2, OPCODE), RD(0x201, 0xFF, OPERAND), RD(0x00FF, 0x78, POINTER),
             RD(0x0000, 0x56, POINTER), RD(0x5678, 0x3C, READ), END},
     .f = {F_PC | F_A, 0x0202, 0x3C, 0, 0, 0, 0, 0}},
    {.name = "edge.stack_push_wraps_0100_to_01ff", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_FINAL, .prog = {0x48}, .a = 0x5A, .sz = 1, .chk = {M(0x0100, 0x5A)},
     .f = {F_PC | F_S, 0x0201, 0, 0, 0, 0xFF, 0, 0}},
    {.name = "edge.stack_push.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0x48}, .a = 0x5A, .sz = 1, .ncyc = 3,
     .exp = {RD(0x200, 0x48, OPCODE), RD(0x201, 0x00, DUMMY_STREAM), WR(0x0100, 0x5A, STACK_WRITE), END},
     .f = {F_PC | F_S, 0x0201, 0, 0, 0, 0xFF, 0, 0}, .chk = {M(0x0100, 0x5A)}},
    {.name = "edge.stack_pull_wraps_01ff_to_0100", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_FINAL, .prog = {0x68}, .s = 0xFF, .mem = {M(0x0100, 0x7B)},
     .f = {F_PC | F_A | F_S, 0x0201, 0x7B, 0, 0, 0x00, 0, 0}},
    {.name = "edge.stack_pull.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0x68}, .s = 0xFF, .mem = {M(0x0100, 0x7B)}, .ncyc = 4,
     .exp = {RD(0x200, 0x68, OPCODE), RD(0x201, 0x00, DUMMY), RD(0x01FF, 0x00, DUMMY),
             RD(0x0100, 0x7B, STACK_READ), END},
     .f = {F_PC | F_A | F_S, 0x0201, 0x7B, 0, 0, 0x00, 0, 0}},
    {.name = "edge.jsr_push_wraps_at_s00", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_FINAL, .prog = {0x20, 0x34, 0x12}, .sz = 1,
     .chk = {M(0x0100, 0x02), M(0x01FF, 0x02)},
     .f = {F_PC | F_S, 0x1234, 0, 0, 0, 0xFE, 0, 0}},
    {.name = "edge.jsr_wrap.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0x20, 0x34, 0x12}, .sz = 1, .ncyc = 6,
     .exp = {RD(0x200, 0x20, OPCODE), RD(0x201, 0x34, OPERAND), RD(0x0100, 0x00, DUMMY),
             WR(0x0100, 0x02, STACK_WRITE), WR(0x01FF, 0x02, STACK_WRITE), RD(0x202, 0x12, OPERAND),
             END},
     .f = {F_PC | F_S, 0x1234, 0, 0, 0, 0xFE, 0, 0}, .chk = {M(0x0100, 0x02), M(0x01FF, 0x02)}},
    {.name = "edge.branch_cross_page.count", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_COUNT, .pc = 0x02FD, .prog = {0xD0, 0x05}, .ncyc = 4,
     .f = {F_PC, 0x0304, 0, 0, 0, 0, 0, 0}},
    {.name = "edge.branch_cross_page.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .pc = 0x02FD, .prog = {0xD0, 0x05}, .ncyc = 4,
     .exp = {RD(0x2FD, 0xD0, OPCODE), RD(0x2FE, 0x05, OPERAND), RD(0x2FF, 0x00, DUMMY_STREAM),
             RD(0x0204, 0x00, DUMMY), END},
     .f = {F_PC, 0x0304, 0, 0, 0, 0, 0, 0}},
    {.name = "edge.abx_cross_page.count", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_COUNT, .prog = {0xBD, 0xFF, 0x12}, .x = 0x01, .mem = {M(0x1300, 0x42)}, .ncyc = 5,
     .f = {F_PC | F_A, 0x0203, 0x42, 0, 0, 0, 0, 0}},
    {.name = "edge.abx_cross_page.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0xBD, 0xFF, 0x12}, .x = 0x01, .mem = {M(0x1300, 0x42)}, .ncyc = 5,
     .exp = {RD(0x200, 0xBD, OPCODE), RD(0x201, 0xFF, OPERAND), RD(0x202, 0x12, OPERAND),
             RD(0x202, 0x12, DUMMY_STREAM), RD(0x1300, 0x42, READ), END},
     .f = {F_PC | F_A, 0x0203, 0x42, 0, 0, 0, 0, 0}},
    {.name = "edge.abx_carry_into_page_ff.count", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_COUNT, .prog = {0xBD, 0xFF, 0xFE}, .x = 0x01, .mem = {M(0xFF00, 0x42)}, .ncyc = 5,
     .f = {F_PC | F_A, 0x0203, 0x42, 0, 0, 0, 0, 0}},
    {.name = "edge.abx_carry_into_page_ff.cycles", .tag = "SST-MODEL", .unv = 1, .profiles = P_BOTH,
     .prog = {0xBD, 0xFF, 0xFE}, .x = 0x01, .mem = {M(0xFF00, 0x42)}, .ncyc = 5,
     .exp = {RD(0x200, 0xBD, OPCODE), RD(0x201, 0xFF, OPERAND), RD(0x202, 0xFE, OPERAND),
             RD(0x202, 0xFE, DUMMY_STREAM), RD(0xFF00, 0x42, READ), END},
     .f = {F_PC | F_A, 0x0203, 0x42, 0, 0, 0, 0, 0}},
    /* decimal results at $99: ADC 55+44, ADC 99+01 (to 00 with carry), SBC 00-01 (to 99
       with borrow). N, Z and C are compared (V in decimal mode is unverified). */
    {.name = "edge.decimal_adc_to_99", .tag = "WDC", .unv = 1, .profiles = P_BOTH, .mode = SC_FINAL,
     .prog = {0x69, 0x44}, .a = 0x55, .p = 0x38,
     .f = {F_PC | F_A | F_P, 0x0202, 0x99, 0, 0, 0, 0x80, 0x83}},
    {.name = "edge.decimal_adc_99_plus_01", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_FINAL, .prog = {0x69, 0x01}, .a = 0x99, .p = 0x38,
     .f = {F_PC | F_A | F_P, 0x0202, 0x00, 0, 0, 0, 0x03, 0x83}},
    {.name = "edge.decimal_sbc_from_00", .tag = "WDC", .unv = 1, .profiles = P_BOTH,
     .mode = SC_FINAL, .prog = {0xE9, 0x01}, .a = 0x00, .p = 0x39,
     .f = {F_PC | F_A | F_P, 0x0202, 0x99, 0, 0, 0, 0x80, 0x83}},
    {.name = "edge.decimal_adc_extra_cycle", .tag = "INFERRED", .unv = 1, .profiles = P_BOTH,
     .prog = {0x69, 0x44}, .a = 0x55, .p = 0x38, .ncyc = 3,
     .exp = {RD(0x200, 0x69, OPCODE), RD(0x201, 0x44, OPERAND), RD(0x201, 0x44, DUMMY), END},
     .f = {F_PC | F_A, 0x0202, 0x99, 0, 0, 0, 0, 0}},
};

#define K_SCEN_COUNT ((unsigned)(sizeof k_scen / sizeof k_scen[0]))

/* ---- generated scenarios ------------------------------------------------------
   The Lynx II bit operations for bits 0 to 7 and the Lynx I shapes of the 32
   bit-operation opcodes. The expected values are derived here from the rule, not
   read from the CPU:
     RMBn / SMBn   clear / set bit n of the zero page byte, flags unchanged; cycles
                   opcode, operand, read, second read, write (RKW matrix 2 5)
     BBRn / BBSn   branch when bit n is clear / set, flags unchanged; cycles opcode,
                   operand zp, read, dummy read, operand offset, then a taken branch
                   adds one dummy stream cycle (RKW matrix 3 5**)
     Lynx I        the SingleStepTests synertek shapes: $x7 with an even high nibble
                   is a 2-byte NOP with a zero page read, with an odd high nibble a
                   2-byte NOP with a zero page,X read; $xF with an even high nibble a
                   3-byte NOP with no data read, with an odd high nibble the same
                   plus a re-read of the last operand byte. The operand byte is $E8,
                   an instruction that would change X if it ran as an instruction;
                   CMD table V says 1 byte and 1 cycle, so these are SST-MODEL and
                   unverified, and X must stay unchanged. */

#define GEN_MAX 96
static scen g_gen[GEN_MAX];
static char g_gen_names[GEN_MAX][40];
static unsigned g_gen_count;

static scen *gen_add(const char *name)
{
    scen *s;
    if (g_gen_count >= GEN_MAX) {
        fprintf(stderr, "scenarios: generated scenario table full\n");
        return NULL;
    }
    s = &g_gen[g_gen_count];
    (void)snprintf(g_gen_names[g_gen_count], sizeof g_gen_names[0], "%s", name);
    s->name = g_gen_names[g_gen_count];
    g_gen_count++;
    return s;
}

static void gen_cyc(scen *s, unsigned *n, uint16_t addr, uint8_t data, uint8_t w, uint8_t kind)
{
    s->exp[*n].addr = addr;
    s->exp[*n].data = data;
    s->exp[*n].w = w;
    s->exp[*n].kind = kind;
    (*n)++;
    s->exp[*n].kind = 0xFFu;
    s->ncyc = *n;
}

static void gen_mem(scen *s, unsigned i, uint16_t addr, uint8_t val)
{
    s->mem[i].addr = addr;
    s->mem[i].val = val;
    s->mem[i].used = 1u;
}

static void gen_bitops(void)
{
    unsigned n;
    for (n = 0; n < 8u; n++) {
        uint8_t bit = (uint8_t)(1u << n);
        uint8_t nbit = (uint8_t)(~bit);
        unsigned pass;
        scen *s;
        unsigned c;
        char nm[40];
        for (pass = 0; pass < 2u; pass++) { /* 0 RMB, 1 SMB */
            uint8_t before = pass == 0u ? 0xFFu : 0x00u;
            uint8_t after = pass == 0u ? nbit : bit;
            (void)snprintf(nm, sizeof nm, "bitop.%s%u", pass == 0u ? "rmb" : "smb", n);
            s = gen_add(nm);
            if (s == NULL) {
                return;
            }
            c = 0;
            s->tag = "RKW";
            s->unv = 1u;
            s->profiles = P_LYNX2;
            s->bitop = 1u;
            s->prog[0] = (uint8_t)((pass == 0u ? 0x07u : 0x87u) + 0x10u * n);
            s->prog[1] = 0x34u;
            s->p = 0xF3u;
            gen_mem(s, 0, 0x0034u, before);
            gen_cyc(s, &c, 0x0200u, s->prog[0], 0u, LYNX__CK_OPCODE);
            gen_cyc(s, &c, 0x0201u, 0x34u, 0u, LYNX__CK_OPERAND);
            gen_cyc(s, &c, 0x0034u, before, 0u, LYNX__CK_READ);
            gen_cyc(s, &c, 0x0034u, before, 0u, LYNX__CK_RMW_READ);
            gen_cyc(s, &c, 0x0034u, after, 1u, LYNX__CK_WRITE);
            s->f.mask = (uint8_t)(F_PC | F_P);
            s->f.pc = 0x0202u;
            s->f.p = 0xF3u;
            s->chk[0].addr = 0x0034u;
            s->chk[0].val = after;
            s->chk[0].used = 1u;
        }
        for (pass = 0; pass < 2u; pass++) { /* 0 BBR, 1 BBS */
            unsigned taken;
            for (taken = 0; taken < 2u; taken++) {
                /* BBR is taken on a clear bit, BBS on a set bit. The other seven bits
                   hold the opposite value so only bit n decides. */
                bool is_taken = taken != 0u;
                uint8_t b;
                (void)snprintf(nm, sizeof nm, "bitop.%s%u.%s", pass == 0u ? "bbr" : "bbs", n,
                               is_taken ? "taken" : "not_taken");
                s = gen_add(nm);
                if (s == NULL) {
                    return;
                }
                if (pass == 0u) {
                    b = is_taken ? nbit : bit; /* BBR is taken on a clear bit */
                } else {
                    b = is_taken ? bit : nbit; /* BBS is taken on a set bit */
                }
                c = 0;
                s->tag = "RKW";
                s->unv = 1u;
                s->profiles = P_LYNX2;
                s->bitop = 1u;
                s->prog[0] = (uint8_t)((pass == 0u ? 0x0Fu : 0x8Fu) + 0x10u * n);
                s->prog[1] = 0x34u;
                s->prog[2] = 0x05u;
                s->p = 0xF3u;
                gen_mem(s, 0, 0x0034u, b);
                gen_cyc(s, &c, 0x0200u, s->prog[0], 0u, LYNX__CK_OPCODE);
                gen_cyc(s, &c, 0x0201u, 0x34u, 0u, LYNX__CK_OPERAND);
                gen_cyc(s, &c, 0x0034u, b, 0u, LYNX__CK_READ);
                gen_cyc(s, &c, 0x0034u, b, 0u, LYNX__CK_DUMMY);
                gen_cyc(s, &c, 0x0202u, 0x05u, 0u, LYNX__CK_OPERAND);
                if (is_taken) {
                    gen_cyc(s, &c, 0x0203u, 0x00u, 0u, LYNX__CK_DUMMY_STREAM);
                }
                s->f.mask = (uint8_t)(F_PC | F_P);
                s->f.pc = is_taken ? 0x0208u : 0x0203u;
                s->f.p = 0xF3u;
            }
        }
    }
}

static void gen_lynx1_shapes(void)
{
    unsigned hi;
    unsigned low;
    for (low = 7u; low <= 15u; low += 8u) {
        for (hi = 0; hi < 16u; hi++) {
            bool odd = (hi & 1u) != 0u;
            uint8_t op = (uint8_t)((hi << 4) | low);
            char nm[40];
            scen *s;
            unsigned c = 0;
            (void)snprintf(nm, sizeof nm, "lynx1.nop_%02x", (unsigned)op);
            s = gen_add(nm);
            if (s == NULL) {
                return;
            }
            s->tag = "SST-MODEL";
            s->unv = 1u;
            s->profiles = P_LYNX1;
            s->prog[0] = op;
            s->prog[1] = 0xE8u;
            s->x = 0x10u;
            gen_cyc(s, &c, 0x0200u, op, 0u, LYNX__CK_OPCODE);
            gen_cyc(s, &c, 0x0201u, 0xE8u, 0u, LYNX__CK_OPERAND);
            s->f.mask = (uint8_t)(F_PC | F_X);
            s->f.x = 0x10u;
            if (low == 7u) {
                if (odd) {
                    gen_cyc(s, &c, 0x00E8u, 0x00u, 0u, LYNX__CK_DUMMY);
                    gen_cyc(s, &c, 0x00F8u, 0x00u, 0u, LYNX__CK_READ);
                } else {
                    gen_cyc(s, &c, 0x00E8u, 0x00u, 0u, LYNX__CK_READ);
                }
                s->f.pc = 0x0202u;
            } else {
                s->prog[2] = 0xE8u;
                gen_cyc(s, &c, 0x0202u, 0xE8u, 0u, LYNX__CK_OPERAND);
                if (odd) {
                    gen_cyc(s, &c, 0x0202u, 0xE8u, 0u, LYNX__CK_DUMMY);
                }
                s->f.pc = 0x0203u;
            }
        }
    }
}

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
    c.s = s->sz != 0u ? 0u : (s->s != 0u ? s->s : 0xFDu);
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

static const scen *scen_at(unsigned k)
{
    return k < K_SCEN_COUNT ? &k_scen[k] : &g_gen[k - K_SCEN_COUNT];
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
    unsigned wrong_passed = 0;
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

    gen_bitops();
    gen_lynx1_shapes();
    for (k = 0; k < K_SCEN_COUNT + g_gen_count; k++) {
        bad += validate_scen(scen_at(k));
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

    for (k = 0; k < K_SCEN_COUNT + g_gen_count; k++) {
        const scen *s = scen_at(k);
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
        if (sprof != rprof && fails == 0) {
            /* the wrong-profile control is only meaningful if every scenario fails */
            fprintf(stderr, "scenario %s: wrong-profile run passed\n", s->name);
            wrong_passed++;
        }
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
    if (sprof != rprof && (wrong_passed != 0u || ran == 0u)) {
        /* exit 4: the control itself is broken, not the CPU under test */
        fprintf(stderr, "scenarios: wrong-profile control invalid: %u of %u scenarios passed\n",
                wrong_passed, ran);
        return 4;
    }
    return total_fails != 0 ? 1 : 0;
}
