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

/* The provisional tracer image: byte i = (7*i + 3) mod 256 for i = 0..31.
   It holds no 0xDB, so the skeleton never halts on it. */
static const uint8_t tracer_image[32] = {
    0x03, 0x0a, 0x11, 0x18, 0x1f, 0x26, 0x2d, 0x34, 0x3b, 0x42, 0x49,
    0x50, 0x57, 0x5e, 0x65, 0x6c, 0x73, 0x7a, 0x81, 0x88, 0x8f, 0x96,
    0x9d, 0xa4, 0xab, 0xb2, 0xb9, 0xc0, 0xc7, 0xce, 0xd5, 0xdc,
};

/* Instance memory for the tests: aligned for any type, sized from the core's
   own layout. */
static _Alignas(max_align_t) unsigned char machine_buf[sizeof(struct lynx_machine)];

static int is_power_of_two(size_t v)
{
    return v != 0u && (v & (v - 1u)) == 0u;
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
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 0x0123456789abcdefu;

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

    lynx_machine *m = NULL;
    CHECK_EQ_U64(lynx_machine_init(machine_buf, size, &cfg, &m), LYNX_OK);
    if (m == NULL) {
        return;
    }
    uint64_t ticks = 99u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, 0u);

    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    ticks = 99u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, 0u);

    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4000u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, 4000u);

    /* Exact values from the step rule after 1000 steps over the tracer image. */
    uint32_t reg = 0u;
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_PC, &reg), LYNX_OK);
    CHECK_EQ_HEX(reg, 0x0208u);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_A, &reg), LYNX_OK);
    CHECK_EQ_HEX(reg, 0xECu);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_X, &reg), LYNX_OK);
    CHECK_EQ_HEX(reg, 0xECu);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_Y, &reg), LYNX_OK);
    CHECK_EQ_HEX(reg, 0xE8u);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_S, &reg), LYNX_OK);
    CHECK_EQ_HEX(reg, 0x18u);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_P, &reg), LYNX_OK);
    CHECK_EQ_HEX(reg, 0x00u);

    uint8_t byte = 0u;
    /* Steps write only page 0x01, so the image itself is unchanged. */
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, &byte, 1u), LYNX_OK);
    CHECK_EQ_HEX(byte, 0x03u);
    /* The last stack write went to 0x0100 + (final S + 1). */
    CHECK_EQ_U64(lynx_read_memory(m, 0x0119u, &byte, 1u), LYNX_OK);
    CHECK_EQ_HEX(byte, 0xECu);

    lynx_machine_deinit(m);
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
    bad.profile = 1u;
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
}

/* ---- instance independence and init/deinit idempotency (API-01) ---- */

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
    CHECK_EQ_U64(lynx_load_ram_image(a, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_load_ram_image(b, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);

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
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
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
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    memcpy(snap_a, machine_buf, MSIZE);

    uint64_t ticks_before = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_before), LYNX_OK);
    CHECK_EQ_U64(ticks_before, 4096u);

    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v = 0u;
        CHECK_EQ_U64(lynx_read_register(m, r, &v), LYNX_OK);
    }
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

static void case_advance_budget(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }

    /* Before any load: STATE, outputs zeroed, buffer untouched. */
    uint64_t consumed = 0xA5A5A5A5u;
    lynx_stop_reason stop = 0xA5A5A5A5u;
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_ERR_STATE);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);

    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 4096u, NULL, &stop), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_advance(NULL, 4096u, &consumed, &stop), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A budget below one step (4 ticks) buys nothing and changes nothing. */
    ADVANCE_EXPECT(m, 0u, 0u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 2u, 0u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 3u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), 0u);

    /* A budget that is not a multiple of 4 consumes the whole steps only. */
    ADVANCE_EXPECT(m, 4u, 4u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 5u, 4u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 4096u, 4096u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), 4u + 4u + 4096u);

    /* The 16-byte image that ends exactly at 0x10000 stays inside itself. */
    uint8_t wrap_image[16];
    for (uint32_t i = 0u; i < sizeof wrap_image; i++) {
        wrap_image[i] = (uint8_t)(i + 1u); /* 0x01..0x10, no 0xDB */
    }
    CHECK_EQ_U64(lynx_load_ram_image(m, wrap_image, sizeof wrap_image, 0xFFF0u), LYNX_OK);
    for (uint32_t call = 1u; call <= 16u; call++) {
        ADVANCE_EXPECT(m, 4u, 4u, LYNX_STOP_BUDGET);
        uint32_t pc = reg_of(m, LYNX_REG_PC);
        CHECK(pc >= 0xFFF0u && pc <= 0xFFFFu);
        CHECK_EQ_HEX(pc, 0xFFF0u + (call % 16u));
    }
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFF0u);
    CHECK_EQ_U64(ticks_of(m), 64u);

    /* A halting image: 0xDB at offset 3 stops after three steps (12 ticks) and
       the halting check itself costs no ticks. */
    uint8_t halt_image[8] = {0x01u, 0x02u, 0x03u, LYNX__HALT_OPCODE, 0x05u, 0x06u, 0x07u, 0x08u};
    CHECK_EQ_U64(lynx_load_ram_image(m, halt_image, sizeof halt_image, 0x0400u), LYNX_OK);
    ADVANCE_EXPECT(m, UINT64_MAX, 12u, LYNX_STOP_HALT);
    CHECK_EQ_U64(ticks_of(m), 12u);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_HALT);
    ADVANCE_EXPECT(m, 4u, 0u, LYNX_STOP_HALT);
    CHECK_EQ_U64(ticks_of(m), 12u);
    /* A halted machine consumes nothing and changes nothing. */
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, 4096u, 0u, LYNX_STOP_HALT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Guest time near the 64-bit limit: white-box, the test sets the counter.
       Room for 5 ticks buys one 4-tick step, then nothing, and never wraps. */
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    ((struct lynx_machine *)machine_buf)->guest_ticks = UINT64_MAX - 5u;
    ADVANCE_EXPECT(m, UINT64_MAX, 4u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), UINT64_MAX - 1u);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), UINT64_MAX - 1u);

    /* The clamp itself. */
    CHECK_EQ_U64(lynx__budget_steps(UINT64_MAX - 5u, UINT64_MAX), 1u);
    CHECK_EQ_U64(lynx__budget_steps(0u, 7u), 1u);
    CHECK_EQ_U64(lynx__budget_steps(0u, 3u), 0u);
    CHECK_EQ_U64(lynx__budget_steps(UINT64_MAX, 100u), 0u);
    CHECK_EQ_U64(lynx__budget_steps(0u, UINT64_MAX), UINT64_MAX / 4u);
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
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_policy, 0u);
    CHECK_EQ_U64(id.ram_seed, 0u);
    CHECK_EQ_U64(id.has_content, 0u);

    /* Reset after load and advance equals a fresh init, byte for byte. */
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0); /* control: the buffer did move */
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    read_ram(m, mem_out);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0x00));
    CHECK_EQ_U64(ticks_of(m), 0u);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.has_content, 0u);
    /* After a reset there is no content, so advance refuses until a reload. */
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_ERR_STATE);

    /* Reset also clears a halt, through the reload that follows. */
    uint8_t halt_image[4] = {0x01u, LYNX__HALT_OPCODE, 0x03u, 0x04u};
    CHECK_EQ_U64(lynx_load_ram_image(m, halt_image, sizeof halt_image, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 4096u, 4u, LYNX_STOP_HALT);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Bad magic and NULL: a status and no change. */
    CHECK_EQ_U64(lynx_reset(NULL), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_reset(m), LYNX_ERR_STATE);
    CHECK_UNCHANGED(machine_buf, snap_a);

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
    CHECK_EQ_U64(lynx_load_ram_image(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
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
