/* API tests. One executable, one case per CTest entry: `lynx-test-api <case>`.
   An unknown case exits 2, so a typo in tests.cmake fails instead of passing.

   The cases use the public header. Where a case also needs the layout of the
   instance it says so and includes core/internal.h for sizeof and _Alignof
   only (a white-box check of lynx_machine_size and lynx_machine_align). */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "cpu/flat_bus.h"
#include "internal.h"
#include "lynxplosion.h"
#include "bundle.h"
#include "sha256.h"

/* The retained Phase 1 tracer image: byte i = (7*i + 3) mod 256 for i =
   0..31. As 65C02 code it reaches the profile-dependent opcode $1F, so it is
   used only by the identity and loader cases. */
static const uint8_t tracer_image[32] = {
    0x03, 0x0a, 0x11, 0x18, 0x1f, 0x26, 0x2d, 0x34, 0x3b, 0x42, 0x49,
    0x50, 0x57, 0x5e, 0x65, 0x6c, 0x73, 0x7a, 0x81, 0x88, 0x8f, 0x96,
    0x9d, 0xa4, 0xab, 0xb2, 0xb9, 0xc0, 0xc7, 0xce, 0xd5, 0xdc,
};

/* fixtures/cpu/sum-trap.s as assembled by the pinned ca65 (load $0200). The
   case "cpu_program" checks the SHA-256 of these bytes against the fixture
   manifest value below, so the array cannot drift from the committed fixture.
   Runs only under profile none: it ends at the bit-op slot $07. */
#define SUM_TRAP_SHA256 "6ae9bada8fb004f5756821c0e756267d2a2e44e6f68928fe27984906acc00f33"
static const uint8_t sum_trap[35] = {
    0xa2, 0x05, 0xa9, 0x00, 0x86, 0x10, 0x18, 0x65, 0x10, 0xca, 0xd0, 0xf8,
    0x8d, 0x00, 0x03, 0x20, 0x20, 0x02, 0xa2, 0xff, 0xbd, 0x01, 0x02, 0x48,
    0xa9, 0x00, 0x68, 0x07, 0xea, 0xea, 0xea, 0xea, 0xe6, 0x11, 0x60,
};

/* fixtures/cpu/churn-loop.s (load $0200): a never-ending loop of opcodes both
   Lynx profiles define identically, reading seeded RAM. 70-tick prelude, then
   255 ticks per iteration at flat5 (hand-derived in the fixture source). */
#define CHURN_LOOP_SHA256 "dcee7216a88ff2ffaf3e4a2794c3369676fc47493d710f26cd8bbadd19745b77"
static const uint8_t churn_loop[46] = {
    0xa2, 0x00, 0xa0, 0x08, 0xa9, 0x00, 0x85, 0x40, 0xa9, 0x05, 0x85, 0x41,
    0xbd, 0x00, 0x04, 0x45, 0x30, 0x71, 0x40, 0x9d, 0x00, 0x06, 0x2a, 0x48,
    0x20, 0x2b, 0x02, 0x68, 0x85, 0x30, 0xe8, 0xd0, 0xeb, 0xe6, 0x31, 0x80,
    0xe7, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0x04, 0x32, 0x60,
};

/* Four NOPs at $FFFA-$FFFD, then JMP $FFFA at $FFFE. Its low operand is
   $FFFF and the high operand comes from $0000 after PC wraps. This private
   snapshot fixture explicitly supplies the vector-window RAM mapping. */
static const uint8_t wrap_image[6] = {0xea, 0xea, 0xea, 0xea, 0x4c, 0xfa};

/* One byte: the bit-op slot $07, so profile none stops before it. */
static const uint8_t trap_image[1] = {0x07};

/* A 10-byte BS93 header and a 13-byte tracer payload at $0200. */
static const uint8_t bll_tracer[] = {
    0x80, 0x08, 0x02, 0x00, 0x00, 0x17, 'B', 'S', '9', '3',
    0xA9, 0x0C, 0x8D, 0xF9, 0xFF, 0xA9, 0xA5, 0x8D, 0x00, 0x04, 0x4C, 0x0A, 0x02,
};

/* Two stores to one watched location, separated by a load. The first store
   must finish before WATCH; rearming then observes the second value. */
static const uint8_t watch_program[] = {
    0xA9u, 0x5Au, 0x8Du, 0x00u, 0x04u,
    0xA9u, 0xA6u, 0x8Du, 0x00u, 0x04u,
    0x4Cu, 0x00u, 0x02u
};

/* Read the watched start, write just before the range, write start, end-1,
   then just after it. The three writes make the half-open contract visible. */
static const uint8_t watch_range_program[] = {
    0xADu, 0x00u, 0x04u,             /* LDA $0400 (reads never hit) */
    0xA9u, 0x11u, 0x8Du, 0xFFu, 0x03u, /* STA $03FF: just before */
    0xA9u, 0x22u, 0x8Du, 0x00u, 0x04u, /* STA $0400: range start */
    0xA9u, 0x33u, 0x8Du, 0x01u, 0x04u, /* STA $0401: end-1 */
    0xA9u, 0x44u, 0x8Du, 0x02u, 0x04u, /* STA $0402: just after */
    0x4Cu, 0x00u, 0x02u
};

/* The counter changes only after the watched store has completed. Replaying
   the same store on resume therefore leaves the independent counter at zero. */
static const uint8_t watch_count_program[] = {
    0xA9u, 0x5Au, 0x8Du, 0x00u, 0x04u, /* LDA #$5A; STA $0400 */
    0xEEu, 0x01u, 0x04u,             /* INC $0401 */
    0x4Cu, 0x00u, 0x02u              /* JMP $0200 */
};

/* MAPCTL is set by guest code before a watch on the last address. */
static const uint8_t watch_last_address_program[] = {
    0xA9u, 0x08u, 0x8Du, 0xF9u, 0xFFu, /* LDA #$08; STA $FFF9 */
    0xA9u, 0xC7u, 0x8Du, 0xFFu, 0xFFu, /* LDA #$C7; STA $FFFF */
    0x4Cu, 0x00u, 0x02u
};

static void make_bll_header(uint8_t *object, size_t len, uint16_t load_addr);
static lynx_status load_bll_at(lynx_machine *m, const uint8_t *payload, size_t payload_len,
                               uint32_t load_addr);

/* Instance memory for the tests: aligned for any type, sized from the core's
   own layout. */
static _Alignas(max_align_t) unsigned char machine_buf[sizeof(struct lynx_machine)];

enum {
    API_FAULT_NONE = 0,
    API_FAULT_WRONG_MAP_BIT,
    API_FAULT_SPECIAL_ADDRESS_ORDER,
    API_FAULT_RAW_AS_MAPPED,
    API_FAULT_ROM_FALLBACK,
    API_FAULT_STOP_BEFORE_STORE,
    API_FAULT_REPEAT_STORE,
    API_FAULT_WATCH_IN_DIGEST
};
static int g_api_fault;

static int is_power_of_two(size_t v)
{
    return v != 0u && (v & (v - 1u)) == 0u;
}

/* The four standard SHA-256 vectors (FIPS 180-4 examples plus the million-a
   message), each confirmed independently with shasum before being written here. */
static void expect_sha256(const void *data, size_t len, const char *want_hex)
{
    lynx__sha256 s;
    uint8_t digest[32];
    char hex[65];
    lynx__sha256_init(&s);
    lynx__sha256_update(&s, data, len);
    lynx__sha256_final(&s, digest);
    lynx__sha256_hex(digest, hex);
    if (strcmp(hex, want_hex) != 0) {
        fprintf(stderr, "%s:%d: sha256 mismatch: got %s want %s\n", __FILE__, __LINE__, hex,
                want_hex);
        check_failures++;
    }
}

static void case_sha256(void)
{
    expect_sha256("", 0u, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    expect_sha256("abc", 3u, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *two_block = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_EQ_U64(strlen(two_block), 56u);
    expect_sha256(two_block, 56u,
                  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    /* One million 'a' bytes fed in 1000-byte chunks. */
    uint8_t chunk[1000];
    memset(chunk, 'a', sizeof chunk);
    lynx__sha256 s;
    uint8_t digest[32];
    char hex[65];
    lynx__sha256_init(&s);
    for (int i = 0; i < 1000; i++) {
        lynx__sha256_update(&s, chunk, sizeof chunk);
    }
    lynx__sha256_final(&s, digest);
    lynx__sha256_hex(digest, hex);
    CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
}

static void case_identity(void)
{
    static const uint8_t zero_hash[32] = {0};
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 0x0123456789abcdefu;
    lynx_machine *m = NULL;
    CHECK_EQ_U64(lynx_machine_init(machine_buf, lynx_machine_size(), &cfg, &m), LYNX_OK);
    if (m == NULL) {
        return;
    }

    lynx_identity id;
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, 0u);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_HEX(id.ram_seed, 0x0123456789abcdefu);
    CHECK_EQ_U64(id.has_content, 0u);
    CHECK_EQ_U64(id.content_addr, 0u);
    CHECK_EQ_U64(id.content_len, 0u);
    CHECK_EQ_U64(id.reserved, 0u);
    CHECK(memcmp(id.content_sha256, zero_hash, 32u) == 0);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_NONE);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_NONE);
    CHECK_EQ_U64(id.entry_addr, 0u);
    CHECK_EQ_U64(id.object_len, 0u);

    CHECK_EQ_U64(load_bll_at(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, 0u);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_HEX(id.ram_seed, 0x0123456789abcdefu);
    CHECK_EQ_U64(id.has_content, 1u);
    CHECK_EQ_HEX(id.content_addr, 0x0200u);
    CHECK_EQ_U64(id.content_len, 32u);
    CHECK_EQ_U64(id.reserved, 0u);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_BS93);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_A);
    CHECK_EQ_U64(id.entry_addr, 0x0200u);
    CHECK_EQ_U64(id.object_len, 42u);
    char hex[65];
    lynx__sha256_hex(id.content_sha256, hex);
    CHECK(strcmp(hex, "bab6f7ea8d9ba129db7e7a776f61a73162ccd67f2348d3a17a31ae7dda31da4e") == 0);

    /* A refused load leaves the identity as it was. */
    uint8_t malformed[sizeof bll_tracer];
    memcpy(malformed, bll_tracer, sizeof malformed);
    malformed[5]--;
    CHECK_EQ_U64(lynx_load_bll(m, malformed, sizeof malformed), LYNX_ERR_SIZE);
    lynx_identity again;
    CHECK_EQ_U64(lynx_get_identity(m, &again), LYNX_OK);
    CHECK(memcmp(&again, &id, sizeof id) == 0);

    /* A different image gives a different hash: the hash follows the bytes. */
    uint8_t other[32];
    memcpy(other, tracer_image, sizeof other);
    other[31] ^= 1u;
    CHECK_EQ_U64(load_bll_at(m, other, sizeof other, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_get_identity(m, &again), LYNX_OK);
    CHECK(memcmp(again.content_sha256, id.content_sha256, 32u) != 0);

    CHECK_EQ_U64(lynx_get_identity(NULL, &id), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_get_identity(m, NULL), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_ERR_STATE);
}

static void case_build_id(void)
{
    const char *id = lynx_build_id();
    CHECK(id != NULL);
    if (id == NULL) {
        return;
    }
    size_t len = strlen(id);
    CHECK(len > 0u);
    CHECK(len < 128u);
    CHECK(strncmp(id, "git=", 4u) == 0);
    CHECK(strstr(id, " cc=") != NULL);
    CHECK(strstr(id, " sys=") != NULL);
    /* No path, date or clock time: none of these characters can appear. */
    CHECK(strchr(id, '/') == NULL);
    CHECK(strchr(id, '~') == NULL);
    CHECK(strchr(id, ':') == NULL);
    /* Fixed for the life of the build: same pointer, same bytes. */
    const char *again = lynx_build_id();
    CHECK(again == id);
    CHECK(strcmp(again, id) == 0);
    CHECK_EQ_U64(strlen(lynx__build_id), len);
}

static lynx_machine *init_in(void *mem, const lynx_config *cfg);

/* ---- Shared helpers for the contract cases (plan 01-06) ------------------

   Every refusal below is proven the same way: copy the whole instance buffer,
   make the call, and memcmp the buffer against the copy. A refusal that
   touches one byte, including padding, fails. The accepted-call controls show
   the comparison can see a change at all. */

#define MSIZE (sizeof(struct lynx_machine))

static _Alignas(max_align_t) unsigned char buf_b[sizeof(struct lynx_machine)];
static unsigned char snap_a[sizeof(struct lynx_machine)];
static unsigned char snap_b[sizeof(struct lynx_machine)];
/* Exact-touch API input starts immediately after this instance. */
static _Alignas(max_align_t) unsigned char bll_adjacent_arena[sizeof(struct lynx_machine) +
                                                              sizeof bll_tracer];
/* Room for a deliberately misaligned start. */
static _Alignas(max_align_t) unsigned char over_buf[sizeof(struct lynx_machine) + 16];
/* One byte more than the address space, for the over-long load refusal. */
static unsigned char big_image[LYNX_ADDRESS_SPACE + 1];
static unsigned char mem_out[LYNX_ADDRESS_SPACE];

static int all_bytes_equal(const unsigned char *p, size_t len, unsigned char v)
{
    for (size_t i = 0u; i < len; i++) {
        if (p[i] != v) {
            return 0;
        }
    }
    return 1;
}

/* Initialises mem with cfg and returns the machine, or NULL after a failed check. */
static lynx_machine *init_in(void *mem, const lynx_config *cfg)
{
    lynx_machine *m = NULL;
    lynx_status st = lynx_machine_init(mem, lynx_machine_size(), cfg, &m);
    CHECK_EQ_U64(st, LYNX_OK);
    if (st != LYNX_OK || m == NULL) {
        return NULL;
    }
    CHECK(m == mem);
    return m;
}

/* The expected whole-buffer comparison: buffer == copy. */
static void check_unchanged(const void *buf, const unsigned char *copy, int line)
{
    if (memcmp(buf, copy, MSIZE) != 0) {
        fprintf(stderr, "%s:%d: instance buffer changed by a call that must not change it\n",
                __FILE__, line);
        check_failures++;
    }
}
#define CHECK_UNCHANGED(buf, copy) check_unchanged((buf), (copy), __LINE__)

/* ---- bounded advance (API-03) ---- */

/* Advances one call and checks what it reports. */
static void advance_expect(lynx_machine *m, uint64_t budget, uint64_t want_consumed,
                           lynx_stop_reason want_stop, int line)
{
    uint64_t consumed = 0xA5A5A5A5u;
    lynx_stop_reason stop = 0xA5A5A5A5u;
    lynx_status st = lynx_advance(m, budget, &consumed, &stop);
    if (st != LYNX_OK || consumed != want_consumed || stop != want_stop || consumed > budget) {
        fprintf(stderr,
                "%s:%d: advance(budget %llu): status %llu consumed %llu stop %llu, want consumed "
                "%llu stop %llu\n",
                __FILE__, line, (unsigned long long)budget, (unsigned long long)st,
                (unsigned long long)consumed, (unsigned long long)stop,
                (unsigned long long)want_consumed, (unsigned long long)want_stop);
        check_failures++;
    }
}
#define ADVANCE_EXPECT(m, b, c, s) advance_expect((m), (b), (c), (s), __LINE__)

static uint64_t ticks_of(const lynx_machine *m)
{
    uint64_t t = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &t), LYNX_OK);
    return t;
}

static uint32_t reg_of(const lynx_machine *m, uint32_t reg)
{
    uint32_t v = 0u;
    CHECK_EQ_U64(lynx_read_register(m, reg, &v), LYNX_OK);
    return v;
}

static void expect_bll_refusal(lynx_machine *m, const uint8_t *bytes, size_t len,
                               lynx_status want)
{
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, bytes, len), want);
    CHECK_UNCHANGED(m, snap_a);
}

static void make_bll_header(uint8_t *object, size_t len, uint16_t load_addr)
{
    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = (uint8_t)(load_addr >> 8);
    object[3] = (uint8_t)load_addr;
    object[4] = (uint8_t)(len >> 8);
    object[5] = (uint8_t)len;
    object[6] = 'B';
    object[7] = 'S';
    object[8] = '9';
    object[9] = '3';
}

/* Package an in-memory payload in the canonical strict BLL object used by
   all normal API workloads. The CPU-vector machine-loop proof has its own
   private adapter in test_cpu_vectors.c. */
static lynx_status load_bll_at(lynx_machine *m, const uint8_t *payload, size_t payload_len,
                               uint32_t load_addr)
{
    if (payload == NULL || payload_len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (load_addr > UINT16_MAX || payload_len > (size_t)UINT16_MAX - 10u ||
        payload_len > (size_t)LYNX_ADDRESS_SPACE - load_addr) {
        return LYNX_ERR_RANGE;
    }
    const size_t object_len = payload_len + 10u;
    uint8_t *object = malloc(object_len);
    if (object == NULL) {
        fprintf(stderr, "%s:%d: cannot allocate temporary BLL object\n", __FILE__, __LINE__);
        check_failures++;
        return LYNX_ERR_SIZE;
    }
    make_bll_header(object, object_len, (uint16_t)load_addr);
    memcpy(object + 10u, payload, payload_len);
    const lynx_status result = lynx_load_bll(m, object, object_len);
    free(object);
    return result;
}

/* CPU-only reads and writes use a BLL program that selects MAPCTL through the
   guest bus before touching one address. The oracle below is hand-derived
   from the four independent Epyx overlay windows; unmapped targets must stop
   before the CPU request completes. */
static int map_target_is_ram(uint8_t mapctl, uint16_t addr)
{
    /* Persistent negative controls deliberately corrupt one expected route.
       The public CPU-request oracle must reject each wrong-map interpretation. */
    if (g_api_fault == API_FAULT_WRONG_MAP_BIT && mapctl == 0u && addr == 0xFC00u) {
        return 1;
    }
    if (g_api_fault == API_FAULT_SPECIAL_ADDRESS_ORDER && mapctl == 0u && addr == 0xFFF8u) {
        return 0;
    }
    if (addr < 0xFC00u || addr == 0xFFF8u) {
        return 1;
    }
    if (addr == 0xFFF9u) {
        return 0; /* MAPCTL is write-only in this provisional guest policy. */
    }
    if (addr <= 0xFCFFu) {
        return (mapctl & 0x01u) != 0u;
    }
    if (addr <= 0xFDFFu) {
        return (mapctl & 0x02u) != 0u;
    }
    if (addr <= 0xFFF7u) {
        return (mapctl & 0x04u) != 0u;
    }
    return (mapctl & 0x08u) != 0u;
}

static void map_read_one(uint8_t mapctl, uint16_t addr)
{
    const uint8_t program[] = {
        0xA9u, mapctl, 0x8Du, 0xF9u, 0xFFu,       /* LDA #mapctl; STA $FFF9 */
        0xADu, (uint8_t)addr, (uint8_t)(addr >> 8), /* LDA $addr */
        0x8Du, 0x00u, 0x03u, 0x4Cu, 0x0Bu, 0x02u /* STA $0300; JMP $020B */
    };
    const uint8_t sentinel = (uint8_t)(0xA5u ^ (uint8_t)addr ^ mapctl);
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[addr] = sentinel;
    m->ram[0x0300u] = 0u;

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    if (map_target_is_ram(mapctl, addr)) {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 512u);
        CHECK_EQ_U64(m->ram[0x0300u], sentinel);
        return;
    }

    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK(consumed < 512u);
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->ram[addr], sentinel);
    memcpy(snap_a, m, MSIZE);
    consumed = UINT64_MAX;
    stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_UNCHANGED(m, snap_a);
}

static void map_write_one(uint8_t mapctl, uint16_t addr)
{
    const uint8_t value = (uint8_t)(0x5Au ^ (uint8_t)addr ^ mapctl);
    const uint8_t sentinel = (uint8_t)~value;
    const uint8_t program[] = {
        0xA9u, mapctl, 0x8Du, 0xF9u, 0xFFu,       /* LDA #mapctl; STA $FFF9 */
        0xA9u, value, 0x8Du, (uint8_t)addr, (uint8_t)(addr >> 8),
        0x4Cu, 0x0Au, 0x02u /* JMP $020A */
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[addr] = sentinel;

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    if (map_target_is_ram(mapctl, addr)) {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 512u);
        CHECK_EQ_U64(m->ram[addr], value);
        return;
    }

    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK(consumed < 512u);
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->ram[addr], sentinel);
    memcpy(snap_a, m, MSIZE);
    consumed = UINT64_MAX;
    stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_UNCHANGED(m, snap_a);
}

static void map_latch_one(uint8_t value)
{
    const uint8_t raw_sentinel = (uint8_t)(value ^ 0xC3u);
    const uint8_t program[] = {
        0xA9u, value, 0x8Du, 0xF9u, 0xFFu, 0x4Cu, 0x05u, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[0xFFF9u] = raw_sentinel;
    ADVANCE_EXPECT(m, 100u, 100u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->mapctl, value);
    CHECK_EQ_U64(m->ram[0xFFF9u], raw_sentinel);
}

static void case_map_matrix(void)
{
    static const uint16_t boundaries[] = {
        0xFBFFu, 0xFC00u, 0xFCFFu, 0xFD00u, 0xFDFFu, 0xFE00u,
        0xFFF7u, 0xFFF8u, 0xFFF9u, 0xFFFAu, 0xFFFFu
    };
    static const uint16_t window_writes[] = {
        0xFC00u, 0xFCFFu, 0xFD00u, 0xFDFFu, 0xFE00u,
        0xFFF7u, 0xFFF8u, 0xFFFAu, 0xFFFFu
    };
    for (uint32_t mapctl = 0u; mapctl < 16u; mapctl++) {
        for (size_t i = 0u; i < sizeof boundaries / sizeof boundaries[0]; i++) {
            const int before = check_failures;
            map_read_one((uint8_t)mapctl, boundaries[i]);
            if (check_failures != before) {
                return;
            }
        }
        for (size_t i = 0u; i < sizeof window_writes / sizeof window_writes[0]; i++) {
            const int before = check_failures;
            map_write_one((uint8_t)mapctl, window_writes[i]);
            if (check_failures != before) {
                return;
            }
        }
    }
    for (uint32_t value = 0u; value <= UINT8_MAX; value++) {
        const int before = check_failures;
        map_latch_one((uint8_t)value);
        if (check_failures != before) {
            return;
        }
    }
    /* Mapped absent vectors must win over profile-none's opcode disagreement. */
    const uint8_t undefined_vector[] = {0x07u};
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, undefined_vector, sizeof undefined_vector, 0xFFFAu), LYNX_OK);
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        CHECK_EQ_U64(lynx_advance(m, 50u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK_EQ_U64(consumed, 0u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFFFAu);
    }
}

static void case_bll_load(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);

    uint8_t short_object[10];
    make_bll_header(short_object, sizeof short_object, 0x0200u);
    expect_bll_refusal(m, short_object, sizeof short_object, LYNX_ERR_SIZE);

    /* The shortest accepted object has exactly one payload byte. Its load
       ends at address-space boundary 0x10000, and its source is freed after
       loading to prove the core copied the byte. */
    uint8_t min_object[11];
    make_bll_header(min_object, sizeof min_object, 0xFFFFu);
    min_object[10] = 0xA5u;
    uint8_t *source = malloc(sizeof min_object);
    CHECK(source != NULL);
    if (source == NULL) {
        goto done;
    }
    memcpy(source, min_object, sizeof min_object);
    CHECK_EQ_U64(lynx_load_bll(m, source, sizeof min_object), LYNX_OK);
    memset(source, 0, sizeof min_object);
    free(source);
    uint8_t end_byte = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFu, &end_byte, 1u), LYNX_OK);
    CHECK_EQ_U64(end_byte, 0xA5u);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_addr, 0xFFFFu);
    CHECK_EQ_U64(id.entry_addr, 0xFFFFu);
    CHECK_EQ_U64(id.content_len, 1u);
    CHECK_EQ_U64(id.object_len, sizeof min_object);

    /* Largest representable total length fits exactly when the payload starts
       at 11: 65535 - 10 payload bytes end at 65536 without wrapping. */
    uint8_t *max_object = malloc(UINT16_MAX);
    CHECK(max_object != NULL);
    if (max_object == NULL) {
        goto done;
    }
    make_bll_header(max_object, UINT16_MAX, 11u);
    memset(max_object + 10u, 0x5Cu, UINT16_MAX - 10u);
    CHECK_EQ_U64(lynx_load_bll(m, max_object, UINT16_MAX), LYNX_OK);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_addr, 11u);
    CHECK_EQ_U64(id.content_len, UINT16_MAX - 10u);
    CHECK_EQ_U64(id.object_len, UINT16_MAX);
    CHECK_EQ_U64(lynx_read_memory(m, 11u, mem_out, UINT16_MAX - 10u), LYNX_OK);
    CHECK(memcmp(mem_out, max_object + 10u, UINT16_MAX - 10u) == 0);
    free(max_object);

    uint8_t past_end[12];
    make_bll_header(past_end, sizeof past_end, 0xFFFFu);
    past_end[10] = 0xAAu;
    past_end[11] = 0xBBu;
    expect_bll_refusal(m, past_end, sizeof past_end, LYNX_ERR_RANGE);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.bll_load\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_bll_alias(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    memset(bll_adjacent_arena, 0xA5, sizeof bll_adjacent_arena);
    lynx_machine *m = init_in(bll_adjacent_arena, &cfg);
    if (m == NULL) {
        goto done;
    }
    uint8_t *const adjacent = bll_adjacent_arena + MSIZE;
    memcpy(adjacent, bll_tracer, sizeof bll_tracer);
    memcpy(snap_a, bll_adjacent_arena, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, adjacent, sizeof bll_tracer), LYNX_OK);
    CHECK(memcmp(snap_a, bll_adjacent_arena, MSIZE) != 0); /* accepted control */
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);

    /* Every overlap with the complete instance, including header, RAM, the
       last byte and a one-byte partial overlap, is refused unchanged. */
    expect_bll_refusal(m, (const uint8_t *)m, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, m->ram + 0x0200u, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, (const uint8_t *)m + MSIZE - 1u, sizeof bll_tracer,
                       LYNX_ERR_ARGUMENT);
    /* Exact touch after the instance is disjoint. A huge size is rejected by
       the length bound without reading the claimed bytes or changing state. */
    expect_bll_refusal(m, adjacent, SIZE_MAX, LYNX_ERR_RANGE);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.bll_alias\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_bll_tracer(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(m->mapctl, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

    lynx_identity id;
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_NONE);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_NONE);
    CHECK_EQ_U64(id.entry_addr, 0u);
    CHECK_EQ_U64(id.object_len, 0u);

    uint8_t source[sizeof bll_tracer];
    memcpy(source, bll_tracer, sizeof source);
    CHECK_EQ_U64(lynx_load_bll(m, source, sizeof source), LYNX_OK);
    CHECK_EQ_U64(reg_of(m, LYNX_REG_PC), 0x0200u);
    CHECK_EQ_U64(m->mapctl, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_BS93);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_A);
    CHECK_EQ_U64(id.content_addr, 0x0200u);
    CHECK_EQ_U64(id.content_len, 13u);
    CHECK_EQ_U64(id.entry_addr, 0x0200u);
    CHECK_EQ_U64(id.object_len, sizeof bll_tracer);
    char hash_hex[65];
    lynx__sha256_hex(id.content_sha256, hash_hex);
    CHECK(strcmp(hash_hex, "50ddb4511395952841333f53031b264adde66d40953df60a2a7c4bcdaf52d279") == 0);

    /* The accepted payload is copied into RAM; changing the caller's object
       cannot alter guest code or the core-computed object identity. */
    source[10] = 0xEAu;
    lynx_identity after_source_change;
    CHECK_EQ_U64(lynx_get_identity(m, &after_source_change), LYNX_OK);
    CHECK(memcmp(after_source_change.content_sha256, id.content_sha256, 32u) == 0);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 300u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 300u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    uint8_t result = 0xA5u;
    CHECK_EQ_U64(lynx_read_memory(m, 0x0400u, &result, 1u), LYNX_OK);
    CHECK_EQ_U64(result, 0xA5u);
    CHECK_EQ_U64(m->mapctl, 0x0Cu);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFF9u, &result, 1u), LYNX_OK);
    CHECK_EQ_U64(result, 0u); /* raw RAM remains distinct from the latch */

    /* Every malformed object is rejected before power-on and leaves the
       already-running instance byte-identical. */
    expect_bll_refusal(m, NULL, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, bll_tracer, 0u, LYNX_ERR_ARGUMENT);
    uint8_t malformed[sizeof bll_tracer + 1u];
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[0] = 0x81u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[1] = 0x00u; /* unsupported/cart-shaped type, not a BS93 load */
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[6] = 'X';
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[5] = 0x16u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_SIZE);
    expect_bll_refusal(m, bll_tracer, sizeof bll_tracer - 1u, LYNX_ERR_SIZE);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[sizeof bll_tracer] = 0u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer + 1u, LYNX_ERR_SIZE);
    const uint8_t empty_payload[10] = {0x80u, 0x08u, 0x02u, 0x00u,
                                       0x00u, 0x0Au, 'B', 'S', '9', '3'};
    expect_bll_refusal(m, empty_payload, sizeof empty_payload, LYNX_ERR_SIZE);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[2] = 0xFFu;
    malformed[3] = 0xF8u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_RANGE);
    expect_bll_refusal(m, (const uint8_t *)m, sizeof *m, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, big_image, LYNX_ADDRESS_SPACE, LYNX_ERR_RANGE);

    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_EQ_U64(m->mapctl, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.bll_tracer\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* ---- init refusal (API-01, API-02) ---- */

static void init_refusal_one(int line, void *mem, size_t size, const lynx_config *cfg,
                             int pass_out, lynx_status want)
{
    /* Both buffers hold a known pattern; a refusal must leave it alone. */
    memset(machine_buf, 0xA5, sizeof machine_buf);
    memset(over_buf, 0xA5, sizeof over_buf);
    lynx_machine *const sentinel = (lynx_machine *)(uintptr_t)0x5A5A5A58u;
    lynx_machine *out = sentinel;
    lynx_status st = lynx_machine_init(mem, size, cfg, pass_out ? &out : NULL);
    if (st != want) {
        fprintf(stderr, "%s:%d: init status %llu, want %llu\n", __FILE__, line,
                (unsigned long long)st, (unsigned long long)want);
        check_failures++;
    }
    if (out != sentinel) {
        fprintf(stderr, "%s:%d: init refusal wrote *out\n", __FILE__, line);
        check_failures++;
    }
    if (!all_bytes_equal(machine_buf, sizeof machine_buf, 0xA5) ||
        !all_bytes_equal(over_buf, sizeof over_buf, 0xA5)) {
        fprintf(stderr, "%s:%d: init refusal wrote to the buffer\n", __FILE__, line);
        check_failures++;
    }
}

static void case_init_refusal(void)
{
    lynx_config good;
    lynx_config_default(&good);
    size_t size = lynx_machine_size();
    lynx_config bad;

    init_refusal_one(__LINE__, NULL, size, &good, 1, LYNX_ERR_ARGUMENT);
    init_refusal_one(__LINE__, machine_buf, size, NULL, 1, LYNX_ERR_ARGUMENT);
    init_refusal_one(__LINE__, machine_buf, size, &good, 0, LYNX_ERR_ARGUMENT);
    init_refusal_one(__LINE__, machine_buf, size - 1u, &good, 1, LYNX_ERR_SIZE);
    init_refusal_one(__LINE__, machine_buf, 0u, &good, 1, LYNX_ERR_SIZE);
    /* Misaligned start with enough room: alignment is the refusal reason. */
    init_refusal_one(__LINE__, over_buf + 1, size + 8u, &good, 1, LYNX_ERR_ALIGNMENT);
    /* Misaligned and too small: the size check comes first. */
    init_refusal_one(__LINE__, machine_buf + 1, size - 1u, &good, 1, LYNX_ERR_SIZE);

    bad = good;
    bad.struct_size += 1u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.struct_size = 0u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.profile = 3u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.profile = 0xFFFFFFFFu;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.ram_policy = 2u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.reserved = 1u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);

    /* Positive control: the same call with good input writes the buffer and *out. */
    memset(machine_buf, 0xA5, sizeof machine_buf);
    lynx_machine *out = NULL;
    CHECK_EQ_U64(lynx_machine_init(machine_buf, size, &good, &out), LYNX_OK);
    CHECK(out == (lynx_machine *)machine_buf);
    CHECK(!all_bytes_equal(machine_buf, sizeof machine_buf, 0xA5));

    /* Profiles 1 and 2 are accepted and reported. */
    for (uint32_t p = LYNX_PROFILE_LYNX1; p <= LYNX_PROFILE_LYNX2; p++) {
        lynx_config c = good;
        c.profile = p;
        out = NULL;
        CHECK_EQ_U64(lynx_machine_init(machine_buf, size, &c, &out), LYNX_OK);
        lynx_identity id;
        CHECK_EQ_U64(lynx_get_identity(out, &id), LYNX_OK);
        CHECK_EQ_U64(id.profile, p);
    }

    /* The config is copied: changing the caller's struct after init changes
       nothing, and a reset still uses the values given at init. */
    lynx_config mine = good;
    mine.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    mine.ram_seed = 5u;
    lynx_machine *m = init_in(machine_buf, &mine);
    lynx_machine *twin = init_in(buf_b, &mine);
    if (m == NULL || twin == NULL) {
        return;
    }
    memcpy(snap_a, machine_buf, MSIZE);
    mine.profile = LYNX_PROFILE_LYNX2;
    mine.ram_seed = 6u;
    mine.ram_policy = LYNX_RAM_POLICY_ZERO;
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, LYNX_PROFILE_NONE);
    CHECK_EQ_U64(id.ram_seed, 5u);
}

/* ---- instance independence and init/deinit idempotency (API-01) ---- */

static _Alignas(max_align_t) unsigned char buf_c[sizeof(struct lynx_machine)];
static unsigned char solo_runs[3][sizeof(struct lynx_machine)];

static void case_instances_independent(void)
{
    lynx_config cfg_a;
    lynx_config cfg_b;
    lynx_config_default(&cfg_a);
    lynx_config_default(&cfg_b);
    cfg_a.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg_a.ram_seed = 1u;
    cfg_b.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg_b.ram_seed = 2u;

    lynx_machine *a = init_in(machine_buf, &cfg_a);
    lynx_machine *b = init_in(buf_b, &cfg_b);
    if (a == NULL || b == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(a, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(load_bll_at(b, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);

    /* Advancing A leaves B's whole buffer alone. */
    memcpy(snap_b, buf_b, MSIZE);
    memcpy(snap_a, machine_buf, MSIZE);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(a, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    CHECK_UNCHANGED(buf_b, snap_b);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0); /* control: A did change */

    /* Loading into A leaves B alone. */
    uint8_t other[16];
    memset(other, 0x11, sizeof other);
    CHECK_EQ_U64(load_bll_at(a, other, sizeof other, 0x4000u), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Resetting A leaves B alone. */
    CHECK_EQ_U64(lynx_reset(a), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Deinitialising A leaves B alone, and B still runs. */
    lynx_machine_deinit(a);
    CHECK_UNCHANGED(buf_b, snap_b);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(b, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_seed, 2u);
    CHECK_EQ_U64(lynx_advance(b, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);

    /* Deinit twice, and deinit of NULL, are no-ops. */
    lynx_machine_deinit(a);
    memcpy(snap_a, machine_buf, MSIZE);
    lynx_machine_deinit(a);
    lynx_machine_deinit(NULL);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Init twice on one buffer with one config gives identical bytes, whatever
       the buffer held before (so no stale byte or padding reaches the result). */
    memset(machine_buf, 0xA5, sizeof machine_buf);
    CHECK(init_in(machine_buf, &cfg_a) != NULL);
    memcpy(snap_a, machine_buf, MSIZE);
    memset(machine_buf, 0x5A, sizeof machine_buf);
    CHECK(init_in(machine_buf, &cfg_a) != NULL);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK(init_in(machine_buf, &cfg_a) != NULL);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* One instance per profile, advanced interleaved in 100-tick slices, each
       ends byte-identical to its own solo run of the same 3000 ticks. */
    static const uint32_t profiles[3] = {LYNX_PROFILE_NONE, LYNX_PROFILE_LYNX1,
                                         LYNX_PROFILE_LYNX2};
    unsigned char *bufs[3] = {machine_buf, buf_b, buf_c};
    lynx_machine *ms[3] = {NULL, NULL, NULL};
    for (size_t i = 0u; i < 3u; i++) {
        lynx_config c = cfg_a;
        c.profile = profiles[i];
        lynx_machine *solo = init_in(bufs[i], &c);
        if (solo == NULL) {
            return;
        }
        CHECK_EQ_U64(load_bll_at(solo, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(solo, 3000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 3000u);
        memcpy(solo_runs[i], bufs[i], MSIZE);
        ms[i] = init_in(bufs[i], &c);
        if (ms[i] == NULL) {
            return;
        }
        CHECK_EQ_U64(load_bll_at(ms[i], churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    }
    for (uint32_t slice = 0u; slice < 30u; slice++) {
        for (size_t i = 0u; i < 3u; i++) {
            CHECK_EQ_U64(lynx_advance(ms[i], 100u, &consumed, &stop), LYNX_OK);
            CHECK_EQ_U64(consumed, 100u);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        }
    }
    for (size_t i = 0u; i < 3u; i++) {
        CHECK_UNCHANGED(bufs[i], solo_runs[i]);
        lynx_identity pid;
        CHECK_EQ_U64(lynx_get_identity(ms[i], &pid), LYNX_OK);
        CHECK_EQ_U64(pid.profile, profiles[i]);
    }
    /* The loop uses only opcodes both Lynx tables agree on, so the three
       profiles reach the same registers and RAM (the instances differ only in
       the stored profile id). */
    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v0 = 0u;
        uint32_t v1 = 1u;
        uint32_t v2 = 2u;
        CHECK_EQ_U64(lynx_read_register(ms[0], r, &v0), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(ms[1], r, &v1), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(ms[2], r, &v2), LYNX_OK);
        CHECK_EQ_HEX(v1, v0);
        CHECK_EQ_HEX(v2, v0);
    }
    CHECK(memcmp(((struct lynx_machine *)machine_buf)->ram, ((struct lynx_machine *)buf_b)->ram,
                 LYNX_ADDRESS_SPACE) == 0);
    CHECK(memcmp(((struct lynx_machine *)machine_buf)->ram, ((struct lynx_machine *)buf_c)->ram,
                 LYNX_ADDRESS_SPACE) == 0);
}

/* ---- load refusal (API-02) ---- */

static void case_load_refusal_unchanged(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 7u;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    memcpy(snap_a, machine_buf, MSIZE);

    /* BLL malformed shape, marker, exact-length and range refusals are
       checked against the entire instance after a prior run. */
    expect_bll_refusal(m, NULL, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, bll_tracer, 0u, LYNX_ERR_ARGUMENT);
    uint8_t short_object[10];
    make_bll_header(short_object, sizeof short_object, 0x0200u);
    expect_bll_refusal(m, short_object, sizeof short_object, LYNX_ERR_SIZE);
    uint8_t wrong_format[sizeof bll_tracer];
    memcpy(wrong_format, bll_tracer, sizeof wrong_format);
    wrong_format[6] = 'X';
    expect_bll_refusal(m, wrong_format, sizeof wrong_format, LYNX_ERR_ARGUMENT);
    uint8_t wrong_length[sizeof bll_tracer];
    memcpy(wrong_length, bll_tracer, sizeof wrong_length);
    wrong_length[5]++;
    expect_bll_refusal(m, wrong_length, sizeof wrong_length, LYNX_ERR_SIZE);
    uint8_t past_end[12];
    make_bll_header(past_end, sizeof past_end, 0xFFFFu);
    past_end[10] = 0xAAu;
    past_end[11] = 0xBBu;
    expect_bll_refusal(m, past_end, sizeof past_end, LYNX_ERR_RANGE);
    expect_bll_refusal(m, big_image, (size_t)UINT16_MAX + 1u, LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_load_bll(NULL, bll_tracer, sizeof bll_tracer), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Control: an accepted canonical object is visible to the comparison. */
    CHECK_EQ_U64(load_bll_at(m, tracer_image, 16u, 0x0300u), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);

    /* BLL may end exactly at 0x10000, but has no raw whole-space form. */
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(load_bll_at(m, big_image, 16u, 0xFFF0u), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(load_bll_at(m, big_image, 1u, 0xFFFFu), LYNX_OK);

    /* A deinitialised machine refuses a load and stays as it was. */
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_ERR_STATE);
    CHECK_UNCHANGED(machine_buf, snap_a);
}

/* ---- inspect purity (API-05) ---- */

static void case_inspect_pure(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 11u;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    memcpy(snap_a, machine_buf, MSIZE);

    uint64_t ticks_before = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_before), LYNX_OK);
    CHECK_EQ_U64(ticks_before, 4096u);

    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v = 0u;
        CHECK_EQ_U64(lynx_read_register(m, r, &v), LYNX_OK);
    }
    /* The two CPU registers carry the documented encoding of the instance's
       fields (white-box), and P reads with bits 4 and 5 set. 4096 ticks end
       inside PLA (cycle 40 of the sixteenth iteration, granted at 4095). */
    const struct lynx_machine *w = (const struct lynx_machine *)machine_buf;
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE),
                 (uint32_t)w->cpu.flags | ((uint32_t)w->cpu.step << 8) |
                     ((uint32_t)w->cpu.opcode << 16) |
                     ((uint32_t)(w->cpu_ready_at - w->guest_ticks) << 24));
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH),
                 (uint32_t)w->cpu.ea | ((uint32_t)w->cpu.tmp << 16) |
                     ((uint32_t)w->cost_model << 24));
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 16, 0x0468u); /* 4 ticks left, opcode PLA */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_P) & 0x30u, 0x30u);
    uint32_t v = 0xA5A5A5A5u;
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_COUNT, &v), LYNX_ERR_RANGE);
    CHECK_EQ_HEX(v, 0xA5A5A5A5u);
    CHECK_EQ_U64(lynx_read_register(m, 0xFFFFFFFFu, &v), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_PC, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_register(NULL, LYNX_REG_PC, &v), LYNX_ERR_ARGUMENT);

    memset(mem_out, 0xA5, sizeof mem_out);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, LYNX_ADDRESS_SPACE), LYNX_OK);
    CHECK(memcmp(mem_out, ((struct lynx_machine *)machine_buf)->ram, LYNX_ADDRESS_SPACE) == 0);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, &m->ram[0x0100u], 1u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* Keep later checks independent when proving the pre-fix implementation
       incorrectly accepts and writes this aliased output. */
    memcpy(machine_buf, snap_a, MSIZE);

    /* A caller-owned output immediately after the machine is disjoint and is
       accepted, including when the two ranges share a boundary. */
    lynx_machine *adjacent = init_in(bll_adjacent_arena, &cfg);
    if (adjacent != NULL) {
        CHECK_EQ_U64(load_bll_at(adjacent, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
        uint8_t *adjacent_out = bll_adjacent_arena + MSIZE;
        CHECK_EQ_U64(lynx_read_memory(adjacent, 0x0200u, adjacent_out, 4u), LYNX_OK);
        CHECK(memcmp(adjacent_out, adjacent->ram + 0x0200u, 4u) == 0);
    }
    memset(mem_out, 0xA5, sizeof mem_out);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFu, mem_out, 2u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0x10000u, mem_out, 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFFFFFu, mem_out, 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, LYNX_ADDRESS_SPACE + 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, SIZE_MAX), LYNX_ERR_RANGE);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0xA5)); /* RANGE writes nothing */
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, mem_out, 0u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, NULL, 4u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_memory(NULL, 0x0200u, mem_out, 4u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFu, mem_out, 1u), LYNX_OK);

    uint64_t ticks_after = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_after), LYNX_OK);
    CHECK_EQ_U64(ticks_after, ticks_before);
    CHECK_EQ_U64(lynx_guest_ticks(m, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_guest_ticks(NULL, &ticks_after), LYNX_ERR_ARGUMENT);

    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.has_content, 1u);
    CHECK_EQ_U64(lynx_get_identity(m, NULL), LYNX_ERR_ARGUMENT);

    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A deinitialised machine: every inspect refuses with STATE, buffer untouched. */
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_PC, &v), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, 4u), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_after), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_ERR_STATE);
    CHECK_UNCHANGED(machine_buf, snap_a);
}

static void capture_inspection_snapshot(const lynx_machine *m, uint8_t *out, size_t expected_size)
{
    size_t size = 0u;
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(m, &size), LYNX_OK);
    CHECK_EQ_U64(size, expected_size);
    CHECK_EQ_U64(lynx_snapshot_save(m, out, expected_size, &written), LYNX_OK);
    CHECK_EQ_U64(written, expected_size);
}

static void case_memory_peek_pure(void)
{
    enum { INSPECT_SNAPSHOT_SIZE = LYNX__SNAPSHOT_HEADER_LEN + LYNX__SNAPSHOT_PAYLOAD_LEN };
    uint8_t before_snapshot[INSPECT_SNAPSHOT_SIZE];
    uint8_t after_snapshot[INSPECT_SNAPSHOT_SIZE];
    const uint8_t select_all_ram[] = {
        0xA9u, 0x8Fu, 0x8Du, 0xF9u, 0xFFu, 0x4Cu, 0x05u, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, select_all_ram, sizeof select_all_ram, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 100u, 100u, LYNX_STOP_BUDGET);
    uint8_t low_ram = 0u;
    lynx_status low_status = lynx_read_cpu_memory(m, 0x0200u, &low_ram, 1u);
    if (low_status != LYNX_OK) {
        CHECK_EQ_U64(low_status, LYNX_OK);
        return;
    }
    CHECK_EQ_HEX(low_ram, 0xA9u);
    uint32_t mapctl = 0u;
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_MAPCTL, &mapctl), LYNX_OK);
    CHECK_EQ_HEX(mapctl, 0x8Fu);

    m->ram[0xFC00u] = 0xC1u;
    m->ram[0xFD00u] = 0xD2u;
    m->ram[0xFE00u] = 0xE3u;
    m->ram[0xFFFAu] = 0xF4u;
    m->ram[0xFFF8u] = 0x38u;
    m->ram[0xFFF9u] = 0x59u;
    static const struct {
        uint16_t addr;
        uint8_t value;
    } mapped_ram[] = {
        {0xFC00u, 0xC1u}, {0xFD00u, 0xD2u}, {0xFE00u, 0xE3u}, {0xFFFAu, 0xF4u}, {0xFFF8u, 0x38u}
    };
    for (size_t i = 0u; i < sizeof mapped_ram / sizeof mapped_ram[0]; i++) {
        uint8_t out = 0xA5u;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, mapped_ram[i].addr, &out, 1u), LYNX_OK);
        CHECK_EQ_U64(out, mapped_ram[i].value);
    }
    uint8_t raw_mapctl_byte = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFF9u, &raw_mapctl_byte, 1u), LYNX_OK);
    CHECK_EQ_HEX(raw_mapctl_byte, 0x59u);

    /* The latch survives snapshots, including bit 7, but mapped guest readback
       remains unavailable. */
    capture_inspection_snapshot(m, before_snapshot, sizeof before_snapshot);
    lynx_machine *m2 = init_in(buf_b, &cfg);
    if (m2 == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m2, select_all_ram, sizeof select_all_ram, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, before_snapshot, sizeof before_snapshot), LYNX_OK);
    CHECK_EQ_U64(lynx_read_register(m2, LYNX_REG_MAPCTL, &mapctl), LYNX_OK);
    CHECK_EQ_HEX(mapctl, 0x8Fu);

    m->mapctl = 0u;
    m->ram[0xFBFFu] = 0xA1u;
    m->ram[0xFC00u] = 0xB2u;
    uint8_t raw_suzy = 0u;
    const lynx_status raw_status =
        g_api_fault == API_FAULT_RAW_AS_MAPPED
            ? lynx_read_cpu_memory(m, 0xFC00u, &raw_suzy, 1u)
            : lynx_read_memory(m, 0xFC00u, &raw_suzy, 1u);
    CHECK_EQ_U64(raw_status, LYNX_OK);
    CHECK_EQ_HEX(raw_suzy, 0xB2u);
    uint64_t ticks_before = ticks_of(m);
    memcpy(snap_a, m, MSIZE);
    capture_inspection_snapshot(m, before_snapshot, sizeof before_snapshot);
    uint8_t crossing[2] = {0x5Au, 0xA5u};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFBFFu, crossing, sizeof crossing),
                 LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_HEX(crossing[0], 0x5Au);
    CHECK_EQ_HEX(crossing[1], 0xA5u);
    uint8_t unavailable = 0xCCu;
    static const uint16_t unavailable_addresses[] = {0xFC00u, 0xFD00u, 0xFE00u, 0xFFF9u, 0xFFFAu};
    for (size_t i = 0u; i < sizeof unavailable_addresses / sizeof unavailable_addresses[0]; i++) {
        unavailable = 0xCCu;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, unavailable_addresses[i], &unavailable, 1u),
                     LYNX_ERR_UNAVAILABLE);
        CHECK_EQ_HEX(unavailable, 0xCCu);
    }
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0x0200u, &m->ram[0x0100u], 1u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFFFFu, crossing, 2u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0x10000u, crossing, 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0u, crossing, 0u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0u, NULL, 1u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_cpu_memory(NULL, 0u, crossing, 1u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(ticks_of(m), ticks_before);
    capture_inspection_snapshot(m, after_snapshot, sizeof after_snapshot);
    CHECK(memcmp(before_snapshot, after_snapshot, sizeof before_snapshot) == 0);

    lynx_machine_deinit(m2);
}

static void rom_read_request(int ram_selected, uint16_t addr, uint8_t map_bit, uint8_t sentinel)
{
    const uint8_t program[] = {
        0xA9u, ram_selected != 0 ? map_bit : 0u, 0x8Du, 0xF9u, 0xFFu,
        0xADu, (uint8_t)addr, (uint8_t)(addr >> 8), 0x8Du, 0x00u, 0x03u,
        0x4Cu, 0x0Bu, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[addr] = sentinel;
    m->ram[0x0300u] = 0u;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    if (ram_selected == 0) {
        if (g_api_fault == API_FAULT_ROM_FALLBACK) {
            /* Counterfactual fallback mutant: it reads the backing RAM and
               completes instead of returning the public unavailable stop. */
            stop = LYNX_STOP_BUDGET;
            consumed = 128u;
            m->ram[0x0300u] = sentinel;
        }
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 128u);
        CHECK_EQ_U64(m->cpu.req_addr, addr);
        CHECK_EQ_HEX(m->ram[addr], sentinel);
        CHECK_EQ_HEX(m->ram[0x0300u], 0u);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 128u);
        CHECK_EQ_HEX(m->ram[0x0300u], sentinel);
    }
    lynx_machine_deinit(m);
}

static void rom_write_request(int ram_selected)
{
    const uint8_t program[] = {
        0xA9u, ram_selected != 0 ? 0x04u : 0x00u, 0x8Du, 0xF9u, 0xFFu,
        0xA9u, 0xA5u, 0x8Du, 0x00u, 0xFEu, 0x4Cu, 0x0Au, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[0xFE00u] = 0x3Cu;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    if (ram_selected == 0) {
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 128u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFE00u);
        CHECK_EQ_HEX(m->ram[0xFE00u], 0x3Cu);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 128u);
        CHECK_EQ_HEX(m->ram[0xFE00u], 0xA5u);
    }
    lynx_machine_deinit(m);
}

static void rom_opcode_request(int ram_selected)
{
    const uint8_t boot_without_map[] = {0x4Cu, 0x00u, 0xFEu};
    const uint8_t boot_with_map[] = {
        0xA9u, 0x04u, 0x8Du, 0xF9u, 0xFFu, 0x4Cu, 0x00u, 0xFEu
    };
    static const uint8_t target_code[] = {
        0xA9u, 0x5Au, 0x8Du, 0x01u, 0x03u, 0x4Cu, 0x00u, 0xFEu
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    const uint8_t *boot = ram_selected != 0 ? boot_with_map : boot_without_map;
    const size_t boot_len = ram_selected != 0 ? sizeof boot_with_map : sizeof boot_without_map;
    CHECK_EQ_U64(load_bll_at(m, boot, boot_len, 0x0200u), LYNX_OK);
    memcpy(&m->ram[0xFE00u], target_code, sizeof target_code);
    m->ram[0x0301u] = 0u;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    if (ram_selected == 0) {
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 128u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFE00u);
        CHECK_EQ_HEX(m->ram[0x0301u], 0u);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 128u);
        CHECK_EQ_HEX(m->ram[0x0301u], 0x5Au);
    }
    lynx_machine_deinit(m);
}

static void case_rom_unavailable(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, bll_tracer + 10u, 13u, 0x0200u), LYNX_OK);
    m->ram[0xFE00u] = 0x3Cu;
    uint8_t out = 0xA5u;
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFE00u, &out, 1u), LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_HEX(out, 0xA5u);
    CHECK_UNCHANGED(m, snap_a);
    uint8_t raw = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFE00u, &raw, 1u), LYNX_OK);
    CHECK_EQ_HEX(raw, 0x3Cu);
    lynx_machine_deinit(m);

    /* The real CPU path refuses mapped absent-ROM reads and writes, vector
       window reads, and ROM opcode fetches before request completion. Each
       corresponding MAPCTL-selected RAM case executes and leaves a marker. */
    rom_read_request(0, 0xFE00u, 0x04u, 0x6Du);
    rom_read_request(1, 0xFE00u, 0x04u, 0x6Du);
    rom_write_request(0);
    rom_write_request(1);
    rom_read_request(0, 0xFFFAu, 0x08u, 0xC7u);
    rom_read_request(1, 0xFFFAu, 0x08u, 0xC7u);
    rom_opcode_request(0);
    rom_opcode_request(1);
}

static void case_watch_post_write(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    const lynx_status configured = lynx_watch_configure(m, 0x0400u, 1u);
    if (configured != LYNX_OK) {
        CHECK_EQ_U64(configured, LYNX_OK);
        return;
    }

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 26u);
    if (g_api_fault == API_FAULT_STOP_BEFORE_STORE) {
        /* Named local mutant: present the state a pre-store stop would expose. */
        m->ram[0x0400u] = 0u;
    }
    if (m->ram[0x0400u] != 0x5Au) {
        fprintf(stderr, "watch returned before store committed\n");
        check_failures++;
    }
    CHECK(consumed <= 26u);
    CHECK_EQ_U64(ticks_of(m), 26u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0205u); /* next LDA opcode: STA completed */
    CHECK_EQ_U64(m->cpu_ready_at, 30u);    /* in-cycle duration remains */

    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0x5Au);
    CHECK_EQ_U64(event.hit_tick, 25u);
    CHECK_EQ_U64(event.return_tick, 26u);

    /* The second store to the same address proves the first request was
       completed, the one-shot was disarmed, and resumption did not replay it. */
    CHECK_EQ_U64(lynx_advance(m, 40u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 40u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0xA6u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.value, 0x5Au);
    CHECK_EQ_U64(event.hit_tick, 25u);
    CHECK_EQ_U64(event.return_tick, 26u);
    lynx_machine_deinit(m);
}

static void case_watch_range_edges(const lynx_config *cfg)
{
    lynx_machine *m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_range_program, sizeof watch_range_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 2u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;

    /* The initial read and preceding $03FF store cannot trigger [0400,0402). */
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->ram[0x03FFu], 0x11u);
    CHECK_EQ_U64(m->ram[0x0400u], 0x22u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0x22u);

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 2u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->ram[0x0401u], 0x33u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0401u);
    CHECK_EQ_U64(event.value, 0x33u);

    /* Exactly 30 more guest ticks include the $0402 store, outside the range. */
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 2u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->ram[0x0402u], 0x44u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);
}

static void case_watch_counting_store(const lynx_config *cfg)
{
    lynx_machine *m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(m->ram[0x0401u], 0u);

    if (g_api_fault == API_FAULT_REPEAT_STORE) {
        /* A bounded white-box mutant rewinds to the just-completed STA before
           rearming. Correct continuation proceeds to INC $0401 instead. */
        m->cpu.pc = 0x0202u;
        m->cpu.opcode = 0u;
        m->cpu.step = 0u;
        m->cpu.ea = 0u;
        m->cpu.tmp = 0u;
        m->cpu.flags = 0u;
        lynx__cpu_repost(&m->cpu, lynx__cpu_table(m->cpu.profile));
        CHECK(lynx__cpu_state_valid(&m->cpu, lynx__cpu_table(m->cpu.profile)));
    }

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    const uint64_t continuation_budget =
        g_api_fault == API_FAULT_REPEAT_STORE ? 25u : 30u;
    CHECK_EQ_U64(lynx_advance(m, continuation_budget, &consumed, &stop), LYNX_OK);
    if (stop != LYNX_STOP_BUDGET || m->ram[0x0401u] != 1u) {
        fprintf(stderr, "watch continuation replayed first store before counter increment\n");
        check_failures++;
    }
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->ram[0x0401u], 1u);
    lynx_machine_deinit(m);

    /* A real store to another address changes RAM without producing a hit. */
    m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0402u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 56u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(m->ram[0x0401u], 1u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);
}

static void case_watch_last_address(const lynx_config *cfg)
{
    lynx_machine *m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    CHECK_EQ_U64(load_bll_at(m, watch_last_address_program,
                             sizeof watch_last_address_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFFFFu, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->mapctl, 0x08u);
    CHECK_EQ_U64(m->ram[0xFFFFu], 0xC7u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0xFFFFu);
    CHECK_EQ_U64(event.value, 0xC7u);
    lynx_machine_deinit(m);
}

static void case_watch_controls(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_configure(NULL, 0x0400u, 1u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_watch_clear(NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_watch_get(NULL, &event), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(event.hit, UINT32_C(0xA5A5A5A5));
    CHECK_EQ_U64(lynx_watch_get(m, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    const uint64_t config_tick = ticks_of(m);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), config_tick);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), config_tick);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_WATCH;
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), config_tick);
    CHECK_EQ_HEX(m->ram[0x0400u], 0u);

    /* The first matching store is due exactly at target=25 and must not run. */
    CHECK_EQ_U64(lynx_advance(m, 25u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 25u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(m->ram[0x0400u], 0u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(event.address, 0u);
    CHECK_EQ_U64(event.value, 0u);
    CHECK_EQ_U64(event.hit_tick, 0u);
    CHECK_EQ_U64(event.return_tick, 0u);

    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFFFFu, 2u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x10000u, 1u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 0u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_watch_get(m, (lynx_watch_event *)(void *)m), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);

    /* Grant 25 is inside the one-tick half-open budget: WATCH wins over BUDGET. */
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(ticks_of(m), 26u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.value, 0x5Au);

    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFFFFu, 2u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(m, snap_a);
    const uint64_t before_rearm = ticks_of(m);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), before_rearm);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(event.address, 0u);
    CHECK_EQ_U64(event.value, 0u);
    CHECK_EQ_U64(event.hit_tick, 0u);
    CHECK_EQ_U64(event.return_tick, 0u);
    CHECK_EQ_U64(lynx_advance(m, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(ticks_of(m), 56u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0xA6u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0xA6u);
    CHECK_EQ_U64(event.hit_tick, 55u);
    CHECK_EQ_U64(event.return_tick, 56u);

    const uint64_t before_clear = ticks_of(m);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), before_clear);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);

    /* Outside-range writes advance normally and leave the event empty. */
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0402u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 100u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);

    /* A due write whose mapped target is unavailable is refused before the
       watch can publish an event or expose hidden backing RAM. */
    static const uint8_t unavailable_write[] = {
        0xA9u, 0x00u, 0x8Du, 0xF9u, 0xFFu, 0xA9u, 0x96u,
        0x8Du, 0x00u, 0xFEu, 0x4Cu, 0x0Au, 0x02u
    };
    m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, unavailable_write, sizeof unavailable_write, 0x0200u),
                 LYNX_OK);
    m->ram[0xFE00u] = 0x3Cu;
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFE00u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_EQ_U64(m->cpu.req_addr, 0xFE00u);
    CHECK_EQ_HEX(m->ram[0xFE00u], 0x3Cu);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);

    /* Successful reset and load clear host-only configuration and hit data. */
    m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);
    lynx_machine_deinit(m);
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_ERR_STATE);
    CHECK_EQ_U64(event.hit, UINT32_C(0xA5A5A5A5));

    case_watch_range_edges(&cfg);
    case_watch_counting_store(&cfg);
    case_watch_last_address(&cfg);
}

static void case_watch_independent(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *a = init_in(machine_buf, &cfg);
    lynx_machine *b = init_in(buf_b, &cfg);
    if (a == NULL || b == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(a, watch_range_program, sizeof watch_range_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(load_bll_at(b, watch_range_program, sizeof watch_range_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(a, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(b, 0x0401u, 1u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event_a;
    lynx_watch_event event_b;

    CHECK_EQ_U64(lynx_advance(a, 80u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(lynx_advance(b, 90u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_watch_get(b, &event_b), LYNX_OK);
    CHECK_EQ_U64(event_b.hit, 0u);
    CHECK_EQ_U64(lynx_advance(a, 10u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_advance(b, 20u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(lynx_watch_get(a, &event_a), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_get(b, &event_b), LYNX_OK);
    CHECK_EQ_U64(event_a.address, 0x0400u);
    CHECK_EQ_U64(event_a.value, 0x22u);
    CHECK_EQ_U64(event_b.address, 0x0401u);
    CHECK_EQ_U64(event_b.value, 0x33u);
    CHECK_EQ_U64(lynx_watch_get(a, &event_a), LYNX_OK);
    CHECK_EQ_U64(event_a.address, 0x0400u);
    CHECK_EQ_U64(event_a.value, 0x22u);
    lynx_machine_deinit(a);
    lynx_machine_deinit(b);
}

/* CPU_STATE as documented in the public header. */
static uint32_t cpu_state_word(uint32_t flags, uint32_t step, uint32_t opcode, uint32_t delta)
{
    return flags | (step << 8) | (opcode << 16) | (delta << 24);
}

static unsigned char sweep_ref[sizeof(struct lynx_machine)];

/* Loads image at addr into an initialised machine (a fresh power-on state). */
static void load_at(lynx_machine *m, const uint8_t *image, size_t len, uint32_t addr)
{
    CHECK_EQ_U64(load_bll_at(m, image, len, addr), LYNX_OK);
}

static void case_advance_budget(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    struct lynx_machine *w = (struct lynx_machine *)machine_buf; /* white-box pokes below */

    /* Before any load: STATE, outputs zeroed, buffer untouched. */
    uint64_t consumed = 0xA5A5A5A5u;
    lynx_stop_reason stop = 0xA5A5A5A5u;
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_ERR_STATE);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);

    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 4096u, NULL, &stop), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_advance(NULL, 4096u, &consumed, &stop), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A budget of 0 consumes nothing and changes nothing. */
    ADVANCE_EXPECT(m, 0u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), 0u);

    /* advance(1) performs one cycle: the opcode fetch of LDX #$00 granted at
       tick 0. PC is past the opcode, the operand read (cycle 1) is pending,
       and the 5-tick cycle has 4 ticks left. */
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), 1u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0201u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 1u, 0xA2u, 4u));
    /* The rest of that cycle's duration starts nothing new. */
    ADVANCE_EXPECT(m, 4u, 4u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0201u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 1u, 0xA2u, 0u));
    /* Half-open: the operand cycle is due exactly at tick 5, the target of a
       0-tick budget, so it is not started. */
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, 0u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* One more tick grants it: LDX #$00 completes at a boundary. */
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0202u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 0u, 0u, 4u));

    /* consumed equals the budget for every budget 1..600, and any two-way split
       of 600 ticks gives the same bytes as one call; so do 600 calls of one. */
    lynx_machine *ref = init_in(buf_b, &cfg);
    if (ref == NULL) {
        return;
    }
    load_at(ref, churn_loop, sizeof churn_loop, 0x0200u);
    ADVANCE_EXPECT(ref, 600u, 600u, LYNX_STOP_BUDGET);
    memcpy(sweep_ref, buf_b, MSIZE);
    for (uint64_t b = 1u; b <= 600u; b++) {
        load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
        ADVANCE_EXPECT(m, b, b, LYNX_STOP_BUDGET);
        ADVANCE_EXPECT(m, 600u - b, 600u - b, LYNX_STOP_BUDGET);
        if (memcmp(machine_buf, sweep_ref, MSIZE) != 0) {
            fprintf(stderr, "%s:%d: split %llu + %llu differs from one 600-tick call\n", __FILE__,
                    __LINE__, (unsigned long long)b, (unsigned long long)(600u - b));
            check_failures++;
        }
    }
    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    for (uint32_t i = 0u; i < 600u; i++) {
        ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    }
    CHECK_UNCHANGED(machine_buf, sweep_ref);

    /* The trap image with the largest budget: UNDEFINED before the first
       opcode, nothing consumed or changed, the same again on a repeat. */
    load_at(m, trap_image, sizeof trap_image, 0x0200u);
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_UNDEFINED);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), 0u);
    /* NOP then the trap: one 2-cycle instruction (10 ticks), then UNDEFINED. */
    static const uint8_t nop_trap[2] = {0xEAu, 0x07u};
    load_at(m, nop_trap, sizeof nop_trap, 0x0200u);
    ADVANCE_EXPECT(m, UINT64_MAX, 10u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0201u);
    CHECK_EQ_U64(ticks_of(m), 10u);

    /* White-box CPU boundary construction only: select RAM for $FFFA-$FFFF
       and seed the JMP high operand at $0000. FFF8/FFF9 keep their fixed routes. */
    load_at(m, wrap_image, sizeof wrap_image, 0xFFFAu);
    m->mapctl = 0x08u;
    m->ram[0x0000u] = 0xFFu;
    ADVANCE_EXPECT(m, 44u, 44u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFFu);
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFFu); /* half-open budget, operand not granted */
    ADVANCE_EXPECT(m, 5u, 5u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0000u); /* low operand fetch wrapped PC */
    ADVANCE_EXPECT(m, 5u, 5u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFAu); /* high operand fetched after wrap */
    for (uint32_t pass = 2u; pass <= 5u; pass++) {
        ADVANCE_EXPECT(m, 55u, 55u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFAu);
        CHECK_EQ_U64(ticks_of(m), 55u * pass);
    }

    /* Guest time near the 64-bit limit (white-box): with less than the headroom
       left nothing runs, nothing is consumed and the clock never wraps. */
    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    w->guest_ticks = UINT64_MAX - 5u;
    w->cpu_ready_at = UINT64_MAX - 5u;
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), UINT64_MAX - 5u);
    /* Seven ticks of room below the headroom: the clamp gives exactly those 7
       (cycles at +0 and +5), then nothing. */
    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    w->guest_ticks = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM - 7u;
    w->cpu_ready_at = w->guest_ticks;
    ADVANCE_EXPECT(m, UINT64_MAX, 7u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0202u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 0u, 0u, 3u));
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_BUDGET);
}

/* ---- reset and power-on state (API-04) ---- */

/* Independent SplitMix64 (Vigna, public domain), written here from the
   published algorithm so the test does not borrow the core's code. */
static uint64_t ref_splitmix64(uint64_t *state)
{
    *state += 0x9e3779b97f4a7c15u;
    uint64_t z = *state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}

static void read_ram(const lynx_machine *m, unsigned char *dst)
{
    CHECK_EQ_U64(lynx_read_memory(m, 0u, dst, LYNX_ADDRESS_SPACE), LYNX_OK);
}

/* The declared power-on CPU values (unverified for the Lynx). */
static void check_power_on_regs(const lynx_machine *m, int line)
{
    static const uint32_t want[LYNX_REG_COUNT] = {0x0000u, 0x00u, 0x00u, 0x00u,
                                                  0xFDu,   0x34u, 0u,    0u};
    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v = 0xA5A5A5A5u;
        lynx_status st = lynx_read_register(m, r, &v);
        if (st != LYNX_OK || v != want[r]) {
            fprintf(stderr, "%s:%d: power-on register %u = 0x%x (status %u), want 0x%x\n", __FILE__,
                    line, (unsigned)r, (unsigned)v, (unsigned)st, (unsigned)want[r]);
            check_failures++;
        }
    }
}

static void case_reset_power_on(void)
{
    /* The default path is fixed 0x00. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    CHECK_EQ_U64(cfg.ram_policy, LYNX_RAM_POLICY_ZERO);
    CHECK_EQ_U64(cfg.ram_seed, 0u);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    read_ram(m, mem_out);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0x00));
    check_power_on_regs(m, __LINE__);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_policy, 0u);
    CHECK_EQ_U64(id.ram_seed, 0u);
    CHECK_EQ_U64(id.has_content, 0u);

    /* Reset after load and advance equals a fresh init, byte for byte. */
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    /* The load starts the CPU at the load address with the power-on registers. */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0200u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_S), 0xFDu);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_P), 0x34u);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0); /* control: the buffer did move */
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    read_ram(m, mem_out);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0x00));
    CHECK_EQ_U64(ticks_of(m), 0u);
    check_power_on_regs(m, __LINE__);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.has_content, 0u);
    /* After a reset there is no content, so advance refuses until a reload. */
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_ERR_STATE);

    /* An UNDEFINED stop is cleared by reset like any other state. */
    CHECK_EQ_U64(load_bll_at(m, trap_image, sizeof trap_image, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 4096u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_ERR_STATE);

    /* The cost model (internal seam) returns to flat5 on reset. */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    memcpy(snap_b, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_MODEL_COUNT), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, 0xFFFFFFFFu), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx__machine_set_cost_model(NULL, LYNX__COST_TEST_SKEW), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_b);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH) >> 24, LYNX__COST_TEST_SKEW);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH), 0u);

    /* Bad magic and NULL: a status and no change. */
    CHECK_EQ_U64(lynx_reset(NULL), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_reset(m), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Reset keeps the profile: a lynx2 instance resets to a fresh lynx2 init. */
    lynx_config l2;
    lynx_config_default(&l2);
    l2.profile = LYNX_PROFILE_LYNX2;
    m = init_in(machine_buf, &l2);
    if (m == NULL) {
        return;
    }
    memcpy(snap_a, machine_buf, MSIZE);
    check_power_on_regs(m, __LINE__);
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, LYNX_PROFILE_LYNX2);

    /* SplitMix64 policy with a seed. */
    const uint64_t seed = 0x0123456789abcdefu;
    lynx_config sm;
    lynx_config_default(&sm);
    sm.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    sm.ram_seed = seed;
    m = init_in(machine_buf, &sm);
    lynx_machine *m2 = init_in(buf_b, &sm);
    if (m == NULL || m2 == NULL) {
        return;
    }
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0); /* two inits, one config */
    memcpy(snap_a, machine_buf, MSIZE);

    /* The first 8 bytes are the first output, little-endian; the next 8 the second. */
    read_ram(m, mem_out);
    uint64_t state = seed;
    for (uint32_t block = 0u; block < 4u; block++) {
        uint64_t want = ref_splitmix64(&state);
        for (uint32_t k = 0u; k < 8u; k++) {
            CHECK_EQ_HEX(mem_out[block * 8u + k], (want >> (8u * k)) & 0xFFu);
        }
    }
    /* Not all zero: the policy really fills. */
    CHECK(!all_bytes_equal(mem_out, sizeof mem_out, 0x00));

    /* Reset after load and advance equals the fresh init; identity reports policy and seed. */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_U64(id.ram_seed, seed);
    CHECK_EQ_U64(id.has_content, 0u);

    /* A different seed gives different RAM. */
    sm.ram_seed = seed + 1u;
    lynx_machine *m3 = init_in(buf_b, &sm);
    if (m3 == NULL) {
        return;
    }
    read_ram(m, mem_out);
    static unsigned char ram_other[LYNX_ADDRESS_SPACE];
    read_ram(m3, ram_other);
    CHECK(memcmp(mem_out, ram_other, LYNX_ADDRESS_SPACE) != 0);

    /* The reset instance reports guest time 0. */
    CHECK_EQ_U64(ticks_of(m), 0u);
}

/* ---- snapshots (API-07) ---- */

#define SNAP_TOTAL ((size_t)LYNX__SNAPSHOT_HEADER_LEN + (size_t)LYNX__SNAPSHOT_PAYLOAD_LEN)
#define SNAP_OFF_PAYLOAD_SHA 220u
#define SNAP_SEED 0x0123456789abcdefu

/* Payload offsets of format 3 (white-box, from core/snapshot.c). */
enum {
    PAY_PC = 0, PAY_P = 6, PAY_FLAGS = 7, PAY_OPCODE = 8, PAY_STEP = 9, PAY_EA = 10, PAY_TMP = 12,
    PAY_COST_MODEL = 13, PAY_MAPCTL = 14, PAY_RESERVED = 15, PAY_TICKS = 16, PAY_READY_AT = 24
};

/* Blobs carry one extra byte of room for the trailing-byte case. */
static unsigned char blob_v[SNAP_TOTAL + 1];
static unsigned char blob_bad[SNAP_TOTAL + 1];
static unsigned char blob_c[SNAP_TOTAL + 1];
static unsigned char at_top[sizeof(struct lynx_machine)];

static void inject_watch_digest(char digest_hex[65], const lynx_watch_event *event)
{
    char material[160];
    const int length = snprintf(material, sizeof material, "%s:%u:%u:%u:%llu:%llu",
                                digest_hex, event->hit, event->address, event->value,
                                (unsigned long long)event->hit_tick,
                                (unsigned long long)event->return_tick);
    if (length > 0 && (size_t)length < sizeof material) {
        lynx__sha256 sha;
        uint8_t bytes[32];
        lynx__sha256_init(&sha);
        lynx__sha256_update(&sha, material, (size_t)length);
        lynx__sha256_final(&sha, bytes);
        lynx__sha256_hex(bytes, digest_hex);
    } else {
        CHECK(0 && "watch digest mutant input must fit");
    }
}

/* Saves m into dst and checks the documented postconditions. Returns 0 on success. */
static int save_into(const lynx_machine *m, unsigned char *dst)
{
    size_t size = 0u;
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(m, &size), LYNX_OK);
    CHECK_EQ_U64(size, SNAP_TOTAL);
    lynx_status st = lynx_snapshot_save(m, dst, SNAP_TOTAL, &written);
    CHECK_EQ_U64(st, LYNX_OK);
    CHECK_EQ_U64(written, SNAP_TOTAL);
    return st == LYNX_OK ? 0 : 1;
}

/* An instance with the cases' shared identity: the given profile, SplitMix64
   and the given seed, the image loaded at addr. Returns NULL after a failed
   check. */
static lynx_machine *snap_instance_p(void *mem, uint32_t profile, uint64_t seed,
                                     const unsigned char *image, size_t len, uint32_t addr)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = seed;
    lynx_machine *m = init_in(mem, &cfg);
    if (m == NULL) {
        return NULL;
    }
    CHECK_EQ_U64(load_bll_at(m, image, len, addr), LYNX_OK);
    return m;
}

static lynx_machine *snap_instance(void *mem, uint64_t seed, const unsigned char *image,
                                   size_t len, uint32_t addr)
{
    return snap_instance_p(mem, LYNX_PROFILE_NONE, seed, image, len, addr);
}

static void advance_ticks(lynx_machine *m, uint64_t ticks)
{
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, ticks, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, ticks);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
}

static void put_le(unsigned char *p, uint64_t v, size_t n)
{
    for (size_t i = 0u; i < n; i++) {
        p[i] = (unsigned char)(v >> (8u * i));
    }
}

static uint64_t get_le(const unsigned char *p, size_t n)
{
    uint64_t v = 0u;
    for (size_t i = 0u; i < n; i++) {
        v |= (uint64_t)p[i] << (8u * i);
    }
    return v;
}

/* Every snapshot field set to a non-default value, restored into a fresh
   instance: the whole buffer must match, and so must the continuation. */
static void snapshot_coverage(void)
{
    lynx_machine *m = snap_instance_p(machine_buf, LYNX_PROFILE_LYNX2, SNAP_SEED, churn_loop,
                                      sizeof churn_loop, 0x0200u);
    if (m == NULL) {
        return;
    }
    struct lynx_machine *w = (struct lynx_machine *)machine_buf;
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    /* Run, one tick at a time, to a point inside an instruction and inside a
       cycle's duration. */
    advance_ticks(m, 1000u);
    for (uint32_t i = 0u; i < 1000u && !(w->cpu.step != 0u && w->cpu_ready_at > w->guest_ticks);
         i++) {
        advance_ticks(m, 1u);
    }
    /* White-box pokes that keep the state one lynx__cpu_state_valid accepts;
       the pending request is reposted, as a restore does. */
    w->cpu.a = 0x5Au;
    w->cpu.x = 0xC3u;
    w->cpu.y = 0x3Cu;
    w->cpu.s = 0x80u;
    w->cpu.p = 0xF7u;
    w->cpu.flags = (uint8_t)LYNX__CS_NMI_PREV;
    w->cpu.ea = 0xBEEFu;
    w->cpu.tmp = 0xA7u;
    const lynx__cpu_decode *table = lynx__cpu_table(w->cpu.profile);
    lynx__cpu_repost(&w->cpu, table);
    CHECK(lynx__cpu_state_valid(&w->cpu, table));
    /* Every field differs from its power-on or boundary default. */
    CHECK(w->cpu.pc != 0x0200u);
    CHECK(w->cpu.opcode != 0u);
    CHECK(w->cpu.step != 0u);
    CHECK(w->cost_model != LYNX__COST_FLAT5);
    CHECK(w->guest_ticks != 0u);
    CHECK(w->cpu_ready_at > w->guest_ticks);
    uint32_t latch = reg_of(m, LYNX_REG_CPU_LATCH);
    CHECK_EQ_HEX(latch, 0xBEEFu | (0xA7u << 16) | ((uint32_t)LYNX__COST_TEST_SKEW << 24));
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) & 0xFFu, LYNX__CS_NMI_PREV);

    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    lynx_machine *m2 = snap_instance_p(buf_b, LYNX_PROFILE_LYNX2, SNAP_SEED, churn_loop,
                                       sizeof churn_loop, 0x0200u);
    if (m2 == NULL) {
        return;
    }
    /* The cost model is identity: a flat5 instance refuses the blob. */
    memcpy(snap_b, buf_b, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_ERR_SNAPSHOT_INVALID);
    CHECK_UNCHANGED(buf_b, snap_b);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m2, LYNX__COST_TEST_SKEW), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
    advance_ticks(m, 3001u);
    advance_ticks(m2, 3001u);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
}

static void case_snapshot_continue(void)
{
    lynx_machine *watched =
        snap_instance(machine_buf, SNAP_SEED, watch_program, sizeof watch_program, 0x0200u);
    lynx_machine *plain =
        snap_instance(buf_b, SNAP_SEED, watch_program, sizeof watch_program, 0x0200u);
    if (watched == NULL || plain == NULL) {
        return;
    }

    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_watch_configure(watched, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(save_into(watched, blob_v), 0u);
    CHECK_EQ_U64(save_into(plain, blob_c), 0u);
    CHECK(memcmp(blob_v, blob_c, SNAP_TOTAL) == 0); /* armed configuration is host-only */
    CHECK_EQ_U64(lynx_advance(watched, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 26u);
    CHECK_EQ_U64(lynx_advance(plain, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 26u);
    CHECK_EQ_U64(ticks_of(watched), 26u);
    CHECK_EQ_U64(ticks_of(plain), 26u);

    /* The runner's observation digest describes the same guest path and final
       tick; watch state is host-only and is deliberately absent. */
    const bundle_run final_run = {NULL, 26u, 26u, LYNX_STOP_BUDGET};
    char watched_digest[65] = {0};
    char plain_digest[65] = {0};
    CHECK_EQ_U64(bundle_observe(watched, &final_run, watched_digest), 0u);
    CHECK_EQ_U64(bundle_observe(plain, &final_run, plain_digest), 0u);
    if (g_api_fault == API_FAULT_WATCH_IN_DIGEST) {
        lynx_watch_event hit;
        CHECK_EQ_U64(lynx_watch_get(watched, &hit), LYNX_OK);
        inject_watch_digest(watched_digest, &hit);
    }
    if (strcmp(watched_digest, plain_digest) != 0) {
        fprintf(stderr, "host watch state leaked into bundle observation digest\n");
        check_failures++;
    }

    /* A hit is host observation only: the two guest states serialize alike. */
    CHECK_EQ_U64(save_into(watched, blob_v), 0u);
    CHECK_EQ_U64(save_into(plain, blob_c), 0u);
    CHECK(memcmp(blob_v, blob_c, SNAP_TOTAL) == 0);

    /* Restoring a matching guest state clears both a prior hit and a newly
       armed watch, while preserving the established post-write tick contract. */
    CHECK_EQ_U64(lynx_snapshot_restore(watched, blob_c, SNAP_TOTAL), LYNX_OK);
    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(watched, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(event.address, 0u);
    CHECK_EQ_U64(event.value, 0u);
    CHECK_EQ_U64(event.hit_tick, 0u);
    CHECK_EQ_U64(event.return_tick, 0u);

    CHECK_EQ_U64(lynx_watch_configure(watched, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(watched, blob_c, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(watched, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(ticks_of(watched), 56u);
    CHECK_EQ_HEX(watched->ram[0x0400u], 0xA6u);

    /* A refusal leaves the active observer configuration byte-for-byte intact. */
    CHECK_EQ_U64(lynx_watch_configure(watched, 0x0410u, 1u), LYNX_OK);
    memcpy(blob_bad, blob_c, SNAP_TOTAL);
    blob_bad[0] ^= 1u;
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_restore(watched, blob_bad, SNAP_TOTAL),
                 LYNX_ERR_SNAPSHOT_INVALID);
    CHECK_UNCHANGED(machine_buf, snap_a);
}

static void case_snapshot_roundtrip(void)
{
    lynx_machine *m = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m == NULL) {
        return;
    }
    /* 2063 = 70 + 7 * 255 + 208: cycle 41 of the eighth iteration (the third
       cycle of PLA) was granted at 2060 and lasts until 2065, so the state is
       inside an instruction and inside a cycle. */
    advance_ticks(m, 2063u);
    uint32_t cs = reg_of(m, LYNX_REG_CPU_STATE);
    CHECK_EQ_HEX((cs >> 16) & 0xFFu, 0x68u);
    CHECK((cs >> 8) & 0xFFu);
    CHECK_EQ_HEX(cs >> 24, 2u);

    /* Size and save leave the instance byte-identical. */
    memcpy(snap_a, machine_buf, MSIZE); /* at_save */
    size_t size = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(m, &size), LYNX_OK);
    CHECK_EQ_U64(size, SNAP_TOTAL);
    CHECK_UNCHANGED(machine_buf, snap_a);
    memset(blob_v, 0xA5, sizeof blob_v);
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_v, size, &written), LYNX_OK);
    CHECK_EQ_U64(written, size);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(blob_v[SNAP_TOTAL], 0xA5u); /* nothing written past the end */
    CHECK(memcmp(blob_v, "LYNXSNAP", 8u) == 0);
    CHECK_EQ_U64(get_le(blob_v + 8u, 4u), 3u); /* format 3 */
    CHECK_EQ_U64(get_le(blob_v + 12u, 4u), LYNX__SNAPSHOT_HEADER_LEN);
    CHECK_EQ_U64(get_le(blob_v + 200u, 4u), LYNX_CONTENT_BS93);
    CHECK_EQ_U64(get_le(blob_v + 204u, 4u), LYNX_STARTUP_A);
    CHECK_EQ_U64(get_le(blob_v + 208u, 4u), 0x0200u);
    CHECK_EQ_U64(get_le(blob_v + 212u, 4u), sizeof churn_loop + 10u);
    CHECK_EQ_U64(get_le(blob_v + 216u, 4u), LYNX__SNAPSHOT_PAYLOAD_LEN);

    /* Saving twice, over different garbage, gives the same bytes. */
    memset(blob_bad, 0x5A, sizeof blob_bad);
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, size, &written), LYNX_OK);
    CHECK(memcmp(blob_v, blob_bad, SNAP_TOTAL) == 0);

    advance_ticks(m, 2048u);
    memcpy(snap_b, machine_buf, MSIZE); /* final */
    CHECK(memcmp(snap_a, snap_b, MSIZE) != 0);

    /* Same instance: restore gives the buffer at save time, then the same continuation. */
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, size), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    advance_ticks(m, 2048u);
    CHECK_UNCHANGED(machine_buf, snap_b);

    /* A second fresh instance, same config and content: restore and continue. */
    lynx_machine *m2 = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m2 == NULL) {
        return;
    }
    CHECK(memcmp(buf_b, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, size), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_a);
    advance_ticks(m2, 2048u);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Capacity: one byte short is refused, nothing written, nothing reported. */
    memset(blob_bad, 0xA5, sizeof blob_bad);
    memcpy(snap_a, machine_buf, MSIZE);
    written = 77u;
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, size - 1u, &written), LYNX_ERR_CAPACITY);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));
    CHECK_EQ_U64(written, 77u);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, 0u, &written), LYNX_ERR_CAPACITY);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));

    /* Argument refusals write nothing. */
    CHECK_EQ_U64(lynx_snapshot_size(NULL, &size), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_size(m, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_save(NULL, blob_bad, SNAP_TOTAL, &written), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_save(m, NULL, SNAP_TOTAL, &written), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, SNAP_TOTAL, NULL), LYNX_ERR_ARGUMENT);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));
    CHECK_EQ_U64(lynx_snapshot_restore(NULL, blob_v, SNAP_TOTAL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_restore(m, NULL, SNAP_TOTAL), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A buffer that overlaps the instance is refused by both calls: inside it,
       reaching into its start, and reaching into its end. A refusal reads and
       writes none of it, so the odd addresses are never dereferenced. */
    unsigned char *inside = machine_buf + 16;
    unsigned char *before = (unsigned char *)(uintptr_t)((uintptr_t)machine_buf - 100u);
    unsigned char *after = machine_buf + MSIZE - 100u;
    unsigned char *overlaps[3] = {inside, before, after};
    for (size_t i = 0u; i < 3u; i++) {
        CHECK_EQ_U64(lynx_snapshot_save(m, overlaps[i], SNAP_TOTAL, &written), LYNX_ERR_ARGUMENT);
        CHECK_UNCHANGED(machine_buf, snap_a);
        CHECK_EQ_U64(lynx_snapshot_restore(m, overlaps[i], SNAP_TOTAL), LYNX_ERR_ARGUMENT);
        CHECK_UNCHANGED(machine_buf, snap_a);
    }

    /* No content loaded: save and restore are refused with STATE; size still answers. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *fresh = init_in(buf_b, &cfg);
    if (fresh == NULL) {
        return;
    }
    memcpy(snap_b, buf_b, MSIZE);
    memset(blob_bad, 0xA5, sizeof blob_bad);
    CHECK_EQ_U64(lynx_snapshot_size(fresh, &size), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_save(fresh, blob_bad, SNAP_TOTAL, &written), LYNX_ERR_STATE);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));
    CHECK_EQ_U64(lynx_snapshot_restore(fresh, blob_v, SNAP_TOTAL), LYNX_ERR_STATE);
    CHECK_UNCHANGED(buf_b, snap_b);
    /* A deinitialised machine: STATE for all three. */
    lynx_machine_deinit(fresh);
    memcpy(snap_b, buf_b, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_size(fresh, &size), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_snapshot_save(fresh, blob_bad, SNAP_TOTAL, &written), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_snapshot_restore(fresh, blob_v, SNAP_TOTAL), LYNX_ERR_STATE);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Top of the address space: a white-box vector-window mapping plus a raw
       $0000 high operand lets the real CPU fetch the JMP operand across wrap. */
    m = snap_instance(machine_buf, SNAP_SEED, wrap_image, sizeof wrap_image, 0xFFFAu);
    m2 = snap_instance(buf_b, SNAP_SEED, wrap_image, sizeof wrap_image, 0xFFFAu);
    if (m == NULL || m2 == NULL) {
        return;
    }
    m->mapctl = 0x08u;
    m->ram[0x0000u] = 0xFFu;
    advance_ticks(m, 50u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0000u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0000u);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    advance_ticks(m, 300u);
    memcpy(at_top, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    advance_ticks(m, 300u);
    CHECK_UNCHANGED(machine_buf, at_top);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_a);
    CHECK_EQ_HEX(reg_of(m2, LYNX_REG_PC), 0x0000u);
    advance_ticks(m2, 5u);
    CHECK_EQ_HEX(reg_of(m2, LYNX_REG_PC), 0xFFFAu); /* fetched through wrap and jumped */
    advance_ticks(m2, 295u);
    CHECK_UNCHANGED(buf_b, at_top);

    /* An UNDEFINED stop survives a snapshot: the restored instance stops at the
       same place with nothing consumed. */
    m = snap_instance(machine_buf, SNAP_SEED, sum_trap, sizeof sum_trap, 0x0200u);
    m2 = snap_instance(buf_b, SNAP_SEED, sum_trap, sizeof sum_trap, 0x0200u);
    if (m == NULL || m2 == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 100000u, 525u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
    memcpy(snap_b, buf_b, MSIZE);
    ADVANCE_EXPECT(m2, 4096u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(buf_b, snap_b);

    snapshot_coverage();
}

static void case_bll_identity(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    lynx_machine *m2 = init_in(buf_b, &cfg);
    if (m2 != NULL) {
        uint8_t relocated[sizeof bll_tracer];
        lynx_identity original_id;
        lynx_identity relocated_id;
        memcpy(relocated, bll_tracer, sizeof relocated);
        relocated[3] = 0x01u; /* header-only change: same payload at $0201 */
        CHECK_EQ_U64(lynx_get_identity(m, &original_id), LYNX_OK);
        CHECK_EQ_U64(lynx_load_bll(m2, relocated, sizeof relocated), LYNX_OK);
        CHECK_EQ_U64(lynx_get_identity(m2, &relocated_id), LYNX_OK);
        CHECK_EQ_U64(relocated_id.content_addr, 0x0201u);
        CHECK_EQ_U64(relocated_id.content_len, original_id.content_len);
        CHECK(memcmp(relocated_id.content_sha256, original_id.content_sha256, 32u) != 0);
        CHECK(memcmp(m->ram + 0x0200u, m2->ram + 0x0201u, 13u) == 0);
        lynx_machine_deinit(m2);
    }
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->mapctl, 0x0Cu);
    CHECK_EQ_U64(save_into(m, blob_v), 0u);

    /* Format 3 carries the complete object identity in the LE header and the
       guest-written MAPCTL latch in the payload. */
    CHECK_EQ_U64(get_le(blob_v + 8u, 4u), 3u);
    CHECK_EQ_U64(get_le(blob_v + 200u, 4u), LYNX_CONTENT_BS93);
    CHECK_EQ_U64(get_le(blob_v + 204u, 4u), LYNX_STARTUP_A);
    CHECK_EQ_U64(get_le(blob_v + 208u, 4u), 0x0200u);
    CHECK_EQ_U64(get_le(blob_v + 212u, 4u), sizeof bll_tracer);
    CHECK_EQ_U64(get_le(blob_v + 216u, 4u), LYNX__SNAPSHOT_PAYLOAD_LEN);
    CHECK_EQ_U64(blob_v[LYNX__SNAPSHOT_HEADER_LEN + 14u], 0x0Cu);
    CHECK_EQ_U64(blob_v[LYNX__SNAPSHOT_HEADER_LEN + 15u], 0u);

    m->mapctl = 0u;
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(m->mapctl, 0x0Cu);

    /* The provisional latch retains all eight guest-written bits. */
    uint8_t all_bits_bll[sizeof bll_tracer];
    memcpy(all_bits_bll, bll_tracer, sizeof all_bits_bll);
    all_bits_bll[11] = 0xFFu; /* LDA #$FF */
    CHECK_EQ_U64(lynx_load_bll(m, all_bits_bll, sizeof all_bits_bll), LYNX_OK);
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->mapctl, 0xFFu);
    CHECK_EQ_U64(save_into(m, blob_c), 0u);
    CHECK_EQ_U64(blob_c[LYNX__SNAPSHOT_HEADER_LEN + PAY_MAPCTL], 0xFFu);
    m->mapctl = 0u;
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_c, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(m->mapctl, 0xFFu);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.bll_identity\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* Recomputes the payload hash of a blob in place, so a test can corrupt a
   value and still pass the hash check. White-box on the layout. */
static void rehash_payload(unsigned char *blob)
{
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, blob + LYNX__SNAPSHOT_HEADER_LEN, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, blob + SNAP_OFF_PAYLOAD_SHA);
}

/* The status a one-bit flip at header offset i must give. */
static lynx_status header_flip_status(size_t i)
{
    if (i < 8u) {
        return LYNX_ERR_SNAPSHOT_INVALID; /* magic */
    }
    if (i < 12u) {
        return LYNX_ERR_SNAPSHOT_VERSION; /* format_version */
    }
    if (i < 16u) {
        return LYNX_ERR_SNAPSHOT_INVALID; /* header_len */
    }
    if (i < 216u) {
        return LYNX_ERR_SNAPSHOT_MISMATCH; /* build, profile, policy, seed, full content identity */
    }
    return LYNX_ERR_SNAPSHOT_INVALID; /* payload_len, payload_sha256 */
}

static void restore_expect(lynx_machine *m, const unsigned char *blob, size_t len,
                           lynx_status want, const unsigned char *ref, int line)
{
    lynx_status st = lynx_snapshot_restore(m, blob, len);
    if (st != want) {
        fprintf(stderr, "%s:%d: snapshot restore status %llu, want %llu\n", __FILE__, line,
                (unsigned long long)st, (unsigned long long)want);
        check_failures++;
    }
    check_unchanged(m, ref, line);
}
#define RESTORE_EXPECT(m, blob, len, want, ref) \
    restore_expect((m), (blob), (len), (want), (ref), __LINE__)

/* Copies src into blob_bad, writes n little-endian bytes of v at payload
   offset off, recomputes the payload hash, and expects the refusal. */
static void value_refused(lynx_machine *m, const unsigned char *src, size_t off, uint64_t v,
                          size_t n, const unsigned char *ref, int line)
{
    memcpy(blob_bad, src, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + off, v, n);
    rehash_payload(blob_bad);
    restore_expect(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, ref, line);
}
#define VALUE_REFUSED(m, src, off, v, n, ref) value_refused((m), (src), (off), (v), (n), (ref), __LINE__)

/* Forged CPU fields in a snapshot whose payload hash is recomputed, so only the
   CPU reachability check can refuse (WR-01, WR-02): a take latch no completion
   leaves set, and a pending micro-operation real execution skips. The control
   blob is a real state of a taken BNE that crosses a page, with the
   page-cross cycle pending; it must restore. */
static void snapshot_forged_cpu_states(void)
{
    static const unsigned char bne_cross[2] = {0xD0u, 0x10u}; /* at $02FD: target $030F */
    lynx_machine *src = snap_instance(machine_buf, SNAP_SEED, bne_cross, sizeof bne_cross, 0x02FDu);
    lynx_machine *dst = snap_instance(buf_b, SNAP_SEED, bne_cross, sizeof bne_cross, 0x02FDu);
    if (src == NULL || dst == NULL) {
        return;
    }
    struct lynx_machine *w = (struct lynx_machine *)machine_buf;
    for (uint32_t i = 0u; i < 100u && w->cpu.step != 3u; i++) {
        advance_ticks(src, 1u);
    }
    CHECK_EQ_U64(w->cpu.opcode, 0xD0u);
    CHECK_EQ_U64(w->cpu.step, 3u);
    CHECK_EQ_HEX(w->cpu.ea, 0x0110u); /* offset with BR_CROSSED */
    CHECK_EQ_U64(save_into(src, blob_c), 0u);
    memcpy(snap_b, buf_b, MSIZE);
    const unsigned char *pay = blob_c + LYNX__SNAPSHOT_HEADER_LEN;
    uint64_t p = pay[PAY_P];
    CHECK((p & 0x02u) == 0u); /* Z clear: the branch is taken */
    /* WR-02: the cross cycle belongs to a taken, crossing branch only. */
    VALUE_REFUSED(dst, blob_c, PAY_P, p | 0x02u, 1u, snap_b);    /* Z set: not taken */
    VALUE_REFUSED(dst, blob_c, PAY_EA, 0x0010u, 2u, snap_b);     /* crossing never recorded */
    VALUE_REFUSED(dst, blob_c, PAY_EA, 0x0210u, 2u, snap_b);     /* BR_SAME_PAGE: cycle skipped */
    VALUE_REFUSED(dst, blob_c, PAY_EA, 0x0310u, 2u, snap_b);
    /* WR-01 on the same state. */
    VALUE_REFUSED(dst, blob_c, PAY_FLAGS, 0x01u, 1u, snap_b);
    VALUE_REFUSED(dst, blob_c, PAY_FLAGS, 0x02u, 1u, snap_b);
    /* Control: the unedited blob restores, so every refusal above was its edit. */
    CHECK_EQ_U64(lynx_snapshot_restore(dst, blob_c, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(((struct lynx_machine *)buf_b)->cpu.step, 3u);
}

/* The guest-time limit through a snapshot (WR-03): a state at the limit is
   reachable and restores, with the last cycle ending up to 255 ticks past it;
   lynx_advance then returns LYNX_OK, LYNX_STOP_BUDGET and consumed 0 (the
   documented signal for the end of guest time). One tick above the limit is
   refused, instance untouched. */
static void snapshot_tick_limit(void)
{
    const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
    lynx_machine *src = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    lynx_machine *dst = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (src == NULL || dst == NULL) {
        return;
    }
    advance_ticks(src, 2063u); /* inside an instruction and inside a cycle */
    CHECK_EQ_U64(save_into(src, blob_c), 0u);
    memcpy(snap_b, buf_b, MSIZE);
    memcpy(blob_bad, blob_c, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit + 1u, 8u);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + 1u, 8u);
    rehash_payload(blob_bad);
    RESTORE_EXPECT(dst, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);
    /* At the limit, with a cycle ending 254 ticks past it. */
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit, 8u);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + 254u, 8u);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_snapshot_restore(dst, blob_bad, SNAP_TOTAL), LYNX_OK);
    uint64_t ticks = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(dst, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, limit);
    uint64_t consumed = 99u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(dst, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_guest_ticks(dst, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, limit);
}

static void case_snapshot_refusal(void)
{
    lynx_machine *m = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m == NULL) {
        return;
    }
    advance_ticks(m, 2063u); /* inside PLA and inside a cycle (see snapshot_roundtrip) */
    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    memcpy(at_top, machine_buf, MSIZE); /* the instance as saved */
    advance_ticks(m, 64u); /* the instance now differs from the blob, so a wrongly applied blob shows */
    memcpy(snap_a, machine_buf, MSIZE);

    /* Every header byte: flip bit 0, restore, expect the field's status, instance unchanged. */
    memcpy(blob_bad, blob_v, SNAP_TOTAL);
    for (size_t i = 0u; i < LYNX__SNAPSHOT_HEADER_LEN; i++) {
        blob_bad[i] ^= 1u;
        lynx_status st = lynx_snapshot_restore(m, blob_bad, SNAP_TOTAL);
        if (st != header_flip_status(i) || st == LYNX_OK) {
            fprintf(stderr, "%s:%d: header byte %zu flip gave status %llu, want %llu\n", __FILE__,
                    __LINE__, i, (unsigned long long)st,
                    (unsigned long long)header_flip_status(i));
            check_failures++;
        }
        CHECK_UNCHANGED(machine_buf, snap_a);
        blob_bad[i] ^= 1u;
    }

    /* Payload bytes: the first, the last and every 251st. The payload hash makes
       any single change detectable, so a stride sample is enough. */
    for (size_t k = 0u; k < LYNX__SNAPSHOT_PAYLOAD_LEN; k += 251u) {
        size_t i = (size_t)LYNX__SNAPSHOT_HEADER_LEN + k;
        blob_bad[i] ^= 1u;
        RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
        blob_bad[i] ^= 1u;
    }
    blob_bad[SNAP_TOTAL - 1u] ^= 1u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    blob_bad[SNAP_TOTAL - 1u] ^= 1u;
    CHECK(memcmp(blob_bad, blob_v, SNAP_TOTAL) == 0); /* every flip was undone */

    /* Lengths: truncated, one extra byte, zero. */
    RESTORE_EXPECT(m, blob_v, SNAP_TOTAL - 1u, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    blob_bad[SNAP_TOTAL] = 0u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL + 1u, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    RESTORE_EXPECT(m, blob_v, 0u, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    RESTORE_EXPECT(m, blob_v, LYNX__SNAPSHOT_HEADER_LEN, LYNX_ERR_SNAPSHOT_INVALID, snap_a);

    /* Formats 1 and 2 are unsupported; format 3 is the current positive case. */
    CHECK_EQ_U64(blob_bad[8], 3u);
    CHECK_EQ_U64(blob_bad[9] | blob_bad[10] | blob_bad[11], 0u);
    blob_bad[8] = 1u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 2u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 3u;

    /* Another identity: each refused as a MISMATCH, whatever the blob holds. */
    unsigned char other_image[sizeof churn_loop];
    memcpy(other_image, churn_loop, sizeof other_image);
    other_image[5] ^= 0x01u;
    lynx_config zero;
    lynx_config_default(&zero);
    for (int variant = 0; variant < 5; variant++) {
        lynx_machine *o = NULL;
        switch (variant) {
        case 0: /* different seed */
            o = snap_instance(buf_b, SNAP_SEED + 1u, churn_loop, sizeof churn_loop, 0x0200u);
            break;
        case 1: /* one different content byte */
            o = snap_instance(buf_b, SNAP_SEED, other_image, sizeof other_image, 0x0200u);
            break;
        case 2: /* different RAM policy (and so seed) */
            o = init_in(buf_b, &zero);
            if (o != NULL) {
                CHECK_EQ_U64(load_bll_at(o, churn_loop, sizeof churn_loop, 0x0200u),
                             LYNX_OK);
            }
            break;
        case 3: /* same bytes at another address */
            o = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0300u);
            break;
        default: /* same bytes, shorter */
            o = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop - 1u, 0x0200u);
            break;
        }
        if (o == NULL) {
            return;
        }
        memcpy(snap_b, buf_b, MSIZE);
        restore_expect(o, blob_v, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_MISMATCH, snap_b, __LINE__);
    }

    /* Every cross-profile pair is an identity mismatch; the same profile is
       accepted (control). */
    for (uint32_t from = 0u; from < 3u; from++) {
        lynx_machine *src = snap_instance_p(buf_b, from, SNAP_SEED, churn_loop, sizeof churn_loop,
                                            0x0200u);
        if (src == NULL) {
            return;
        }
        advance_ticks(src, 500u);
        CHECK_EQ_U64(save_into(src, blob_c), 0u);
        for (uint32_t to = 0u; to < 3u; to++) {
            lynx_machine *dst = snap_instance_p(buf_c, to, SNAP_SEED, churn_loop,
                                                sizeof churn_loop, 0x0200u);
            if (dst == NULL) {
                return;
            }
            memcpy(snap_b, buf_c, MSIZE);
            if (to == from) {
                CHECK_EQ_U64(lynx_snapshot_restore(dst, blob_c, SNAP_TOTAL), LYNX_OK);
                CHECK(memcmp(buf_c, buf_b, MSIZE) == 0);
            } else {
                restore_expect(dst, blob_c, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_MISMATCH, snap_b,
                               __LINE__);
            }
        }
    }

    /* A valid hash is not enough: values are checked after the hash and before
       the apply. Each edit below recomputes the hash so the value check, not
       the hash, refuses. */
    const unsigned char *pay = blob_v + LYNX__SNAPSHOT_HEADER_LEN;
    VALUE_REFUSED(m, blob_v, PAY_P, pay[PAY_P] & ~0x10u, 1u, snap_a);   /* P bit 4 clear */
    VALUE_REFUSED(m, blob_v, PAY_P, pay[PAY_P] & ~0x20u, 1u, snap_a);   /* P bit 5 clear */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x40u, 1u, snap_a);             /* undefined latch bit */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x80u, 1u, snap_a);             /* undefined latch bit */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x01u, 1u, snap_a);             /* IRQ_TAKE: never left set */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x02u, 1u, snap_a);             /* NMI_TAKE: never left set */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x03u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_RESERVED, 0x01u, 1u, snap_a);          /* reserved byte */
    VALUE_REFUSED(m, blob_v, PAY_RESERVED, 0x80u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_STEP, 0xFFu, 1u, snap_a);              /* step past the script */
    VALUE_REFUSED(m, blob_v, PAY_STEP, 0x20u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_OPCODE, 0x07u, 1u, snap_a);            /* undefined under none */
    VALUE_REFUSED(m, blob_v, PAY_COST_MODEL, LYNX__COST_TEST_SKEW, 1u, snap_a); /* not this instance's */
    VALUE_REFUSED(m, blob_v, PAY_COST_MODEL, LYNX__COST_MODEL_COUNT, 1u, snap_a); /* undefined */
    VALUE_REFUSED(m, blob_v, PAY_COST_MODEL, 0xFFu, 1u, snap_a);
    uint64_t ticks = get_le(pay + PAY_TICKS, 8u);
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, ticks + 256u, 8u, snap_a);   /* beyond +255 */
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, ticks - 1u, 8u, snap_a);     /* before guest time */
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, UINT64_MAX, 8u, snap_a);
    /* Guest time above the lynx_advance limit is a state no run reaches (WR-03):
       the span check alone passes these (ready == ticks), the limit check refuses. */
    {
        const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
        static const uint64_t past_limit[] = {1u, 2u, 255u, 0x10000u};
        for (size_t i = 0u; i < sizeof past_limit / sizeof past_limit[0]; i++) {
            memcpy(blob_bad, blob_v, SNAP_TOTAL);
            put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit + past_limit[i], 8u);
            put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + past_limit[i], 8u);
            rehash_payload(blob_bad);
            RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
        }
    }

    /* The reverse cost-model case: a test_skew instance refuses a flat5 blob. */
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    memcpy(snap_b, machine_buf, MSIZE);
    RESTORE_EXPECT(m, blob_v, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_FLAT5), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A boundary state (70 ticks: the prelude done, PC at the loop head): the
       canonical boundary zeros are checked too. */
    lynx_machine *bnd = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (bnd == NULL) {
        return;
    }
    advance_ticks(bnd, 70u);
    CHECK_EQ_HEX(reg_of(bnd, LYNX_REG_CPU_STATE), 0u);
    CHECK_EQ_U64(save_into(bnd, blob_c), 0u);
    VALUE_REFUSED(m, blob_c, PAY_OPCODE, 0xBDu, 1u, snap_a); /* opcode held at a boundary */
    VALUE_REFUSED(m, blob_c, PAY_EA, 0x0001u, 2u, snap_a);
    VALUE_REFUSED(m, blob_c, PAY_TMP, 0x01u, 1u, snap_a);

    /* Positive cases, each a control that the rehash is right and the apply is
       seen: a PC outside the loaded content restores (the CPU may run
       anywhere), as does the largest allowed cpu_ready_at. */
    static const uint16_t outside_pcs[] = {0x0000u, 0x01FFu, 0x0300u, 0xFFFFu};
    for (size_t i = 0u; i < sizeof outside_pcs / sizeof outside_pcs[0]; i++) {
        memcpy(blob_bad, blob_c, SNAP_TOTAL);
        put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_PC, outside_pcs[i], 2u);
        rehash_payload(blob_bad);
        CHECK_EQ_U64(lynx_snapshot_restore(m, blob_bad, SNAP_TOTAL), LYNX_OK);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), outside_pcs[i]);
    }
    memcpy(blob_bad, blob_v, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, ticks + 255u, 8u);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_bad, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 255u);

    /* Control: the untouched blob is accepted and gives the instance as saved,
       so every refusal above was the edit. */
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, at_top);

    snapshot_forged_cpu_states();
    snapshot_tick_limit();
}

/* ---- the CPU programs with hand-derived values (API-03, CPU-02, CPU-03) ---- */

/* Registers PC, A, X, Y, S, P against the hand-derived values. */
static void check_regs(const lynx_machine *m, uint32_t pc, uint32_t a, uint32_t x, uint32_t y,
                       uint32_t s, uint32_t p, int line)
{
    const uint32_t want[6] = {pc, a, x, y, s, p};
    static const char *const names[6] = {"PC", "A", "X", "Y", "S", "P"};
    for (uint32_t r = 0u; r < 6u; r++) {
        uint32_t v = 0xA5A5A5A5u;
        lynx_status st = lynx_read_register(m, r, &v);
        if (st != LYNX_OK || v != want[r]) {
            fprintf(stderr, "%s:%d: %s = 0x%x (status %u), hand-derived 0x%x\n", __FILE__, line,
                    names[r], (unsigned)v, (unsigned)st, (unsigned)want[r]);
            check_failures++;
        }
    }
}
#define CHECK_REGS(m, pc, a, x, y, s, p) check_regs((m), (pc), (a), (x), (y), (s), (p), __LINE__)

static unsigned char want_ram[LYNX_ADDRESS_SPACE];

/* The whole RAM against want_ram, reporting the first difference. */
static void check_ram(const lynx_machine *m, int line)
{
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, LYNX_ADDRESS_SPACE), LYNX_OK);
    for (uint32_t a = 0u; a < LYNX_ADDRESS_SPACE; a++) {
        if (mem_out[a] != want_ram[a]) {
            fprintf(stderr, "%s:%d: RAM $%04x = %02x, hand-derived %02x\n", __FILE__, line,
                    (unsigned)a, (unsigned)mem_out[a], (unsigned)want_ram[a]);
            check_failures++;
            return;
        }
    }
}

static void case_cpu_program(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    CHECK_EQ_U64(cfg.struct_size, sizeof(lynx_config));
    CHECK_EQ_U64(cfg.profile, LYNX_PROFILE_NONE);
    CHECK_EQ_U64(cfg.ram_policy, LYNX_RAM_POLICY_ZERO);
    CHECK_EQ_U64(cfg.ram_seed, 0u);
    CHECK_EQ_U64(cfg.reserved, 0u);
    CHECK(strcmp(lynx_profile_name(LYNX_PROFILE_NONE), "none") == 0);
    CHECK(strcmp(lynx_profile_name(LYNX_PROFILE_LYNX1), "lynx1") == 0);
    CHECK(strcmp(lynx_profile_name(LYNX_PROFILE_LYNX2), "lynx2") == 0);
    CHECK(lynx_profile_name(3u) == NULL);
    CHECK(lynx_profile_name(0xFFFFFFFFu) == NULL);

    size_t size = lynx_machine_size();
    size_t align = lynx_machine_align();
    CHECK(size > 65536u);
    CHECK(is_power_of_two(align));
    CHECK(align <= _Alignof(max_align_t));
    CHECK_EQ_U64(lynx_machine_size(), size);
    CHECK_EQ_U64(lynx_machine_align(), align);
    /* White-box: the two queries must report exactly the real layout. */
    CHECK_EQ_U64(size, sizeof(struct lynx_machine));
    CHECK_EQ_U64(align, _Alignof(struct lynx_machine));
    if (size > sizeof machine_buf) {
        return; /* the next step would overrun the buffer; the checks above already failed */
    }
    CHECK_EQ_U64((uintptr_t)machine_buf % align, 0u);

    /* The embedded programs are the committed fixtures, byte for byte. */
    expect_sha256(sum_trap, sizeof sum_trap, SUM_TRAP_SHA256);
    expect_sha256(churn_loop, sizeof churn_loop, CHURN_LOOP_SHA256);

    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(ticks_of(m), 0u);

    /* sum-trap under none, RAM policy zero. Expected values are the trace table
       in fixtures/cpu/sum-trap.s (datasheet cycle counts, 5 ticks per cycle):
       105 cycles, then the $07 at $021B stops the run before it is fetched. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), 0u);
    ADVANCE_EXPECT(m, 100000u, 525u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(ticks_of(m), 525u);
    CHECK_REGS(m, 0x021Bu, 0x0Fu, 0xFFu, 0x00u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u); /* boundary, nothing in flight */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH), 0u);
    memset(want_ram, 0, sizeof want_ram);
    memcpy(&want_ram[0x0200], sum_trap, sizeof sum_trap);
    want_ram[0x0010] = 0x01u; /* STX $10 of the last iteration */
    want_ram[0x0011] = 0x01u; /* INC $11 */
    want_ram[0x0300] = 0x0Fu; /* STA $0300 */
    want_ram[0x01FC] = 0x11u; /* JSR return address low byte */
    want_ram[0x01FD] = 0x0Fu; /* the JSR high byte $02, overwritten by PHA */
    check_ram(m, __LINE__);
    /* Repeated: the same stop, nothing consumed, the buffer byte-identical. */
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, 100000u, 0u, LYNX_STOP_UNDEFINED);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* advance(524): all 105 cycles are granted (the last, PLA's pull, at 520),
       the registers are already final and one cycle is in flight until 525. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 524u, 524u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x021Bu, 0x0Fu, 0xFFu, 0x00u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 1u << 24);
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
    ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* Exactly 525 in one call: guest time reaches the target at the boundary,
       so BUDGET wins and the next call reports UNDEFINED. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 525u, 525u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_UNDEFINED);
    /* advance(520) stops before the last PLA cycle: A is still 0, S still $FC,
       PC already $021B. The next call performs it and then stops. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 520u, 520u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x021Bu, 0x00u, 0xFFu, 0x00u, 0xFCu, 0x36u);
    ADVANCE_EXPECT(m, 100u, 5u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* churn-loop under none, RAM policy zero, after the 70-tick prelude and one
       255-tick iteration (fixtures/cpu/churn-loop.s). */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 325u, 325u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x020Cu, 0x00u, 0x01u, 0x08u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
    memset(want_ram, 0, sizeof want_ram);
    memcpy(&want_ram[0x0200], churn_loop, sizeof churn_loop);
    want_ram[0x0041] = 0x05u; /* pointer high byte; $0040 = $00 */
    want_ram[0x01FC] = 0x02u; /* JSR return address $021A */
    want_ram[0x01FB] = 0x1Au; /* ($01FD = $00 from PHA, $0600 = $00 from STA) */
    check_ram(m, __LINE__);
    /* After 70 + 255k ticks: PC at the loop head and X = k. */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 70u, 70u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x020Cu, 0x05u, 0x00u, 0x08u, 0xFDu, 0x34u); /* last flag write: LDA #$05 */
    for (uint32_t k = 1u; k <= 20u; k++) {
        ADVANCE_EXPECT(m, 255u, 255u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x020Cu);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_X), k);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_Y), 0x08u);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_S), 0xFDu);
    }

    lynx_machine_deinit(m);
}


/* ---- profile selection and divergence (CPU-02, CPU-05) ---- */

/* The Rockwell R65C02 meaning of the 32 profile-dependent opcodes, written from
   the opcode bits alone (a second statement of the rule, not a read of the
   core's tables): low nibble 7 is the zero-page bit operation, F the bit
   branch; bits 4-6 give the bit number; bit 7 chooses RMB or BBR (0) against
   SMB or BBS (1). */
static int slot_defined(uint8_t op)
{
    return (op & 0x0Fu) == 0x07u || (op & 0x0Fu) == 0x0Fu;
}
static int slot_is_branch(uint8_t op)
{
    return (op & 0x0Fu) == 0x0Fu;
}
static uint8_t slot_mask(uint8_t op)
{
    return (uint8_t)(1u << ((op >> 4) & 7u));
}
static uint8_t slot_result(uint8_t op, uint8_t v)
{
    return (op & 0x80u) != 0u ? (uint8_t)(v | slot_mask(op)) : (uint8_t)(v & (uint8_t)~slot_mask(op));
}
/* BBR branches when the bit is clear, BBS when it is set. */
static int slot_taken(uint8_t op, uint8_t v)
{
    int bit_set = (v & slot_mask(op)) != 0u;
    return (op & 0x80u) != 0u ? bit_set : !bit_set;
}

/* LDA #v; STA $10; then the slot with zero-page operand $10, followed by marker
   code that shows which way execution went:
     bit operation (2 bytes): LDA #$5A; STA $11; JMP self
     bit branch (3 bytes, relative offset 7):
       fall-through at $0207: LDA #$BB; STA $12; JMP self
       branch target $020E:   LDA #$AA; STA $12; JMP self
   Under profile none the slot is never fetched. Under lynx1 the slot is a NOP
   of 2 bytes (bit operations) or 3 bytes (bit branches), so the marker for the
   fall-through path appears and $10 is unchanged. */
static size_t slot_image(uint8_t *img, uint8_t op, uint8_t v)
{
    size_t n = 0u;
    img[n++] = 0xA9u; img[n++] = v;
    img[n++] = 0x85u; img[n++] = 0x10u;
    img[n++] = op;    img[n++] = 0x10u;
    if (slot_is_branch(op)) {
        static const uint8_t tail[] = {0x07u, 0xA9u, 0xBBu, 0x85u, 0x12u, 0x4Cu, 0x0Bu, 0x02u,
                                       0xA9u, 0xAAu, 0x85u, 0x12u, 0x4Cu, 0x12u, 0x02u};
        memcpy(img + n, tail, sizeof tail);
        n += sizeof tail;
    } else {
        static const uint8_t tail[] = {0xA9u, 0x5Au, 0x85u, 0x11u, 0x4Cu, 0x0Au, 0x02u};
        memcpy(img + n, tail, sizeof tail);
        n += sizeof tail;
    }
    return n;
}

static uint8_t ram_byte(const lynx_machine *m, uint32_t addr)
{
    uint8_t b = 0xA5u;
    CHECK_EQ_U64(lynx_read_memory(m, addr, &b, 1u), LYNX_OK);
    return b;
}

static lynx_machine *profile_instance(void *mem, uint32_t profile, const uint8_t *image,
                                      size_t len)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(mem, &cfg);
    if (m == NULL) {
        return NULL;
    }
    CHECK_EQ_U64(load_bll_at(m, image, len, 0x0200u), LYNX_OK);
    return m;
}

/* One slot under one profile with the zero-page cell starting at v. */
static void slot_run(uint32_t profile, uint8_t op, uint8_t v)
{
    uint8_t img[32];
    const size_t len = slot_image(img, op, v);
    const int branch = slot_is_branch(op);
    const uint32_t before = (uint32_t)check_failures;
    lynx_machine *m = profile_instance(machine_buf, profile, img, len);
    if (m == NULL) {
        return;
    }
    if (profile == LYNX_PROFILE_NONE) {
        /* LDA #v and STA $10 are 5 cycles = 25 ticks; the slot is not fetched. */
        ADVANCE_EXPECT(m, 1000u, 25u, LYNX_STOP_UNDEFINED);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0204u);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), v);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
        memcpy(snap_a, machine_buf, MSIZE);
        ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
        CHECK_UNCHANGED(machine_buf, snap_a);
    } else {
        /* Long enough for the slot and the marker code, which then loops. */
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        const int lynx2 = profile == LYNX_PROFILE_LYNX2;
        const uint8_t want10 = lynx2 && !branch ? slot_result(op, v) : v;
        CHECK_EQ_HEX(ram_byte(m, 0x10u), want10);
        if (branch) {
            const uint8_t want12 = (lynx2 && slot_taken(op, v)) ? 0xAAu : 0xBBu;
            CHECK_EQ_HEX(ram_byte(m, 0x12u), want12);
            CHECK_EQ_HEX(ram_byte(m, 0x11u), 0u);
            CHECK_EQ_HEX(reg_of(m, LYNX_REG_A), want12);
        } else {
            CHECK_EQ_HEX(ram_byte(m, 0x11u), 0x5Au);
            CHECK_EQ_HEX(ram_byte(m, 0x12u), 0u);
            CHECK_EQ_HEX(reg_of(m, LYNX_REG_A), 0x5Au);
        }
        if (lynx2) {
            /* Rockwell datasheet counts at flat5 (5 ticks per cycle): RMB and SMB
               take 5 cycles, BBR and BBS 5 cycles plus 1 when taken (same
               page here). Stop exactly after the slot: PC is the next byte. */
            uint64_t slot_cycles = branch ? (slot_taken(op, v) ? 6u : 5u) : 5u;
            uint32_t want_pc = !branch ? 0x0206u : (slot_taken(op, v) ? 0x020Eu : 0x0207u);
            m = profile_instance(machine_buf, profile, img, len);
            if (m != NULL) {
                ADVANCE_EXPECT(m, 25u + 5u * slot_cycles, 25u + 5u * slot_cycles,
                               LYNX_STOP_BUDGET);
                CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), want_pc);
                CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
                CHECK_EQ_HEX(ram_byte(m, 0x10u), branch ? v : slot_result(op, v));
            }
        }
    }
    if ((uint32_t)check_failures != before) {
        fprintf(stderr, "  (profile %u, opcode $%02X, zero-page cell $%02X)\n", (unsigned)profile,
                (unsigned)op, (unsigned)v);
    }
}

static void case_profile_select(void)
{
    /* A host selects the profile at init; the instance reports it and keeps it
       across reset. */
    for (uint32_t profile = LYNX_PROFILE_NONE; profile <= LYNX_PROFILE_LYNX2; profile++) {
        lynx_config cfg;
        lynx_identity id;
        lynx_config_default(&cfg);
        cfg.profile = profile;
        lynx_machine *m = init_in(machine_buf, &cfg);
        if (m == NULL) {
            return;
        }
        CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
        CHECK_EQ_U64(id.profile, profile);
        CHECK(lynx_profile_name(id.profile) != NULL);
        CHECK(strcmp(lynx_profile_name(id.profile),
                     profile == 0u ? "none" : (profile == 1u ? "lynx1" : "lynx2")) == 0);
        CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
        CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
        CHECK_EQ_U64(id.profile, profile);
        lynx_machine_deinit(m);
    }

    /* The named case of the plan: RMB0 $10 on a cell holding $FF. */
    {
        static const uint8_t rmb0[] = {0xA9u, 0xFFu, 0x85u, 0x10u, 0x07u, 0x10u,
                                       0xA9u, 0x5Au, 0x85u, 0x11u, 0x4Cu, 0x0Au, 0x02u};
        lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_LYNX2, rmb0, sizeof rmb0);
        if (m == NULL) {
            return;
        }
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), 0xFEu); /* lynx2: RMB0 cleared bit 0 */
        /* Reset keeps the profile; a reload runs the same program the same way. */
        CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
        CHECK_EQ_U64(load_bll_at(m, rmb0, sizeof rmb0, 0x0200u), LYNX_OK);
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), 0xFEu);
        lynx_machine_deinit(m);
        m = profile_instance(machine_buf, LYNX_PROFILE_LYNX1, rmb0, sizeof rmb0);
        if (m == NULL) {
            return;
        }
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), 0xFFu); /* lynx1: a NOP of 2 bytes */
        CHECK_EQ_HEX(ram_byte(m, 0x11u), 0x5Au);
        lynx_machine_deinit(m);
    }

    /* All 32 slots, every profile, cell values that tell the bit numbers apart
       (all set, all clear, only bit n set, only bit n clear). */
    for (uint32_t op = 0u; op < 256u; op++) {
        if (!slot_defined((uint8_t)op)) {
            continue;
        }
        const uint8_t cells[4] = {0xFFu, 0x00u, slot_mask((uint8_t)op),
                                  (uint8_t)~slot_mask((uint8_t)op)};
        for (uint32_t profile = LYNX_PROFILE_NONE; profile <= LYNX_PROFILE_LYNX2; profile++) {
            for (size_t c = 0u; c < 4u; c++) {
                slot_run(profile, (uint8_t)op, cells[c]);
            }
        }
    }
}

/* The retained 32-byte tracer image (RESEARCH 7.3): one image, three outcomes.
   As code at $0200: 03 is a 1-cycle NOP, 0A is ASL A (2 cycles), 11 18 is ORA
   ($18),Y (5 cycles; RAM is zero, so the pointer is $0000, Y is 0 and the
   operand is 0): 8 cycles = 40 ticks at flat5. Then $1F, a bit-op slot, at $0204. */
static void case_profile_divergence(void)
{
    /* Zero RAM, A = X = Y = 0 and Z set by the ORA: P = $34 | Z = $36. */
    lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_NONE, tracer_image,
                                       sizeof tracer_image);
    if (m == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 1000u, 40u, LYNX_STOP_UNDEFINED);
    CHECK_REGS(m, 0x0204u, 0u, 0u, 0u, 0xFDu, 0x36u);
    ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
    lynx_machine_deinit(m);

    /* lynx1: $1F $26 $2D is a 3-byte NOP of 4 cycles (20 ticks): PC = $0207. */
    m = profile_instance(machine_buf, LYNX_PROFILE_LYNX1, tracer_image, sizeof tracer_image);
    if (m == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 60u, 60u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x0204u + 3u, 0u, 0u, 0u, 0xFDu, 0x36u);
    lynx_machine_deinit(m);

    /* lynx2: $1F is BBR1 $26,+$2D. RAM[$26] is 0, so bit 1 is clear and the
       branch is taken: 6 cycles (30 ticks), PC = $0207 + $2D = $0234. */
    m = profile_instance(machine_buf, LYNX_PROFILE_LYNX2, tracer_image, sizeof tracer_image);
    if (m == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 70u, 70u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x0204u + 3u + tracer_image[6], 0u, 0u, 0u, 0xFDu, 0x36u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0234u);
    lynx_machine_deinit(m);

    /* Under none every one of the 32 bit-op opcodes placed first in an image
       stops with nothing consumed and the buffer unchanged; the other 224 run
       the whole budget, which also exercises every defined opcode under the
       sanitizers. The operand byte $80 points zero-page and absolute operands at
       $0080, so no instruction writes the zero bytes at $0000 (BRK, defined
       everywhere) that a BRK or a wild jump would execute; DEC $00 would turn
       one into $FF, a bit-op slot. */
    uint32_t stopped = 0u;
    uint32_t ran = 0u;
    m = profile_instance(machine_buf, LYNX_PROFILE_NONE, trap_image, sizeof trap_image);
    if (m == NULL) {
        return;
    }
    for (uint32_t op = 0u; op < 256u; op++) {
        const uint8_t img[3] = {(uint8_t)op, 0x80u, 0u};
        CHECK_EQ_U64(load_bll_at(m, img, sizeof img, 0x0200u), LYNX_OK);
        memcpy(snap_a, machine_buf, MSIZE);
        if (slot_defined((uint8_t)op)) {
            ADVANCE_EXPECT(m, 100u, 0u, LYNX_STOP_UNDEFINED);
            CHECK_UNCHANGED(machine_buf, snap_a);
            stopped++;
        } else {
            const uint32_t before = (uint32_t)check_failures;
            ADVANCE_EXPECT(m, 100u, 100u, LYNX_STOP_BUDGET);
            if ((uint32_t)check_failures != before) {
                fprintf(stderr, "  (opcode $%02X first in the image)\n", (unsigned)op);
            }
            ran++;
        }
    }
    CHECK_EQ_U64(stopped, 32u);
    CHECK_EQ_U64(ran, 224u);
    lynx_machine_deinit(m);
}

/* ---- cost model and slicing (CPU-03, ROADMAP criterion 3) ---- */

/* Named-fault switch of the persistent control api.cost_model.self_test: the
   case runs on the flat5 model where it expects test_skew, which is what a
   timing function that ignores its cost table would produce. */
static int g_fault_constant;

/* The test's own copy of the test_skew prices, one per cycle kind in the order
   of enum lynx__ck (RESEARCH section 8). It is deliberately not read from the
   core: a wrong price in the core then shows up as a wrong total. */
static const uint8_t skew_copy[LYNX__CK_COUNT] = {
    [LYNX__CK_OPCODE] = 4,       [LYNX__CK_OPERAND] = 6,   [LYNX__CK_DUMMY_STREAM] = 3,
    [LYNX__CK_DUMMY] = 10,       [LYNX__CK_READ] = 7,      [LYNX__CK_WRITE] = 9,
    [LYNX__CK_RMW_READ] = 8,     [LYNX__CK_STACK_READ] = 11, [LYNX__CK_STACK_WRITE] = 12,
    [LYNX__CK_POINTER] = 13,     [LYNX__CK_VECTOR] = 14,
};

static flat_bus g_fb;
static uint8_t seam_kinds[1024];
static uint8_t seam_replay[LYNX_ADDRESS_SPACE];

typedef struct seam_totals {
    uint64_t cycles;
    uint64_t skew_ticks;      /* sum of the test_skew price over every cycle */
    long long skew_extra;     /* sum of (skew price - 5) over every cycle */
    uint32_t end_pc;
} seam_totals;

/* Runs a program on the flat test bus (the internal seam, profile none) until
   the next instruction is one profile none stops at, or until max_cycles cycles
   have been logged. Records the cycle kinds in order, the sums over the log and
   the replay of every logged write onto the initial RAM. */
static seam_totals seam_run(const uint8_t *image, size_t len, size_t max_cycles)
{
    seam_totals tot;
    const lynx__cpu_decode *t = lynx__cpu_table(LYNX_PROFILE_NONE);
    lynx__cpu cpu;
    memset(&tot, 0, sizeof tot);
    flat_bus_clear(&g_fb);
    flat_bus_load(&g_fb, 0x0200u, image, (unsigned)len);
    memcpy(seam_replay, g_fb.mem, sizeof seam_replay);
    lynx__cpu_init(&cpu, LYNX_PROFILE_NONE);
    lynx__cpu_start_at(&cpu, t, 0x0200u);
    while (tot.cycles < max_cycles && !slot_defined(g_fb.mem[cpu.pc])) {
        flat_bus_reset_log(&g_fb);
        if (!flat_bus_step_instruction(&g_fb, &cpu, t)) {
            fprintf(stderr, "%s: the seam overran its cycle bound\n", __FILE__);
            check_failures++;
            break;
        }
        for (unsigned i = 0u; i < g_fb.nlog; i++) {
            const flat_entry *e = &g_fb.log[i];
            if (tot.cycles < sizeof seam_kinds) {
                seam_kinds[tot.cycles] = e->kind;
            }
            tot.cycles++;
            tot.skew_ticks += skew_copy[e->kind];
            tot.skew_extra += (long long)skew_copy[e->kind] - 5;
            if (e->write != 0u) {
                seam_replay[e->addr] = e->data;
            }
        }
    }
    tot.end_pc = cpu.pc;
    return tot;
}

/* sum-trap on the machine under one cost model. Returns ticks consumed. */
static uint64_t cost_run(uint32_t model, unsigned char *regs_ram)
{
    lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_NONE, sum_trap, sizeof sum_trap);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    if (m == NULL) {
        return 0u;
    }
    /* Fault fault-constant: the machine stays on flat5 where the test expects
       test_skew, as a timing function that ignores its cost table would. */
    if (model != LYNX__COST_FLAT5 && !g_fault_constant) {
        CHECK_EQ_U64(lynx__machine_set_cost_model(m, model), LYNX_OK);
    }
    CHECK_EQ_U64(lynx_advance(m, 100000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNDEFINED);
    /* registers (six words), CPU_STATE, then RAM */
    for (uint32_t r = 0u; r < 7u; r++) {
        put_le(regs_ram + 4u * r, reg_of(m, r), 4u);
    }
    CHECK_EQ_U64(lynx_read_memory(m, 0u, regs_ram + 28u, LYNX_ADDRESS_SPACE), LYNX_OK);
    /* The cost-model byte of CPU_LATCH names the model that ran. */
    CHECK_EQ_U64(reg_of(m, LYNX_REG_CPU_LATCH) >> 24, g_fault_constant ? 0u : model);
    lynx_machine_deinit(m);
    return consumed;
}

static unsigned char cost_flat[28u + LYNX_ADDRESS_SPACE];
static unsigned char cost_skew[28u + LYNX_ADDRESS_SPACE];

static void case_cost_model(void)
{
    /* The seam over the same bytes: cycle kinds, the price sums and the writes. */
    const seam_totals tot = seam_run(sum_trap, sizeof sum_trap, 100000u);
    CHECK_EQ_U64(tot.cycles, 105u);                 /* RESEARCH 7.1: 105 cycles */
    CHECK_EQ_HEX(tot.end_pc, 0x021Bu);              /* the next opcode is the $07 */

    const uint64_t flat = cost_run(LYNX__COST_FLAT5, cost_flat);
    const uint64_t skew = cost_run(LYNX__COST_TEST_SKEW, cost_skew);
    CHECK_EQ_U64(flat, 525u);
    CHECK_EQ_U64(flat, tot.cycles * 5u);

    /* The cost model changes consumed ticks and nothing architectural: the same
       registers, the same CPU position and the same RAM. */
    CHECK(memcmp(cost_flat, cost_skew, sizeof cost_flat) == 0);
    /* RAM equals the replay of the logged writes onto the initial RAM, so no write
       reached memory without passing the bus. */
    CHECK(memcmp(cost_flat + 28u, seam_replay, LYNX_ADDRESS_SPACE) == 0);

    /* The consumed-tick difference is the sum over the log of (price - 5). */
    const long long want_diff = tot.skew_extra;
    const long long got_diff = (long long)skew - (long long)flat;
    CHECK(want_diff > 0);
    if (skew == flat && want_diff != 0) {
        fprintf(stderr,
                "%s:%d: cost model ignored: test_skew consumed %llu ticks, the same as flat5, "
                "but the seam log prices it at %llu\n",
                __FILE__, __LINE__, (unsigned long long)skew,
                (unsigned long long)tot.skew_ticks);
        check_failures++;
    } else if (got_diff != want_diff) {
        fprintf(stderr, "%s:%d: test_skew minus flat5 is %lld ticks, the seam log gives %lld\n",
                __FILE__, __LINE__, got_diff, want_diff);
        check_failures++;
    }
    CHECK_EQ_U64(skew, tot.skew_ticks);

    /* The setter refuses a model this build does not define and writes nothing. */
    lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_NONE, sum_trap, sizeof sum_trap);
    if (m == NULL) {
        return;
    }
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, 2u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, 0xFFFFFFFFu), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx__machine_set_cost_model(NULL, 1u), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
}

static unsigned char slice_ref[sizeof(struct lynx_machine)];

/* Runs churn-loop (seeded RAM) on a fresh load, under the model, for total ticks
   in the given chunk sizes (the last chunk is the remainder). */
static lynx_machine *slice_start(uint32_t model)
{
    lynx_machine *m = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m != NULL && model != LYNX__COST_FLAT5) {
        CHECK_EQ_U64(lynx__machine_set_cost_model(m, model), LYNX_OK);
    }
    return m;
}

static void slice_run(lynx_machine *m, uint64_t total, uint64_t chunk)
{
    uint64_t done = 0u;
    while (done < total) {
        const uint64_t n = total - done < chunk ? total - done : chunk;
        advance_ticks(m, n);
        done += n;
    }
}

static void slice_compare(const char *what, uint64_t arg, uint32_t model)
{
    if (memcmp(machine_buf, slice_ref, MSIZE) != 0) {
        fprintf(stderr, "%s: model %u: %s %llu differs from one 4096-tick call\n", __FILE__,
                (unsigned)model, what, (unsigned long long)arg);
        check_failures++;
    }
}

static void slice_one_model(uint32_t model)
{
    lynx_machine *m = slice_start(model);
    if (m == NULL) {
        return;
    }
    advance_ticks(m, 4096u);
    memcpy(slice_ref, machine_buf, MSIZE);
    CHECK_EQ_U64(ticks_of(m), 4096u);

    static const uint64_t chunks[] = {1u, 3u, 7u, 64u, 255u, 512u};
    for (size_t i = 0u; i < sizeof chunks / sizeof chunks[0]; i++) {
        m = slice_start(model);
        if (m == NULL) {
            return;
        }
        slice_run(m, 4096u, chunks[i]);
        slice_compare("chunk size", chunks[i], model);
    }
    /* Every two-chunk split from 1 to 600. */
    for (uint64_t split = 1u; split <= 600u; split++) {
        m = slice_start(model);
        if (m == NULL) {
            return;
        }
        advance_ticks(m, split);
        advance_ticks(m, 4096u - split);
        slice_compare("split at", split, model);
    }

    /* Grants chain: stepping one tick at a time, the cycle count is read from
       CPU_STATE (bits 24-31, ticks until the next cycle may start). A grant is a
       step after which that delta is one less than the cycle's price, which
       happens only at the tick where the previous cycle ended. g[i+1] must be
       g[i] + ticks[i], with ticks[i] the price of the i-th cycle of the seam
       log under this model. */
    const seam_totals tot = seam_run(churn_loop, sizeof churn_loop, 300u);
    CHECK(tot.cycles >= 300u);
    m = slice_start(model);
    if (m == NULL) {
        return;
    }
    uint64_t grants[320];
    size_t ngrants = 0u;
    uint32_t prev_delta = 0u; /* the first cycle is due at tick 0 */
    for (uint64_t tick = 0u; tick < 600u; tick++) {
        advance_ticks(m, 1u);
        const uint32_t delta = reg_of(m, LYNX_REG_CPU_STATE) >> 24;
        if (prev_delta == 0u) {
            /* The CPU was ready at this tick: a cycle was granted here. */
            if (ngrants < sizeof grants / sizeof grants[0]) {
                const uint8_t kind = seam_kinds[ngrants];
                const uint32_t price = model == LYNX__COST_FLAT5 ? 5u : skew_copy[kind];
                CHECK_EQ_U64(delta + 1u, price); /* the ready delta restarts at the price */
                grants[ngrants++] = tick;
            }
        } else {
            CHECK_EQ_U64(delta, prev_delta - 1u); /* no grant: the delta counts down */
        }
        prev_delta = delta;
    }
    CHECK(ngrants > 40u);
    CHECK_EQ_U64(grants[0], 0u);
    for (size_t i = 0u; i + 1u < ngrants; i++) {
        const uint32_t price = model == LYNX__COST_FLAT5 ? 5u : skew_copy[seam_kinds[i]];
        CHECK_EQ_U64(grants[i + 1u], grants[i] + price);
    }
    /* And the stepped run is the same instance as a single 600-tick call. */
    memcpy(slice_ref, machine_buf, MSIZE);
    m = slice_start(model);
    if (m != NULL) {
        advance_ticks(m, 600u);
        slice_compare("600 one-tick steps against a single call of", 600u, model);
    }
}

static void case_advance_slicing(void)
{
    slice_one_model(LYNX__COST_FLAT5);
    slice_one_model(LYNX__COST_TEST_SKEW);
}

static const struct {
    const char *name;
    void (*run)(void);
} cases[] = {
    {"advance_budget", case_advance_budget},
    {"advance_slicing", case_advance_slicing},
    {"bll_alias", case_bll_alias},
    {"bll_identity", case_bll_identity},
    {"bll_load", case_bll_load},
    {"bll_tracer", case_bll_tracer},
    {"build_id", case_build_id},
    {"cost_model", case_cost_model},
    {"cpu_program", case_cpu_program},
    {"identity", case_identity},
    {"init_refusal", case_init_refusal},
    {"inspect_pure", case_inspect_pure},
    {"instances_independent", case_instances_independent},
    {"map_matrix", case_map_matrix},
    {"memory_peek_pure", case_memory_peek_pure},
    {"load_refusal_unchanged", case_load_refusal_unchanged},
    {"profile_divergence", case_profile_divergence},
    {"profile_select", case_profile_select},
    {"rom_unavailable", case_rom_unavailable},
    {"reset_power_on", case_reset_power_on},
    {"sha256", case_sha256},
    {"snapshot_refusal", case_snapshot_refusal},
    {"snapshot_roundtrip", case_snapshot_roundtrip},
    {"snapshot_continue", case_snapshot_continue},
    {"watch_controls", case_watch_controls},
    {"watch_independent", case_watch_independent},
    {"watch_post_write", case_watch_post_write},
};

int main(int argc, char **argv)
{
    /* Each persistent control dispatches a named, semantically targeted
       mutation. Unknown faults remain usage errors, never passing controls. */
    if (argc == 3) {
        if (strcmp(argv[1], "cost_model") == 0 && strcmp(argv[2], "fault-constant") == 0) {
            g_fault_constant = 1;
        } else if (strcmp(argv[1], "map_matrix") == 0 &&
                   strcmp(argv[2], "fault-wrong-map-bit") == 0) {
            g_api_fault = API_FAULT_WRONG_MAP_BIT;
        } else if (strcmp(argv[1], "map_matrix") == 0 &&
                   strcmp(argv[2], "fault-special-address-order") == 0) {
            g_api_fault = API_FAULT_SPECIAL_ADDRESS_ORDER;
        } else if (strcmp(argv[1], "memory_peek_pure") == 0 &&
                   strcmp(argv[2], "fault-raw-as-mapped") == 0) {
            g_api_fault = API_FAULT_RAW_AS_MAPPED;
        } else if (strcmp(argv[1], "rom_unavailable") == 0 &&
                   strcmp(argv[2], "fault-fallback-to-ram") == 0) {
            g_api_fault = API_FAULT_ROM_FALLBACK;
        } else if (strcmp(argv[1], "watch_post_write") == 0 &&
                   strcmp(argv[2], "fault-stop-before-store") == 0) {
            g_api_fault = API_FAULT_STOP_BEFORE_STORE;
        } else if (strcmp(argv[1], "watch_controls") == 0 &&
                   strcmp(argv[2], "fault-repeat-store") == 0) {
            g_api_fault = API_FAULT_REPEAT_STORE;
        } else if (strcmp(argv[1], "snapshot_continue") == 0 &&
                   strcmp(argv[2], "fault-watch-in-digest") == 0) {
            g_api_fault = API_FAULT_WATCH_IN_DIGEST;
        } else {
            fprintf(stderr, "usage: lynx-test-api <case> [named-fault]\n");
            return 2;
        }
    } else if (argc != 2) {
        fprintf(stderr, "usage: lynx-test-api <case> [named-fault]\n");
        return 2;
    }
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            cases[i].run();
            CHECK_DONE();
        }
    }
    fprintf(stderr, "lynx-test-api: unknown case %s\n", argv[1]);
    return 2;
}
