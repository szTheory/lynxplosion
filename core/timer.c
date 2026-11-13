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
    TIMER_CONTROL_A_SOURCE_MASK = 0x07u,
    TIMER_CONTROL_B_DONE = 0x08u,
    TIMER_CONTROL_B_LAST_CLOCK = 0x04u,
    TIMER_CONTROL_B_BORROW_IN = 0x02u,
    TIMER_CONTROL_B_BORROW_OUT = 0x01u,
    TIMER_CONTROL_B_STATUS_MASK = 0x07u,
    TIMER_SOURCE_LINK = 0x07u,
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

static bool timer_link_target(uint8_t source, uint8_t *target)
{
    switch (source) {
    case 0u:
        *target = 2u;
        return true;
    case 1u:
        *target = 3u;
        return true;
    case 2u:
        *target = 4u;
        return true;
    case 3u:
        *target = 5u;
        return true;
    case 5u:
        *target = 7u;
        return true;
    default:
        return false;
    }
}

static bool timer_link_source(uint8_t target, uint8_t *source)
{
    switch (target) {
    case 1u:
        *source = 7u; /* audio channel 3 closes the timer/audio ring */
        return true;
    case 2u:
        *source = 0u;
        return true;
    case 3u:
        *source = 1u;
        return true;
    case 4u:
        *source = 2u;
        return true;
    case 5u:
        *source = 3u;
        return true;
    case 7u:
        *source = 5u;
        return true;
    default:
        return false;
    }
}

static bool timer_link_mode_supported(uint8_t timer_index)
{
    uint8_t source = 0u;
    return timer_link_source(timer_index, &source);
}

static bool timer_can_advance(const lynx__timer *timer)
{
    return (timer->control_a & TIMER_CONTROL_A_COUNT) != 0u &&
           ((timer->control_b & TIMER_CONTROL_B_DONE) == 0u ||
            (timer->control_a & (TIMER_CONTROL_A_RELOAD | TIMER_CONTROL_A_RESET_DONE)) != 0u);
}

static uint8_t ring_debt_high(const lynx__audio_state *audio, uint8_t node)
{
    return (uint8_t)((audio->ring_debt_high >> ((uint32_t)node * LYNX__AUDIO_RING_HIGH_BITS)) &
                     LYNX__AUDIO_RING_HIGH_MASK);
}

static void ring_debt_set_high(lynx__audio_state *audio, uint8_t node, uint8_t high)
{
    const uint32_t shift = (uint32_t)node * LYNX__AUDIO_RING_HIGH_BITS;
    const uint32_t mask = (uint32_t)LYNX__AUDIO_RING_HIGH_MASK << shift;
    audio->ring_debt_high = (audio->ring_debt_high & ~mask) |
                            ((uint32_t)(high & LYNX__AUDIO_RING_HIGH_MASK) << shift);
}

static bool ring_debt_add(lynx__audio_state *audio, uint8_t node)
{
    uint64_t *low = &audio->ring_debt[node];
    uint8_t high = ring_debt_high(audio, node);
    if (*low == UINT64_MAX) {
        if (high == LYNX__AUDIO_RING_HIGH_MASK) return false;
        *low = 0u;
        high++;
        ring_debt_set_high(audio, node, high);
    } else {
        (*low)++;
    }
    return true;
}

static bool ring_debt_take(lynx__audio_state *audio, uint8_t node)
{
    uint64_t *low = &audio->ring_debt[node];
    uint8_t high = ring_debt_high(audio, node);
    if (*low != 0u) {
        (*low)--;
        return true;
    }
    if (high == 0u) return false;
    *low = UINT64_MAX;
    ring_debt_set_high(audio, node, (uint8_t)(high - 1u));
    return true;
}

static bool ring_node_has_debt(const lynx__audio_state *audio, uint8_t node)
{
    return audio->ring_debt[node] != 0u || ring_debt_high(audio, node) != 0u;
}

static bool ring_has_debt(const lynx__audio_state *audio)
{
    for (uint8_t node = 0u; node < LYNX__AUDIO_RING_NODE_COUNT; node++) {
        if (ring_node_has_debt(audio, node)) return true;
    }
    return false;
}

/* ring-defer-1 makes an overdue suffix eligible only at its serialized
   deadline. Other same-tick device roots or CPU writes can still arrive
   before that next tick and must retain, rather than prematurely drain, it. */
static bool ring_debt_due_at(const lynx__audio_state *audio, uint64_t tick)
{
    return !ring_has_debt(audio) || audio->ring_defer_valid == 0u ||
           audio->ring_defer_tick <= tick;
}

/* The eight ring nodes have at most eight natural roots per master tick
   (T1/T3/T5/T7 and A0-A3) and the CPU can grant at most one bus write per
   master tick. Propagation consumes one debt and can emit at most one
   successor even when reload is disabled, so only those nine roots can grow
   the total. A snapshot at tick T is therefore bounded by 9*(T+1), including
   the initial tick. The total is accumulated as a 72-bit temporary so forged
   per-node maxima cannot overflow the comparison. */
bool lynx__audio_ring_debt_valid(const lynx__audio_state *audio, uint64_t ticks)
{
    if (audio == NULL) return false;
    uint64_t total_low = 0u;
    uint16_t total_high = 0u;
    for (uint8_t node = 0u; node < LYNX__AUDIO_RING_NODE_COUNT; node++) {
        const uint64_t old_low = total_low;
        total_low += audio->ring_debt[node];
        total_high = (uint16_t)(total_high + ring_debt_high(audio, node) +
                                (total_low < old_low ? 1u : 0u));
    }

    /* Compute 9*ticks + 9 in portable C17 as a 68-bit {high,low} pair. */
    uint64_t bound_low = ticks << 3u;
    uint8_t bound_high = (uint8_t)(ticks >> 61u);
    const uint64_t scaled_low = bound_low;
    bound_low += ticks;
    if (bound_low < scaled_low) bound_high++;
    const uint64_t plus_one_low = bound_low;
    bound_low += UINT64_C(9);
    if (bound_low < plus_one_low) bound_high++;
    return total_high < bound_high ||
           (total_high == bound_high && total_low <= bound_low);
}

bool lynx__audio_ring_state_valid(const lynx__audio_state *audio, uint64_t ticks)
{
    if (audio == NULL || audio->ring_defer_valid > 1u ||
        (audio->same_tick_device_cursor & LYNX__AUDIO_RING_RESERVED_MASK) != 0u ||
        (audio->same_tick_device_cursor & LYNX__AUDIO_RING_VISIT_MASK) >
            LYNX__AUDIO_RING_VISIT_LIMIT || !lynx__audio_ring_debt_valid(audio, ticks)) {
        return false;
    }
    const bool debt = ring_has_debt(audio);
    if (debt != (audio->ring_defer_valid != 0u)) return false;
    if (audio->ring_defer_valid == 0u) return audio->ring_defer_tick == 0u;
    const uint8_t visits = (uint8_t)(audio->same_tick_device_cursor &
                                     LYNX__AUDIO_RING_VISIT_MASK);
    if (audio->ring_defer_tick > ticks && visits != LYNX__AUDIO_RING_VISIT_LIMIT) {
        return false;
    }
    return audio->ring_defer_tick >= ticks && ticks != UINT64_MAX &&
           audio->ring_defer_tick - ticks <= 1u;
}

static void ring_clear_deadline(lynx__audio_state *audio)
{
    audio->ring_defer_valid = 0u;
    audio->ring_defer_tick = 0u;
}

static bool ring_begin_tick(lynx__audio_state *audio, uint64_t tick)
{
    if (audio->ring_defer_valid > 1u) return false;
    if (audio->ring_defer_valid != 0u && tick >= audio->ring_defer_tick) {
        ring_clear_deadline(audio);
    }
    return true;
}

static bool ring_find_debt(const lynx__audio_state *audio, uint8_t *node_out)
{
    for (uint8_t offset = 0u; offset < LYNX__AUDIO_RING_NODE_COUNT; offset++) {
        const uint8_t node = (uint8_t)((audio->ring_cursor + offset) %
                                       LYNX__AUDIO_RING_NODE_COUNT);
        if (ring_node_has_debt(audio, node)) {
            *node_out = node;
            return true;
        }
    }
    return false;
}

static uint8_t timer_ring_node(uint8_t timer_index)
{
    switch (timer_index) {
    case 1u: return 0u;
    case 3u: return 1u;
    case 5u: return 2u;
    case 7u: return 3u;
    default: return UINT8_MAX;
    }
}

static uint8_t timer_ring_index(uint8_t node)
{
    static const uint8_t indexes[4] = {1u, 3u, 5u, 7u};
    return node < 4u ? indexes[node] : UINT8_MAX;
}

static bool ring_node_is_linked(const lynx__timer_bank *bank,
                                const lynx__audio_state *audio, uint8_t node)
{
    if (node < 4u) {
        const uint8_t timer_index = timer_ring_index(node);
        return (bank->timers[timer_index].control_a & TIMER_CONTROL_A_SOURCE_MASK) ==
               TIMER_SOURCE_LINK;
    }
    const lynx__audio_channel *channel = &audio->channels[node - 4u];
    return (channel->control_a & 0x07u) == TIMER_SOURCE_LINK;
}

static bool ring_queue_successor(lynx__timer_bank *bank, lynx__audio_state *audio,
                                 uint8_t source_node)
{
    const uint8_t target = (uint8_t)((source_node + 1u) % LYNX__AUDIO_RING_NODE_COUNT);
    return !ring_node_is_linked(bank, audio, target) || ring_debt_add(audio, target);
}

static bool audio_channel_can_advance(const lynx__audio_channel *channel)
{
    return (channel->control_a & 0x08u) != 0u &&
           ((channel->control_b & 0x08u) == 0u ||
            (channel->control_a & 0x50u) != 0u);
}

static bool timer_ring_clock(lynx__timer_bank *bank, uint8_t timer_index,
                             bool forced, bool incoming_link, bool expose_status,
                             bool *borrowed_out, uint8_t *borrow_mask)
{
    lynx__timer *timer = &bank->timers[timer_index];
    if (borrowed_out != NULL) *borrowed_out = false;
    if (!forced && !timer_can_advance(timer)) return true;
    if (expose_status) {
        timer->control_b = (uint8_t)(timer->control_b &
                                     (uint8_t)~TIMER_CONTROL_B_STATUS_MASK);
        timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_LAST_CLOCK |
                                     (incoming_link ? TIMER_CONTROL_B_BORROW_IN : 0u));
    }
    if (timer->count != 0u) {
        timer->count--;
        return true;
    }

    if (borrowed_out != NULL) *borrowed_out = true;
    if (borrow_mask != NULL) {
        *borrow_mask = (uint8_t)(*borrow_mask | (uint8_t)(1u << timer_index));
    }
    if (expose_status) {
        timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_BORROW_OUT);
    }
    if ((timer->control_a & TIMER_CONTROL_A_IRQ) != 0u && timer_index != 4u) {
        bank->pending = (uint8_t)(bank->pending | (uint8_t)(1u << timer_index));
    }
    if ((timer->control_a & TIMER_CONTROL_A_RELOAD) != 0u) {
        timer->count = timer->backup;
    }
    if ((timer->control_a & TIMER_CONTROL_A_RESET_DONE) != 0u) {
        timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
    } else {
        timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_DONE);
    }
    return true;
}

static bool ring_node_can_advance(const lynx__timer_bank *bank,
                                  const lynx__audio_state *audio, uint8_t node)
{
    if (node < 4u) return timer_can_advance(&bank->timers[timer_ring_index(node)]);
    return audio_channel_can_advance(&audio->channels[node - 4u]);
}

static bool ring_clock_one(lynx__timer_bank *bank, lynx__audio_state *audio,
                           uint8_t node, uint64_t tick,
                           lynx__audio_event_sink *sink, uint8_t *borrow_mask)
{
    bool borrowed = false;
    if (node < 4u) {
        if (!timer_ring_clock(bank, timer_ring_index(node), false, true, true,
                              &borrowed, borrow_mask)) return false;
    } else {
        (void)lynx__audio_clock_root(audio, (uint8_t)(node - 4u), tick, true,
                                     sink, &borrowed);
    }
    return !borrowed || ring_queue_successor(bank, audio, node);
}

static bool ring_drain(lynx__timer_bank *bank, lynx__audio_state *audio,
                       uint64_t tick, lynx__audio_event_sink *sink,
                       uint8_t *borrow_mask)
{
    uint8_t visits = (uint8_t)(audio->same_tick_device_cursor &
                               LYNX__AUDIO_RING_VISIT_MASK);
    while (visits < LYNX__AUDIO_RING_VISIT_LIMIT) {
        uint8_t node = 0u;
        if (!ring_find_debt(audio, &node)) break;
        if (ring_node_can_advance(bank, audio, node)) {
            if (!ring_debt_take(audio, node) ||
                !ring_clock_one(bank, audio, node, tick, sink, borrow_mask)) {
                return false;
            }
        }
        audio->ring_cursor = (uint8_t)((node + 1u) % LYNX__AUDIO_RING_NODE_COUNT);
        visits++;
        audio->same_tick_device_cursor = (uint8_t)((audio->same_tick_device_cursor &
                                                     (uint8_t)~LYNX__AUDIO_RING_VISIT_MASK) |
                                                    visits);
    }
    if (ring_has_debt(audio)) {
        if (audio->ring_defer_valid == 0u || audio->ring_defer_tick <= tick) {
            if (tick == UINT64_MAX) return false;
            audio->ring_defer_valid = 1u;
            audio->ring_defer_tick = tick + 1u;
        }
    } else {
        ring_clear_deadline(audio);
    }
    return true;
}

/* Legacy standalone timer prefix. The production machine uses the shared
   timer/audio ring below; this helper preserves the separate 0→2→4 path. */
static uint8_t lynx__timer_borrow(lynx__timer_bank *bank,
                                  bool incoming[LYNX__TIMER_COUNT],
                                  uint8_t forced_index, bool expose_status)
{
    uint8_t borrow_mask = 0u;
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        if (!incoming[i]) {
            continue;
        }
        lynx__timer *timer = &bank->timers[i];
        const bool forced = forced_index == (uint8_t)i;
        const bool linked = !forced &&
                            (timer->control_a & TIMER_CONTROL_A_SOURCE_MASK) == TIMER_SOURCE_LINK;
        if (!forced && !timer_can_advance(timer)) {
            continue;
        }
        if (linked && !timer_link_mode_supported((uint8_t)i)) {
            continue;
        }

        if (expose_status) {
            timer->control_b = (uint8_t)(timer->control_b &
                                         (uint8_t)~TIMER_CONTROL_B_STATUS_MASK);
            timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_LAST_CLOCK |
                                         (linked ? TIMER_CONTROL_B_BORROW_IN : 0u));
        }
        if (timer->count != 0u) {
            timer->count--;
            continue;
        }

        borrow_mask = (uint8_t)(borrow_mask | (uint8_t)(1u << i));

        if (expose_status) {
            timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_BORROW_OUT);
        }
        if ((timer->control_a & TIMER_CONTROL_A_IRQ) != 0u && i != 4u) {
            bank->pending = (uint8_t)(bank->pending | (uint8_t)(1u << i));
        }

        const bool reload = (timer->control_a & TIMER_CONTROL_A_RELOAD) != 0u;
        const bool reset_done = (timer->control_a & TIMER_CONTROL_A_RESET_DONE) != 0u;
        if (reload) {
            timer->count = timer->backup;
        }
        if (reset_done) {
            timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
        } else {
            timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_DONE);
        }

        /* Underflow emits at most one linked pulse, independent of reload;
           reload changes only the source counter. */
        uint8_t target = 0u;
        if (timer_link_target((uint8_t)i, &target) &&
            (bank->timers[target].control_a & TIMER_CONTROL_A_SOURCE_MASK) ==
                TIMER_SOURCE_LINK) {
            incoming[target] = true;
        }
    }
    return borrow_mask;
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
        const uint8_t source = (uint8_t)(timer->control_a & TIMER_CONTROL_A_SOURCE_MASK);
        if (source == TIMER_SOURCE_LINK) {
            continue;
        }
        const uint64_t period = divider_ticks(source);
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

uint8_t lynx__timer_apply(lynx__timer_bank *bank, uint64_t tick)
{
    if (bank == NULL) {
        return 0u;
    }
    bank->last_event_valid = 1u;
    bank->last_event_tick = tick;
    bool incoming[LYNX__TIMER_COUNT] = {false};
    for (size_t i = 0u; i < LYNX__TIMER_IMPLEMENTED_COUNT; i++) {
        lynx__timer *timer = &bank->timers[i];
        timer->control_b = (uint8_t)(timer->control_b &
                                     (uint8_t)~TIMER_CONTROL_B_STATUS_MASK);
        const uint8_t source = (uint8_t)(timer->control_a & TIMER_CONTROL_A_SOURCE_MASK);
        if (!timer_can_advance(timer) || source == TIMER_SOURCE_LINK) {
            continue;
        }
        const uint64_t period = divider_ticks(source);
        if (tick % period != 0u) {
            continue;
        }
        incoming[i] = true;
    }
    return lynx__timer_borrow(bank, incoming, UINT8_MAX, true);
}

static uint8_t timer_apply_nonring_roots(lynx__timer_bank *bank, uint64_t tick)
{
    bool incoming[LYNX__TIMER_COUNT] = {false};
    for (uint8_t i = 0u; i < LYNX__TIMER_IMPLEMENTED_COUNT; i++) {
        if (timer_ring_node(i) != UINT8_MAX) continue;
        lynx__timer *timer = &bank->timers[i];
        const uint8_t source = (uint8_t)(timer->control_a & TIMER_CONTROL_A_SOURCE_MASK);
        if (!timer_can_advance(timer) || source == TIMER_SOURCE_LINK) continue;
        if (tick % divider_ticks(source) == 0u) incoming[i] = true;
    }
    return lynx__timer_borrow(bank, incoming, UINT8_MAX, true);
}

static bool timer_ring_root(lynx__timer_bank *bank, lynx__audio_state *audio,
                            uint8_t node, uint64_t tick, uint8_t *borrow_mask)
{
    const uint8_t index = timer_ring_index(node);
    lynx__timer *timer = &bank->timers[index];
    const uint8_t source = (uint8_t)(timer->control_a & TIMER_CONTROL_A_SOURCE_MASK);
    if (source == TIMER_SOURCE_LINK || !timer_can_advance(timer) ||
        tick % divider_ticks(source) != 0u) return true;
    bool borrowed = false;
    if (!timer_ring_clock(bank, index, false, false, true, &borrowed, borrow_mask)) {
        return false;
    }
    return !borrowed || ring_queue_successor(bank, audio, node);
}

static bool audio_ring_root(lynx__timer_bank *bank, lynx__audio_state *audio,
                            uint8_t channel_index, uint64_t tick,
                            lynx__audio_event_sink *sink)
{
    lynx__audio_channel *channel = &audio->channels[channel_index];
    const uint8_t source = (uint8_t)(channel->control_a & 0x07u);
    if (source == TIMER_SOURCE_LINK || !audio_channel_can_advance(channel)) return true;
    const uint64_t period = UINT64_C(16) << source;
    if (tick % period != 0u) return true;
    bool borrowed = false;
    (void)lynx__audio_clock_root(audio, channel_index, tick, false, sink, &borrowed);
    return !borrowed || ring_queue_successor(bank, audio,
                                              (uint8_t)(4u + channel_index));
}

static lynx__work_status work_status_for_candidate(lynx__timer_bank *bank,
                                                   lynx__audio_state *audio,
                                                   uint64_t tick, uint64_t current_tick,
                                                   bool timer_due,
                                                   bool audio_due,
                                                   lynx__audio_event_sink *sink,
                                                   uint8_t *borrow_mask)
{
    if (!lynx__audio_ring_state_valid(audio, current_tick)) {
        return LYNX__WORK_CAPACITY;
    }
    lynx__timer_bank next_bank = *bank;
    lynx__audio_state next_audio = *audio;
    uint8_t borrowed = 0u;
    lynx_audio_event staged_events[LYNX__AUDIO_MAX_EVENTS_PER_TICK];
    lynx__audio_event_sink staged_sink = {
        staged_events, LYNX__AUDIO_MAX_EVENTS_PER_TICK, 0u
    };
    lynx__audio_event_sink *work_sink = sink != NULL ? &staged_sink : NULL;

    if (tick > current_tick) {
        next_audio.same_tick_device_cursor = 0u;
        next_audio.same_tick_sample_cursor = 0u;
        if (next_audio.event_cursor_valid != 0u && tick > next_audio.event_tick) {
            next_audio.event_cursor_valid = 0u;
            next_audio.event_sequence = 0u;
            next_audio.event_tick = 0u;
        }
        if (next_bank.last_event_valid != 0u && tick > next_bank.last_event_tick) {
            next_bank.last_event_valid = 0u;
            next_bank.last_event_tick = 0u;
        }
    }

    if (timer_due) {
        next_bank.last_event_valid = 1u;
        next_bank.last_event_tick = tick;
        for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
            next_bank.timers[i].control_b = (uint8_t)(next_bank.timers[i].control_b &
                                                       (uint8_t)~TIMER_CONTROL_B_STATUS_MASK);
        }
    }
    if (audio_due) {
        next_audio.same_tick_device_cursor = (uint8_t)(next_audio.same_tick_device_cursor |
                                                       LYNX__AUDIO_DEVICE_APPLIED);
    }

    if (!ring_begin_tick(&next_audio, tick)) return LYNX__WORK_CAPACITY;

    /* Old deferred work gets the first eight-visit slice. If any remains,
       newer roots are still recorded but wait behind it for the next tick. */
    if (ring_debt_due_at(&next_audio, tick) &&
        !ring_drain(&next_bank, &next_audio, tick, work_sink, &borrowed)) {
        return LYNX__WORK_CAPACITY;
    }
    const bool old_debt_remains = ring_has_debt(&next_audio);

    if (timer_due) {
        borrowed = (uint8_t)(borrowed | timer_apply_nonring_roots(&next_bank, tick));
        for (uint8_t node = 0u; node < 4u; node++) {
            if (!timer_ring_root(&next_bank, &next_audio, node, tick, &borrowed)) {
                return LYNX__WORK_CAPACITY;
            }
        }
    }
    if (audio_due) {
        for (uint8_t channel = 0u; channel < LYNX__AUDIO_CHANNEL_COUNT; channel++) {
            if (!audio_ring_root(&next_bank, &next_audio, channel, tick, work_sink)) {
                return LYNX__WORK_CAPACITY;
            }
        }
    }

    if (!old_debt_remains &&
        !ring_drain(&next_bank, &next_audio, tick, work_sink, &borrowed)) {
        return LYNX__WORK_CAPACITY;
    }

    if (!lynx__audio_ring_state_valid(&next_audio, tick)) return LYNX__WORK_CAPACITY;

    if (sink != NULL) {
        if (sink->count > sink->capacity || staged_sink.count > sink->capacity - sink->count ||
            (staged_sink.count != 0u && sink->events == NULL)) {
            return LYNX__WORK_OUTPUT;
        }
        for (size_t i = 0u; i < staged_sink.count; i++) {
            sink->events[sink->count + i] = staged_events[i];
        }
        sink->count += staged_sink.count;
    }
    *bank = next_bank;
    *audio = next_audio;
    if (borrow_mask != NULL) *borrow_mask = borrowed;
    return LYNX__WORK_OK;
}

bool lynx__timer_audio_next(const lynx__audio_state *audio, uint64_t now,
                            uint64_t *tick)
{
    if (audio == NULL || tick == NULL || !ring_has_debt(audio)) {
        return false;
    }
    if (audio->ring_defer_valid != 0u) {
        *tick = audio->ring_defer_tick > now ? audio->ring_defer_tick : now;
    } else {
        if (now == UINT64_MAX) return false;
        *tick = now + 1u;
    }
    return true;
}

lynx__work_status lynx__timer_audio_apply(lynx__timer_bank *bank,
                                           lynx__audio_state *audio, uint64_t tick,
                                           uint64_t current_tick,
                                           bool timer_due, bool audio_due,
                                           lynx__audio_event_sink *sink,
                                           uint8_t *borrow_mask)
{
    if (bank == NULL || audio == NULL) return LYNX__WORK_CAPACITY;
    return work_status_for_candidate(bank, audio, tick, current_tick, timer_due, audio_due,
                                     sink, borrow_mask);
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

static bool timer_write_raw(lynx__timer_bank *bank, uint16_t addr, uint8_t value,
                            bool ring_root, bool *borrowed_out)
{
    if (borrowed_out != NULL) *borrowed_out = false;
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
        if ((value & TIMER_CONTROL_A_SOURCE_MASK) == TIMER_SOURCE_LINK &&
            (value & TIMER_CONTROL_A_COUNT) != 0u &&
            !timer_link_mode_supported(index)) {
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
        timer->control_b = (uint8_t)(timer->control_b &
                                     (uint8_t)~TIMER_CONTROL_B_STATUS_MASK);
        if ((value & TIMER_CONTROL_B_DONE) != 0u) {
            timer->control_b = (uint8_t)(timer->control_b | TIMER_CONTROL_B_DONE);
        } else {
            timer->control_b = (uint8_t)(timer->control_b & (uint8_t)~TIMER_CONTROL_B_DONE);
        }
        if ((value & TIMER_CONTROL_B_BORROW_IN) != 0u) {
            if (ring_root && timer_ring_node(index) != UINT8_MAX) {
                if (!timer_ring_clock(bank, index, true, false, false,
                                      borrowed_out, NULL)) return false;
            } else {
                bool incoming[LYNX__TIMER_COUNT] = {false};
                incoming[index] = true;
                const uint8_t mask = lynx__timer_borrow(bank, incoming, index, false);
                if (borrowed_out != NULL) {
                    *borrowed_out = (mask & (uint8_t)(1u << index)) != 0u;
                }
            }
        }
        return true;
    }
    return false;
}

bool lynx__timer_write(lynx__timer_bank *bank, uint16_t addr, uint8_t value)
{
    return timer_write_raw(bank, addr, value, false, NULL);
}

lynx__work_status lynx__timer_audio_write(lynx__timer_bank *bank,
                                           lynx__audio_state *audio, uint16_t addr,
                                           uint8_t value, uint64_t tick,
                                           lynx__audio_event_sink *sink)
{
    if (bank == NULL || audio == NULL) return LYNX__WORK_CAPACITY;
    if (!lynx__audio_ring_state_valid(audio, tick)) {
        return LYNX__WORK_CAPACITY;
    }
    lynx__timer_bank next_bank = *bank;
    lynx__audio_state next_audio = *audio;
    lynx_audio_event staged_events[LYNX__AUDIO_MAX_EVENTS_PER_TICK];
    lynx__audio_event_sink staged_sink = {
        staged_events, LYNX__AUDIO_MAX_EVENTS_PER_TICK, 0u
    };
    lynx__audio_event_sink *work_sink = sink != NULL ? &staged_sink : NULL;

    if (!ring_begin_tick(&next_audio, tick)) return LYNX__WORK_CAPACITY;
    if (ring_debt_due_at(&next_audio, tick) &&
        !ring_drain(&next_bank, &next_audio, tick, work_sink, NULL)) {
        return LYNX__WORK_CAPACITY;
    }
    const bool old_debt_remains = ring_has_debt(&next_audio);

    if ((addr >= 0xFD20u && addr <= 0xFD3Fu) || addr == 0xFD50u) {
        const uint8_t channel = addr == 0xFD50u ? UINT8_MAX :
                                (uint8_t)((addr - 0xFD20u) / 8u);
        const uint8_t reg = addr == 0xFD50u ? UINT8_MAX :
                            (uint8_t)((addr - 0xFD20u) % 8u);
        if (!lynx__audio_write(&next_audio, addr, value, tick, work_sink)) {
            return LYNX__WORK_CAPACITY;
        }
        if (channel != UINT8_MAX && reg == 7u && (value & 0x02u) != 0u &&
            (next_audio.channels[channel].control_b & 0x01u) != 0u &&
            !ring_queue_successor(&next_bank, &next_audio, (uint8_t)(4u + channel))) {
            return LYNX__WORK_CAPACITY;
        }
    } else {
        bool borrowed = false;
        if (!timer_write_raw(&next_bank, addr, value, true, &borrowed)) {
            return LYNX__WORK_CAPACITY;
        }
        uint8_t index = 0u;
        uint8_t reg = 0u;
        if (borrowed && timer_register(addr, &index, &reg) &&
            reg == TIMER_CONTROL_B && timer_ring_node(index) != UINT8_MAX &&
            !ring_queue_successor(&next_bank, &next_audio, timer_ring_node(index))) {
            return LYNX__WORK_CAPACITY;
        }
    }

    if (!old_debt_remains &&
        !ring_drain(&next_bank, &next_audio, tick, work_sink, NULL)) {
        return LYNX__WORK_CAPACITY;
    }

    if (!lynx__audio_ring_state_valid(&next_audio, tick)) return LYNX__WORK_CAPACITY;

    if (sink != NULL) {
        if (sink->count > sink->capacity || staged_sink.count > sink->capacity - sink->count ||
            (staged_sink.count != 0u && sink->events == NULL)) {
            return LYNX__WORK_OUTPUT;
        }
        for (size_t i = 0u; i < staged_sink.count; i++) {
            sink->events[sink->count + i] = staged_events[i];
        }
        sink->count += staged_sink.count;
    }
    *bank = next_bank;
    *audio = next_audio;
    return LYNX__WORK_OK;
}
