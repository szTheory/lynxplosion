#include <stdint.h>
#include <stdbool.h>
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
    EMIT(0xADu); EMIT(0x92u); EMIT(0xFCu); /* capture post-operation unsafe */
    EMIT(0x8Du); EMIT(0x05u); EMIT(0x04u); /* STA $0405 */
    EMIT(0xA9u); EMIT(0x04u);              /* LDA #clear unsafe */
    EMIT(0x8Du); EMIT(0x92u); EMIT(0xFCu); /* STA SPRSYS */
    const uint16_t loop_addr = (uint16_t)(load_addr + (uint16_t)length);
    EMIT(0x4Cu); EMIT((uint8_t)loop_addr); EMIT((uint8_t)(loop_addr >> 8)); /* JMP loop */
#undef EMIT
    return length;
}

static size_t make_object(uint8_t object[138])
{
    enum { LOAD_ADDRESS = 0x0200u, OBJECT_HEADER = 10u, PROGRAM_CAPACITY = 128u };
    uint8_t program[PROGRAM_CAPACITY];
    const size_t program_len = make_guest(program, LOAD_ADDRESS);
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
    memcpy(object + OBJECT_HEADER, program, program_len);
    return object_len;
}

static size_t emit_write(uint8_t *program, size_t *length, uint16_t addr, uint8_t value)
{
    append(program, length, 0xA9u); /* LDA #value */
    append(program, length, value);
    append(program, length, 0x8Du); /* STA address */
    append(program, length, (uint8_t)addr);
    append(program, length, (uint8_t)(addr >> 8));
    return *length;
}

static size_t make_arithmetic_guest(uint8_t *program, uint16_t load_addr)
{
    size_t length = 0u;
    append(program, &length, 0x78u); /* SEI */
    (void)emit_write(program, &length, 0xFC92u, 0x80u); /* signed mode */
    (void)emit_write(program, &length, 0xFC52u, 0xFDu); /* CD low */
    (void)emit_write(program, &length, 0xFC53u, 0xFFu); /* explicit CD high/sign */
    (void)emit_write(program, &length, 0xFC54u, 0x05u); /* AB low */
    (void)emit_write(program, &length, 0xFC55u, 0x00u); /* explicit AB high/start */
    const uint16_t loop_addr = (uint16_t)(load_addr + (uint16_t)length);
    append(program, &length, 0x4Cu); /* JMP loop */
    append(program, &length, (uint8_t)loop_addr);
    append(program, &length, (uint8_t)(loop_addr >> 8));
    return length;
}

static size_t make_program_object(uint8_t *object, size_t capacity,
                                 const uint8_t *program, size_t program_len,
                                 uint16_t load_addr)
{
    const size_t header_len = 10u;
    const size_t object_len = header_len + program_len;
    if (object_len > capacity || object_len > UINT16_MAX) return 0u;
    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = (uint8_t)(load_addr >> 8);
    object[3] = (uint8_t)load_addr;
    object[4] = (uint8_t)(object_len >> 8);
    object[5] = (uint8_t)object_len;
    object[6] = 'B';
    object[7] = 'S';
    object[8] = '9';
    object[9] = '3';
    memcpy(object + header_len, program, program_len);
    return object_len;
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

static uint8_t read_sprsys(const lynx_machine *machine);

static void run_profile(uint32_t profile)
{
    uint8_t object[138];
    const size_t object_len = make_object(object);

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

    uint8_t guest_result[6] = {0u};
    CHECK_EQ_U64(lynx_read_memory(machine, 0x0400u, guest_result, sizeof guest_result), LYNX_OK);
    CHECK((guest_result[0] & 0x80u) != 0u);
    CHECK_EQ_HEX(guest_result[1], 0xFEu);
    CHECK_EQ_HEX(guest_result[2], 0x01u);
    CHECK_EQ_HEX(guest_result[3], 0x00u);
    CHECK_EQ_HEX(guest_result[4], 0x00u);
    CHECK((guest_result[5] & 0x04u) != 0u);
    CHECK((read_sprsys(machine) & 0x04u) == 0u);

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

static uint8_t read_sprsys(const lynx_machine *machine)
{
    uint8_t value = 0u;
    CHECK_EQ_U64(lynx_read_cpu_memory(machine, 0xFC92u, &value, 1u), LYNX_OK);
    return value;
}

static void run_snapshot_profile(uint32_t profile)
{
    uint8_t object[138];
    const size_t object_len = make_object(object);
    void *source_allocation = NULL;
    lynx_machine *source = make_machine(profile, &source_allocation);
    if (source == NULL) {
        free(source_allocation);
        return;
    }
    CHECK_EQ_U64(lynx_load_bll(source, object, object_len), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(source, 0xFC55u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(source, 2000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    lynx_watch_event event;
    memset(&event, 0, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(source, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0xFC55u);
    CHECK_EQ_HEX(event.value, 0u);
    CHECK_EQ_U64(event.return_tick, event.hit_tick + 1u);
    CHECK((read_sprsys(source) & 0x80u) != 0u);

    size_t snapshot_size = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(source, &snapshot_size), LYNX_OK);
    uint8_t *snapshot = malloc(snapshot_size);
    CHECK(snapshot != NULL);
    if (snapshot == NULL) {
        lynx_machine_deinit(source);
        free(source_allocation);
        return;
    }
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_save(source, snapshot, snapshot_size, &written), LYNX_OK);
    CHECK_EQ_U64(written, snapshot_size);

    void *restored_allocation = NULL;
    lynx_machine *restored = make_machine(profile, &restored_allocation);
    if (restored == NULL) {
        free(snapshot);
        lynx_machine_deinit(source);
        free(source_allocation);
        free(restored_allocation);
        return;
    }
    CHECK_EQ_U64(lynx_load_bll(restored, object, object_len), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(restored, snapshot, snapshot_size), LYNX_OK);
    CHECK((read_sprsys(restored) & 0x80u) != 0u);

    uint8_t source_digest[32];
    uint8_t restored_digest[32];
    CHECK_EQ_U64(lynx_state_digest(source, source_digest), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(restored, restored_digest), LYNX_OK);
    CHECK(memcmp(source_digest, restored_digest, sizeof source_digest) == 0);

    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(source, &now), LYNX_OK);
    const uint64_t deadline = event.hit_tick + 44u;
    CHECK(deadline > now);
    const uint64_t to_deadline = deadline - now;
    CHECK_EQ_U64(lynx_advance(source, to_deadline, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, to_deadline);
    CHECK_EQ_U64(lynx_advance(restored, to_deadline, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, to_deadline);
    CHECK_EQ_U64(lynx_guest_ticks(source, &now), LYNX_OK);
    CHECK_EQ_U64(now, deadline);
    CHECK((read_sprsys(source) & 0x80u) != 0u);
    CHECK((read_sprsys(restored) & 0x80u) != 0u);

    CHECK_EQ_U64(lynx_advance(source, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(lynx_advance(restored, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 1u);
    CHECK((read_sprsys(source) & 0x80u) == 0u);
    CHECK((read_sprsys(restored) & 0x80u) == 0u);
    CHECK((read_sprsys(source) & 0x04u) != 0u);
    CHECK((read_sprsys(restored) & 0x04u) != 0u);

    CHECK_EQ_U64(lynx_advance(source, 12000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 12000u);
    CHECK_EQ_U64(lynx_advance(restored, 12000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 12000u);

    uint8_t guest_result[6] = {0u};
    CHECK_EQ_U64(lynx_read_memory(source, 0x0400u, guest_result,
                                  sizeof guest_result), LYNX_OK);
    CHECK((guest_result[0] & 0x80u) != 0u);
    CHECK_EQ_HEX(guest_result[1], 0xFEu);
    CHECK_EQ_HEX(guest_result[2], 0x01u);
    CHECK_EQ_HEX(guest_result[3], 0x00u);
    CHECK_EQ_HEX(guest_result[4], 0x00u);
    CHECK((guest_result[5] & 0x04u) != 0u);
    CHECK((read_sprsys(source) & 0x04u) == 0u);

    uint8_t *source_ram = malloc(LYNX_ADDRESS_SPACE);
    uint8_t *restored_ram = malloc(LYNX_ADDRESS_SPACE);
    CHECK(source_ram != NULL && restored_ram != NULL);
    if (source_ram != NULL && restored_ram != NULL) {
        CHECK_EQ_U64(lynx_read_memory(source, 0u, source_ram, LYNX_ADDRESS_SPACE), LYNX_OK);
        CHECK_EQ_U64(lynx_read_memory(restored, 0u, restored_ram,
                                      LYNX_ADDRESS_SPACE), LYNX_OK);
        CHECK(memcmp(source_ram, restored_ram, LYNX_ADDRESS_SPACE) == 0);
    }
    CHECK_EQ_U64(lynx_state_digest(source, source_digest), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(restored, restored_digest), LYNX_OK);
    CHECK(memcmp(source_digest, restored_digest, sizeof source_digest) == 0);
    uint8_t *source_final = malloc(snapshot_size);
    uint8_t *restored_final = malloc(snapshot_size);
    CHECK(source_final != NULL && restored_final != NULL);
    if (source_final != NULL && restored_final != NULL) {
        CHECK_EQ_U64(lynx_snapshot_save(source, source_final, snapshot_size, &written), LYNX_OK);
        CHECK_EQ_U64(lynx_snapshot_save(restored, restored_final, snapshot_size, &written), LYNX_OK);
        CHECK(memcmp(source_final, restored_final, snapshot_size) == 0);
    }
    free(source_final);
    free(restored_final);
    free(source_ram);
    free(restored_ram);
    free(snapshot);
    lynx_machine_deinit(restored);
    lynx_machine_deinit(source);
    free(restored_allocation);
    free(source_allocation);
}

static int case_tracer(void)
{
    run_profile(LYNX_PROFILE_LYNX1);
    run_profile(LYNX_PROFILE_LYNX2);
    return check_failures == 0 ? 0 : 1;
}

static int case_snapshot_tracer(void)
{
    run_snapshot_profile(LYNX_PROFILE_LYNX1);
    run_snapshot_profile(LYNX_PROFILE_LYNX2);
    return check_failures == 0 ? 0 : 1;
}

static void run_signed_arithmetic_profile(uint32_t profile)
{
    enum { LOAD_ADDRESS = 0x0200u };
    uint8_t program[64];
    uint8_t object[128];
    const size_t program_len = make_arithmetic_guest(program, LOAD_ADDRESS);
    const size_t object_len = make_program_object(object, sizeof object, program,
                                                   program_len, LOAD_ADDRESS);
    CHECK(object_len != 0u);
    void *allocation = NULL;
    lynx_machine *machine = make_machine(profile, &allocation);
    if (machine == NULL) {
        free(allocation);
        return;
    }
    CHECK_EQ_U64(lynx_load_bll(machine, object, object_len), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    CHECK_EQ_U64(lynx_advance(machine, 12000u, &consumed, &stop), LYNX_OK);
    if (stop != LYNX_STOP_BUDGET) {
        fprintf(stderr, "suzy.arithmetic: signed mapped multiply must complete; stop=%u tick=%llu\n",
                (unsigned)stop, (unsigned long long)consumed);
    }
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 12000u);
    uint8_t product[4] = {0u};
    CHECK_EQ_U64(lynx_read_cpu_memory(machine, 0xFC60u, product, sizeof product), LYNX_OK);
    CHECK_EQ_HEX(product[0], 0xF1u);
    CHECK_EQ_HEX(product[1], 0xFFu);
    CHECK_EQ_HEX(product[2], 0xFFu);
    CHECK_EQ_HEX(product[3], 0xFFu);
    lynx_machine_deinit(machine);
    free(allocation);
}

static int case_arithmetic(void)
{
    run_signed_arithmetic_profile(LYNX_PROFILE_LYNX1);
    run_signed_arithmetic_profile(LYNX_PROFILE_LYNX2);
    return check_failures == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1] == NULL || argv[1][0] == '\0') {
        fprintf(stderr, "usage: lynx-test-suzy <case>\n");
        return 2;
    }
    if (strcmp(argv[1], "tracer") != 0 && strcmp(argv[1], "snapshot_tracer") != 0 &&
        strcmp(argv[1], "arithmetic") != 0) {
        fprintf(stderr, "lynx-test-suzy: unknown case '%s'\n", argv[1]);
        return 2;
    }
    printf("TAP version 13\n1..1\n");
    int result = 0;
    if (strcmp(argv[1], "snapshot_tracer") == 0) result = case_snapshot_tracer();
    else if (strcmp(argv[1], "arithmetic") == 0) result = case_arithmetic();
    else result = case_tracer();
    printf("%s 1 - suzy.%s\n", result == 0 ? "ok" : "not ok", argv[1]);
    return result;
}
