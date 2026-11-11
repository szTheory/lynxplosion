/* Instance-owned standalone Mikey timer state. Time and divider phase come
   from the machine's guest_ticks; this module owns no host clock or queue. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "internal.h"

enum {
    TIMER_BACKUP = 0u,
    TIMER_CONTROL_A = 1u,
    TIMER_COUNT = 2u,
    TIMER_CONTROL_B = 3u,
    TIMER_CONTROL_A_COUNT = 0x08u,
    TIMER_CONTROL_A_RELOAD = 0x10u,
    TIMER_CONTROL_A_RESET_DONE = 0x40u,
    TIMER_CONTROL_A_IRQ = 0x80u,
    TIMER_CONTROL_B_DONE = 0x08u,
};

static bool timer_register(uint16_t addr, uint8_t *index, uint8_t *reg)
{
    const uint16_t timer_end = (uint16_t)(0xFD00u + 4u * LYNX__TIMER_IMPLEMENTED_COUNT);
    if (addr < 0xFD00u || addr >= timer_end) {
        return false;
    }
    const uint16_t offset = (uint16_t)(addr - 0xFD00u);
    *index = (uint8_t)(offset / 4u);
    *reg = (uint8_t)(offset % 4u);
    return true;
}

static uint64_t divider_ticks(uint8_t source)
{
    return UINT64_C(16) << source;
}

static bool timer_can_advance(const lynx__timer *timer)
{
    if ((timer->control_a & TIMER_CONTROL_A_COUNT) == 0u ||
        (timer->control_a & 0x07u) == 0x07u) {
        return false;
    }
    return (timer->control_b & TIMER_CONTROL_B_DONE) == 0u ||
           (timer->control_a & (TIMER_CONTROL_A_RELOAD | TIMER_CONTROL_A_RESET_DONE)) != 0u;
}

void lynx__timer_init(lynx__timer_bank *bank)
{
    if (bank == NULL) {
        return;
    }
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        bank->timers[i].backup = 0u;
        bank->timers[i].count = 0u;
        bank->timers[i].control_a = 0u;
        bank->timers[i].control_b = 0u;
    }
    bank->pending = 0u;
    bank->phase_policy = (uint8_t)LYNX__TIMER_PHASE_FIXED_ZERO_1;
    bank->last_event_valid = 0u;
    bank->last_event_tick = 0u;
}

bool lynx__timer_next(const lynx__timer_bank *bank, uint64_t now, uint64_t *tick)
{
    if (bank == NULL || tick == NULL) {
        return false;
    }
    bool found = false;
    uint64_t earliest = 0u;
    for (size_t i = 0u; i < LYNX__TIMER_IMPLEMENTED_COUNT; i++) {
        const lynx__timer *timer = &bank->timers[i];
        if (!timer_can_advance(timer)) {
            continue;
        }
        const uint64_t period = divider_ticks((uint8_t)(timer->control_a & 0x07u));
        const uint64_t phase = now % period;
        uint64_t delta = period - phase;
        if (phase == 0u) {
            if ((bank->last_event_valid != 0u && bank->last_event_tick == now) || now == 0u) {
                delta = period;
            } else {
                delta = 0u;
            }
        }
        if (delta > UINT64_MAX - now) {
            continue;
        }
        const uint64_t candidate = now + delta;
        if (!found || candidate < earliest) {
            found = true;
            earliest = candidate;
        }
    }
    if (found) {
        *tick = earliest;
    }
    return found;
}

void lynx__timer_apply(lynx__timer_bank *bank, uint64_t tick)
{
    if (bank == NULL) {
        return;
    }
    bank->last_event_valid = 1u;
    bank->last_event_tick = tick;
    for (size_t i = 0u; i < LYNX__TIMER_IMPLEMENTED_COUNT; i++) {
        lynx__timer *timer = &bank->timers[i];
        if (!timer_can_advance(timer)) {
            continue;
        }
        const uint8_t source = (uint8_t)(timer->control_a & 0x07u);
        const uint64_t period = divider_ticks(source);
        if (tick % period != 0u) {
            continue;
        }
        const bool reset_done = (timer->control_a & TIMER_CONTROL_A_RESET_DONE) != 0u;
        if (reset_done) {
            timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
        }
        if (timer->count != 0u) {
            timer->count--;
            continue;
        }
        if ((timer->control_a & TIMER_CONTROL_A_IRQ) != 0u && i != 4u) {
            bank->pending = (uint8_t)(bank->pending | (uint8_t)(1u << i));
        }
        if ((timer->control_a & TIMER_CONTROL_A_RELOAD) != 0u) {
            timer->count = timer->backup;
        }
        if (reset_done) {
            timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
        } else {
            timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_DONE);
        }
    }
}

void lynx__timer_mark_tick(lynx__timer_bank *bank, uint64_t tick)
{
    if (bank != NULL) {
        bank->last_event_valid = 1u;
        bank->last_event_tick = tick;
    }
}

bool lynx__timer_available(uint16_t addr)
{
    uint8_t index = 0u;
    uint8_t reg = 0u;
    return timer_register(addr, &index, &reg) || addr == 0xFD80u || addr == 0xFD81u;
}

uint8_t lynx__timer_peek(const lynx__timer_bank *bank, uint16_t addr)
{
    if (bank == NULL) {
        return 0u;
    }
    if (addr == 0xFD80u || addr == 0xFD81u) {
        return bank->pending;
    }
    uint8_t index = 0u;
    uint8_t reg = 0u;
    if (!timer_register(addr, &index, &reg)) {
        return 0u;
    }
    const lynx__timer *timer = &bank->timers[index];
    switch (reg) {
    case TIMER_BACKUP:
        return timer->backup;
    case TIMER_CONTROL_A:
        return timer->control_a;
    case TIMER_COUNT:
        return timer->count;
    case TIMER_CONTROL_B:
        return timer->control_b;
    }
    return 0u;
}

bool lynx__timer_write(lynx__timer_bank *bank, uint16_t addr, uint8_t value)
{
    if (bank == NULL) {
        return false;
    }
    if (addr == 0xFD80u) {
        bank->pending = (uint8_t)(bank->pending & (uint8_t)~value);
        return true;
    }
    if (addr == 0xFD81u) {
        bank->pending = (uint8_t)(bank->pending | value);
        return true;
    }
    uint8_t index = 0u;
    uint8_t reg = 0u;
    if (!timer_register(addr, &index, &reg)) {
        return false;
    }
    lynx__timer *timer = &bank->timers[index];
    switch (reg) {
    case TIMER_BACKUP:
        timer->backup = value;
        return true;
    case TIMER_CONTROL_A:
        if ((value & 0x07u) == 0x07u) {
            return false;
        }
        timer->control_a = value;
        if ((value & TIMER_CONTROL_A_RESET_DONE) != 0u) {
            timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
        }
        return true;
    case TIMER_COUNT:
        timer->count = value;
        return true;
    case TIMER_CONTROL_B:
        if ((value & TIMER_CONTROL_B_DONE) == 0u) {
            timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
        }
        return true;
    }
    return false;
}
