#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "lynxplosion.h"

static void append(uint8_t *program, size_t *length, uint8_t byte)
{
    program[*length] = byte;
    *length += 1u;
}

static size_t make_guest(uint8_t *program, uint16_t load_addr)
{
    size_t length = 0u;
#define EMIT(v) append(program, &length, (uint8_t)(v))
    EMIT(0x78u);                            /* SEI */
    EMIT(0xA9u); EMIT(0xFFu);              /* LDA #$FF */
    EMIT(0x8Du); EMIT(0x52u); EMIT(0xFCu); /* STA $FC52: D, CD low */
    EMIT(0xA9u); EMIT(0x00u);              /* LDA #$00 */
    EMIT(0x8Du); EMIT(0x53u); EMIT(0xFCu); /* STA $FC53: C, CD high */
    EMIT(0xA9u); EMIT(0x02u);              /* LDA #$02 */
    EMIT(0x8Du); EMIT(0x54u); EMIT(0xFCu); /* STA $FC54: AB low */
    EMIT(0xA9u); EMIT(0x00u);              /* LDA #$00 */
    EMIT(0x8Du); EMIT(0x55u); EMIT(0xFCu); /* STA $FC55: AB high starts multiply */
    EMIT(0xADu); EMIT(0x92u); EMIT(0xFCu); /* LDA $FC92: capture SPRSYS busy */
    EMIT(0x8Du); EMIT(0x00u); EMIT(0x04u); /* STA $0400 */
    EMIT(0xA2u); EMIT(0x00u);              /* LDX #0 */
    EMIT(0xE8u);                           /* wait: INX */
    EMIT(0xD0u); EMIT(0xFDu);              /* BNE wait */
    for (uint16_t offset = 0u; offset < 4u; offset++) {
        EMIT(0xADu); EMIT(0x60u + offset); EMIT(0xFCu); /* LDA H/G/F/E */
        EMIT(0x8Du); EMIT((uint8_t)(0x01u + offset)); EMIT(0x04u); /* STA $0401..4 */
    }
    const uint16_t loop_addr = (uint16_t)(load_addr + (uint16_t)length);
    EMIT(0x4Cu); EMIT((uint8_t)loop_addr); EMIT((uint8_t)(loop_addr >> 8)); /* JMP loop */
#undef EMIT
    return length;
}

static lynx_machine *make_machine(uint32_t profile, void **allocation)
{
    const size_t alignment = lynx_machine_align();
    const size_t size = lynx_machine_size();
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u || size == 0u ||
        size > SIZE_MAX - (alignment - 1u)) {
        CHECK(0 && "machine size/alignment must be valid");
        return NULL;
    }
    const size_t allocated_size = (size + alignment - 1u) & ~(alignment - 1u);
    *allocation = aligned_alloc(alignment, allocated_size);
    if (*allocation == NULL) {
        CHECK(0 && "machine allocation must succeed");
        return NULL;
    }
    lynx_config config;
    lynx_config_default(&config);
    config.profile = profile;
    lynx_machine *machine = NULL;
    CHECK_EQ_U64(lynx_machine_init(*allocation, size, &config, &machine), LYNX_OK);
    return machine;
}

static void run_profile(uint32_t profile)
{
    enum { LOAD_ADDRESS = 0x0200u, OBJECT_HEADER = 10u, PROGRAM_CAPACITY = 128u };
    uint8_t program[PROGRAM_CAPACITY];
    const size_t program_len = make_guest(program, LOAD_ADDRESS);
    uint8_t object[OBJECT_HEADER + PROGRAM_CAPACITY];
    const size_t object_len = OBJECT_HEADER + program_len;
    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = (uint8_t)(LOAD_ADDRESS >> 8);
    object[3] = (uint8_t)LOAD_ADDRESS;
    object[4] = (uint8_t)(object_len >> 8);
    object[5] = (uint8_t)object_len;
    object[6] = 'B';
    object[7] = 'S';
    object[8] = '9';
    object[9] = '3';
    for (size_t i = 0u; i < program_len; i++) {
        object[OBJECT_HEADER + i] = program[i];
    }

    void *allocation = NULL;
    lynx_machine *machine = make_machine(profile, &allocation);
    if (machine == NULL) {
        free(allocation);
        return;
    }
    CHECK_EQ_U64(lynx_load_bll(machine, object, object_len), LYNX_OK);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    const lynx_status advance_status = lynx_advance(machine, 12000u, &consumed, &stop);
    CHECK_EQ_U64(advance_status, LYNX_OK);
    if (stop == LYNX_STOP_UNAVAILABLE) {
        fprintf(stderr, "semantic failure: profile %s refused guest access at $%04X\n",
                lynx_profile_name(profile), 0xFC52u);
    }
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);

    uint8_t guest_result[5] = {0u};
    CHECK_EQ_U64(lynx_read_memory(machine, 0x0400u, guest_result, sizeof guest_result), LYNX_OK);
    CHECK((guest_result[0] & 0x80u) != 0u);
    CHECK_EQ_HEX(guest_result[1], 0xFEu);
    CHECK_EQ_HEX(guest_result[2], 0x01u);
    CHECK_EQ_HEX(guest_result[3], 0x00u);
    CHECK_EQ_HEX(guest_result[4], 0x00u);

    uint8_t mapped_product[4] = {0u};
    CHECK_EQ_U64(lynx_read_cpu_memory(machine, 0xFC60u, mapped_product,
                                      sizeof mapped_product), LYNX_OK);
    CHECK_EQ_HEX(mapped_product[0], 0xFEu);
    CHECK_EQ_HEX(mapped_product[1], 0x01u);
    CHECK_EQ_HEX(mapped_product[2], 0x00u);
    CHECK_EQ_HEX(mapped_product[3], 0x00u);
    lynx_machine_deinit(machine);
    free(allocation);
}

static int case_tracer(void)
{
    run_profile(LYNX_PROFILE_LYNX1);
    run_profile(LYNX_PROFILE_LYNX2);
    return check_failures == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1] == NULL || argv[1][0] == '\0') {
        fprintf(stderr, "usage: lynx-test-suzy <case>\n");
        return 2;
    }
    if (strcmp(argv[1], "tracer") != 0) {
        fprintf(stderr, "lynx-test-suzy: unknown case '%s'\n", argv[1]);
        return 2;
    }
    printf("TAP version 13\n1..1\n");
    const int result = case_tracer();
    printf("%s 1 - suzy.tracer completes mapped unsigned multiply\n",
           result == 0 ? "ok" : "not ok");
    return result;
}
