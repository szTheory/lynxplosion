/* Authored digital audio checks. Register access uses a real 65C02 guest;
   exact device transitions and public PCM output use independent literals. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "lynxplosion.h"
#include "sha256.h"

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
    ASSERT_EQ("audio.integrator.volume-write-keeps-output", lynx__audio_peek(&hot, 0xFD22u), 0u);
    (void)write_register(&hot, 0u, 1u, 1u);
    (void)write_register(&hot, 0u, 3u, 1u);
    (void)write_register(&hot, 0u, 7u, 2u);
    ASSERT_EQ("audio.integrator.hot-volume", lynx__audio_peek(&hot, 0xFD22u), 0x81u);
    (void)write_register(&hot, 0u, 0u, 0x80u);
    ASSERT_EQ("audio.integrator.hot-volume-latched", lynx__audio_peek(&hot, 0xFD22u), 0x81u);
    (void)write_register(&hot, 0u, 3u, 1u);
    (void)write_register(&hot, 0u, 7u, 2u);
    ASSERT_EQ("audio.integrator.hot-volume", lynx__audio_peek(&hot, 0xFD22u), 0x7Fu);
    (void)write_register(&hot, 0u, 3u, 0u);
    ASSERT_EQ("audio.integrator.seed-write-keeps-output", lynx__audio_peek(&hot, 0xFD22u), 0x7Fu);
    (void)write_register(&hot, 0u, 7u, 2u);
    ASSERT_EQ("audio.integrator.hot-volume-polarity", lynx__audio_peek(&hot, 0xFD22u), 0x80u);
}

static uint8_t audio_pen_scratch[LYNX_FRAME_PENS_CAPACITY];
static uint8_t audio_palette_scratch[LYNX_FRAME_PALETTE_CAPACITY];
static uint64_t audio_line_tick_scratch[LYNX_FRAME_LINE_TICKS_CAPACITY];
static lynx_pcm_frame audio_pcm_scratch[1];
static lynx_audio_event audio_event_scratch[LYNX_AV_MIN_EVENT_CAPACITY];

static void av_buffers_init(lynx_av_buffers *buffers, lynx_pcm_frame *pcm,
                            size_t pcm_capacity, lynx_audio_event *events,
                            size_t event_capacity)
{
    memset(buffers, 0, sizeof *buffers);
    buffers->struct_size = (uint32_t)sizeof *buffers;
    buffers->frame.struct_size = (uint32_t)sizeof buffers->frame;
    buffers->frame.pens = audio_pen_scratch;
    buffers->frame.pens_capacity = sizeof audio_pen_scratch;
    buffers->frame.palettes = audio_palette_scratch;
    buffers->frame.palettes_capacity = sizeof audio_palette_scratch;
    buffers->frame.line_ticks = audio_line_tick_scratch;
    buffers->frame.line_ticks_capacity = sizeof audio_line_tick_scratch /
                                         sizeof audio_line_tick_scratch[0];
    buffers->pcm = pcm;
    buffers->pcm_capacity = pcm_capacity;
    buffers->events = events;
    buffers->event_capacity = event_capacity;
}

static lynx_status av_advance(lynx_machine *machine, uint64_t budget,
                              const lynx_av_buffers *buffers, lynx_av_result *result)
{
    memset(result, 0, sizeof *result);
    result->struct_size = (uint32_t)sizeof *result;
    return lynx_advance_av(machine, budget, buffers, result);
}

static int load_until_write_request(lynx_machine *machine, const uint8_t *program,
                                    size_t length, uint16_t address,
                                    const char *label)
{
    const lynx_status loaded = load_guest(machine, program, length);
    if (loaded != LYNX_OK) {
        ASSERT_EQ(label, loaded, LYNX_OK);
        return 0;
    }
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    for (uint32_t attempt = 0u; attempt < 128u; attempt++) {
        if (lynx_advance(machine, 1u, &consumed, &stop) != LYNX_OK) break;
        if (machine->cpu.req_addr == address &&
            (machine->cpu.req_flags & LYNX__CF_WRITE) != 0u) return 1;
    }
    ASSERT_NAMED("audio.order.posted-cpu-write", 0);
    return 0;
}

static void case_cadence(void)
{
    static const uint8_t looping_guest[3] = {0x4Cu, 0x00u, 0x02u};
    machine_box coarse, one_tick, irregular, headroom;
    ASSERT_NAMED("audio.cadence.coarse-init",
                 machine_open(&coarse, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.cadence.one-tick-init",
                 machine_open(&one_tick, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.cadence.irregular-init",
                 machine_open(&irregular, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.cadence.headroom-init",
                 machine_open(&headroom, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (coarse.machine == NULL || one_tick.machine == NULL || irregular.machine == NULL ||
        headroom.machine == NULL) {
        machine_close(&coarse); machine_close(&one_tick);
        machine_close(&irregular); machine_close(&headroom);
        return;
    }
    ASSERT_EQ("audio.cadence.coarse-load", load_guest(coarse.machine, looping_guest,
                                                       sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.cadence.one-tick-load", load_guest(one_tick.machine, looping_guest,
                                                         sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.cadence.irregular-load", load_guest(irregular.machine, looping_guest,
                                                          sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.cadence.headroom-load", load_guest(headroom.machine, looping_guest,
                                                         sizeof looping_guest), LYNX_OK);
    lynx_machine *machines[3] = {coarse.machine, one_tick.machine, irregular.machine};
    for (size_t path = 0u; path < 3u; path++) {
        machines[path]->audio.channels[0].output = -128;
        machines[path]->audio.channels[1].output = 127;
    }

    lynx_pcm_frame coarse_pcm[2], one_tick_pcm[2], irregular_pcm[2];
    lynx_audio_event coarse_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event one_tick_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event irregular_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_av_buffers coarse_buffers, one_tick_buffers, irregular_buffers;
    av_buffers_init(&coarse_buffers, coarse_pcm, 2u, coarse_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    av_buffers_init(&one_tick_buffers, one_tick_pcm, 1u, one_tick_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    av_buffers_init(&irregular_buffers, irregular_pcm, 1u, irregular_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    lynx_av_result result;

    ASSERT_EQ("audio.cadence.zero-budget", av_advance(coarse.machine, 0u,
              &coarse_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.cadence.zero-budget-ticks", result.ticks_consumed, 0u);
    ASSERT_EQ("audio.cadence.zero-budget-ordinal", result.next_sample_ordinal, 0u);
    ASSERT_EQ("audio.cadence.zero-budget-pcm", result.pcm_count, 0u);
    ASSERT_EQ("audio.cadence.origin-ordinal-tick", coarse.machine->audio.next_sample_tick, 400u);
    ASSERT_EQ("audio.cadence.origin-ordinal", coarse.machine->audio.next_sample_ordinal, 0u);
    ASSERT_EQ("audio.cadence.coarse-advance", av_advance(coarse.machine, 801u,
              &coarse_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.cadence.coarse-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.cadence.coarse-ticks", result.ticks_consumed, 801u);
    ASSERT_EQ("audio.cadence.coarse-first-ordinal", result.first_sample_ordinal, 0u);
    ASSERT_EQ("audio.cadence.coarse-next-ordinal", result.next_sample_ordinal, 2u);
    ASSERT_EQ("audio.cadence.coarse-sample-count", result.pcm_count, 2u);
    ASSERT_EQ("audio.cadence.coarse-event-count", result.event_count, 0u);
    ASSERT_EQ("audio.cadence.sample-400-left", coarse_pcm[0].left, -64);
    ASSERT_EQ("audio.cadence.sample-400-right", coarse_pcm[0].right, -64);
    ASSERT_EQ("audio.cadence.sample-800-left", coarse_pcm[1].left, -64);
    ASSERT_EQ("audio.cadence.sample-800-right", coarse_pcm[1].right, -64);
    ASSERT_EQ("audio.cadence.coarse-zero-after", av_advance(coarse.machine, 0u,
              &coarse_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.cadence.zero-no-duplicate", result.pcm_count, 0u);
    ASSERT_EQ("audio.cadence.zero-keeps-ordinal", result.next_sample_ordinal, 2u);

    uint32_t one_tick_count = 0u;
    for (uint32_t step = 0u; step < 801u; step++) {
        one_tick_buffers.pcm = &one_tick_pcm[one_tick_count];
        ASSERT_EQ("audio.cadence.one-tick-advance", av_advance(one_tick.machine, 1u,
                  &one_tick_buffers, &result), LYNX_OK);
        ASSERT_EQ("audio.cadence.one-tick-budget", result.ticks_consumed, 1u);
        ASSERT_EQ("audio.cadence.one-tick-stop", result.stop, LYNX_STOP_BUDGET);
        ASSERT_EQ("audio.cadence.one-tick-no-events", result.event_count, 0u);
        if (step == 398u) {
            ASSERT_EQ("audio.cadence.tick-399", one_tick.machine->guest_ticks, 399u);
            ASSERT_EQ("audio.cadence.tick-399-pending", result.next_sample_ordinal, 0u);
            ASSERT_EQ("audio.cadence.tick-399-empty", result.pcm_count, 0u);
        } else if (step == 399u) {
            ASSERT_EQ("audio.cadence.tick-400-exclusive", one_tick.machine->guest_ticks, 400u);
            ASSERT_EQ("audio.cadence.tick-400-pending", result.next_sample_ordinal, 0u);
            ASSERT_EQ("audio.cadence.tick-400-empty", result.pcm_count, 0u);
        } else if (step == 400u) {
            ASSERT_EQ("audio.cadence.tick-401", one_tick.machine->guest_ticks, 401u);
            ASSERT_EQ("audio.cadence.tick-401-first", result.first_sample_ordinal, 0u);
            ASSERT_EQ("audio.cadence.tick-401-next", result.next_sample_ordinal, 1u);
            ASSERT_EQ("audio.cadence.tick-401-sample-once", result.pcm_count, 1u);
            one_tick_count += result.pcm_count;
        } else if (step == 798u) {
            ASSERT_EQ("audio.cadence.tick-799", one_tick.machine->guest_ticks, 799u);
            ASSERT_EQ("audio.cadence.tick-799-pending", result.next_sample_ordinal, 1u);
            ASSERT_EQ("audio.cadence.tick-799-empty", result.pcm_count, 0u);
        } else if (step == 799u) {
            ASSERT_EQ("audio.cadence.tick-800-exclusive", one_tick.machine->guest_ticks, 800u);
            ASSERT_EQ("audio.cadence.tick-800-pending", result.next_sample_ordinal, 1u);
            ASSERT_EQ("audio.cadence.tick-800-empty", result.pcm_count, 0u);
        } else if (step == 800u) {
            ASSERT_EQ("audio.cadence.tick-801", one_tick.machine->guest_ticks, 801u);
            ASSERT_EQ("audio.cadence.tick-801-first", result.first_sample_ordinal, 1u);
            ASSERT_EQ("audio.cadence.tick-801-next", result.next_sample_ordinal, 2u);
            ASSERT_EQ("audio.cadence.tick-801-sample-once", result.pcm_count, 1u);
            one_tick_count += result.pcm_count;
        }
    }
    ASSERT_EQ("audio.cadence.one-tick-total", one_tick_count, 2u);

    static const uint64_t irregular_slices[] = {137u, 262u, 1u, 1u, 211u, 187u, 1u, 1u};
    uint32_t irregular_count = 0u;
    uint64_t irregular_tick = 0u;
    for (size_t i = 0u; i < sizeof irregular_slices / sizeof irregular_slices[0]; i++) {
        irregular_buffers.pcm = &irregular_pcm[irregular_count];
        ASSERT_EQ("audio.cadence.irregular-advance", av_advance(irregular.machine,
                  irregular_slices[i], &irregular_buffers, &result), LYNX_OK);
        irregular_tick += irregular_slices[i];
        ASSERT_EQ("audio.cadence.irregular-ticks", result.ticks_consumed,
                  irregular_slices[i]);
        ASSERT_EQ("audio.cadence.irregular-guest-tick", irregular.machine->guest_ticks,
                  irregular_tick);
        ASSERT_EQ("audio.cadence.irregular-no-events", result.event_count, 0u);
        if (irregular_tick == 399u || irregular_tick == 400u ||
            irregular_tick == 799u || irregular_tick == 800u) {
            ASSERT_EQ("audio.cadence.irregular-exclusive-target", result.pcm_count, 0u);
        } else if (irregular_tick == 401u || irregular_tick == 801u) {
            ASSERT_EQ("audio.cadence.irregular-first-ordinal", result.first_sample_ordinal,
                      irregular_count);
            ASSERT_EQ("audio.cadence.irregular-next-ordinal", result.next_sample_ordinal,
                      irregular_count + 1u);
            ASSERT_EQ("audio.cadence.irregular-once", result.pcm_count, 1u);
            irregular_count += result.pcm_count;
        } else {
            ASSERT_EQ("audio.cadence.irregular-no-sample", result.pcm_count, 0u);
        }
    }
    ASSERT_EQ("audio.cadence.irregular-total", irregular_count, 2u);
    ASSERT_NAMED("audio.cadence.slicing-equal",
                 memcmp(coarse_pcm, one_tick_pcm, sizeof coarse_pcm) == 0 &&
                 memcmp(coarse_pcm, irregular_pcm, sizeof coarse_pcm) == 0);

    /* A legacy no-capture call must land on the same canonical phase and
       continuation as captured output at each half-open sample boundary. */
    machine_box boundary_capture, boundary_legacy;
    ASSERT_NAMED("audio.cadence.boundary-capture-init",
                 machine_open(&boundary_capture, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.cadence.boundary-legacy-init",
                 machine_open(&boundary_legacy, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (boundary_capture.machine == NULL || boundary_legacy.machine == NULL) {
        machine_close(&boundary_capture); machine_close(&boundary_legacy);
        machine_close(&coarse); machine_close(&one_tick);
        machine_close(&irregular); machine_close(&headroom);
        return;
    }
    ASSERT_EQ("audio.cadence.boundary-capture-load", load_guest(
        boundary_capture.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.cadence.boundary-legacy-load", load_guest(
        boundary_legacy.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    lynx_machine *boundary_machines[2] = {
        boundary_capture.machine, boundary_legacy.machine
    };
    for (size_t i = 0u; i < 2u; i++) {
        boundary_machines[i]->bus_hold = 1u;
        lynx__video_init(&boundary_machines[i]->video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
        boundary_machines[i]->video.refresh_next_tick = 0u;
        boundary_machines[i]->audio.channels[0].output = -3;
        boundary_machines[i]->audio.channels[1].output = 2;
    }
    lynx_pcm_frame boundary_pcm[2];
    lynx_audio_event boundary_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_av_buffers boundary_buffers;
    av_buffers_init(&boundary_buffers, boundary_pcm, 2u, boundary_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    static const uint64_t boundary_slices[] = {399u, 1u, 1u, 398u, 1u, 1u};
    static const uint64_t boundary_ordinals[] = {0u, 0u, 1u, 1u, 1u, 2u};
    uint32_t boundary_samples = 0u;
    uint64_t boundary_tick = 0u;
    uint64_t boundary_consumed = 0u;
    lynx_stop_reason boundary_stop = LYNX_STOP_UNAVAILABLE;
    for (size_t i = 0u; i < sizeof boundary_slices / sizeof boundary_slices[0]; i++) {
        const uint64_t slice = boundary_slices[i];
        boundary_buffers.pcm = &boundary_pcm[boundary_samples];
        boundary_buffers.pcm_capacity = 2u - boundary_samples;
        ASSERT_EQ("audio.cadence.boundary-captured-advance", av_advance(
            boundary_capture.machine, slice, &boundary_buffers, &result), LYNX_OK);
        ASSERT_EQ("audio.cadence.boundary-legacy-advance", lynx_advance(
            boundary_legacy.machine, slice, &boundary_consumed, &boundary_stop), LYNX_OK);
        boundary_tick += slice;
        ASSERT_EQ("audio.cadence.boundary-guest-tick", boundary_capture.machine->guest_ticks,
                  boundary_tick);
        ASSERT_EQ("audio.cadence.boundary-legacy-tick", boundary_legacy.machine->guest_ticks,
                  boundary_tick);
        ASSERT_EQ("audio.cadence.boundary-stop", result.stop, LYNX_STOP_BUDGET);
        ASSERT_EQ("audio.cadence.boundary-legacy-stop", boundary_stop, LYNX_STOP_BUDGET);
        ASSERT_EQ("audio.cadence.boundary-ordinal", result.next_sample_ordinal,
                  boundary_ordinals[i]);
        ASSERT_EQ("audio.cadence.boundary-legacy-ordinal",
                  boundary_legacy.machine->audio.next_sample_ordinal, boundary_ordinals[i]);
        ASSERT_EQ("audio.cadence.boundary-pcm-count", result.pcm_count,
                  (i == 2u || i == 5u) ? 1u : 0u);
        ASSERT_EQ("audio.cadence.boundary-event-count", result.event_count, 0u);
        boundary_samples += result.pcm_count;
        uint8_t captured_digest[32], legacy_digest[32];
        ASSERT_EQ("audio.cadence.boundary-captured-digest", lynx_state_digest(
            boundary_capture.machine, captured_digest), LYNX_OK);
        ASSERT_EQ("audio.cadence.boundary-legacy-digest", lynx_state_digest(
            boundary_legacy.machine, legacy_digest), LYNX_OK);
        ASSERT_NAMED("audio.cadence.boundary-canonical-match",
                     memcmp(captured_digest, legacy_digest, sizeof captured_digest) == 0);
    }
    ASSERT_EQ("audio.cadence.boundary-sample-count", boundary_samples, 2u);
    ASSERT_EQ("audio.cadence.boundary-sample-value-left", boundary_pcm[0].left, -64);
    ASSERT_EQ("audio.cadence.boundary-sample-value-right", boundary_pcm[0].right, -64);
    ASSERT_EQ("audio.cadence.boundary-second-sample-left", boundary_pcm[1].left, -64);
    ASSERT_EQ("audio.cadence.boundary-second-sample-right", boundary_pcm[1].right, -64);
    machine_close(&boundary_capture); machine_close(&boundary_legacy);

    /* Frame-stop continuation has the same canonical timer/audio phase with
       and without the PCM sink. The video fixture uses its ordinary timer roots. */
    machine_box frame_capture, frame_legacy;
    ASSERT_NAMED("audio.cadence.frame-capture-init",
                 machine_open(&frame_capture, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.cadence.frame-legacy-init",
                 machine_open(&frame_legacy, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (frame_capture.machine == NULL || frame_legacy.machine == NULL) {
        machine_close(&frame_capture); machine_close(&frame_legacy);
        machine_close(&coarse); machine_close(&one_tick);
        machine_close(&irregular); machine_close(&headroom);
        return;
    }
    ASSERT_EQ("audio.cadence.frame-load", load_guest(frame_capture.machine, looping_guest,
                                                       sizeof looping_guest), LYNX_OK);
    lynx_machine *frame_machine = frame_capture.machine;
    frame_machine->bus_hold = 1u;
    lynx__video_init(&frame_machine->video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    lynx__timer_init(&frame_machine->timer_bank);
    frame_machine->timer_bank.timers[0].backup = 158u;
    frame_machine->timer_bank.timers[0].count = 158u;
    frame_machine->timer_bank.timers[0].control_a = 0x18u;
    frame_machine->timer_bank.timers[2].backup = 104u;
    frame_machine->timer_bank.timers[2].count = 104u;
    frame_machine->timer_bank.timers[2].control_a = 0x1Fu;
    memcpy(frame_legacy.machine, frame_machine, lynx_machine_size());
    const uint64_t frame_edge = UINT64_C(159) * 16u * 105u;
    lynx_frame_result legacy_frame_result;
    memset(&legacy_frame_result, 0, sizeof legacy_frame_result);
    legacy_frame_result.struct_size = (uint32_t)sizeof legacy_frame_result;
    lynx_frame_buffers frame_buffers;
    memset(&frame_buffers, 0, sizeof frame_buffers);
    frame_buffers.struct_size = (uint32_t)sizeof frame_buffers;
    frame_buffers.pens = audio_pen_scratch;
    frame_buffers.pens_capacity = sizeof audio_pen_scratch;
    frame_buffers.palettes = audio_palette_scratch;
    frame_buffers.palettes_capacity = sizeof audio_palette_scratch;
    frame_buffers.line_ticks = audio_line_tick_scratch;
    frame_buffers.line_ticks_capacity = sizeof audio_line_tick_scratch /
                                        sizeof audio_line_tick_scratch[0];
    ASSERT_EQ("audio.cadence.frame-legacy-stop", lynx_advance_frame(
        frame_legacy.machine, frame_edge + 1u, &frame_buffers, &legacy_frame_result), LYNX_OK);
    ASSERT_EQ("audio.cadence.frame-legacy-kind", legacy_frame_result.stop, LYNX_STOP_FRAME);
    ASSERT_EQ("audio.cadence.frame-legacy-edge", legacy_frame_result.frame_end_tick, frame_edge);

    lynx_pcm_frame frame_pcm[700];
    lynx_audio_event frame_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_av_buffers frame_av_buffers;
    av_buffers_init(&frame_av_buffers, frame_pcm,
                    sizeof frame_pcm / sizeof frame_pcm[0], frame_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.cadence.frame-captured-stop", av_advance(
        frame_capture.machine, frame_edge + 1u, &frame_av_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.cadence.frame-captured-kind", result.stop, LYNX_STOP_FRAME);
    ASSERT_EQ("audio.cadence.frame-captured-edge", result.frame_end_tick, frame_edge);
    ASSERT_EQ("audio.cadence.frame-captured-samples", result.pcm_count,
              (frame_edge - 1u) / 400u);
    uint8_t frame_capture_digest[32], frame_legacy_digest[32];
    ASSERT_EQ("audio.cadence.frame-captured-digest", lynx_state_digest(
        frame_capture.machine, frame_capture_digest), LYNX_OK);
    ASSERT_EQ("audio.cadence.frame-legacy-digest", lynx_state_digest(
        frame_legacy.machine, frame_legacy_digest), LYNX_OK);
    ASSERT_NAMED("audio.cadence.frame-canonical-match",
                 memcmp(frame_capture_digest, frame_legacy_digest,
                        sizeof frame_capture_digest) == 0);
    machine_close(&frame_capture); machine_close(&frame_legacy);

    /* A final representable multiple is sampled without signed wrap; advancing
       its next tick uses a checked saturation rather than overflowing u64. */
    lynx__audio_state phase;
    lynx__audio_init(&phase);
    const uint64_t last_multiple = UINT64_MAX - (UINT64_MAX % UINT64_C(400));
    phase.next_sample_tick = last_multiple;
    phase.next_sample_ordinal = last_multiple / UINT64_C(400) - 1u;
    lynx_pcm_frame edge_sample;
    ASSERT_NAMED("audio.cadence.last-safe-sample-ready",
                 lynx__audio_sample(&phase, last_multiple, &edge_sample));
    const uint64_t ordinal_before_edge = phase.next_sample_ordinal;
    lynx__audio_mark_sample(&phase);
    ASSERT_EQ("audio.cadence.ordinal-no-wrap", phase.next_sample_ordinal,
              ordinal_before_edge + 1u);
    ASSERT_EQ("audio.cadence.tick-add-saturates", phase.next_sample_tick, UINT64_MAX);
    phase.next_sample_ordinal = UINT64_MAX;
    const uint64_t saturated_tick = phase.next_sample_tick;
    lynx__audio_mark_sample(&phase);
    ASSERT_EQ("audio.cadence.ordinal-saturation", phase.next_sample_ordinal, UINT64_MAX);
    ASSERT_EQ("audio.cadence.tick-saturation", phase.next_sample_tick, saturated_tick);

    const uint64_t guest_limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
    headroom.machine->guest_ticks = guest_limit - 7u;
    headroom.machine->cpu_ready_at = headroom.machine->guest_ticks;
    headroom.machine->bus_hold = 1u;
    headroom.machine->audio.next_sample_tick = UINT64_MAX;
    lynx_pcm_frame headroom_pcm;
    lynx_audio_event headroom_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_av_buffers headroom_buffers;
    av_buffers_init(&headroom_buffers, &headroom_pcm, 1u, headroom_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.cadence.headroom-advance", av_advance(headroom.machine, UINT64_MAX,
              &headroom_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.cadence.headroom-consumed", result.ticks_consumed, 7u);
    ASSERT_EQ("audio.cadence.headroom-tick", headroom.machine->guest_ticks, guest_limit);
    ASSERT_EQ("audio.cadence.headroom-no-sample", result.pcm_count, 0u);

    machine_close(&coarse); machine_close(&one_tick);
    machine_close(&irregular); machine_close(&headroom);
}

static void case_order(void)
{
    static const uint8_t volume_guest[] = {
        0xA9u, 0x07u,             /* LDA #new volume */
        0x8Du, 0x20u, 0xFDu,      /* STA A0 volume */
        0x4Cu, 0x00u, 0x02u       /* JMP $0200 */
    };
    machine_box box;
    ASSERT_NAMED("audio.order.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    lynx_machine *m = box.machine;
    ASSERT_NAMED("audio.order.posted-volume-write",
                 load_until_write_request(m, volume_guest, sizeof volume_guest,
                                          0xFD20u, "audio.order.guest-load"));
    if (m->cpu.req_addr != 0xFD20u || (m->cpu.req_flags & LYNX__CF_WRITE) == 0u) {
        machine_close(&box);
        return;
    }

    m->guest_ticks = 399u;
    m->cpu_ready_at = 400u;
    lynx__timer_init(&m->timer_bank);
    lynx__video_init(&m->video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    m->video.refresh_next_tick = 0u;
    lynx__audio_init(&m->audio);
    m->audio.channels[0].volume = 5u;
    m->audio.channels[0].feedback = 1u;
    m->audio.channels[0].control_a = 0x58u; /* A0 natural source, count/reload/reset-DONE */
    m->audio.channels[0].count = 0u;
    m->audio.channels[0].backup = 0u;
    m->audio.channels[1].volume = 3u;
    m->audio.channels[1].feedback = 1u;
    m->audio.channels[1].control_a = 0x5Fu; /* A1 receives the pending A0 link */
    m->audio.channels[1].count = 0u;
    m->audio.channels[1].backup = 0u;
    m->audio.ring_debt[4] = 1u;
    m->audio.ring_cursor = 4u;
    m->audio.ring_defer_valid = 1u;
    m->audio.ring_defer_tick = 400u;
    m->audio.same_tick_device_cursor = LYNX__AUDIO_RING_VISIT_LIMIT;
    ASSERT_NAMED("audio.order.fixture-ring-valid",
                 lynx__audio_ring_state_valid(&m->audio, 399u));
    ASSERT_EQ("audio.order.watch-configure", lynx_watch_configure(m, 0xFD20u, 1u),
              LYNX_OK);

    lynx_pcm_frame pcm[1];
    lynx_audio_event events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_av_buffers buffers;
    av_buffers_init(&buffers, pcm, 1u, events, LYNX_AV_MIN_EVENT_CAPACITY);
    lynx_av_result result;
    machine_box legacy_watch;
    ASSERT_NAMED("audio.order.legacy-watch-init",
                 machine_open(&legacy_watch, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (legacy_watch.machine == NULL) {
        machine_close(&box);
        return;
    }
    memcpy(legacy_watch.machine, m, lynx_machine_size());
    uint64_t legacy_consumed = 0u;
    lynx_stop_reason legacy_stop = LYNX_STOP_UNAVAILABLE;
    ASSERT_EQ("audio.order.legacy-watch-advance", lynx_advance(
        legacy_watch.machine, 2u, &legacy_consumed, &legacy_stop), LYNX_OK);
    ASSERT_EQ("audio.order.legacy-watch-stop", legacy_stop, LYNX_STOP_WATCH);
    ASSERT_EQ("audio.order.legacy-watch-consumed", legacy_consumed, 2u);
    ASSERT_EQ("audio.order.legacy-watch-return", legacy_watch.machine->guest_ticks, 401u);

    /* The exact target is exclusive: ring/device/sample/CPU work at 400 stays
       pending. The next one-tick slice commits it in declared order. */
    ASSERT_EQ("audio.order.exact-target", av_advance(m, 1u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.order.exact-target-tick", m->guest_ticks, 400u);
    ASSERT_EQ("audio.order.exact-target-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.order.exact-target-events", result.event_count, 0u);
    ASSERT_EQ("audio.order.exact-target-samples", result.pcm_count, 0u);
    ASSERT_EQ("audio.order.exact-target-debt", m->audio.ring_debt[4], 1u);
    ASSERT_EQ("audio.order.exact-target-sample", m->audio.next_sample_tick, 400u);
    ASSERT_EQ("audio.order.next-call", av_advance(m, 1u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.order.device-prefix-stop", result.stop, LYNX_STOP_OUTPUT);
    ASSERT_EQ("audio.order.device-prefix-ticks", result.ticks_consumed, 0u);
    ASSERT_EQ("audio.order.device-prefix-samples", result.pcm_count, 1u);
    ASSERT_EQ("audio.order.device-prefix-count", result.event_count, 4u);
    ASSERT_EQ("audio.order.device-prefix-pcm", pcm[0].left, -512);
    ASSERT_EQ("audio.order.device-prefix-first-ordinal", result.first_sample_ordinal, 0u);
    ASSERT_EQ("audio.order.device-prefix-next-ordinal", result.next_sample_ordinal, 1u);
    ASSERT_EQ("audio.order.device-prefix-cpu-pending", m->cpu.req_addr, 0xFD20u);
    lynx_audio_event resume_events[LYNX_AV_MIN_EVENT_CAPACITY];
    av_buffers_init(&buffers, pcm, 1u, resume_events, LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.order.resume-cpu-strobe", av_advance(m, 1u, &buffers, &result),
              LYNX_OK);
    ASSERT_EQ("audio.order.watch-stop", result.stop, LYNX_STOP_WATCH);
    ASSERT_EQ("audio.order.watch-grant-plus-one", m->guest_ticks, 401u);
    ASSERT_EQ("audio.order.watch-ticks", result.ticks_consumed, 1u);
    ASSERT_EQ("audio.order.resume-sample-count", result.pcm_count, 0u);
    ASSERT_EQ("audio.order.resume-first-ordinal", result.first_sample_ordinal, 1u);
    ASSERT_EQ("audio.order.resume-next-ordinal", result.next_sample_ordinal, 1u);
    ASSERT_EQ("audio.order.resume-event-count", result.event_count, 1u);
    ASSERT_EQ("audio.order.resume-event-tick", resume_events[0].tick, 400u);
    ASSERT_EQ("audio.order.resume-event-sequence", resume_events[0].sequence, 4u);
    ASSERT_EQ("audio.order.resume-event-channel", resume_events[0].channel, 0u);
    ASSERT_EQ("audio.order.resume-event-cause", resume_events[0].cause,
              LYNX_AUDIO_EVENT_WRITE);
    ASSERT_EQ("audio.order.sample-first-ordinal-after-resume", result.first_sample_ordinal, 1u);
    ASSERT_EQ("audio.order.sample-next-ordinal", result.next_sample_ordinal, 1u);
    ASSERT_EQ("audio.order.sample-once-left", pcm[0].left, -512);
    ASSERT_EQ("audio.order.sample-once-right", pcm[0].right, -512);
    ASSERT_EQ("audio.order.resume-event-count", result.event_count, 1u);
    static const uint8_t order_channel[4] = {0u, 1u, 0u, 1u};
    static const uint8_t order_cause[4] = {
        LYNX_AUDIO_EVENT_CLOCK, LYNX_AUDIO_EVENT_CLOCK,
        LYNX_AUDIO_EVENT_CLOCK, LYNX_AUDIO_EVENT_CLOCK
    };
    for (uint32_t i = 0u; i < 4u; i++) {
        ASSERT_EQ("audio.order.equal-tick", events[i].tick, 400u);
        ASSERT_EQ("audio.order.sequence", events[i].sequence, i);
        ASSERT_EQ("audio.order.channel", events[i].channel, order_channel[i]);
        ASSERT_EQ("audio.order.cause", events[i].cause, order_cause[i]);
    }
    ASSERT_EQ("audio.order.link-event-target", events[1].channel, 1u);
    ASSERT_EQ("audio.order.register-write-value", m->audio.channels[0].volume, 7u);
    ASSERT_EQ("audio.order.volume-write-latches-until-clock", m->audio.channels[0].output, -5);
    lynx_watch_event watch;
    ASSERT_EQ("audio.order.watch-read", lynx_watch_get(m, &watch), LYNX_OK);
    ASSERT_EQ("audio.order.watch-hit", watch.hit, 1u);
    ASSERT_EQ("audio.order.watch-address", watch.address, 0xFD20u);
    ASSERT_EQ("audio.order.watch-value", watch.value, 7u);
    ASSERT_EQ("audio.order.watch-grant", watch.hit_tick, 400u);
    ASSERT_EQ("audio.order.watch-return", watch.return_tick, 401u);
    uint8_t captured_watch_digest[32], legacy_watch_digest[32];
    ASSERT_EQ("audio.order.captured-watch-digest", lynx_state_digest(
        m, captured_watch_digest), LYNX_OK);
    ASSERT_EQ("audio.order.legacy-watch-digest", lynx_state_digest(
        legacy_watch.machine, legacy_watch_digest), LYNX_OK);
    ASSERT_NAMED("audio.order.watch-canonical-match",
                 memcmp(captured_watch_digest, legacy_watch_digest,
                        sizeof captured_watch_digest) == 0);
    machine_close(&legacy_watch);
    machine_close(&box);

    /* A write that returns at tick 400 leaves sample ordinal zero pending at
       that same return tick; the following AV call emits it exactly once. */
    static const uint8_t ram_guest[] = {
        0xA9u, 0x5Au,             /* LDA #$5A */
        0x8Du, 0x00u, 0x04u,      /* STA $0400 */
        0x4Cu, 0x00u, 0x02u       /* JMP $0200 */
    };
    ASSERT_NAMED("audio.order.return-machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    m = box.machine;
    ASSERT_NAMED("audio.order.posted-ram-write",
                 load_until_write_request(m, ram_guest, sizeof ram_guest,
                                          0x0400u, "audio.order.ram-guest-load"));
    if (m->cpu.req_addr != 0x0400u || (m->cpu.req_flags & LYNX__CF_WRITE) == 0u) {
        machine_close(&box);
        return;
    }
    m->guest_ticks = 399u;
    m->cpu_ready_at = 399u;
    lynx__timer_init(&m->timer_bank);
    lynx__video_init(&m->video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    m->video.refresh_next_tick = 0u;
    lynx__audio_init(&m->audio);
    m->audio.channels[0].output = 2;
    m->audio.channels[1].output = -1;
    ASSERT_EQ("audio.order.return-watch-configure", lynx_watch_configure(m, 0x0400u, 1u),
              LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    ASSERT_EQ("audio.order.return-watch-advance", lynx_advance(m, 1u, &consumed, &stop),
              LYNX_OK);
    ASSERT_EQ("audio.order.return-watch-stop", stop, LYNX_STOP_WATCH);
    ASSERT_EQ("audio.order.return-watch-tick", m->guest_ticks, 400u);
    ASSERT_EQ("audio.order.return-watch-ram", m->ram[0x0400u], 0x5Au);
    ASSERT_EQ("audio.order.return-sample-pending", m->audio.next_sample_tick, 400u);
    ASSERT_EQ("audio.order.return-sample-ordinal", m->audio.next_sample_ordinal, 0u);
    ASSERT_EQ("audio.order.return-av", av_advance(m, 1u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.order.return-sample-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.order.return-sample-tick", m->guest_ticks, 401u);
    ASSERT_EQ("audio.order.return-sample-count", result.pcm_count, 1u);
    ASSERT_EQ("audio.order.return-sample-first", result.first_sample_ordinal, 0u);
    ASSERT_EQ("audio.order.return-sample-next", result.next_sample_ordinal, 1u);
    ASSERT_EQ("audio.order.return-sample-value-left", pcm[0].left, 64);
    ASSERT_EQ("audio.order.return-sample-value-right", pcm[0].right, 64);
    ASSERT_EQ("audio.order.return-sample-no-duplicate", av_advance(m, 0u, &buffers, &result),
              LYNX_OK);
    ASSERT_EQ("audio.order.return-no-duplicate", result.pcm_count, 0u);
    machine_close(&box);
}

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
    ASSERT_EQ("audio.stereo.mute-event-before", event.before_output, 0);
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

static const uint8_t ring_timer_indexes[4] = {1u, 3u, 5u, 7u};

static uint16_t timer_register(uint8_t index, uint8_t reg)
{
    return (uint16_t)(0xFD00u + (uint16_t)index * 4u + reg);
}

static void configure_zero_link_ring(lynx__timer_bank *bank, lynx__audio_state *audio)
{
    lynx__timer_init(bank);
    lynx__audio_init(audio);
    for (size_t i = 0u; i < 4u; i++) {
        bank->timers[ring_timer_indexes[i]].control_a = 0x4Fu;
        bank->timers[ring_timer_indexes[i]].count = 0u;
        audio->channels[i].control_a = 0x4Fu;
        audio->channels[i].count = 0u;
    }
}

static uint64_t ring_debt_sum(const lynx__audio_state *audio)
{
    uint64_t total = 0u;
    for (size_t i = 0u; i < LYNX__AUDIO_RING_NODE_COUNT; i++) {
        total += audio->ring_debt[i];
    }
    return total;
}

static void check_a3_t1_wake(void)
{
    machine_box source;
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    static const uint8_t irq_loop[] = {0xEAu, 0x4Cu, 0x00u, 0x02u};

    /* A3→T1 also supplies the pending IRQ line before the CPU's existing
       instruction poll. I masks IRQ acceptance, not wake/progress. */
    for (uint8_t mask_irq = 0u; mask_irq <= 1u; mask_irq++) {
        ASSERT_NAMED("audio.links.a3-t1-machine", machine_open(
            &source, LYNX_TIMING_POLICY_LEGACY_FLAT5));
        if (source.machine == NULL) return;
        ASSERT_EQ("audio.links.a3-t1-program",
                  load_guest(source.machine, irq_loop, sizeof irq_loop), LYNX_OK);
        lynx_machine *m = source.machine;
        m->timer_bank.timers[1].control_a = 0xCFu; /* link, IRQ, count, reset-DONE */
        m->timer_bank.timers[1].count = 0u;
        m->audio.channels[3].control_a = 0x48u; /* A3 natural root */
        m->audio.channels[3].count = 0u;
        m->cpu.p = mask_irq != 0u ? (uint8_t)(m->cpu.p | 0x04u) :
                                    (uint8_t)(m->cpu.p & (uint8_t)~0x04u);
        const lynx__cpu before_hold = m->cpu;
        m->bus_hold = 1u;
        ASSERT_EQ("audio.links.a3-t1-held-advance", lynx_advance(
            m, 17u, &consumed, &stop), LYNX_OK);
        ASSERT_EQ("audio.links.a3-t1-pending", m->timer_bank.pending & 0x02u, 0x02u);
        ASSERT_EQ("audio.links.a3-t1-wake", m->wake_pending, 1u);
        ASSERT_NAMED("audio.links.a3-t1-held-cpu-unchanged",
                     memcmp(&before_hold, &m->cpu, sizeof before_hold) == 0);
        ASSERT_EQ("audio.links.a3-t1-release", lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
        ASSERT_EQ("audio.links.a3-t1-cpu-poll-advance", lynx_advance(
            m, 20u, &consumed, &stop), LYNX_OK);
        if (mask_irq != 0u) {
            ASSERT_EQ("audio.links.a3-t1-masked-no-irq-entry",
                      m->cpu.flags & LYNX__CS_SEQ_MASK, 0u);
            ASSERT_NAMED("audio.links.a3-t1-masked-cpu-progress",
                         m->cpu.req_addr != 0x0200u || m->cpu.req_kind != LYNX__CK_OPCODE);
            ASSERT_EQ("audio.links.a3-t1-masked-pending-retained",
                      m->timer_bank.pending & 0x02u, 0x02u);
            ASSERT_EQ("audio.links.a3-t1-masked-wake-retained", m->wake_pending, 1u);
        } else {
            ASSERT_EQ("audio.links.a3-t1-cpu-poll",
                      m->cpu.flags & LYNX__CS_SEQ_MASK, 0x10u);
        }
        machine_close(&source);
    }
}

static void case_links(void)
{
    static const char *const edge_names[8] = {
        "audio.links.t1-t3", "audio.links.t3-t5", "audio.links.t5-t7",
        "audio.links.t7-a0", "audio.links.a0-a1", "audio.links.a1-a2",
        "audio.links.a2-a3", "audio.links.a3-t1"
    };
    for (uint8_t source = 0u; source < LYNX__AUDIO_RING_NODE_COUNT; source++) {
        lynx__timer_bank bank;
        lynx__audio_state audio;
        configure_zero_link_ring(&bank, &audio);
        for (size_t i = 0u; i < 4u; i++) {
            bank.timers[ring_timer_indexes[i]].control_a = 0u;
            audio.channels[i].control_a = 0u;
        }
        const uint8_t target = (uint8_t)((source + 1u) % LYNX__AUDIO_RING_NODE_COUNT);
        if (source < 4u) {
            bank.timers[ring_timer_indexes[source]].control_a = 0x48u;
            bank.timers[ring_timer_indexes[source]].count = 0u;
        } else {
            audio.channels[source - 4u].control_a = 0x48u;
            audio.channels[source - 4u].count = 0u;
        }
        if (target < 4u) {
            bank.timers[ring_timer_indexes[target]].control_a = 0x4Fu;
            bank.timers[ring_timer_indexes[target]].count = 1u;
        } else {
            audio.channels[target - 4u].control_a = 0x4Fu;
            audio.channels[target - 4u].count = 1u;
        }

        const uint16_t source_ctlb = source < 4u ?
            timer_register(ring_timer_indexes[source], 3u) :
            channel_register((uint8_t)(source - 4u), 7u);
        const lynx__work_status status = lynx__timer_audio_write(
            &bank, &audio, source_ctlb, 0x02u, 0u, NULL);
        ASSERT_EQ(edge_names[source], status, LYNX__WORK_OK);
        if (target < 4u) {
            const lynx__timer *timer = &bank.timers[ring_timer_indexes[target]];
            ASSERT_EQ(edge_names[source], timer->count, 0u);
            ASSERT_EQ(edge_names[source], timer->control_b & 0x06u, 0x06u);
        } else {
            const lynx__audio_channel *channel = &audio.channels[target - 4u];
            ASSERT_EQ(edge_names[source], channel->count, 0u);
            ASSERT_EQ(edge_names[source], channel->control_b & 0x06u, 0x06u);
        }
        ASSERT_EQ(edge_names[source], ring_debt_sum(&audio), 0u);
        ASSERT_NAMED(edge_names[source], lynx__audio_ring_state_valid(&audio, 0u));
    }

    /* A one-shot T1 underflow still emits the T3 pulse when reload is off. */
    lynx__timer_bank bank;
    lynx__audio_state audio;
    configure_zero_link_ring(&bank, &audio);
    for (size_t i = 0u; i < 4u; i++) {
        bank.timers[ring_timer_indexes[i]].control_a = 0u;
        audio.channels[i].control_a = 0u;
    }
    bank.timers[1].control_a = 0x0Fu; /* COUNT + link, no reload/reset-DONE */
    bank.timers[1].count = 0u;
    bank.timers[3].control_a = 0x4Fu;
    bank.timers[3].count = 1u;
    ASSERT_EQ("audio.links.oneshot-underflow", lynx__timer_audio_write(
        &bank, &audio, timer_register(1u, 3u), 0x02u, 0u, NULL), LYNX__WORK_OK);
    ASSERT_EQ("audio.links.oneshot-done", bank.timers[1].control_b & 0x08u, 0x08u);
    ASSERT_EQ("audio.links.oneshot-successor", bank.timers[3].count, 0u);

    /* All eight ordinary source roots may coincide; the scheduler applies each
       once in timer order followed by audio channel order. */
    lynx__timer_init(&bank);
    lynx__audio_init(&audio);
    for (size_t i = 0u; i < 4u; i++) {
        bank.timers[ring_timer_indexes[i]].control_a = 0x48u;
        bank.timers[ring_timer_indexes[i]].count = 0u;
        audio.channels[i].control_a = 0x48u;
        audio.channels[i].count = 0u;
    }
    uint8_t borrow_mask = 0u;
    ASSERT_EQ("audio.links.simultaneous-root-work", lynx__timer_audio_apply(
        &bank, &audio, 16u, 0u, true, true, NULL, &borrow_mask), LYNX__WORK_OK);
    ASSERT_EQ("audio.links.simultaneous-timer-roots", borrow_mask, 0xAAu);
    for (size_t i = 0u; i < 4u; i++) {
        ASSERT_EQ("audio.links.simultaneous-audio-roots",
                  audio.channels[i].control_b & 0x05u, 0x05u);
    }
    check_a3_t1_wake();
}

static void case_ring_zero(void)
{
    lynx__timer_bank bank;
    lynx__audio_state audio;
    configure_zero_link_ring(&bank, &audio);
    lynx__audio_state malformed_deadline;
    lynx__audio_init(&malformed_deadline);
    malformed_deadline.ring_debt[0] = 1u;
    malformed_deadline.ring_defer_valid = 1u;
    malformed_deadline.ring_defer_tick = 1u;
    ASSERT_NAMED("audio.ring_zero.future-deadline-requires-exhausted-slice",
                 !lynx__audio_ring_state_valid(&malformed_deadline, 0u));
    malformed_deadline.same_tick_device_cursor = LYNX__AUDIO_RING_VISIT_LIMIT;
    ASSERT_NAMED("audio.ring_zero.future-deadline-valid-at-limit",
                 lynx__audio_ring_state_valid(&malformed_deadline, 0u));
    ASSERT_EQ("audio.ring_zero.seed", lynx__timer_audio_write(
        &bank, &audio, timer_register(1u, 3u), 0x02u, 0u, NULL), LYNX__WORK_OK);
    ASSERT_EQ("audio.ring_zero.seed-debt", ring_debt_sum(&audio), 1u);
    ASSERT_EQ("audio.ring_zero.seed-deadline", audio.ring_defer_tick, 1u);

    for (uint64_t tick = 1u; tick <= 32u; tick++) {
        uint8_t borrow_mask = 0u;
        ASSERT_EQ("audio.ring_zero.deferred-tick", lynx__timer_audio_apply(
            &bank, &audio, tick, tick - 1u, false, false, NULL, &borrow_mask),
            LYNX__WORK_OK);
        ASSERT_EQ("audio.ring_zero.manual-injection", lynx__timer_audio_write(
            &bank, &audio, timer_register(1u, 3u), 0x02u, tick, NULL), LYNX__WORK_OK);
        ASSERT_EQ("audio.ring_zero.conserved-multiplicity", ring_debt_sum(&audio), tick + 1u);
        ASSERT_NAMED("audio.ring_zero.guest-time-debt-bound",
                     lynx__audio_ring_state_valid(&audio, tick));
    }

    /* A source-mode change cannot erase already queued copies. One pulse goes
       around to T1 and stops because T3 no longer selects link. */
    bank.timers[3].control_a = 0x48u;
    const uint64_t before_mode_change = ring_debt_sum(&audio);
    ASSERT_EQ("audio.ring_zero.mode-change", lynx__timer_audio_apply(
        &bank, &audio, 33u, 32u, false, false, NULL, NULL), LYNX__WORK_OK);
    ASSERT_EQ("audio.ring_zero.mode-change-retains-copies", ring_debt_sum(&audio),
              before_mode_change - 1u);
    bank.timers[3].control_a = 0x4Fu;
    ASSERT_EQ("audio.ring_zero.mode-restore", lynx__timer_audio_apply(
        &bank, &audio, 34u, 33u, false, false, NULL, NULL), LYNX__WORK_OK);
    ASSERT_EQ("audio.ring_zero.mode-restore-multiplicity", ring_debt_sum(&audio),
              before_mode_change - 1u);

    /* Exercise a low-word carry near the guest-time limit while the total is
       still reachable, then reject an unreachable high nibble atomically. */
    const uint64_t near_limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
    lynx__timer_init(&bank);
    lynx__audio_init(&audio);
    bank.timers[1].control_a = 0x0Fu;
    bank.timers[1].count = 0u;
    bank.timers[3].control_a = 0x07u; /* linked but stopped: retain its debt */
    audio.ring_debt[1] = UINT64_MAX;
    audio.ring_defer_valid = 1u;
    audio.ring_defer_tick = near_limit;
    ASSERT_NAMED("audio.ring_zero.near-u64-bound-valid",
                 lynx__audio_ring_state_valid(&audio, near_limit));
    ASSERT_EQ("audio.ring_zero.near-u64-carry", lynx__timer_audio_write(
        &bank, &audio, timer_register(1u, 3u), 0x02u, near_limit, NULL), LYNX__WORK_OK);
    ASSERT_EQ("audio.ring_zero.near-u64-low", audio.ring_debt[1], 0u);
    ASSERT_EQ("audio.ring_zero.near-u64-high",
              (audio.ring_debt_high >> LYNX__AUDIO_RING_HIGH_BITS) & 0x0Fu, 1u);

    lynx__timer_bank malformed_bank = bank;
    lynx__audio_state malformed_audio = audio;
    malformed_audio.ring_debt[1] = UINT64_MAX;
    malformed_audio.ring_debt_high = 0x0Fu;
    const lynx__timer_bank before_bank = malformed_bank;
    const lynx__audio_state before_audio = malformed_audio;
    ASSERT_EQ("audio.ring_zero.unreachable-add-refused", lynx__timer_audio_write(
        &malformed_bank, &malformed_audio, timer_register(1u, 3u), 0x02u,
        near_limit, NULL), LYNX__WORK_CAPACITY);
    ASSERT_NAMED("audio.ring_zero.unreachable-add-atomic",
                 memcmp(&malformed_bank, &before_bank, sizeof before_bank) == 0 &&
                 memcmp(&malformed_audio, &before_audio, sizeof before_audio) == 0);

    /* Machine-level zero-cycle work is bounded, survives an exact-target stop,
       and resumes at the serialized due-now tick after save/restore. */
    machine_box source;
    machine_box restored;
    ASSERT_NAMED("audio.ring_zero.machine-init", machine_open(
        &source, LYNX_TIMING_POLICY_LEGACY_FLAT5));
    if (source.machine == NULL) return;
    ASSERT_NAMED("audio.ring_zero.restore-init", machine_open(
        &restored, LYNX_TIMING_POLICY_LEGACY_FLAT5));
    if (restored.machine == NULL) {
        machine_close(&source);
        return;
    }
    static const uint8_t ring_guest[] = {0x4Cu, 0x00u, 0x02u};
    ASSERT_EQ("audio.ring_zero.guest-load", load_guest(
        source.machine, ring_guest, sizeof ring_guest), LYNX_OK);
    ASSERT_EQ("audio.ring_zero.restore-guest-load", load_guest(
        restored.machine, ring_guest, sizeof ring_guest), LYNX_OK);
    configure_zero_link_ring(&source.machine->timer_bank, &source.machine->audio);
    source.machine->bus_hold = 1u;
    ASSERT_EQ("audio.ring_zero.machine-seed", lynx__timer_audio_write(
        &source.machine->timer_bank, &source.machine->audio, timer_register(1u, 3u),
        0x02u, 0u, NULL), LYNX__WORK_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    ASSERT_EQ("audio.ring_zero.exact-boundary-advance",
              lynx_advance(source.machine, 1u, &consumed, &stop), LYNX_OK);
    ASSERT_EQ("audio.ring_zero.exact-boundary-stop", stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.ring_zero.exact-boundary-tick", source.machine->guest_ticks, 1u);
    ASSERT_EQ("audio.ring_zero.exact-boundary-due-now",
              source.machine->audio.ring_defer_tick, source.machine->guest_ticks);
    size_t snapshot_size = 0u;
    ASSERT_EQ("audio.ring_zero.snapshot-size",
              lynx_snapshot_size(source.machine, &snapshot_size), LYNX_OK);
    uint8_t *snapshot = (uint8_t *)malloc(snapshot_size);
    ASSERT_NAMED("audio.ring_zero.snapshot-allocation", snapshot != NULL);
    if (snapshot != NULL) {
        size_t written = 0u;
        ASSERT_EQ("audio.ring_zero.snapshot-save",
                  lynx_snapshot_save(source.machine, snapshot, snapshot_size, &written), LYNX_OK);
        ASSERT_EQ("audio.ring_zero.snapshot-written", written, snapshot_size);
        ASSERT_EQ("audio.ring_zero.snapshot-restore",
                  lynx_snapshot_restore(restored.machine, snapshot, snapshot_size), LYNX_OK);
        free(snapshot);
    }
    for (uint8_t step = 0u; step < 16u; step++) {
        ASSERT_EQ("audio.ring_zero.source-resume", lynx_advance(
            source.machine, 1u, &consumed, &stop), LYNX_OK);
        ASSERT_EQ("audio.ring_zero.restored-resume", lynx_advance(
            restored.machine, 1u, &consumed, &stop), LYNX_OK);
        uint8_t source_digest[32];
        uint8_t restored_digest[32];
        ASSERT_EQ("audio.ring_zero.source-digest",
                  lynx_state_digest(source.machine, source_digest), LYNX_OK);
        ASSERT_EQ("audio.ring_zero.restored-digest",
                  lynx_state_digest(restored.machine, restored_digest), LYNX_OK);
        ASSERT_NAMED("audio.ring_zero.snapshot-continuation",
                     memcmp(source_digest, restored_digest, sizeof source_digest) == 0);
    }
    machine_close(&restored);
    machine_close(&source);

    /* Exercise both CPU bus entry points for manual borrow-in. Each instruction
       is a distinct timed root, and the cycle keeps both copies pending. */
    ASSERT_NAMED("audio.ring_zero.cpu-manual-init", machine_open(
        &source, LYNX_TIMING_POLICY_LEGACY_FLAT5));
    if (source.machine == NULL) return;
    static const uint8_t cpu_manual_roots[] = {
        0xA9u, 0x02u,             /* LDA #BORROW_IN */
        0x8Du, 0x07u, 0xFDu,      /* STA T1 CTLB */
        0xA9u, 0x02u,             /* LDA #BORROW_IN */
        0x8Du, 0x3Fu, 0xFDu,      /* STA A3 CTLB */
        0x4Cu, 0x0Au, 0x02u       /* JMP $020A */
    };
    ASSERT_EQ("audio.ring_zero.cpu-manual-load", load_guest(
        source.machine, cpu_manual_roots, sizeof cpu_manual_roots), LYNX_OK);
    configure_zero_link_ring(&source.machine->timer_bank, &source.machine->audio);
    ASSERT_EQ("audio.ring_zero.cpu-manual-advance", lynx_advance(
        source.machine, 70u, &consumed, &stop), LYNX_OK);
    ASSERT_EQ("audio.ring_zero.cpu-manual-stop", stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.ring_zero.cpu-manual-budget", consumed, 70u);
    ASSERT_EQ("audio.ring_zero.cpu-manual-multiplicity",
              ring_debt_sum(&source.machine->audio), 2u);
    ASSERT_EQ("audio.ring_zero.cpu-manual-deadline",
              source.machine->audio.ring_defer_tick, 70u);
    machine_close(&source);

}

typedef struct av_span_storage {
    max_align_t alignment;
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY + 1u];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY + 1u];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY + 1u];
    _Alignas(max_align_t) uint8_t audio[1024];
} av_span_storage;

static void av_span_storage_init(av_span_storage *storage, lynx_av_buffers *buffers,
                                 size_t pcm_capacity, size_t event_capacity)
{
    memset(storage, 0xA5, sizeof *storage);
    memset(buffers, 0, sizeof *buffers);
    buffers->struct_size = (uint32_t)sizeof *buffers;
    buffers->frame.struct_size = (uint32_t)sizeof buffers->frame;
    buffers->frame.pens = storage->pens;
    buffers->frame.pens_capacity = LYNX_FRAME_PENS_CAPACITY;
    buffers->frame.palettes = storage->palettes;
    buffers->frame.palettes_capacity = LYNX_FRAME_PALETTE_CAPACITY;
    buffers->frame.line_ticks = storage->line_ticks;
    buffers->frame.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
    buffers->pcm = (lynx_pcm_frame *)(void *)storage->audio;
    buffers->pcm_capacity = pcm_capacity;
    buffers->events = (lynx_audio_event *)(void *)(storage->audio + 256u);
    buffers->event_capacity = event_capacity;
}

static void av_result_init(lynx_av_result *result)
{
    memset(result, 0x5Au, sizeof *result);
    result->struct_size = (uint32_t)sizeof *result;
}

static void check_av_refusal_unchanged(lynx_machine *machine, lynx_av_buffers *buffers,
                                       lynx_av_result *result, av_span_storage *storage,
                                       lynx_status expected, const char *label)
{
    const size_t machine_size = lynx_machine_size();
    uint8_t *machine_before = (uint8_t *)malloc(machine_size);
    av_span_storage *storage_before = (av_span_storage *)malloc(sizeof *storage_before);
    ASSERT_NAMED("audio.span.snapshot-allocation",
                 machine_before != NULL && storage_before != NULL);
    if (machine_before == NULL || storage_before == NULL) {
        free(machine_before);
        free(storage_before);
        return;
    }
    lynx_av_buffers buffers_before = *buffers;
    lynx_av_result result_before = *result;
    memcpy(machine_before, machine, machine_size);
    memcpy(storage_before, storage, sizeof *storage_before);
    ASSERT_EQ(label, lynx_advance_av(machine, 1u, buffers, result), expected);
    ASSERT_NAMED(label, memcmp(machine_before, machine, machine_size) == 0);
    ASSERT_NAMED(label, memcmp(&buffers_before, buffers, sizeof buffers_before) == 0);
    ASSERT_NAMED(label, memcmp(&result_before, result, sizeof result_before) == 0);
    ASSERT_NAMED(label, memcmp(storage_before, storage, sizeof *storage_before) == 0);
    free(machine_before);
    free(storage_before);
}

static void case_span_refusal(void)
{
    static const uint8_t looping_guest[] = {0x4Cu, 0x00u, 0x02u};
    machine_box box;
    ASSERT_NAMED("audio.span.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.span.guest-load", load_guest(box.machine, looping_guest,
                                                    sizeof looping_guest), LYNX_OK);

    av_span_storage storage;
    lynx_av_buffers buffers;
    lynx_av_result result;
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.struct_size--;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.descriptor-size");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.reserved = 1u;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.descriptor-reserved");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.struct_size--;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.frame-size");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.reserved = 1u;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.frame-reserved");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    result.struct_size--;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.result-size");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = NULL;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.null-pcm");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.events = NULL;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.null-events");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.pens = NULL;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.null-pens");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.palettes = NULL;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.null-palettes");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.line_ticks = NULL;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.null-line-ticks");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.pens_capacity--;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_CAPACITY, "audio.span.pens-one-short");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.palettes_capacity--;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_CAPACITY, "audio.span.palettes-one-short");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.line_ticks_capacity--;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_CAPACITY, "audio.span.ticks-one-short");
    av_span_storage_init(&storage, &buffers, 0u, LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_CAPACITY, "audio.span.pcm-zero-capacity");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY - 1u);
    av_result_init(&result);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_CAPACITY, "audio.span.events-one-short");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = (lynx_pcm_frame *)(void *)(storage.audio + 1u);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ALIGNMENT, "audio.span.pcm-alignment");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.events = (lynx_audio_event *)(void *)(storage.audio + 257u);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ALIGNMENT, "audio.span.event-alignment");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.line_ticks = (uint64_t *)(void *)((uint8_t *)storage.line_ticks + 1u);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ALIGNMENT, "audio.span.tick-alignment");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = (lynx_pcm_frame *)(uintptr_t)(UINTPTR_MAX - 3u);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.pcm-pointer-wrap");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm_capacity = SIZE_MAX / sizeof(lynx_pcm_frame) + 1u;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.pcm-size-wrap");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.event_capacity = SIZE_MAX / sizeof(lynx_audio_event) + 1u;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.event-size-wrap");
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.frame.line_ticks_capacity = SIZE_MAX / sizeof(uint64_t) + 1u;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span.tick-size-wrap");

    machine_close(&box);
}

static void case_span_alias(void)
{
    static const uint8_t looping_guest[] = {0x4Cu, 0x00u, 0x02u};
    machine_box box;
    ASSERT_NAMED("audio.span_alias.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.span_alias.guest-load", load_guest(box.machine, looping_guest,
                                                         sizeof looping_guest), LYNX_OK);

    av_span_storage storage;
    lynx_av_buffers buffers;
    lynx_av_result result;
    av_span_storage_init(&storage, &buffers, 2u, LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm_capacity = 8u; /* The write prefix is short; these full declared spans overlap. */
    buffers.events = (lynx_audio_event *)(void *)(storage.audio + 8u);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.pcm-event-full-range");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.events = (lynx_audio_event *)(void *)storage.line_ticks;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.frame-event");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = (lynx_pcm_frame *)(void *)buffers.frame.pens;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.pcm-frame");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = (lynx_pcm_frame *)(void *)&buffers;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.descriptor-pcm");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = (lynx_pcm_frame *)(void *)box.machine;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.machine-pcm");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.pcm = (lynx_pcm_frame *)(void *)&result;
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.result-pcm");

    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    check_av_refusal_unchanged(box.machine, &buffers,
                               (lynx_av_result *)(void *)&buffers, &storage,
                               LYNX_ERR_ARGUMENT, "audio.span_alias.descriptor-result");

    av_span_storage_init(&storage, &buffers, 2u, LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    buffers.events = (lynx_audio_event *)(void *)(storage.audio + 8u);
    buffers.event_capacity = LYNX_AV_MIN_EVENT_CAPACITY;
    ASSERT_EQ("audio.span_alias.adjacent-one-past-end",
              lynx_advance_av(box.machine, 0u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.span_alias.adjacent-zero-budget", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.span_alias.adjacent-no-pcm", result.pcm_count, 0u);
    ASSERT_EQ("audio.span_alias.adjacent-no-events", result.event_count, 0u);
    ASSERT_NAMED("audio.span_alias.adjacent-caller-bytes",
                 storage.pens[0] == 0xA5u && storage.audio[0] == 0xA5u &&
                 storage.audio[8] == 0xA5u);

    machine_close(&box);
}

static bool audio_event_equal(const lynx_audio_event *left,
                              const lynx_audio_event *right)
{
    return left->tick == right->tick && left->sequence == right->sequence &&
           left->channel == right->channel && left->cause == right->cause &&
           left->before_output == right->before_output &&
           left->after_output == right->after_output;
}

static void case_capacity(void)
{
    static const uint8_t looping_guest[] = {0x4Cu, 0x00u, 0x02u};
    machine_box sliced;
    machine_box whole;
    ASSERT_NAMED("audio.capacity.sliced-init",
                 machine_open(&sliced, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.capacity.whole-init",
                 machine_open(&whole, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (sliced.machine == NULL || whole.machine == NULL) {
        machine_close(&sliced);
        machine_close(&whole);
        return;
    }
    ASSERT_EQ("audio.capacity.sliced-load", load_guest(sliced.machine, looping_guest,
                                                         sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.capacity.whole-load", load_guest(whole.machine, looping_guest,
                                                        sizeof looping_guest), LYNX_OK);
    for (uint8_t i = 0u; i < LYNX_AUDIO_CHANNELS; i++) {
        sliced.machine->audio.channels[i].volume = (uint8_t)(i + 1u);
        sliced.machine->audio.channels[i].feedback = 1u;
        sliced.machine->audio.channels[i].control_a = 0x58u; /* reload, reset-DONE, count */
        whole.machine->audio.channels[i] = sliced.machine->audio.channels[i];
    }

    lynx_pcm_frame pcm[1];
    lynx_audio_event first_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event retry_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event whole_events[LYNX_AV_MIN_EVENT_CAPACITY * 2u];
    lynx_av_buffers buffers;
    lynx_av_result result;
    av_buffers_init(&buffers, pcm, 1u, first_events, LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.capacity.first-call", av_advance(sliced.machine, 33u,
                                                        &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.first-stop", result.stop, LYNX_STOP_OUTPUT);
    ASSERT_EQ("audio.capacity.first-pcm-count", result.pcm_count, 0u);
    ASSERT_EQ("audio.capacity.first-event-count", result.event_count, 4u);
    ASSERT_EQ("audio.capacity.first-event-tick", first_events[0].tick, 16u);
    ASSERT_EQ("audio.capacity.first-next-device-tick",
              sliced.machine->audio.channels[0].count, 0u);
    for (uint32_t i = 0u; i < result.event_count; i++) {
        ASSERT_EQ("audio.capacity.first-sequence", first_events[i].sequence, i);
        ASSERT_EQ("audio.capacity.first-channel", first_events[i].channel, i);
        ASSERT_EQ("audio.capacity.first-tick", first_events[i].tick, 16u);
    }

    const uint64_t retry_budget = 33u - sliced.machine->guest_ticks;
    av_buffers_init(&buffers, pcm, 1u, retry_events, LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.capacity.retry-call", av_advance(sliced.machine, retry_budget,
                                                        &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.retry-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.capacity.retry-event-count", result.event_count, 4u);
    for (uint32_t i = 0u; i < result.event_count; i++) {
        ASSERT_EQ("audio.capacity.retry-sequence", retry_events[i].sequence, i);
        ASSERT_EQ("audio.capacity.retry-channel", retry_events[i].channel, i);
        ASSERT_EQ("audio.capacity.retry-tick", retry_events[i].tick, 32u);
    }

    lynx_pcm_frame whole_pcm[1];
    av_buffers_init(&buffers, whole_pcm, 1u, whole_events,
                    sizeof whole_events / sizeof whole_events[0]);
    ASSERT_EQ("audio.capacity.whole-call", av_advance(whole.machine, 33u,
                                                        &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.whole-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.capacity.whole-event-count", result.event_count, 8u);
    for (uint32_t i = 0u; i < 4u; i++) {
        ASSERT_NAMED("audio.capacity.first-prefix-equal",
                     audio_event_equal(&first_events[i], &whole_events[i]));
        ASSERT_NAMED("audio.capacity.retry-suffix-equal",
                     audio_event_equal(&retry_events[i], &whole_events[i + 4u]));
    }
    uint8_t sliced_digest[32];
    uint8_t whole_digest[32];
    ASSERT_EQ("audio.capacity.sliced-digest",
              lynx_state_digest(sliced.machine, sliced_digest), LYNX_OK);
    ASSERT_EQ("audio.capacity.whole-digest",
              lynx_state_digest(whole.machine, whole_digest), LYNX_OK);
    ASSERT_NAMED("audio.capacity.retry-state-equal",
                 memcmp(sliced_digest, whole_digest, sizeof sliced_digest) == 0);
    machine_close(&whole);
    machine_close(&sliced);

    /* A full one-frame caller span returns the first sample and stops before
       the second sample frontier. A fresh span resumes ordinal 1 exactly. */
    machine_box sample_sliced;
    machine_box sample_whole;
    ASSERT_NAMED("audio.capacity.sample-sliced-init",
                 machine_open(&sample_sliced, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.capacity.sample-whole-init",
                 machine_open(&sample_whole, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (sample_sliced.machine == NULL || sample_whole.machine == NULL) {
        machine_close(&sample_sliced);
        machine_close(&sample_whole);
        return;
    }
    ASSERT_EQ("audio.capacity.sample-sliced-load", load_guest(
        sample_sliced.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.capacity.sample-whole-load", load_guest(
        sample_whole.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    lynx_pcm_frame sliced_pcm[1];
    lynx_pcm_frame whole_pcm_pair[2];
    lynx_audio_event sliced_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event resumed_events[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event ample_events[LYNX_AV_MIN_EVENT_CAPACITY];
    av_buffers_init(&buffers, sliced_pcm, 1u, sliced_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.capacity.sample-first-call", av_advance(
        sample_sliced.machine, 801u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.sample-output-stop", result.stop, LYNX_STOP_OUTPUT);
    ASSERT_EQ("audio.capacity.sample-first-count", result.pcm_count, 1u);
    ASSERT_EQ("audio.capacity.sample-first-ordinal", result.first_sample_ordinal, 0u);
    ASSERT_EQ("audio.capacity.sample-next-ordinal", result.next_sample_ordinal, 1u);
    ASSERT_EQ("audio.capacity.sample-next-tick",
              sample_sliced.machine->audio.next_sample_tick, 800u);

    const uint64_t sample_retry_budget = 801u - sample_sliced.machine->guest_ticks;
    av_buffers_init(&buffers, sliced_pcm, 1u, resumed_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.capacity.sample-retry", av_advance(
        sample_sliced.machine, sample_retry_budget, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.sample-retry-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.capacity.sample-retry-count", result.pcm_count, 1u);
    ASSERT_EQ("audio.capacity.sample-retry-first-ordinal", result.first_sample_ordinal, 1u);
    ASSERT_EQ("audio.capacity.sample-retry-next-ordinal", result.next_sample_ordinal, 2u);

    av_buffers_init(&buffers, whole_pcm_pair, 2u, ample_events,
                    LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.capacity.sample-whole", av_advance(
        sample_whole.machine, 801u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.sample-whole-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.capacity.sample-whole-count", result.pcm_count, 2u);
    ASSERT_NAMED("audio.capacity.sample-prefix-equal",
                 sliced_pcm[0].left == whole_pcm_pair[0].left &&
                 sliced_pcm[0].right == whole_pcm_pair[0].right);
    ASSERT_NAMED("audio.capacity.sample-suffix-equal",
                 sliced_pcm[0].left == whole_pcm_pair[1].left &&
                 sliced_pcm[0].right == whole_pcm_pair[1].right);
    ASSERT_EQ("audio.capacity.sample-final-tick", sample_sliced.machine->guest_ticks, 801u);
    ASSERT_EQ("audio.capacity.sample-whole-tick", sample_whole.machine->guest_ticks, 801u);
    ASSERT_EQ("audio.capacity.sample-sliced-digest", lynx_state_digest(
        sample_sliced.machine, sliced_digest), LYNX_OK);
    ASSERT_EQ("audio.capacity.sample-whole-digest", lynx_state_digest(
        sample_whole.machine, whole_digest), LYNX_OK);
    ASSERT_NAMED("audio.capacity.sample-retry-state-equal",
                 memcmp(sliced_digest, whole_digest, sizeof sliced_digest) == 0);
    machine_close(&sample_whole);
    machine_close(&sample_sliced);

    /* Distinct instances remain isolated when their caller buffers are
       replaced between calls. Mutating the old spans is safe, and a later
       call writes only to the newly supplied spans. */
    machine_box first_instance;
    machine_box second_instance;
    ASSERT_NAMED("audio.capacity.instance-first-init",
                 machine_open(&first_instance, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.capacity.instance-second-init",
                 machine_open(&second_instance, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (first_instance.machine == NULL || second_instance.machine == NULL) {
        machine_close(&first_instance);
        machine_close(&second_instance);
        return;
    }
    ASSERT_EQ("audio.capacity.instance-first-load", load_guest(
        first_instance.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.capacity.instance-second-load", load_guest(
        second_instance.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    av_span_storage first_old_storage;
    av_span_storage first_new_storage;
    av_span_storage second_old_storage;
    av_span_storage second_new_storage;
    lynx_av_buffers first_old_buffers;
    lynx_av_buffers first_new_buffers;
    lynx_av_buffers second_old_buffers;
    lynx_av_buffers second_new_buffers;
    av_span_storage_init(&first_old_storage, &first_old_buffers,
                         LYNX_AV_MIN_PCM_CAPACITY, LYNX_AV_MIN_EVENT_CAPACITY);
    av_span_storage_init(&first_new_storage, &first_new_buffers,
                         LYNX_AV_MIN_PCM_CAPACITY, LYNX_AV_MIN_EVENT_CAPACITY);
    av_span_storage_init(&second_old_storage, &second_old_buffers,
                         LYNX_AV_MIN_PCM_CAPACITY, LYNX_AV_MIN_EVENT_CAPACITY);
    av_span_storage_init(&second_new_storage, &second_new_buffers,
                         LYNX_AV_MIN_PCM_CAPACITY, LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    ASSERT_EQ("audio.capacity.instance-first-sample", lynx_advance_av(
        first_instance.machine, 401u, &first_old_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.instance-first-count", result.pcm_count, 1u);
    const lynx_pcm_frame first_sample = *(const lynx_pcm_frame *)(const void *)
        first_old_buffers.pcm;
    memset(&first_old_storage, 0x3Cu, sizeof first_old_storage);

    av_result_init(&result);
    ASSERT_EQ("audio.capacity.instance-second-sample", lynx_advance_av(
        second_instance.machine, 401u, &second_old_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.instance-second-count", result.pcm_count, 1u);
    ASSERT_NAMED("audio.capacity.instance-old-span-not-retained",
                 first_old_storage.audio[0] == 0x3Cu &&
                 first_old_storage.audio[256] == 0x3Cu);
    const lynx_pcm_frame second_sample = *(const lynx_pcm_frame *)(const void *)
        second_old_buffers.pcm;
    memset(&second_old_storage, 0x6Bu, sizeof second_old_storage);

    av_result_init(&result);
    ASSERT_EQ("audio.capacity.instance-first-resume", lynx_advance_av(
        first_instance.machine, 400u, &first_new_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.instance-first-resume-count", result.pcm_count, 1u);
    ASSERT_EQ("audio.capacity.instance-first-resume-ordinal",
              result.first_sample_ordinal, 1u);
    ASSERT_NAMED("audio.capacity.instance-first-new-span-written",
                 first_new_storage.audio[0] != 0xA5u);
    ASSERT_NAMED("audio.capacity.instance-first-old-span-preserved",
                 first_old_storage.audio[0] == 0x3Cu &&
                 first_old_storage.audio[256] == 0x3Cu);

    av_result_init(&result);
    ASSERT_EQ("audio.capacity.instance-second-resume", lynx_advance_av(
        second_instance.machine, 400u, &second_new_buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.capacity.instance-second-resume-count", result.pcm_count, 1u);
    ASSERT_EQ("audio.capacity.instance-second-resume-ordinal",
              result.first_sample_ordinal, 1u);
    ASSERT_NAMED("audio.capacity.instance-second-new-span-written",
                 second_new_storage.audio[0] != 0xA5u);
    ASSERT_NAMED("audio.capacity.instance-second-old-span-preserved",
                 second_old_storage.audio[0] == 0x6Bu &&
                 second_old_storage.audio[256] == 0x6Bu);
    ASSERT_NAMED("audio.capacity.instance-first-samples-equal",
                 first_sample.left == second_sample.left &&
                 first_sample.right == second_sample.right);
    uint8_t first_instance_digest[32];
    uint8_t second_instance_digest[32];
    ASSERT_EQ("audio.capacity.instance-first-digest", lynx_state_digest(
        first_instance.machine, first_instance_digest), LYNX_OK);
    ASSERT_EQ("audio.capacity.instance-second-digest", lynx_state_digest(
        second_instance.machine, second_instance_digest), LYNX_OK);
    ASSERT_NAMED("audio.capacity.instance-state-equal",
                 memcmp(first_instance_digest, second_instance_digest,
                        sizeof first_instance_digest) == 0);
    machine_close(&second_instance);
    machine_close(&first_instance);
}

static void case_stops(void)
{
    static const uint8_t looping_guest[] = {0x4Cu, 0x00u, 0x02u};
    static const uint8_t unavailable_guest[] = {
        0xADu, 0x91u, 0xFDu, /* LDA unavailable FD91 */
        0x4Cu, 0x00u, 0x02u
    };
    machine_box box;
    ASSERT_NAMED("audio.stops.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.stops.guest-load", load_guest(box.machine, looping_guest,
                                                    sizeof looping_guest), LYNX_OK);
    av_span_storage storage;
    lynx_av_buffers buffers;
    lynx_av_result result;
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    uint8_t *machine_before = (uint8_t *)malloc(lynx_machine_size());
    ASSERT_NAMED("audio.stops.snapshot-allocation", machine_before != NULL);
    if (machine_before != NULL) {
        memcpy(machine_before, box.machine, lynx_machine_size());
        ASSERT_EQ("audio.stops.zero-budget-call", lynx_advance_av(
            box.machine, 0u, &buffers, &result), LYNX_OK);
        ASSERT_EQ("audio.stops.zero-budget-reason", result.stop, LYNX_STOP_BUDGET);
        ASSERT_EQ("audio.stops.zero-budget-ticks", result.ticks_consumed, 0u);
        ASSERT_EQ("audio.stops.zero-budget-pcm", result.pcm_count, 0u);
        ASSERT_EQ("audio.stops.zero-budget-events", result.event_count, 0u);
        ASSERT_NAMED("audio.stops.zero-budget-machine",
                     memcmp(machine_before, box.machine, lynx_machine_size()) == 0);
        ASSERT_NAMED("audio.stops.zero-budget-buffers",
                     storage.pens[0] == 0xA5u && storage.palettes[0] == 0xA5u &&
                     storage.line_ticks[0] == UINT64_C(0xA5A5A5A5A5A5A5A5) &&
                     storage.audio[0] == 0xA5u && storage.audio[256] == 0xA5u);

        const uint64_t guest_limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
        box.machine->guest_ticks = guest_limit - 7u;
        box.machine->cpu_ready_at = box.machine->guest_ticks;
        box.machine->bus_hold = 1u;
        box.machine->audio.next_sample_tick = UINT64_MAX;
        av_result_init(&result);
        ASSERT_EQ("audio.stops.headroom-call", lynx_advance_av(
            box.machine, UINT64_MAX, &buffers, &result), LYNX_OK);
        ASSERT_EQ("audio.stops.headroom-reason", result.stop, LYNX_STOP_BUDGET);
        ASSERT_EQ("audio.stops.headroom-consumed", result.ticks_consumed, 7u);
        ASSERT_EQ("audio.stops.headroom-tick", box.machine->guest_ticks, guest_limit);
        ASSERT_EQ("audio.stops.headroom-pcm", result.pcm_count, 0u);
        ASSERT_EQ("audio.stops.headroom-events", result.event_count, 0u);
        ASSERT_NAMED("audio.stops.headroom-preserves-payload",
                     storage.pens[0] == 0xA5u && storage.palettes[0] == 0xA5u &&
                     storage.line_ticks[0] == UINT64_C(0xA5A5A5A5A5A5A5A5) &&
                     storage.audio[0] == 0xA5u && storage.audio[256] == 0xA5u);
        memcpy(machine_before, box.machine, lynx_machine_size());
        av_result_init(&result);
        ASSERT_EQ("audio.stops.headroom-repeat", lynx_advance_av(
            box.machine, UINT64_MAX, &buffers, &result), LYNX_OK);
        ASSERT_EQ("audio.stops.headroom-repeat-reason", result.stop, LYNX_STOP_BUDGET);
        ASSERT_EQ("audio.stops.headroom-repeat-zero-progress", result.ticks_consumed, 0u);
        ASSERT_NAMED("audio.stops.headroom-repeat-machine",
                     memcmp(machine_before, box.machine, lynx_machine_size()) == 0);
        ASSERT_NAMED("audio.stops.headroom-repeat-preserves-payload",
                     storage.pens[0] == 0xA5u && storage.palettes[0] == 0xA5u &&
                     storage.line_ticks[0] == UINT64_C(0xA5A5A5A5A5A5A5A5) &&
                     storage.audio[0] == 0xA5u && storage.audio[256] == 0xA5u);
        free(machine_before);
    }
    machine_close(&box);

    ASSERT_NAMED("audio.stops.undefined-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    uint8_t undefined_opcode = 0u;
    bool found_undefined = false;
    for (uint32_t candidate = 0u; candidate <= UINT8_MAX; candidate++) {
        if (!lynx__cpu_opcode_defined(box.machine->cpu.profile, (uint8_t)candidate)) {
            undefined_opcode = (uint8_t)candidate;
            found_undefined = true;
            break;
        }
    }
    ASSERT_NAMED("audio.stops.undefined-opcode-available", found_undefined);
    if (!found_undefined) {
        machine_close(&box);
        return;
    }
    const uint8_t undefined_guest[1] = {undefined_opcode};
    ASSERT_EQ("audio.stops.undefined-load", load_guest(box.machine, undefined_guest,
                                                         sizeof undefined_guest), LYNX_OK);
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.stops.undefined-call", av_advance(box.machine, 20u,
                                                        &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.stops.undefined-reason", result.stop, LYNX_STOP_UNDEFINED);
    ASSERT_EQ("audio.stops.undefined-no-pcm", result.pcm_count, 0u);
    ASSERT_EQ("audio.stops.undefined-no-events", result.event_count, 0u);
    ASSERT_NAMED("audio.stops.undefined-no-stale-payload",
                 storage.audio[0] == 0xA5u && storage.audio[256] == 0xA5u);
    machine_close(&box);

    ASSERT_NAMED("audio.stops.unavailable-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.stops.unavailable-load", load_guest(
        box.machine, unavailable_guest, sizeof unavailable_guest), LYNX_OK);
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    ASSERT_EQ("audio.stops.unavailable-call", av_advance(box.machine, 100u,
                                                          &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.stops.unavailable-reason", result.stop, LYNX_STOP_UNAVAILABLE);
    ASSERT_EQ("audio.stops.unavailable-no-pcm", result.pcm_count, 0u);
    ASSERT_EQ("audio.stops.unavailable-no-events", result.event_count, 0u);
    ASSERT_NAMED("audio.stops.unavailable-no-stale-payload",
                 storage.audio[0] == 0xA5u && storage.audio[256] == 0xA5u);
    machine_close(&box);

    ASSERT_NAMED("audio.stops.no-content-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    check_av_refusal_unchanged(box.machine, &buffers, &result, &storage,
                               LYNX_ERR_STATE, "audio.stops.no-content-unchanged");
    machine_close(&box);
}

enum {
    AUDIO_SNAPSHOT_PAYLOAD_OFFSET = 65604u + LYNX__VIDEO_SNAPSHOT_LEN,
    AUDIO_SNAPSHOT_HASH_OFFSET = 220u
};

typedef struct audio_snapshot_edit {
    size_t field_offset;
    uint64_t value;
    size_t width;
} audio_snapshot_edit;

static void audio_snapshot_put_le(uint8_t *out, uint64_t value, size_t width)
{
    for (size_t i = 0u; i < width; i++) out[i] = (uint8_t)(value >> (i * 8u));
}

static void audio_snapshot_rehash(uint8_t *snapshot)
{
    lynx__sha256 hash;
    uint8_t digest[32];
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, snapshot + LYNX__SNAPSHOT_HEADER_LEN,
                        LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, digest);
    memcpy(snapshot + AUDIO_SNAPSHOT_HASH_OFFSET, digest, sizeof digest);
}

static size_t audio_snapshot_field(size_t field_offset)
{
    return (size_t)LYNX__SNAPSHOT_HEADER_LEN + AUDIO_SNAPSHOT_PAYLOAD_OFFSET +
           field_offset;
}

static bool audio_snapshot_watch_equal(const lynx_watch_event *left,
                                      const lynx_watch_event *right)
{
    return left->hit == right->hit && left->address == right->address &&
           left->value == right->value && left->hit_tick == right->hit_tick &&
           left->return_tick == right->return_tick;
}

static void audio_snapshot_expect_refused(
    lynx_machine *machine, const uint8_t *base, uint8_t *candidate,
    uint8_t *candidate_before, uint8_t *machine_before,
    size_t snapshot_size, size_t machine_size,
    const lynx_watch_event *watch_before, const audio_snapshot_edit *edits,
    size_t edit_count, const char *label)
{
    memcpy(candidate, base, snapshot_size);
    for (size_t i = 0u; i < edit_count; i++) {
        audio_snapshot_put_le(candidate + audio_snapshot_field(edits[i].field_offset),
                              edits[i].value, edits[i].width);
    }
    audio_snapshot_rehash(candidate);
    memcpy(candidate_before, candidate, snapshot_size);
    memcpy(machine_before, machine, machine_size);
    ASSERT_EQ(label, lynx_snapshot_restore(machine, candidate, snapshot_size),
              LYNX_ERR_SNAPSHOT_INVALID);
    ASSERT_NAMED(label, memcmp(machine_before, machine, machine_size) == 0);
    ASSERT_NAMED(label, memcmp(candidate_before, candidate, snapshot_size) == 0);
    lynx_watch_event watch_after;
    memset(&watch_after, 0xA5, sizeof watch_after);
    ASSERT_EQ(label, lynx_watch_get(machine, &watch_after), LYNX_OK);
    ASSERT_NAMED(label, audio_snapshot_watch_equal(watch_before, &watch_after));
}

static void case_snapshot_refusal(void)
{
    static const uint8_t watched_guest[] = {
        0xA9u, 0xFFu,             /* LDA #$FF */
        0x8Du, 0x20u, 0xFDu,      /* STA A0 volume */
        0x4Cu, 0x05u, 0x02u       /* JMP to itself */
    };
    machine_box box;
    ASSERT_NAMED("audio.snapshot_refusal.machine-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    ASSERT_EQ("audio.snapshot_refusal.guest-load",
              load_guest(box.machine, watched_guest, sizeof watched_guest), LYNX_OK);
    ASSERT_EQ("audio.snapshot_refusal.watch-configure",
              lynx_watch_configure(box.machine, 0xFD20u, 1u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    ASSERT_EQ("audio.snapshot_refusal.watch-advance",
              lynx_advance(box.machine, 1000u, &consumed, &stop), LYNX_OK);
    ASSERT_EQ("audio.snapshot_refusal.watch-stop", stop, LYNX_STOP_WATCH);
    lynx_watch_event watch_before;
    memset(&watch_before, 0, sizeof watch_before);
    ASSERT_EQ("audio.snapshot_refusal.watch-read",
              lynx_watch_get(box.machine, &watch_before), LYNX_OK);
    ASSERT_EQ("audio.snapshot_refusal.watch-hit", watch_before.hit, 1u);

    /* Use a fresh same-content blob so each field mutation differs from the
       original value regardless of the scheduler stage where the watch hit. */
    machine_box base_box;
    ASSERT_NAMED("audio.snapshot_refusal.base-init",
                 machine_open(&base_box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (base_box.machine == NULL) {
        machine_close(&box);
        return;
    }
    ASSERT_EQ("audio.snapshot_refusal.base-load",
              load_guest(base_box.machine, watched_guest, sizeof watched_guest), LYNX_OK);

    size_t snapshot_size = 0u;
    ASSERT_EQ("audio.snapshot_refusal.size",
              lynx_snapshot_size(base_box.machine, &snapshot_size), LYNX_OK);
    const size_t machine_size = lynx_machine_size();
    uint8_t *base = (uint8_t *)malloc(snapshot_size);
    uint8_t *candidate = (uint8_t *)malloc(snapshot_size);
    uint8_t *candidate_before = (uint8_t *)malloc(snapshot_size);
    uint8_t *machine_before = (uint8_t *)malloc(machine_size);
    ASSERT_NAMED("audio.snapshot_refusal.allocations",
                 base != NULL && candidate != NULL && candidate_before != NULL &&
                 machine_before != NULL);
    if (base == NULL || candidate == NULL || candidate_before == NULL ||
        machine_before == NULL) {
        free(machine_before);
        free(candidate_before);
        free(candidate);
        free(base);
        machine_close(&base_box);
        machine_close(&box);
        return;
    }
    size_t written = 0u;
    ASSERT_EQ("audio.snapshot_refusal.save",
              lynx_snapshot_save(base_box.machine, base, snapshot_size, &written), LYNX_OK);
    ASSERT_EQ("audio.snapshot_refusal.written", written, snapshot_size);
    const uint64_t ticks = base_box.machine->guest_ticks;
    audio_snapshot_edit edits[10];

#define TRY_AUDIO_SNAPSHOT_REFUSAL(label_value, edits_value) \
    audio_snapshot_expect_refused( \
        box.machine, base, candidate, candidate_before, machine_before, \
        snapshot_size, machine_size, &watch_before, (edits_value), \
        sizeof(edits_value) / sizeof((edits_value)[0]), (label_value))

    const audio_snapshot_edit ring_cursor[] = {{41u, LYNX__AUDIO_RING_NODE_COUNT, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.ring-cursor", ring_cursor);
    const audio_snapshot_edit shift_width[] = {{6u, 0x1000u, 2u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.shift-width", shift_width);
    const audio_snapshot_edit control_b_reserved[] = {{4u, 0x10u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.control-b-reserved",
                               control_b_reserved);
    const audio_snapshot_edit debt_without_deadline[] = {{42u, 1u, 8u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.debt-without-deadline",
                               debt_without_deadline);
    const audio_snapshot_edit high_debt_without_deadline[] = {{106u, 1u, 4u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.high-debt-without-deadline",
                               high_debt_without_deadline);
    const audio_snapshot_edit deferred_flag[] = {{110u, 2u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.deferred-flag", deferred_flag);
    const audio_snapshot_edit device_reserved[] = {{111u, 0x10u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.device-reserved", device_reserved);
    const audio_snapshot_edit device_visits[] = {{111u, 9u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.device-visits", device_visits);
    const audio_snapshot_edit sample_cursor_width[] = {{112u, 2u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.sample-cursor-width",
                               sample_cursor_width);
    const audio_snapshot_edit sample_cursor_off_lattice[] = {{112u, 1u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.sample-cursor-off-lattice",
                               sample_cursor_off_lattice);
    const audio_snapshot_edit event_cursor_width[] = {{113u, 2u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.event-cursor-width",
                               event_cursor_width);
    const audio_snapshot_edit reserved_114[] = {{114u, 1u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.reserved-114", reserved_114);
    const audio_snapshot_edit event_sequence_limit[] = {
        {113u, 1u, 1u}, {118u, LYNX__AUDIO_MAX_EVENTS_PER_TICK + 1u, 4u}
    };
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.event-sequence-limit",
                               event_sequence_limit);
    const audio_snapshot_edit event_tick[] = {
        {113u, 1u, 1u}, {122u, ticks + 1u, 8u}
    };
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.event-tick", event_tick);
    const audio_snapshot_edit event_sequence_without_cursor[] = {{118u, 1u, 4u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.event-sequence-without-cursor",
                               event_sequence_without_cursor);
    const audio_snapshot_edit next_tick_mismatch[] = {{130u, 800u, 8u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.next-tick-mismatch",
                               next_tick_mismatch);
    const audio_snapshot_edit next_ordinal_mismatch[] = {{138u, 1u, 8u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.next-ordinal-mismatch",
                               next_ordinal_mismatch);
    const audio_snapshot_edit far_future_sample[] = {
        {130u, 1200u, 8u}, {138u, 2u, 8u}
    };
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.far-future-sample",
                               far_future_sample);
    const audio_snapshot_edit defer_without_debt[] = {
        {146u, (uint64_t)ticks + 1u, 8u}
    };
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.defer-without-debt",
                               defer_without_debt);
    const audio_snapshot_edit reserved_154[] = {{154u, 1u, 1u}};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.reserved-154", reserved_154);

    for (size_t node = 0u; node < LYNX__AUDIO_RING_NODE_COUNT; node++) {
        edits[node].field_offset = 42u + node * 8u;
        edits[node].value = (UINT64_C(9) * ((uint64_t)ticks + 1u)) /
                           LYNX__AUDIO_RING_NODE_COUNT + 1u;
        edits[node].width = 8u;
    }
    edits[8] = (audio_snapshot_edit){110u, 1u, 1u};
    edits[9] = (audio_snapshot_edit){146u, (uint64_t)ticks, 8u};
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.aggregate-debt", edits);

    const audio_snapshot_edit delayed_without_exhaustion[] = {
        {42u, 1u, 8u}, {110u, 1u, 1u},
        {111u, 0u, 1u}, {146u, (uint64_t)ticks + 1u, 8u}
    };
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.deferred-before-exhaustion",
                               delayed_without_exhaustion);
    const audio_snapshot_edit deadline_too_far[] = {
        {42u, 1u, 8u}, {110u, 1u, 1u},
        {111u, LYNX__AUDIO_RING_VISIT_LIMIT, 1u},
        {146u, (uint64_t)ticks + 2u, 8u}
    };
    TRY_AUDIO_SNAPSHOT_REFUSAL("audio.snapshot_refusal.deadline-too-far",
                               deadline_too_far);

#undef TRY_AUDIO_SNAPSHOT_REFUSAL

    free(machine_before);
    free(candidate_before);
    free(candidate);
    free(base);
    machine_close(&base_box);
    machine_close(&box);

    /* At the sample tick, the pre-sample phase is valid. Setting the applied
       cursor without advancing ordinal/tick would duplicate the frame. */
    ASSERT_NAMED("audio.snapshot_refusal.phase-init",
                 machine_open(&box, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (box.machine == NULL) return;
    static const uint8_t looping_guest[] = {0x4Cu, 0x00u, 0x02u};
    ASSERT_EQ("audio.snapshot_refusal.phase-load", load_guest(
        box.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    av_span_storage storage;
    lynx_av_buffers buffers;
    lynx_av_result result;
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    ASSERT_EQ("audio.snapshot_refusal.phase-advance",
              lynx_advance_av(box.machine, 400u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.snapshot_refusal.phase-tick", box.machine->guest_ticks, 400u);
    size_t phase_size = 0u;
    ASSERT_EQ("audio.snapshot_refusal.phase-size",
              lynx_snapshot_size(box.machine, &phase_size), LYNX_OK);
    uint8_t *phase_base = (uint8_t *)malloc(phase_size);
    uint8_t *phase_bad = (uint8_t *)malloc(phase_size);
    uint8_t *phase_before = (uint8_t *)malloc(phase_size);
    uint8_t *phase_machine_before = (uint8_t *)malloc(lynx_machine_size());
    ASSERT_NAMED("audio.snapshot_refusal.phase-allocations",
                 phase_base != NULL && phase_bad != NULL && phase_before != NULL &&
                 phase_machine_before != NULL);
    if (phase_base != NULL && phase_bad != NULL && phase_before != NULL &&
        phase_machine_before != NULL) {
        written = 0u;
        ASSERT_EQ("audio.snapshot_refusal.phase-save", lynx_snapshot_save(
            box.machine, phase_base, phase_size, &written), LYNX_OK);
        const audio_snapshot_edit duplicate_sample[] = {{112u, 1u, 1u}};
        lynx_watch_event no_watch;
        memset(&no_watch, 0, sizeof no_watch);
        ASSERT_EQ("audio.snapshot_refusal.phase-no-watch",
                  lynx_watch_get(box.machine, &no_watch), LYNX_OK);
        audio_snapshot_expect_refused(box.machine, phase_base, phase_bad,
                                      phase_before, phase_machine_before,
                                      phase_size, lynx_machine_size(), &no_watch,
                                      duplicate_sample, 1u,
                                      "audio.snapshot_refusal.applied-sample-not-advanced");
        free(phase_machine_before);
        free(phase_before);
        free(phase_bad);
        free(phase_base);
    } else {
        free(phase_machine_before);
        free(phase_before);
        free(phase_bad);
        free(phase_base);
    }
    machine_close(&box);
}

static void case_snapshot(void)
{
    static const uint8_t looping_guest[] = {0x4Cu, 0x00u, 0x02u};
    machine_box source;
    machine_box restored;
    ASSERT_NAMED("audio.snapshot.source-init",
                 machine_open(&source, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    ASSERT_NAMED("audio.snapshot.restored-init",
                 machine_open(&restored, LYNX_TIMING_POLICY_VIDEO_POLICY1));
    if (source.machine == NULL || restored.machine == NULL) {
        machine_close(&restored);
        machine_close(&source);
        return;
    }
    ASSERT_EQ("audio.snapshot.source-load",
              load_guest(source.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    ASSERT_EQ("audio.snapshot.restored-load",
              load_guest(restored.machine, looping_guest, sizeof looping_guest), LYNX_OK);

    lynx_identity identity;
    memset(&identity, 0, sizeof identity);
    ASSERT_EQ("audio.snapshot.identity", lynx_get_identity(source.machine, &identity), LYNX_OK);
    ASSERT_EQ("audio.snapshot.identity-policy", identity.audio_policy,
              LYNX_AUDIO_POLICY_DIGITAL_1);
    ASSERT_EQ("audio.snapshot.identity-rate-num", identity.audio_rate_num,
              LYNX_AUDIO_RATE_NUM);
    ASSERT_EQ("audio.snapshot.identity-rate-den", identity.audio_rate_den,
              LYNX_AUDIO_RATE_DEN);
    ASSERT_EQ("audio.snapshot.identity-channels", identity.audio_channels,
              LYNX_AUDIO_PCM_CHANNELS);
    ASSERT_EQ("audio.snapshot.identity-bits", identity.audio_bits, LYNX_AUDIO_PCM_BITS);

    /* A completed device application may leave a valid event cursor with no
       emitted events yet; sequence zero is reachable at that scheduler stage. */
    (void)lynx__audio_apply(&source.machine->audio, source.machine->guest_ticks, NULL);
    ASSERT_EQ("audio.snapshot.empty-event-cursor-valid",
              source.machine->audio.event_cursor_valid, 1u);
    ASSERT_EQ("audio.snapshot.empty-event-cursor-sequence",
              source.machine->audio.event_sequence, 0u);

    /* Raw guest latches retain their complete byte domains. The format only
       constrains bits that are structurally reserved by the serializer. */
    lynx__audio_channel *channel = &source.machine->audio.channels[0];
    channel->volume = UINT8_MAX;
    channel->backup = UINT8_MAX;
    channel->count = UINT8_MAX;
    channel->control_a = UINT8_MAX;
    channel->feedback = UINT8_MAX;
    channel->shift = 0x0FFFu;
    channel->control_b = 0x0Fu;
    channel->output = INT8_MIN;
    channel->integrator = INT8_MAX;
    source.machine->audio.stereo_disable = UINT8_MAX;

    size_t snapshot_size = 0u;
    ASSERT_EQ("audio.snapshot.size", lynx_snapshot_size(source.machine, &snapshot_size),
              LYNX_OK);
    uint8_t *snapshot = (uint8_t *)malloc(snapshot_size);
    uint8_t *roundtrip = (uint8_t *)malloc(snapshot_size);
    ASSERT_NAMED("audio.snapshot.allocations", snapshot != NULL && roundtrip != NULL);
    if (snapshot == NULL || roundtrip == NULL) {
        free(roundtrip);
        free(snapshot);
        machine_close(&restored);
        machine_close(&source);
        return;
    }
    size_t written = 0u;
    ASSERT_EQ("audio.snapshot.save",
              lynx_snapshot_save(source.machine, snapshot, snapshot_size, &written), LYNX_OK);
    ASSERT_EQ("audio.snapshot.written", written, snapshot_size);
    ASSERT_EQ("audio.snapshot.restore",
              lynx_snapshot_restore(restored.machine, snapshot, snapshot_size), LYNX_OK);
    ASSERT_EQ("audio.snapshot.resave", lynx_snapshot_save(
        restored.machine, roundtrip, snapshot_size, &written), LYNX_OK);
    ASSERT_NAMED("audio.snapshot.fieldwise-roundtrip",
                 memcmp(snapshot, roundtrip, snapshot_size) == 0);
    ASSERT_EQ("audio.snapshot.channel-volume", restored.machine->audio.channels[0].volume,
              UINT8_MAX);
    ASSERT_EQ("audio.snapshot.channel-backup", restored.machine->audio.channels[0].backup,
              UINT8_MAX);
    ASSERT_EQ("audio.snapshot.channel-count", restored.machine->audio.channels[0].count,
              UINT8_MAX);
    ASSERT_EQ("audio.snapshot.channel-control-a",
              restored.machine->audio.channels[0].control_a, UINT8_MAX);
    ASSERT_EQ("audio.snapshot.channel-feedback",
              restored.machine->audio.channels[0].feedback, UINT8_MAX);
    ASSERT_EQ("audio.snapshot.channel-shift", restored.machine->audio.channels[0].shift,
              0x0FFFu);
    ASSERT_EQ("audio.snapshot.channel-control-b",
              restored.machine->audio.channels[0].control_b, 0x0Fu);
    ASSERT_EQ("audio.snapshot.channel-output", restored.machine->audio.channels[0].output,
              INT8_MIN);
    ASSERT_EQ("audio.snapshot.channel-integrator",
              restored.machine->audio.channels[0].integrator, INT8_MAX);
    ASSERT_EQ("audio.snapshot.stereo-disable", restored.machine->audio.stereo_disable,
              UINT8_MAX);
    free(roundtrip);
    free(snapshot);

    /* A pending deferred ring suffix and an already emitted sample are a
       valid continuation exactly at the sample tick. */
    ASSERT_EQ("audio.snapshot.reload-source",
              load_guest(source.machine, looping_guest, sizeof looping_guest), LYNX_OK);
    av_span_storage storage;
    lynx_av_buffers buffers;
    lynx_av_result result;
    av_span_storage_init(&storage, &buffers, LYNX_AV_MIN_PCM_CAPACITY,
                         LYNX_AV_MIN_EVENT_CAPACITY);
    av_result_init(&result);
    ASSERT_EQ("audio.snapshot.advance-to-sample", lynx_advance_av(
        source.machine, 400u, &buffers, &result), LYNX_OK);
    ASSERT_EQ("audio.snapshot.sample-boundary-stop", result.stop, LYNX_STOP_BUDGET);
    ASSERT_EQ("audio.snapshot.sample-boundary-tick", source.machine->guest_ticks, 400u);
    source.machine->audio.ring_debt[4] = 1u;
    source.machine->audio.ring_cursor = 4u;
    source.machine->audio.ring_defer_valid = 1u;
    source.machine->audio.ring_defer_tick = 401u;
    source.machine->audio.same_tick_device_cursor = LYNX__AUDIO_DEVICE_APPLIED |
                                                    LYNX__AUDIO_RING_VISIT_LIMIT;
    lynx__audio_mark_sample(&source.machine->audio);
    ASSERT_NAMED("audio.snapshot.deferred-ring-valid",
                 lynx__audio_ring_state_valid(&source.machine->audio, 400u));
    ASSERT_EQ("audio.snapshot.deferred-sample-tick", source.machine->audio.next_sample_tick,
              800u);
    ASSERT_EQ("audio.snapshot.deferred-sample-ordinal",
              source.machine->audio.next_sample_ordinal, 1u);
    snapshot_size = 0u;
    ASSERT_EQ("audio.snapshot.deferred-size",
              lynx_snapshot_size(source.machine, &snapshot_size), LYNX_OK);
    snapshot = (uint8_t *)malloc(snapshot_size);
    roundtrip = (uint8_t *)malloc(snapshot_size);
    ASSERT_NAMED("audio.snapshot.deferred-allocations",
                 snapshot != NULL && roundtrip != NULL);
    if (snapshot != NULL && roundtrip != NULL) {
        written = 0u;
        ASSERT_EQ("audio.snapshot.deferred-save", lynx_snapshot_save(
            source.machine, snapshot, snapshot_size, &written), LYNX_OK);
        ASSERT_EQ("audio.snapshot.deferred-restore", lynx_snapshot_restore(
            restored.machine, snapshot, snapshot_size), LYNX_OK);
        ASSERT_EQ("audio.snapshot.deferred-resave", lynx_snapshot_save(
            restored.machine, roundtrip, snapshot_size, &written), LYNX_OK);
        ASSERT_NAMED("audio.snapshot.deferred-roundtrip",
                     memcmp(snapshot, roundtrip, snapshot_size) == 0);
        ASSERT_EQ("audio.snapshot.deferred-debt", restored.machine->audio.ring_debt[4], 1u);
        ASSERT_EQ("audio.snapshot.deferred-deadline",
                  restored.machine->audio.ring_defer_tick, 401u);
        ASSERT_EQ("audio.snapshot.deferred-sample-cursor",
                  restored.machine->audio.same_tick_sample_cursor, 1u);
        free(roundtrip);
        free(snapshot);
    } else {
        free(roundtrip);
        free(snapshot);
    }
    machine_close(&restored);
    machine_close(&source);
}

typedef struct test_case {
    const char *name;
    void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"cadence", case_cadence},
    {"registers", case_registers},
    {"lfsr", case_lfsr},
    {"counter", case_counter},
    {"integrator", case_integrator},
    {"stereo", case_stereo},
    {"mix", case_mix},
    {"order", case_order},
    {"links", case_links},
    {"ring_zero", case_ring_zero},
    {"span_refusal", case_span_refusal},
    {"span_alias", case_span_alias},
    {"capacity", case_capacity},
    {"stops", case_stops},
    {"snapshot", case_snapshot},
    {"snapshot_refusal", case_snapshot_refusal},
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        (void)fprintf(stderr, "usage: lynx-test-audio <cadence|registers|lfsr|counter|integrator|stereo|mix|order|links|ring_zero|span_refusal|span_alias|capacity|stops|snapshot|snapshot_refusal>\n");
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
