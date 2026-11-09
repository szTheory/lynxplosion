/* Snapshot save and restore, format version 1.

   A snapshot is a header followed by a payload. Every integer is
   little-endian and is written and read one field at a time, never by copying
   a struct, so padding bytes and host byte order never reach the blob.

   Header (236 bytes), in order:
     0    magic          8 bytes  "LYNXSNAP"
     8    format_version u32      1
     12   header_len     u32      236
     16   build_id       128      lynx__build_id, NUL-padded to 128
     144  profile        u32
     148  ram_policy     u32
     152  ram_seed       u64
     160  content_sha256 32
     192  content_addr   u32
     196  content_len    u32
     200  payload_len    u32      65552
     204  payload_sha256 32       SHA-256 of the payload
   Payload (65552 bytes), in order:
     pc u16, a u8, x u8, y u8, s u8, p u8, halted u8, guest_ticks u64,
     ram 65536 bytes.

   Restore accepts a blob only for an instance with the identical build id,
   profile, RAM policy, seed and loaded content: the identity comes from the
   instance, so a blob cannot assert its own compatibility. It validates in a
   fixed order, all before the first write, and then applies in one step that
   cannot fail. A refused restore leaves the instance byte-identical.

   Reversibility: restore needs the same build, so no old snapshot is ever
   accepted and the format may change in any later build without migration. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "internal.h"
#include "sha256.h"

/* Header field offsets. */
enum {
    OFF_MAGIC = 0,
    OFF_FORMAT = 8,
    OFF_HEADER_LEN = 12,
    OFF_BUILD_ID = 16,
    OFF_PROFILE = OFF_BUILD_ID + (int)LYNX__BUILD_ID_FIELD,
    OFF_POLICY = OFF_PROFILE + 4,
    OFF_SEED = OFF_POLICY + 4,
    OFF_CONTENT_SHA = OFF_SEED + 8,
    OFF_CONTENT_ADDR = OFF_CONTENT_SHA + 32,
    OFF_CONTENT_LEN = OFF_CONTENT_ADDR + 4,
    OFF_PAYLOAD_LEN = OFF_CONTENT_LEN + 4,
    OFF_PAYLOAD_SHA = OFF_PAYLOAD_LEN + 4,
    OFF_END = OFF_PAYLOAD_SHA + 32
};

/* Payload field offsets, relative to the start of the payload. */
enum {
    P_PC = 0,
    P_A = 2,
    P_X = 3,
    P_Y = 4,
    P_S = 5,
    P_P = 6,
    P_HALTED = 7,
    P_TICKS = 8,
    P_RAM = 16,
    P_END = P_RAM + (int)LYNX_ADDRESS_SPACE
};

_Static_assert(OFF_END == (int)LYNX__SNAPSHOT_HEADER_LEN, "snapshot header length");
_Static_assert(P_END == (int)LYNX__SNAPSHOT_PAYLOAD_LEN, "snapshot payload length");

#define SNAPSHOT_TOTAL ((size_t)LYNX__SNAPSHOT_HEADER_LEN + (size_t)LYNX__SNAPSHOT_PAYLOAD_LEN)

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64(uint8_t *p, uint64_t v)
{
    put_u32(p, (uint32_t)v);
    put_u32(p + 4, (uint32_t)(v >> 32));
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const uint8_t *p)
{
    return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p + 4) << 32);
}

/* The build id, NUL-padded to the 128-byte header field. The configure step
   refuses an id of 128 characters or more, and the copy is bounded here too. */
static void padded_build_id(uint8_t field[LYNX__BUILD_ID_FIELD])
{
    memset(field, 0, LYNX__BUILD_ID_FIELD);
    for (size_t i = 0u; i + 1u < LYNX__BUILD_ID_FIELD && lynx__build_id[i] != '\0'; i++) {
        field[i] = (uint8_t)lynx__build_id[i];
    }
}

/* True when [a, a+alen) and [b, b+blen) share no byte. Subtraction form, so
   no sum of an address and a length can wrap. */
static int ranges_disjoint(const void *a, size_t alen, const void *b, size_t blen)
{
    uintptr_t ua = (uintptr_t)a;
    uintptr_t ub = (uintptr_t)b;
    if (ua >= ub) {
        return (ua - ub) >= blen;
    }
    return (ub - ua) >= alen;
}

static int machine_ok(const lynx_machine *m)
{
    return m->magic == LYNX__MAGIC;
}

lynx_status lynx_snapshot_size(const lynx_machine *m, size_t *size)
{
    if (m == NULL || size == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_ok(m)) {
        return LYNX_ERR_STATE;
    }
    *size = SNAPSHOT_TOTAL;
    return LYNX_OK;
}

lynx_status lynx_snapshot_save(const lynx_machine *m, uint8_t *buf, size_t cap, size_t *written)
{
    if (m == NULL || buf == NULL || written == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_ok(m) || m->has_content == 0u) {
        return LYNX_ERR_STATE;
    }
    if (cap < SNAPSHOT_TOTAL) {
        return LYNX_ERR_CAPACITY;
    }
    if (!ranges_disjoint(buf, SNAPSHOT_TOTAL, m, sizeof *m)) {
        return LYNX_ERR_ARGUMENT;
    }

    uint8_t *payload = buf + LYNX__SNAPSHOT_HEADER_LEN;
    put_u16(payload + P_PC, m->pc);
    payload[P_A] = m->a;
    payload[P_X] = m->x;
    payload[P_Y] = m->y;
    payload[P_S] = m->s;
    payload[P_P] = m->p;
    payload[P_HALTED] = m->halted;
    put_u64(payload + P_TICKS, m->guest_ticks);
    memcpy(payload + P_RAM, m->ram, LYNX_ADDRESS_SPACE);

    memcpy(buf + OFF_MAGIC, LYNX__SNAPSHOT_MAGIC, LYNX__SNAPSHOT_MAGIC_LEN);
    put_u32(buf + OFF_FORMAT, LYNX__SNAPSHOT_FORMAT);
    put_u32(buf + OFF_HEADER_LEN, LYNX__SNAPSHOT_HEADER_LEN);
    padded_build_id(buf + OFF_BUILD_ID);
    put_u32(buf + OFF_PROFILE, m->cfg.profile);
    put_u32(buf + OFF_POLICY, m->cfg.ram_policy);
    put_u64(buf + OFF_SEED, m->cfg.ram_seed);
    memcpy(buf + OFF_CONTENT_SHA, m->content_sha256, sizeof m->content_sha256);
    put_u32(buf + OFF_CONTENT_ADDR, m->content_addr);
    put_u32(buf + OFF_CONTENT_LEN, m->content_len);
    put_u32(buf + OFF_PAYLOAD_LEN, LYNX__SNAPSHOT_PAYLOAD_LEN);

    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, payload, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, buf + OFF_PAYLOAD_SHA);

    *written = SNAPSHOT_TOTAL;
    return LYNX_OK;
}

lynx_status lynx_snapshot_restore(lynx_machine *m, const uint8_t *buf, size_t len)
{
    /* 1. Arguments, instance state, overlap. */
    if (m == NULL || buf == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_ok(m) || m->has_content == 0u) {
        return LYNX_ERR_STATE;
    }
    if (!ranges_disjoint(buf, len, m, sizeof *m)) {
        return LYNX_ERR_ARGUMENT;
    }
    /* 2. Exact length: a truncated blob or one with trailing bytes. */
    if (len != SNAPSHOT_TOTAL) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 3. Magic. */
    if (memcmp(buf + OFF_MAGIC, LYNX__SNAPSHOT_MAGIC, LYNX__SNAPSHOT_MAGIC_LEN) != 0) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 4. Format version. */
    if (get_u32(buf + OFF_FORMAT) != LYNX__SNAPSHOT_FORMAT) {
        return LYNX_ERR_SNAPSHOT_VERSION;
    }
    /* 5. Header length. */
    if (get_u32(buf + OFF_HEADER_LEN) != LYNX__SNAPSHOT_HEADER_LEN) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 6. The whole 128-byte build id field, padding included. */
    uint8_t build_field[LYNX__BUILD_ID_FIELD];
    padded_build_id(build_field);
    if (memcmp(buf + OFF_BUILD_ID, build_field, LYNX__BUILD_ID_FIELD) != 0) {
        return LYNX_ERR_SNAPSHOT_MISMATCH;
    }
    /* 7. Identity against this instance. */
    if (get_u32(buf + OFF_PROFILE) != m->cfg.profile ||
        get_u32(buf + OFF_POLICY) != m->cfg.ram_policy ||
        get_u64(buf + OFF_SEED) != m->cfg.ram_seed ||
        memcmp(buf + OFF_CONTENT_SHA, m->content_sha256, sizeof m->content_sha256) != 0 ||
        get_u32(buf + OFF_CONTENT_ADDR) != m->content_addr ||
        get_u32(buf + OFF_CONTENT_LEN) != m->content_len) {
        return LYNX_ERR_SNAPSHOT_MISMATCH;
    }
    /* 8. Payload length. */
    if (get_u32(buf + OFF_PAYLOAD_LEN) != LYNX__SNAPSHOT_PAYLOAD_LEN) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 9. Payload hash. */
    const uint8_t *payload = buf + LYNX__SNAPSHOT_HEADER_LEN;
    uint8_t digest[32];
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, payload, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, digest);
    if (memcmp(digest, buf + OFF_PAYLOAD_SHA, sizeof digest) != 0) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 10. Value ranges, in 32-bit arithmetic: content_addr + content_len can
       reach 0x10000, which a 16-bit sum would wrap to zero. */
    uint32_t pc = (uint32_t)get_u16(payload + P_PC);
    if (payload[P_HALTED] > 1u || pc < m->content_addr || pc - m->content_addr >= m->content_len) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }

    /* Apply: one step, field by field, no failure path. */
    m->pc = (uint16_t)pc;
    m->a = payload[P_A];
    m->x = payload[P_X];
    m->y = payload[P_Y];
    m->s = payload[P_S];
    m->p = payload[P_P];
    m->halted = payload[P_HALTED];
    m->guest_ticks = get_u64(payload + P_TICKS);
    memcpy(m->ram, payload + P_RAM, LYNX_ADDRESS_SPACE);
    return LYNX_OK;
}
