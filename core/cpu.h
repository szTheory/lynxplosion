/* The CPU: a cycle-stepped 65C02-class core that posts one bus request per
   cycle as data. It never touches memory or time. The caller (the machine loop
   or a test bus) reads the pending request, grants and performs it, then calls
   lynx__cpu_complete with the data of that access; that call is the only state
   transition, one bus access per call.

   Includes only <stdint.h>, <stdbool.h>, <stddef.h> and <string.h> (this header
   needs the first two, cpu.c also <stddef.h> for NULL): the core.cpu_includes
   gate refuses anything else in core/cpu*.

   Source honesty: the instruction set and cycle sequences come from other
   vendors' datasheets, a hardware-informed test and software-model vectors,
   never from a Lynx specification. Every value is unverified for the Lynx
   unless the profile document says otherwise. */
#ifndef LYNX_CPU_H
#define LYNX_CPU_H

#include <stdbool.h>
#include <stdint.h>

/* Cycle kind, chosen by the CPU for each posted request. */
enum lynx__ck {
    LYNX__CK_OPCODE = 0,   /* opcode fetch at PC */
    LYNX__CK_OPERAND,      /* instruction-stream byte after the opcode */
    LYNX__CK_DUMMY_STREAM, /* internal cycle re-reading the stream */
    LYNX__CK_DUMMY,        /* internal cycle at another address */
    LYNX__CK_READ,
    LYNX__CK_WRITE,
    LYNX__CK_RMW_READ,     /* second read of a read-modify-write */
    LYNX__CK_STACK_READ,
    LYNX__CK_STACK_WRITE,
    LYNX__CK_POINTER,      /* indirect pointer byte, also the JMP indirect pointer */
    LYNX__CK_VECTOR,       /* interrupt, BRK and reset vector byte */
    LYNX__CK_COUNT         /* 11 */
};

/* Request flags. */
enum { LYNX__CF_WRITE = 1u, LYNX__CF_POLL = 2u, LYNX__CF_LAST = 4u };

/* Interrupt lines passed to lynx__cpu_complete. */
enum { LYNX__LINE_IRQ = 1u, LYNX__LINE_NMI = 2u };

/* Latch bits of lynx__cpu.flags (also the low byte of the public CPU state
   register). The two bits of LYNX__CS_SEQ_MASK hold the running sequence:
   0 none, 1 IRQ entry, 2 NMI entry, 3 reset. */
enum {
    LYNX__CS_IRQ_TAKE = 1u,
    LYNX__CS_NMI_TAKE = 2u,
    LYNX__CS_NMI_PENDING = 4u,
    LYNX__CS_NMI_PREV = 8u,
    LYNX__CS_SEQ_MASK = 0x30u
};

/* Operation kinds (the "kind" column of a decode row). */
enum {
    LYNX__K_BRK = 0,
    LYNX__K_ORA, LYNX__K_AND, LYNX__K_EOR, LYNX__K_ADC, LYNX__K_SBC,
    LYNX__K_CMP, LYNX__K_CPX, LYNX__K_CPY,
    LYNX__K_LDA, LYNX__K_LDX, LYNX__K_LDY,
    LYNX__K_STA, LYNX__K_STX, LYNX__K_STY, LYNX__K_STZ,
    LYNX__K_BIT, LYNX__K_BITI, LYNX__K_TSB, LYNX__K_TRB,
    LYNX__K_ASL, LYNX__K_LSR, LYNX__K_ROL, LYNX__K_ROR, LYNX__K_INC, LYNX__K_DEC,
    LYNX__K_ASLA, LYNX__K_LSRA, LYNX__K_ROLA, LYNX__K_RORA, LYNX__K_INCA, LYNX__K_DECA,
    LYNX__K_INX, LYNX__K_INY, LYNX__K_DEX, LYNX__K_DEY,
    LYNX__K_TAX, LYNX__K_TAY, LYNX__K_TXA, LYNX__K_TYA, LYNX__K_TSX, LYNX__K_TXS,
    LYNX__K_CLC, LYNX__K_SEC, LYNX__K_CLI, LYNX__K_SEI, LYNX__K_CLD, LYNX__K_SED, LYNX__K_CLV,
    LYNX__K_NOP,
    LYNX__K_PHA, LYNX__K_PHP, LYNX__K_PHX, LYNX__K_PHY,
    LYNX__K_PLA, LYNX__K_PLP, LYNX__K_PLX, LYNX__K_PLY,
    LYNX__K_JMP, LYNX__K_JSR, LYNX__K_RTS, LYNX__K_RTI,
    LYNX__K_BRA, LYNX__K_BPL, LYNX__K_BMI, LYNX__K_BVC, LYNX__K_BVS,
    LYNX__K_BCC, LYNX__K_BCS, LYNX__K_BNE, LYNX__K_BEQ,
    LYNX__K_RMB, LYNX__K_SMB, LYNX__K_BBR, LYNX__K_BBS,
    LYNX__K_COUNT
};

/* Addressing modes (the "mode" column): each selects a bus sequence. The access
   class (read, write, read-modify-write) is part of the mode. M_BRK is the BRK
   sequence; M_ENTRY and M_RESETSEQ are the interrupt entry and reset sequences
   and never appear in a decode row. */
enum {
    LYNX__M_IMPL = 0, LYNX__M_IMM, LYNX__M_NOP1,
    LYNX__M_ZP_R, LYNX__M_ZP_W, LYNX__M_ZP_RMW,
    LYNX__M_ZPX_R, LYNX__M_ZPX_W, LYNX__M_ZPX_RMW,
    LYNX__M_ZPY_R, LYNX__M_ZPY_W,
    LYNX__M_ABS_R, LYNX__M_ABS_W, LYNX__M_ABS_RMW,
    LYNX__M_ABX_R, LYNX__M_ABX_W, LYNX__M_ABX_RMW, LYNX__M_ABX_RMWI,
    LYNX__M_ABY_R, LYNX__M_ABY_W,
    LYNX__M_IZX_R, LYNX__M_IZX_W, LYNX__M_IZY_R, LYNX__M_IZY_W, LYNX__M_IZP_R, LYNX__M_IZP_W,
    LYNX__M_REL, LYNX__M_ZPREL,
    LYNX__M_JMP_ABS, LYNX__M_JMP_IND, LYNX__M_JMP_INDX, LYNX__M_JSR,
    LYNX__M_PUSH, LYNX__M_PULL, LYNX__M_RTS, LYNX__M_RTI, LYNX__M_BRK,
    LYNX__M_NOP_ABS3, LYNX__M_NOP_ABS4,
    LYNX__M_ENTRY, LYNX__M_RESETSEQ,
    LYNX__M_COUNT
};

/* Row flags. The two LYNX__F_UNV_ bits never change execution; tests list the
   unverified expectations from them. */
enum {
    LYNX__F_NO_POLL = 1u,     /* the instruction carries no POLL cycle */
    LYNX__F_DEC_EXTRA = 2u,   /* ADC and SBC: one extra cycle when D is set */
    LYNX__F_BITOP_SLOT = 4u,  /* member of the 32 profile-dependent opcodes */
    LYNX__F_UNV_CYCLES = 8u,  /* cycle sequence not sourced for the Lynx */
    LYNX__F_UNV_SHAPE = 16u   /* length or sequence not sourced for the Lynx */
};

/* Vector policy, read only by the vector runner and the inventory; the CPU
   never reads it. */
enum { LYNX__V_RUN = 0, LYNX__V_TIER_STATE = 1, LYNX__V_EXCL_HW = 2 };

/* One decode row, 4 bytes. */
typedef struct lynx__cpu_decode {
    uint8_t kind, mode, flags, vec;
} lynx__cpu_decode;

/* The CPU instance. Plain data, no pointers, so a whole instance compares with
   memcmp. step is the index of the pending cycle inside the instruction (0 at a
   boundary); at a boundary opcode, ea and tmp are zero. */
typedef struct lynx__cpu {
    uint16_t pc, ea;
    uint8_t a, x, y, s, p; /* p keeps bits 4 and 5 set; interrupt entry pushes it with bit 4 clear */
    uint8_t profile;       /* 0 none, 1 lynx1, 2 lynx2 */
    uint8_t opcode, step, tmp;
    uint8_t flags;         /* LYNX__CS_* */
    uint16_t req_addr;     /* pending request: derived state, recomputed by lynx__cpu_repost */
    uint8_t req_data, req_kind, req_flags;
} lynx__cpu;

/* Both profile halves: [0] the lynx1 rows, [1] the lynx2 rows. */
extern const lynx__cpu_decode lynx__cpu_decode_table[2][256];

/* Profile 0 and 1 give the lynx1 rows, 2 the lynx2 rows, anything above 2 gives
   NULL (the core never passes it). */
const lynx__cpu_decode *lynx__cpu_table(uint32_t profile);

/* Under none: true only when the lynx1 and lynx2 rows are identical. Under the
   Lynx profiles: true for every opcode. */
bool lynx__cpu_opcode_defined(uint32_t profile, uint8_t opcode);

/* Declared power-on state (all unverified): A, X, Y zero, S 0xFD, P 0x34, a
   boundary at pc 0. */
void lynx__cpu_init(lynx__cpu *c, uint32_t profile);

/* Boundary at pc with the latches cleared; posts the opcode fetch. */
void lynx__cpu_start_at(lynx__cpu *c, const lynx__cpu_decode *t, uint16_t pc);

/* Internal seam: begin the 7-cycle reset sequence at the current pc. */
void lynx__cpu_start_reset(lynx__cpu *c, const lynx__cpu_decode *t);

/* Apply the data of the access that was posted, sample the lines, advance and
   post the next request. */
void lynx__cpu_complete(lynx__cpu *c, const lynx__cpu_decode *t, uint8_t data, uint8_t lines);

bool lynx__cpu_at_boundary(const lynx__cpu *c);

/* True for a state that real execution can reach (snapshot restore gate). */
bool lynx__cpu_state_valid(const lynx__cpu *c, const lynx__cpu_decode *t);

/* Recompute the req_* fields from the rest of the state; a pure function. */
void lynx__cpu_repost(lynx__cpu *c, const lynx__cpu_decode *t);

#endif /* LYNX_CPU_H */
