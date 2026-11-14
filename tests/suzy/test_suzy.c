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

typedef struct suzy_write {
    uint16_t addr;
    uint8_t value;
} suzy_write;

typedef struct suzy_live_machine {
    lynx_machine *machine;
    void *allocation;
} suzy_live_machine;

static lynx_machine *make_machine(uint32_t profile, void **allocation);

static size_t make_guest_writes(uint8_t *program, uint16_t load_addr,
                                const suzy_write *writes, size_t write_count)
{
    size_t length = 0u;
    append(program, &length, 0x78u); /* SEI */
    for (size_t i = 0u; i < write_count; i++) {
        (void)emit_write(program, &length, writes[i].addr, writes[i].value);
        if (writes[i].addr == 0xFC55u || writes[i].addr == 0xFC63u) {
            append(program, &length, 0xA2u); append(program, &length, 0x00u); /* LDX #0 */
            append(program, &length, 0xE8u); /* wait: INX */
            append(program, &length, 0xD0u); append(program, &length, 0xFDu); /* BNE wait */
        }
    }
    const uint16_t loop_addr = (uint16_t)(load_addr + (uint16_t)length);
    append(program, &length, 0x4Cu); /* JMP loop */
    append(program, &length, (uint8_t)loop_addr);
    append(program, &length, (uint8_t)(loop_addr >> 8));
    return length;
}

static suzy_live_machine run_guest_writes(uint32_t profile, const suzy_write *writes,
                                          size_t write_count)
{
    enum { LOAD_ADDRESS = 0x0200u };
    uint8_t program[4096];
    uint8_t object[4106];
    const size_t program_len = make_guest_writes(program, LOAD_ADDRESS, writes, write_count);
    const size_t object_len = make_program_object(object, sizeof object, program,
                                                   program_len, LOAD_ADDRESS);
    CHECK(object_len != 0u);
    suzy_live_machine live = {NULL, NULL};
    live.machine = make_machine(profile, &live.allocation);
    if (live.machine == NULL) return live;
    CHECK_EQ_U64(lynx_load_bll(live.machine, object, object_len), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    CHECK_EQ_U64(lynx_advance(live.machine, 12000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 12000u);
    return live;
}

static void destroy_live_machine(suzy_live_machine *live)
{
    if (live->machine != NULL) lynx_machine_deinit(live->machine);
    free(live->allocation);
    live->machine = NULL;
    live->allocation = NULL;
}

static void peek_bytes(const lynx_machine *machine, uint16_t addr,
                       uint8_t *out, size_t length)
{
    CHECK_EQ_U64(lynx_read_cpu_memory(machine, addr, out, length), LYNX_OK);
}

static void expect_u32_bytes(const lynx_machine *machine, uint16_t addr, uint32_t value,
                             const char *invariant)
{
    uint8_t actual[4] = {0u};
    peek_bytes(machine, addr, actual, sizeof actual);
    for (uint32_t i = 0u; i < 4u; i++) {
        if (actual[i] != (uint8_t)(value >> (8u * i))) {
            fprintf(stderr, "suzy semantic failure: %s byte %u: got %02X expected %02X\n",
                    invariant, (unsigned)i, (unsigned)actual[i],
                    (unsigned)(uint8_t)(value >> (8u * i)));
        }
        CHECK_EQ_HEX(actual[i], (uint8_t)(value >> (8u * i)));
    }
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

static void run_unsigned_vector(uint32_t profile, uint16_t cd, uint16_t ab)
{
    const suzy_write writes[] = {
        {0xFC92u, 0x00u}, {0xFC52u, (uint8_t)cd},
        {0xFC53u, (uint8_t)(cd >> 8)}, {0xFC54u, (uint8_t)ab},
        {0xFC55u, (uint8_t)(ab >> 8)}
    };
    suzy_live_machine live = run_guest_writes(profile, writes,
                                               sizeof writes / sizeof writes[0]);
    if (live.machine != NULL) {
        const uint32_t expected = (uint32_t)((uint64_t)cd * (uint64_t)ab);
        expect_u32_bytes(live.machine, 0xFC60u, expected,
                         "suzy.arithmetic unsigned product matches uint64 model");
    }
    destroy_live_machine(&live);
}

static void run_signed_vectors(uint32_t profile)
{
    const suzy_write negative[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine live = run_guest_writes(profile, negative,
                                               sizeof negative / sizeof negative[0]);
    if (live.machine != NULL) {
        expect_u32_bytes(live.machine, 0xFC60u, UINT32_C(0xFFFFFFF1),
                         "suzy.arithmetic signed FFFD times 5 is FFFFFFF1");
        CHECK((read_sprsys(live.machine) & 0x24u) == 0x24u);
    }
    destroy_live_machine(&live);

    const suzy_write stale_sign[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u},
        {0xFC52u, 0x05u}, {0xFC54u, 0x03u}, {0xFC55u, 0x00u}
    };
    live = run_guest_writes(profile, stale_sign,
                            sizeof stale_sign / sizeof stale_sign[0]);
    if (live.machine != NULL) {
        expect_u32_bytes(live.machine, 0xFC60u, UINT32_C(0xFFFFFFF1),
                         "suzy.arithmetic low write preserves stale CD sign latch");
    }
    destroy_live_machine(&live);

    const suzy_write signed_8000[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0x00u}, {0xFC53u, 0x80u},
        {0xFC54u, 0x02u}, {0xFC55u, 0x00u}
    };
    live = run_guest_writes(profile, signed_8000,
                            sizeof signed_8000 / sizeof signed_8000[0]);
    if (live.machine != NULL) {
        expect_u32_bytes(live.machine, 0xFC60u, UINT32_C(0x00010000),
                         "suzy.arithmetic signed 8000 remains positive");
        CHECK((read_sprsys(live.machine) & 0x24u) == 0x04u);
    }
    destroy_live_machine(&live);
}

static int case_arithmetic(void)
{
    static const uint16_t edges[] = {0u, 1u, 0xFFFFu, 0x8000u};
    for (uint32_t profile = LYNX_PROFILE_LYNX1; profile <= LYNX_PROFILE_LYNX2; profile++) {
        for (size_t i = 0u; i < sizeof edges / sizeof edges[0]; i++) {
            run_unsigned_vector(profile, edges[i], edges[(i + 1u) % 4u]);
        }
        uint32_t seed = UINT32_C(0x6D2B79F5);
        for (uint32_t i = 0u; i < 12u; i++) {
            seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
            const uint16_t cd = (uint16_t)(seed >> 16);
            seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
            const uint16_t ab = (uint16_t)(seed >> 16);
            run_unsigned_vector(profile, cd, ab);
        }
        run_signed_vectors(profile);
    }
    return check_failures == 0 ? 0 : 1;
}

static int case_flags(void)
{
    for (uint32_t profile = LYNX_PROFILE_LYNX1; profile <= LYNX_PROFILE_LYNX2; profile++) {
        const suzy_write accumulate[] = {
            {0xFC6Cu, 0xF0u}, {0xFC6Du, 0xFFu},
            {0xFC6Eu, 0xFFu}, {0xFC6Fu, 0xFFu},
            {0xFC92u, 0x40u}, {0xFC52u, 0x10u}, {0xFC53u, 0x00u},
            {0xFC54u, 0x10u}, {0xFC55u, 0x00u}
        };
        suzy_live_machine live = run_guest_writes(profile, accumulate,
                                                   sizeof accumulate / sizeof accumulate[0]);
        if (live.machine != NULL) {
            expect_u32_bytes(live.machine, 0xFC60u, UINT32_C(0x00000100),
                             "suzy.flags accumulator multiply product");
            expect_u32_bytes(live.machine, 0xFC6Cu, UINT32_C(0x000000F0),
                             "suzy.flags accumulator wraps low32");
            CHECK((read_sprsys(live.machine) & 0x64u) == 0x64u);
        }
        destroy_live_machine(&live);

        const suzy_write clear_warning[] = {
            {0xFC6Cu, 0xF0u}, {0xFC6Du, 0xFFu},
            {0xFC6Eu, 0xFFu}, {0xFC6Fu, 0xFFu},
            {0xFC92u, 0x40u}, {0xFC52u, 0x10u}, {0xFC53u, 0x00u},
            {0xFC54u, 0x10u}, {0xFC55u, 0x00u}, {0xFC6Cu, 0x00u}
        };
        live = run_guest_writes(profile, clear_warning,
                                sizeof clear_warning / sizeof clear_warning[0]);
        if (live.machine != NULL) CHECK((read_sprsys(live.machine) & 0x64u) == 0x24u);
        destroy_live_machine(&live);

        const suzy_write negative_zero[] = {
            {0xFC92u, 0x80u}, {0xFC52u, 0x00u}, {0xFC53u, 0x00u},
            {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
        };
        live = run_guest_writes(profile, negative_zero,
                                sizeof negative_zero / sizeof negative_zero[0]);
        if (live.machine != NULL) {
            expect_u32_bytes(live.machine, 0xFC60u, 0u,
                             "suzy.flags negative zero does not set negation carry");
            CHECK((read_sprsys(live.machine) & 0x24u) == 0x04u);
        }
        destroy_live_machine(&live);

        const suzy_write signed_accumulate[] = {
            {0xFC92u, 0xC0u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
            {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
        };
        live = run_guest_writes(profile, signed_accumulate,
                                sizeof signed_accumulate / sizeof signed_accumulate[0]);
        if (live.machine != NULL) {
            expect_u32_bytes(live.machine, 0xFC60u, UINT32_C(0xFFFFFFF1),
                             "suzy.flags signed product remains two-complement");
            expect_u32_bytes(live.machine, 0xFC6Cu, UINT32_C(0xFFFFFFF1),
                             "suzy.flags signed accumulation uses low32 sum");
            CHECK((read_sprsys(live.machine) & 0x64u) == 0x04u);
        }
        destroy_live_machine(&live);

        const suzy_write divide_remainder[] = {
            {0xFC92u, 0xC0u}, {0xFC56u, 0x0Au}, {0xFC57u, 0x00u},
            {0xFC60u, 0x00u}, {0xFC61u, 0x00u}, {0xFC62u, 0x01u},
            {0xFC63u, 0x00u}
        };
        live = run_guest_writes(profile, divide_remainder,
                                sizeof divide_remainder / sizeof divide_remainder[0]);
        if (live.machine != NULL) {
            expect_u32_bytes(live.machine, 0xFC52u, UINT32_C(0x00001999),
                             "suzy.arithmetic division quotient");
            expect_u32_bytes(live.machine, 0xFC6Cu, 6u,
                             "suzy.arithmetic selected remainder policy");
            CHECK((read_sprsys(live.machine) & 0x64u) == 0x24u);
        }
        destroy_live_machine(&live);

        const suzy_write divide_exact[] = {
            {0xFC92u, 0x00u}, {0xFC56u, 0xFFu}, {0xFC57u, 0x00u},
            {0xFC60u, 0xFFu}, {0xFC61u, 0xFFu}, {0xFC62u, 0x00u},
            {0xFC63u, 0x00u}
        };
        live = run_guest_writes(profile, divide_exact,
                                sizeof divide_exact / sizeof divide_exact[0]);
        if (live.machine != NULL) {
            expect_u32_bytes(live.machine, 0xFC52u, UINT32_C(0x00000101),
                             "suzy.arithmetic exact division quotient");
            expect_u32_bytes(live.machine, 0xFC6Cu, 0u,
                             "suzy.arithmetic exact division remainder");
            CHECK((read_sprsys(live.machine) & 0x64u) == 0x04u);
        }
        destroy_live_machine(&live);

        const suzy_write divide_zero[] = {
            {0xFC56u, 0x00u}, {0xFC57u, 0x00u},
            {0xFC60u, 0x34u}, {0xFC61u, 0x12u}, {0xFC62u, 0x00u},
            {0xFC63u, 0x00u}
        };
        live = run_guest_writes(profile, divide_zero,
                                sizeof divide_zero / sizeof divide_zero[0]);
        if (live.machine != NULL) {
            expect_u32_bytes(live.machine, 0xFC52u, UINT32_MAX,
                             "suzy.arithmetic divide by zero quotient");
            expect_u32_bytes(live.machine, 0xFC6Cu, 0u,
                             "suzy.arithmetic divide by zero remainder policy");
            CHECK((read_sprsys(live.machine) & 0x64u) == 0x64u);
        }
        destroy_live_machine(&live);
    }
    return check_failures == 0 ? 0 : 1;
}

static void run_register_refusal(uint32_t profile, uint16_t address)
{
    enum { LOAD_ADDRESS = 0x0200u };
    const suzy_write write = {address, 0xA5u};
    uint8_t program[64];
    uint8_t object[128];
    const size_t program_len = make_guest_writes(program, LOAD_ADDRESS, &write, 1u);
    const size_t object_len = make_program_object(object, sizeof object, program,
                                                   program_len, LOAD_ADDRESS);
    void *allocation = NULL;
    lynx_machine *machine = make_machine(profile, &allocation);
    if (machine == NULL) {
        free(allocation);
        return;
    }
    CHECK_EQ_U64(lynx_load_bll(machine, object, object_len), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(machine, 2000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    uint8_t actual = 0u;
    CHECK_EQ_U64(lynx_read_memory(machine, address, &actual, 1u), LYNX_OK);
    CHECK_EQ_HEX(actual, 0u);
    CHECK_EQ_U64(lynx_read_cpu_memory(machine, address, &actual, 1u), LYNX_ERR_UNAVAILABLE);
    lynx_machine_deinit(machine);
    free(allocation);
}

static int case_registers(void)
{
    for (uint32_t profile = LYNX_PROFILE_LYNX1; profile <= LYNX_PROFILE_LYNX2; profile++) {
        suzy_write writes[48];
        uint8_t expected[48];
        for (uint8_t i = 0u; i < 48u; i++) {
            expected[i] = (uint8_t)(i * 13u + 0x37u);
            writes[i].addr = (uint16_t)(0xFC00u + i);
            writes[i].value = expected[i];
        }
        suzy_live_machine live = run_guest_writes(profile, writes, 48u);
        if (live.machine != NULL) {
            uint8_t actual[48];
            peek_bytes(live.machine, 0xFC40u, actual, sizeof actual);
            CHECK(memcmp(actual, expected, sizeof actual) == 0);
        }
        destroy_live_machine(&live);

        const suzy_write mirror_writes[] = {
            {0xFC40u, 0xA1u}, {0xFC42u, 0xB2u}, {0xFC50u, 0xC3u},
            {0xFC58u, 0xD4u}, {0xFC64u, 0xE5u}, {0xFC68u, 0xF6u},
            {0xFC13u, 0xFFu}, {0xFC15u, 0x55u}, {0xFC23u, 0xAAu}
        };
        live = run_guest_writes(profile, mirror_writes,
                                sizeof mirror_writes / sizeof mirror_writes[0]);
        if (live.machine != NULL) {
            const uint16_t base_addrs[] = {0xFC00u, 0xFC02u, 0xFC10u};
            const uint8_t expected_values[] = {0xA1u, 0xB2u, 0xC3u};
            for (size_t i = 0u; i < sizeof base_addrs / sizeof base_addrs[0]; i++) {
                uint8_t actual = 0u;
                peek_bytes(live.machine, base_addrs[i], &actual, 1u);
                CHECK_EQ_HEX(actual, expected_values[i]);
            }
            const uint16_t math_addrs[] = {0xFC53u, 0xFC55u, 0xFC63u};
            const uint8_t math_values[] = {0xFFu, 0x55u, 0xAAu};
            for (size_t i = 0u; i < sizeof math_addrs / sizeof math_addrs[0]; i++) {
                uint8_t actual = 0u;
                peek_bytes(live.machine, math_addrs[i], &actual, 1u);
                CHECK_EQ_HEX(actual, math_values[i]);
            }
            CHECK((read_sprsys(live.machine) & 0x80u) == 0u);
        }
        destroy_live_machine(&live);
        run_register_refusal(profile, 0xFC30u);
        run_register_refusal(profile, 0xFC70u);
    }
    return check_failures == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1] == NULL || argv[1][0] == '\0') {
        fprintf(stderr, "usage: lynx-test-suzy <case>\n");
        return 2;
    }
    if (strcmp(argv[1], "tracer") != 0 && strcmp(argv[1], "snapshot_tracer") != 0 &&
        strcmp(argv[1], "arithmetic") != 0 && strcmp(argv[1], "flags") != 0 &&
        strcmp(argv[1], "registers") != 0) {
        fprintf(stderr, "lynx-test-suzy: unknown case '%s'\n", argv[1]);
        return 2;
    }
    printf("TAP version 13\n1..1\n");
    int result = 0;
    if (strcmp(argv[1], "snapshot_tracer") == 0) result = case_snapshot_tracer();
    else if (strcmp(argv[1], "arithmetic") == 0) result = case_arithmetic();
    else if (strcmp(argv[1], "flags") == 0) result = case_flags();
    else if (strcmp(argv[1], "registers") == 0) result = case_registers();
    else result = case_tracer();
    printf("%s 1 - suzy.%s\n", result == 0 ? "ok" : "not ok", argv[1]);
    return result;
}
