/* The cycle-stepped CPU. Every instruction is a script of micro-operations, one
   per bus cycle, chosen by the decode row's mode. A micro-operation says which
   request to post (address, kind, data, write) and what to do with the data of
   the completed access. Cycle counts are not stored anywhere: they are the
   length of the script, minus the conditional micro-operations that do not
   apply (page crossing, taken branch, decimal extra cycle).

   Coding rules: one bus access per call of lynx__cpu_complete, unsigned
   arithmetic only, explicit narrowing casts, no pointer tables, no function
   pointers. Dispatch is switch statements. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"

/* Processor status bits. */
#define P_C 0x01u
#define P_Z 0x02u
#define P_I 0x04u
#define P_D 0x08u
#define P_B 0x10u
#define P_U 0x20u
#define P_V 0x40u
#define P_N 0x80u

/* Running sequences, held in the two LYNX__CS_SEQ_MASK bits of flags. */
#define SEQ_NONE 0u
#define SEQ_IRQ 1u
#define SEQ_NMI 2u
#define SEQ_RESET 3u
#define SEQ_SHIFT 4u

/* Slots per script: slot 0 is the opcode fetch (never in a script), slots 1 to 7
   hold micro-operations. Entry and reset sequences use slots 1 to 7 as well,
   with step + 1 as the slot, because they have no opcode fetch. */
#define SLOTS 8u

/* Micro-operations. */
enum {
    U_END = 0,
    U_IMPL,        /* DUMMY_STREAM at pc; implied operation */
    U_IMM,         /* OPERAND at pc; pc++; read operation on the data */
    U_ARG_LO,      /* OPERAND at pc; ea = data; pc++ */
    U_ARG_HI,      /* OPERAND at pc; ea gets the high byte; pc++ */
    U_ARG_HI_X,    /* as U_ARG_HI, then ea += x with the page-cross flag in tmp */
    U_ARG_HI_Y,    /* as U_ARG_HI, then ea += y */
    U_ZPX,         /* DUMMY at the zero page address; ea = zp + x, 8 bits */
    U_ZPY,         /* as U_ZPX with y */
    U_DUMMY_EA,    /* DUMMY at ea */
    U_DUMMY_EA1,   /* DUMMY at ea + 1, 16 bits */
    U_PTR_LO,      /* POINTER at ea; tmp = data */
    U_PTR_HI8,     /* POINTER at ea + 1 within the zero page; ea = pointer */
    U_PTR_HI8_Y,   /* as U_PTR_HI8, then ea += y with the page-cross flag in tmp */
    U_PTR_HI16,    /* POINTER at ea + 1, 16 bits; pc = pointer */
    U_X_S,         /* DUMMY_STREAM at the last stream byte */
    U_X_S_C,       /* as U_X_S, only on a page cross */
    U_X_D,         /* DUMMY at the last stream byte */
    U_X_D_C,       /* as U_X_D, only on a page cross */
    U_X_PM2,       /* DUMMY two bytes back in the stream; ea += x, 16 bits */
    U_READ,        /* READ at ea; read operation */
    U_WRITE,       /* WRITE at ea of the store value */
    U_RMW1,        /* READ at ea; tmp = data */
    U_RMW2,        /* RMW_READ at ea; tmp = modified value */
    U_RMWW,        /* WRITE at ea of tmp */
    U_DECX_EA_C,   /* DUMMY at ea, only for decimal ADC and SBC */
    U_DECX_IMM_C,  /* DUMMY at the immediate byte, only for decimal ADC and SBC */
    U_BR_TAKEN,    /* DUMMY_STREAM at pc, only when taken; pc = target unless a page is crossed */
    U_BR_CROSS_REL, /* DUMMY in the page of pc with the target low byte; pc = target */
    U_BR_CROSS_ZP, /* DUMMY at pc, only when taken across a page; pc = target */
    U_JMP_HI,      /* OPERAND at pc; pc = ea with this high byte */
    U_STK_DUMMY,   /* DUMMY at the stack top */
    U_DUMMY_PC,    /* DUMMY at pc */
    U_DUMMY_PC_S,  /* DUMMY_STREAM at pc */
    U_PUSH,        /* STACK_WRITE of the register of the push kind; s-- */
    U_PUSH_PCH,
    U_PUSH_PCL,
    U_PUSH_P_BRK,  /* push p with B set; set I, clear D */
    U_PUSH_P_INT,  /* push p with B clear; set I, clear D */
    U_PULL,        /* STACK_READ at s + 1; s++; pull operation */
    U_PULL_P,
    U_PULL_PCL,
    U_PULL_PCH_PC,
    U_PULL_PCH_EA,
    U_RTS_DUMMY,   /* DUMMY at ea; pc = ea + 1 */
    U_VEC_LO,
    U_VEC_HI,
    U_BRK_OP,      /* OPERAND at pc; pc++ (the signature byte) */
    U_RST_STK      /* STACK_READ at s; s-- */
};

/* The scripts, indexed by mode and slot. Integer data only. */
static const uint8_t lynx__script[LYNX__M_COUNT][SLOTS] = {
    [LYNX__M_IMPL] = {0, U_IMPL},
    [LYNX__M_IMM] = {0, U_IMM, U_DECX_IMM_C},
    [LYNX__M_NOP1] = {0},
    [LYNX__M_ZP_R] = {0, U_ARG_LO, U_READ, U_DECX_EA_C},
    [LYNX__M_ZP_W] = {0, U_ARG_LO, U_WRITE},
    [LYNX__M_ZP_RMW] = {0, U_ARG_LO, U_RMW1, U_RMW2, U_RMWW},
    [LYNX__M_ZPX_R] = {0, U_ARG_LO, U_ZPX, U_READ, U_DECX_EA_C},
    [LYNX__M_ZPX_W] = {0, U_ARG_LO, U_ZPX, U_WRITE},
    [LYNX__M_ZPX_RMW] = {0, U_ARG_LO, U_ZPX, U_RMW1, U_RMW2, U_RMWW},
    [LYNX__M_ZPY_R] = {0, U_ARG_LO, U_ZPY, U_READ, U_DECX_EA_C},
    [LYNX__M_ZPY_W] = {0, U_ARG_LO, U_ZPY, U_WRITE},
    [LYNX__M_ABS_R] = {0, U_ARG_LO, U_ARG_HI, U_READ, U_DECX_EA_C},
    [LYNX__M_ABS_W] = {0, U_ARG_LO, U_ARG_HI, U_WRITE},
    [LYNX__M_ABS_RMW] = {0, U_ARG_LO, U_ARG_HI, U_RMW1, U_RMW2, U_RMWW},
    [LYNX__M_ABX_R] = {0, U_ARG_LO, U_ARG_HI_X, U_X_S_C, U_READ, U_DECX_EA_C},
    [LYNX__M_ABX_W] = {0, U_ARG_LO, U_ARG_HI_X, U_X_S, U_WRITE},
    [LYNX__M_ABX_RMW] = {0, U_ARG_LO, U_ARG_HI_X, U_X_D_C, U_RMW1, U_RMW2, U_RMWW},
    [LYNX__M_ABX_RMWI] = {0, U_ARG_LO, U_ARG_HI_X, U_X_D, U_RMW1, U_RMW2, U_RMWW},
    [LYNX__M_ABY_R] = {0, U_ARG_LO, U_ARG_HI_Y, U_X_S_C, U_READ, U_DECX_EA_C},
    [LYNX__M_ABY_W] = {0, U_ARG_LO, U_ARG_HI_Y, U_X_S, U_WRITE},
    [LYNX__M_IZX_R] = {0, U_ARG_LO, U_ZPX, U_PTR_LO, U_PTR_HI8, U_READ, U_DECX_EA_C},
    [LYNX__M_IZX_W] = {0, U_ARG_LO, U_ZPX, U_PTR_LO, U_PTR_HI8, U_WRITE},
    [LYNX__M_IZY_R] = {0, U_ARG_LO, U_PTR_LO, U_PTR_HI8_Y, U_X_S_C, U_READ, U_DECX_EA_C},
    [LYNX__M_IZY_W] = {0, U_ARG_LO, U_PTR_LO, U_PTR_HI8_Y, U_X_S, U_WRITE},
    [LYNX__M_IZP_R] = {0, U_ARG_LO, U_PTR_LO, U_PTR_HI8, U_READ, U_DECX_EA_C},
    [LYNX__M_IZP_W] = {0, U_ARG_LO, U_PTR_LO, U_PTR_HI8, U_WRITE},
    [LYNX__M_REL] = {0, U_ARG_LO, U_BR_TAKEN, U_BR_CROSS_REL},
    [LYNX__M_ZPREL] = {0, U_ARG_LO, U_RMW1, U_DUMMY_EA, U_ARG_LO, U_BR_TAKEN, U_BR_CROSS_ZP},
    [LYNX__M_JMP_ABS] = {0, U_ARG_LO, U_JMP_HI},
    [LYNX__M_JMP_IND] = {0, U_ARG_LO, U_ARG_HI, U_PTR_LO, U_PTR_HI16, U_DUMMY_EA1},
    [LYNX__M_JMP_INDX] = {0, U_ARG_LO, U_ARG_HI, U_X_PM2, U_PTR_LO, U_PTR_HI16},
    [LYNX__M_JSR] = {0, U_ARG_LO, U_STK_DUMMY, U_PUSH_PCH, U_PUSH_PCL, U_JMP_HI},
    [LYNX__M_PUSH] = {0, U_DUMMY_PC_S, U_PUSH},
    [LYNX__M_PULL] = {0, U_DUMMY_PC, U_STK_DUMMY, U_PULL},
    [LYNX__M_RTS] = {0, U_DUMMY_PC, U_STK_DUMMY, U_PULL_PCL, U_PULL_PCH_EA, U_RTS_DUMMY},
    [LYNX__M_RTI] = {0, U_DUMMY_PC, U_STK_DUMMY, U_PULL_P, U_PULL_PCL, U_PULL_PCH_PC},
    [LYNX__M_BRK] = {0, U_BRK_OP, U_PUSH_PCH, U_PUSH_PCL, U_PUSH_P_BRK, U_VEC_LO, U_VEC_HI},
    [LYNX__M_NOP_ABS3] = {0, U_ARG_LO, U_ARG_HI},
    [LYNX__M_NOP_ABS4] = {0, U_ARG_LO, U_ARG_HI, U_X_D},
    [LYNX__M_ENTRY] = {0, U_DUMMY_PC_S, U_DUMMY_PC_S, U_PUSH_PCH, U_PUSH_PCL, U_PUSH_P_INT, U_VEC_LO,
                       U_VEC_HI},
    [LYNX__M_RESETSEQ] = {0, U_DUMMY_PC_S, U_DUMMY_PC_S, U_RST_STK, U_RST_STK, U_RST_STK, U_VEC_LO,
                          U_VEC_HI}
};

/* ---- small helpers ---------------------------------------------------- */

static uint8_t seq_of(const lynx__cpu *c)
{
    return (uint8_t)((c->flags & LYNX__CS_SEQ_MASK) >> SEQ_SHIFT);
}

static uint8_t mode_of(const lynx__cpu *c, const lynx__cpu_decode *t)
{
    uint8_t seq = seq_of(c);
    if (seq == SEQ_NONE) {
        return t[c->opcode].mode;
    }
    return seq == SEQ_RESET ? (uint8_t)LYNX__M_RESETSEQ : (uint8_t)LYNX__M_ENTRY;
}

/* The script slot of the pending cycle. Not for the opcode fetch. */
static uint8_t slot_of(const lynx__cpu *c)
{
    return (uint8_t)(c->step + (seq_of(c) == SEQ_NONE ? 0u : 1u));
}

static void set_flag(lynx__cpu *c, uint32_t mask, bool on)
{
    uint32_t p = c->p;
    if (on) {
        p |= mask;
    } else {
        p &= ~mask;
    }
    c->p = (uint8_t)p;
}

static void set_nz(lynx__cpu *c, uint32_t v)
{
    set_flag(c, P_N, (v & 0x80u) != 0u);
    set_flag(c, P_Z, (v & 0xFFu) == 0u);
}

static uint16_t stack_addr(uint32_t s)
{
    return (uint16_t)(0x100u | (s & 0xFFu));
}

static uint16_t vector_addr(const lynx__cpu *c)
{
    uint8_t seq = seq_of(c);
    if (seq == SEQ_NMI) {
        return 0xFFFAu;
    }
    if (seq == SEQ_RESET) {
        return 0xFFFCu;
    }
    return 0xFFFEu;
}

/* Branch target of the offset held in the low byte of ea, from pc. */
static uint16_t br_target(const lynx__cpu *c)
{
    uint32_t off = c->ea & 0xFFu;
    uint32_t delta = off - (((off & 0x80u) != 0u) ? 0x100u : 0u);
    return (uint16_t)(c->pc + delta);
}

static bool br_cross(const lynx__cpu *c)
{
    return (br_target(c) & 0xFF00u) != (c->pc & 0xFF00u);
}

/* The branch taken-dummy cycle may move pc, so it records its page-cross
   answer in ea above the offset byte: 0x100 crossed, 0x200 not crossed. Before
   that cycle the answer is computed live. */
#define BR_CROSSED 0x100u
#define BR_SAME_PAGE 0x200u

static bool br_cross_now(const lynx__cpu *c)
{
    if ((c->ea & BR_SAME_PAGE) != 0u) {
        return false;
    }
    if ((c->ea & BR_CROSSED) != 0u) {
        return true;
    }
    return br_cross(c);
}

static bool br_taken(const lynx__cpu *c, const lynx__cpu_decode *t)
{
    uint32_t p = c->p;
    uint32_t bit = (c->opcode >> 4) & 7u;
    switch (t[c->opcode].kind) {
    case LYNX__K_BRA: return true;
    case LYNX__K_BPL: return (p & P_N) == 0u;
    case LYNX__K_BMI: return (p & P_N) != 0u;
    case LYNX__K_BVC: return (p & P_V) == 0u;
    case LYNX__K_BVS: return (p & P_V) != 0u;
    case LYNX__K_BCC: return (p & P_C) == 0u;
    case LYNX__K_BCS: return (p & P_C) != 0u;
    case LYNX__K_BNE: return (p & P_Z) == 0u;
    case LYNX__K_BEQ: return (p & P_Z) != 0u;
    case LYNX__K_BBR: return (((uint32_t)c->tmp >> bit) & 1u) == 0u;
    case LYNX__K_BBS: return (((uint32_t)c->tmp >> bit) & 1u) != 0u;
    default: return false;
    }
}

/* ---- operations -------------------------------------------------------- */

static void do_adc(lynx__cpu *c, uint32_t m)
{
    uint32_t a = c->a;
    uint32_t cin = c->p & P_C;
    uint32_t r;
    if ((c->p & P_D) == 0u) {
        r = a + m + cin;
        set_flag(c, P_C, r > 0xFFu);
        set_flag(c, P_V, ((~(a ^ m)) & (a ^ r) & 0x80u) != 0u);
    } else {
        uint32_t al = (a & 0x0Fu) + (m & 0x0Fu) + cin;
        if (al >= 0x0Au) {
            al = ((al + 6u) & 0x0Fu) + 0x10u;
        }
        r = (a & 0xF0u) + (m & 0xF0u) + al;
        set_flag(c, P_V, ((~(a ^ m)) & (a ^ (r & 0xFFu)) & 0x80u) != 0u);
        if (r >= 0xA0u) {
            r += 0x60u;
        }
        set_flag(c, P_C, r >= 0x100u);
    }
    c->a = (uint8_t)(r & 0xFFu);
    set_nz(c, c->a);
}

static void do_sbc(lynx__cpu *c, uint32_t m)
{
    uint32_t a = c->a;
    if ((c->p & P_D) == 0u) {
        do_adc(c, (m ^ 0xFFu) & 0xFFu);
    } else {
        uint32_t borrow = 1u - (c->p & P_C);
        uint32_t r = a - m - borrow;
        bool neg = a < (m + borrow);
        uint32_t res = r & 0xFFu;
        set_flag(c, P_C, !neg);
        set_flag(c, P_V, ((a ^ m) & (a ^ r) & 0x80u) != 0u);
        if ((a & 0x0Fu) < ((m & 0x0Fu) + borrow)) {
            res = (res - 6u) & 0xFFu;
        }
        if (neg) {
            res = (res - 0x60u) & 0xFFu;
        }
        c->a = (uint8_t)res;
        set_nz(c, res);
    }
}

static void do_cmp(lynx__cpu *c, uint32_t reg, uint32_t m)
{
    set_flag(c, P_C, reg >= m);
    set_nz(c, (reg - m) & 0xFFu);
}

/* Operation applied to the data of a read cycle (also the immediate byte). */
static void op_read(lynx__cpu *c, uint8_t kind, uint8_t data)
{
    uint32_t m = data;
    switch (kind) {
    case LYNX__K_LDA: c->a = data; set_nz(c, m); break;
    case LYNX__K_LDX: c->x = data; set_nz(c, m); break;
    case LYNX__K_LDY: c->y = data; set_nz(c, m); break;
    case LYNX__K_ORA: c->a = (uint8_t)(c->a | data); set_nz(c, c->a); break;
    case LYNX__K_AND: c->a = (uint8_t)(c->a & data); set_nz(c, c->a); break;
    case LYNX__K_EOR: c->a = (uint8_t)(c->a ^ data); set_nz(c, c->a); break;
    case LYNX__K_ADC: do_adc(c, m); break;
    case LYNX__K_SBC: do_sbc(c, m); break;
    case LYNX__K_CMP: do_cmp(c, c->a, m); break;
    case LYNX__K_CPX: do_cmp(c, c->x, m); break;
    case LYNX__K_CPY: do_cmp(c, c->y, m); break;
    case LYNX__K_BIT:
        set_flag(c, P_Z, (c->a & data) == 0u);
        set_flag(c, P_N, (m & 0x80u) != 0u);
        set_flag(c, P_V, (m & 0x40u) != 0u);
        break;
    case LYNX__K_BITI: set_flag(c, P_Z, (c->a & data) == 0u); break;
    default: break; /* NOP: the data is read and dropped */
    }
}

/* The shift, rotate, increment, decrement and bit operations on a byte. */
static uint8_t op_rmw(lynx__cpu *c, uint8_t kind, uint8_t v)
{
    uint32_t cin = c->p & P_C;
    uint32_t in = v;
    uint32_t r = in;
    uint32_t bit = (uint32_t)1u << ((c->opcode >> 4) & 7u);
    switch (kind) {
    case LYNX__K_ASL:
        set_flag(c, P_C, (in & 0x80u) != 0u);
        r = (in << 1) & 0xFFu;
        set_nz(c, r);
        break;
    case LYNX__K_LSR:
        set_flag(c, P_C, (in & 1u) != 0u);
        r = in >> 1;
        set_nz(c, r);
        break;
    case LYNX__K_ROL:
        set_flag(c, P_C, (in & 0x80u) != 0u);
        r = ((in << 1) | cin) & 0xFFu;
        set_nz(c, r);
        break;
    case LYNX__K_ROR:
        set_flag(c, P_C, (in & 1u) != 0u);
        r = (in >> 1) | (cin << 7);
        set_nz(c, r);
        break;
    case LYNX__K_INC: r = (in + 1u) & 0xFFu; set_nz(c, r); break;
    case LYNX__K_DEC: r = (in - 1u) & 0xFFu; set_nz(c, r); break;
    case LYNX__K_TSB:
        set_flag(c, P_Z, (c->a & in) == 0u);
        r = in | c->a;
        break;
    case LYNX__K_TRB:
        set_flag(c, P_Z, (c->a & in) == 0u);
        r = in & ~((uint32_t)c->a) & 0xFFu;
        break;
    case LYNX__K_RMB: r = in & ~bit & 0xFFu; break;
    case LYNX__K_SMB: r = (in | bit) & 0xFFu; break;
    default: break;
    }
    return (uint8_t)r;
}

/* Implied and accumulator operations. */
static void op_implied(lynx__cpu *c, uint8_t kind)
{
    switch (kind) {
    case LYNX__K_ASLA: c->a = op_rmw(c, LYNX__K_ASL, c->a); break;
    case LYNX__K_LSRA: c->a = op_rmw(c, LYNX__K_LSR, c->a); break;
    case LYNX__K_ROLA: c->a = op_rmw(c, LYNX__K_ROL, c->a); break;
    case LYNX__K_RORA: c->a = op_rmw(c, LYNX__K_ROR, c->a); break;
    case LYNX__K_INCA: c->a = op_rmw(c, LYNX__K_INC, c->a); break;
    case LYNX__K_DECA: c->a = op_rmw(c, LYNX__K_DEC, c->a); break;
    case LYNX__K_INX: c->x = (uint8_t)(c->x + 1u); set_nz(c, c->x); break;
    case LYNX__K_INY: c->y = (uint8_t)(c->y + 1u); set_nz(c, c->y); break;
    case LYNX__K_DEX: c->x = (uint8_t)(c->x - 1u); set_nz(c, c->x); break;
    case LYNX__K_DEY: c->y = (uint8_t)(c->y - 1u); set_nz(c, c->y); break;
    case LYNX__K_TAX: c->x = c->a; set_nz(c, c->x); break;
    case LYNX__K_TAY: c->y = c->a; set_nz(c, c->y); break;
    case LYNX__K_TXA: c->a = c->x; set_nz(c, c->a); break;
    case LYNX__K_TYA: c->a = c->y; set_nz(c, c->a); break;
    case LYNX__K_TSX: c->x = c->s; set_nz(c, c->x); break;
    case LYNX__K_TXS: c->s = c->x; break;
    case LYNX__K_CLC: set_flag(c, P_C, false); break;
    case LYNX__K_SEC: set_flag(c, P_C, true); break;
    case LYNX__K_CLI: set_flag(c, P_I, false); break;
    case LYNX__K_SEI: set_flag(c, P_I, true); break;
    case LYNX__K_CLD: set_flag(c, P_D, false); break;
    case LYNX__K_SED: set_flag(c, P_D, true); break;
    case LYNX__K_CLV: set_flag(c, P_V, false); break;
    default: break; /* NOP */
    }
}

static uint8_t store_value(const lynx__cpu *c, uint8_t kind)
{
    switch (kind) {
    case LYNX__K_STA: return c->a;
    case LYNX__K_STX: return c->x;
    case LYNX__K_STY: return c->y;
    default: return 0u; /* STZ */
    }
}

static uint8_t push_value(const lynx__cpu *c, uint8_t kind)
{
    switch (kind) {
    case LYNX__K_PHA: return c->a;
    case LYNX__K_PHP: return (uint8_t)(c->p | P_B | P_U);
    case LYNX__K_PHX: return c->x;
    default: return c->y; /* PHY */
    }
}

static void op_pull(lynx__cpu *c, uint8_t kind, uint8_t v)
{
    switch (kind) {
    case LYNX__K_PLA: c->a = v; set_nz(c, v); break;
    case LYNX__K_PLX: c->x = v; set_nz(c, v); break;
    case LYNX__K_PLY: c->y = v; set_nz(c, v); break;
    default: c->p = (uint8_t)(v | P_B | P_U); break; /* PLP */
    }
}

/* ea = ea + idx with the page-cross flag in tmp. */
static void index_ea(lynx__cpu *c, uint32_t idx)
{
    uint32_t base = c->ea;
    uint32_t fin = (base + idx) & 0xFFFFu;
    c->tmp = (uint8_t)((((base ^ fin) & 0xFF00u) != 0u) ? 1u : 0u);
    c->ea = (uint16_t)fin;
}

static void push_done(lynx__cpu *c)
{
    c->s = (uint8_t)(c->s - 1u);
}

static void pull_done(lynx__cpu *c)
{
    c->s = (uint8_t)(c->s + 1u);
}

/* ---- the request of the pending micro-operation ------------------------- */

/* A micro-operation that is skipped when its condition is false. */
static bool uop_active(const lynx__cpu *c, const lynx__cpu_decode *t, uint8_t u)
{
    switch (u) {
    case U_X_S_C:
    case U_X_D_C:
        return c->tmp != 0u;
    case U_BR_TAKEN:
        return br_taken(c, t);
    case U_BR_CROSS_REL:
    case U_BR_CROSS_ZP:
        return br_taken(c, t) && br_cross_now(c);
    case U_DECX_EA_C:
    case U_DECX_IMM_C:
        return (t[c->opcode].flags & LYNX__F_DEC_EXTRA) != 0u && (c->p & P_D) != 0u;
    default:
        return true;
    }
}

/* The first active slot after from, or 0 when the script is done. */
static uint8_t next_slot(const lynx__cpu *c, const lynx__cpu_decode *t, uint8_t mode, uint8_t from)
{
    uint8_t s;
    for (s = (uint8_t)(from + 1u); s < SLOTS; s++) {
        uint8_t u = lynx__script[mode][s];
        if (u == U_END) {
            return 0u;
        }
        if (uop_active(c, t, u)) {
            return s;
        }
    }
    return 0u;
}

static void post_uop(lynx__cpu *c, const lynx__cpu_decode *t, uint8_t u)
{
    uint8_t kind = t[c->opcode].kind;
    uint32_t addr = c->pc;
    uint8_t rk = LYNX__CK_DUMMY;
    uint8_t data = 0u;
    uint8_t wr = 0u;
    switch (u) {
    case U_IMPL: rk = LYNX__CK_DUMMY_STREAM; break;
    case U_IMM:
    case U_ARG_LO:
    case U_ARG_HI:
    case U_ARG_HI_X:
    case U_ARG_HI_Y:
    case U_JMP_HI:
    case U_BRK_OP: rk = LYNX__CK_OPERAND; break;
    case U_ZPX:
    case U_ZPY:
    case U_DUMMY_EA:
    case U_DECX_EA_C: addr = c->ea; break;
    case U_DUMMY_EA1: addr = (c->ea + 1u) & 0xFFFFu; break;
    case U_PTR_LO: addr = c->ea; rk = LYNX__CK_POINTER; break;
    case U_PTR_HI8:
    case U_PTR_HI8_Y: addr = (c->ea + 1u) & 0xFFu; rk = LYNX__CK_POINTER; break;
    case U_PTR_HI16: addr = (c->ea + 1u) & 0xFFFFu; rk = LYNX__CK_POINTER; break;
    case U_X_S:
    case U_X_S_C: addr = c->pc - 1u; rk = LYNX__CK_DUMMY_STREAM; break;
    case U_X_D:
    case U_X_D_C:
    case U_DECX_IMM_C: addr = c->pc - 1u; break;
    case U_X_PM2: addr = c->pc - 2u; break;
    case U_READ: addr = c->ea; rk = LYNX__CK_READ; break;
    case U_RMW1: addr = c->ea; rk = LYNX__CK_READ; break;
    case U_RMW2: addr = c->ea; rk = LYNX__CK_RMW_READ; break;
    case U_WRITE:
        addr = c->ea;
        rk = LYNX__CK_WRITE;
        data = store_value(c, kind);
        wr = 1u;
        break;
    case U_RMWW:
        addr = c->ea;
        rk = LYNX__CK_WRITE;
        data = c->tmp;
        wr = 1u;
        break;
    case U_BR_TAKEN: rk = LYNX__CK_DUMMY_STREAM; break;
    case U_BR_CROSS_REL: addr = (c->pc & 0xFF00u) | (br_target(c) & 0xFFu); break;
    case U_BR_CROSS_ZP: break;
    case U_STK_DUMMY: addr = stack_addr(c->s); break;
    case U_DUMMY_PC: break;
    case U_DUMMY_PC_S: rk = LYNX__CK_DUMMY_STREAM; break;
    case U_PUSH:
        addr = stack_addr(c->s);
        rk = LYNX__CK_STACK_WRITE;
        data = push_value(c, kind);
        wr = 1u;
        break;
    case U_PUSH_PCH:
        addr = stack_addr(c->s);
        rk = LYNX__CK_STACK_WRITE;
        data = (uint8_t)(c->pc >> 8);
        wr = 1u;
        break;
    case U_PUSH_PCL:
        addr = stack_addr(c->s);
        rk = LYNX__CK_STACK_WRITE;
        data = (uint8_t)(c->pc & 0xFFu);
        wr = 1u;
        break;
    case U_PUSH_P_BRK:
        addr = stack_addr(c->s);
        rk = LYNX__CK_STACK_WRITE;
        data = (uint8_t)(c->p | P_B | P_U);
        wr = 1u;
        break;
    case U_PUSH_P_INT:
        addr = stack_addr(c->s);
        rk = LYNX__CK_STACK_WRITE;
        data = (uint8_t)((c->p | P_U) & ~P_B);
        wr = 1u;
        break;
    case U_PULL:
    case U_PULL_P:
    case U_PULL_PCL:
    case U_PULL_PCH_PC:
    case U_PULL_PCH_EA: addr = stack_addr((uint32_t)c->s + 1u); rk = LYNX__CK_STACK_READ; break;
    case U_RTS_DUMMY: addr = c->ea; break;
    case U_VEC_LO: addr = vector_addr(c); rk = LYNX__CK_VECTOR; break;
    case U_VEC_HI: addr = (uint32_t)vector_addr(c) + 1u; rk = LYNX__CK_VECTOR; break;
    case U_RST_STK: addr = stack_addr(c->s); rk = LYNX__CK_STACK_READ; break;
    default: break;
    }
    c->req_addr = (uint16_t)(addr & 0xFFFFu);
    c->req_kind = rk;
    c->req_data = data;
    c->req_flags = wr != 0u ? (uint8_t)LYNX__CF_WRITE : 0u;
}

void lynx__cpu_repost(lynx__cpu *c, const lynx__cpu_decode *t)
{
    uint8_t mode;
    uint8_t slot;
    if (c->step == 0u && seq_of(c) == SEQ_NONE) {
        c->req_addr = c->pc;
        c->req_kind = LYNX__CK_OPCODE;
        c->req_data = 0u;
        c->req_flags = 0u;
        return;
    }
    mode = mode_of(c, t);
    slot = slot_of(c);
    post_uop(c, t, lynx__script[mode][slot]);
    if (next_slot(c, t, mode, slot) == 0u) {
        c->req_flags = (uint8_t)(c->req_flags | LYNX__CF_LAST);
        if (seq_of(c) == SEQ_NONE && mode != LYNX__M_BRK &&
            (t[c->opcode].flags & LYNX__F_NO_POLL) == 0u) {
            c->req_flags = (uint8_t)(c->req_flags | LYNX__CF_POLL);
        }
    }
}

/* ---- completion -------------------------------------------------------- */

static void set_latch(lynx__cpu *c, uint32_t mask)
{
    c->flags = (uint8_t)(c->flags | mask);
}

static void clear_latch(lynx__cpu *c, uint32_t mask)
{
    c->flags = (uint8_t)(c->flags & ~mask);
}

static void poll(lynx__cpu *c, uint8_t lines)
{
    if ((lines & LYNX__LINE_IRQ) != 0u && (c->p & P_I) == 0u) {
        set_latch(c, LYNX__CS_IRQ_TAKE);
    }
    if ((c->flags & LYNX__CS_NMI_PENDING) != 0u) {
        set_latch(c, LYNX__CS_NMI_TAKE);
    }
}

/* Back at a boundary with the canonical zeros; a taken interrupt starts its
   entry sequence here instead of an opcode fetch. */
static void finish(lynx__cpu *c)
{
    c->step = 0u;
    c->opcode = 0u;
    c->ea = 0u;
    c->tmp = 0u;
    clear_latch(c, LYNX__CS_SEQ_MASK);
    if ((c->flags & LYNX__CS_NMI_TAKE) != 0u) {
        clear_latch(c, LYNX__CS_IRQ_TAKE | LYNX__CS_NMI_TAKE | LYNX__CS_NMI_PENDING);
        set_latch(c, SEQ_NMI << SEQ_SHIFT);
    } else if ((c->flags & LYNX__CS_IRQ_TAKE) != 0u) {
        clear_latch(c, LYNX__CS_IRQ_TAKE | LYNX__CS_NMI_TAKE);
        set_latch(c, SEQ_IRQ << SEQ_SHIFT);
    }
}

static void apply(lynx__cpu *c, const lynx__cpu_decode *t, uint8_t u, uint8_t data)
{
    uint8_t kind = t[c->opcode].kind;
    switch (u) {
    case U_IMPL: op_implied(c, kind); break;
    case U_IMM:
        c->pc = (uint16_t)(c->pc + 1u);
        op_read(c, kind, data);
        break;
    case U_ARG_LO:
        c->ea = data;
        c->pc = (uint16_t)(c->pc + 1u);
        break;
    case U_ARG_HI:
    case U_ARG_HI_X:
    case U_ARG_HI_Y:
        c->ea = (uint16_t)(c->ea | ((uint32_t)data << 8));
        c->pc = (uint16_t)(c->pc + 1u);
        if (u == U_ARG_HI_X) {
            index_ea(c, c->x);
        } else if (u == U_ARG_HI_Y) {
            index_ea(c, c->y);
        }
        break;
    case U_ZPX: c->ea = (uint8_t)(c->ea + c->x); break;
    case U_ZPY: c->ea = (uint8_t)(c->ea + c->y); break;
    case U_PTR_LO: c->tmp = data; break;
    case U_PTR_HI8:
        c->ea = (uint16_t)(c->tmp | ((uint32_t)data << 8));
        break;
    case U_PTR_HI8_Y:
        c->ea = (uint16_t)(c->tmp | ((uint32_t)data << 8));
        index_ea(c, c->y);
        break;
    case U_PTR_HI16: c->pc = (uint16_t)(c->tmp | ((uint32_t)data << 8)); break;
    case U_X_PM2: c->ea = (uint16_t)(c->ea + c->x); break;
    case U_READ: op_read(c, kind, data); break;
    case U_RMW1: c->tmp = data; break;
    case U_RMW2: c->tmp = op_rmw(c, kind, c->tmp); break;
    case U_BR_TAKEN:
        if (br_cross(c)) {
            c->ea = (uint16_t)((c->ea & 0xFFu) | BR_CROSSED);
        } else {
            c->ea = (uint16_t)((c->ea & 0xFFu) | BR_SAME_PAGE);
            c->pc = br_target(c);
        }
        break;
    case U_BR_CROSS_REL:
    case U_BR_CROSS_ZP: c->pc = br_target(c); break;
    case U_JMP_HI: c->pc = (uint16_t)(c->ea | ((uint32_t)data << 8)); break;
    case U_PUSH:
    case U_PUSH_PCH:
    case U_PUSH_PCL: push_done(c); break;
    case U_PUSH_P_BRK:
    case U_PUSH_P_INT:
        push_done(c);
        set_flag(c, P_I, true);
        set_flag(c, P_D, false);
        break;
    case U_PULL:
        pull_done(c);
        op_pull(c, kind, data);
        break;
    case U_PULL_P:
        pull_done(c);
        c->p = (uint8_t)(data | P_B | P_U);
        break;
    case U_PULL_PCL:
        pull_done(c);
        c->tmp = data;
        break;
    case U_PULL_PCH_PC:
        pull_done(c);
        c->pc = (uint16_t)(c->tmp | ((uint32_t)data << 8));
        break;
    case U_PULL_PCH_EA:
        pull_done(c);
        c->ea = (uint16_t)(c->tmp | ((uint32_t)data << 8));
        break;
    case U_RTS_DUMMY: c->pc = (uint16_t)(c->ea + 1u); break;
    case U_VEC_LO: c->tmp = data; break;
    case U_VEC_HI:
        c->pc = (uint16_t)(c->tmp | ((uint32_t)data << 8));
        if (seq_of(c) == SEQ_RESET) {
            set_flag(c, P_I, true);
            set_flag(c, P_D, false);
        }
        break;
    case U_BRK_OP: c->pc = (uint16_t)(c->pc + 1u); break;
    case U_RST_STK: c->s = (uint8_t)(c->s - 1u); break;
    default: break; /* writes, dummies: nothing to apply */
    }
}

void lynx__cpu_complete(lynx__cpu *c, const lynx__cpu_decode *t, uint8_t data, uint8_t lines)
{
    uint8_t mode;
    uint8_t slot;
    uint8_t next;
    uint8_t seq = seq_of(c);

    if ((lines & LYNX__LINE_NMI) != 0u) {
        if ((c->flags & LYNX__CS_NMI_PREV) == 0u) {
            set_latch(c, LYNX__CS_NMI_PENDING);
        }
        set_latch(c, LYNX__CS_NMI_PREV);
    } else {
        clear_latch(c, LYNX__CS_NMI_PREV);
    }

    if (c->step == 0u && seq == SEQ_NONE) {
        /* The opcode fetch: pick the row, then the first micro-operation. */
        c->opcode = data;
        c->pc = (uint16_t)(c->pc + 1u);
        mode = t[data].mode;
        next = next_slot(c, t, mode, 0u);
        if (next == 0u) {
            if ((t[data].flags & LYNX__F_NO_POLL) == 0u) {
                poll(c, lines);
            }
            finish(c);
        } else {
            c->step = next;
        }
        lynx__cpu_repost(c, t);
        return;
    }

    mode = mode_of(c, t);
    slot = slot_of(c);
    if ((c->req_flags & LYNX__CF_POLL) != 0u) {
        poll(c, lines);
    }
    apply(c, t, lynx__script[mode][slot], data);
    next = next_slot(c, t, mode, slot);
    if (next == 0u) {
        finish(c);
    } else {
        c->step = (uint8_t)(next - (seq == SEQ_NONE ? 0u : 1u));
    }
    lynx__cpu_repost(c, t);
}

/* ---- setup and validation ---------------------------------------------- */

void lynx__cpu_start_at(lynx__cpu *c, const lynx__cpu_decode *t, uint16_t pc)
{
    c->pc = pc;
    c->ea = 0u;
    c->opcode = 0u;
    c->step = 0u;
    c->tmp = 0u;
    c->flags = 0u;
    lynx__cpu_repost(c, t);
}

void lynx__cpu_start_reset(lynx__cpu *c, const lynx__cpu_decode *t)
{
    c->ea = 0u;
    c->opcode = 0u;
    c->step = 0u;
    c->tmp = 0u;
    c->flags = (uint8_t)(SEQ_RESET << SEQ_SHIFT);
    lynx__cpu_repost(c, t);
}

void lynx__cpu_init(lynx__cpu *c, uint32_t profile)
{
    const lynx__cpu_decode *t = lynx__cpu_table(profile);
    c->pc = 0u;
    c->ea = 0u;
    c->a = 0u;
    c->x = 0u;
    c->y = 0u;
    c->s = 0xFDu;
    c->p = 0x34u;
    c->profile = (uint8_t)profile;
    c->opcode = 0u;
    c->step = 0u;
    c->tmp = 0u;
    c->flags = 0u;
    c->req_addr = 0u;
    c->req_data = 0u;
    c->req_kind = 0u;
    c->req_flags = 0u;
    if (t != NULL) {
        lynx__cpu_repost(c, t);
    }
}

bool lynx__cpu_at_boundary(const lynx__cpu *c)
{
    return c->step == 0u && seq_of(c) == SEQ_NONE;
}

/* Highest slot of a mode's script. */
static uint8_t max_slot(uint8_t mode)
{
    uint8_t s = 0u;
    while (s + 1u < SLOTS && lynx__script[mode][s + 1u] != U_END) {
        s++;
    }
    return s;
}

bool lynx__cpu_state_valid(const lynx__cpu *c, const lynx__cpu_decode *t)
{
    uint8_t seq;
    uint8_t mode;
    if (c->profile > 2u || (c->p & 0x30u) != 0x30u || (c->flags & ~0x3Fu) != 0u) {
        return false;
    }
    seq = seq_of(c);
    if (c->step == 0u && (c->opcode != 0u || c->ea != 0u || c->tmp != 0u)) {
        return false;
    }
    if (seq != SEQ_NONE) {
        /* An entry or reset sequence started from a boundary: no opcode held. */
        if (c->opcode != 0u) {
            return false;
        }
        mode = mode_of(c, t);
        return (uint32_t)c->step + 1u <= max_slot(mode);
    }
    if (c->step == 0u) {
        return true;
    }
    if (!lynx__cpu_opcode_defined(c->profile, c->opcode)) {
        return false;
    }
    mode = t[c->opcode].mode;
    if (mode == LYNX__M_ENTRY || mode == LYNX__M_RESETSEQ) {
        return false;
    }
    return c->step <= max_slot(mode);
}
