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

static const struct {
    const char *name;
    void (*run)(void);
} cases[] = {
    {"build_id", case_build_id},
    {"identity", case_identity},
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
