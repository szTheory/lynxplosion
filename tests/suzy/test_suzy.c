#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "internal.h"
#include "lynxplosion.h"
#include "sha256.h"

#define CHECK_NAMED(cond, name)                                                                    \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "ASSERT %s failed\n", (name));                                      \
            check_failures++;                                                                      \
        }                                                                                          \
    } while (0)

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
                                const suzy_write *writes, size_t write_count,
                                bool wait_after_command)
{
    size_t length = 0u;
    append(program, &length, 0x78u); /* SEI */
    for (size_t i = 0u; i < write_count; i++) {
        (void)emit_write(program, &length, writes[i].addr, writes[i].value);
        if (wait_after_command &&
            (writes[i].addr == 0xFC55u || writes[i].addr == 0xFC63u)) {
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

static suzy_live_machine load_guest_program(uint32_t profile, const uint8_t *program,
                                            size_t program_len)
{
    enum { LOAD_ADDRESS = 0x0200u };
    uint8_t object[4106];
    const size_t object_len = make_program_object(object, sizeof object, program,
                                                   program_len, LOAD_ADDRESS);
    CHECK(object_len != 0u);
    suzy_live_machine live = {NULL, NULL};
    live.machine = make_machine(profile, &live.allocation);
    if (live.machine == NULL) return live;
    CHECK_EQ_U64(lynx_load_bll(live.machine, object, object_len), LYNX_OK);
    return live;
}

static suzy_live_machine load_guest_writes(uint32_t profile, const suzy_write *writes,
                                           size_t write_count)
{
    enum { LOAD_ADDRESS = 0x0200u };
    uint8_t program[4096];
    const size_t program_len = make_guest_writes(program, LOAD_ADDRESS, writes,
                                                  write_count, true);
    return load_guest_program(profile, program, program_len);
}

static void run_to_budget(suzy_live_machine *live, uint64_t budget)
{
    if (live->machine == NULL) return;
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    CHECK_EQ_U64(lynx_advance(live->machine, budget, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, budget);
}

static suzy_live_machine run_guest_writes(uint32_t profile, const suzy_write *writes,
                                          size_t write_count)
{
    suzy_live_machine live = load_guest_writes(profile, writes, write_count);
    run_to_budget(&live, 12000u);
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

static uint32_t test_timing_policy = LYNX_TIMING_POLICY_LEGACY_FLAT5;

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
    config.timing_policy = test_timing_policy;
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
    const size_t program_len = make_guest_writes(program, LOAD_ADDRESS, &write, 1u, true);
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

static size_t make_status_poll_program(uint8_t *program, uint16_t load_addr,
                                       const suzy_write *writes, size_t write_count)
{
    size_t length = 0u;
    append(program, &length, 0x78u); /* SEI */
    for (size_t i = 0u; i < write_count; i++) {
        (void)emit_write(program, &length, writes[i].addr, writes[i].value);
    }
    const uint16_t poll_addr = (uint16_t)(load_addr + (uint16_t)length);
    append(program, &length, 0xADu); append(program, &length, 0x92u); append(program, &length, 0xFCu);
    append(program, &length, 0x30u); append(program, &length, 0xFBu); /* BMI poll */
    append(program, &length, 0x8Du); append(program, &length, 0x00u); append(program, &length, 0x04u);
    const uint16_t done_addr = (uint16_t)(load_addr + (uint16_t)length);
    append(program, &length, 0x4Cu); append(program, &length, (uint8_t)done_addr);
    append(program, &length, (uint8_t)(done_addr >> 8));
    (void)poll_addr;
    return length;
}

static bool start_watched_operation(lynx_machine *machine, uint16_t command,
                                    uint64_t *start_tick)
{
    CHECK_EQ_U64(lynx_watch_configure(machine, command, 1u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(machine, 2000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    lynx_watch_event event;
    memset(&event, 0, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(machine, &event), LYNX_OK);
    CHECK_EQ_HEX(event.address, command);
    CHECK_EQ_U64(event.hit, 1u);
    *start_tick = event.hit_tick;
    return stop == LYNX_STOP_WATCH && event.hit == 1u && event.address == command;
}

static void check_deadline_case(uint32_t profile, const suzy_write *writes,
                                size_t write_count, uint16_t command,
                                uint64_t duration, uint16_t result_address,
                                uint32_t result)
{
    suzy_live_machine live = load_guest_writes(profile, writes, write_count);
    if (live.machine == NULL) {
        destroy_live_machine(&live);
        return;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(live.machine, command, &start);
    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    const uint64_t deadline = start + duration;
    CHECK(deadline > now);
    if (deadline <= now) {
        destroy_live_machine(&live);
        return;
    }

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    CHECK_EQ_U64(lynx_advance(live.machine, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK((read_sprsys(live.machine) & 0x80u) != 0u);

    const uint64_t to_before_deadline = deadline - 1u - now;
    CHECK_EQ_U64(lynx_advance(live.machine, to_before_deadline, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, to_before_deadline);
    CHECK((read_sprsys(live.machine) & 0x80u) != 0u);
    CHECK_EQ_U64(lynx_advance(live.machine, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    CHECK_EQ_U64(now, deadline);
    CHECK((read_sprsys(live.machine) & 0x80u) != 0u);
    CHECK_EQ_U64(lynx_advance(live.machine, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    CHECK_EQ_U64(now, deadline + 1u);
    CHECK((read_sprsys(live.machine) & 0x80u) == 0u);
    CHECK((read_sprsys(live.machine) & 0x04u) != 0u);
    expect_u32_bytes(live.machine, result_address, result,
                     "suzy.lifecycle event publishes exact result");
    destroy_live_machine(&live);
}

static int case_completion_read_tie(void)
{
    enum { LOAD_ADDRESS = 0x0200u };
    const suzy_write writes[] = {
        {0xFC52u, 0x03u}, {0xFC53u, 0x00u},
        {0xFC54u, 0x02u}, {0xFC55u, 0x00u}
    };
    uint8_t program[256];
    const size_t program_len = make_status_poll_program(program, LOAD_ADDRESS, writes,
                                                         sizeof writes / sizeof writes[0]);
    suzy_live_machine live = load_guest_program(LYNX_PROFILE_LYNX1, program, program_len);
    if (live.machine == NULL) {
        destroy_live_machine(&live);
        return check_failures == 0 ? 0 : 1;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(live.machine, 0xFC55u, &start);
    const uint64_t deadline = start + 44u;
    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    bool held_status_read = false;
    for (uint32_t i = 0u; i < 80u && now < deadline; i++) {
        if (live.machine->cpu.req_addr == 0xFC92u &&
            live.machine->cpu.req_kind == LYNX__CK_READ &&
            live.machine->cpu_ready_at <= deadline) {
            held_status_read = true;
            break;
        }
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
        CHECK_EQ_U64(lynx_advance(live.machine, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 1u);
        CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    }
    CHECK_NAMED(held_status_read, "suzy.lifecycle.status-read-pending-at-deadline");
    if (held_status_read) {
        CHECK_EQ_U64(lynx__machine_set_bus_hold(live.machine, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
        const uint64_t to_deadline = deadline - now;
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
        CHECK_EQ_U64(lynx_advance(live.machine, to_deadline, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, to_deadline);
        CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
        CHECK_EQ_U64(now, deadline);
        CHECK((read_sprsys(live.machine) & 0x80u) != 0u);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(live.machine, 0u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(live.machine, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 1u);
        uint32_t accumulator = 0u;
        CHECK_EQ_U64(lynx_read_register(live.machine, LYNX_REG_A, &accumulator), LYNX_OK);
        CHECK_NAMED((read_sprsys(live.machine) & 0x80u) == 0u &&
                    (accumulator & 0x80u) == 0u && (accumulator & 0x04u) != 0u,
                    "suzy.lifecycle.completion-before-status-read");
    }
    destroy_live_machine(&live);
    return check_failures == 0 ? 0 : 1;
}

static int case_reset_load(void);
static int case_busy_backing_refusal(void);
static int case_headroom_refusal(void);

static int case_lifecycle(void)
{
    static const uint16_t divisors[] = {0x8000u, 0x4000u, 0x0001u, 0x0000u};
    for (uint32_t profile = LYNX_PROFILE_LYNX1; profile <= LYNX_PROFILE_LYNX2; profile++) {
        const suzy_write plain[] = {
            {0xFC52u, 0x03u}, {0xFC53u, 0x00u},
            {0xFC54u, 0x02u}, {0xFC55u, 0x00u}
        };
        check_deadline_case(profile, plain, sizeof plain / sizeof plain[0],
                            0xFC55u, 44u, 0xFC60u, 6u);
        const suzy_write signed_mul[] = {
            {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
            {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
        };
        check_deadline_case(profile, signed_mul, sizeof signed_mul / sizeof signed_mul[0],
                            0xFC55u, 54u, 0xFC60u, UINT32_C(0xFFFFFFF1));
        const suzy_write accum_mul[] = {
            {0xFC92u, 0x40u}, {0xFC52u, 0x10u}, {0xFC53u, 0x00u},
            {0xFC54u, 0x10u}, {0xFC55u, 0x00u}
        };
        check_deadline_case(profile, accum_mul, sizeof accum_mul / sizeof accum_mul[0],
                            0xFC55u, 54u, 0xFC6Cu, 0x100u);
        for (size_t i = 0u; i < sizeof divisors / sizeof divisors[0]; i++) {
            const uint16_t divisor = divisors[i];
            const uint64_t duration = 176u + 14u * (uint64_t)(i == 0u ? 0u :
                i == 1u ? 1u : i == 2u ? 15u : 16u);
            const uint32_t quotient = divisor == 0u ? UINT32_MAX :
                UINT32_C(0x00010000) / divisor;
            const suzy_write divide[] = {
                {0xFC56u, (uint8_t)divisor}, {0xFC57u, (uint8_t)(divisor >> 8)},
                {0xFC60u, 0x00u}, {0xFC61u, 0x00u},
                {0xFC62u, 0x01u}, {0xFC63u, 0x00u}
            };
            check_deadline_case(profile, divide, sizeof divide / sizeof divide[0],
                                0xFC63u, duration, 0xFC52u, quotient);
        }
    }
    (void)case_completion_read_tie();
    (void)case_reset_load();
    (void)case_busy_backing_refusal();
    (void)case_headroom_refusal();
    return check_failures == 0 ? 0 : 1;
}

enum { SNAP_PAYLOAD_HASH_OFFSET = 220u, SNAP_SUZY_RECORD_OFFSET = 78108u };

static void rehash_snapshot_payload(uint8_t *snapshot)
{
    uint8_t digest[32];
    lynx__sha256 sha;
    lynx__sha256_init(&sha);
    lynx__sha256_update(&sha, snapshot + LYNX__SNAPSHOT_HEADER_LEN,
                        LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&sha, digest);
    memcpy(snapshot + SNAP_PAYLOAD_HASH_OFFSET, digest, sizeof digest);
}

static uint8_t *save_snapshot_blob(const lynx_machine *machine, size_t *size)
{
    CHECK_EQ_U64(lynx_snapshot_size(machine, size), LYNX_OK);
    uint8_t *snapshot = malloc(*size);
    CHECK(snapshot != NULL);
    if (snapshot != NULL) {
        size_t written = 0u;
        CHECK_EQ_U64(lynx_snapshot_save(machine, snapshot, *size, &written), LYNX_OK);
        CHECK_EQ_U64(written, *size);
    }
    return snapshot;
}

static void advance_through_math_deadline(lynx_machine *machine, uint64_t deadline)
{
    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(machine, &now), LYNX_OK);
    CHECK(deadline >= now);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    CHECK_EQ_U64(lynx_advance(machine, deadline - now, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, deadline - now);
    CHECK((read_sprsys(machine) & 0x80u) != 0u);
    CHECK_EQ_U64(lynx_advance(machine, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 1u);
    CHECK((read_sprsys(machine) & 0x80u) == 0u);
}

static void advance_to_math_deadline(lynx_machine *machine, uint64_t deadline)
{
    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(machine, &now), LYNX_OK);
    CHECK(deadline >= now);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
    CHECK_EQ_U64(lynx_advance(machine, deadline - now, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, deadline - now);
    CHECK((read_sprsys(machine) & 0x80u) != 0u);
}

static int case_snapshot_refusal(void)
{
    const suzy_write signed_mul[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine source = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                  sizeof signed_mul / sizeof signed_mul[0]);
    suzy_live_machine victim = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                  sizeof signed_mul / sizeof signed_mul[0]);
    if (source.machine == NULL || victim.machine == NULL) {
        destroy_live_machine(&source);
        destroy_live_machine(&victim);
        return check_failures == 0 ? 0 : 1;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(source.machine, 0xFC55u, &start);
    size_t snapshot_size = 0u;
    uint8_t *active = save_snapshot_blob(source.machine, &snapshot_size);
    if (active == NULL) {
        destroy_live_machine(&source);
        destroy_live_machine(&victim);
        return check_failures == 0 ? 0 : 1;
    }
    uint8_t *forged = malloc(snapshot_size);
    uint8_t *before = malloc(sizeof *victim.machine);
    CHECK(forged != NULL && before != NULL);
    if (forged != NULL && before != NULL) {
        const size_t record = LYNX__SNAPSHOT_HEADER_LEN + SNAP_SUZY_RECORD_OFFSET;
        static const char *const forged_names[] = {
            "suzy.snapshot_refusal.forged-kind-is-atomic",
            "suzy.snapshot_refusal.forged-sign-is-atomic",
            "suzy.snapshot_refusal.forged-mode-is-atomic",
            "suzy.snapshot_refusal.forged-idle-is-atomic",
            "suzy.snapshot_refusal.forged-deadline-is-atomic",
            "suzy.snapshot_refusal.forged-start-is-atomic",
            "suzy.snapshot_refusal.forged-backing-is-atomic"
        };
        CHECK_EQ_U64(lynx_watch_configure(victim.machine, 0x1234u, 1u), LYNX_OK);
        for (size_t i = 0u; i < sizeof forged_names / sizeof forged_names[0]; i++) {
            memcpy(forged, active, snapshot_size);
            switch (i) {
            case 0u:
                forged[record + 54u] = 3u;
                break;
            case 1u:
                forged[record + 70u] ^= 1u;
                break;
            case 2u:
                forged[record + 55u] = 0x10u;
                break;
            case 3u:
                forged[record + 54u] = 0u;
                break;
            case 4u:
                forged[record + 80u]++;
                break;
            case 5u:
                forged[record + 72u]++;
                break;
            case 6u:
                forged[record + 0x14u] ^= 1u;
                break;
            default:
                CHECK(0 && "complete forged snapshot table");
                continue;
            }
            rehash_snapshot_payload(forged);
            memcpy(before, victim.machine, sizeof *victim.machine);
            const lynx_status status = lynx_snapshot_restore(victim.machine, forged, snapshot_size);
            CHECK_NAMED(status == LYNX_ERR_SNAPSHOT_INVALID &&
                        memcmp(before, victim.machine, sizeof *victim.machine) == 0,
                        forged_names[i]);
        }
    }
    free(forged);
    free(before);

    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(source.machine, &now), LYNX_OK);
    const uint64_t deadline = source.machine->suzy.deadline_tick;
    advance_to_math_deadline(source.machine, deadline);
    free(active);
    active = save_snapshot_blob(source.machine, &snapshot_size);
    CHECK(active != NULL);
    if (active != NULL) {
        CHECK_EQ_U64(lynx_snapshot_restore(victim.machine, active, snapshot_size), LYNX_OK);
        CHECK_EQ_U64(lynx_guest_ticks(victim.machine, &now), LYNX_OK);
        CHECK_EQ_U64(now, deadline);
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
        CHECK_EQ_U64(lynx_advance(source.machine, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 1u);
        CHECK((read_sprsys(source.machine) & 0x80u) == 0u);
        CHECK_EQ_U64(lynx_advance(victim.machine, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 1u);
        CHECK((read_sprsys(victim.machine) & 0x80u) == 0u);
        expect_u32_bytes(victim.machine, 0xFC60u, UINT32_C(0xFFFFFFF1),
                         "suzy.snapshot_refusal.equal-deadline-is-reachable");
    }
    free(active);
    destroy_live_machine(&source);
    destroy_live_machine(&victim);

    const suzy_write changed_control[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u}, {0xFC92u, 0x4Cu}
    };
    uint8_t program[256];
    const size_t program_len = make_guest_writes(program, 0x0200u, changed_control,
                                                  sizeof changed_control / sizeof changed_control[0],
                                                  false);
    source = load_guest_program(LYNX_PROFILE_LYNX1, program, program_len);
    victim = load_guest_program(LYNX_PROFILE_LYNX1, program, program_len);
    if (source.machine != NULL && victim.machine != NULL) {
        CHECK_EQ_U64(lynx_watch_configure(source.machine, 0xFC55u, 1u), LYNX_OK);
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        CHECK_EQ_U64(lynx_advance(source.machine, 2000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK_EQ_U64(lynx_watch_configure(source.machine, 0xFC92u, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(source.machine, 2000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK_EQ_HEX(source.machine->suzy.mode, 0x40u);
        CHECK_EQ_HEX(source.machine->suzy.latched_modes, 0x80u);
        CHECK_EQ_HEX(source.machine->video.lefthand, 1u);
        const uint64_t saved_deadline = source.machine->suzy.deadline_tick;
        size_t changed_size = 0u;
        uint8_t *changed = save_snapshot_blob(source.machine, &changed_size);
        CHECK(changed != NULL);
        if (changed != NULL) {
            CHECK_EQ_U64(lynx_snapshot_restore(victim.machine, changed, changed_size), LYNX_OK);
            advance_through_math_deadline(source.machine, saved_deadline);
            advance_through_math_deadline(victim.machine, saved_deadline);
            uint8_t *source_final = save_snapshot_blob(source.machine, &changed_size);
            size_t victim_size = 0u;
            uint8_t *victim_final = save_snapshot_blob(victim.machine, &victim_size);
            CHECK(source_final != NULL && victim_final != NULL);
            if (source_final != NULL && victim_final != NULL) {
                CHECK_EQ_U64(changed_size, victim_size);
                CHECK(memcmp(source_final, victim_final, source_final != NULL ? changed_size : 0u) == 0);
            }
            free(source_final);
            free(victim_final);
        }
        free(changed);
    }
    destroy_live_machine(&source);
    destroy_live_machine(&victim);
    return check_failures == 0 ? 0 : 1;
}

static int case_fault_snapshot(void)
{
    const suzy_write signed_mul[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine source = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                  sizeof signed_mul / sizeof signed_mul[0]);
    suzy_live_machine restored = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                    sizeof signed_mul / sizeof signed_mul[0]);
    if (source.machine == NULL || restored.machine == NULL) {
        destroy_live_machine(&source);
        destroy_live_machine(&restored);
        return check_failures == 0 ? 0 : 1;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(source.machine, 0xFC55u, &start);
    size_t snapshot_size = 0u;
    uint8_t *snapshot = save_snapshot_blob(source.machine, &snapshot_size);
    if (snapshot != NULL) {
        CHECK_EQ_U64(lynx_snapshot_restore(restored.machine, snapshot, snapshot_size), LYNX_OK);
        const uint64_t deadline = source.machine->suzy.deadline_tick;
        advance_through_math_deadline(source.machine, deadline);
        advance_through_math_deadline(restored.machine, deadline);
        uint8_t source_product[4] = {0u};
        uint8_t restored_product[4] = {0u};
        peek_bytes(source.machine, 0xFC60u, source_product, sizeof source_product);
        peek_bytes(restored.machine, 0xFC60u, restored_product, sizeof restored_product);
        CHECK_EQ_HEX(source_product[0], 0xF1u);
        CHECK_NAMED(memcmp(source_product, restored_product, sizeof source_product) == 0,
                    "suzy.snapshot.continuation-suffix");
    }
    free(snapshot);
    destroy_live_machine(&source);
    destroy_live_machine(&restored);
    return check_failures == 0 ? 0 : 1;
}

static uint32_t peek_result_word(const lynx_machine *machine, uint16_t addr)
{
    uint8_t bytes[4] = {0u};
    peek_bytes(machine, addr, bytes, sizeof bytes);
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int case_fault_decoder(void)
{
    const suzy_write writes[] = {
        {0xFC52u, 0xFFu}, {0xFC53u, 0x00u},
        {0xFC54u, 0x02u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine live = run_guest_writes(LYNX_PROFILE_LYNX1, writes,
                                               sizeof writes / sizeof writes[0]);
    if (live.machine != NULL) {
        CHECK_NAMED(peek_result_word(live.machine, 0xFC60u) == UINT32_C(0x000001FE),
                    "suzy.fault.decoder-mapped-command-result");
    }
    destroy_live_machine(&live);
    return check_failures == 0 ? 0 : 1;
}

static int case_fault_result(void)
{
    const suzy_write writes[] = {
        {0xFC52u, 0xFFu}, {0xFC53u, 0x00u},
        {0xFC54u, 0x02u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine live = run_guest_writes(LYNX_PROFILE_LYNX1, writes,
                                               sizeof writes / sizeof writes[0]);
    if (live.machine != NULL) {
        CHECK_NAMED(peek_result_word(live.machine, 0xFC60u) == UINT32_C(0x000001FE),
                    "suzy.fault.result-independent-product");
    }
    destroy_live_machine(&live);
    return check_failures == 0 ? 0 : 1;
}

static int case_fault_completion(void)
{
    enum { LOAD_ADDRESS = 0x0200u };
    const suzy_write writes[] = {
        {0xFC52u, 0x03u}, {0xFC53u, 0x00u},
        {0xFC54u, 0x02u}, {0xFC55u, 0x00u}
    };
    uint8_t program[256];
    const size_t program_len = make_status_poll_program(program, LOAD_ADDRESS, writes,
                                                         sizeof writes / sizeof writes[0]);
    suzy_live_machine live = load_guest_program(LYNX_PROFILE_LYNX1, program, program_len);
    if (live.machine == NULL) {
        destroy_live_machine(&live);
        return check_failures == 0 ? 0 : 1;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(live.machine, 0xFC55u, &start);
    const uint64_t deadline = start + 44u;
    uint64_t now = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    bool held_status_read = false;
    for (uint32_t i = 0u; i < 80u && now < deadline; i++) {
        if (live.machine->cpu.req_addr == 0xFC92u &&
            live.machine->cpu.req_kind == LYNX__CK_READ &&
            live.machine->cpu_ready_at <= deadline) {
            held_status_read = true;
            break;
        }
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
        CHECK_EQ_U64(lynx_advance(live.machine, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 1u);
        CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
    }
    CHECK_NAMED(held_status_read, "suzy.lifecycle.status-read-pending-at-deadline");
    if (held_status_read) {
        CHECK_EQ_U64(lynx__machine_set_bus_hold(live.machine, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_guest_ticks(live.machine, &now), LYNX_OK);
        const uint64_t to_deadline = deadline - now;
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
        CHECK_EQ_U64(lynx_advance(live.machine, to_deadline, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, to_deadline);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(live.machine, 0u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(live.machine, 1u, &consumed, &stop), LYNX_OK);
        uint32_t accumulator = 0u;
        CHECK_EQ_U64(lynx_read_register(live.machine, LYNX_REG_A, &accumulator), LYNX_OK);
        CHECK_NAMED((accumulator & 0x80u) == 0u && (accumulator & 0x04u) != 0u,
                    "suzy.lifecycle.completion-before-status-read");
    }
    destroy_live_machine(&live);
    return check_failures == 0 ? 0 : 1;
}

static int case_reset_load(void)
{
    const suzy_write signed_mul[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine busy = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                sizeof signed_mul / sizeof signed_mul[0]);
    suzy_live_machine reset_reference = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                           sizeof signed_mul / sizeof signed_mul[0]);
    if (busy.machine == NULL || reset_reference.machine == NULL) {
        destroy_live_machine(&busy);
        destroy_live_machine(&reset_reference);
        return check_failures == 0 ? 0 : 1;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(busy.machine, 0xFC55u, &start);
    uint8_t *before = malloc(sizeof *busy.machine);
    CHECK(before != NULL);
    if (before != NULL) {
        memcpy(before, busy.machine, sizeof *busy.machine);
        uint8_t malformed[10] = {0u};
        CHECK_NAMED(lynx_load_bll(busy.machine, malformed, sizeof malformed) != LYNX_OK &&
                    memcmp(before, busy.machine, sizeof *busy.machine) == 0,
                    "suzy.lifecycle.refused-load-preserves-busy-instance");
    }
    free(before);
    CHECK_EQ_U64(lynx_reset(busy.machine), LYNX_OK);
    CHECK_EQ_U64(lynx_reset(reset_reference.machine), LYNX_OK);
    CHECK_NAMED(busy.machine->suzy.operation == 0u && busy.machine->suzy.deadline_tick == 0u &&
                memcmp(busy.machine, reset_reference.machine, sizeof *busy.machine) == 0,
                "suzy.lifecycle.reset-cancels-and-matches-fresh-reset");
    destroy_live_machine(&busy);
    destroy_live_machine(&reset_reference);

    const uint8_t replacement_program[] = {0x4Cu, 0x00u, 0x02u};
    uint8_t object[32];
    const size_t object_length = make_program_object(object, sizeof object,
                                                     replacement_program,
                                                     sizeof replacement_program, 0x0200u);
    CHECK(object_length != 0u);
    busy = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                             sizeof signed_mul / sizeof signed_mul[0]);
    suzy_live_machine load_reference = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                          sizeof signed_mul / sizeof signed_mul[0]);
    if (busy.machine != NULL && load_reference.machine != NULL) {
        (void)start_watched_operation(busy.machine, 0xFC55u, &start);
        CHECK_EQ_U64(lynx_load_bll(busy.machine, object, object_length), LYNX_OK);
        CHECK_EQ_U64(lynx_load_bll(load_reference.machine, object, object_length), LYNX_OK);
        CHECK_NAMED(busy.machine->suzy.operation == 0u && busy.machine->suzy.deadline_tick == 0u &&
                    memcmp(busy.machine, load_reference.machine, sizeof *busy.machine) == 0,
                    "suzy.lifecycle.accepted-load-cancels-and-matches-fresh-load");
    }
    destroy_live_machine(&busy);
    destroy_live_machine(&load_reference);
    return check_failures == 0 ? 0 : 1;
}

static int case_busy_backing_refusal(void)
{
    const suzy_write signed_mul[] = {
        {0xFC92u, 0x80u}, {0xFC52u, 0xFDu}, {0xFC53u, 0xFFu},
        {0xFC54u, 0x05u}, {0xFC55u, 0x00u}
    };
    suzy_live_machine live = load_guest_writes(LYNX_PROFILE_LYNX1, signed_mul,
                                                sizeof signed_mul / sizeof signed_mul[0]);
    if (live.machine == NULL) {
        destroy_live_machine(&live);
        return check_failures == 0 ? 0 : 1;
    }
    uint64_t start = 0u;
    (void)start_watched_operation(live.machine, 0xFC55u, &start);
    uint8_t *before = malloc(sizeof *live.machine);
    CHECK(before != NULL);
    if (before != NULL) {
        memcpy(before, live.machine, sizeof *live.machine);
        static const uint8_t protected_indices[] = {
            0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
            0x20u, 0x21u, 0x22u, 0x23u,
            0x2Cu, 0x2Du, 0x2Eu, 0x2Fu
        };
        for (size_t i = 0u; i < sizeof protected_indices; i++) {
            const uint16_t base = (uint16_t)(0xFC00u + protected_indices[i]);
            const uint16_t addresses[] = {base, (uint16_t)(base + 0x40u)};
            for (size_t view = 0u; view < sizeof addresses / sizeof addresses[0]; view++) {
                CHECK_NAMED(!lynx__suzy_write(&live.machine->suzy, addresses[view], 0xA5u,
                                              live.machine->guest_ticks) &&
                            memcmp(before, live.machine, sizeof *live.machine) == 0,
                            "suzy.lifecycle.busy-backing-refusal-both-views");
            }
        }
    }
    free(before);
    CHECK(live.machine->suzy.operation == 1u);
    CHECK(live.machine->guest_ticks < live.machine->suzy.deadline_tick);
    CHECK(lynx__suzy_valid(&live.machine->suzy, live.machine->guest_ticks));
    destroy_live_machine(&live);

    static const uint16_t cpu_targets[] = {0xFC12u, 0xFC52u};
    for (size_t i = 0u; i < sizeof cpu_targets / sizeof cpu_targets[0]; i++) {
        const suzy_write with_busy_write[] = {
            {0xFC52u, 0x02u}, {0xFC53u, 0x00u},
            {0xFC54u, 0x03u}, {0xFC55u, 0x00u},
            {cpu_targets[i], 0xA5u}
        };
        uint8_t program[128];
        const size_t program_length = make_guest_writes(program, 0x0200u, with_busy_write,
                                                         sizeof with_busy_write /
                                                             sizeof with_busy_write[0],
                                                         false);
        live = load_guest_program(LYNX_PROFILE_LYNX1, program, program_length);
        if (live.machine == NULL) {
            destroy_live_machine(&live);
            continue;
        }
        live.machine->ram[cpu_targets[i]] = 0x5Au;
        start = 0u;
        (void)start_watched_operation(live.machine, 0xFC55u, &start);
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        CHECK_EQ_U64(lynx_advance(live.machine, 40u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 40u);
        CHECK(live.machine->guest_ticks < live.machine->suzy.deadline_tick);
        CHECK(live.machine->suzy.operation == 1u);
        CHECK_EQ_HEX(live.machine->ram[cpu_targets[i]], 0x5Au);
        CHECK_EQ_HEX(live.machine->suzy.registers[0x12u], 0x02u);
        CHECK((read_sprsys(live.machine) & 0x80u) != 0u);
        destroy_live_machine(&live);
    }
    return check_failures == 0 ? 0 : 1;
}

static void write_plain_operands(lynx__suzy_state *suzy, uint64_t tick)
{
    CHECK(lynx__suzy_write(suzy, 0xFC52u, 0x02u, tick));
    CHECK(lynx__suzy_write(suzy, 0xFC53u, 0x00u, tick));
    CHECK(lynx__suzy_write(suzy, 0xFC54u, 0x03u, tick));
}

static int case_headroom_refusal(void)
{
    const uint64_t latest_start = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM - 44u;
    lynx__suzy_state allowed;
    lynx__suzy_init(&allowed);
    write_plain_operands(&allowed, latest_start);
    CHECK(lynx__suzy_write(&allowed, 0xFC55u, 0x00u, latest_start));
    CHECK_EQ_U64(allowed.start_tick, latest_start);
    CHECK_EQ_U64(allowed.deadline_tick, UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM);
    CHECK(lynx__suzy_valid(&allowed, allowed.deadline_tick));

    lynx__suzy_state refused;
    lynx__suzy_init(&refused);
    write_plain_operands(&refused, latest_start + 1u);
    const lynx__suzy_state before = refused;
    CHECK_NAMED(!lynx__suzy_write(&refused, 0xFC55u, 0x00u, latest_start + 1u) &&
                memcmp(&before, &refused, sizeof refused) == 0,
                "suzy.lifecycle.headroom-rejects-overflowing-deadline-atomically");
    CHECK(lynx__suzy_valid(&refused, latest_start + 1u));
    return check_failures == 0 ? 0 : 1;
}

static int case_coarse_deadline(void)
{
    /* A running CPU whose granted cycle straddles a pending Suzy deadline, with
       an audio sample tick between the deadline and the next CPU tick, must not
       let a coarse advance step past the deadline. The NOP count sweeps the
       alignment between deadline, CPU cycles and sample ticks. */
    enum { LOAD_ADDRESS = 0x0200u };
    static const suzy_write divide_by_one[] = {
        {0xFC56u, 0x01u}, {0xFC57u, 0x00u}, {0xFC60u, 0x00u},
        {0xFC61u, 0x00u}, {0xFC62u, 0x01u}, {0xFC63u, 0x00u}
    };
    for (uint32_t policy = 0u; policy <= 1u; policy++) {
        for (uint32_t nops = 0u; nops <= 400u; nops++) {
            uint8_t program[1024];
            size_t length = 0u;
            append(program, &length, 0x78u); /* SEI */
            for (uint32_t i = 0u; i < nops; i++) append(program, &length, 0xEAu);
            for (size_t i = 0u; i < sizeof divide_by_one / sizeof divide_by_one[0]; i++) {
                (void)emit_write(program, &length, divide_by_one[i].addr, divide_by_one[i].value);
            }
            const uint16_t loop_addr = (uint16_t)(LOAD_ADDRESS + (uint16_t)length);
            append(program, &length, 0x4Cu);
            append(program, &length, (uint8_t)loop_addr);
            append(program, &length, (uint8_t)(loop_addr >> 8));

            test_timing_policy = policy;
            suzy_live_machine live = load_guest_program(LYNX_PROFILE_LYNX1, program, length);
            test_timing_policy = LYNX_TIMING_POLICY_LEGACY_FLAT5;
            if (live.machine == NULL) {
                destroy_live_machine(&live);
                return check_failures == 0 ? 0 : 1;
            }
            CHECK_EQ_U64(lynx_watch_configure(live.machine, 0xFC63u, 1u), LYNX_OK);
            uint64_t consumed = 0u;
            lynx_stop_reason stop = LYNX_STOP_BUDGET;
            for (uint32_t chunk = 0u; chunk < 64u && stop != LYNX_STOP_WATCH; chunk++) {
                CHECK_EQ_U64(lynx_advance(live.machine, 200u, &consumed, &stop), LYNX_OK);
            }
            CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
            if (stop != LYNX_STOP_WATCH) {
                destroy_live_machine(&live);
                return 1;
            }
            const uint64_t deadline = live.machine->suzy.deadline_tick;
            CHECK_EQ_U64(lynx_advance(live.machine, 5000u, &consumed, &stop), LYNX_OK);
            const bool completed = live.machine->suzy.operation == 0u &&
                                   (read_sprsys(live.machine) & 0x80u) == 0u;
            if (!completed) {
                fprintf(stderr, "suzy semantic failure: policy %u nops %u deadline %llu left busy\n",
                        (unsigned)policy, (unsigned)nops, (unsigned long long)deadline);
            }
            CHECK_NAMED(completed, "suzy.coarse_deadline.operation-completes");
            size_t size = 0u;
            uint8_t *blob = save_snapshot_blob(live.machine, &size);
            if (blob != NULL) {
                CHECK_NAMED(lynx_snapshot_restore(live.machine, blob, size) == LYNX_OK,
                            "suzy.coarse_deadline.own-snapshot-restores");
                free(blob);
            }
            destroy_live_machine(&live);
            if (check_failures != 0) return 1; /* first failing alignment is enough */
        }
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
        strcmp(argv[1], "registers") != 0 && strcmp(argv[1], "lifecycle") != 0 &&
        strcmp(argv[1], "snapshot_refusal") != 0 &&
        strcmp(argv[1], "fault_decoder") != 0 && strcmp(argv[1], "fault_result") != 0 &&
        strcmp(argv[1], "fault_completion") != 0 && strcmp(argv[1], "fault_snapshot") != 0 &&
        strcmp(argv[1], "reset_load") != 0 && strcmp(argv[1], "coarse_deadline") != 0) {
        fprintf(stderr, "lynx-test-suzy: unknown case '%s'\n", argv[1]);
        return 2;
    }
    printf("TAP version 13\n1..1\n");
    int result = 0;
    if (strcmp(argv[1], "snapshot_tracer") == 0) result = case_snapshot_tracer();
    else if (strcmp(argv[1], "arithmetic") == 0) result = case_arithmetic();
    else if (strcmp(argv[1], "flags") == 0) result = case_flags();
    else if (strcmp(argv[1], "registers") == 0) result = case_registers();
    else if (strcmp(argv[1], "lifecycle") == 0) result = case_lifecycle();
    else if (strcmp(argv[1], "snapshot_refusal") == 0) result = case_snapshot_refusal();
    else if (strcmp(argv[1], "fault_decoder") == 0) result = case_fault_decoder();
    else if (strcmp(argv[1], "fault_result") == 0) result = case_fault_result();
    else if (strcmp(argv[1], "fault_completion") == 0) result = case_fault_completion();
    else if (strcmp(argv[1], "fault_snapshot") == 0) result = case_fault_snapshot();
    else if (strcmp(argv[1], "reset_load") == 0) result = case_reset_load();
    else if (strcmp(argv[1], "coarse_deadline") == 0) result = case_coarse_deadline();
    else result = case_tracer();
    printf("%s 1 - suzy.%s\n", result == 0 ? "ok" : "not ok", argv[1]);
    return result;
}
