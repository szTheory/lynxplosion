/* API tests. One executable, one case per CTest entry: `lynx-test-api <case>`.
   An unknown case exits 2, so a typo in tests.cmake fails instead of passing.

   The cases use the public header. Where a case also needs the layout of the
   instance it says so and includes core/internal.h for sizeof and _Alignof
   only (a white-box check of lynx_machine_size and lynx_machine_align). */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "internal.h"
#include "lynxplosion.h"
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
   case "skeleton" checks the SHA-256 of these bytes against the fixture
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

/* A top-of-memory loop at $FFF0: NOP x13, then JMP $FFF0 whose operand ends
   at $FFFF, so the program counter wraps through $0000 inside the JMP.
   13 NOPs of 2 cycles and a 3-cycle JMP: 29 cycles, 145 ticks per pass. */
static const uint8_t wrap_image[16] = {
    0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
    0x4c, 0xf0, 0xff,
};

/* One byte: the bit-op slot $07, so profile none stops before it. */
static const uint8_t trap_image[1] = {0x07};

/* Instance memory for the tests: aligned for any type, sized from the core's
   own layout. */
static _Alignas(max_align_t) unsigned char machine_buf[sizeof(struct lynx_machine)];

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

    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, 0u);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_HEX(id.ram_seed, 0x0123456789abcdefu);
    CHECK_EQ_U64(id.has_content, 1u);
    CHECK_EQ_HEX(id.content_addr, 0x0200u);
    CHECK_EQ_U64(id.content_len, 32u);
    CHECK_EQ_U64(id.reserved, 0u);
    char hex[65];
    lynx__sha256_hex(id.content_sha256, hex);
    CHECK(strcmp(hex, "ab5f8b5cb9435354c7b58603592d5faf081e17ceb05f7a7c67f4b666f12ca457") == 0);

    /* A refused load leaves the identity as it was. */
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, LYNX_ADDRESS_SPACE),
                 LYNX_ERR_RANGE);
    lynx_identity again;
    CHECK_EQ_U64(lynx_get_identity(m, &again), LYNX_OK);
    CHECK(memcmp(&again, &id, sizeof id) == 0);

    /* A different image gives a different hash: the hash follows the bytes. */
    uint8_t other[32];
    memcpy(other, tracer_image, sizeof other);
    other[31] ^= 1u;
    CHECK_EQ_U64(lynx_load_ram_image(m, other, sizeof other, 0x0200u), LYNX_OK);
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

/* ---- Shared helpers for the contract cases (plan 01-06) ------------------

   Every refusal below is proven the same way: copy the whole instance buffer,
   make the call, and memcmp the buffer against the copy. A refusal that
   touches one byte, including padding, fails. The accepted-call controls show
   the comparison can see a change at all. */

#define MSIZE (sizeof(struct lynx_machine))

static _Alignas(max_align_t) unsigned char buf_b[sizeof(struct lynx_machine)];
static unsigned char snap_a[sizeof(struct lynx_machine)];
static unsigned char snap_b[sizeof(struct lynx_machine)];
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
    CHECK_EQ_U64(lynx_load_ram_image(a, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_load_ram_image(b, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);

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
    CHECK_EQ_U64(lynx_load_ram_image(a, other, sizeof other, 0x4000u), LYNX_OK);
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
        CHECK_EQ_U64(lynx_load_ram_image(solo, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(solo, 3000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 3000u);
        memcpy(solo_runs[i], bufs[i], MSIZE);
        ms[i] = init_in(bufs[i], &c);
        if (ms[i] == NULL) {
            return;
        }
        CHECK_EQ_U64(lynx_load_ram_image(ms[i], churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    memcpy(snap_a, machine_buf, MSIZE);

    CHECK_EQ_U64(lynx_load_ram_image(m, NULL, 32u, 0x0200u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, 0u, 0x0200u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(m, big_image, LYNX_ADDRESS_SPACE + 1u, 0u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, SIZE_MAX, 0x10u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, LYNX_ADDRESS_SPACE),
                 LYNX_ERR_RANGE);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0xFFFFFFFFu),
                 LYNX_ERR_RANGE);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* Runs one byte past 0xFFFF. */
    CHECK_EQ_U64(lynx_load_ram_image(m, big_image, 17u, 0xFFF0u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(m, big_image, 2u, 0xFFFFu), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_load_ram_image(NULL, tracer_image, sizeof tracer_image, 0x0200u),
                 LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Control: an accepted load is visible to the comparison. */
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, 16u, 0x0300u), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);

    /* Boundaries that are accepted: exactly to 0x10000, and the whole space. */
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_load_ram_image(m, big_image, 16u, 0xFFF0u), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(lynx_load_ram_image(m, big_image, LYNX_ADDRESS_SPACE, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_load_ram_image(m, big_image, 1u, 0xFFFFu), LYNX_OK);

    /* A deinitialised machine refuses a load and stays as it was. */
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u),
                 LYNX_ERR_STATE);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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

/* CPU_STATE as documented in the public header. */
static uint32_t cpu_state_word(uint32_t flags, uint32_t step, uint32_t opcode, uint32_t delta)
{
    return flags | (step << 8) | (opcode << 16) | (delta << 24);
}

static unsigned char sweep_ref[sizeof(struct lynx_machine)];

/* Loads image at addr into an initialised machine (a fresh power-on state). */
static void load_at(lynx_machine *m, const uint8_t *image, size_t len, uint32_t addr)
{
    CHECK_EQ_U64(lynx_load_ram_image(m, image, len, addr), LYNX_OK);
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

    /* The top-of-memory loop runs across $FFFF without leaving its 16 bytes. */
    load_at(m, wrap_image, sizeof wrap_image, 0xFFF0u);
    /* 28 cycles start before tick 140: 26 NOP cycles, the JMP opcode and its
       low operand byte, so PC is at the high byte. */
    ADVANCE_EXPECT(m, 140u, 140u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFFu);
    /* The high byte is fetched at tick 140: PC wraps and the JMP lands. */
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFF0u);
    ADVANCE_EXPECT(m, 4u, 4u, LYNX_STOP_BUDGET);
    for (uint32_t pass = 2u; pass <= 5u; pass++) {
        ADVANCE_EXPECT(m, 145u, 145u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFF0u);
        CHECK_EQ_U64(ticks_of(m), 145u * pass);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, trap_image, sizeof trap_image, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 4096u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_ERR_STATE);

    /* The cost model (internal seam) returns to flat5 on reset. */
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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
#define SNAP_OFF_PAYLOAD_SHA 204u
#define SNAP_SEED 0x0123456789abcdefu

/* Payload offsets of format 2 (white-box, from core/snapshot.c). */
enum {
    PAY_PC = 0, PAY_P = 6, PAY_FLAGS = 7, PAY_OPCODE = 8, PAY_STEP = 9, PAY_EA = 10, PAY_TMP = 12,
    PAY_COST_MODEL = 13, PAY_RESERVED = 14, PAY_TICKS = 16, PAY_READY_AT = 24
};

/* Blobs carry one extra byte of room for the trailing-byte case. */
static unsigned char blob_v[SNAP_TOTAL + 1];
static unsigned char blob_bad[SNAP_TOTAL + 1];
static unsigned char blob_c[SNAP_TOTAL + 1];
static unsigned char at_top[sizeof(struct lynx_machine)];

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
    CHECK_EQ_U64(lynx_load_ram_image(m, image, len, addr), LYNX_OK);
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
    CHECK_EQ_U64(get_le(blob_v + 8u, 4u), 2u); /* format 2 */
    CHECK_EQ_U64(get_le(blob_v + 200u, 4u), 65568u);

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

    /* Top of the address space: the wrap loop at 0xFFF0, saved inside the JMP
       with PC at its last byte 0xFFFF (140 ticks), restored and continued. */
    m = snap_instance(machine_buf, SNAP_SEED, wrap_image, sizeof wrap_image, 0xFFF0u);
    m2 = snap_instance(buf_b, SNAP_SEED, wrap_image, sizeof wrap_image, 0xFFF0u);
    if (m == NULL || m2 == NULL) {
        return;
    }
    advance_ticks(m, 140u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFFu);
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
    CHECK_EQ_HEX(reg_of(m2, LYNX_REG_PC), 0xFFFFu);
    advance_ticks(m2, 5u);
    CHECK_EQ_HEX(reg_of(m2, LYNX_REG_PC), 0xFFF0u); /* wrapped and jumped */
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
    if (i < 200u) {
        return LYNX_ERR_SNAPSHOT_MISMATCH; /* build id, profile, policy, seed, content */
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

    /* Format 1 (the Phase 1 skeleton) and format 3: version errors. */
    CHECK_EQ_U64(blob_bad[8], 2u);
    CHECK_EQ_U64(blob_bad[9] | blob_bad[10] | blob_bad[11], 0u);
    blob_bad[8] = 1u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 3u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 2u;

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
                CHECK_EQ_U64(lynx_load_ram_image(o, churn_loop, sizeof churn_loop, 0x0200u),
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
    VALUE_REFUSED(m, blob_v, PAY_RESERVED, 0x0001u, 2u, snap_a);        /* reserved bytes */
    VALUE_REFUSED(m, blob_v, PAY_RESERVED, 0x8000u, 2u, snap_a);
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

static void case_skeleton(void)
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
    CHECK_EQ_U64(lynx_load_ram_image(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 524u, 524u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x021Bu, 0x0Fu, 0xFFu, 0x00u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 1u << 24);
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
    ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* Exactly 525 in one call: guest time reaches the target at the boundary,
       so BUDGET wins and the next call reports UNDEFINED. */
    CHECK_EQ_U64(lynx_load_ram_image(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 525u, 525u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_UNDEFINED);
    /* advance(520) stops before the last PLA cycle: A is still 0, S still $FC,
       PC already $021B. The next call performs it and then stops. */
    CHECK_EQ_U64(lynx_load_ram_image(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 520u, 520u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x021Bu, 0x00u, 0xFFu, 0x00u, 0xFCu, 0x36u);
    ADVANCE_EXPECT(m, 100u, 5u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* churn-loop under none, RAM policy zero, after the 70-tick prelude and one
       255-tick iteration (fixtures/cpu/churn-loop.s). */
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
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

static const struct {
    const char *name;
    void (*run)(void);
} cases[] = {
    {"advance_budget", case_advance_budget},
    {"build_id", case_build_id},
    {"identity", case_identity},
    {"init_refusal", case_init_refusal},
    {"inspect_pure", case_inspect_pure},
    {"instances_independent", case_instances_independent},
    {"load_refusal_unchanged", case_load_refusal_unchanged},
    {"reset_power_on", case_reset_power_on},
    {"sha256", case_sha256},
    {"snapshot_refusal", case_snapshot_refusal},
    {"snapshot_roundtrip", case_snapshot_roundtrip},
    {"skeleton", case_skeleton},
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: lynx-test-api <case>\n");
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
