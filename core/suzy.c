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
    SUZY_OPERATION_DIVIDE = 2u,
    SUZY_SIGNED = 0x80u,
    SUZY_ACCUMULATE = 0x40u,
    SUZY_MULTIPLY_TICKS = 44u,
    SUZY_SIGNED_MULTIPLY_TICKS = 54u,
    SUZY_DIVIDE_BASE_TICKS = 176u,
    SUZY_DIVIDE_STEP_TICKS = 14u,
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

static uint32_t read_dword(const uint8_t registers[SUZY_REGISTER_COUNT], uint8_t low_index)
{
    return (uint32_t)registers[low_index] |
           ((uint32_t)registers[(uint8_t)(low_index + 1u)] << 8) |
           ((uint32_t)registers[(uint8_t)(low_index + 2u)] << 16) |
           ((uint32_t)registers[(uint8_t)(low_index + 3u)] << 24);
}

static uint8_t leading_zeroes16(uint16_t value)
{
    uint8_t count = 0u;
    uint16_t mask = 0x8000u;
    while (mask != 0u && (value & mask) == 0u) {
        count++;
        mask = (uint16_t)(mask >> 1);
    }
    return count;
}

static uint64_t divide_ticks(uint16_t divisor)
{
    return (uint64_t)SUZY_DIVIDE_BASE_TICKS +
           (uint64_t)SUZY_DIVIDE_STEP_TICKS * leading_zeroes16(divisor);
}

static bool deadline_fits(uint64_t tick, uint64_t duration)
{
    return duration <= UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM &&
           tick <= UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM - duration;
}

static uint16_t read_word(const uint8_t registers[SUZY_REGISTER_COUNT], uint8_t low_index);

static void convert_signed_high(lynx__suzy_state *suzy, uint8_t low_index,
                                uint8_t *sign_latch)
{
    if ((suzy->mode & SUZY_SIGNED) == 0u) {
        *sign_latch = 0u;
        return;
    }
    const uint16_t value = read_word(suzy->registers, low_index);
    const bool negative = value == 0u || value > 0x8000u;
    *sign_latch = negative ? 1u : 0u;
    if (negative && value != 0u) {
        put_u16(&suzy->registers[low_index], (uint16_t)(0u - value));
    }
}

static bool math_backing_register(uint8_t index)
{
    return (index >= 0x12u && index <= 0x17u) ||
           (index >= 0x20u && index <= 0x23u) ||
           (index >= 0x2Cu && index <= 0x2Fu);
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
    if (suzy == NULL || !register_index(addr, &index)) {
        return false;
    }
    if (suzy->operation != SUZY_OPERATION_IDLE && math_backing_register(index)) {
        return false;
    }

    const bool multiply_command = addr == SUZY_MUL_COMMAND;
    const bool divide_command = addr == SUZY_DIV_COMMAND;
    if (suzy->operation != SUZY_OPERATION_IDLE && (multiply_command || divide_command)) {
        return false;
    }
    const uint64_t duration = multiply_command
        ? (((suzy->mode & (SUZY_SIGNED | SUZY_ACCUMULATE)) != 0u)
               ? SUZY_SIGNED_MULTIPLY_TICKS : SUZY_MULTIPLY_TICKS)
        : (divide_command ? divide_ticks(read_word(suzy->registers, 0x16u)) : 0u);
    if ((multiply_command || divide_command) && !deadline_fits(tick, duration)) return false;

    suzy->registers[index] = value;
    if ((addr & 1u) == 0u && (uint8_t)(index + 1u) < SUZY_REGISTER_COUNT) {
        suzy->registers[(uint8_t)(index + 1u)] = 0u;
    }
    if (addr == SUZY_WARNING_CLEAR) {
        suzy->warning = 0u;
    }
    if (addr == 0xFC53u) {
        convert_signed_high(suzy, SUZY_R_CD_LOW, &suzy->sign_cd);
    }
    if (addr == SUZY_MUL_COMMAND) {
        convert_signed_high(suzy, SUZY_R_AB_LOW, &suzy->sign_ab);
        suzy->operation = SUZY_OPERATION_MULTIPLY;
        suzy->latched_modes = suzy->mode;
        suzy->latched_ab = read_word(suzy->registers, SUZY_R_AB_LOW);
        suzy->latched_cd = read_word(suzy->registers, SUZY_R_CD_LOW);
        suzy->latched_np = 0u;
        suzy->latched_dividend = 0u;
        suzy->latched_accumulator = (suzy->mode & SUZY_ACCUMULATE) != 0u
            ? read_dword(suzy->registers, 0x2Cu) : 0u;
        suzy->latched_sign_ab = suzy->sign_ab;
        suzy->latched_sign_cd = suzy->sign_cd;
        suzy->start_tick = tick;
        suzy->deadline_tick = tick + duration;
    } else if (divide_command) {
        suzy->operation = SUZY_OPERATION_DIVIDE;
        suzy->latched_modes = suzy->mode;
        suzy->latched_ab = 0u;
        suzy->latched_cd = 0u;
        suzy->latched_np = read_word(suzy->registers, 0x16u);
        suzy->latched_dividend = read_dword(suzy->registers, SUZY_R_PRODUCT_LOW);
        suzy->latched_accumulator = 0u;
        suzy->latched_sign_ab = 0u;
        suzy->latched_sign_cd = 0u;
        suzy->start_tick = tick;
        suzy->deadline_tick = tick + duration;
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
    if (suzy == NULL || suzy->operation == SUZY_OPERATION_IDLE ||
        suzy->deadline_tick != tick) {
        return;
    }
    if (suzy->operation == SUZY_OPERATION_MULTIPLY) {
        uint32_t product = (uint32_t)suzy->latched_ab * (uint32_t)suzy->latched_cd;
        const bool negative = (suzy->latched_modes & SUZY_SIGNED) != 0u &&
            ((suzy->latched_sign_ab ^ suzy->latched_sign_cd) != 0u);
        const bool negated = negative && product != 0u;
        if (negated) product = 0u - product;
        put_u32(&suzy->registers[SUZY_R_PRODUCT_LOW], product);
        if ((suzy->latched_modes & SUZY_ACCUMULATE) != 0u) {
            const uint64_t sum = (uint64_t)suzy->latched_accumulator + product;
            put_u32(&suzy->registers[0x2Cu], (uint32_t)sum);
            suzy->carry = sum > UINT32_MAX ? 1u : 0u;
            if (sum > UINT32_MAX) suzy->warning = 1u;
        } else {
            suzy->carry = negated ? 1u : 0u;
        }
    } else if (suzy->operation == SUZY_OPERATION_DIVIDE) {
        const uint32_t quotient = suzy->latched_np == 0u
            ? UINT32_MAX : suzy->latched_dividend / (uint32_t)suzy->latched_np;
        const uint32_t remainder = suzy->latched_np == 0u
            ? 0u : suzy->latched_dividend % (uint32_t)suzy->latched_np;
        put_u32(&suzy->registers[SUZY_R_CD_LOW], quotient);
        put_u32(&suzy->registers[0x2Cu], remainder);
        suzy->carry = suzy->latched_np == 0u || remainder != 0u ? 1u : 0u;
        if (suzy->latched_np == 0u) suzy->warning = 1u;
    } else {
        return;
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
    uint64_t duration = 0u;
    if (suzy->operation == SUZY_OPERATION_MULTIPLY) {
        duration = (suzy->latched_modes & (SUZY_SIGNED | SUZY_ACCUMULATE)) != 0u
            ? SUZY_SIGNED_MULTIPLY_TICKS : SUZY_MULTIPLY_TICKS;
        if (suzy->latched_np != 0u || suzy->latched_dividend != 0u ||
            suzy->latched_ab != read_word(suzy->registers, SUZY_R_AB_LOW) ||
            suzy->latched_cd != read_word(suzy->registers, SUZY_R_CD_LOW) ||
            suzy->latched_sign_ab != suzy->sign_ab ||
            suzy->latched_sign_cd != suzy->sign_cd ||
            (((suzy->latched_modes & SUZY_ACCUMULATE) == 0u) &&
             suzy->latched_accumulator != 0u) ||
            (((suzy->latched_modes & SUZY_ACCUMULATE) != 0u) &&
             suzy->latched_accumulator != read_dword(suzy->registers, 0x2Cu))) {
            return false;
        }
    } else if (suzy->operation == SUZY_OPERATION_DIVIDE) {
        duration = divide_ticks(suzy->latched_np);
        if (suzy->latched_ab != 0u || suzy->latched_cd != 0u ||
            suzy->latched_accumulator != 0u || suzy->latched_sign_ab != 0u ||
            suzy->latched_sign_cd != 0u ||
            suzy->latched_np != read_word(suzy->registers, 0x16u) ||
            suzy->latched_dividend != read_dword(suzy->registers, SUZY_R_PRODUCT_LOW)) {
            return false;
        }
    } else {
        return false;
    }
    if (!deadline_fits(suzy->start_tick, duration) ||
        suzy->deadline_tick != suzy->start_tick + duration ||
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
