/* Authored digital audio checks. Register access uses a real 65C02 guest;
   exact device transitions and public PCM output use independent literals. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "lynxplosion.h"

static int failures;

#define ASSERT_NAMED(name, condition)                                                               \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "ASSERT %s failed\n", (name));                                  \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define ASSERT_EQ(name, actual, expected)                                                           \
    do {                                                                                           \
        const int64_t got_ = (int64_t)(actual);                                                     \
        const int64_t want_ = (int64_t)(expected);                                                  \
        if (got_ != want_) {                                                                       \
            (void)fprintf(stderr, "ASSERT %s failed: got %lld, expected %lld\n", (name),           \
                          (long long)got_, (long long)want_);                                       \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct machine_box {
    void *allocation;
    lynx_machine *machine;
} machine_box;

static int machine_open(machine_box *box, uint32_t timing_policy)
{
    memset(box, 0, sizeof *box);
    const size_t size = lynx_machine_size();
    const size_t alignment = lynx_machine_align();
    if (size == 0u || alignment == 0u || size > SIZE_MAX - alignment) return 0;
    box->allocation = malloc(size + alignment - 1u);
    if (box->allocation == NULL) return 0;
    uintptr_t address = (uintptr_t)box->allocation;
    const size_t remainder = (size_t)(address % alignment);
    if (remainder != 0u) address += alignment - remainder;
    lynx_config config;
    lynx_config_default(&config);
    config.timing_policy = timing_policy;
    if (lynx_machine_init((void *)address, size, &config, &box->machine) != LYNX_OK) {
        free(box->allocation);
        memset(box, 0, sizeof *box);
        return 0;
    }
    return 1;
}

static void machine_close(machine_box *box)
{
    if (box->machine != NULL) lynx_machine_deinit(box->machine);
    free(box->allocation);
    memset(box, 0, sizeof *box);
}

static lynx_status load_guest(lynx_machine *machine, const uint8_t *program, size_t length)
{
    if (program == NULL || length == 0u || length > UINT16_MAX) return LYNX_ERR_ARGUMENT;
    const size_t object_length = length + 10u;
    uint8_t *object = (uint8_t *)malloc(object_length);
    if (object == NULL) return LYNX_ERR_STATE;
    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = 0x02u;
    object[3] = 0x00u;
    object[4] = (uint8_t)(object_length >> 8);
    object[5] = (uint8_t)object_length;
    object[6] = 'B';
    object[7] = 'S';
    object[8] = '9';
    object[9] = '3';
    memcpy(object + 10u, program, length);
    const lynx_status result = lynx_load_bll(machine, object, object_length);
    free(object);
    return result;
}

static uint16_t channel_register(uint8_t channel, uint8_t reg)
{
    return (uint16_t)(0xFD20u + (uint16_t)channel * 8u + reg);
}

static int write_register(lynx__audio_state *audio, uint8_t channel, uint8_t reg,
                          uint8_t value)
{
    return lynx__audio_write(audio, channel_register(channel, reg), value, 0u, NULL) ? 1 : 0;
}

typedef struct byte_program {
    uint8_t bytes[1024];
    size_t length;
} byte_program;

static int emit_byte(byte_program *program, uint8_t value)
{
    if (program->length == sizeof program->bytes) return 0;
    program->bytes[program->length++] = value;
    return 1;
}

static int emit_store_imm(byte_program *program, uint16_t address, uint8_t value)
{
    return emit_byte(program, 0xA9u) && emit_byte(program, value) &&
           emit_byte(program, 0x8Du) && emit_byte(program, (uint8_t)address) &&
           emit_byte(program, (uint8_t)(address >> 8));
}

static int emit_read_store(byte_program *program, uint16_t source, uint16_t destination)
{
    return emit_byte(program, 0xADu) && emit_byte(program, (uint8_t)source) &&
           emit_byte(program, (uint8_t)(source >> 8)) && emit_byte(program, 0x8Du) &&
           emit_byte(program, (uint8_t)destination) &&
           emit_byte(program, (uint8_t)(destination >> 8));
}

static int append_loop(byte_program *program)
{
    const uint16_t address = (uint16_t)(0x0200u + program->length);
    return emit_byte(program, 0x4Cu) && emit_byte(program, (uint8_t)address) &&
           emit_byte(program, (uint8_t)(address >> 8));
}

static const uint8_t register_values[4][8] = {
    {0x7Fu, 0x01u, 0x23u, 0x01u, 0x00u, 0x00u, 0x00u, 0x18u},
    {0x80u, 0x42u, 0xD9u, 0xA6u, 0xFFu, 0x00u, 0xFFu, 0x28u},
    {0xF0u, 0x81u, 0x80u, 0x55u, 0xA5u, 0x20u, 0x01u, 0x30u},
    {0x00u, 0x35u, 0x7Fu, 0xFFu, 0x5Au, 0x80u, 0x80u, 0x48u},
};

static void case_registers(void)
{
    static const uint8_t write_order[8] = {0u, 1u, 3u, 4u, 5u, 6u, 7u, 2u};
    byte_program program = {{0u}, 0u};
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        for (size_t j = 0u; j < sizeof write_order; j++) {
            const uint8_t reg = write_order[j];
            ASSERT_NAMED("audio.registers.program-build",
                         emit_store_imm(&program, channel_register(channel, reg),
                                        register_values[channel][reg]));
        }
    }
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        for (uint8_t reg = 0u; reg < 8u; reg++) {
            const uint16_t destination = (uint16_t)(0x0400u + (uint16_t)channel * 8u + reg);
            ASSERT_NAMED("audio.registers.program-build",
                         emit_read_store(&program, channel_register(channel, reg), destination));
        }
    }
    ASSERT_NAMED("audio.registers.program-build", append_loop(&program));

    machine_box box;
    ASSERT_NAMED("audio.registers.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_LEGACY_FLAT5));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.registers.guest-load",
              load_guest(box.machine, program.bytes, program.length), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    ASSERT_EQ("audio.registers.guest-advance",
              lynx_advance(box.machine, 12000u, &consumed, &stop), LYNX_OK);
    ASSERT_EQ("audio.registers.guest-stop", stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.registers.guest-budget", consumed, 12000u);

    uint8_t guest_values[32];
    ASSERT_EQ("audio.registers.guest-readback",
              lynx_read_memory(box.machine, 0x0400u, guest_values, sizeof guest_values), LYNX_OK);
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        for (uint8_t reg = 0u; reg < 8u; reg++) {
            uint8_t expected = register_values[channel][reg];
            if (reg == 7u) expected = (uint8_t)((expected & 0xF0u) | (expected & 0x08u));
            ASSERT_EQ("audio.registers.guest-value",
                      guest_values[(size_t)channel * 8u + reg], expected);
        }
    }

    uint8_t mapped[32];
    uint8_t *before = (uint8_t *)malloc(lynx_machine_size());
    ASSERT_NAMED("audio.registers.snapshot-allocation", before != NULL);
    if (before != NULL) {
        memcpy(before, box.machine, lynx_machine_size());
        ASSERT_EQ("audio.registers.mapped-read",
                  lynx_read_cpu_memory(box.machine, 0xFD20u, mapped, sizeof mapped), LYNX_OK);
        for (uint8_t channel = 0u; channel < 4u; channel++) {
            for (uint8_t reg = 0u; reg < 8u; reg++) {
                uint8_t expected = register_values[channel][reg];
                if (reg == 7u) expected = (uint8_t)((expected & 0xF0u) | (expected & 0x08u));
                ASSERT_EQ("audio.registers.mapped-value", mapped[(size_t)channel * 8u + reg], expected);
            }
        }
        ASSERT_NAMED("audio.registers.peek-pure", memcmp(before, box.machine,
                                                        lynx_machine_size()) == 0);
        free(before);
    }
    machine_close(&box);

    static const uint8_t jump_to_audio[3] = {0x4Cu, 0x22u, 0xFDu};
    ASSERT_NAMED("audio.registers.opcode-machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_LEGACY_FLAT5));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.registers.opcode-guest-load",
              load_guest(box.machine, jump_to_audio, sizeof jump_to_audio), LYNX_OK);
    (void)lynx__audio_write(&box.machine->audio, 0xFD23u, 0xEAu, 0u, NULL);
    (void)lynx__audio_write(&box.machine->audio, 0xFD22u, 0xEAu, 0u, NULL);
    box.machine->ram[0xFD22u] = 0x00u;
    lynx__cpu_start_at(&box.machine->cpu, lynx__cpu_table(box.machine->cpu.profile), 0xFD22u);
    box.machine->cpu_ready_at = box.machine->guest_ticks;
    consumed = 0u;
    stop = LYNX_STOP_UNAVAILABLE;
    ASSERT_EQ("audio.registers.opcode-advance",
              lynx_advance(box.machine, 5u, &consumed, &stop), LYNX_OK);
    ASSERT_EQ("audio.registers.opcode-peek-route", stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.registers.opcode-peek-budget", consumed, 5u);
    ASSERT_EQ("audio.registers.opcode-ram-is-not-route", box.machine->ram[0xFD22u], 0u);
    machine_close(&box);
}

static void case_lfsr(void)
{
    static const struct {
        uint8_t tap;
        uint8_t feedback;
        uint16_t seed;
        uint16_t after_one_edge;
    } vectors[] = {
        {0u, 0x01u, 0x001u, 0x002u},
        {1u, 0x02u, 0x002u, 0x004u},
        {2u, 0x04u, 0x004u, 0x008u},
        {3u, 0x08u, 0x008u, 0x010u},
        {4u, 0x10u, 0x010u, 0x020u},
        {5u, 0x20u, 0x020u, 0x040u},
        {10u, 0x40u, 0x400u, 0x800u},
        {11u, 0x80u, 0x800u, 0x000u},
        {7u, 0x00u, 0x080u, 0x100u},
    };
    for (size_t i = 0u; i < sizeof vectors / sizeof vectors[0]; i++) {
        lynx__audio_state audio;
        lynx__audio_init(&audio);
        ASSERT_NAMED("audio.lfsr.setup", write_register(&audio, 0u, 0u, 5u));
        ASSERT_NAMED("audio.lfsr.setup", write_register(&audio, 0u, 1u, vectors[i].feedback));
        ASSERT_NAMED("audio.lfsr.setup", write_register(&audio, 0u, 3u,
                                                         (uint8_t)vectors[i].seed));
        ASSERT_NAMED("audio.lfsr.setup", write_register(&audio, 0u, 6u, 0u));
        const uint8_t seed_high = (uint8_t)((vectors[i].seed >> 4) & 0xF0u);
        ASSERT_NAMED("audio.lfsr.setup", write_register(&audio, 0u, 7u, seed_high));
        const uint8_t control_a = (uint8_t)(0x08u | (vectors[i].tap == 7u ? 0x80u : 0u));
        ASSERT_NAMED("audio.lfsr.setup", write_register(&audio, 0u, 5u, control_a));
        (void)lynx__audio_apply(&audio, 16u, NULL);
        const uint16_t observed = (uint16_t)(((uint16_t)(lynx__audio_peek(&audio, 0xFD27u) & 0xF0u) << 4) |
                                             lynx__audio_peek(&audio, 0xFD23u));
        ASSERT_EQ("audio.lfsr.tap-map", observed, vectors[i].after_one_edge);
    }

    static const struct {
        uint8_t feedback;
        uint8_t shift_low;
        uint8_t shift_high;
        uint8_t volume;
        uint8_t control_a;
        uint16_t expected_shift;
        uint8_t expected_output;
    } channel_vectors[4] = {
        {0x01u, 0x01u, 0x00u, 0x05u, 0x08u, 0x002u, 0xFBu},
        {0x02u, 0x02u, 0x00u, 0xF9u, 0x08u, 0x004u, 0x07u},
        {0x40u, 0x00u, 0x40u, 0x0Bu, 0x08u, 0x800u, 0xF5u},
        {0x00u, 0x80u, 0x00u, 0xF3u, 0x88u, 0x100u, 0x0Du},
    };
    lynx__audio_state four_channels;
    lynx__audio_init(&four_channels);
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        (void)write_register(&four_channels, channel, 0u, channel_vectors[channel].volume);
        (void)write_register(&four_channels, channel, 1u, channel_vectors[channel].feedback);
        (void)write_register(&four_channels, channel, 3u, channel_vectors[channel].shift_low);
        (void)write_register(&four_channels, channel, 6u, 0u);
        (void)write_register(&four_channels, channel, 7u, channel_vectors[channel].shift_high);
        (void)write_register(&four_channels, channel, 5u, channel_vectors[channel].control_a);
    }
    (void)lynx__audio_apply(&four_channels, 16u, NULL);
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        const uint16_t address = channel_register(channel, 7u);
        const uint16_t observed = (uint16_t)(((uint16_t)(lynx__audio_peek(&four_channels, address) & 0xF0u) << 4) |
                                             lynx__audio_peek(&four_channels,
                                                              channel_register(channel, 3u)));
        char label[64];
        (void)snprintf(label, sizeof label, "audio.lfsr.tap-map channel %u", channel);
        ASSERT_EQ(label, observed, channel_vectors[channel].expected_shift);
        (void)snprintf(label, sizeof label, "audio.lfsr.channel-output channel %u", channel);
        ASSERT_EQ(label, lynx__audio_peek(&four_channels, channel_register(channel, 2u)),
                  channel_vectors[channel].expected_output);
    }

    lynx__audio_state parity;
    lynx__audio_init(&parity);
    (void)write_register(&parity, 0u, 0u, 0u);
    (void)write_register(&parity, 0u, 1u, 0x03u);
    (void)write_register(&parity, 0u, 3u, 0x03u);
    (void)write_register(&parity, 0u, 6u, 0u);
    (void)write_register(&parity, 0u, 5u, 0x08u);
    (void)lynx__audio_apply(&parity, 16u, NULL);
    const uint16_t parity_state = (uint16_t)(((uint16_t)(lynx__audio_peek(&parity, 0xFD27u) & 0xF0u) << 4) |
                                             lynx__audio_peek(&parity, 0xFD23u));
    ASSERT_EQ("audio.lfsr.tap-map", parity_state, 0x007u);
    ASSERT_EQ("audio.lfsr.zero-volume-register", lynx__audio_peek(&parity, 0xFD22u), 0u);

    lynx__audio_state hot;
    lynx__audio_init(&hot);
    (void)write_register(&hot, 0u, 0u, 5u);
    (void)write_register(&hot, 0u, 3u, 1u);
    (void)write_register(&hot, 0u, 4u, 0u);
    (void)write_register(&hot, 0u, 6u, 0u);
    (void)write_register(&hot, 0u, 5u, 0x18u);
    (void)lynx__audio_apply(&hot, 16u, NULL);
    ASSERT_EQ("audio.lfsr.hot-seed-baseline", lynx__audio_peek(&hot, 0xFD23u), 3u);
    (void)write_register(&hot, 0u, 3u, 1u);
    (void)write_register(&hot, 0u, 1u, 1u);
    (void)lynx__audio_apply(&hot, 32u, NULL);
    ASSERT_EQ("audio.lfsr.hot-tap-and-seed", lynx__audio_peek(&hot, 0xFD23u), 2u);
}

static void case_counter(void)
{
    lynx__audio_state audio;
    lynx__audio_init(&audio);
    (void)write_register(&audio, 0u, 4u, 2u);
    (void)write_register(&audio, 0u, 6u, 1u);
    (void)write_register(&audio, 0u, 5u, 0x18u);
    (void)write_register(&audio, 1u, 6u, 0xFFu);
    (void)write_register(&audio, 1u, 5u, 0x08u);
    (void)write_register(&audio, 2u, 6u, 0u);
    (void)write_register(&audio, 2u, 5u, 0x08u);
    (void)write_register(&audio, 3u, 6u, 0u);
    (void)write_register(&audio, 3u, 5u, 0x08u);
    (void)lynx__audio_apply(&audio, 16u, NULL);
    ASSERT_EQ("audio.counter.count-255", lynx__audio_peek(&audio, 0xFD2Eu), 0xFEu);
    ASSERT_EQ("audio.counter.zero-one-shot", lynx__audio_peek(&audio, 0xFD36u), 0u);
    ASSERT_EQ("audio.counter.zero-one-shot-done", lynx__audio_peek(&audio, 0xFD37u) & 0x08u, 0x08u);
    ASSERT_EQ("audio.counter.channel3-one-shot-done", lynx__audio_peek(&audio, 0xFD3Fu) & 0x08u, 0x08u);
    ASSERT_EQ("audio.counter.reload-first", lynx__audio_peek(&audio, 0xFD26u), 0u);
    (void)lynx__audio_apply(&audio, 32u, NULL);
    ASSERT_EQ("audio.counter.reload-value", lynx__audio_peek(&audio, 0xFD26u), 2u);
    ASSERT_EQ("audio.counter.reload-done", lynx__audio_peek(&audio, 0xFD27u) & 0x08u, 0x08u);
    (void)lynx__audio_apply(&audio, 48u, NULL);
    ASSERT_EQ("audio.counter.reload-countdown", lynx__audio_peek(&audio, 0xFD26u), 1u);

    lynx__audio_state zero_volume;
    lynx__audio_init(&zero_volume);
    (void)write_register(&zero_volume, 0u, 0u, 0u);
    (void)write_register(&zero_volume, 0u, 3u, 1u);
    (void)write_register(&zero_volume, 0u, 4u, 0u);
    (void)write_register(&zero_volume, 0u, 6u, 0u);
    (void)write_register(&zero_volume, 0u, 5u, 0x18u);
    (void)lynx__audio_apply(&zero_volume, 16u, NULL);
    (void)lynx__audio_apply(&zero_volume, 32u, NULL);
    ASSERT_EQ("audio.counter.zero-volume-output", lynx__audio_peek(&zero_volume, 0xFD22u), 0u);
    ASSERT_EQ("audio.counter.zero-volume-lfsr", lynx__audio_peek(&zero_volume, 0xFD23u), 7u);
    ASSERT_EQ("audio.counter.zero-volume-count", lynx__audio_peek(&zero_volume, 0xFD26u), 0u);
    ASSERT_EQ("audio.counter.zero-volume-done", lynx__audio_peek(&zero_volume, 0xFD27u) & 0x08u, 0x08u);

    lynx__audio_state speed;
    lynx__audio_init(&speed);
    (void)write_register(&speed, 3u, 6u, 0u);
    (void)write_register(&speed, 3u, 5u, 0x08u);
    uint64_t tick = 0u;
    ASSERT_NAMED("audio.counter.speed-query", lynx__audio_next(&speed, 0u, &tick));
    ASSERT_EQ("audio.counter.speed-base", tick, 16u);
    (void)write_register(&speed, 3u, 5u, 0x09u);
    ASSERT_NAMED("audio.counter.speed-query", lynx__audio_next(&speed, 0u, &tick));
    ASSERT_EQ("audio.counter.speed-hot-change", tick, 32u);
    (void)write_register(&speed, 3u, 5u, 0x0Au);
    ASSERT_NAMED("audio.counter.speed-query", lynx__audio_next(&speed, 0u, &tick));
    ASSERT_EQ("audio.counter.speed-second-hot-change", tick, 64u);
}

static void case_integrator(void)
{
    lynx__audio_state audio;
    lynx__audio_init(&audio);
    (void)write_register(&audio, 0u, 0u, 0x7Fu);
    (void)write_register(&audio, 0u, 2u, 120u);
    (void)write_register(&audio, 0u, 4u, 0u);
    (void)write_register(&audio, 0u, 6u, 0u);
    (void)write_register(&audio, 0u, 5u, 0x38u);
    (void)write_register(&audio, 1u, 0u, 0x80u);
    (void)write_register(&audio, 1u, 2u, 0x88u);
    (void)write_register(&audio, 1u, 4u, 0u);
    (void)write_register(&audio, 1u, 6u, 0u);
    (void)write_register(&audio, 1u, 5u, 0x38u);
    (void)write_register(&audio, 2u, 0u, 10u);
    (void)write_register(&audio, 2u, 2u, 0u);
    (void)write_register(&audio, 2u, 4u, 0u);
    (void)write_register(&audio, 2u, 6u, 0u);
    (void)write_register(&audio, 2u, 5u, 0x38u);
    (void)write_register(&audio, 3u, 0u, 0xF6u);
    (void)write_register(&audio, 3u, 2u, 0u);
    (void)write_register(&audio, 3u, 4u, 0u);
    (void)write_register(&audio, 3u, 6u, 0u);
    (void)write_register(&audio, 3u, 5u, 0x38u);
    (void)lynx__audio_apply(&audio, 16u, NULL);
    ASSERT_EQ("audio.integrator.saturates", lynx__audio_peek(&audio, 0xFD22u), 127u);
    ASSERT_EQ("audio.integrator.saturates", lynx__audio_peek(&audio, 0xFD2Au), 0x80u);
    ASSERT_EQ("audio.integrator.channel2-output", lynx__audio_peek(&audio, 0xFD32u), 10u);
    ASSERT_EQ("audio.integrator.channel3-output", lynx__audio_peek(&audio, 0xFD3Au), 0xF6u);
    (void)lynx__audio_apply(&audio, 32u, NULL);
    ASSERT_EQ("audio.integrator.saturates", lynx__audio_peek(&audio, 0xFD22u), 127u);
    ASSERT_EQ("audio.integrator.saturates", lynx__audio_peek(&audio, 0xFD2Au), 0x80u);

    lynx__audio_state hot;
    lynx__audio_init(&hot);
    (void)write_register(&hot, 0u, 0u, 0x7Fu);
    ASSERT_EQ("audio.integrator.hot-volume", lynx__audio_peek(&hot, 0xFD22u), 0x81u);
    (void)write_register(&hot, 0u, 0u, 0x80u);
    ASSERT_EQ("audio.integrator.hot-volume", lynx__audio_peek(&hot, 0xFD22u), 0x7Fu);
    (void)write_register(&hot, 0u, 3u, 1u);
    ASSERT_EQ("audio.integrator.hot-volume-polarity", lynx__audio_peek(&hot, 0xFD22u), 0x80u);
}

static uint8_t audio_pen_scratch[LYNX_FRAME_PENS_CAPACITY];
static uint8_t audio_palette_scratch[LYNX_FRAME_PALETTE_CAPACITY];
static uint64_t audio_line_tick_scratch[LYNX_FRAME_LINE_TICKS_CAPACITY];
static lynx_pcm_frame audio_pcm_scratch[1];
static lynx_audio_event audio_event_scratch[LYNX_AV_MIN_EVENT_CAPACITY];

static int public_sample(const int8_t outputs[4], uint8_t muted, int16_t expected_left,
                        int16_t expected_right, const char *left_label, const char *right_label)
{
    static const uint8_t looping_guest[3] = {0x4Cu, 0x00u, 0x02u};
    machine_box box;
    ASSERT_NAMED("audio.mix.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return 0;
    ASSERT_EQ("audio.mix.guest-load", load_guest(box.machine, looping_guest,
                                                  sizeof looping_guest), LYNX_OK);
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        ASSERT_NAMED("audio.mix.channel-write",
                     lynx__audio_write(&box.machine->audio,
                                       channel_register(channel, 2u),
                                       (uint8_t)outputs[channel], 0u, NULL));
    }
    ASSERT_NAMED("audio.mix.mute-write",
                 lynx__audio_write(&box.machine->audio, 0xFD50u, muted, 0u, NULL));

    lynx_av_buffers buffers;
    memset(&buffers, 0, sizeof buffers);
    buffers.struct_size = (uint32_t)sizeof buffers;
    buffers.frame.struct_size = (uint32_t)sizeof buffers.frame;
    buffers.frame.pens = audio_pen_scratch;
    buffers.frame.pens_capacity = sizeof audio_pen_scratch;
    buffers.frame.palettes = audio_palette_scratch;
    buffers.frame.palettes_capacity = sizeof audio_palette_scratch;
    buffers.frame.line_ticks = audio_line_tick_scratch;
    buffers.frame.line_ticks_capacity = sizeof audio_line_tick_scratch / sizeof audio_line_tick_scratch[0];
    buffers.pcm = audio_pcm_scratch;
    buffers.pcm_capacity = sizeof audio_pcm_scratch / sizeof audio_pcm_scratch[0];
    buffers.events = audio_event_scratch;
    buffers.event_capacity = LYNX_AV_MIN_EVENT_CAPACITY;
    lynx_av_result result;
    memset(&result, 0, sizeof result);
    result.struct_size = (uint32_t)sizeof result;
    ASSERT_EQ("audio.mix.public-advance", lynx_advance_av(box.machine, 401u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.mix.public-sample-count", result.pcm_count, 1u);
    if (result.pcm_count == 1u) {
        ASSERT_EQ(left_label, audio_pcm_scratch[0].left, expected_left);
        ASSERT_EQ(right_label, audio_pcm_scratch[0].right, expected_right);
    }
    machine_close(&box);
    return 1;
}

static void case_stereo(void)
{
    for (uint8_t channel = 0u; channel < 4u; channel++) {
        int8_t outputs[4] = {0, 0, 0, 0};
        outputs[channel] = 37;
        char left[64];
        char right[64];
        (void)snprintf(left, sizeof left, "audio.stereo.left-route channel %u", channel);
        (void)snprintf(right, sizeof right, "audio.stereo.right-route channel %u", channel);
        (void)public_sample(outputs, (uint8_t)(1u << (channel + 4u)), 0, 2368, left, right);
        (void)public_sample(outputs, (uint8_t)(1u << channel), 2368, 0, left, right);
    }
    const int8_t silence[4] = {0, 0, 0, 0};
    (void)public_sample(silence, 0u, 0, 0, "audio.stereo.left-route all-enabled",
                        "audio.stereo.right-route all-enabled");
    (void)public_sample(silence, 0xFFu, 0, 0, "audio.stereo.left-route all-muted",
                        "audio.stereo.right-route all-muted");

    lynx__audio_state audio;
    lynx__audio_init(&audio);
    (void)write_register(&audio, 0u, 0u, 7u);
    (void)write_register(&audio, 0u, 3u, 0u);
    (void)write_register(&audio, 0u, 6u, 0u);
    (void)write_register(&audio, 0u, 5u, 0x08u);
    (void)lynx__audio_write(&audio, 0xFD50u, 0xFFu, 0u, NULL);
    lynx_audio_event event;
    lynx__audio_event_sink sink = {&event, 1u, 0u};
    (void)lynx__audio_apply(&audio, 16u, &sink);
    ASSERT_EQ("audio.stereo.mute-keeps-evolution", lynx__audio_peek(&audio, 0xFD23u), 1u);
    ASSERT_EQ("audio.stereo.mute-keeps-event", sink.count, 1u);
    ASSERT_EQ("audio.stereo.mute-event-before", event.before_output, -7);
    ASSERT_EQ("audio.stereo.mute-event-after", event.after_output, 7);
    lynx_pcm_frame muted = {INT16_MIN, INT16_MIN};
    ASSERT_NAMED("audio.stereo.muted-sample-ready", lynx__audio_sample(&audio, 400u, &muted));
    ASSERT_EQ("audio.stereo.muted-left", muted.left, 0);
    ASSERT_EQ("audio.stereo.muted-right", muted.right, 0);
}

static void case_mix(void)
{
    int8_t outputs[4] = {127, 127, 127, 127};
    (void)public_sample(outputs, 0u, 32512, 32512,
                        "audio.mix.positive-maximum-left", "audio.mix.positive-maximum-right");
    outputs[0] = -128;
    outputs[1] = -128;
    outputs[2] = -128;
    outputs[3] = -128;
    (void)public_sample(outputs, 0u, INT16_MIN, INT16_MIN,
                        "audio.mix.negative-maximum-left", "audio.mix.negative-maximum-right");
    outputs[0] = 127;
    outputs[1] = -128;
    outputs[2] = 0;
    outputs[3] = 0;
    (void)public_sample(outputs, 0u, -64, -64,
                        "audio.mix.cancellation-left", "audio.mix.cancellation-right");
}

typedef struct test_case {
    const char *name;
    void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"registers", case_registers},
    {"lfsr", case_lfsr},
    {"counter", case_counter},
    {"integrator", case_integrator},
    {"stereo", case_stereo},
    {"mix", case_mix},
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        (void)fprintf(stderr, "usage: lynx-test-audio <registers|lfsr|counter|integrator|stereo|mix>\n");
        return 2;
    }
    for (size_t i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            cases[i].run();
            return failures == 0 ? 0 : 1;
        }
    }
    (void)fprintf(stderr, "lynx-test-audio: unknown case %s\n", argv[1]);
    return 2;
}
