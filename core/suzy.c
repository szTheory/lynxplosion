/* Private instance-owned Suzy math registers and deterministic multiply event. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "internal.h"

enum {
    SUZY_REGISTER_COUNT = 48u,
    SUZY_MUL_COMMAND = 0xFC55u,
    SUZY_DIV_COMMAND = 0xFC63u,
    SUZY_WARNING_CLEAR = 0xFC6Cu,
    SUZY_MODE_MASK = 0xC0u,
    SUZY_OPERATION_IDLE = 0u,
    SUZY_OPERATION_MULTIPLY = 1u,
    SUZY_MULTIPLY_TICKS = 44u,
    SUZY_R_AB_LOW = 0x14u,
    SUZY_R_CD_LOW = 0x12u,
    SUZY_R_PRODUCT_LOW = 0x20u
};

static void put_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static void put_u64(uint8_t *out, uint64_t value)
{
    for (uint32_t i = 0u; i < 8u; i++) {
        out[i] = (uint8_t)(value >> (8u * i));
    }
}

static uint16_t get_u16(const uint8_t *in)
{
    return (uint16_t)((uint16_t)in[0] | (uint16_t)((uint16_t)in[1] << 8));
}

static uint32_t get_u32(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static uint64_t get_u64(const uint8_t *in)
{
    uint64_t value = 0u;
    for (uint32_t i = 0u; i < 8u; i++) {
        value |= (uint64_t)in[i] << (8u * i);
    }
    return value;
}

static bool register_index(uint16_t addr, uint8_t *index)
{
    if ((addr & 0xFF00u) != 0xFC00u || index == NULL) {
        return false;
    }
    const uint8_t offset = (uint8_t)addr;
    if (offset <= 0x2Fu || (offset >= 0x40u && offset <= 0x6Fu)) {
        *index = (uint8_t)(offset & 0x3Fu);
        return true;
    }
    return false;
}

static uint16_t read_word(const uint8_t registers[SUZY_REGISTER_COUNT], uint8_t low_index)
{
    return (uint16_t)((uint16_t)registers[low_index] |
                      (uint16_t)((uint16_t)registers[(uint8_t)(low_index + 1u)] << 8));
}

static void clear_operation(lynx__suzy_state *suzy)
{
    suzy->operation = SUZY_OPERATION_IDLE;
    suzy->latched_modes = 0u;
    suzy->latched_ab = 0u;
    suzy->latched_cd = 0u;
    suzy->latched_np = 0u;
    suzy->latched_dividend = 0u;
    suzy->latched_accumulator = 0u;
    suzy->latched_sign_ab = 0u;
    suzy->latched_sign_cd = 0u;
    suzy->start_tick = 0u;
    suzy->deadline_tick = 0u;
}

void lynx__suzy_init(lynx__suzy_state *suzy)
{
    if (suzy != NULL) {
        memset(suzy, 0, sizeof *suzy);
    }
}

bool lynx__suzy_available(uint16_t addr)
{
    uint8_t index = 0u;
    return register_index(addr, &index);
}

uint8_t lynx__suzy_peek(const lynx__suzy_state *suzy, uint16_t addr)
{
    uint8_t index = 0u;
    if (suzy == NULL || !register_index(addr, &index)) {
        return 0u;
    }
    return suzy->registers[index];
}

bool lynx__suzy_write(lynx__suzy_state *suzy, uint16_t addr, uint8_t value,
                      uint64_t tick)
{
    uint8_t index = 0u;
    if (suzy == NULL || !register_index(addr, &index) ||
        suzy->operation != SUZY_OPERATION_IDLE) {
        return false;
    }
    if (addr == SUZY_DIV_COMMAND) {
        /* Division is implemented in the next scoped arithmetic slice. */
        return false;
    }
    if (addr == SUZY_MUL_COMMAND &&
        ((suzy->mode & SUZY_MODE_MASK) != 0u ||
         tick > UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM - SUZY_MULTIPLY_TICKS)) {
        return false;
    }

    suzy->registers[index] = value;
    if ((addr & 1u) == 0u && (uint8_t)(index + 1u) < SUZY_REGISTER_COUNT) {
        suzy->registers[(uint8_t)(index + 1u)] = 0u;
    }
    if (addr == SUZY_WARNING_CLEAR) {
        suzy->warning = 0u;
    }
    if (addr == SUZY_MUL_COMMAND) {
        suzy->operation = SUZY_OPERATION_MULTIPLY;
        suzy->latched_modes = suzy->mode;
        suzy->latched_ab = read_word(suzy->registers, SUZY_R_AB_LOW);
        suzy->latched_cd = read_word(suzy->registers, SUZY_R_CD_LOW);
        suzy->latched_np = 0u;
        suzy->latched_dividend = 0u;
        suzy->latched_accumulator = 0u;
        suzy->latched_sign_ab = suzy->sign_ab;
        suzy->latched_sign_cd = suzy->sign_cd;
        suzy->start_tick = tick;
        suzy->deadline_tick = tick + SUZY_MULTIPLY_TICKS;
    }
    return true;
}

bool lynx__suzy_next(const lynx__suzy_state *suzy, uint64_t now, uint64_t *tick)
{
    if (suzy == NULL || tick == NULL || suzy->operation == SUZY_OPERATION_IDLE ||
        suzy->deadline_tick < now) {
        return false;
    }
    *tick = suzy->deadline_tick;
    return true;
}

void lynx__suzy_apply(lynx__suzy_state *suzy, uint64_t tick)
{
    if (suzy == NULL || suzy->operation != SUZY_OPERATION_MULTIPLY ||
        suzy->deadline_tick != tick) {
        return;
    }
    const uint32_t product = (uint32_t)suzy->latched_ab * (uint32_t)suzy->latched_cd;
    for (uint32_t i = 0u; i < 4u; i++) {
        suzy->registers[SUZY_R_PRODUCT_LOW + i] = (uint8_t)(product >> (8u * i));
    }
    suzy->unsafe = 1u;
    clear_operation(suzy);
}

static bool operation_fields_zero(const lynx__suzy_state *suzy)
{
    return suzy->operation == SUZY_OPERATION_IDLE && suzy->latched_modes == 0u &&
           suzy->latched_ab == 0u && suzy->latched_cd == 0u && suzy->latched_np == 0u &&
           suzy->latched_dividend == 0u && suzy->latched_accumulator == 0u &&
           suzy->latched_sign_ab == 0u && suzy->latched_sign_cd == 0u &&
           suzy->start_tick == 0u && suzy->deadline_tick == 0u;
}

bool lynx__suzy_valid(const lynx__suzy_state *suzy, uint64_t ticks)
{
    if (suzy == NULL || suzy->sign_ab > 1u || suzy->sign_cd > 1u ||
        (suzy->mode & (uint8_t)~SUZY_MODE_MASK) != 0u || suzy->warning > 1u ||
        suzy->carry > 1u || suzy->unsafe > 1u ||
        (suzy->latched_modes & (uint8_t)~SUZY_MODE_MASK) != 0u) {
        return false;
    }
    if (suzy->operation == SUZY_OPERATION_IDLE) {
        return operation_fields_zero(suzy);
    }
    if (suzy->operation != SUZY_OPERATION_MULTIPLY || suzy->latched_modes != 0u ||
        suzy->latched_np != 0u || suzy->latched_dividend != 0u ||
        suzy->latched_accumulator != 0u || suzy->latched_sign_ab != 0u ||
        suzy->latched_sign_cd != 0u ||
        suzy->latched_ab != read_word(suzy->registers, SUZY_R_AB_LOW) ||
        suzy->latched_cd != read_word(suzy->registers, SUZY_R_CD_LOW) ||
        suzy->start_tick > UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM - SUZY_MULTIPLY_TICKS ||
        suzy->deadline_tick != suzy->start_tick + SUZY_MULTIPLY_TICKS ||
        ticks < suzy->start_tick || ticks > suzy->deadline_tick) {
        return false;
    }
    return true;
}

void lynx__suzy_snapshot_write(uint8_t out[LYNX__SUZY_SNAPSHOT_LEN],
                               const lynx__suzy_state *suzy)
{
    if (out == NULL || suzy == NULL) {
        return;
    }
    memset(out, 0, LYNX__SUZY_SNAPSHOT_LEN);
    memcpy(out, suzy->registers, sizeof suzy->registers);
    out[48] = suzy->sign_ab;
    out[49] = suzy->sign_cd;
    out[50] = suzy->mode;
    out[51] = suzy->warning;
    out[52] = suzy->carry;
    out[53] = suzy->unsafe;
    out[54] = suzy->operation;
    out[55] = suzy->latched_modes;
    put_u16(out + 56, suzy->latched_ab);
    put_u16(out + 58, suzy->latched_cd);
    put_u16(out + 60, suzy->latched_np);
    put_u32(out + 62, suzy->latched_dividend);
    put_u32(out + 66, suzy->latched_accumulator);
    out[70] = suzy->latched_sign_ab;
    out[71] = suzy->latched_sign_cd;
    put_u64(out + 72, suzy->start_tick);
    put_u64(out + 80, suzy->deadline_tick);
}

void lynx__suzy_snapshot_read(lynx__suzy_state *suzy,
                              const uint8_t in[LYNX__SUZY_SNAPSHOT_LEN])
{
    if (suzy == NULL || in == NULL) {
        return;
    }
    memset(suzy, 0, sizeof *suzy);
    memcpy(suzy->registers, in, sizeof suzy->registers);
    suzy->sign_ab = in[48];
    suzy->sign_cd = in[49];
    suzy->mode = in[50];
    suzy->warning = in[51];
    suzy->carry = in[52];
    suzy->unsafe = in[53];
    suzy->operation = in[54];
    suzy->latched_modes = in[55];
    suzy->latched_ab = get_u16(in + 56);
    suzy->latched_cd = get_u16(in + 58);
    suzy->latched_np = get_u16(in + 60);
    suzy->latched_dividend = get_u32(in + 62);
    suzy->latched_accumulator = get_u32(in + 66);
    suzy->latched_sign_ab = in[70];
    suzy->latched_sign_cd = in[71];
    suzy->start_tick = get_u64(in + 72);
    suzy->deadline_tick = get_u64(in + 80);
}
