/* Instance-owned digital audio registers and fixed-rate PCM observations.
   Divider phase, stereo routing and mix scale are explicit software policy. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "internal.h"

enum {
    AUDIO_VOLUME = 0u,
    AUDIO_FEEDBACK = 1u,
    AUDIO_OUTPUT = 2u,
    AUDIO_SHIFT_LOW = 3u,
    AUDIO_BACKUP = 4u,
    AUDIO_CONTROL_A = 5u,
    AUDIO_COUNT = 6u,
    AUDIO_CONTROL_B = 7u,
    AUDIO_CONTROL_A_INTEGRATE = 0x20u,
    AUDIO_CONTROL_A_RELOAD = 0x10u,
    AUDIO_CONTROL_A_COUNT = 0x08u,
    AUDIO_CONTROL_A_RESET_DONE = 0x40u,
    AUDIO_CONTROL_A_LINK = 0x07u,
    AUDIO_CONTROL_B_DONE = 0x08u,
    AUDIO_CONTROL_B_LAST_CLOCK = 0x04u,
    AUDIO_CONTROL_B_BORROW_IN = 0x02u,
    AUDIO_CONTROL_B_BORROW_OUT = 0x01u,
    AUDIO_CONTROL_B_STATUS = 0x07u,
    AUDIO_EVENT_WRITE = LYNX_AUDIO_EVENT_WRITE,
    AUDIO_EVENT_CLOCK = LYNX_AUDIO_EVENT_CLOCK,
    AUDIO_STEREO_DISABLE_BASE = 0x10u,
    AUDIO_TICKS_PER_MICROSECOND = 16u,
    AUDIO_SAMPLE_TICKS = 400u
};

static bool audio_channel_address(uint16_t addr, uint8_t *channel, uint8_t *reg)
{
    if (addr < 0xFD20u || addr > 0xFD3Fu) return false;
    const uint16_t offset = (uint16_t)(addr - 0xFD20u);
    *channel = (uint8_t)(offset / 8u);
    *reg = (uint8_t)(offset % 8u);
    return true;
}

static int8_t clamp_output(int16_t value)
{
    if (value > INT8_MAX) return INT8_MAX;
    if (value < INT8_MIN) return INT8_MIN;
    return (int8_t)value;
}

/* Convert the register's two's-complement byte without an out-of-range cast
   from uint8_t to int8_t. */
static int8_t signed_register_byte(uint8_t value)
{
    const int16_t decoded = value <= (uint8_t)INT8_MAX
                                ? (int16_t)value
                                : (int16_t)value - INT16_C(256);
    return (int8_t)decoded;
}

static void audio_emit(lynx__audio_state *audio, uint64_t tick, uint8_t channel,
                       uint8_t cause, int8_t before, int8_t after,
                       lynx__audio_event_sink *sink)
{
    if (audio->event_cursor_valid == 0u || audio->event_tick != tick) {
        audio->event_cursor_valid = 1u;
        audio->event_tick = tick;
        audio->event_sequence = 0u;
    }
    if (sink != NULL && sink->count < sink->capacity) {
        lynx_audio_event *event = &sink->events[sink->count++];
        event->tick = tick;
        event->sequence = audio->event_sequence;
        event->channel = channel;
        event->cause = cause;
        event->before_output = before;
        event->after_output = after;
    }
    audio->event_sequence++;
}

static int16_t channel_volume(const lynx__audio_channel *channel)
{
    return (int16_t)signed_register_byte(channel->volume);
}

static void channel_update_output(lynx__audio_channel *channel)
{
    if ((channel->control_a & AUDIO_CONTROL_A_INTEGRATE) != 0u) {
        channel->output = channel->integrator;
        return;
    }
    const int16_t volume = channel_volume(channel);
    channel->output = clamp_output((channel->shift & 1u) != 0u ? volume : (int16_t)-volume);
}

static uint8_t lfsr_feedback(const lynx__audio_channel *channel)
{
    uint16_t taps = (uint16_t)(channel->feedback & 0x3Fu);
    if ((channel->feedback & 0x40u) != 0u) taps = (uint16_t)(taps | (uint16_t)(1u << 10));
    if ((channel->feedback & 0x80u) != 0u) taps = (uint16_t)(taps | (uint16_t)(1u << 11));
    if ((channel->control_a & 0x80u) != 0u) taps = (uint16_t)(taps | (uint16_t)(1u << 7));
    /* Appendix 3 names taps 11, 10, 7, 5, 4, 3, 2, 1 and 0. The lower
       feedback byte carries 0..5, 10 and 11; FD25.B7 supplies tap 7. */
    uint8_t parity = 0u;
    for (uint8_t bit = 0u; bit < 12u; bit++) {
        if ((taps & (uint16_t)(1u << bit)) != 0u &&
            (channel->shift & (uint16_t)(1u << bit)) != 0u) {
            parity ^= 1u;
        }
    }
    return (uint8_t)(parity ^ 1u);
}

static void channel_clock(lynx__audio_channel *channel)
{
    const int16_t volume = channel_volume(channel);
    const uint8_t output_bit = lfsr_feedback(channel);
    channel->shift = (uint16_t)((((uint32_t)channel->shift << 1) |
                                 (uint32_t)output_bit) & UINT32_C(0x0FFF));
    if ((channel->control_a & AUDIO_CONTROL_A_INTEGRATE) != 0u) {
        const int16_t delta = output_bit != 0u ? volume : (int16_t)-volume;
        channel->integrator = clamp_output((int16_t)channel->integrator + delta);
        channel->output = channel->integrator;
    } else {
        channel_update_output(channel);
    }
}

static bool channel_can_advance(const lynx__audio_channel *channel)
{
    return (channel->control_a & AUDIO_CONTROL_A_COUNT) != 0u &&
           ((channel->control_b & AUDIO_CONTROL_B_DONE) == 0u ||
            (channel->control_a & (AUDIO_CONTROL_A_RELOAD | AUDIO_CONTROL_A_RESET_DONE)) != 0u);
}

static bool channel_clock_at(const lynx__audio_channel *channel, uint64_t tick)
{
    const uint8_t source = (uint8_t)(channel->control_a & 0x07u);
    if (source == AUDIO_CONTROL_A_LINK) return false;
    const uint64_t period = (uint64_t)AUDIO_TICKS_PER_MICROSECOND << source;
    return tick % period == 0u;
}

static bool audio_clock_channel(lynx__audio_state *audio, uint8_t index,
                                uint64_t tick, uint8_t cause, bool incoming_link,
                                bool expose_status, lynx__audio_event_sink *sink,
                                bool *borrowed_out)
{
    lynx__audio_channel *channel = &audio->channels[index];
    const int8_t before = channel->output;
    if (borrowed_out != NULL) *borrowed_out = false;
    if (expose_status) {
        channel->control_b = (uint8_t)(channel->control_b & (uint8_t)~AUDIO_CONTROL_B_STATUS);
        channel->control_b = (uint8_t)(channel->control_b | AUDIO_CONTROL_B_LAST_CLOCK |
                                       (incoming_link ? AUDIO_CONTROL_B_BORROW_IN : 0u));
    }
    if (channel->count != 0u) {
        channel->count--;
        return false;
    }

    if (borrowed_out != NULL) *borrowed_out = true;
    if (expose_status) {
        channel->control_b = (uint8_t)(channel->control_b | AUDIO_CONTROL_B_BORROW_OUT);
    }
    if ((channel->control_a & AUDIO_CONTROL_A_RELOAD) != 0u) channel->count = channel->backup;
    if ((channel->control_a & AUDIO_CONTROL_A_RESET_DONE) != 0u) {
        channel->control_b = (uint8_t)(channel->control_b & (uint8_t)~AUDIO_CONTROL_B_DONE);
    } else {
        channel->control_b = (uint8_t)(channel->control_b | AUDIO_CONTROL_B_DONE);
    }
    channel_clock(channel);
    if (channel->output != before) {
        audio_emit(audio, tick, index, cause, before, channel->output, sink);
        return true;
    }
    return false;
}

bool lynx__audio_clock_root(lynx__audio_state *audio, uint8_t channel,
                            uint64_t tick, bool incoming_link,
                            lynx__audio_event_sink *sink, bool *borrowed_out)
{
    if (audio == NULL || channel >= LYNX__AUDIO_CHANNEL_COUNT) {
        if (borrowed_out != NULL) *borrowed_out = false;
        return false;
    }
    return audio_clock_channel(audio, channel, tick, AUDIO_EVENT_CLOCK,
                               incoming_link, true, sink, borrowed_out);
}

void lynx__audio_init(lynx__audio_state *audio)
{
    if (audio == NULL) return;
    memset(audio, 0, sizeof *audio);
    audio->next_sample_tick = AUDIO_SAMPLE_TICKS;
}

bool lynx__audio_next(const lynx__audio_state *audio, uint64_t now, uint64_t *tick)
{
    if (audio == NULL || tick == NULL) return false;
    bool found = false;
    uint64_t earliest = 0u;
    for (uint8_t i = 0u; i < LYNX__AUDIO_CHANNEL_COUNT; i++) {
        const lynx__audio_channel *channel = &audio->channels[i];
        if (!channel_can_advance(channel)) continue;
        const uint8_t source = (uint8_t)(channel->control_a & 0x07u);
        if (source == AUDIO_CONTROL_A_LINK) continue;
        const uint64_t period = (uint64_t)AUDIO_TICKS_PER_MICROSECOND << source;
        const uint64_t phase = now % period;
        uint64_t delta = period - phase;
        if (phase == 0u) {
            if ((audio->same_tick_device_cursor & LYNX__AUDIO_DEVICE_APPLIED) != 0u ||
                now == 0u) {
                delta = period;
            } else {
                delta = 0u;
            }
        }
        if (delta > UINT64_MAX - now) continue;
        const uint64_t candidate = now + delta;
        if (!found || candidate < earliest) {
            found = true;
            earliest = candidate;
        }
    }
    if (found) *tick = earliest;
    return found;
}

size_t lynx__audio_apply(lynx__audio_state *audio, uint64_t tick,
                         lynx__audio_event_sink *sink)
{
    size_t emitted = 0u;
    if (audio == NULL) return 0u;
    if (audio->event_cursor_valid == 0u || audio->event_tick != tick) {
        audio->event_cursor_valid = 1u;
        audio->event_tick = tick;
        audio->event_sequence = 0u;
    }
    audio->same_tick_device_cursor = (uint8_t)(audio->same_tick_device_cursor |
                                               LYNX__AUDIO_DEVICE_APPLIED);
    for (uint8_t i = 0u; i < LYNX__AUDIO_CHANNEL_COUNT; i++) {
        lynx__audio_channel *channel = &audio->channels[i];
        if (!channel_can_advance(channel) || !channel_clock_at(channel, tick)) continue;
        if (audio_clock_channel(audio, i, tick, AUDIO_EVENT_CLOCK, false, true,
                                sink, NULL)) emitted++;
    }
    return emitted;
}

bool lynx__audio_sample(const lynx__audio_state *audio, uint64_t tick,
                        lynx_pcm_frame *out)
{
    if (audio == NULL || out == NULL || tick != audio->next_sample_tick) return false;
    int32_t left = 0;
    int32_t right = 0;
    for (uint8_t i = 0u; i < LYNX__AUDIO_CHANNEL_COUNT; i++) {
        const int32_t value = audio->channels[i].output;
        if ((audio->stereo_disable & (uint8_t)(AUDIO_STEREO_DISABLE_BASE << i)) == 0u) left += value;
        if ((audio->stereo_disable & (uint8_t)(1u << i)) == 0u) right += value;
    }
    out->left = (int16_t)(left * 64);
    out->right = (int16_t)(right * 64);
    return true;
}

void lynx__audio_mark_sample(lynx__audio_state *audio)
{
    if (audio == NULL) return;
    audio->same_tick_sample_cursor = 1u;
    if (audio->next_sample_ordinal != UINT64_MAX) audio->next_sample_ordinal++;
    if (audio->next_sample_tick <= UINT64_MAX - AUDIO_SAMPLE_TICKS) {
        audio->next_sample_tick += AUDIO_SAMPLE_TICKS;
    } else {
        audio->next_sample_tick = UINT64_MAX;
    }
}

uint64_t lynx__audio_advance_sample_phase_before(lynx__audio_state *audio,
                                                 uint64_t before_tick)
{
    if (audio == NULL || audio->next_sample_tick >= before_tick) return 0u;

    const uint64_t first_tick = audio->next_sample_tick;
    const uint64_t sample_count =
        ((before_tick - 1u) - first_tick) / AUDIO_SAMPLE_TICKS + 1u;
    const uint64_t last_offset = (sample_count - 1u) * AUDIO_SAMPLE_TICKS;
    const uint64_t last_tick = first_tick + last_offset;

    if (sample_count > UINT64_MAX - audio->next_sample_ordinal) {
        audio->next_sample_ordinal = UINT64_MAX;
    } else {
        audio->next_sample_ordinal += sample_count;
    }
    const uint64_t tick_steps_before_overflow =
        (UINT64_MAX - audio->next_sample_tick) / AUDIO_SAMPLE_TICKS;
    if (sample_count > tick_steps_before_overflow) {
        audio->next_sample_tick = UINT64_MAX;
    } else {
        audio->next_sample_tick += sample_count * AUDIO_SAMPLE_TICKS;
    }
    return last_tick;
}

bool lynx__audio_available(uint16_t addr)
{
    return (addr >= 0xFD20u && addr <= 0xFD3Fu) || addr == 0xFD50u;
}

uint8_t lynx__audio_peek(const lynx__audio_state *audio, uint16_t addr)
{
    if (audio == NULL) return 0u;
    if (addr == 0xFD50u) return audio->stereo_disable;
    uint8_t index = 0u;
    uint8_t reg = 0u;
    if (!audio_channel_address(addr, &index, &reg)) return 0u;
    const lynx__audio_channel *channel = &audio->channels[index];
    switch (reg) {
    case AUDIO_VOLUME: return channel->volume;
    case AUDIO_FEEDBACK: return channel->feedback;
    case AUDIO_OUTPUT: return (uint8_t)channel->output;
    case AUDIO_SHIFT_LOW: return (uint8_t)channel->shift;
    case AUDIO_BACKUP: return channel->backup;
    case AUDIO_CONTROL_A: return channel->control_a;
    case AUDIO_COUNT: return channel->count;
    case AUDIO_CONTROL_B: return (uint8_t)((channel->shift >> 4) & 0xF0u) |
                                      (channel->control_b & 0x0Fu);
    }
    return 0u;
}

bool lynx__audio_write(lynx__audio_state *audio, uint16_t addr, uint8_t value,
                       uint64_t tick, lynx__audio_event_sink *sink)
{
    if (audio == NULL) return false;
    if (addr == 0xFD50u) {
        audio->stereo_disable = value;
        return true;
    }
    uint8_t index = 0u;
    uint8_t reg = 0u;
    if (!audio_channel_address(addr, &index, &reg)) return false;
    lynx__audio_channel *channel = &audio->channels[index];
    const int8_t before = channel->output;
    bool clocked = false;
    switch (reg) {
    case AUDIO_VOLUME:
        channel->volume = value;
        break;
    case AUDIO_FEEDBACK:
        channel->feedback = value;
        break;
    case AUDIO_OUTPUT:
        channel->output = signed_register_byte(value);
        channel->integrator = channel->output;
        break;
    case AUDIO_SHIFT_LOW:
        channel->shift = (uint16_t)((channel->shift & 0x0F00u) | value);
        break;
    case AUDIO_BACKUP:
        channel->backup = value;
        break;
    case AUDIO_CONTROL_A:
        if (((value ^ channel->control_a) & AUDIO_CONTROL_A_INTEGRATE) != 0u) {
            channel->integrator = channel->output;
        }
        channel->control_a = value;
        if ((value & AUDIO_CONTROL_A_RESET_DONE) != 0u) {
            channel->control_b = (uint8_t)(channel->control_b & (uint8_t)~AUDIO_CONTROL_B_DONE);
        }
        break;
    case AUDIO_COUNT:
        channel->count = value;
        break;
    case AUDIO_CONTROL_B:
        channel->shift = (uint16_t)(((uint32_t)channel->shift & UINT32_C(0x00FF)) |
                                    ((uint32_t)(value & 0xF0u) << 4));
        channel->control_b = (uint8_t)((channel->control_b & 0xF0u) | (value & AUDIO_CONTROL_B_DONE));
        if ((value & AUDIO_CONTROL_B_BORROW_IN) != 0u) {
            clocked = audio_clock_channel(audio, index, tick, AUDIO_EVENT_CLOCK,
                                          false, false, sink, NULL);
        }
        break;
    }
    if (!clocked) {
        audio_emit(audio, tick, index, AUDIO_EVENT_WRITE, before, channel->output, sink);
    }
    return true;
}
